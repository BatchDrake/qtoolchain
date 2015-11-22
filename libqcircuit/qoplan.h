/*
  qo.h: The Quantum Object file format, file planification

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

#ifndef _LIBQCIRCUIT_QOPLAN_H
#define _LIBQCIRCUIT_QOPLAN_H

#include "qcircuit.h"

struct qoplan_object
{
  union
  {
    const qgate_t *gate;
    const qcircuit_t *circuit;
    char *depend;
    void *priv;
  };

  uint32_t offset;
  uint32_t size;
};

struct qoplan
{
  fastlist_t depends;
  fastlist_t gates;
  fastlist_t circuits;
};

typedef struct qoplan qoplan_t;

qoplan_t *qoplan_new (void);
void qoplan_destroy (qoplan_t *);

QBOOL qoplan_add_gate (qoplan_t *, const qgate_t *);
QBOOL qoplan_add_circuit (qoplan_t *, const qcircuit_t *);
QBOOL qoplan_add_depend (qoplan_t *, const char *);

QBOOL qoplan_dump_to_file (qoplan_t *, const char *);

#endif /* _LIBQCIRCUI_QOPLAN_H */
