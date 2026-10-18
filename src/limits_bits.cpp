/***************************************************************************
 *
 * limits_bits.cpp - source for the C++ Standard Library limits constants
 *
 * $Id$
 *
 ***************************************************************************
 *
 * Licensed to the Apache Software  Foundation (ASF) under one or more
 * contributor  license agreements.  See  the NOTICE  file distributed
 * with  this  work  for  additional information  regarding  copyright
 * ownership.   The ASF  licenses this  file to  you under  the Apache
 * License, Version  2.0 (the  License); you may  not use  this file
 * except in  compliance with the License.   You may obtain  a copy of
 * the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the  License is distributed on an  "AS IS" BASIS,
 * WITHOUT  WARRANTIES OR CONDITIONS  OF ANY  KIND, either  express or
 * implied.   See  the License  for  the  specific language  governing
 * permissions and limitations under the License.
 *
 * Copyright 1994-2007 Rogue Wave Software, Inc.
 * 
 **************************************************************************/

#define _RWSTD_LIB_SRC

#include <float.h>    // for INFINITY, NAN
#include <stdlib.h>   // for atof()

// must not #include <limits> to avoid colliding with symbols
// declared there with a different type than what they are
// defined with here
// #include <limits>
                       
#include <rw/_defs.h>  
                       

namespace __rw { 

#ifndef _RWSTD_NO_INFINITY

#  if !defined (_RWSTD_DBL_INF_BITS)
#    error one of _RWSTD_NO_INFINITY and _RWSTD_DBL_INF_BITS must be #defined
#  endif


union _FltBits {
    char  _C_bits [sizeof (float)];
    float _C_val;
};


union _DblBits {
    char   _C_bits [sizeof (double)];
    double _C_val;
};


union _LDblBits {
    char        _C_bits [sizeof (long double)];
    long double _C_val;
};

#  ifndef _RWSTD_NO_OBJECT_MANGLING

extern "C" {

#  else   // if defined (_RWSTD_NO_OBJECT_MANGLING)

extern "C++" {

#  endif   // _RWSTD_NO_OBJECT_MANGLING

// The constants below are declared in <limits> but with a different
// type. C linkage is (conditionally) used to defeat MSVC and other
// "clever" compilers that mangle the type of objects into their names.

// infinity computed at config time
_RWSTD_EXPORT extern const _DblBits
__rw_dbl_infinity = { _RWSTD_DBL_INF_BITS };


_RWSTD_EXPORT extern const _FltBits
__rw_flt_infinity = { _RWSTD_FLT_INF_BITS };


_RWSTD_EXPORT extern const _LDblBits
__rw_ldbl_infinity = { _RWSTD_LDBL_INF_BITS };



// quiet NaN computed at config time
_RWSTD_EXPORT extern const _DblBits
__rw_dbl_qNaN = { _RWSTD_DBL_QNAN_BITS };


_RWSTD_EXPORT extern const _FltBits
__rw_flt_qNaN = { _RWSTD_FLT_QNAN_BITS };


_RWSTD_EXPORT extern const _LDblBits
__rw_ldbl_qNaN = { _RWSTD_LDBL_QNAN_BITS };



// signaling NaN computed at config time
_RWSTD_EXPORT extern const _DblBits
__rw_dbl_sNaN = { _RWSTD_DBL_SNAN_BITS };


_RWSTD_EXPORT extern const _FltBits
__rw_flt_sNaN = { _RWSTD_FLT_SNAN_BITS };


_RWSTD_EXPORT extern const _LDblBits
__rw_ldbl_sNaN = { _RWSTD_LDBL_SNAN_BITS };



// denormalized minima computed at config time
_RWSTD_EXPORT extern const _DblBits
__rw_dbl_denorm_min = { _RWSTD_DBL_DENORM_MIN_BITS };


_RWSTD_EXPORT extern const _FltBits
__rw_flt_denorm_min = { _RWSTD_FLT_DENORM_MIN_BITS };


_RWSTD_EXPORT extern const _LDblBits
__rw_ldbl_denorm_min = { _RWSTD_LDBL_DENORM_MIN_BITS };


}   // extern "C"/"C++"


#else   // if defined (_RWSTD_NO_INFINITY)


#  ifndef _RWSTD_NO_OBJECT_MANGLING

extern "C" {

#  else   // if defined (_RWSTD_NO_OBJECT_MANGLING)

extern "C++" {

#  endif   // _RWSTD_NO_OBJECT_MANGLING


// as the last resort compute values at dynamic initialization time
_RWSTD_EXPORT extern const float __rw_flt_infinity =

#  if defined (INFINITY)

    INFINITY;

#  elif defined (FLT_INFINITY)

    FLT_INFINITY;

#  else   // !defined (INFINITY)

static_cast< float >(atof ("INF"));

#  endif   // defined (INFINITY)

_RWSTD_EXPORT extern const double __rw_dbl_infinity = __rw_flt_infinity;

_RWSTD_EXPORT extern const long double __rw_ldbl_infinity = __rw_flt_infinity;

_RWSTD_EXPORT extern const float __rw_flt_qNaN =

#  if defined (NAN)

    NAN;

#  elif defined (FLT_QNAN)

    FLT_QNAN;

#  else   // !defined (NAN)

static_cast< float >(atof ("NAN"));

#  endif   // defined (NAN)

_RWSTD_EXPORT extern const float __rw_flt_sNaN =

#  if defined (FLT_SNAN)
    FLT_SNAN;
#  else   // if !defined (FLT_SNAN)
    __rw_flt_qNaN;
#  endif   // FLT_SNAN

_RWSTD_EXPORT extern const double __rw_dbl_qNaN = __rw_flt_qNaN;
_RWSTD_EXPORT extern const double __rw_dbl_sNaN = __rw_flt_sNaN;

_RWSTD_EXPORT extern const float __rw_flt_denorm_min = 0;
_RWSTD_EXPORT extern const float __rw_dbl_denorm_min = 0;

_RWSTD_EXPORT extern const long double __rw_ldbl_qNaN = __rw_dbl_qNaN;
_RWSTD_EXPORT extern const long double __rw_ldbl_sNaN = __rw_dbl_sNaN;

_RWSTD_EXPORT extern const long double __rw_ldbl_denorm_min = 0;

}   // extern "C"/"C++"

#endif   // _RWSTD_NO_INFINITY


}   // namespace __rw
