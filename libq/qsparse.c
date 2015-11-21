#include <stdlib.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>

#include "qsparse.h"
#include "qsb.h"

int
qsparse_row_init (struct qsparse_row *row, unsigned int order)
{
  unsigned int length;
  
  memset (row, 0, sizeof (struct qsparse_row));

  if (order > QSPARSE_INLINE_ORDER_MAX)
  {
    length = (1 << (order - QSPARSE_INLINE_ORDER_MAX));
    
    if ((row->bitmap_buf = calloc (length, sizeof (uint64_t))) == NULL)
    {
      q_set_last_error ("qsparse_row_init: memory exhausted while allocating row");
      return Q_FALSE;
    }
  }

  return Q_TRUE;
}

qsparse_t *
qsparse_new (unsigned int order)
{
  qsparse_t *new;
  unsigned int length, i;

  length = 1 << order;
  
  if (order > QSPARSE_ORDER_MAX)
  {
    q_set_last_error ("qsparse_new: qsparse matrix order %d (%ux%u) too big", order, length, length);
    return NULL;
  }
  
  if ((new = malloc (sizeof (qsparse_t))) == NULL)
    return NULL;

  new->order = order;

  if ((new->headers = malloc (sizeof (struct qsparse_row) * length)) == NULL)
  {
    q_set_last_error ("qsparse_init: memory exhausted while allocating matrix");
    goto fail;
  }

  /* Non-zero counters */
  if ((new->row_nz = calloc (sizeof (QNZCOUNT), length)) == NULL ||
      (new->col_nz = calloc (sizeof (QNZCOUNT), length)) == NULL)
  {
    q_set_last_error ("qsparse_init: memory exhausted while allocating NZ counters");
    goto fail;
  }

  for (i = 0; i < length; ++i)
    if (!qsparse_row_init (&new->headers[i], order))
      goto fail;
  
  return new;
  
fail:
  qsparse_destroy (new);

  return NULL;
}

qsparse_t *
qsparse_eye_new (unsigned int order)
{
  qsparse_t *new;
  unsigned int i, length;

  if ((new = qsparse_new (order)) == NULL)
    return NULL;

  length = QSPARSE_LENGTH (new);

  for (i = 0; i < length; ++i)
    if (!qsparse_set (new, i, i, 1.))
      goto fail;

  return new;

fail:
  qsparse_destroy (new);

  return NULL;
}

static inline int
__qsparse_coef_is_nz (const qsparse_t *qsparse, unsigned int i, unsigned int j)
{
  return qsparse->order <= QSPARSE_INLINE_ORDER_MAX ?
      !!BITMAP_HAS_BIT (qsparse->headers[i].bitmap_long, j) :
      !!BITMAP_HAS_BIT (qsparse->headers[i].bitmap_buf[j >> QSPARSE_INLINE_ORDER_MAX],
                      j & QSPARSE_INLINE_BITMAP_MASK);
}

static inline QBOOL
qsparse_row_is_empty (const qsparse_t *qsparse, unsigned int i)
{
  return qsparse->row_nz[i] == 0;
}

static inline QBOOL
qsparse_col_is_empty (const qsparse_t *qsparse, unsigned int i)
{
  return qsparse->col_nz[i] == 0;
}

static inline QCOMPLEX *
qsparse_row_get_col_ptr (const struct qsparse_row *row, unsigned int col)
{
  if (col < row->allocation_start ||
      col >= row->allocation_start + row->allocation_size)
    return NULL;

  return &row->coef[col - row->allocation_start];
}

static inline void
qsparse_row_get_allocation (unsigned int *start, unsigned int *size)
{
  unsigned int i;
  unsigned int rounded_size = 1;
  unsigned int rounded_start;
  /* Align the size to the closest power of two */

  while (*size > rounded_size)
    rounded_size <<= 1;

  /* We can see each matrix row as a set of rounded_size-sized blocks.
   * Now we align the start index to this block layout.
   */

  rounded_start = (*start / rounded_size) * rounded_size;

  if (rounded_start + rounded_size < *start + *size)
    rounded_size <<= 1;

  *start = rounded_start;
  *size  = rounded_size;
}

