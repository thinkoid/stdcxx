/***************************************************************************
 *
 * num_get.cpp
 *
 * $Id$
 *
 ***************************************************************************
 *
 * Licensed to the Apache Software  Foundation (ASF) under one or more
 * contributor  license agreements.  See  the NOTICE  file distributed
 * with  this  work  for  additional information  regarding  copyright
 * ownership.   The ASF  licenses this  file to  you under  the Apache
 * License, Version  2.0 (the  "License"); you may  not use  this file
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
 * Copyright 2001-2007 Rogue Wave Software, Inc.
 * 
 **************************************************************************/

#define _RWSTD_LIB_SRC
#include <rw/_defs.h>     

#include "setlocale.h"
#include "strtol.h"

#include <ios>          // for ios_base, needed by <rw/_punct.h>
#include <loc/_num_get.h>

#include <errno.h>      // for ERANGE, errno
#include <float.h>      // for {DBL,FLT,LDBL}_{MIN,MAX}
#include <stdlib.h>     // for strtod()
#include <string.h>     // for memcpy()

#include <rw/_defs.h>     


// in case EINVAL is not #defined in errno.h
#ifdef EINVAL
#  define _RWSTD_EINVAL EINVAL
#else   // if !defined (EINVAL)
// actual value of EINVAL on both Linux and SunOS is 22
#  define _RWSTD_EINVAL 22
#endif   // EINVAL



