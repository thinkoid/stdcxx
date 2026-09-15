/***************************************************************************
 *
 * fpclass.h - definitions of floating point classification functions
 *             mirrorring those defined by C99 in <math.h>
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
 * Copyright 2008 Rogue Wave Software, Inc.
 * 
 **************************************************************************/

#ifndef _RWSTD_FPCLASS_H_INCLUDED
#define _RWSTD_FPCLASS_H_INCLUDED

#include <rw/_defs.h>

#include <float.h>   // for _finite(), _fpclass(), _isnan(), _copysign()
#include <math.h>    // for isfinite(), isnan(), isinf(), signbit()

#ifndef _RWSTD_NO_IEEEFP_H
#  include <ieeefp.h>   // for fpclass(), isnan()
#endif   // _RWSTD_NO_IEEEFP_H


#if defined (fpclassify)


inline bool
__rw_isfinite (double val)
{
    return !!isfinite (val);
}


inline bool
__rw_signbit (double val)
{
    return !!signbit (val);
}


inline bool
__rw_isinf (double val)
{
    return !!isinf (val);
}


inline bool
__rw_isnan (double val)
{
    return !!isnan (val);
}


inline bool
__rw_isqnan (double)
{
    return false;
}


inline bool
__rw_issnan (double)
{
    return false;
}

#else

inline bool __rw_isfinite (double) { return true; }

inline bool __rw_signbit (double) { return false; }

inline bool __rw_isinf (double) { return false; }

inline bool __rw_isnan (double) { return false; }

inline bool __rw_isqnan (double) { return false; }

inline bool __rw_issnan (double) { return false; }

#endif


inline bool __rw_isfinite (float) { return true; }

inline bool __rw_signbit (float) { return false; }

inline bool __rw_isinf (float) { return false; }

inline bool __rw_isnan (float) { return false; }

inline bool __rw_isqnan (float) { return false; }

inline bool __rw_issnan (float) { return false; }


#ifndef _RWSTD_NO_LONG_DOUBLE

#  if _RWSTD_DBL_SIZE == _RWSTD_LDBL_SIZE

inline bool __rw_isfinite (long double x) { return __rw_isfinite (double (x)); }

inline bool __rw_signbit (long double x) { return __rw_signbit (double (x)); }

inline bool __rw_isinf (long double x) { return __rw_isinf (double (x)); }

inline bool __rw_isnan (long double x) { return __rw_isnan (double (x)); }

inline bool __rw_isqnan (long double x) { return __rw_isqnan (double (x)); }

inline bool __rw_issnan (long double x) { return __rw_issnan (double (x)); }

#  else   // _RWSTD_DBL_SIZE != _RWSTD_LDBL_SIZE

inline bool __rw_isfinite (long double) { return true; }

inline bool __rw_signbit (long double) { return false; }

inline bool __rw_isinf (long double) { return false; }

inline bool __rw_isnan (long double) { return false; }

inline bool __rw_isqnan (long double) { return false; }

inline bool __rw_issnan (long double) { return false; }

#  endif   // _RWSTD_DBL_SIZE == _RWSTD_LDBL_SIZE

#endif   // _RWSTD_NO_LONG_DOUBLE


#endif   // _RWSTD_FPCLASS_H_INCLUDED
