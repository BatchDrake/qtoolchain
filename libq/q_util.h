/*
  q_util.h: QToolChain utility functions

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

#ifndef LIBQ_Q_UTIL_H_
#define LIBQ_Q_UTIL_H_

void q_set_last_error (const char *, ...);
const char *q_get_last_error (void);

#endif /* LIBQ_Q_UTIL_H_ */
