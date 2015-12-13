/*
  qcircuit.c: Quantum circuit implementation

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
#include <stdlib.h>
#include <math.h>
#include "qcircuit.h"

struct qstate_info
{
  unsigned int i;
  double p; /* Density function */
  double d; /* Distribution function */
};


/* The following PRNG has been taken from Paul Hsieh's website at
 * http://www.azillionmonkeys.com/qed/random.html
 */

#define RS_SCALE (1.0 / (1.0 + RAND_MAX))

/* A non-overflowing average function */
#define average2scomplement(x,y) ((x) & (y)) + (((x) ^ (y))/2)

int
randbiased (double x)
{
  double p;

  for (;;)
  {
    p = rand () * RS_SCALE;

    if (p >= x)
      return 0;

    if (p + RS_SCALE <= x)
      return 1;
    /* p < x < p+RS_SCALE */
    x = (x - p) * (1.0 + RAND_MAX);
  }

  return 0; /* We will never get here */
}

unsigned int
randslot (const struct qstate_info *slots, unsigned int n)
{
  double xhi;

  /* Select a random range [x,x+RS_SCALE) */
  double x = rand () * RS_SCALE;

  /* Perform binary search to find the intersecting slot */
  unsigned int hi = n - 2, lo = 0, mi, li;
  while (hi > lo)
  {
    mi = average2scomplement (lo, hi);

    if (x >= slots[mi].d)
      lo = mi + 1;
    else
      hi = mi;
  }

  /* Taking slots[-1]=0.0, this is now true: slots[lo-1] <= x < slots[lo] */

  /* If slots[lo-1] <= x < x+RS_SCALE <= slots[lo] then
     any point in [x,x+RS_SCALE) is in [slots[lo-1],slots[lo]) */

  if ((xhi = x + RS_SCALE) <= slots[lo].d)
    return lo;

  /* Otherwise x < slots[lo] < x+RS_SCALE */

  for (;;)
  {
    /* x < slots[lo] < xhi */
    if (randbiased ((slots[lo].d - x) / (xhi - x)))
      return lo;

    x = slots[lo].d;

    lo++;

    if (lo >= n - 1)
      return n - 1;

    /* slots[lo-1] = x <= xhi <= slots[lo] */
    if (xhi <= slots[lo].d)
        return lo;
  }

  /* We will never get here */
  return 0;
}

void
qgate_destroy (qgate_t *gate)
{
  if (gate->name != NULL)
    free (gate->name);

  if (gate->description != NULL)
    free (gate->description);

  if (gate->coef != NULL)
    free (gate->coef);

  if (gate->sparse != NULL)
    qsparse_destroy (gate->sparse);

  free (gate);
}

QBOOL
qgate_init_sparse (qgate_t *gate)
{
  unsigned int i, j;
  unsigned int length;

  length = 1 << gate->order;

  if (gate->sparse != NULL)
  {
    q_set_last_error ("gate sparse matrix already initialized");

    return Q_FALSE;
  }

  if ((gate->sparse = qsparse_new (gate->order)) == NULL)
    return Q_FALSE;

  for (j = 0; j < length; ++j)
    for (i = 0; i < length; ++i)
      if (!QSPARSE_IS_ZERO (gate->coef[i + length * j]))
        if (!qsparse_set (gate->sparse, j, i, gate->coef[i + length * j]))
        {
          qsparse_destroy (gate->sparse);

          gate->sparse = NULL;

          return Q_FALSE;
        }

  return Q_TRUE;
}

qgate_t *
qgate_new (unsigned int order, const char *name, const char *desc, const QCOMPLEX *coef)
{
  unsigned int length;
  qgate_t *new = NULL;

  length = 1 << (order << 1);

  if ((new = calloc (1, sizeof (qgate_t))) == NULL)
    goto fail;

  if ((new->name = strdup (name)) == NULL)
    goto fail;

  if ((new->description = strdup (desc)) == NULL)
      goto fail;

  if ((new->coef = calloc (length, sizeof (QCOMPLEX))) == NULL)
      goto fail;

  new->order = order;

  if (coef != NULL)
  {
    memcpy (new->coef, coef, length);

    if (!qgate_init_sparse (new))
      goto fail;
  }

  return new;

fail:
  if (new != NULL)
    qgate_destroy (new);

  return NULL;
}