_RWSTD_NAMESPACE (__rw) { 


typedef unsigned char UChar;


#ifndef _RWSTD_NO_OBJECT_MANGLING

extern "C" {

#else   // if defined (_RWSTD_NO_OBJECT_MANGLING)

extern "C++" {

#endif   // _RWSTD_NO_OBJECT_MANGLING


extern const float       __rw_flt_infinity;
extern const double      __rw_dbl_infinity;

#ifndef _RWSTD_NO_LONG_DOUBLE

extern const long double __rw_ldbl_infinity;

#endif   // _RWSTD_NO_LONG_DOUBLE

}   // extern "C"/"C++"


// verifies that the `grps' array of size `ngrps' representing the
// lengths of individual groups of digits separated by thousands_sep
// in the representation of a number corresponds to the `grping' array
// of `ngrpings' elements
// returns a non-negative number of success, negative number whose
// absolute value indicates the bad group on failure

// Examples of valid groups:
//
//      sequence        grps  grping
//
//           123        "\3"  "\3\2"   3 <= 3,
//         1,234      "\1\3"  "\3\2"   1 <= 2 && 3 == 3
//      1,23,456    "\1\2\3"  "\3\2"   1 <= 2 && 2 == 2 && 3 == 3
//     12,34,567    "\2\2\3"  "\3\2"   2 <= 2 && 2 == 2 && 3 == 3
//   1,34,56,789  "\1\2\2\3"  "\3\2"   1 <= 2 && 2 == 2 && 2 == 2 && 3 == 3

// Examples of invalid groups:
//
//      sequence        grps  grping
//
//           123        "\3"  "\1\2"   !(3 <= 1)
//        1,2,34    "\1\1\2"  "\1\3"   1 <= 1 && 1 == 1 && !(2 == 3)
//      1,23,456    "\1\2\3"  "\1\2"   1 <= 1 && 2 == 2 && !(3 == 2)

_RWSTD_EXPORT int
__rw_check_grouping (const char *grps,   _RWSTD_SIZE_T ngrps,
                     const char *grping, _RWSTD_SIZE_T ngrpings)
{
    if (!ngrps || !ngrpings)
        return 0;

    _RWSTD_ASSERT (0 != grps);
    _RWSTD_ASSERT (0 != grping);

    // current grouping number
    _RWSTD_SIZE_T grpn = ngrpings;

    // find the last non-zero grouping if it exists (see below)
    while (grpn-- != ngrpings && !grping [grpn])
        /* silence gcc -Wempty-body warning */;

    // the index of the last meaningful grouping (e.g., `grphi' of
    // "\3\0\0\0" is 2 since the redundant NULs can be safely ignored)
    const _RWSTD_SIZE_T grphi = (_RWSTD_SIZE_T)(-1) == grpn ? 0 : grpn;

    grpn = 0;

    typedef const UChar* PCUChar;

    // iterate over elements of `groups' starting with the least significant
    // one (at the greatest index), comparing each against the relevant
    // element of `groupings' (at the lowest index)
    for (PCUChar g = _RWSTD_REINTERPRET_CAST (PCUChar, grps + ngrps - 1),
                 e = _RWSTD_REINTERPRET_CAST (PCUChar, grping); ; --g) {

        typedef const void* PVoid;

        if (PVoid (g) == PVoid (grps)) {
            // the most significant group may be shorter (but not empty)
            if (*g > *e || !*g) {
                // subtracting from 0 to avoid warnings about
                // applying the - operator to an unsigned type
                grpn = 0U - (grpn + 1U);
            }

            break;
        }

        if (*g != *e) {
            // all but the most significant group must be exactly
            // as long as the last expected group length
            grpn = 0U - (grpn + 1U);
            break;
        }

        if (grpn++ < grphi)
            ++e;   // `*e' is the expected length of the group
    }

    // negative return value indicates an error
    return int (grpn);
}


// validates a floating point value and errno value
template <class FloatT>
inline FloatT
__rw_validate (FloatT val, FloatT fmin, FloatT finf, char sign, int &err)
{
    if (ERANGE == err) {

        if (val < -(fmin)) {
            // negative overflow: store -infinity and set failbit
            val = -finf;
            err = _RWSTD_IOS_FAILBIT;
        }
        else if (val > (fmin)) {
            // positive overflow: store +infinity and set failbit
            val = finf;
            err = _RWSTD_IOS_FAILBIT;
        }
        else {   // val >= fmin && val <= max
            // benign underflow: store +/-fmin, clear failbit
            val = '-' == sign ? -(fmin) : (fmin);
            err = 0;
        }
    }
    else
        err = 0;

    return val;
}


_RWSTD_EXPORT int
__rw_get_num (void *pval, const char *buf, int type, int flags,
              const char *groups, _RWSTD_SIZE_T ngroups,
              const char *grouping, _RWSTD_SIZE_T ngroupings)
{
    // 22.2.2.1.2, p3: Stage 1
    int err = 0;   // errno value, ERANGE, or 0

    _RWSTD_ASSERT (0 != buf);
    _RWSTD_ASSERT ('+' == *buf || '-' == *buf);

    if (!(type & __rw_facet::_C_floating)) {

        //////////////////////////////////////////////////////////////
        // integral parsing (including void*)

        // Stage 2 passes the base it determined in basefield
        const int basefield = flags & _RWSTD_IOS_BASEFIELD;

        const int base =   _RWSTD_IOS_OCT == basefield ? 8
                         : _RWSTD_IOS_HEX == basefield ? 16
                         : 10;

        union {
#ifdef _RWSTD_LONG_LONG

            _RWSTD_LONG_LONG           ll;
            unsigned _RWSTD_LONG_LONG ull;

#endif   // _RWSTD_LONG_LONG

            long           l;
            unsigned long ul;
        } val;

        if (__rw_facet::_C_ullong == type) {

#ifdef _RWSTD_LONG_LONG

            val.ull = _RW::__rw_strtoull (buf, &err, base);

#else   // if !defined (_RWSTD_LONG_LONG)

            _RWSTD_ASSERT (!"logic error: unsigned long long not configured");

#endif   // _RWSTD_LONG_LONG
        }
        else if (__rw_facet::_C_llong == type) {

#ifdef _RWSTD_LONG_LONG

            val.ll = _RW::__rw_strtoll (buf, &err, base);

#else   // if !defined (_RWSTD_LONG_LONG)

            _RWSTD_ASSERT (!"logic error: long long not configured");

#endif   // _RWSTD_LONG_LONG
        }
        else if (type & __rw_facet::_C_signed || __rw_facet::_C_bool == type) {
            val.l = _RW::__rw_strtol (buf, &err, base);
        }
        else if (__rw_facet::_C_pvoid == type) {

#if defined (_RWSTD_LONG_LONG) && _RWSTD_PTR_SIZE > _RWSTD_LONG_SIZE
            // assume pointers fit into long long
            val.ull = _RW::__rw_strtoull (buf, &err, base);
#else
            val.ul = _RW::__rw_strtoul (buf, &err, base);
#endif
        }
        else {
            val.ul = _RW::__rw_strtoul (buf, &err, base);
        }

        if (_RWSTD_EINVAL == err)
            return _RWSTD_IOS_FAILBIT;

        _RWSTD_ASSERT (0 != pval);

        switch (type) {
        case __rw_facet::_C_bool:
            if (0 == val.l || 1 == val.l) {
                *_RWSTD_STATIC_CAST (bool*, pval) = 1 == val.l;
            }
            else
                err = ERANGE;

            break;

        case __rw_facet::_C_ushort:
            // detect and handle unsigned overflow
            if (val.ul > _RWSTD_USHRT_MAX) {
                val.ul = _RWSTD_USHRT_MAX;
                err    = ERANGE;
            }
            else {
                if ('-' == *buf)
                    val.ul = _RWSTD_USHRT_MAX - val.ul + 1;
            }

            *_RWSTD_STATIC_CAST (unsigned short*, pval) =
                _RWSTD_STATIC_CAST (unsigned short, val.ul);
            break;
                
        case __rw_facet::_C_short:
            // detect and handle overflow
            if (val.l < _RWSTD_SHRT_MIN) {
                val.l = long (_RWSTD_SHRT_MIN);
                err   = ERANGE;
            }
            else if (val.l > _RWSTD_SHRT_MAX) {
                val.l = long (_RWSTD_SHRT_MAX);
                err   = ERANGE;
            }

            *_RWSTD_STATIC_CAST (short*, pval) =
                _RWSTD_STATIC_CAST (short, val.l);
            break;

        case __rw_facet::_C_uint:

            // detect and handle unsigned overflow

#if _RWSTD_UINT_MAX < _RWSTD_ULONG_MAX

            if (val.ul > _RWSTD_UINT_MAX)

#else   // if _RWSTD_UINT_MAX >= _RWSTD_ULONG_MAX

            if (val.ul == _RWSTD_ULONG_MAX && ERANGE == err)

#endif   // _RWSTD_UINT_MAX < _RWSTD_ULONG_MAX

            {
                val.ul = _RWSTD_UINT_MAX;
            }
            else {
                if ('-' == *buf)
                    val.ul = _RWSTD_UINT_MAX - val.ul + 1;
            }

            *_RWSTD_STATIC_CAST (unsigned*, pval) = unsigned (val.ul);
            break;

        case __rw_facet::_C_int:

            // detect and handle overflow

#if _RWSTD_UINT_MAX < _RWSTD_ULONG_MAX

            if (val.l < _RWSTD_INT_MIN) {
                val.l = long (_RWSTD_INT_MIN);
                err   = ERANGE;
            }
            else if (val.l > _RWSTD_INT_MAX) {
                val.l = long (_RWSTD_INT_MAX);
                err   = ERANGE;
            }

#else   // if _RWSTD_UINT_MAX >= _RWSTD_ULONG_MAX

            if (ERANGE == err) {
                if (_RWSTD_LONG_MIN == val.l)
                    val.l = long (_RWSTD_INT_MIN);
                else if (_RWSTD_LONG_MAX == val.l)
                    val.l = long (_RWSTD_INT_MAX);
            }

#endif   // _RWSTD_UINT_MAX < _RWSTD_ULONG_MAX

            *_RWSTD_STATIC_CAST (int*, pval) = int (val.l);
            break;

        case __rw_facet::_C_ulong:
            *_RWSTD_STATIC_CAST (unsigned long*, pval) = val.ul;
            break;
            
        case __rw_facet::_C_long:
            *_RWSTD_STATIC_CAST (long*, pval) = val.l;
            break;

#ifdef _RWSTD_LONG_LONG

        case __rw_facet::_C_ullong:
            *_RWSTD_STATIC_CAST (unsigned _RWSTD_LONG_LONG*, pval) = val.ull;
            break;

        case __rw_facet::_C_llong:
            *_RWSTD_STATIC_CAST (_RWSTD_LONG_LONG*, pval) = val.ll;
            break;

#endif   // _RWSTD_LONG_LONG

        case __rw_facet::_C_pvoid:
            *_RWSTD_STATIC_CAST (void**, pval) =
#if defined (_RWSTD_LONG_LONG) && _RWSTD_PTR_SIZE > _RWSTD_LONG_SIZE
                _RWSTD_REINTERPRET_CAST (void*, val.ull);
#else
                _RWSTD_REINTERPRET_CAST (void*, val.ul);
#endif

            // disable grouping
            grouping = "";
            break;

        }

        if (err)
            err = _RWSTD_IOS_FAILBIT;
    }
    else {
        //////////////////////////////////////////////////////////////
        // floating point parsing

        // fail if the sequence is empty
        if (!*buf)
            return _RWSTD_IOS_FAILBIT;

        char *end;

        // save errno value, reset only if ERANGE to distinguish strtol()
        // failure from success if errno already set on function entry
        const int errno_save = errno;

        if (ERANGE == errno_save)
            errno = 0;

        _RWSTD_ASSERT (0 != pval);

        switch (type) {
        case __rw_facet::_C_float: {

            // assumes `buf' is formatted for the current locale
            // specifically, affects the value of decimal_point
            float f = strtof (_RWSTD_CONST_CAST (char*, buf), &end);

            err = errno;

            _RWSTD_ASSERT (0 != end);

            if ('.' == *end || (end == buf && '.' == buf [1])) {
                // on failure caused by an unrecognized decimal point
                // set teporarily the global locale to "C" and reparse
                __rw_setlocale loc ("C", _RWSTD_LC_NUMERIC);

                f   = strtof (_RWSTD_CONST_CAST (char*, buf), &end);
                err = errno;
            }

            // restore errno if it was reset above
            if (ERANGE == errno_save)
                errno = ERANGE;

            if (*end)
                return _RWSTD_IOS_FAILBIT;

            *_RWSTD_STATIC_CAST (float*, pval) =
                __rw_validate (f, float (_RWSTD_FLT_MIN),
                               __rw_flt_infinity, *buf, err);

            break;

        }

        case __rw_facet::_C_double: {
            // assumes `buf' is formatted for the current locale
            // specifically, affects the value of decimal_point
            double d = strtod (buf, &end);
            err = errno;

            _RWSTD_ASSERT (0 != end);

            if ('.' == *end || (end == buf && '.' == buf [1])) {
                // on failure caused by an unrecognized decimal point
                // set teporarily the global locale to "C" and reparse
                __rw_setlocale loc ("C", _RWSTD_LC_NUMERIC);

                d   = strtod (buf, &end);
                err = errno;
            }

            // restore errno if it was reset above
            if (ERANGE == errno_save)
                errno = errno_save;

            if (*end)
                return _RWSTD_IOS_FAILBIT;

            *_RWSTD_STATIC_CAST (double*, pval) =
               __rw_validate (d, double (_RWSTD_DBL_MIN),
                              __rw_dbl_infinity, *buf, err);
            break;
        }


        case __rw_facet::_C_ldouble: {

            typedef long double LDbl;

#ifndef _RWSTD_NO_LONG_DOUBLE

            LDbl ld;

            // assumes `buf' is formatted for the current locale
            // specifically, affects the value of decimal_point

            // not HP-UX or HP-UX with gcc
            ld = strtold (buf, &end);

            err = errno;

            _RWSTD_ASSERT (0 != end);

            if ('.' == *end || (end == buf && '.' == buf [1])) {
                // on failure caused by an unrecognized decimal point
                // set teporarily the global locale to "C" and reparse
                __rw_setlocale loc ("C", _RWSTD_LC_NUMERIC);

                ld = strtold (buf, &end);

                err = errno;
            }

            // restore errno if it was reset above
            if (ERANGE == errno_save)
                errno = errno_save;

            if (*end)
                return _RWSTD_IOS_FAILBIT;

            *_RWSTD_STATIC_CAST (LDbl*, pval) =
                __rw_validate (ld, LDbl (_RWSTD_LDBL_MIN),
                               __rw_ldbl_infinity, *buf, err);

#endif   // _RWSTD_LONG_DOUBLE

            break;
        }
        }
    }

    _RWSTD_ASSERT (grouping);

    // 22.2.2.1.2, p12: check digit grouping, if any
    // note that grouping is optional and parsing fails due to bad format
    // only if the positions thousands_sep's do not match those specified
    // by grouping; in such cases, the value is still stored
    if (   (   *grouping && ngroups > 1
            && 0 > __rw_check_grouping (groups, ngroups, grouping, ngroupings))
        || (!*grouping && ngroups > 1))
        err |= _RWSTD_IOS_FAILBIT;

    return err;
}


}   // namespace __rw
