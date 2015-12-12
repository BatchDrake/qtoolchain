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

#ifndef _QAS_QAS_H
#define _QAS_QAS_H

#include <stdint.h>

#include <qcircuit.h>

#define QAS_CTX_EOF -1
#define QAS_ERROR_MAX 256

enum qas_ctx_kind
{
  QAS_CTX_KIND_GLOBAL,
  QAS_CTX_KIND_CIRCUIT,
  QAS_CTX_KIND_GATE
};

struct qas_ctx
{
  char *path;
  unsigned int line;

  union
  {
    void            *file_buffer;
    const uint8_t *file_bytes;
  };

  unsigned int file_ptr;
  size_t file_size;

  void (*closebuf_fn) (void *, size_t);

  char last_error[QAS_ERROR_MAX];
  QBOOL failed;
  QBOOL parsed;

  struct qas_ctx *parent; /* For included files */

  qdb_t *qdb; /* Database of parsed objects */

  enum qas_ctx_kind ctx_kind;

  /* For building circuits */
  qcircuit_t *curr_circuit;
  fastlist_t  qubit_aliases; /* Qubit aliases to ease programming */

  /* For building gates */
  qgate_t *curr_gate;
  unsigned int curr_coef;
};

typedef struct qas_ctx qas_ctx_t;

qas_ctx_t *qas_open_from_file (const char *);
qas_ctx_t *__qas_ctx_open_from_file (const char *, qas_ctx_t *);

QBOOL qas_parse (qas_ctx_t *);

QBOOL qas_is_parsed (const qas_ctx_t *);
QBOOL qas_has_failed (const qas_ctx_t *);
void qas_set_error (qas_ctx_t *, const char *, ...);
const char *qas_get_error (const qas_ctx_t *);
const char *qas_get_path (const qas_ctx_t *);
unsigned int qas_get_line (const qas_ctx_t *);

/* Pops the database and sets it to NULL. */
qdb_t *qas_pop_db (qas_ctx_t *);

void qas_close (qas_ctx_t *);

QBOOL qas_register_include (const char *);
char *qas_resolve_include (const char *);

QBOOL qas_init (void);

#endif /* _QAS_QAS_H */
