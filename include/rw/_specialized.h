// -*- C++ -*-
/***************************************************************************
 *
 * _specialized.h - definitions of specialized algorithms
 *
 * This is an internal header file used to implement the C++ Standard
 * Library. It should never be #included directly by a program.
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
 ***************************************************************************
 *
 * Copyright (c) 1994
 * Hewlett-Packard Company
 *
 * Permission to use, copy, modify, distribute and sell this software
 * and its documentation for any purpose is hereby granted without fee,
 * provided that the above copyright notice appear in all copies and
 * that both that copyright notice and this permission notice appear
 * in supporting documentation.  Hewlett-Packard Company makes no
 * representations about the suitability of this software for any
 * purpose.  It is provided "as is" without express or implied warranty.
 *
 **************************************************************************/

#ifndef _RWSTD_RW_SPECIALIZED_H_INCLUDED
#define _RWSTD_RW_SPECIALIZED_H_INCLUDED


#ifndef _RWSTD_RW_NEW_H_INCLUDED
#  include <rw/_new.h>
#endif   // _RWSTD_RW_NEW_H_INCLUDED


#ifndef _RWSTD_RW_ITERBASE_H_INCLUDED
#  include <rw/_iterbase.h>
#endif   // _RWSTD_RW_ITERBASE_H_INCLUDED


namespace __rw { 


// returns a suggested new capacity for a container needing more space
template <class _Container>
inline typename _Container::size_type
__rw_new_capacity (typename _Container::size_type __size, const _Container*)
{
    typedef typename _Container::size_type _RWSizeT;

    const _RWSizeT __ratio = _RWSizeT (  (_RWSTD_NEW_CAPACITY_RATIO << 10)
                                       / _RWSTD_RATIO_DIVIDER);

    const _RWSizeT __cap =   (__size >> 10) * __ratio
                           + (((__size & 0x3ff) * __ratio) >> 10);

    return (__size += _RWSTD_MINIMUM_NEW_CAPACITY) > __cap ? __size : __cap;
}                     


template <class _TypeT, class _TypeU>
inline void
__rw_construct (_TypeT* __p, _TypeU& __val)
{
    ::new (static_cast< void* >(__p)) _TypeT (__val);
}


template <class _TypeT, class _TypeU>
inline void
__rw_construct (volatile _TypeT* __p, _TypeU& __val)
{
    // remove volatile before invoking operator new
    __rw_construct (const_cast< _TypeT* >(__p), __val);
}

template <class _TypeT>
inline void
__rw_destroy (_TypeT &__ref)
{
    __ref.~_TypeT ();
}


template <class _ForwardIterator> 
inline void
__rw_destroy (_ForwardIterator __first, _ForwardIterator __last)
{
    for (; __first != __last; ++__first)
        __rw_destroy (*__first);
}


// for compilers that don't optimize "empty" loops
template <class _TypeT> 
inline void __rw_destroy (_TypeT**, _TypeT**)
{ }


}   // namespace __rw


namespace std { 

template <class _TypeT>
class allocator;


// 20.4.4.1
template <class _InputIterator, class _ForwardIterator>
inline _ForwardIterator
uninitialized_copy (_InputIterator __first, _InputIterator __last,
                    _ForwardIterator __res)
{
    const _ForwardIterator __start = __res;
    typedef typename iterator_traits<_ForwardIterator>::value_type _TypeT;

    try {
        for (; __first != __last; ++__first, ++__res) {
            // avoid const-qualifying ptr to prevent an HP aCC 3 bug
            volatile void* /* const */ __ptr =
                static_cast< volatile void* >(&*__res);
            ::new (const_cast< void* >(__ptr)) _TypeT (*__first);
        }
    }
    catch (...) {
        __rw::__rw_destroy (__start, __res);
        throw;
    }

    return __res;
}


// extension
template <class _InputIterator, class _ForwardIterator, class _Allocator>
inline _ForwardIterator
uninitialized_copy (_InputIterator __first, _InputIterator __last,
                    _ForwardIterator __res, _Allocator &__alloc)
{
    _ForwardIterator __start = __res;

    try {
        for (; __first != __last; ++__first, ++__res)
            __alloc.construct (__alloc.address (*__res), *__first);
    }
    catch (...) {
        for (; __start != __res; ++__start)
            __alloc.destroy (__alloc.address (*__start));
        throw;
    }

    return __res;
}


// 20.4.4.2
template <class _ForwardIterator, class _TypeT>
inline void
uninitialized_fill (_ForwardIterator __first, _ForwardIterator __last,
                    const _TypeT& __x)
{
    const _ForwardIterator __start = __first;

    try {
        for (; __first != __last; ++__first)
            __rw::__rw_construct (&*__first, __x);
    }
    catch (...) {
        __rw::__rw_destroy (__start, __first);
        throw;
    }
}


// 20.4.4.3
template <class _ForwardIterator, class _Size, class _TypeT>
inline void
uninitialized_fill_n (_ForwardIterator __first, _Size __n, const _TypeT& __x)
{
    const _ForwardIterator __start = __first;

    try {
        for (; __n; --__n, ++__first)
            __rw::__rw_construct (&*__first, __x);
    }
    catch (...) {
        __rw::__rw_destroy (__start, __first);
        throw;
    }
}


// extension
template <class _ForwardIter, class _Size, class _TypeT, class _Allocator>
inline void
uninitialized_fill_n (_ForwardIter __first, _Size __n,
                      const _TypeT& __x, _Allocator& __alloc)
{
    _ForwardIter __start = __first;

    try {
        for (; __n; --__n, ++__first)
            __alloc.construct (__alloc.address (*__first), __x);
    }
    catch (...) {
        for (; __start != __first; ++__start)
            __alloc.destroy (__alloc.address (*__start));
        throw;
    }
}

}   // namespace std


#endif   // _RWSTD_RW_SPECIALIZED_H_INCLUDED
