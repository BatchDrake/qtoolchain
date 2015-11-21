/*
  util.c: QToolChain utility functions

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

#include <stdarg.h>

#include "q_defines.h"
#include "q_util.h"

static char last_error[QSPARSE_LAST_ERROR_MAX];

void
q_set_last_error (const char *fmt, ...)
{
  va_list ap;

  va_start (ap, fmt);

  vsnprintf (last_error, QSPARSE_LAST_ERROR_MAX, fmt, ap);

  va_end (ap);
}

const char *
q_get_last_error (void)
{
  return last_error;
}
