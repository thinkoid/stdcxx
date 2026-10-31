/***************************************************************************
 *
 * _time_get.cc - definition of std::time_get members
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
 * Copyright 1994-2006 Rogue Wave Software, Inc.
 * 
 **************************************************************************/


#include <streambuf>
#include <loc/_ctype.h>
#include <loc/_punct.h>
#include <loc/_locale.h>


namespace __rw {

_RWSTD_EXPORT _RWSTD_SIZE_T
__rw_get_timepunct (const __rw_facet *pfacet,
                    int data [5], _RWSTD_C::tm *tmb, int **pmem,
                    const void *names[], _RWSTD_SIZE_T sizes[]);

}   // namespace __rw


namespace std { 


template <class _CharT, class _InputIter>
__rw::__rw_facet_id time_get<_CharT, _InputIter>::id;


// outlined to avoid generating a vtable in each translation unit
// that uses the class
template <class _CharT, class _InputIter>
/* virtual */ time_get<_CharT, _InputIter>::
~time_get ()
{
    // no-op
}


// 22.2.5.1.2, p1: the order of the day, month and year components
// of the facet's x format. The order is read off the format: %d and
// %e are the day, %b, %B, %h and %m the month, %C, %y and %Y the
// year, %D and %F the sequences they stand for; the E and O
// modifiers, the flags and widths of the C library's formats, %%,
// whitespace and the weekday conversions are skipped since they do
// not take part in the order, and a %y right after a %C completes
// that year rather than repeating it. A format that lacks one of the
// three, repeats one, or carries any other conversion gives no_order,
// which footnote 233 allows in a valid locale.
template <class _CharT, class _InputIter>
/* virtual */ typename time_get<_CharT, _InputIter>::dateorder
time_get<_CharT, _InputIter>::
do_date_order () const
{
    const char_type* __names [1];
    _RWSTD_SIZE_T    __sizes [1];

    const void** __pv =
        reinterpret_cast< const void** >(__names);

    tm   __tmp  = tm ();
    int *__pmem = 0;

    int __data [5] = {
        'x', '\0', sizeof (char_type) > sizeof (char), 0, 0
    };

    // the x format, a single string stored in `names [0]'
    const _RWSTD_SIZE_T __cnt =
        __rw::__rw_get_timepunct (this, __data, &__tmp,
                                 &__pmem, __pv, __sizes);

    if (1 != __cnt)
        return no_order;

    char          __order [3];
    _RWSTD_SIZE_T __n = 0;
    bool          __century = false;

    const char_type*       __p   = __names [0];
    const char_type* const __end = __p + __sizes [0];

    for ( ; __p != __end; ++__p) {

        if (char_type ('%') != *__p)
            continue;

        // past the flags and the width, then the modifier
        do {
            if (++__p == __end)
                return no_order;
        } while (   char_type ('-') == *__p || char_type ('_') == *__p
                 || char_type ('0') == *__p || char_type ('^') == *__p
                 || char_type ('#') == *__p
                 || (char_type ('1') <= *__p && *__p <= char_type ('9')));

        if (char_type ('E') == *__p || char_type ('O') == *__p)
            if (++__p == __end)
                return no_order;

        const char *__comp = "";

        switch (*__p) {
        case 'd': case 'e':                     __comp = "d";   break;
        case 'b': case 'B': case 'h': case 'm': __comp = "m";   break;
        case 'C': case 'Y':                     __comp = "y";   break;
        case 'y':             __comp = __century ? "" : "y";    break;
        case 'D':                               __comp = "mdy"; break;
        case 'F':                               __comp = "ymd"; break;
        case 'a': case 'A': case 'n': case 't': case '%':       break;
        default:
            return no_order;
        }

        __century = char_type ('C') == *__p;

        for ( ; *__comp; ++__comp) {

            if (3 == __n)
                return no_order;

            for (_RWSTD_SIZE_T __i = 0; __i != __n; ++__i)
                if (__order [__i] == *__comp)
                    return no_order;

            __order [__n++] = *__comp;
        }
    }

    if (3 != __n)
        return no_order;

    if ('d' == __order [0])
        return 'm' == __order [1] ? dmy : no_order;

    if ('m' == __order [0])
        return 'd' == __order [1] ? mdy : no_order;

    return 'm' == __order [1] ? ymd : ydm;
}


template <class _CharT, class _InputIter>
typename time_get<_CharT, _InputIter>::iter_type
time_get<_CharT, _InputIter>::
get (iter_type __it, iter_type __end, ios_base &__fl, _RWSTD_IOSTATE &__err,
     tm *__tmb, const char_type *__pat, const char_type *__pat_end) const
{
    _RWSTD_ASSERT (0 != __pat);
    _RWSTD_ASSERT (__pat <= __pat_end);

    const ctype<char_type> &__ctp =
        std::use_facet< ctype<char_type> >(__fl.getloc ());

    // extension: `tmb' may be 0
    tm __tmp = __tmb ? *__tmb : tm ();

    _RWSTD_IOSTATE __errtmp = __rw::__rw_goodbit;

    char __fmt [] = { '\0', '\0' };

    for (; __pat != __pat_end; ++__pat) {

        if (__errtmp) {
            // failed to match the whole pattern
            __errtmp |= __rw::__rw_failbit;
            break;
        }

        const char __ch = __ctp.narrow (*__pat, '\0');

        if (__fmt [0]) {
            if ('E' == __ch || 'O' == __ch)
                __fmt [1] = __ch;
            else if ('.' == __ch || __ch >= '0' && __ch <= '9') {
                // FIXME: handle the "%.[0-9]*" HP-UX extension
            }
            else {
                __it = do_get (__it, __end, __fl, __errtmp, &__tmp, __ch,
                               __fmt [1]);
                __fmt [0] = __fmt [1] = '\0';   // reset
            }
        }
        else if ('%' == __ch) {
            __fmt [0] = __ch;
        }
        else if (__ctp.is (ctype_base::space, *__pat)) {
            // skip all whitespace characters
            while (__it != __end && __ctp.is (ctype_base::space, *__it))
                ++__it;
        }
        else {
            if (__it != __end && __ch == __ctp.narrow (*__it, '\0'))
                ++__it;
            else {
                // mismatch between pattern and input
                __errtmp |= __rw::__rw_failbit;
                break;
            }
        }
    }

    // store value only on success
    if (!(__errtmp & ~__rw::__rw_eofbit) && __tmb)
        *__tmb = __tmp;

    __err |= __errtmp;

    return __it;
}


template <class _CharT, class _InputIter>
/* virtual */ typename time_get<_CharT, _InputIter>::iter_type
time_get<_CharT, _InputIter>::
do_get (iter_type __it, iter_type __end, ios_base &__fl,
        _RWSTD_IOSTATE &__err, tm *__tmb,
        char __fmt, char __modifier /* = '\0' */) const
{
    // enough pointers for 100 alternative numeric symbols and their sizes
    const char_type* __names [100];
    _RWSTD_SIZE_T    __sizes [sizeof __names / sizeof *__names];

    const void** __pv =
        reinterpret_cast< const void** >(__names);

    tm __tmp = __tmb ? *__tmb : tm ();

    int *__pmem = 0;   // pointer to std::tm member

    _RWSTD_SIZE_T __dup;   // number of allowed duplicates

    const ctype<char_type> &__ctp =
        std::use_facet< ctype<char_type> >(__fl.getloc ());

    switch (__fmt) {

    case 'A': case 'a': case 'B': case 'b': case 'h':
        // names of days and months can have duplicates (i.e.,
        // it can be either a full name aor an abbreviation)
        __dup = 2;
        break;

    case 'n': case 't': {   // any whitespace
        
        for ( ; ; ++__it) {

            if (__it == __end) {
                __err |= __rw::__rw_eofbit;
                break;
            }

            if (!__ctp.is (ctype_base::space, *__it))
                break;
        }

        return __it;
    }

    case 'Z': case 'z': {
        // %Z and %z read as the C library's strptime reads them, after
        // leading whitespace: %Z a time zone name, a run of non-space
        // characters that may be empty and is not converted; %z an
        // offset from UTC, [+-]hh, [+-]hhmm, [+-]hh:mm or Z, stored in
        // tm_gmtoff where std::tm has it; the minutes are 0..59, the
        // hours any two digits. With a modifier, both are invalid.
        if (__modifier) {
            __dup = 1;
            break;
        }

        while (__it != __end && __ctp.is (ctype_base::space, *__it))
            ++__it;

        _RWSTD_IOSTATE __errtmp = __rw::__rw_goodbit;

        if ('Z' == __fmt) {
            while (__it != __end && !__ctp.is (ctype_base::space, *__it))
                ++__it;
        }
        else {
            const char __sign =
                __it != __end ? __ctp.narrow (*__it, '\0') : '\0';

            bool __colon  = false;   // minutes after a colon
            int  __ndigs  = 0;       // number of digits read
            long __offset = 0;       // hhmm

            if ('Z' == __sign) {
                // UTC, equivalent to +0000
                __ndigs = 4;
                ++__it;
            }
            else if ('+' == __sign || '-' == __sign) {

                for (++__it; __it != __end && __ndigs < 4; ++__it) {

                    const char __ch = __ctp.narrow (*__it, '\0');

                    if (2 == __ndigs && ':' == __ch && !__colon)
                        __colon = true;
                    else if ('0' <= __ch && __ch <= '9') {
                        __offset = 10 * __offset + __ch - '0';
                        ++__ndigs;
                    }
                    else
                        break;
                }
            }

            if (2 == __ndigs && !__colon)
                __offset *= 100;
            else if (4 != __ndigs || 59 < __offset % 100)
                __errtmp |= __rw::__rw_failbit;

            if (!__errtmp && __tmb) {

#if !defined (_RWSTD_NO_TM_GMTOFF) && defined (_RWSTD_NO_PURE_C_HEADERS)

                __offset = (__offset / 100 * 60 + __offset % 100) * 60;

                __tmb->tm_gmtoff = '-' == __sign ? -__offset : __offset;

#endif   // !_RWSTD_NO_TM_GMTOFF && _RWSTD_NO_PURE_C_HEADERS

            }
        }

        if (__it == __end)
            __errtmp |= __rw::__rw_eofbit;

        __err |= __errtmp;

        return __it;
    }

    default: __dup = 1;
    }

    int __data [5] = {
        // array of input data for __rw_get_timepunct will on output
        // contain data needed to interpret values parsed below
        __fmt, __modifier, sizeof (char_type) > sizeof (char), 0, 0
    };

    // get the number of punctuator (or format) string(s) and their values
    const _RWSTD_SIZE_T __cnt =
        __rw::__rw_get_timepunct (this, __data, &__tmp,
                                 &__pmem, __pv, __sizes);

    // std::tm has no member for the week number of %U and %W: it is
    // read into a local, validated and discarded; C++11 22.4.5.1.2,
    // p12, has do_get assign the members the directive corresponds to
    int __week = 0;

    if (('U' == __fmt || 'W' == __fmt) && 'E' != __modifier)
        __pmem = &__week;

    if (1 == __cnt) {
        _RWSTD_ASSERT (0 != __names [0]);

        // single punctuator string stored in `names [0]'
        // e.g., "%m/%d/%y" for the format of "%D" (`fmt' == 'D')
        return get (__it, __end, __fl, __err, __tmb,
                    __names [0], __names [0] + __sizes [0]);
    }

    _RWSTD_IOSTATE __errtmp = __rw::__rw_goodbit;

    if (__cnt > 1) {

        // parse string on input

        // set `inx' to 0 to allow numeric input, SIZE_MAX otherwise
        _RWSTD_SIZE_T __inx = 'O' == __modifier ? 0 : _RWSTD_SIZE_MAX;

        // on function input, `interr' contains the maximum number
        // of duplicates; on output, it contains an iostate value
        // and `inx' contains the index of the parsed name within
        // the `names' array
        int __interr = int (__dup);

        if ('p' == __fmt && 0 == __sizes [0]) {
            // an empty AM name matches at once, as in the C library's
            // strptime, which tries the AM name first; the PM name is
            // never reached
            __inx    = 0;
            __interr = __it == __end ? _RWSTD_IOS_EOFBIT : 0;
        }
        else
            __it = __rw::__rw_match_name (__it, __end, __names, __sizes,
                                         __cnt, __inx, __interr, &__fl);

        __errtmp = _RWSTD_IOSTATE (__interr);

        if (__inx == _RWSTD_SIZE_MAX) {
            // no name matches; a value without an alternative symbol
            // is read as digits by __rw_match_name above
            __errtmp |= __rw::__rw_failbit;
        }
        else if ('p' == __fmt) {
            if (__pmem) {
                // %p sets the half of the day of the hour it finds, a
                // %H or a %I parsed before it or the caller's: AM takes
                // the 12 hours off an hour of 12 or more, PM adds them
                // to an hour below 12
                if (*__pmem < 0 || 23 < *__pmem) {
                    // invalid data
                    __errtmp |= __rw::__rw_failbit;
                }
                else if (!__inx && 12 <= *__pmem)
                    *__pmem -= 12;
                else if (__inx && *__pmem < 12)
                    *__pmem += 12;
            }
            else {
                // invalid (or unimplemented) format
                __errtmp |= __rw::__rw_failbit;
            }
        }
        else if (__pmem) {
            _RWSTD_ASSERT (0 != __pmem);

            if (__inx == __cnt && !__sizes [0] || __inx >= 2 * __cnt)
                *__pmem = int (__inx - __cnt);
            else if (__inx < __cnt)
                *__pmem = int (__inx % (__cnt / __dup));
            else
                __errtmp |= __rw::__rw_failbit;

            const int __adj = __data [0];   // value to increment result by
            const int __fac = __data [1];   // value to multiply result by

            *__pmem = *__pmem * __fac + __adj;

            int __lob = __data [2];   // lower bound on valid input
            int __hib = __data [3];   // upper bound on valid input

            if ('y' == __fmt && *__pmem < 100) {
                // %Oy pivots as %y does below
                __lob =  69;
                __hib = 168;

                if (*__pmem < 69)
                    *__pmem += 100;
            }

            // validate input value
            if (*__pmem < __lob || *__pmem > __hib)
                __errtmp |= __rw::__rw_failbit;
        }
        else {
            // invalid (or unimplemented) format
            __errtmp |= __rw::__rw_failbit;
        }
    }
    else if (__pmem) {

        // parse numeric input

        _RWSTD_SIZE_T __nread = 0;

        // the most digits to read, as in the C library's strptime: 2 for
        // %y and the other two-digit fields, 3 for %j and 4 for %Y, so
        // that adjacent conversions separate themselves
        const _RWSTD_SIZE_T __dig = static_cast< _RWSTD_SIZE_T >(__data [4]);

        // the upper bound on the value as read, before the adjustment
        // and the factor apply, 99 for %y, whose bounds are the pivot's;
        // as in the C library's strptime, a digit that would take the
        // value past it is left for what follows: %d on "45" reads 4
        const int __max = 'y' == __fmt ? 99
            : (__data [3] - __data [0]) / __data [1];

        // white space before the digits is skipped, as in strptime
        while (__it != __end && __ctp.is (ctype_base::space, *__it))
            ++__it;

        int __mem = 0;

        for ( ; ; ++__it, ++__nread) {
            if (__it == __end) {
                __err |= __rw::__rw_eofbit;
                break;
            }

            if (__nread == __dig || __max / 10 < __mem)
                break;

            const char_type __c = *__it;
            if (!__ctp.is (ctype_base::digit, __c))
                break;

            const char __ch = __ctp.narrow (__c, '\0');
            if (__ch < '0' || __ch > '9')
                break;

            __mem = 10 * __mem + __ch - '0';
        }

        if (__nread) {

            const int __adj = __data [0];   // value to increment result by
            const int __fac = __data [1];   // value to multiply result by

            int __lob = __data [2];   // lower bound on valid input
            int __hib = __data [3];   // upper bound on valid input

            if ('y' == __fmt) {

                // IEEE Std 1003.1-2001: When a century is not otherwise
                // specified, values in the range [69,99] shall refer to
                // years 1969 to 1999 inclusive, and values in the range
                // [00,68] shall refer to years 2000 to 2068 inclusive.

                __lob =  69;
                __hib = 168;

                if (__mem < 69)
                    __mem += 100;
            }

            *__pmem = __mem * __fac + __adj;

            // validate input value
            if (*__pmem < __lob || *__pmem > __hib)
                __errtmp |= __rw::__rw_failbit;
        }
        else {
            // invalid data
            __errtmp |= __rw::__rw_failbit;
        }
    }
    else {
        // invalid (or unimplemented) format
        __errtmp |= __rw::__rw_failbit;
    }

    if (!(__errtmp & ~__rw::__rw_eofbit) && 'E' != __modifier) {

        // C++11 22.4.5.1.2, p14: do_get updates the tm it is given, so
        // that a member carries what one directive leaves to another

        if ('C' == __fmt) {
            // %C sets the century and keeps the year within it found
            // in tm_year: "%y %C" on "26 19" is 1926; a %y sets the
            // whole year, pivoted, so "%C %y" on "19 26" is 2026
            const int __year = __tmb ? __tmb->tm_year : 0;

            __tmp.tm_year += (__year + 1900) % 100;
        }
        else if ('I' == __fmt || 'l' == __fmt) {
            // %I sets the hour within the half of the day found in
            // tm_hour, as %p sets the half of the hour it finds, so
            // that "%p %I" reads the same as "%I %p"
            const int __hour = __tmb ? __tmb->tm_hour : 0;

            __tmp.tm_hour %= 12;

            if (12 <= __hour)
                __tmp.tm_hour += 12;
        }
    }

    if (!(__errtmp & ~__rw::__rw_eofbit) && __tmb)
        *__tmb = __tmp;

    __err |= __errtmp;

    return __it;
}


// outlined to avoid generating a vtable in each translation unit
// that uses the class
template <class _CharT, class _InputIter>
/* virtual */ time_get_byname<_CharT, _InputIter>::
~time_get_byname ()
{
    // no-op
}


}   // namespace std
