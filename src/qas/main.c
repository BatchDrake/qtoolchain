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

int
main (int argc, char *argv[])
{
  qas_ctx_t *ctx;
  qdb_t *db;
  unsigned int i;

  if (argc != 2)
  {
    fprintf (stderr, "Usage:\n\t%s <file.qas>\n", argv[0]);
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

  fprintf (stderr, "%s: parse OK\n", argv[0]);

  db = qas_pop_db (ctx);

  FASTLIST_FOR_BEGIN (qgate_t *, gate, &db->qgates)
    printf ("Gate %s (%s)\n", gate->name, gate->description);

    qgate_debug(gate);

    printf ("\n");

  FASTLIST_FOR_END

  qas_close (ctx);

  qdb_destroy (db, Q_TRUE);

  return 0;
}

