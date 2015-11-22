/*
  qsb.h: QToolChain's serializable buffers

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

#ifndef _LIBQ_QSB_H
#define _LIBQ_QSB_H

#include <complex.h>
#include <string.h>

#include <q_defines.h>
#include <q_util.h>

#define QSB_FLOAT_SERIALIZED_SIZE    sizeof (uint32_t)
#define QSB_DOUBLE_SERIALIZED_SIZE   sizeof (uint64_t)
#define QSB_QCOMPLEX_SERIALIZED_SIZE (QSB_DOUBLE_SERIALIZED_SIZE * 2)

struct qsb
{
  union
  {
    void *buffer;
    uint8_t *bytes;
  };

  uint32_t size;
  uint32_t ptr;
};

static inline uint32_t
__qsb_htof32 (uint32_t buf)
{
  uint8_t *bytes = (uint8_t *) &buf;

#ifdef LITTLE_ENDIAN
  return ((uint32_t) bytes[0] << 24) |
          ((uint32_t) bytes[1] << 16) |
          ((uint32_t) bytes[2] << 8)  |
           (uint32_t) bytes[3];
#else
  return buf;
#endif
}

static inline uint64_t
__qsb_htof64 (uint64_t buf)
{
  uint8_t *bytes = (uint8_t *) &buf;

#ifdef LITTLE_ENDIAN
  return ((uint64_t) bytes[0] << 56) |
          ((uint64_t) bytes[1] << 48) |
          ((uint64_t) bytes[2] << 40) |
          ((uint64_t) bytes[3] << 32) |
          ((uint64_t) bytes[4] << 24) |
          ((uint64_t) bytes[5] << 16) |
          ((uint64_t) bytes[6] <<  8) |
           (uint64_t) bytes[7];
#else
  return buf;
#endif
}

static inline uint32_t
__qsb_ftoh32 (uint32_t buf)
{
  return __qsb_htof32 (buf);
}

static inline uint64_t
__qsb_ftoh64 (uint64_t buf)
{
  return __qsb_htof64 (buf);
}

static inline uint32_t
__qsb_htof_ieee754_32 (float buf)
{
  uint8_t *bytes = (uint8_t *) &buf;

#ifdef LITTLE_ENDIAN
  return ((uint32_t) bytes[0] << 24) |
          ((uint32_t) bytes[1] << 16) |
          ((uint32_t) bytes[2] << 8)  |
           (uint32_t) bytes[3];
#else
  return buf;
#endif
}

static inline uint64_t
__qsb_htof_ieee754_64 (double buf)
{
  uint8_t *bytes = (uint8_t *) &buf;

#ifdef LITTLE_ENDIAN
  return ((uint64_t) bytes[0] << 56) |
          ((uint64_t) bytes[1] << 48) |
          ((uint64_t) bytes[2] << 40) |
          ((uint64_t) bytes[3] << 32) |
          ((uint64_t) bytes[4] << 24) |
          ((uint64_t) bytes[5] << 16) |
          ((uint64_t) bytes[6] <<  8) |
           (uint64_t) bytes[7];
#else
  return buf;
#endif
}

static inline float
__qsb_ftoh_ieee754_32 (uint32_t buf)
{
  uint32_t dest;

  dest = __qsb_ftoh32 (buf);

  return *(float *) &dest;
}

static inline double
__qsb_ftoh_ieee754_64 (uint64_t buf)
{
  uint64_t dest;

  dest = __qsb_ftoh64 (buf);

  return *(double *) &dest;
}

static inline void
__qsb_write_uint32_t (struct qsb *state, uint32_t val)
{
  val = __qsb_htof32 (val);
  memcpy (&state->bytes[state->ptr], &val, sizeof (uint32_t));
}

static inline void
__qsb_write_uint64_t (struct qsb *state, uint64_t val)
{
  val = __qsb_htof64 (val);
  memcpy (&state->bytes[state->ptr], &val, sizeof (uint64_t));
}

static inline void
__qsb_write_float (struct qsb *state, float fval)
{
  uint32_t val;
  val = __qsb_htof_ieee754_32 (fval);
  memcpy (&state->bytes[state->ptr], &val, sizeof (uint32_t));
}

static inline void
__qsb_write_double (struct qsb *state, double fval)
{
  uint64_t val;
  val = __qsb_htof_ieee754_64 (fval);
  memcpy (&state->bytes[state->ptr], &val, sizeof (uint64_t));
}

static inline void
__qsb_read_uint32_t (struct qsb *state, uint32_t *val)
{
  *val = __qsb_htof32 (*(uint32_t *) &state->bytes[state->ptr]);
}

static inline void
__qsb_read_uint64_t (struct qsb *state, uint64_t *val)
{
  *val = __qsb_htof64 (*(uint64_t *) &state->bytes[state->ptr]);
}

static inline void
__qsb_read_float (struct qsb *state, float *fval)
{
  uint32_t *val;

  val = (uint32_t *) &state->bytes[state->ptr];
  *fval = __qsb_ftoh_ieee754_32 (*val);
}

static inline void
__qsb_read_double (struct qsb *state, double *fval)
{
  uint64_t *val;

  val = (uint64_t *) &state->bytes[state->ptr];
  *fval = __qsb_ftoh_ieee754_64 (*val);
}


void qsb_init (struct qsb *, void *, uint32_t);
void qsb_seek (struct qsb *, uint32_t);
void qsb_advance (struct qsb *state, uint32_t incr);
uint32_t qsb_tell (const struct qsb *);
uint32_t qsb_remainder (const struct qsb *);

QBOOL qsb_ensure (const struct qsb *, uint32_t);

void qsb_write_uint32_t (struct qsb *, uint32_t);
void qsb_write_uint64_t (struct qsb *, uint64_t);
void qsb_write_float (struct qsb *, float);
void qsb_write_double (struct qsb *, double);
void qsb_write_complex (struct qsb *, QCOMPLEX);
void qsb_write_string (struct qsb *, const char *);

QBOOL qsb_read_uint32_t (struct qsb *, uint32_t *);
QBOOL qsb_read_uint64_t (struct qsb *, uint64_t *);
QBOOL qsb_read_float (struct qsb *, float *);
QBOOL qsb_read_double (struct qsb *, double *);
QBOOL qsb_read_complex (struct qsb *, QCOMPLEX *);
QBOOL qsb_read_string (struct qsb *, char **);

#endif /* _LIBQ_QSB_H */
