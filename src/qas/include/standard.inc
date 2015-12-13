#
#  standard.inc: Standard set of quantum gates of 1, 2 and 3 qubits
#
#  Copyright (C) 2015 Gonzalo José Carracedo Carballal
#
#  This program is free software: you can redistribute it and/or modify
#  it under the terms of the GNU Lesser General Public License as
#  published by the Free Software Foundation, either version 3 of the
#  License, or (at your option) any later version.
#
#  This program is distributed in the hope that it will be useful, but
#  WITHOUT ANY WARRANTY; without even the implied warranty of
#  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#  GNU Lesser General Public License for more details.
#
#  You should have received a copy of the GNU Lesser General Public
#  License along with this program.  If not, see
#  <http://www.gnu.org/licenses/>
#

.gate H, 1, "Hadamard gate"
  .coef 0.707106781186547573, 0.707106781186547573
  .coef 0.707106781186547573, -0.707106781186547573
.end

.gate not, 1, "Classical NOT gate"
  .coef 0, 1
  .coef 1, 0
.end

.gate X, 1, "Pauli-X gate"
  .coef 0, 1
  .coef 1, 0
.end

.gate Y, 1, "Pauli-Y gate"
  .coef 0, -1[1.57079632679489656]
  .coef 1[1.57079632679489656], 0
.end

.gate Z, 1, "Pauli-Z gate"
  .coef 1, 0
  .coef 0, -1
.end

.gate T, 1, "Pi/8 or T gate"
  .coef 1, 0
  .coef 0, 1[0.785398163397448279]
.end

.gate swap, 2, "Swap gate"
  .coef 1, 0, 0, 0
  .coef 0, 0, 1, 0
  .coef 0, 1, 0, 0
  .coef 0, 0, 0, 1
.end

.gate sqrt_swap, 2, "Square root of swap gate"
  .coef 1, 0, 0, 0
  .coef 0, 0.707106781186547573[0.785398163397448279], 0.707106781186547573[-0.785398163397448279], 0
  .coef 0, 0.707106781186547573[-0.785398163397448279], 0.707106781186547573[0.785398163397448279], 0
  .coef 0, 0, 0, 1
.end

.gate cnot, 2, "Controlled NOT gate"
  .coef 1, 0, 0, 0
  .coef 0, 1, 0, 0
  .coef 0, 0, 0, 1
  .coef 0, 0, 1, 0
.end

.gate ccnot, 3, "Double controlled NOT / Toffoli gate"
  .coef 1, 0, 0, 0, 0, 0, 0, 0
  .coef 0, 1, 0, 0, 0, 0, 0, 0
  .coef 0, 0, 1, 0, 0, 0, 0, 0
  .coef 0, 0, 0, 1, 0, 0, 0, 0
  .coef 0, 0, 0, 0, 1, 0, 0, 0
  .coef 0, 0, 0, 0, 0, 1, 0, 0
  .coef 0, 0, 0, 0, 0, 0, 0, 1
  .coef 0, 0, 0, 0, 0, 0, 1, 0
.end

.gate cswap, 3, "Controlled swap / Fredkin gate"
  .coef 1, 0, 0, 0, 0, 0, 0, 0
  .coef 0, 1, 0, 0, 0, 0, 0, 0
  .coef 0, 0, 1, 0, 0, 0, 0, 0
  .coef 0, 0, 0, 1, 0, 0, 0, 0
  .coef 0, 0, 0, 0, 1, 0, 0, 0
  .coef 0, 0, 0, 0, 0, 0, 1, 0
  .coef 0, 0, 0, 0, 0, 1, 0, 0
  .coef 0, 0, 0, 0, 0, 0, 0, 1
.end


