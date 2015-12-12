/*
  q_defines.h: Useful macros and datatypes for QToolChain

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

#ifndef _LIBQ_Q_DEFINES_H
#define _LIBQ_Q_DEFINES_H

#include <stdint.h>
#include <complex.h>

#define Q_TRUE  1
#define Q_FALSE 0

#define QBOOL    int
#define QREAL    double
#define QREALFMT "%lf"
#define QCOMPLEX double complex
#define QNZCOUNT uint8_t

#define QSPARSE_LAST_ERROR_MAX 256

#endif /* _LIBQ_Q_DEFINES_H */
