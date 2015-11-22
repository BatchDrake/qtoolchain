/*
  fastlist.c: Exponentially growing pointer lists

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
#include <string.h>

#include <q_defines.h>

#include "fastlist.h"

static inline QBOOL
fastlist_grow (fastlist_t *fl)
{
  unsigned int allocation = fl->allocation;
  void **new_list;

  if (allocation == 0)
    allocation = 1;
  else
    allocation <<= 1;

  if ((new_list = realloc (fl->list, allocation * sizeof (void *))) == NULL)
    return Q_FALSE;

  if (allocation == 1)
    new_list[0];
  else
    memset (&new_list[fl->allocation], 0, fl->allocation * sizeof (void *));

  fl->allocation = allocation;
  fl->list       = new_list;

  return Q_TRUE;
}

static inline fastlist_ref_t
fastlist_find_free (const fastlist_t *fl)
{
  unsigned int i;

  for (i = fl->last_freed; i < fl->size; ++i)
    if (fl->list[i] == NULL)
      return i;

  for (i = 0; i < fl->last_freed; ++i)
    if (fl->list[i] == NULL)
          return i;

  if (fl->size < fl->allocation)
    return fl->size;

  return FASTLIST_INVALID_REF;
}

QBOOL
fastlist_set (fastlist_t *fl, fastlist_ref_t ref, void *buf)
{
  if (ref >= fl->size)
    return Q_FALSE;

  if (fl->list[ref] != NULL && buf == NULL)
    --fl->used;
  else if (fl->list[ref] == NULL && buf != NULL)
    ++fl->used;

  fl->list[ref] = buf;

  if (buf == NULL)
  {
    if (ref == fl->size - 1)
      fl->size = ref;
    else
      fl->last_freed = ref;
  }

  return Q_TRUE;
}

void *
fastlist_get (const fastlist_t *fl, fastlist_ref_t ref)
{
  if (ref >= fl->size)
    return NULL;

  return fl->list[ref];
}

fastlist_ref_t
fastlist_append (fastlist_t *fl, void *ptr)
{
  fastlist_ref_t ref;

  if (ptr == NULL)
    return fl->size;

  if ((ref = fastlist_find_free (fl)) == FASTLIST_INVALID_REF)
  {
    if (!fastlist_grow (fl))
      return FASTLIST_INVALID_REF;

    ref = fl->size;
  }

  (void) fastlist_set (fl, ref, ptr);

  return Q_TRUE;
}

unsigned int
fastlist_size (const fastlist_t *fl)
{
  return fl->size;
}

unsigned int
fastlist_used (const fastlist_t *fl)
{
  return fl->used;
}

unsigned int
fastlist_allocation (const fastlist_t *fl)
{
  return fl->allocation;
}

void *
fastlist_walk (const fastlist_t *fl, void *(*cb) (void *, void *), void *private)
{
  fastlist_ref_t i = 0;
  void *result;

  for (i = 0; i < fl->size; ++i)
    if (fl->list[i] != NULL)
      if ((result = (cb) (fl->list[i], private)) != NULL)
        return result;

  return result;
}

void
fastlist_free (fastlist_t *fl)
{
  if (fl->list != NULL)
    free (fl->list);
}