QCOMPLEX
qsparse_get (const qsparse_t *qsparse, unsigned int row, unsigned int col)
{
  unsigned int length;
  const QCOMPLEX *val;

  length = QSPARSE_LENGTH (qsparse);

  if (row >= length)
    return 0.0;

  if ((val = qsparse_row_get_col_ptr (&qsparse->headers[row], col)) == NULL)
    return 0.0;

  return *val;
}

QCOMPLEX
qsparse_get_from_iterator (const qsparse_t *qsparse, const qsparse_iterator_t *it)
{
  return qsparse_get (
      qsparse,
      qsparse_iterator_row (it),
      qsparse_iterator_col (it)
      );
}

QBOOL
qsparse_set_from_iterator (qsparse_t *qsparse, const qsparse_iterator_t *it, QCOMPLEX val)
{
  return qsparse_set (
      qsparse,
      qsparse_iterator_row (it),
      qsparse_iterator_col (it),
      val
      );
}

static inline QCOMPLEX
__qsparse_det (const qsparse_t *qsparse, uint64_t *mask)
{
  unsigned int i = 0, n = 0, j;
  unsigned int length;

  unsigned int col1, col2;
  QCOMPLEX det = 0;

  length = QSPARSE_LENGTH (qsparse);

  for (i = 0; i < length; ++i)
  {
    if (BITMAP_HAS_BIT (mask[i >> QSPARSE_INLINE_ORDER_MAX],
                        i & QSPARSE_INLINE_BITMAP_MASK))
    {
      if (++n == 1)
        col1 = i;
      else if (n == 2)
        col2 = i;
    }
  }

  if (n == 1)
    det = qsparse_get (qsparse, length - 1, col1);
  else if (n == 2)
    det =
        qsparse_get (qsparse, length - 2, col1) *
        qsparse_get (qsparse, length - 1, col2) -
        qsparse_get (qsparse, length - 2, col2) *
        qsparse_get (qsparse, length - 1, col1);
  else
  {
    for (i = 0; i < length; ++i)
    {
      j = i >> QSPARSE_INLINE_ORDER_MAX;

      if (BITMAP_HAS_BIT (mask[j], i))
      {
        if (__qsparse_coef_is_nz (qsparse, length - n, i))
        {
          mask[j] &= ~(1ull << i);

          det += ((n + i) & 1 ? -1 : 1) *
              qsparse_get (qsparse, length - n, i) *
              __qsparse_det (qsparse, mask);

          mask[j] |= 1ull << i;
        }
      }
    }
  }

  return det;
}

QBOOL
qsparse_det (const qsparse_t *qsparse, QCOMPLEX *det)
{
  uint64_t *mask;
  unsigned int length;

  length = QSPARSE_LENGTH (qsparse);

  if (length < (sizeof (uint64_t) << 3))
    length = sizeof (uint64_t) << 3;

  if ((mask = malloc (length >> 3)) == NULL)
  {
    q_set_last_error ("qsparse_det: memory exhausted when computing determinant");
    return Q_FALSE;
  }

  memset (mask, 0xff, length >> 3);

  /* Pass full mask */
  *det = __qsparse_det (qsparse, mask);

  free (mask);

  return Q_TRUE;
}

static inline void
__qsparse_update_counter (qsparse_t *qsparse, unsigned int row, unsigned int col, int incr)
{
  qsparse->col_nz[col] += incr;
  qsparse->row_nz[row] += incr;
}

