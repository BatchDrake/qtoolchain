/*
  parser.c: Quantum Assembler file parser

  Copyright (C) 2015 Gonzalo José Carracedo Carballal

  This program is free software: you can redistribute it and/or modify
  it under the terms of the GNU Lesser General Public License as
  published by the Free Software Foundation, either version 3 of the
  License, or (at your option) any later version.

  This program is distributed in the hope that it will be useful, but
  WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU Lesser General Public License for more details.

  You should have received a copy of the GNU Lesser General Public
  License along with this program.  If not, see
  <http://www.gnu.org/licenses/>

*/

#include <stdio.h>
#include <stdarg.h>
#include <sys/mman.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <ctype.h>
#include <util.h>

#include "qas.h"

#define QAS_PARSE_IS_LINE_TERMINATOR(p) ((p) == '\0' || (p) == '#')
#define QAS_PARSE_IS_INSTRUCTION_PREFIX(p) (isalpha(p) || strchr ("._-$", p) != NULL)
#define QAS_PARSE_IS_ARGUMENT_PREFIX(p) (isalnum(p) || strchr ("._-$", p) != NULL)
#define QAS_PARSE_IS_STRING_PREFIX(p) (p == '"')

#define QINSTFUNC(name) JOIN (__qas_process_, name)
#define QINSTDECL(name) static QBOOL QINSTFUNC(name) (qas_ctx_t *ctx, const char *inst, const fastlist_t *args)

static char *q_string_remove_quotes (const char *);


#define Q_ENSURE_ARGS(num)                \
  if (fastlist_size (args) != (num))      \
  {                                       \
    qas_set_error (ctx,                   \
                   "%s: expected %d arguments, but %d were given", \
                   inst,                  \
                   num,                   \
                   fastlist_size (args)); \
    return Q_FALSE;                       \
  }

#define Q_ENSURE_MIN_ARGS(num)            \
  if (fastlist_size (args) < (num))       \
  {                                       \
    qas_set_error (ctx,                   \
                   "%s: expected at least %d arguments, but only %d were given", \
                   inst,                  \
                   num,                   \
                   fastlist_size (args)); \
    return Q_FALSE;                       \
  }

#define Q_ENSURE_CONTEXT(kind)            \
  if (ctx->ctx_kind != (kind))            \
  {                                       \
    qas_set_error (ctx,                   \
                   "%s: wrong context for instruction", \
                   inst);                 \
    return Q_FALSE;                       \
  }

#define Q_ARG(num) ((const char *) fastlist_get (args, num))

#define Q_ENSURE_STRING(arg)              \
  if (*Q_ARG (arg) != '"')                \
  {                                       \
    qas_set_error (ctx,                   \
                   "%s: argument %d not a number", \
                   inst, arg + 1);        \
    return Q_FALSE;                       \
  }

#define Q_ENSURE_NUM(arg)                 \
  if (!isdigit (*Q_ARG (arg)))            \
  {                                       \
    qas_set_error (ctx,                   \
                   "%s: argument %d not a string", \
                   inst, arg + 1);        \
    return Q_FALSE;                       \
  }

#define Q_ENSURE_IDENTIFIER(arg)          \
  if (!QAS_PARSE_IS_INSTRUCTION_PREFIX    \
      (*Q_ARG (arg)))                     \
  {                                       \
    qas_set_error (ctx,                   \
                   "%s: argument %d not an identifier", \
                   inst, arg + 1);        \
    return Q_FALSE;                       \
  }

#define Q_PARSE_NUM(arg, output)          \
  if (!sscanf (Q_ARG (arg), "%i", (int *)&output))  \
  {                                       \
    qas_set_error (ctx,                   \
                   "%s: argument %d not a number", \
                   inst, arg + 1);        \
    return Q_FALSE;                       \
  }

#define Q_PARSE_COMPLEX(arg, output)      \
  {                                       \
    QREAL __abs, __arg = .0;              \
                                          \
    if (sscanf (Q_ARG (arg),              \
                QREALFMT "[" QREALFMT "]",\
                &__abs,                   \
                &__arg) != 2)             \
      if (!sscanf (Q_ARG (arg),           \
                   QREALFMT,              \
                   &__abs))               \
      {                                   \
        qas_set_error (ctx,               \
                       "%s: argument %d not a complex number", \
                       inst, arg + 1);    \
        return Q_FALSE;                   \
      }                                   \
                                          \
    output = __abs * cexp (I * __arg);    \
  }

