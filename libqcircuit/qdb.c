/*
  qdb.c: Quantum object database

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

#include "qcircuit.h"

static void *
__qgate_destroy_cb (void *gate, void *priv)
{
  qgate_destroy ((qgate_t *) gate);

  return NULL;
}

static void *
__qcircuit_destroy_cb (void *circuit, void *priv)
{
  qcircuit_destroy ((qcircuit_t *) circuit);

  return NULL;
}

void
qdb_destroy (qdb_t *db, QBOOL deep)
{
  unsigned int i;
  unsigned int size;

  (void) fastlist_walk (&db->qgates, __qgate_destroy_cb, NULL);
  (void) fastlist_walk (&db->qcircuits, __qcircuit_destroy_cb, NULL);

  fastlist_free (&db->qgates);
  fastlist_free (&db->qcircuits);

  free (db);
}

qdb_t *
qdb_new (void)
{
  qdb_t *new;

  new = calloc (1, sizeof (qdb_t));

  return new;
}

qgate_t *
qdb_lookup_qgate (const qdb_t *db, const char *name)
{
  unsigned int i;
  unsigned int size;
  qgate_t *gate;

  size = fastlist_size (&db->qgates);

  for (i = 0; i < size; ++i)
    if ((gate = fastlist_get (&db->qgates, i)) != NULL)
      if (strcmp (gate->name, name) == 0)
        return gate;

  return NULL;
}

qcircuit_t *
qdb_lookup_qcircuit (const qdb_t *db, const char *name)
{
  unsigned int i;
  unsigned int size;
  qcircuit_t *circuit;

  size = fastlist_size (&db->qcircuits);

  for (i = 0; i < size; ++i)
    if ((circuit = fastlist_get (&db->qcircuits, i)) != NULL)
      if (strcmp (circuit->name, name) == 0)
        return circuit;

  return NULL;
}

QBOOL
qdb_register_qgate (qdb_t *db, qgate_t *gate)
{
  if (fastlist_append (&db->qgates, gate) == FASTLIST_INVALID_REF)
    return Q_FALSE;

  return Q_TRUE;
}

QBOOL
qdb_register_qcircuit (qdb_t *db, qcircuit_t *qcircuit)
{
  if (fastlist_append (&db->qcircuits, qcircuit) == FASTLIST_INVALID_REF)
    return Q_FALSE;

  return Q_TRUE;
}

