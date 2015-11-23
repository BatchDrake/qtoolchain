/*
  fastlist.h: Exponentially growing pointer lists

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

#ifndef _LIBQCIRCUIT_FASTLIST_H
#define _LIBQCIRCUIT_FASTLIST_H

#define FASTLIST_INITIALIZER {0, 0, 0, 0, NULL}
#define FASTLIST_INVALID_REF ((fastlist_ref_t) -1)


#define FASTLIST_FOR_BEGIN(type, obj, fl)        \
{                                                 \
  type obj;                                       \
  unsigned int __i, __size;                      \
  __size = fastlist_size (fl);                    \
  for (__i = 0; __i < __size; ++__i)              \
    if ((obj = (type) (fl)->list[__i]) != NULL)   \
    {

#define FASTLIST_FOR_END }}


struct fastlist
{
  unsigned int   size;
  unsigned int   allocation;
  unsigned int   used; /* Always < size */
  unsigned int   last_freed;

  void          **list;
};

typedef struct fastlist fastlist_t;
typedef unsigned int fastlist_ref_t;

QBOOL fastlist_set (fastlist_t *, fastlist_ref_t, void *);
fastlist_ref_t fastlist_append (fastlist_t *, void *);
void *fastlist_get (const fastlist_t *, fastlist_ref_t);
unsigned int fastlist_size (const fastlist_t *);
unsigned int fastlist_used (const fastlist_t *);
unsigned int fastlist_allocation (const fastlist_t *);
void *fastlist_walk (const fastlist_t *, void * (*) (void *, void *), void *);
void fastlist_free (fastlist_t *);

#endif /* _LIBQCIRCUIT_FASTLIST_H */