QBOOL
qsparse_set (qsparse_t *qsparse, unsigned int row, unsigned int col, QCOMPLEX value)
{
  unsigned int length;
  struct qsparse_row *rowptr;
  QCOMPLEX *new;
  unsigned int new_start, new_size;

  length = QSPARSE_LENGTH (qsparse);

  if (row >= length || col >= length)
  {
    q_set_last_error ("qsparse_set: coefficient indices (%d, %d) out of bounds", row, col);

    return Q_FALSE;
  }

  rowptr = &qsparse->headers[row];

  if (rowptr->coef == NULL)
  {
    if (QSPARSE_IS_ZERO (value))
      return Q_TRUE;

    if ((rowptr->coef = malloc (sizeof (QCOMPLEX))) == NULL)
    {
      q_set_last_error ("qsparse_set: memory exhausted while allocating row");

      return Q_FALSE;
    }

    rowptr->allocation_start = col;
    rowptr->allocation_size = 1;
    rowptr->coef[0] = value;
  }
  else
  {
    new_size = rowptr->allocation_size;

    if (col < rowptr->allocation_start)
    {
      new_start = col;
      new_size  = rowptr->allocation_size + rowptr->allocation_start - col;

      qsparse_row_get_allocation (&new_start, &new_size);
    }
    else if (col >= rowptr->allocation_start + rowptr->allocation_size)
    {
      new_start = rowptr->allocation_start;
      new_size  = col - rowptr->allocation_start + 1;

      qsparse_row_get_allocation (&new_start, &new_size);
    }

    if (new_size != rowptr->allocation_size)
    {
      if (QSPARSE_IS_ZERO (value))
        return Q_TRUE;

      if ((new = calloc (new_size, sizeof (QCOMPLEX))) == NULL)
      {
        q_set_last_error ("qsparse_set: memory exhausted while increasing row size (cannot allocate %d elements)", new_size);

        return Q_FALSE;
      }

      memcpy (&new[rowptr->allocation_start - new_start], rowptr->coef, rowptr->allocation_size * sizeof (QCOMPLEX));

      free (rowptr->coef);

      rowptr->coef = new;
      rowptr->allocation_start = new_start;
      rowptr->allocation_size  = new_size;
    }

    rowptr->coef[col - rowptr->allocation_start] = value;
  }

  /* Update matrix metadata */
  if (qsparse->order <= QSPARSE_INLINE_ORDER_MAX)
  {
    if (QSPARSE_IS_ZERO (value) && (rowptr->bitmap_long & (1ull << col)))
    {
      rowptr->bitmap_long &= ~(1ull << col);
      __qsparse_update_counter (qsparse, row, col, -1);
    }
    else if (!QSPARSE_IS_ZERO (value) && !(rowptr->bitmap_long & (1ull << col)))
    {
      rowptr->bitmap_long |= 1ull << col;
      __qsparse_update_counter (qsparse, row, col, 1);
    }
  }
  else
  {
    if (QSPARSE_IS_ZERO (value) && (rowptr->bitmap_buf[col >> QSPARSE_INLINE_ORDER_MAX] & (1ull << (col & QSPARSE_INLINE_BITMAP_MASK))))
    {
      rowptr->bitmap_buf[col >> QSPARSE_INLINE_ORDER_MAX] &= ~(1ull << (col & QSPARSE_INLINE_BITMAP_MASK));
      __qsparse_update_counter (qsparse, row, col, -1);
    }
    else if (!QSPARSE_IS_ZERO (value) && !(rowptr->bitmap_buf[col >> QSPARSE_INLINE_ORDER_MAX] & (1ull << (col & QSPARSE_INLINE_BITMAP_MASK))))
    {
      rowptr->bitmap_buf[col >> QSPARSE_INLINE_ORDER_MAX] |= 1ull << (col & QSPARSE_INLINE_BITMAP_MASK);
      __qsparse_update_counter (qsparse, row, col, 1);
    }
  }

  return Q_TRUE;
}


static inline int
qsparse_find_next_nz (const qsparse_t *qsparse, unsigned int *curr_row, unsigned int *curr_col)
{
  unsigned int i, j;
  unsigned int length;
  unsigned int bitmap;
  unsigned int col_start;
  int id, bit;

  length = QSPARSE_LENGTH (qsparse);

  col_start = *curr_col;

  for (i = *curr_row; i < length; ++i)
    if (!qsparse_row_is_empty (qsparse, i))
    {
      if (QSPARSE_USES_INLINE (qsparse))
        for (j = col_start; j < length; ++j)
        {
          if (BITMAP_HAS_BIT(qsparse->headers[i].bitmap_long, j))
          {
            *curr_row = i;
            *curr_col = j;

            return 1;
          }
        }
      else
      {
        for (j = col_start; j < length; ++j)
        {
          bitmap = j >> QSPARSE_INLINE_ORDER_MAX;

          if (BITMAP_HAS_BIT(qsparse->headers[i].bitmap_buf[bitmap],
                             j & QSPARSE_INLINE_BITMAP_MASK))
          {
            *curr_row = i;
            *curr_col = j;

            return 1;
          }
        }
      }

      col_start = 0;
    }

  return 0;
}

