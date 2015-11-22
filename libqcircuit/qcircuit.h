/*
  qcircuit.h: Quantum circuit implementation

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

#ifndef _LIBQCIRCUIT_QCIRCUIT_H
#define _LIBQCIRCUIT_QCIRCUIT_H

#include <qsparse.h>
#include <stdlib.h>
#include <string.h>

#define QCIRCUIT_LAST_ERROR_MAX 256

struct qgate
{
  unsigned int order;

  char *name;
  char *description;

  QCOMPLEX *coef;

  qsparse_t *sparse;
};

typedef struct qgate qgate_t;

struct qwiring
{
  struct qwiring *prev;
  struct qwiring *next;

  const qgate_t *gate;

  unsigned int *remap; /* This has order gate->order */

  /* Cached QSPARSE representation */
  qsparse_t *__cached;
};

typedef struct qwiring qwiring_t;

struct qcircuit
{
  unsigned int order; /* Number of qubits */
  char *name;

  QBOOL updated; /* U is not updated */

  qsparse_t *u;

  /* Store previous measures */
  uint64_t collapsed_mask;
  uint64_t measure_result;

  QCOMPLEX *state;     /* Wave function */
  QCOMPLEX *collapsed; /* Collapsed wave function */

  qwiring_t *wiring_head;
  qwiring_t *wiring_tail;
};

typedef struct qcircuit qcircuit_t;

void qgate_destroy (qgate_t *);
qgate_t *qgate_new (unsigned int, const char *, const char *, const QCOMPLEX *);
void qgate_set_coef (qgate_t *, const QCOMPLEX *);

qwiring_t *qwiring_new (const qgate_t *, const unsigned int *);
void qwiring_destroy (qwiring_t *);

qcircuit_t *qcircuit_new (unsigned int, const char *);

void qcircuit_measure_reset (qcircuit_t *circuit);

QBOOL qcircuit_append_wiring (qcircuit_t *, qwiring_t *);
QBOOL qcircuit_prepend_wiring (qcircuit_t *, qwiring_t *);
QBOOL qcircuit_wire (qcircuit_t *, const qgate_t *, const unsigned int *);

static inline qwiring_t *
qcircuit_get_wiring_head(const qcircuit_t *circuit)
{
  return circuit->wiring_head;
}

static inline qwiring_t *
qcircuit_get_wiring_tail(const qcircuit_t *circuit)
{
  return circuit->wiring_tail;
}

static inline qwiring_t *
qwiring_next (const qwiring_t *wiring)
{
  return wiring->next;
}

static inline qwiring_t *
qwiring_prev (const qwiring_t *wiring)
{
  return wiring->prev;
}

QBOOL qcircuit_update (qcircuit_t *);

/* These functions may fail if U is not updated*/
QBOOL qcircuit_apply_state (qcircuit_t *, const QCOMPLEX *);
const QCOMPLEX *qcircuit_get_state (const qcircuit_t *);
QBOOL qcircuit_collapse (qcircuit_t *, uint64_t, unsigned int *);

void qcircuit_destroy (qcircuit_t *);

#include "qdb.h"

/* Serialize / deserialize functions */
uint32_t qgate_serialize (const qgate_t *, void *, uint32_t);
qgate_t *qgate_deserialize (const void *, uint32_t);

uint32_t qwiring_serialize (const qwiring_t *, void *, uint32_t);
qwiring_t *qwiring_deserialize (const qdb_t *, const void *, uint32_t);

uint32_t qcircuit_serialize (const qcircuit_t *, void *, uint32_t);
qcircuit_t *qcircuit_deserialize (const qdb_t *, const void *, uint32_t);

#endif /* _LIBQCIRCUIT_QCIRCUIT_H */
