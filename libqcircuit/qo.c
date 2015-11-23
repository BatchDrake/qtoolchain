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

#include <stdio.h>
#include <errno.h>

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

static uint32_t
__qcircuit_serialize_fn (const void *obj, void *buffer, uint32_t size)
{
  return qcircuit_serialize ((const qcircuit_t *) obj, buffer, size);
}

static uint32_t
__qgate_serialize_fn (const void *obj, void *buffer, uint32_t size)
{
  return qgate_serialize ((const qgate_t *) obj, buffer, size);
}

static uint32_t
__depend_serialize_fn (const void *obj, void *buffer, uint32_t size)
{
  uint32_t sersize;

  sersize = strlen ((const char *) obj) + 1;

  if (size > sersize)
    size = sersize;

  if (size > 0)
    memcpy (buffer, obj, size);

  return sersize;
}


static QBOOL
qoplan_dump_objects (FILE *fp, fastlist_t *fl, uint32_t (*serialize_fn) (const void *, void *, uint32_t))
{
  unsigned int size;
  unsigned int i;
  unsigned int objsize;
  void *buffer;

  /* Dump all dependencies */
  FASTLIST_FOR_BEGIN (struct qoplan_object *, this, fl)
    this->offset = ftell (fp);
    this->size   = (serialize_fn) (this, NULL, 0);

    if ((buffer = malloc (this->size)) == NULL)
    {
      q_set_last_error ("qoplan_dump_objects: cannot create serialization buffer");
      return Q_FALSE;
    }

    (serialize_fn) (this, buffer, this->size);

    if (fwrite (buffer, this->size, 1, fp) < 1)
    {
      q_set_last_error ("qoplan_dump_objects: write error: %s", strerror (errno));

      free (buffer);
      return Q_FALSE;
    }

    free (buffer);
  FASTLIST_FOR_END

  return Q_TRUE;
}

QBOOL
qoplan_dump_to_file (qoplan_t *plan, const char *output)
{
  FILE *fp = NULL;
  struct qo_header header;
  struct qo_descriptor *descriptors = NULL;

  unsigned int descriptor_count;
  unsigned int i;
  unsigned int size;

  descriptor_count = fastlist_size (&plan->circuits) +
                     fastlist_size (&plan->gates)    +
                     fastlist_size (&plan->depends);

  if (descriptor_count == 0)
  {
    q_set_last_error ("qoplan_dump_to_file: will not dump empty plan");
    goto fail;
  }

  if ((fp = fopen (output, "wb")) == NULL)
  {
    q_set_last_error ("qoplan_dump_to_file: cannot open %s for writing: %s",
                      output, strerror (errno));
    goto fail;
  }

  /* Populate header */
  memset (&header, 0, sizeof (struct qo_header));
  memcpy (header.qh_sig, QO_HEADER_SIGNATURE, 4);

  /* Write placeholder */
  if (fwrite (&header, sizeof (struct qo_header), 1, fp) < 1)
  {
    q_set_last_error ("qoplan_dump_to_file: write failed: %s", strerror (errno));
    goto fail;
  }

  /* Initialize descriptors */
  if ((descriptors = calloc (descriptor_count,
                             sizeof (struct qo_descriptor))) == NULL)
    goto fail;

  /* Write placeholder */
  if (fwrite (descriptors, sizeof (struct qo_descriptor), descriptor_count, fp) < descriptor_count)
  {
    q_set_last_error ("qoplan_dump_to_file: write failed: %s", strerror (errno));
    goto fail;
  }

  /* Dump objects */
  if (!qoplan_dump_objects (fp, &plan->depends,  __depend_serialize_fn))
    goto fail;

  if (!qoplan_dump_objects (fp, &plan->gates,    __qgate_serialize_fn))
    goto fail;

  if (!qoplan_dump_objects (fp, &plan->circuits, __qcircuit_serialize_fn))
    goto fail;

  /* Repopulate header */
  if (fseek (fp, 0, SEEK_SET) == -1)
  {
    q_set_last_error ("qoplan_dump_to_file: seek failed: %s", strerror (errno));
    goto fail;
  }

  header.qh_depnum     = __qsb_htof32 (fastlist_size (&plan->depends));
  header.qh_gatenum    = __qsb_htof32 (fastlist_size (&plan->gates));
  header.qh_circuitnum = __qsb_htof32 (fastlist_size (&plan->circuits));

  header.qh_depoff     = __qsb_htof32 (sizeof (struct qo_header));
  header.qh_gateoff    = __qsb_htof32 (sizeof (struct qo_header) +
                                       sizeof (struct qo_descriptor) * header.qh_depnum);

  header.qh_circuitoff = __qsb_htof32 (sizeof (struct qo_header) +
                                       sizeof (struct qo_descriptor) *
                                       (header.qh_depnum + header.qh_circuitnum));

  /* Write header */
  if (fwrite (&header, sizeof (struct qo_header), 1, fp) < 1)
  {
    q_set_last_error ("qoplan_dump_to_file: write failed: %s", strerror (errno));
    goto fail;
  }

  /* Repopulate descriptors */
  i = 0;

  FASTLIST_FOR_BEGIN (struct qoplan_object *, this, &plan->depends)
    descriptors[i].qd_offset = __qsb_htof32 (this->offset);
    descriptors[i].qd_size   = __qsb_htof32 (this->size);

    ++i;
  FASTLIST_FOR_END

  FASTLIST_FOR_BEGIN (struct qoplan_object *, this, &plan->gates)
    descriptors[i].qd_offset = __qsb_htof32 (this->offset);
    descriptors[i].qd_size   = __qsb_htof32 (this->size);

    ++i;
  FASTLIST_FOR_END

  FASTLIST_FOR_BEGIN (struct qoplan_object *, this, &plan->circuits)
    descriptors[i].qd_offset = __qsb_htof32 (this->offset);
    descriptors[i].qd_size   = __qsb_htof32 (this->size);

    ++i;
  FASTLIST_FOR_END

  /* Write descriptors */
  if (fwrite (descriptors, sizeof (struct qo_descriptor), descriptor_count, fp) < descriptor_count)
  {
    q_set_last_error ("qoplan_dump_to_file: write failed: %s", strerror (errno));
    goto fail;
  }

  free (descriptors);

  fclose (fp);

  return Q_TRUE;

fail:
  if (fp != NULL)
    fclose (fp);

  if (descriptors != NULL)
    free (descriptors);

  return Q_FALSE;
}


