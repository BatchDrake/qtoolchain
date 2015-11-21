/*
 * main.c: entry point for qtoolchain
 * Creation date: Fri Oct 16 19:13:35 2015
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <getopt.h>
#include <sys/time.h>
#include <qtoolchain.h>

#include <qsparse.h>
#include <qcircuit.h>

#define HALFSQRT 0.70710678118

int
main (int argc, char *argv[], char *envp[])
{
  qcircuit_t *c;
  unsigned int m, i;
  unsigned int results[2] = {0, 0};

  QCOMPLEX psi[] =
      {
          (QCOMPLEX) HALFSQRT,
          (QCOMPLEX) 0,
          (QCOMPLEX) 0,
          (QCOMPLEX) HALFSQRT
      };

  c = qcircuit_new (2);

  if (!qcircuit_update (c))
  {
    fprintf (stderr, "%s: failed to update circuit: %s\n", argv[0], q_get_last_error ());
    exit (EXIT_FAILURE);
  }

  if (!qcircuit_apply_state (c, psi))
  {
    fprintf (stderr, "%s: failed to apply state: %s\n", argv[0], q_get_last_error ());
    exit (EXIT_FAILURE);
  }

  srand (time (NULL));

  for (i = 0; i < 3000; ++i)
  {
    if (!qcircuit_collapse (c, 2, &m))
    {
      fprintf (stderr, "%s: failed to collapse: %s\n", argv[0], q_get_last_error ());
      exit (EXIT_FAILURE);
    }

    printf ("M(\\psi, 10b) = a|%d0> + b|%d1>\n", m >> 1, m >> 1);

    if (!qcircuit_collapse (c, 3, &m))
    {
      fprintf (stderr, "%s: failed to collapse: %s\n", argv[0], q_get_last_error ());
      exit (EXIT_FAILURE);
    }

    printf ("M(a|%d0> + b|%d1>, 11b) = \033[1;32m|%d%d>\033[0m\n\n", m >> 1, m >> 1, m >> 1, m & 1);

    results[m & 1]++;

    qcircuit_measure_reset (c);
  }

  printf ("Summary: %d |00>s, %d |11>s\n", results[0], results[1]);

  qcircuit_destroy (c);

  return 0;
}