void
qsparse_iterator_init (const qsparse_t *qsparse, qsparse_iterator_t *it)
{
  unsigned int i;
  unsigned int length;

  length = QSPARSE_LENGTH (qsparse);

  it->sparse = qsparse;
  it->col_index = 0;
  it->row_index = 0;

  (void) qsparse_find_next_nz (qsparse, &it->row_index, &it->col_index);
}

int
qsparse_iterator_next (qsparse_iterator_t *it)
{
  unsigned int nrow, ncol;
  int ret = 1;

  ncol = it->col_index + 1;
  nrow = it->row_index;

  if (ncol >= QSPARSE_LENGTH (it->sparse))
  {
    ncol = 0;
    ++nrow;
  }

  if (!qsparse_find_next_nz (it->sparse, &nrow, &ncol))
    ret = 0;

  it->row_index = nrow;
  it->col_index = ncol;

  return ret;
}

QBOOL
qsparse_iterator_end (qsparse_iterator_t *it)
{
  unsigned int nrow, ncol;

  ncol = it->col_index;
  nrow = it->row_index;

  return !qsparse_find_next_nz (it->sparse, &nrow, &ncol);
}

/* Expansion and contraction works the following way:

   - Each column of the matrix is mapped to a given eigenstate of
     the whole system. Eigenstates are mapped to combinations of
     zeroes and ones.
   - To identify a eigenstate with a column of the matrix, we can
     assume the following convention:

     Bit N of the row index: Eigenstate of the qubit Q(N)

     This is, column 3 (011) refers to the eigenstate:

     Q(0) = 1
     Q(1) = 1
     Q(2) = 0
     
   - The matrix has the following arrangement:

      Q(2) Q(1) Q(0)  Column ID
     +----+-----+---+
     |  0 |  0  | 0 |  #0
     |    |     +---+       
     |  . |  .  | 1 |  #1
     |    +-----+---+
     |  . |  1  | 0 |  #2
     |    +     +---+
     |    |  .  | 1 |  #3
     +----+-----+---+
     |  1 |  0  | 0 |  #4
     |    |     +---+       
     |  . |  .  | 1 |  #5
     |    +-----+---+
     |  . |  1  | 0 |  #6
     |    +     +---+
     |    |  .  | 1 |  #7
     +----+-----+---+
       
     We see then a block structure: the Q(k) qubit switches
     each 2^k columns. If we want to remap a N qbit matrix
     to a bigger M qbit matrix like this:

     Q(1) --> Q(0)
     Q(0) --> Q(2)
     Q(2) --> Q(3)

     Values will be copied 2x2^(M - N) = 2x2 = 4 times. The best
     way to proceed is the following:

     1. Iterate through all elements of existing 2^N matrix
       2. Iterate through all non-mapped qubits (in rows & cols)
         3. Translate indexes to get new rows and cols
           4. Copy
     
*/

