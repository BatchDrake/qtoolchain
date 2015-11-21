#include <stdlib.h>
#include <stdint.h>
#include <string.h>

#include "qsb.h"

void
qsb_init (struct qsb *state, void *buf, uint32_t size)
{
  state->buffer = buf;
  state->size   = size;
  state->ptr    = 0;
}

void
qsb_seek (struct qsb *state, uint32_t pos)
{
  state->ptr = pos;
}

void
qsb_advance (struct qsb *state, uint32_t incr)
{
  state->ptr += incr;
}

uint32_t
qsb_tell (const struct qsb *state)
{
  return state->ptr;
}

QBOOL
qsb_ensure (const struct qsb *state, uint32_t size)
{
  if (state->buffer == NULL)
    return Q_FALSE;

  return state->ptr + size <= state->size;
}

void
qsb_write_uint32_t (struct qsb *state, uint32_t val)
{
  if (state->buffer != NULL)
    if (qsb_ensure (state, sizeof (uint32_t)))
      __qsb_write_uint32_t (state, val);

  state->ptr += sizeof (uint32_t);
}

void
qsb_write_uint64_t (struct qsb *state, uint64_t val)
{
  if (state->buffer != NULL)
    if (qsb_ensure (state, sizeof (uint64_t)))
      __qsb_write_uint64_t (state, val);

  state->ptr += sizeof (uint64_t);
}

void
qsb_write_float (struct qsb *state, float val)
{
  if (state->buffer != NULL)
    if (qsb_ensure (state, sizeof (uint32_t)))
      __qsb_write_float (state, val);

  state->ptr += sizeof (uint32_t);
}

void
qsb_write_double (struct qsb *state, double val)
{
  if (state->buffer != NULL)
    if (qsb_ensure (state, sizeof (uint64_t)))
      __qsb_write_double (state, val);

  state->ptr += sizeof (uint64_t);
}

void
qsb_write_complex (struct qsb *state, QCOMPLEX val)
{
  qsb_write_double (state, creal(val));
  qsb_write_double (state, cimag(val));
}

/* Read functions */

QBOOL
qsb_read_uint32_t (struct qsb *state, uint32_t *val)
{
  if (!qsb_ensure (state, sizeof (uint32_t)))
    return Q_FALSE;

  __qsb_read_uint32_t (state, val);

  state->ptr += sizeof (uint32_t);

  return Q_TRUE;
}

QBOOL
qsb_read_uint64_t (struct qsb *state, uint64_t *val)
{
  if (!qsb_ensure (state, sizeof (uint64_t)))
    return Q_FALSE;

  __qsb_read_uint64_t (state, val);

  state->ptr += sizeof (uint64_t);

  return Q_TRUE;
}

QBOOL
qsb_read_float (struct qsb *state, float *val)
{
  if (!qsb_ensure (state, sizeof (uint32_t)))
      return Q_FALSE;

  __qsb_read_float (state, val);

  state->ptr += sizeof (uint32_t);

  return Q_TRUE;
}

QBOOL
qsb_read_double (struct qsb *state, double *val)
{
  if (!qsb_ensure (state, sizeof (uint64_t)))
      return Q_FALSE;

  __qsb_read_double (state, val);

  state->ptr += sizeof (uint64_t);

  return Q_TRUE;
}

QBOOL
qsb_read_complex (struct qsb *state, QCOMPLEX *val)
{
  double real, imag;

  if (!qsb_ensure (state, 2 * sizeof (uint64_t)))
    return Q_FALSE;

  __qsb_read_double (state, &real);

  state->ptr += sizeof (uint64_t);

  __qsb_read_double (state, &imag);

  state->ptr += sizeof (uint64_t);

  *val = real + I * imag;

  return Q_TRUE;
}