void
qgate_debug (const qgate_t *gate)
{
  unsigned int length, i, j;
  length = 1 << gate->order;

  for (j = 0; j < length; ++j)
  {
    for (i = 0; i < length; ++i)
      printf ("%lg(%lg)  ",
              cabs (gate->coef[i + j * length]),
              carg (gate->coef[i + j * length]));

    printf ("\n");
  }
}

QBOOL
qgate_set_coef (qgate_t *gate, const QCOMPLEX *coef)
{
  unsigned int length;
  length = 1 << (gate->order << 1);

  if (gate->sparse != NULL)
  {
    q_set_last_error ("cannot set gate coefficients twice");

    return Q_FALSE;
  }

  memcpy (gate->coef, coef, length);

  return qgate_init_sparse (gate);
}

static unsigned int *
__remap_dup (const unsigned int *remap, unsigned int order)
{
  unsigned int *dup;
  unsigned int i;

  if ((dup = malloc (order * sizeof (unsigned int))) == NULL)
    return NULL;

  for (i = 0; i < order; ++i)
    dup[i] = remap[i];

  return dup;
}

qwiring_t *
qwiring_new (const qgate_t *gate, const unsigned int *remap)
{
  qwiring_t *new;

  if ((new = malloc (sizeof (qgate_t))) == NULL)
    return NULL;

  new->prev = new->next = NULL;

  new->gate  = gate;

  new->__cached = NULL;

  if (remap != NULL)
  {
    if ((new->remap = __remap_dup (remap, gate->order)) == NULL)
    {
      free (new);
      return NULL;
    }
  }
  else if ((new->remap = calloc (gate->order, sizeof (unsigned int))) == NULL)
  {
    free (new);
    return NULL;
  }

  return new;
}

void
qwiring_destroy (qwiring_t *wiring)
{
  if (wiring->__cached != NULL)
    qsparse_destroy (wiring->__cached);

  free (wiring->remap);
  free (wiring);
}

void
qcircuit_destroy (qcircuit_t *circuit)
{
  qwiring_t *this, *next;

  if (circuit->u != NULL)
    qsparse_destroy (circuit->u);

  if (circuit->state != NULL)
    free (circuit->state);

  if (circuit->collapsed != NULL)
    free (circuit->collapsed);

  if (circuit->name != NULL)
    free (circuit->name);

  this = circuit->wiring_head;

  while (this != NULL)
  {
    next = this->next;

    qwiring_destroy (this);

    this = next;
  }

  free (circuit);
}

qcircuit_t *
qcircuit_new (unsigned int order, const char *name)
{
  qcircuit_t *new;
  unsigned int length;

  if ((new = calloc (1, sizeof (qcircuit_t))) == NULL)
    return NULL;

  if ((new->name = strdup (name)) == NULL)
    goto fail;

  new->updated = Q_FALSE;
  new->order   = order;

  length = 1 << order;

  if ((new->state = calloc (length, sizeof (QCOMPLEX))) == NULL)
    goto fail;

  if ((new->collapsed = calloc (length, sizeof (QCOMPLEX))) == NULL)
      goto fail;

  return new;

fail:
  qcircuit_destroy (new);

  return NULL;
}

void
qcircuit_measure_reset (qcircuit_t *circuit)
{
  circuit->collapsed_mask  = 0;
  circuit->measure_result = 0;

  memcpy (circuit->collapsed, circuit->state, (1 << circuit->order) * sizeof (QCOMPLEX));
}

static QBOOL
qcircuit_check_wiring (const qcircuit_t *circuit, const qwiring_t *wiring)
{
  unsigned int i;

  for (i = 0; i < wiring->gate->order; ++i)
    if (wiring->remap[i] >= circuit->order)
      return Q_FALSE;

  return Q_TRUE;
}

QBOOL
qcircuit_append_wiring (qcircuit_t *circuit, qwiring_t *wiring)
{
  if (!qcircuit_check_wiring (circuit, wiring))
  {
    q_set_last_error ("qcircuit_append_wiring: bad remap");
    return Q_FALSE;
  }

  if (circuit->wiring_head == NULL)
  {
    circuit->wiring_head = circuit->wiring_tail = wiring;
    wiring->next = wiring->prev = NULL;
  }
  else
  {
    wiring->prev = circuit->wiring_tail;
    wiring->next = NULL;

    circuit->wiring_tail->next = wiring;
    circuit->wiring_tail = wiring;
  }

  return Q_TRUE;
}

