#ifndef _LIBQCIRCUIT_QCIRCUIT_H
#define _LIBQCIRCUIT_QCIRCUIT_H

#include <qsparse.h>
#include <stdlib.h>
#include <string.h>

#include "fastlist.h"

#define QCIRCUIT_LAST_ERROR_MAX 256

struct qgate
{
  char *name;
  char *description;

  unsigned int order;
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
  QBOOL updated; /* U is not updated */

  unsigned int order; /* Number of qubits */

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

qwiring_t *qwiring_new (const qgate_t *, const unsigned int *);
void qwiring_destroy (qwiring_t *);

qcircuit_t *qcircuit_new (unsigned int);

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


#endif /* _LIBQCIRCUIT_QCIRCUIT_H */
