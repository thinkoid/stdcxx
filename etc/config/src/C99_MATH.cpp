// checking for the C99 float and long double functions in <math.h>

/***************************************************************************
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
 * Copyright 1999-2007 Rogue Wave Software, Inc.
 * 
 **************************************************************************/


#include <math.h>


// the float and long double versions of the C90 functions, which
// the C++ <cmath> overloads forward to

float test_acosf (float x) { return acosf (x); }
float test_asinf (float x) { return asinf (x); }
float test_atan2f (float x, float y) { return atan2f (x, y); }
float test_atanf (float x) { return atanf (x); }
float test_ceilf (float x) { return ceilf (x); }
float test_cosf (float x) { return cosf (x); }
float test_coshf (float x) { return coshf (x); }
float test_expf (float x) { return expf (x); }
float test_fabsf (float x) { return fabsf (x); }
float test_floorf (float x) { return floorf (x); }
float test_fmodf (float x, float y) { return fmodf (x, y); }
float test_frexpf (float x, int *e) { return frexpf (x, e); }
float test_ldexpf (float x, int e) { return ldexpf (x, e); }
float test_log10f (float x) { return log10f (x); }
float test_logf (float x) { return logf (x); }
float test_modff (float x, float *y) { return modff (x, y); }
float test_powf (float x, float y) { return powf (x, y); }
float test_sinf (float x) { return sinf (x); }
float test_sinhf (float x) { return sinhf (x); }
float test_sqrtf (float x) { return sqrtf (x); }
float test_tanf (float x) { return tanf (x); }
float test_tanhf (float x) { return tanhf (x); }

long double test_acosl (long double x) { return acosl (x); }
long double test_asinl (long double x) { return asinl (x); }
long double test_atan2l (long double x, long double y) { return atan2l (x, y); }
long double test_atanl (long double x) { return atanl (x); }
long double test_ceill (long double x) { return ceill (x); }
long double test_coshl (long double x) { return coshl (x); }
long double test_cosl (long double x) { return cosl (x); }
long double test_expl (long double x) { return expl (x); }
long double test_fabsl (long double x) { return fabsl (x); }
long double test_floorl (long double x) { return floorl (x); }
long double test_fmodl (long double x, long double y) { return fmodl (x, y); }
long double test_frexpl (long double x, int *e) { return frexpl (x, e); }
long double test_ldexpl (long double x, int e) { return ldexpl (x, e); }
long double test_log10l (long double x) { return log10l (x); }
long double test_logl (long double x) { return logl (x); }
long double test_modfl (long double x, long double *y) { return modfl (x, y); }
long double test_powl (long double x, long double y) { return powl (x, y); }
long double test_sinhl (long double x) { return sinhl (x); }
long double test_sinl (long double x) { return sinl (x); }
long double test_sqrtl (long double x) { return sqrtl (x); }
long double test_tanhl (long double x) { return tanhl (x); }
long double test_tanl (long double x) { return tanl (x); }