QBOOL
qcircuit_prepend_wiring (qcircuit_t *circuit, qwiring_t *wiring)
{
  if (!qcircuit_check_wiring (circuit, wiring))
  {
    q_set_last_error ("qcircuit_prepend_wiring: bad remap");
    return Q_FALSE;
  }

  if (circuit->wiring_head == NULL)
  {
    circuit->wiring_head = circuit->wiring_tail = wiring;
    wiring->next = wiring->prev = NULL;
  }
  else
  {
    wiring->next = circuit->wiring_head;
    wiring->prev = NULL;

    circuit->wiring_head->prev = wiring;
    circuit->wiring_head = wiring;
  }

  return Q_TRUE;
}

QBOOL
qcircuit_wire (qcircuit_t *circuit, const qgate_t *gate, const unsigned int *remap)
{
  qwiring_t *wiring = NULL;

  if ((wiring = qwiring_new (gate, remap)) == NULL)
    goto fail;

  if (!qcircuit_append_wiring (circuit, wiring))
    goto fail;

  return Q_TRUE;

fail:
  if (wiring != NULL)
    qwiring_destroy (wiring);

  return Q_FALSE;
}

static QBOOL
qwiring_assert_sparse (qcircuit_t *circuit, qwiring_t *wiring)
{
  if (wiring->gate->sparse == NULL)
  {
    q_set_last_error ("cannot assert sparse matrix on uninitialized gate");

    return Q_FALSE;
  }

  if (wiring->__cached == NULL)
    if ((wiring->__cached = qsparse_expand (wiring->gate->sparse, circuit->order, wiring->remap)) == NULL)
      return Q_FALSE;

  return Q_TRUE;
}

QBOOL
qcircuit_update (qcircuit_t *circuit)
{
  qwiring_t *this;
  qsparse_t *u = NULL, *result = NULL;

  if (circuit->u != NULL)
  {
    qsparse_destroy (circuit->u);
    circuit->u = NULL;
    circuit->updated = Q_FALSE;
  }

  if ((u = qsparse_eye_new (circuit->order)) == NULL)
    goto fail;

  this = qcircuit_get_wiring_head (circuit);

  while (this != NULL)
  {
    if (!qwiring_assert_sparse (circuit, this))
      goto fail;

    /* Applying a new gate is equivalent to multiply the gate
     * operator leftwards.
     */
    if ((result = qsparse_mul (this->__cached, u)) == NULL)
      goto fail;

    qsparse_destroy (u);

    u = result;

    this = qwiring_next (this);
  }

  circuit->u = u;

  circuit->updated = Q_TRUE;

  return Q_TRUE;

fail:
  if (u != NULL)
    qsparse_destroy (u);

  if (result != NULL)
    qsparse_destroy (result);

  return Q_FALSE;
}

QBOOL
qcircuit_apply_state (qcircuit_t *circuit, const QCOMPLEX *psi)
{
  if (!circuit->updated)
  {
    q_set_last_error ("qcircuit_apply_state: circuit operator not updated");
    return Q_FALSE;
  }

  qsparse_mul_vec (circuit->u, psi, circuit->state);
  qcircuit_measure_reset (circuit);

  return Q_TRUE;
}

QBOOL
qcircuit_get_state (const qcircuit_t *circuit, QCOMPLEX *psi)
{
  unsigned int i;
  unsigned int length;

  if (!circuit->updated)
  {
    q_set_last_error ("qcircuit_apply_state: circuit operator not updated");
    return Q_FALSE;
  }

  length = 1 << circuit->order;

  for (i = 0; i < length; ++i)
    if (!circuit->collapsed_mask ||
        (circuit->collapsed_mask & i) == circuit->measure_result)
      psi[i] = circuit->collapsed[i];
    else
      psi[i] = 0.0;

  return Q_TRUE;
}

uint64_t
qcircuit_get_measure_bits (const qcircuit_t *circuit)
{
  return circuit->measure_result;
}

static int
qstate_info_compare (const void *a, const void *b)
{
  const struct qstate_info *qa, *qb;

  qa = (const struct qstate_info *) a;
  qb = (const struct qstate_info *) b;

  if (qa->d < qb->d)
    return -1;
  else if (qa->d > qb->d)
    return 1;

  return 0;
}

void
qcircuit_debug_state (const qcircuit_t *circuit)
{
  unsigned int i;
  unsigned int length;

  length = 1 << circuit->order;

  printf ("SYSTEM SUMMARY:\n");
  printf ("---------------------------\n");
  printf ("  Collapsed mask: 0x%x\n", circuit->collapsed_mask);
  printf ("  Measure result: 0x%x\n", circuit->measure_result);
  printf ("  State vector:\n");

  for (i = 0; i < length; ++i)
    if (!circuit->collapsed_mask ||
        (circuit->collapsed_mask & i) == circuit->measure_result)
      printf ("    <%d|psi> = %lg + %lgi\n",
              i,
              creal (circuit->collapsed[i]),
              cimag (circuit->collapsed[i]));

  printf ("---------------------------\n");

}

