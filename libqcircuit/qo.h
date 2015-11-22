/*
  qo.h: The Quantum Object file format

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

#ifndef _LIBQCIRCUIT_QO_H
#define _LIBQCIRCUIT_QO_H

#include "qsb.h"

#define QO_HEADER_SIGNATURE "QOFF"

struct qo_header
{
  uint8_t qh_sig[4];

  /* Array of pointers to strings + string sizes */
  uint32_t qh_depnum;
  uint32_t qh_depoff;

  /* Array of pointers to gates + gate sizes */
  uint32_t qh_gatenum;
  uint32_t qh_gateoff;

  /* Array of pointers to circuits + circuit sizes */
  uint32_t qh_circuitnum;
  uint32_t qh_circuitoff;

  /* TODO: Add cached matrices */
};

struct qo_descriptor
{
  uint32_t qd_size;
  uint32_t qd_offset;
};

#endif /* _LIBQCIRCUIT_QO_H */
