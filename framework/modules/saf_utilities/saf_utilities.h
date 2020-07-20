/*
 * Copyright 2016-2018 Leo McCormack
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES WITH
 * REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY
 * AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY SPECIAL, DIRECT,
 * INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM
 * LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR
 * OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
 * PERFORMANCE OF THIS SOFTWARE.
 */

/**
 *@addtogroup Utilities
 *@{
 * @file saf_utilities.h  
 * @brief Main header for the utilities module (#SAF_UTILITIES_MODULE)
 *
 * A collection of useful utility functions; including: cross-platform complex
 * number wrappers; optimised linear algebra routines based on CBLAS and LAPACK;
 * FFT wrappers and STFT implementation; IIR/FIR filter coefficients and filter
 * bank designs; lists of common loudspeaker and microphone array coordinates;
 * multi-channel and matrix convolvers; spherical Bessel/Hankel functions
 * (including their derivatives); etc.
 *
 * ## Dependencies
 *   A performance library comprising CBLAS and LAPACK routines is required by
 *   the module and, thus, also by SAF as a whole. Add one of the following
 *   FLAGS to your project's preprocessor definitions list in order to enable
 *   one of these suitable performance libraries, which must also be correctly
 *   linked to your project.
 *   - SAF_USE_INTEL_MKL:
 *       to enable Intel's Math Kernal Library with the Fortran LAPACK interface
 *   - SAF_USE_OPENBLAS_WITH_LAPACKE:
 *       to enable OpenBLAS with the LAPACKE interface
 *   - SAF_USE_APPLE_ACCELERATE:
 *       to enable the Accelerate framework with the Fortran LAPACK interface
 *   - SAF_USE_ATLAS:
 *       to enable ATLAS BLAS routines and ATLAS's CLAPACK interface
 *
 * @see More information can be found here:
 *      https://github.com/leomccormack/Spatial_Audio_Framework
 * @note MacOSX users only: saf_utilities will employ Apple's Accelerate library
 *       by default, if none of the above FLAGS are defined.
 *
 * @author Leo McCormack
 * @date 11.07.2016
 */

#ifndef __SAF_UTILITIES_H_INCLUDED__
#define __SAF_UTILITIES_H_INCLUDED__
    
/* ========================================================================== */
/*                        Performance Library to Employ                       */
/* ========================================================================== */

#if defined(SAF_USE_INTEL_MKL)
/*
 * Using Intel's Math Kernal Library (MKL)
 * (Generally the fastest library for x86 based architectures)
 */
# define VECLIB_USE_LAPACK_FORTRAN_INTERFACE /**< Fortan interface of LAPACK */
# include "mkl.h"

#elif defined(SAF_USE_OPEN_BLAS_AND_LAPACKE)
/*
 * Using OpenBLAS and the LAPACKE interface
 * (A good option for both x86 and ARM based architectures)
 */
# define VECLIB_USE_LAPACKE_INTERFACE /**< LAPACKE interface */
# include "cblas.h"
# include "lapacke.h"

#elif defined(SAF_USE_ATLAS)
/*
 * Using the Automatically Tuned Linear Algebra Software (ATLAS) library
 * (Not recommended, since some saf_veclib functions do not work with ATLAS)
 */
# define VECLIB_USE_CLAPACK_INTERFACE /**< CLAPACK interface */
# include "cblas-atlas.h"
# include "clapack.h" /* NOTE: CLAPACK does not include all LAPACK routines! */

#elif defined(__APPLE__)
/*
 * Using Apple's Accelerate library (vDSP)
 * (Solid choice for both x86 and ARM, but only works under MacOSX and is not as
 * fast as Intel MKL for x86 systems)
 */
#ifndef SAF_USE_APPLE_ACCELERATE
# define SAF_USE_APPLE_ACCELERATE
#endif
# define VECLIB_USE_LAPACK_FORTRAN_INTERFACE /**< Fortan interface of LAPACK */
# include "Accelerate/Accelerate.h"

#else
# error "SAF requires a library (or libraries) which supports CBLAS and LAPACK"
#endif


/* ========================================================================== */
/*                        Macros and Global Constants                         */
/* ========================================================================== */

