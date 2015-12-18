/*
  main.c: The Quantum Assembler entry point

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
#include "qas.h"

#include <qo.h>
#include <qoplan.h>

#define SQRT_2 7.07106781186547573e-01
#define ALPHA 0.6
#define BETA  -0.8

QBOOL
qdb_dump_to_qo (const qdb_t *qdb, const char *path)
{
  qoplan_t *plan = NULL;

  if ((plan = qoplan_new ()) == NULL)
    goto fail;

  FASTLIST_FOR_BEGIN (const qcircuit_t *, c, &qdb->qcircuits)
    if (!qoplan_add_circuit (plan, c))
      goto fail;
  FASTLIST_FOR_END

  FASTLIST_FOR_BEGIN (const qgate_t *, g, &qdb->qgates)
      if (!qoplan_add_gate (plan, g))
        goto fail;
  FASTLIST_FOR_END

  if (!qoplan_dump_to_file (plan, path))
    goto fail;

  qoplan_destroy (plan);

  return Q_TRUE;

fail:
  if (plan != NULL)
    qoplan_destroy (plan);

  return Q_FALSE;
}

int
main (int argc, char *argv[])
{
  qas_ctx_t *ctx;
  qdb_t *db;
  unsigned int i;
  QCOMPLEX state[8] = {0};
  qcircuit_t *send, *recv;
  unsigned int measure;

  if (argc != 3)
  {
    fprintf (stderr, "Usage:\n\t%s <file.qas> <output.qo>\n", argv[0]);
    exit (EXIT_FAILURE);
  }

  if (!qas_init ())
  {
    fprintf (stderr, "%s: couldn't initialize assembler: %s\n",
             argv[0],
             q_get_last_error ());

    exit (EXIT_FAILURE);
  }

  if ((ctx = qas_open_from_file (argv[1])) == NULL)
  {
    fprintf (stderr, "%s: cannot open `%s'\n",
             argv[0],
             argv[1]);
    fprintf (stderr, "%s\n",
             q_get_last_error ());
    exit (EXIT_FAILURE);
  }

  if (!qas_parse (ctx))
  {
    fprintf (stderr, "error: %s:%d: %s\n",
                     qas_get_path (ctx),
                     qas_get_line (ctx),
                     qas_get_error (ctx));

    qas_close (ctx);

    exit (EXIT_FAILURE);
  }

  db = qas_pop_db (ctx);

  qas_close (ctx);

  if (!qdb_dump_to_qo (db, argv[2]))
  {
    fprintf (stderr, "%s: cannot dump quantum object file: %s\n", argv[0], q_get_last_error ());

    qdb_destroy (db, Q_TRUE);

    exit (EXIT_FAILURE);
  }

  qdb_destroy (db, Q_TRUE);

  return 0;
}

#if 0
  unsigned int bins[4] = {0};

  if ((send = qdb_lookup_qcircuit (db, "teleporter_send")) != NULL &&
      (recv = qdb_lookup_qcircuit (db, "teleporter_recv")) != NULL)
  {
    for (i = 0; i < 1000; ++i)
    {
      memset (state, 0, sizeof (state));

      state[0] = ALPHA * SQRT_2; /* |000> */
      state[6] = ALPHA * SQRT_2; /* |110> */

      state[1] = BETA  * SQRT_2; /* |001> */
      state[7] = BETA  * SQRT_2; /* |111> */

      printf ("Alice puts state \033[1;32m%lg|0> + %lg|1>\033[0m in qubit 0\n", ALPHA, BETA);

      if (!qcircuit_apply_state (send, state))
      {
        printf ("Failed to apply state: %s\n", q_get_last_error ());

        goto done;
      }

      if (!qcircuit_collapse (send, 0x3, &measure))
      {
        printf ("Measure failed: %s\n", q_get_last_error ());

        goto done;
      }

/*      qcircuit_debug_state (send);*/

      measure = qcircuit_get_measure_bits (send);

      if (!qcircuit_get_state (send, state))
      {
        printf ("Failed to get state: %s\n", q_get_last_error ());

        goto done;
      }

      if (!qcircuit_apply_state (recv, state))
      {
        printf ("Failed to apply state: %s\n", q_get_last_error ());

        goto done;
      }

      if (!qcircuit_get_state (recv, state))
      {
        printf ("Failed to get state: %s\n", q_get_last_error ());

        goto done;
      }

/*      qcircuit_debug_state (recv);*/

      printf ("Bob   gets state \033[1;31m%lg|0> + %lg|1>\033[0m in qubit 2 (measure: 0x%x)\n\n", creal (state[measure]), creal (state[measure + 4]), measure);

      bins[measure]++;
    }

    for (i = 0; i < 4; ++i)
      printf ("Number of collapses with measure 0x%x: %d\n", i, bins[i]);
  }
#endif