#define Q_CIRCUIT_ERROR(ctx, fmt, arg...) \
    qas_set_error (ctx,                   \
                   "in circuit `%s': "    \
                   fmt,                   \
                   ctx->curr_circuit->name, \
                   ##arg)

#define Q_GATE_ERROR(ctx, fmt, arg...)    \
    qas_set_error (ctx,                   \
                   "in gate `%s': "       \
                   fmt,                   \
                   ctx->curr_gate->name,  \
                   ##arg)

typedef QBOOL (*qas_instruction_callback_t) (qas_ctx_t *, const char *, const fastlist_t *);

struct qas_instruction
{
  const char *inst;
  qas_instruction_callback_t cb;
};

QINSTDECL(circuit);
QINSTDECL(end);
QINSTDECL(include);
QINSTDECL(gate);
QINSTDECL(coef);
QINSTDECL(qubit);

struct qas_instruction instructions[] =
{
    {".circuit", QINSTFUNC (circuit)},
    {".gate",    QINSTFUNC (gate)},
    {".end",     QINSTFUNC (end)},
    {".include", QINSTFUNC (include)},
    {".coef",    QINSTFUNC (coef)},
    {".qubit",   QINSTFUNC (qubit)},
    {NULL, NULL}
};

QINSTDECL(circuit)
{
  unsigned int qubits;

  Q_ENSURE_ARGS (2);
  Q_ENSURE_CONTEXT (QAS_CTX_KIND_GLOBAL);

  Q_ENSURE_IDENTIFIER (0);
  Q_ENSURE_NUM (1);

  /* Create circuit */
  Q_PARSE_NUM (1, qubits);

  if ((ctx->curr_circuit = qcircuit_new (qubits, Q_ARG (0))) == NULL)
    return Q_FALSE;

  ctx->ctx_kind = QAS_CTX_KIND_CIRCUIT;

  return Q_TRUE;
}

QINSTDECL(gate)
{
  unsigned int qubits;
  char *desc;

  Q_ENSURE_ARGS (3);
  Q_ENSURE_CONTEXT (QAS_CTX_KIND_GLOBAL);

  Q_ENSURE_IDENTIFIER (0);
  Q_ENSURE_NUM (1);
  Q_ENSURE_STRING (2);

  /* Create gate */
  Q_PARSE_NUM (1, qubits);

  if ((desc = q_string_remove_quotes (Q_ARG (2))) == NULL)
  {
    qas_set_error (ctx, "memory exhausted");

    return Q_FALSE;
  }

  if ((ctx->curr_gate = qgate_new (qubits, Q_ARG (0), desc, NULL)) == NULL)
  {
    qas_set_error (ctx, "failed to create gate: %s", q_get_last_error ());

    free (desc);

    return Q_FALSE;
  }

  ctx->curr_coef = 0;

  free (desc);

  ctx->ctx_kind = QAS_CTX_KIND_GATE;

  return Q_TRUE;
}

QINSTDECL(coef)
{
  unsigned int i;
  unsigned int matrix_length;
  QCOMPLEX coef;

  Q_ENSURE_MIN_ARGS (1);

  Q_ENSURE_CONTEXT (QAS_CTX_KIND_GATE);

  matrix_length = 1 << (ctx->curr_gate->order + 1);

  for (i = 0; i < fastlist_size (args); ++i)
  {
    Q_PARSE_COMPLEX (i, coef);

    if (ctx->curr_coef >= matrix_length)
    {
      Q_GATE_ERROR (ctx,
                    "gate matrix coefficient count exceeded (%d)",
                     matrix_length);

      return Q_FALSE;
    }

    ctx->curr_gate->coef[ctx->curr_coef++] = coef;
  }

  return Q_TRUE;
}

static fastlist_ref_t
qas_ctx_resolve_qubit_alias (const qas_ctx_t *ctx, const char *name)
{
  FASTLIST_FOR_BEGIN (const char *, alias, &ctx->qubit_aliases)
    if (strcmp (alias, name) == 0)
      return FASTLIST_FOR_INNERMOST_REF;
  FASTLIST_FOR_END

  return FASTLIST_INVALID_REF;
}

QINSTDECL(qubit)
{
  char *dup;

  Q_ENSURE_ARGS (1);

  Q_ENSURE_CONTEXT (QAS_CTX_KIND_CIRCUIT);

  if (fastlist_size (&ctx->qubit_aliases) >= ctx->curr_circuit->order)
  {
    Q_CIRCUIT_ERROR (ctx, "too many qubits");

    return Q_FALSE;
  }

  if (qas_ctx_resolve_qubit_alias (ctx, Q_ARG (0)) != FASTLIST_INVALID_REF)
  {
    Q_CIRCUIT_ERROR (ctx, "qubit `%s' already declared", Q_ARG (0));

    return Q_FALSE;
  }

  if ((dup = strdup (Q_ARG (0))) == NULL)
  {
    Q_CIRCUIT_ERROR (ctx, "memory exhausted");

    return Q_FALSE;
  }

  if (fastlist_append(&ctx->qubit_aliases, dup) == 0)
  {
    Q_CIRCUIT_ERROR (ctx, "memory exhausted");

    free (dup);

    return Q_FALSE;
  }

  return Q_TRUE;
}

QINSTDECL(include)
{
  char *file;
  qas_ctx_t *inc_ctx;

  Q_ENSURE_ARGS (1);
  Q_ENSURE_CONTEXT (QAS_CTX_KIND_GLOBAL);

  Q_ENSURE_STRING (0);

  if ((file = q_string_remove_quotes (Q_ARG (0))) == NULL)
  {
    qas_set_error (ctx, "memory exhausted");

    return Q_FALSE;
  }

  if ((inc_ctx = __qas_ctx_open_from_file (file, ctx)) == NULL)
  {
    qas_set_error (ctx, ".include: %s", q_get_last_error ());

    free (file);

    return Q_FALSE;
  }

  free (file);

  if (!qas_parse (inc_ctx))
  {
    fprintf (stderr, "error: %s:%d: %s\n",
             qas_get_path (inc_ctx),
             qas_get_line (inc_ctx),
             qas_get_error (inc_ctx));

    qas_set_error (ctx, "... in included file");

    qas_close (inc_ctx);

    return Q_FALSE;
  }

  qas_close (inc_ctx);

  return Q_TRUE;
}

QINSTDECL(end)
{
  Q_ENSURE_ARGS (0);

  switch (ctx->ctx_kind)
  {
    case QAS_CTX_KIND_CIRCUIT:
      if (!qdb_register_qcircuit (ctx->qdb, ctx->curr_circuit))
      {
        qas_set_error (ctx, "cannot register circuit: %s", q_get_last_error ());

        return Q_FALSE;
      }

      ctx->curr_circuit = NULL;

      FASTLIST_FOR_BEGIN (char *, alias, &ctx->qubit_aliases)
        free (alias);
      FASTLIST_FOR_END

      fastlist_free (&ctx->qubit_aliases);

      break;

    case QAS_CTX_KIND_GATE:
      if (!qdb_register_qgate (ctx->qdb, ctx->curr_gate))
      {
        qas_set_error (ctx, "cannot register circuit: %s", q_get_last_error ());

        return Q_FALSE;
      }

      ctx->curr_gate = NULL;

      break;

    case QAS_CTX_KIND_GLOBAL:
      qas_set_error (ctx, "%s: no context to close", inst);
      return Q_FALSE;
  }

  return Q_TRUE;
}

static char *
q_string_remove_quotes (const char *str)
{
  char *dup;

  if ((dup = malloc (strlen (str) + 1)) == NULL)
    return NULL;

  strcpy (dup, str + 1);

  dup[strlen (dup) - 1] = '\0';

  return dup;
}

static inline QBOOL
qas_eof (const qas_ctx_t *ctx)
{
  return ctx->file_ptr >= ctx->file_size;
}

static inline int
qas_getchar (qas_ctx_t *ctx)
{
  if (qas_eof (ctx))
    return QAS_CTX_EOF;

  return (int) ctx->file_bytes[ctx->file_ptr++];
}

static inline unsigned int
qas_tell (const qas_ctx_t *ctx)
{
  return ctx->file_ptr;
}

static inline QBOOL
qas_seek (const qas_ctx_t *ctx, unsigned int ptr)
{
  if (ptr > ctx->file_ptr)
    return Q_FALSE;

  return Q_TRUE;
}

static inline void *
qas_tell_buffer (qas_ctx_t *ctx)
{
  return (void *) &ctx->file_bytes[ctx->file_ptr];
}

char *
qas_read_line (qas_ctx_t *ctx)
{
  unsigned int line_size = 0;
  unsigned int line_off;
  int c;

  char *copy;
  char *line_start;

  line_off = qas_tell (ctx);
  line_start = (char *) qas_tell_buffer (ctx);

  while ((c = qas_getchar (ctx)) != QAS_CTX_EOF && c != '\n');

  line_size = qas_tell (ctx) - line_off;

  if ((copy = malloc (line_size + 1)) == NULL)
  {
    qas_seek (ctx, line_off); /* We leave it where we left it */
    qas_set_error (ctx, "internal: qas_read_line: memory exhausted");
    return NULL;
  }

  memcpy (copy, line_start, line_size);

  copy[line_size] = '\0';

  if (copy[line_size - 1] == '\n')
    copy[line_size - 1] = '\0';

  return copy;
}

static inline const char *
__get_next_nontok (const char *str)
{
  while (*str && (isalnum (*str) || strchr ("._-$[]", *str) != NULL))
    ++str;

  return str;
}

static inline const char *
__get_next_nonspc (const char *str)
{
  while (*str && isspace (*str))
    ++str;

  return str;
}

static inline const char *
__get_string_end (const char *str)
{
  QBOOL escaped = Q_FALSE;

  /* Accepts: beggining of the string */
  ++str;

  while (*str && (*str != '"' || (escaped && *str == '"')))
  {
    if (escaped)
      escaped = Q_FALSE;
    else
      escaped = *str == '\\';

    ++str;
  }

  return str;
}

QBOOL
qas_parse (qas_ctx_t *ctx)
{
  char *line  = NULL;
  char *instr = NULL;
  char *arg   = NULL;
  unsigned int length;
  const char *trimmed;
  const char *p;

  unsigned int i;

  fastlist_t arglist = FASTLIST_INITIALIZER;

  ctx->line = 0;
  ctx->parsed = Q_TRUE;
  ctx->failed = Q_FALSE;

  while (!qas_eof (ctx))
  {
    if ((line = qas_read_line (ctx)) == NULL)
      goto fail;

    ++ctx->line;

    trimmed = __get_next_nonspc (line);

    /* Comment? */
    if (QAS_PARSE_IS_LINE_TERMINATOR (*trimmed))
    {
      free (line);
      continue;
    }

    p = __get_next_nontok (trimmed);

    length = p - trimmed;
    if ((instr = malloc (length + 1)) == NULL)
    {
      qas_set_error (ctx, "internal: qas_parse: memory exhausted");
      goto fail;
    }

    memcpy (instr, trimmed, length);
    instr[length] = '\0';

    p = __get_next_nonspc (p);

    if (!QAS_PARSE_IS_ARGUMENT_PREFIX (*p) && !QAS_PARSE_IS_STRING_PREFIX (*p))
    {
      qas_set_error (ctx, "unrecognized instruction-argument separator `%c'", *p);
      goto fail;
    }

    /* Extract arguments */
    trimmed = p;
    while (!QAS_PARSE_IS_LINE_TERMINATOR (*trimmed))
    {
      if (QAS_PARSE_IS_ARGUMENT_PREFIX (*trimmed))
        p = __get_next_nontok (trimmed);
      else if (QAS_PARSE_IS_STRING_PREFIX (*trimmed))
      {
        p = __get_string_end (trimmed);
        if (!QAS_PARSE_IS_STRING_PREFIX (*p))
        {
          qas_set_error (ctx, "unterminated string", *p);
          goto fail;
        }
        ++p;
      }
      else
      {
        qas_set_error (ctx, "unrecognized character before argument `%c'", *p);
        goto fail;
      }

      length = p - trimmed;
      if ((arg = malloc (length + 1)) == NULL)
      {
        qas_set_error (ctx, "internal: qas_parse: memory exhausted");
        goto fail;
      }

      memcpy (arg, trimmed, length);
      arg[length] = '\0';

      if (fastlist_append (&arglist, arg) == FASTLIST_INVALID_REF)
      {
        free (arg);
        qas_set_error (ctx, "internal: qas_parse: memory exhausted");
        goto fail;
      }

      p = __get_next_nonspc (p);

      if (QAS_PARSE_IS_LINE_TERMINATOR (*p))
        break;
      else if (*p == ',')
        trimmed = __get_next_nonspc (p + 1);
      else
      {
        qas_set_error (ctx, "unrecognized character after argument `%c'", *p);
        goto fail;
      }
    }

    i = 0;

    while (instructions[i].cb != NULL)
    {
      if (strcmp (instructions[i].inst, instr) == 0)
        break;

      ++i;
    }

    if (instructions[i].cb == NULL)
    {
      qas_set_error (ctx, "unrecognized instruction `%s'", instr);
      goto fail;
    }
    else if (!instructions[i].cb (ctx, instr, &arglist))
      goto fail;

    FASTLIST_FOR_BEGIN (char *, arg, &arglist)
      free (arg);
    FASTLIST_FOR_END

    fastlist_free (&arglist);

    free (instr);
    instr = NULL;

    free (line);
    line = NULL;
  }

  ctx->failed = Q_FALSE;

  return Q_TRUE;

fail:
  if (line != NULL)
    free (line);

  if (instr != NULL)
    free (instr);

  /* Clear arguments (if any) */
  FASTLIST_FOR_BEGIN (char *, arg, &arglist)
    free (arg);
  FASTLIST_FOR_END

  fastlist_free (&arglist);

  return Q_FALSE;
}
