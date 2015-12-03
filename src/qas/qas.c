/*
  qas.c: The Quantum Assembler object

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

#include "qas.h"

void
qas_close (qas_ctx_t *ctx)
{
  if (ctx->closebuf_fn != NULL)
    (ctx->closebuf_fn) (ctx->file_buffer, ctx->file_size);

  if (ctx->parent == NULL)
    if (ctx->qdb != NULL)
      qdb_destroy (ctx->qdb, Q_TRUE); /* Deep destroy */

  if (ctx->path != NULL)
    free (ctx->path);

  free (ctx);
}

qas_ctx_t *
qas_ctx_new (const char *path, qas_ctx_t *parent)
{
  qas_ctx_t *new = NULL;

  if ((new = calloc (1, sizeof (qas_ctx_t))) == NULL)
    goto fail;

  /* Root context has always its own QDB */
  if (parent == NULL)
  {
    if ((new->qdb = qdb_new ()) == NULL)
      goto fail;
  }
  else
    new->qdb = parent->qdb;

  if ((new->path = strdup (path)) == NULL)
      goto fail;

  new->parent = parent;

  return new;
fail:
  if (new != NULL)
    qas_close (new);

  return NULL;
}

static void
__qas_ctx_mappedfile_close_fn (void *buffer, size_t size)
{
  (void) munmap (buffer, size);
}

static QBOOL
qas_ctx_map_file (qas_ctx_t *ctx, int fd)
{
  size_t size;
  void *map;

  if ((size = lseek (fd, 0, SEEK_END)) == (size_t) -1)
  {
    q_set_last_error ("qas_ctx_map_file: cannot get file size: %s", strerror (errno));
    goto fail;
  }

  if ((map = mmap (NULL, size, PROT_READ, MAP_PRIVATE, fd, 0)) == (caddr_t) -1)
  {
    q_set_last_error ("qas_ctx_map_file: cannot map file: %s", strerror (errno));
    goto fail;
  }

  ctx->closebuf_fn = __qas_ctx_mappedfile_close_fn;
  ctx->file_buffer = map;
  ctx->file_size   = size;

  return Q_TRUE;

fail:
  return Q_FALSE;
}

static qas_ctx_t *
__qas_ctx_open_from_file (const char *path, qas_ctx_t *parent)
{
  int fd = -1;
  qas_ctx_t *ctx = NULL;

  if ((ctx = qas_ctx_new (path, parent)) == NULL)
  {
    q_set_last_error ("qas: failed to create context object: memory exhausted");
    goto fail;
  }

  if ((fd = open (path, O_RDONLY)) == -1)
  {
    q_set_last_error ("qas: cannot open `%s' for reading: `%s'", path, strerror (errno));
    goto fail;
  }

  if (!qas_ctx_map_file (ctx, fd))
    goto fail;

  close (fd);

  return ctx;

fail:
  if (ctx != NULL)
    qas_close (ctx);

  if (fd != -1)
    close (fd);
  return NULL;
}

qas_ctx_t *
qas_open_from_file (const char *path)
{
  return __qas_ctx_open_from_file (path, NULL);
}

qdb_t *
qas_pop_db (qas_ctx_t *ctx)
{
  qdb_t *db;

  db = ctx->qdb;

  ctx->qdb = NULL;

  return db;
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
  void
  qas_set_error (qas_ctx_t *ctx, const char *fmt, ...)
  {
    va_list ap;

    va_start (ap, fmt);

    vsnprintf (ctx->error, QAS_ERROR_MAX, fmt, ap);

    va_end (ap);

    ctx->failed = Q_TRUE;
  }

  const char *
  qas_get_error (const qas_ctx_t *ctx)
  {
    return ctx->error;
  }

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
  while (*str && (isalnum (*str) || strchr ("._-$", *str) != NULL))
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


#define QAS_PARSE_IS_LINE_TERMINATOR(p) ((p) == '\0' || (p) == '#')
#define QAS_PARSE_IS_INSTRUCTION_PREFIX(p) (isalpha(p) || strchr ("._-$", p) != NULL)
#define QAS_PARSE_IS_ARGUMENT_PREFIX(p) (isalnum(p) || strchr ("._-$", p) != NULL)
#define QAS_PARSE_IS_STRING_PREFIX(p) (p == '"')

QBOOL
qas_parse (qas_ctx_t *ctx)
{
  char *line  = NULL;
  char *instr = NULL;
  char *arg   = NULL;
  unsigned int length;
  const char *trimmed;
  const char *p;

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

    printf ("%s:%d: Instruction %s arguments (%d):\n", ctx->path, ctx->line, instr, fastlist_size (&arglist));

    FASTLIST_FOR_BEGIN (char *, arg, &arglist)
      printf ("  [%d] %s\n", __i, arg);
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

QBOOL
qas_is_parsed (const qas_ctx_t *ctx)
{
  return ctx->parsed;
}

QBOOL
qas_is_failed (const qas_ctx_t *ctx)
{
  return ctx->parsed && ctx->failed;
}

void
qas_set_error (qas_ctx_t *ctx, const char *fmt, ...)
{
  va_list ap;

  va_start (ap, fmt);

  vsnprintf (ctx->error, QAS_ERROR_MAX, fmt, ap);

  va_end (ap);

  ctx->failed = Q_TRUE;
}

const char *
qas_get_error (const qas_ctx_t *ctx)
{
  return ctx->error;
}

const char *
qas_get_path (const qas_ctx_t *ctx)
{
  return ctx->path;
}

unsigned int
qas_get_line (const qas_ctx_t *ctx)
{
  return ctx->line;
}