qsparse_t *
qsparse_expand (const qsparse_t *src, unsigned int order, unsigned int *remap)
{
  unsigned int i, j, k;
  qsparse_t *new = NULL;
  qsparse_iterator_t it;
  uint64_t used_bits = 0;

  unsigned int unmapped_order = 0;
  unsigned int unmapped_length;
  uint8_t *unmapped_bits = NULL;
  unsigned int expanded_length;
  unsigned int n = 0;

  unsigned int it_row, it_col;
  unsigned int row, col;

  if (order < src->order)
  {
    q_set_last_error ("qsparse_expand: trying to expand matrix to lower order");
    goto fail;
  }

  for (i = 0; i < src->order; ++i)
  {
    if (remap[i] >= order)
    {
      q_set_last_error ("qsparse_expand: index remap out of bounds (%d -> %d)", i, remap[i]);
      goto fail;
    }
    else if (BITMAP_HAS_BIT (used_bits, remap[i]))
    {
      q_set_last_error ("qsparse_expand: index remapped to the same state twice (%d -> %d)", i, remap[i]);
      goto fail;
    }

    used_bits |= 1ull << remap[i];
  }

  unmapped_order = order - src->order;
  unmapped_length = 1 << unmapped_order;

  if ((unmapped_bits = malloc (unmapped_order * sizeof (uint8_t))) == NULL)
  {
    q_set_last_error ("qsparse_expand: memory exhausted");
    goto fail;
  }

  for (i = 0; i < order; ++i)
    if (!BITMAP_HAS_BIT (used_bits, i))
      unmapped_bits[n++] = i;

  assert (n == unmapped_order);

  if ((new = qsparse_new (order)) == NULL) /* Error already given by qsparse_new */
    goto fail;

  expanded_length = QSPARSE_LENGTH (new);

  for (
        qsparse_iterator_init (src, &it);
        !qsparse_iterator_end (&it);
        qsparse_iterator_next (&it)
        )
  {
    /* TODO: Compose index bits from the iterator */

    it_row = 0;
    it_col = 0;

    for (k = 0; k < src->order; ++k)
    {
      if (BITMAP_HAS_BIT (qsparse_iterator_col (&it), k))
        it_col |= 1 << remap[k];

      if (BITMAP_HAS_BIT (qsparse_iterator_row (&it), k))
        it_row |= 1 << remap[k];
    }

    for (i = 0; i < unmapped_length; ++i)
    {
      row = it_row;
      col = it_col;

      for (k = 0; k < unmapped_order; ++k)
        if (BITMAP_HAS_BIT (i, k))
        {
          row |= 1 << unmapped_bits[k];
          col |= 1 << unmapped_bits[k];
        }

      qsparse_set (new, col, row, qsparse_get_from_iterator (src, &it));
    }
  }

  free (unmapped_bits);

  return new;

fail:
  if (unmapped_bits != NULL)
    free (unmapped_bits);

  if (new != NULL)
    qsparse_destroy (new);

  return NULL;
}

void
qsparse_destroy (qsparse_t *qsparse)
{
  int i;
  unsigned int length;

  length = QSPARSE_LENGTH (qsparse);

  if (qsparse->headers != NULL)
  {
    for (i = 0; i < length; ++i)
    {
      if (qsparse->headers[i].coef != NULL)
        free (qsparse->headers[i].coef);

      if (qsparse->order > QSPARSE_INLINE_ORDER_MAX)
        if (qsparse->headers[i].bitmap_buf != NULL)
          free (qsparse->headers[i].bitmap_buf);
    }

    free (qsparse->headers);
  }

  if (qsparse->col_nz != NULL)
    free (qsparse->col_nz);

  if (qsparse->row_nz != NULL)
      free (qsparse->row_nz);

  free (qsparse);
}

void
qsparse_debug (const qsparse_t *qsparse)
{
  unsigned int i, j;
  unsigned int length;

  length = QSPARSE_LENGTH (qsparse);

  for (i = 0; i < length; ++i)
  {
    printf ("(nz: %3d) |", qsparse->row_nz[i]);

    for (j = 0; j < length; ++j)
    {
      if (j > 0)
        printf (" ");

      printf ("%2.2lg", cabs (qsparse_get (qsparse, i, j)));
    }
    printf ("|\n");
  }
}

