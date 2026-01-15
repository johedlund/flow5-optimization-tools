/****************************************************************************

    fl5-lib library
    Copyright (C) 2025 Johan Hedlund

    This file is part of flow5.

    flow5 is free software: you can redistribute it and/or modify it
    under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License,
    or (at your option) any later version.

    flow5 is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty
    of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
    See the GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with flow5.
    If not, see <https://www.gnu.org/licenses/>.


*****************************************************************************/

/**
 * LAPACK complex type configuration for MSVC
 *
 * The OpenBLAS lapack.h uses C99 _Complex which is not compatible with MSVC.
 * This header must be included BEFORE any LAPACK headers to define the types.
 */

#ifndef LAPACK_CONFIG_H
#define LAPACK_CONFIG_H

#ifdef _MSC_VER
// MSVC doesn't support C99 _Complex, use std::complex instead
#include <complex>
#define lapack_complex_float std::complex<float>
#define lapack_complex_double std::complex<double>
#endif

#endif // LAPACK_CONFIG_H
