/*
  qsparse.h: QToolChain's sparse matrices

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

#ifndef _LIBQ_QSPARSE_H
#define _LIBQ_QSPARSE_H

#include "q_defines.h"
#include "q_util.h"

#define QSPARSE_ORDER_MAX 8
#define QSPARSE_INLINE_ORDER_MAX 6
#define QSPARSE_INLINE_BITMAP_MASK ((1ull << (QSPARSE_INLINE_ORDER_MAX)) - 1)

#define BITMAP_HAS_BIT(bitmap, id) ((bitmap) & (1ull << (id)))
#define QSPARSE_LENGTH(qsparse) (1 << (qsparse)->order)
#define QSPARSE_USES_INLINE(qsparse) ((qsparse)->order <= QSPARSE_INLINE_ORDER_MAX)
#define QSPARSE_IS_ZERO(x) ((x) == 0.0)

struct qsparse_row
{
  QCOMPLEX *coef;

  /* Most of the time we don't allocate everything, just the relevant
   * coefficients. One solution could be to save from the first to the
   * last coefficients. However, if we want to store a new coefficient
   * right before the first one (or right after the last one) we would
   * require to a) reallocate memory and b) move all coefficients to
   * its new relative position. Both operations (specially the second
   * one) are hard. The tradeoff is solved aligning the starts and
   * the sizes to the closest power of two.
   */
  unsigned int allocation_start;
  unsigned int allocation_size;

  union
  {
    uint64_t bitmap_long; /* If order <= 6 */
    uint64_t *bitmap_buf;  /* If order > 6 */
  };
};

struct qsparse
{
  unsigned int order; /* Logarithmic order (i.e. number of qubits) */

  /* Non-zero counters. Used for accelerating products */
  QNZCOUNT *row_nz;
  QNZCOUNT *col_nz;

  struct qsparse_row *headers;
};

struct qsparse_iterator
{
  const struct qsparse *sparse;
  unsigned int row_index;
  unsigned int col_index;
};

typedef struct qsparse qsparse_t;
typedef struct qsparse_iterator qsparse_iterator_t;

static inline
unsigned int qsparse_iterator_col (const qsparse_iterator_t *it)
{
  return it->col_index;
}

static inline
unsigned int qsparse_iterator_row (const qsparse_iterator_t *it)
{
  return it->row_index;
}

qsparse_t *qsparse_new (unsigned int);
qsparse_t *qsparse_eye_new (unsigned int);
qsparse_t *qsparse_expand (const qsparse_t *, unsigned int, unsigned int *);
qsparse_t *qsparse_contract (const qsparse_t *, unsigned int, unsigned int *);

void qsparse_iterator_init (const qsparse_t *, qsparse_iterator_t *);
QBOOL qsparse_iterator_next (qsparse_iterator_t *);
QBOOL qsparse_iterator_end (qsparse_iterator_t *);

QCOMPLEX qsparse_get (const qsparse_t *, unsigned int, unsigned int);
QCOMPLEX qsparse_get_from_iterator (const qsparse_t *, const qsparse_iterator_t *);

QBOOL qsparse_set (qsparse_t *, unsigned int, unsigned int, QCOMPLEX);
QBOOL qsparse_set_from_iterator (qsparse_t *, const qsparse_iterator_t *, QCOMPLEX);

QBOOL qsparse_det (const qsparse_t *, QCOMPLEX *); /* Compute determinant */

int qsparse_contractable (const qsparse_t *, unsigned int, unsigned int *);
qsparse_t *qsparse_copy (const qsparse_t *);
QBOOL qsparse_apply (qsparse_t *, const qsparse_t *);
qsparse_t *qsparse_mul (const qsparse_t *, const qsparse_t *);

void qsparse_destroy (qsparse_t *);

uint32_t qsparse_serialize (const qsparse_t *, void *, uint32_t);
qsparse_t *qsparse_deserialize (const void *, uint32_t);

void qsparse_set_last_error (const char *, ...);
const char *qsparse_get_last_error (void);

void qsparse_debug (const qsparse_t *);

QCOMPLEX *qsparse_alloc_vec (const qsparse_t *);
void qsparse_mul_vec (const qsparse_t *, const QCOMPLEX *, QCOMPLEX *);

#endif /* _LIBQ_QSPARSE_H */