/* Serialization code is, as it couldn't be otherwise, ugly as hell */
uint32_t
qsparse_serialize (const qsparse_t *sparse, void *buffer, uint32_t size)
{
  struct qsb s;
  uint64_t bitmap;
  uint32_t required_size;
  unsigned int i, j, rowgroups, groupsize;
  unsigned int length;
  qsparse_iterator_t it;

  length = QSPARSE_LENGTH (sparse);

  qsb_init (&s, buffer, size);

  /* Placeholder for total buffer size */
  qsb_seek (&s, sizeof (uint32_t));

  /* Store order */
  qsb_write_uint32_t (&s, sparse->order);

  /* Store row allocation bitmap */
  rowgroups = length >> QSPARSE_INLINE_ORDER_MAX;

  if (rowgroups == 0)
  {
    rowgroups = 1;
    groupsize = length;
  }
  else
    groupsize = 1 << QSPARSE_INLINE_ORDER_MAX;

  for (i = 0; i < rowgroups; ++i)
  {
    bitmap = 0;

    for (j = 0; j < groupsize; ++j)
      if (!qsparse_row_is_empty (
          sparse, (i << QSPARSE_INLINE_ORDER_MAX) + j))
        bitmap |= 1ull << j;

    /* Store bitmap */
    qsb_write_uint64_t (&s, bitmap);
  }

  /* Store column allocation bitmaps */
  for (i = 0; i < length; ++i)
    if (!qsparse_row_is_empty (sparse, i))
    {
      if (sparse->order <= QSPARSE_INLINE_ORDER_MAX)
        qsb_write_uint64_t (&s, sparse->headers[i].bitmap_long);
      else
        for (j = 0; j < rowgroups; ++j)
          qsb_write_uint64_t (&s, sparse->headers[i].bitmap_buf[j]);
    }

  /* Store coefficients */
  for (
          qsparse_iterator_init (sparse, &it);
          !qsparse_iterator_end (&it);
          qsparse_iterator_next (&it)
          )
      qsb_write_complex (&s, qsparse_get_from_iterator (sparse, &it));

  /* Save required size in header */
  required_size = qsb_tell (&s);

  qsb_seek (&s, 0);
  qsb_write_uint32_t (&s, required_size);

  /* Return required size */
  return required_size;
}

qsparse_t *
qsparse_deserialize (const void *buffer, uint32_t size)
{
  uint32_t order, ssize;
  struct qsb s;
  uint32_t length;
  uint64_t *row_bitmap = NULL;
  uint64_t *col_bitmap = NULL;

  unsigned int row_bitmap_size;
  unsigned int row_bitmap_words;
  unsigned int i, j;
  unsigned int n = 0;
  qsparse_iterator_t it;

  double v1, v2;

  QCOMPLEX val;

  qsparse_t *new = NULL;

  /* Safe as we shouldn't be calling write functions */
  qsb_init (&s, (void *) buffer, size);

  /* Read serialized buffer size */
  if (!qsb_read_uint32_t (&s, &ssize))
  {
    q_set_last_error ("Cannot read buffer length from input");
    goto fail;
  }

  if (ssize > size)
  {
    q_set_last_error ("Input buffer size too short");
    goto fail;
  }

  /* Read order */
  if (!qsb_read_uint32_t (&s, &order))
  {
    q_set_last_error ("Cannot read matrix order from input");
    goto fail;
  }

  if (order > QSPARSE_ORDER_MAX)
  {
    q_set_last_error ("Matrix order too big");
    goto fail;
  }

  length = 1 << order;

  row_bitmap_size = length >> 3;
  row_bitmap_words = length >> QSPARSE_INLINE_ORDER_MAX;

  if (row_bitmap_words == 0)
  {
    row_bitmap_size  = sizeof (uint64_t);
    row_bitmap_words = 1;
  }

  if ((row_bitmap = malloc (row_bitmap_size)) == NULL)
  {
    q_set_last_error ("Memory exhausted while allocating bitmaps");
    goto fail;
  }

  if ((col_bitmap = malloc (row_bitmap_size)) == NULL)
  {
    q_set_last_error ("Memory exhausted while allocating bitmaps");
      goto fail;
  }

  /* Read row allocation bitmap */
  if (!qsb_ensure (&s, row_bitmap_size))
  {
    q_set_last_error ("Unexpected end-of-buffer while retrieving row allocation bitmap");
    goto fail;
  }

  printf ("With order %d, I have %d bitmap words\n", order, row_bitmap_words);

  for (i = 0; i < row_bitmap_words; ++i)

  {
    __qsb_read_uint64_t (&s, &row_bitmap[i]);
    qsb_advance (&s, sizeof (uint64_t));
  }

  /* Error set by qsparse_new */
  if ((new = qsparse_new (order)) == NULL)
    goto fail;

  /* Read all coefficients */

  /* For each row */
  for (i = 0; i < length; ++i)
    if (BITMAP_HAS_BIT (row_bitmap[i >> QSPARSE_INLINE_ORDER_MAX],
                        i & QSPARSE_INLINE_BITMAP_MASK))
    {
      /* Row i is not empty, load */
      if (!qsb_ensure (&s, row_bitmap_size))
      {
        q_set_last_error ("Unexpected end-of-buffer while retrieving column allocation bitmap");
        goto fail;
      }

      /* Load column allocation bitmap */
      for (j = 0; j < row_bitmap_words; ++j)
      {
        __qsb_read_uint64_t (&s, &col_bitmap[j]);
        qsb_advance (&s, sizeof (uint64_t));
      }

      /* Allocate memory */
      for (j = 0; j < length; ++j)
          if (BITMAP_HAS_BIT (col_bitmap[j >> QSPARSE_INLINE_ORDER_MAX],
                              j & QSPARSE_INLINE_BITMAP_MASK))
          {
            if (!qsparse_set (new, i, j, 1))
              goto fail;
            ++n;
          }
    }

  /* Bitmaps have been transferred to the destination matrix. Now we can
   * read all coefficients using iterators.
   */

  if (!qsb_ensure (&s, n * sizeof (QCOMPLEX)))
  {
    q_set_last_error ("Unexpected end-of-buffer while retrieving coefficients");
    goto fail;
  }

  /* Store coefficients */
    for (
            qsparse_iterator_init (new, &it);
            !qsparse_iterator_end (&it);
            qsparse_iterator_next (&it)
            )
    {
      __qsb_read_double (&s, &v1);
      qsb_advance (&s, sizeof (uint64_t));
      __qsb_read_double (&s, &v2);
      qsb_advance (&s, sizeof (uint64_t));

      val = v1 + I * v2;

      if (!qsparse_set_from_iterator (new, &it, val))
        goto fail;
    }

  free (row_bitmap);
  free (col_bitmap);

  return new;

fail:
  if (row_bitmap != NULL)
    free (row_bitmap);

  if (col_bitmap != NULL)
    free (col_bitmap);

  if (new != NULL)
    qsparse_destroy (new);

  return NULL;
}

