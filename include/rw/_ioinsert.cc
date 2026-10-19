/***************************************************************************
 *
 * _ioinsert.cc - definitions for the C++ Standard Library inserter
 *                helper templates
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

#include <rw/_select.h>


namespace __rw { 


template <class _CharT, class _Traits, class _NativeType>
std::basic_ostream<_CharT, _Traits>&
__rw_insert (std::basic_ostream<_CharT, _Traits> &__strm,
             _NativeType                           __val)
{
    const typename std::basic_ostream<_CharT, _Traits>::sentry
        __opfx (__strm);

    try {

        typedef std::ostreambuf_iterator<_CharT, _Traits> _Iter;
        typedef std::num_put<_CharT, _Iter>                _NumPut;

        if (__opfx && std::use_facet< _NumPut >(__strm.getloc ())
            .put (_Iter (__strm), __strm, __strm.fill (), __val).failed ()) {

            // this may throw but the exception will just be caught
            // and rethrown below; this optimizes for the common case
            // (i.e., success) but pessimizes the case of failure
            __strm.setstate (std::ios_base::badbit);
        }
    }
    catch (...) {
        __strm.setstate (std::ios_base::badbit | __rw::__rw_rethrow);
    }

    return __strm;
}


// optimized for when stream's char_type is the same
// as the type of the character string being inserted,
// i.e., ostream and char* and wostream and wchar_t*
template <class _CharT, class _Traits>
inline _RWSTD_STREAMSIZE
__rw_sputn (std::basic_ostream<_CharT, _Traits> &__strm,
            const _CharT *__s, _RWSTD_STREAMSIZE __len, __rw_true_t)
{
    _RWSTD_ASSERT (!__len || __len > 0 && 0 != __s);

    if (1 == __len) {
        typedef typename _Traits::int_type _IntT;

        const _IntT __ic = __strm.rdbuf ()->sputc (*__s);

        return 0 + !_Traits::eq_int_type (_Traits::eof (), __ic);
    }

    return __strm.rdbuf ()->sputn (__s, __len);
}


// special case for char* insertion into a stream whose
// char_type is not char (e.g., wostream or user-defined)
template <class _CharT, class _Traits>
inline _RWSTD_STREAMSIZE
__rw_sputn (std::basic_ostream<_CharT, _Traits> &__strm,
            const char *__s, _RWSTD_STREAMSIZE __len, __rw_false_t)
{
    _RWSTD_ASSERT (!__len || __len > 0 && 0 != __s);

    const std::ctype<_CharT>& __ctp =
        std::use_facet< std::ctype<_CharT> >(__strm.getloc ());

    for (_RWSTD_STREAMSIZE __i = 0; __i < __len; ++__i) {

        typedef typename _Traits::int_type _IntT;

        const _IntT __ic = __strm.rdbuf ()->sputc (__ctp.widen (__s [__i]));

        if (_Traits::eq_int_type (__ic, _Traits::eof ()))
            return __i;
    }

    return __len;
}


template<class _CharT, class _Traits, class _StringT>
std::basic_ostream<_CharT, _Traits>&
__rw_insert (std::basic_ostream<_CharT, _Traits> &__strm,
             const _StringT                       *__s,
             _RWSTD_STREAMSIZE                     __len,
             _RWSTD_STREAMSIZE                     __width)
{
    _RWSTD_ASSERT (0 != __strm.rdbuf ());
    _RWSTD_ASSERT (0 != __s);
      
    std::ios_base::iostate __err = std::ios_base::goodbit;

    // writes out max(len, width) characters, padding appropriately
    // with the fill character if (len < width);
    // calls width(0) if and only if the write has been successful
    // (i.e., will leave it unchanged if sputn fails or throws an
    // exception)

    try {
  
        const typename std::basic_ostream<_CharT, _Traits>::sentry
            __opfx (__strm);

        if (__opfx) {

            // compute the number of fill chars to pad with
            // according to the rules described in 22.2.2.2.2, p19
            const _RWSTD_STREAMSIZE __pad = __width - __len;

            typedef typename
                __rw_is_same<_CharT, _StringT>::_C_type _Same;

            if (__pad > 0) {
                const int __padbits =
                    std::ios_base::adjustfield & __strm.flags ();

                // output left padding (output is right-aligned by default)
                if (   std::ios_base::left != __padbits
                    && __pad != __strm._C_pad (__pad)
                    // write out (not necessarily null-terminated) string
                    || __len != __rw_sputn (__strm, __s, __len, _Same ())
                    // output right padding (output is left-aligned)
                    || std::ios_base::left == __padbits
                    && __pad != __strm._C_pad (__pad))
                    __err = std::ios_base::badbit;
            }
            else if (__len != __rw_sputn (__strm, __s, __len, _Same ())) {
                __err = std::ios_base::badbit;
            }

            // 21.3.7.9, p4: the inserter calls width(0) after the
            // characters are inserted, whether or not sputn failed
            __strm.width (0);
        }
    }
    catch (...) {
        // set badbit and rethrow the caught exception if badbit
        // is also set in exceptions()
        __strm.setstate (std::ios_base::badbit | __rw::__rw_rethrow);
    }

    if (__err) {
        // set badbit which may throw ios_base::failure
        __strm.setstate (__err);
    }

    return __strm;
}


}   // namespace __rw
