/***************************************************************************
 *
 * _punct.cc
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
 * Copyright 1994-2006 Rogue Wave Software.
 * 
 **************************************************************************/


#include <streambuf>
#include <loc/_ctype.h>


namespace __rw {


template <class _CharT, class _InputIter>
_InputIter __rw_match_name (_InputIter __it, _InputIter __end,
                            const _CharT* const  *__names,
                            const _RWSTD_SIZE_T  *__sizes,
                            _RWSTD_SIZE_T         __count,
                            _RWSTD_SIZE_T        &__inx,
                            int                  &__err,
                            std::ios_base        *__flags)
{
    const _RWSTD_SIZE_T __badval = _RWSTD_SIZE_MAX;

    // the number of bits in each word of the set below
    const _RWSTD_SIZE_T __wbits = sizeof (_RWSTD_SIZE_T) * _RWSTD_CHAR_BIT;

    // a bit for each `name', 256 with a 64-bit size_t and 128 with
    // a 32-bit one; a locale defines at most 100 alt_digits
    _RWSTD_SIZE_T __bits [4] = {
        _RWSTD_SIZE_MAX, _RWSTD_SIZE_MAX, _RWSTD_SIZE_MAX, _RWSTD_SIZE_MAX
    };

    // make sure there are enough bits for all `count' `names'
    _RWSTD_ASSERT (__count <= sizeof __bits / sizeof *__bits * __wbits);

    // on input, `err' contains the maximum allowed number of matches
    const _RWSTD_SIZE_T __matchmax = (_RWSTD_SIZE_T)__err;

    __err = 0;

    _RWSTD_SIZE_T __nmatch = __count;   // total number of positive matches
    _RWSTD_SIZE_T __pos    = 0;         // position within input
    _RWSTD_SIZE_T __num    = __inx;     // input as a number

    __inx = __badval;   // invalidate

    const std::ctype<_CharT> *__ctp = __flags ?
        &std::use_facet< std::ctype<_CharT> >(__flags->getloc ()) : 0;

    _CharT __ch;

    for (; (   (__bits [0] | __bits [1] | __bits [2] | __bits [3])
            || __num != __badval) && !__err; ++__it, ++__pos) {

        if (__it == __end) {
            __err |= _RWSTD_IOS_EOFBIT;
            __ch   = _CharT ();
        }
        else
            __ch = *__it;

        if (__ctp && __num != __badval) {
            if (__ctp->is (std::ctype_base::digit, __ch))
                __num = __num * 10 + __ctp->narrow (__ch, '0') - '0';
            else
                __num = __badval;
        }

        // iterate over all names, clear bits of those that do not match
        for (_RWSTD_SIZE_T __k = 0; __k != __count; ++__k) {

            typedef std::char_traits<_CharT> _Traits;

            const _RWSTD_SIZE_T __word = __k / __wbits;
            const _RWSTD_SIZE_T __mask =
                static_cast< _RWSTD_SIZE_T >(1UL) << (__k % __wbits);

            if (__bits [__word] & __mask) {
                // `name' is still in the set, see if the next char matches
                // (case insensitive comparison done if `ctp' is nonzero)
                if (   __pos < __sizes [__k]
                    && !__err
                    && (__ctp ?
                            _Traits::eq ((__ctp->toupper)(__names [__k][__pos]),
                                         (__ctp->toupper)(__ch))
                          : _Traits::eq (__names [__k][__pos], __ch))) {

                    // next character on `name' mactches input
                    if (__badval == __inx) {
                        // no prior match, remember this one
                        __inx = __k;
                    }
                    else if (   __k != __inx
                             && __sizes [__k] == __sizes [__inx]
                             && __pos + 1 == __sizes [__inx]) {

                        // this match is a duplicate of the last best one
                        // remove this match from the set
                        __bits [__word] &= ~__mask;
                        --__nmatch;
                    }
                    else if (   __sizes [__k] < __sizes [__inx]
                             || __pos >= __sizes [__inx]) {
                        // this match is either shorter than the last one
                        // or the last one will be eliminated
                        __inx = __k;
                    }
                }
                else {
                    if (1 == __nmatch) {
                        if (__badval == __inx && __badval == __num)
                            return __it;
                        goto __endloop;
                    }

                    if (__inx == __k) {
                        // if this `name' is the current shortest match...
                        if (__err && __pos == __sizes [__k]) {
                            // ...and we're at eof, return successfully
                            return __it;
                        }
                        // otherwise forget the match
                        __inx = __badval;
                    }

                    // clear the bit for the `name' that failed to match
                    // and decrement the numeber of matches
                    __bits [__word] &= ~__mask;
                    --__nmatch;
                }
            }
        }

        if (1 == __nmatch && __badval != __inx && __pos + 1 == __sizes [__inx])
            return ++__it;
    }

__endloop:

    // no match leaves `inx' at `badval', which is no index into `sizes'
    if (   __matchmax < __nmatch
        || (__badval != __inx && __pos < __sizes [__inx]))
        __inx = __badval;

    if (__ctp && __badval == __inx && !__err && __num != __badval) {

        // if symbolic matching fails, fall back on numeric parsing
        for ( ; ; ) {
            if (++__it == __end) {
                __err |= _RWSTD_IOS_EOFBIT;
                break;
            }

            __ch = *__it;

            if (__ctp->is (std::ctype_base::digit, __ch))
                __num = __num * 10 + __ctp->narrow (__ch, '0') - '0';
            else
                break;
        }

        __inx = __count + __num;
    }

    return __it;
}


}   // namespace __rw