qsparse_t *
qsparse_mul (const qsparse_t *a, const qsparse_t *b)
{
  qsparse_t *new = NULL;
  unsigned int i, j, k;
  unsigned int length;

  QCOMPLEX prod;

  if (a->order != b->order)
  {
    q_set_last_error ("qsparse_apply: order mismatch");
    goto fail;
  }

  length = QSPARSE_LENGTH (a);

  if ((new = qsparse_new (a->order)) == NULL)
    goto fail;

  /*
   * i: row number in A
   * j: col number in B
   */

  for (j = 0; j < length; ++j)
    if (b->col_nz[j] > 0) /* Is column J of B nonempty? */
      for (i = 0; i < length; ++i)
        if (a->row_nz[i] > 0) /* Is row I of A nonempty? */
        {
          prod = 0;

          for (k = 0; k < length; ++k)
            if (__qsparse_coef_is_nz (a, i, k) &&
                __qsparse_coef_is_nz (b, k, j))
              prod += qsparse_get (a, i, k) * qsparse_get (b, k, j);

          if (!qsparse_set (new, i, j, prod))
            goto fail;
        }

  return new;

fail:
  if (new == NULL)
    qsparse_destroy (new);

  return NULL;
}

QCOMPLEX *
qsparse_alloc_vec (const qsparse_t *sparse)
{
  unsigned int length;

  length = QSPARSE_LENGTH (sparse);

  return (QCOMPLEX *) calloc (length, sizeof (QCOMPLEX));
}

void
qsparse_mul_vec (const qsparse_t *sparse, const QCOMPLEX *x, QCOMPLEX *y)
{
  unsigned int length;
  unsigned int i, j;
  unsigned int start, size;
  QCOMPLEX prod;

  length = QSPARSE_LENGTH (sparse);

  for (i = 0; i < length; ++i)
  {
    prod = 0.0;

    if (sparse->row_nz[i] > 0)
    {
      size  = sparse->headers[i].allocation_size;
      start = sparse->headers[i].allocation_start;

      for (j = 0; j < size; ++j)
        if (__qsparse_coef_is_nz (sparse, i, j + start))
          prod += x[j + start] * qsparse_get (sparse, i, j + start);
    }

    y[i] = prod;
  }
}