#ifndef NUM_EARS
/** 2 (true for most humans) */
# define NUM_EARS 2
#endif
#ifndef MIN
/** Returns the minimum of the two values */
# define MIN(a,b) (( (a) < (b) ) ? (a) : (b))
#endif
#ifndef MAX
/** Returns the maximum of the two values */
# define MAX(a,b) (( (a) > (b) ) ? (a) : (b))
#endif
#ifndef CLAMP
/** Ensures value "a" is clamped between the "min" and "max" values */
# define CLAMP(a,min,max) (MAX(min, MIN(max, a)))
#endif
#ifndef M_PI
/** pi constant (single precision) */
# define M_PI ( 3.14159265358979323846264338327950288f )
#endif
#ifndef M_PId
/** pi constant (double precision) */
# define M_PId ( 3.14159265358979323846264338327950288 )
#endif
#ifndef SAF_PI
/** pi constant (single precision) */
# define SAF_PI ( 3.14159265358979323846264338327950288f )
#endif
#ifndef SAF_PId
/** pi constant (double precision) */
# define SAF_PId ( 3.14159265358979323846264338327950288 )
#endif
/** Returns 0 if "x" is not a power of 2 */
#define SAF_ISPOW2(x) (((x & ~(x-1))==x) ? x : 0);
#ifndef ISEVEN
/** Returns 1 if "n" is even valued, and 0 if it is not */
# define ISEVEN(n)   ((n%2 == 0) ? 1 : 0)
#endif
#ifndef ISODD
/** Returns 1 if "n" is odd valued, and 0 if it is not */
# define ISODD(n)    ((n%2 != 0) ? 1 : 0)
#endif
/** sqrt(4pi) (single precision) */
#define SQRT4PI ( 3.544907701811032f )
/* 4pi (single precision) */
#define FOURPI ( 12.566370614359172f )
/** Converts elevation to inclincation, (in radians) */
#define ELEV2INCL(E) ( (SAF_PI/2 - E) )


/* ========================================================================== */
/*                 External Resources and SAF Utility Headers                 */
/* ========================================================================== */

/* For error message handling */
#include "saf_utility_error.h"
/* For allocating contiguous multi-dimensional arrays.
 * The original source code can be found here (MIT license):
 *   https://github.com/leomccormack/md_malloc
 */
#include "../../resources/md_malloc/md_malloc.h"
/* The default FFT implementation, for when no optimised implementation is
 * available.
 * The original source code can be found here (BSD-3-Clause license):
 *   https://github.com/mborgerding/kissfft
 */
#include "../../resources/kissFFT/kiss_fftr.h"
#include "../../resources/kissFFT/kiss_fft.h"
/* For Computing 3-D convex hulls.
 * The original source code can be found here (MIT license):
 *   https://github.com/leomccormack/convhull_3d */
#include "../../resources/convhull_3d/convhull_3d.h"
/* For cross-platform complex number support */
#include "saf_utility_complex.h"
/* For sorting vectors */
#include "saf_utility_sort.h"
/* Filter coefficients and filterbanks (IIR/FIR) */
#include "saf_utility_filters.h"
/* Many handy linear algebra functions based on CBLAS/LAPACK, and some based on
 * optimised proprietary Intel MKL and Apple Accelerate routines */
#include "saf_utility_veclib.h"
/* For computing spherical/cylindrical Bessel and Hankel functions and their
 * derivatives */
#include "saf_utility_bessel.h"
/* Wrappers for different FFT implementations */
#include "saf_utility_fft.h"
/* Matrix and multi-channel convolvers */
#include "saf_utility_matrixConv.h"
/* Pitch shifting algorithms */
#include "saf_utility_pitch.h"
/* A collection of signal decorrelators */
#include "saf_utility_decor.h"
/* For misc. functions */
#include "saf_utility_misc.h"
/* For computational geometry functions */
#include "saf_utility_geometry.h"
/* For an implementation of the hybrid complex quadrature mirror filterbank */
#include "saf_utility_qmf.h"
/* Various presets for loudspeaker arrays and uniform distributions of points on
 * spheres. */
#include "saf_utility_loudspeaker_presets.h"
/* Various presets for microphone and hydrophone arrays. */
#include "saf_utility_sensorarray_presets.h"
/* A modified version of the alias-free STFT implementation by Juha Vilkamo.
 * The original source code can be found here (MIT license):
 *   https://github.com/jvilkamo/afSTFT
 * The design is also detailed in chapter 1 of [1]
 *
 * @see [1] Pulkki, V., Delikaris-Manias, S. and Politis, A. 2018. Parametric
 *          time--frequency domain spatial audio. John Wiley & Sons,
 *          Incorporated.
 */
#include "../../resources/afSTFT/afSTFTlib.h"


#include "../../resources/libsamplerate/samplerate.h"


#endif /* __SAF_UTILITIES_H_INCLUDED__ */

/**@} */ /* doxygen addtogroup Utilities */
