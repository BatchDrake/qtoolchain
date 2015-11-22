/*
  qo.c: The Quantum Object file format

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

#include "qo.h"
#include "qoplan.h"

#define QOPLAN_OBJECT_TYPE_DEPEND  0
#define QOPLAN_OBJECT_TYPE_GATE    1
#define QOPLAN_OBJECT_TYPE_CIRCUIT 2

static void *
__qoplan_object_destroy_cb (void *data, void *priv)
{
  unsigned long type;
  struct qoplan_object *obj;

  obj = (struct qoplan_object *) data;
  type = (unsigned long) priv;

  if (type == QOPLAN_OBJECT_TYPE_DEPEND)
    free (obj->depend);

  free (obj);

  return NULL;
}

qoplan_t *
qoplan_new (void)
{
  qoplan_t *new;

  new = calloc (1, sizeof (qoplan_t));

  return new;
}

void
qoplan_destroy (qoplan_t *plan)
{
  (void) fastlist_walk (&plan->depends,  __qoplan_object_destroy_cb, (void *) QOPLAN_OBJECT_TYPE_DEPEND);
  (void) fastlist_walk (&plan->gates,    __qoplan_object_destroy_cb, (void *) QOPLAN_OBJECT_TYPE_GATE);
  (void) fastlist_walk (&plan->circuits, __qoplan_object_destroy_cb, (void *) QOPLAN_OBJECT_TYPE_CIRCUIT);

  fastlist_free (&plan->depends);
  fastlist_free (&plan->gates);
  fastlist_free (&plan->circuits);

  free (plan);
}

static struct qoplan_object *
qoplan_object_new (void *object)
{
  struct qoplan_object *qobj;

  if ((qobj = malloc (sizeof (struct qoplan_object))) == NULL)
    return NULL;

  qobj->priv   = object;
  qobj->offset = 0;
  qobj->size   = 0;

  return qobj;
}

static void *
__qoplan_object_lookup_cb (void *data, void *priv)
{
  struct qoplan_object *obj;

  obj = (struct qoplan_object *) data;

  if (obj->priv == priv)
    return (void *) 1;

  return NULL;
}

static void *
__qoplan_depend_lookup_cb (void *data, void *priv)
{
  struct qoplan_object *obj;

  obj = (struct qoplan_object *) data;

  if (strcmp (obj->depend, (const char *) priv) == 0)
    return (void *) 1;

  return NULL;
}

static QBOOL
qoplan_have_gate (qoplan_t *plan, const qgate_t *gate)
{
  return fastlist_walk (&plan->gates, __qoplan_object_lookup_cb, (void *) gate) != NULL;
}

static QBOOL
qoplan_have_circuit (qoplan_t *plan, const qcircuit_t *circuit)
{
  return fastlist_walk (&plan->circuits, __qoplan_object_lookup_cb, (void *) circuit) != NULL;
}

static QBOOL
qoplan_have_depend (qoplan_t *plan, const char *depend)
{
  return fastlist_walk (&plan->depends, __qoplan_depend_lookup_cb, (void *) depend) != NULL;
}

QBOOL
qoplan_add_gate (qoplan_t *plan, const qgate_t *gate)
{
  struct qoplan_object *obj;

  if (qoplan_have_gate (plan, gate))
    return Q_TRUE;

  if ((obj = qoplan_object_new ((void *) gate)) == NULL ||
      !fastlist_append (&plan->gates, obj))
  {
    if (obj != NULL)
      __qoplan_object_destroy_cb (obj, (void *) QOPLAN_OBJECT_TYPE_GATE);

    q_set_last_error ("Memory exhausted while adding gate");
    return Q_FALSE;
  }

  return Q_TRUE;
}

QBOOL
qoplan_add_circuit (qoplan_t *plan, const qcircuit_t *circuit)
{
  struct qoplan_object *obj;

  if (qoplan_have_circuit (plan, circuit))
    return Q_TRUE;

  if ((obj = qoplan_object_new ((void *) circuit)) == NULL ||
      !fastlist_append (&plan->circuits, obj))
  {
    if (obj != NULL)
      __qoplan_object_destroy_cb (obj, (void *) QOPLAN_OBJECT_TYPE_CIRCUIT);

    q_set_last_error ("Memory exhausted while adding circuit");
    return Q_FALSE;
  }

  return Q_TRUE;
}

QBOOL
qoplan_add_depend (qoplan_t *plan, const char *depend)
{
  struct qoplan_object *obj;
  char *depdup;

  if (qoplan_have_depend (plan, depend))
    return Q_TRUE;

  if ((depdup = strdup (depend)) ||
      (obj = qoplan_object_new (depdup)) == NULL
      || !fastlist_append (&plan->circuits, obj))
  {
    if (obj != NULL)
      __qoplan_object_destroy_cb (obj, (void *) QOPLAN_OBJECT_TYPE_DEPEND);
    else if (depdup != NULL)
      free (depdup);

    q_set_last_error ("Memory exhausted while adding depend");
    return Q_FALSE;
  }

  return Q_TRUE;
}

QBOOL
qoplan_dump_to_file (qoplan_t *plan, const char *output)
{
  /* TODO: implement me */
}


