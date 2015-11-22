/*
  qdb.h: Quantum object database

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

#ifndef _LIBQCIRCUIT_QDB_H
#define _LIBQCIRCUIT_QDB_H

#include "fastlist.h"

struct qdb
{
  fastlist_t qgates;
  fastlist_t qcircuits;
};

typedef struct qdb qdb_t;

qdb_t *qdb_new (void);
qgate_t *qdb_lookup_qgate (const qdb_t *, const char *);
qcircuit_t *qdb_lookup_qcircuit (const qdb_t *, const char *);
QBOOL qdb_register_qgate (qdb_t *, qgate_t *);
QBOOL qdb_register_qcircuit (qdb_t *, qcircuit_t *);
void qdb_destroy (qdb_t *, QBOOL);

#endif /* _LIBQCIRCUIT_QDB_H */
