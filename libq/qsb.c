/*
  qsb.c: QToolChain's serializable buffers

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
qsb_advance (struct qsb *state, int32_t incr)
{
  state->ptr += incr;
}

uint32_t
qsb_tell (const struct qsb *state)
{
  return state->ptr;
}

uint8_t *
qsb_bufptr (const struct qsb *state)
{
  if (state->ptr > state->size)
      return NULL;

  return state->bytes + state->ptr;
}

uint32_t
qsb_remainder (const struct qsb *state)
{
  if (state->ptr > state->size)
    return 0;

  return state->size - state->ptr;
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

  state->ptr += QSB_FLOAT_SERIALIZED_SIZE;
}

void
qsb_write_double (struct qsb *state, double val)
{
  if (state->buffer != NULL)
    if (qsb_ensure (state, sizeof (uint64_t)))
      __qsb_write_double (state, val);

  state->ptr += QSB_DOUBLE_SERIALIZED_SIZE;
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
  if (!qsb_ensure (state, QSB_FLOAT_SERIALIZED_SIZE))
    return Q_FALSE;

  __qsb_read_float (state, val);

  state->ptr += QSB_FLOAT_SERIALIZED_SIZE;

  return Q_TRUE;
}

QBOOL
qsb_read_double (struct qsb *state, double *val)
{
  if (!qsb_ensure (state, QSB_DOUBLE_SERIALIZED_SIZE))
    return Q_FALSE;

  __qsb_read_double (state, val);

  state->ptr += QSB_DOUBLE_SERIALIZED_SIZE;

  return Q_TRUE;
}

QBOOL
qsb_read_complex (struct qsb *state, QCOMPLEX *val)
{
  double real, imag;

  if (!qsb_ensure (state, QSB_QCOMPLEX_SERIALIZED_SIZE))
    return Q_FALSE;

  __qsb_read_double (state, &real);

  state->ptr += QSB_DOUBLE_SERIALIZED_SIZE;

  __qsb_read_double (state, &imag);

  state->ptr += QSB_DOUBLE_SERIALIZED_SIZE;

  *val = real + I * imag;

  return Q_TRUE;
}



void
qsb_write_string (struct qsb *state, const char *string)
{
  uint32_t len;
  uint32_t rem;

  len = strlen (string) + 1;

  qsb_write_uint32_t (state, len);

  rem = qsb_remainder (state);

  if (rem > len)
    rem = len;

  if (rem > 0)
    memcpy (&state->bytes[state->ptr], string, rem);

  qsb_advance (state, len);
}

QBOOL
qsb_read_string (struct qsb *state, char **string)
{
  uint32_t len;
  const char *ref;

  if (!qsb_ensure (state, sizeof (uint32_t)))
    return Q_FALSE;

  __qsb_read_uint32_t (state, &len);

  /* We either read the whole string or we don't read it at all */
  if (!qsb_ensure (state, sizeof (uint32_t) + len))
    return Q_FALSE;

  state->ptr += sizeof (uint32_t);

  ref = (const char *) &state->bytes[state->ptr];

  state->ptr += len;

  if (string != NULL && (*string = malloc (len)) != NULL)
  {
    memcpy (*string, ref, len);
    (*string)[len - 1] = '\0';
  }

  return Q_FALSE;
}


