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

  if (ctx->curr_circuit != NULL)
    qcircuit_destroy (ctx->curr_circuit);

  if (ctx->curr_gate != NULL)
    qgate_destroy (ctx->curr_gate);

  FASTLIST_FOR_BEGIN (char *, alias, &ctx->qubit_aliases)
    free (alias);
  FASTLIST_FOR_END

  fastlist_free (&ctx->qubit_aliases);

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

qas_ctx_t *
__qas_ctx_open_from_file (const char *path, qas_ctx_t *parent)
{
  int fd = -1;
  qas_ctx_t *ctx = NULL;

  if ((ctx = qas_ctx_new (path, parent)) == NULL)
  {
    q_set_last_error ("failed to create context object: memory exhausted");
    goto fail;
  }

  if ((fd = open (path, O_RDONLY)) == -1)
  {
    q_set_last_error ("cannot open `%s' for reading: %s", path, strerror (errno));
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

  vsnprintf (ctx->last_error, QAS_ERROR_MAX, fmt, ap);

  va_end (ap);

  ctx->failed = Q_TRUE;
}

const char *
qas_get_error (const qas_ctx_t *ctx)
{
  return ctx->last_error;
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

