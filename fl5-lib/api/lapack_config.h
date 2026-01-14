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