QBOOL
qcircuit_collapse (qcircuit_t *circuit, uint64_t mask, unsigned int *measure)
{
  unsigned int i, j, k;
  unsigned int qubits;
  unsigned int length;

  /* Measure order and length */
  unsigned int m_order;
  unsigned int m_length;
  uint8_t m_indices[64];

  /* Uncollapsed qubits */
  unsigned int u_order;
  unsigned int u_length;
  uint8_t u_indices[64];

  unsigned int index_full;

  unsigned int state;

  struct qstate_info *qinfo;


  uint64_t saved_mask = mask;

  if (!circuit->updated)
  {
    q_set_last_error ("qcircuit_collapse: circuit operator not updated");
    return Q_FALSE;
  }

  /* Null measure? */
  if (mask == 0)
    return Q_TRUE;

  length = 1 << circuit->order;

  /* If we have bits that have already been measured, we can skip them and
   * update *measure with the previous calculation.
   */

  mask &= ~circuit->collapsed_mask;

  /* Measure is performed against already collapsed qubis. */
  if (mask == 0)
  {
    *measure = circuit->measure_result & saved_mask;
    return Q_TRUE;
  }


  /* Setup of qubits to measure */
  m_order = 0;

  for (i = 0; i < circuit->order; ++i)
    if (BITMAP_HAS_BIT (mask, i))
      m_indices[m_order++] = i;

  m_length = 1 << m_order;

  /* Setup of non-collapsed qubits */
  u_order = 0;

  for (i = 0; i < circuit->order; ++i)
    if (!BITMAP_HAS_BIT (circuit->collapsed_mask | mask, i))
      u_indices[u_order++] = i;

  u_length = 1 << u_order;

  if ((qinfo = malloc (m_length * sizeof (struct qstate_info))) == NULL)
  {
    q_set_last_error ("qcircuit_collapse: memory exhausted");
    return Q_FALSE;
  }


  double total_p = 0.0;

  /* First: setup all bins */
  for (i = 0; i < m_length; ++i)
  {
    qubits = 0;

    for (j = 0; j < m_order; ++j)
      if (BITMAP_HAS_BIT (i, j))
        qubits |= 1 << m_indices[j];

    /* @qubits: enabled bits for this set of states */

    /* Compute probability for this configuration of qubits */
    /* First: setup all qubits shared by this computation */

    qinfo[i].i = circuit->measure_result | qubits;
    qinfo[i].p = 0.0;

    /* Second: iterate through all states having this set of qubits */
    for (j = 0; j < u_length; ++j)
    {
      index_full = qinfo[i].i;

      for (k = 0; k < u_order; ++k)
        if (BITMAP_HAS_BIT (j, k))
          index_full |= 1 << u_indices[k];

      qinfo[i].p += creal (circuit->collapsed[index_full] *
                          conj (circuit->collapsed[index_full]));
    }

    total_p += qinfo[i].p;
  }

  /* Normalize probabilities */
  for (i = 0; i < m_length; ++i)
  {
    qinfo[i].p /= total_p;
    qinfo[i].d  = qinfo[i].p;

    if (i > 0)
      qinfo[i].d += qinfo[i - 1].d;
  }

  /* Third: sort */
  qsort (qinfo, m_length, sizeof (struct qstate_info), qstate_info_compare);

  /* Fourth: collapse */
  state = randslot (qinfo, m_length);
  circuit->measure_result  = qinfo[state].i;
  circuit->collapsed_mask |= mask;

  /* Fifth: update probabilities */
  /* We modify the probability of this state here, so we don't use
   * extra variables or compute the sqrt several times
   */

  qinfo[state].p = sqrt (qinfo[state].p);

  for (j = 0; j < u_length; ++j)
  {
    index_full = qinfo[state].i;

    for (k = 0; k < u_order; ++k)
      if (BITMAP_HAS_BIT (j, k))
        index_full |= 1 << u_indices[k];

    circuit->collapsed[index_full] /= qinfo[state].p;
  }

  *measure = circuit->measure_result & saved_mask;

  return Q_TRUE;

fail:
  return Q_FALSE;
}
