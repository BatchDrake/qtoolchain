/*
  serialize.c: Serialize and deserialize circuit objects.

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

#include <qsb.h>

#include "qcircuit.h"

uint32_t
qgate_serialize (const qgate_t *gate, void *buffer, uint32_t size)
{
  struct qsb s;
  unsigned int length, i;

  qsb_init (&s, buffer, size);

  qsb_write_uint32_t (&s, gate->order);

  qsb_write_string (&s, gate->name);

  qsb_write_string (&s, gate->description);

  /* TODO: if order > threshold, serialize sparse matrix directly */
  length = 1 << (gate->order + 1);

  for (i = 0; i < length; ++i)
    qsb_write_complex (&s, gate->coef[i]);
}

qgate_t *
qgate_deserialize (const void *buffer, uint32_t size)
{
  struct qsb s;
  qgate_t *new = NULL;
  char *name = NULL;
  char *description = NULL;
  unsigned int order, i, length;
  QCOMPLEX coef;

  qsb_init (&s, (void *) buffer, size);

  if (!qsb_read_uint32_t (&s, &order))
  {
    q_set_last_error ("Unexpected end-of-buffer while deserializing quantum gate");
    goto fail;
  }

  if (!qsb_read_string (&s, &name))
  {
    q_set_last_error ("Unexpected end-of-buffer while deserializing quantum gate");
    goto fail;
  }

  if (name == NULL)
  {
    q_set_last_error ("Memory exhausted while deserializing quantum gate");
    goto fail;
  }

  if (!qsb_read_string (&s, &description))
  {
    q_set_last_error ("Unexpected end-of-buffer while deserializing quantum gate");
    goto fail;
  }

  if (description == NULL)
  {
    q_set_last_error ("Memory exhausted while deserializing quantum gate");
    goto fail;
  }

  length = 1 << (order + 1);

  if (!qsb_ensure (&s, length * QSB_QCOMPLEX_SERIALIZED_SIZE))
  {
    q_set_last_error ("Unexpected end-of-buffer while reading quantum gate matrix coefficients");
    goto fail;
  }

  if ((new = qgate_new (order, name, description, NULL)) == NULL)
  {
    q_set_last_error ("Memory exhausted while allocating quantum gate coefficients");
    goto fail;
  }

  /* We can read all matrix coefficients without checking for errors because
   * we have ensured all them earlier
   */
  for (i = 0; i < length; ++i)
    (void) qsb_read_complex (&s, new->coef + i);

  free (name);
  free (description);

  return new;

fail:
  if (new != NULL)
    qgate_destroy (new);

  if (name != NULL)
    free (name);

  if (description != NULL)
    free (description);

  return NULL;
}
