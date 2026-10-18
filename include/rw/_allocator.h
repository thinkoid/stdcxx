// -*- C++ -*-
/***************************************************************************
 *
 * _allocator.h - definition of the class template allocator
 *
 * This is an internal header file used to implement the C++ Standard
 * Library. It should never be #included directly by a program.
 *
 * $Id$
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

#ifndef _RWSTD_RW_ALLOCATOR_H_INCLUDED
#define _RWSTD_RW_ALLOCATOR_H_INCLUDED

#ifndef _RWSTD_SPECIALIZED_H_INCLUDED
#  include <rw/_specialized.h>
#endif   // _RWSTD_SPECIALIZED_H_INCLUDED


namespace __rw {

// [de]allocate storage (in bytes)
_RWSTD_EXPORT void*
__rw_allocate (_RWSTD_SIZE_T, int = 0);

_RWSTD_EXPORT void
__rw_deallocate (void*, _RWSTD_SIZE_T, int = 0) _THROWS (());

}   // namespace __rw


namespace std {


template <class _TypeT> class
allocator;


template <>
class allocator<void>
{
public:
    typedef void              value_type;
    typedef value_type*       pointer;
    typedef const value_type* const_pointer;

    template <class _TypeU>
    struct rebind {
        typedef allocator<_TypeU> other;
    };
};


template <class _TypeT>
class allocator
{
public:
    typedef _RWSTD_SIZE_T       size_type;
    typedef _RWSTD_PTRDIFF_T    difference_type;
    typedef _TypeT              value_type;
    typedef value_type*         pointer;
    typedef const value_type*   const_pointer;
    typedef value_type&         reference;
    typedef const value_type&   const_reference;

    allocator () _THROWS (()) { }

    allocator (const allocator &__rhs) _THROWS (()) {
        // working around an HP aCC warning 431
        _RWSTD_UNUSED (__rhs);
    }

    template <class _TypeU>
    struct rebind {
        typedef allocator<_TypeU> other;
    };

    template <class _TypeU>
    allocator (const allocator<_TypeU>&) _THROWS (()) { }

    template <class _TypeU>
    allocator&
    operator= (const allocator<_TypeU>&) _THROWS (()) { 
        return *this; 
    }

    pointer address (reference __x) const {
        // lwg issue 350
        return reinterpret_cast< pointer >(&reinterpret_cast< char& >(__x));
    }

    const_pointer address (const_reference __x) const {
        // lwg issue 350
        return reinterpret_cast< const_pointer >(
            &reinterpret_cast< const char& >(__x));
    }

    pointer allocate (size_type __n, allocator<void>::const_pointer = 0) {
        return static_cast< pointer >(
            __rw::__rw_allocate (__n * sizeof (value_type)));
    }

    void deallocate (pointer __p, size_type __n) _THROWS (()) {
        __rw::__rw_deallocate (__p, __n);
    }

    // 20.4.1.1, p11 - the largest N for which allocate (N) might succeed
    size_type max_size () const _THROWS (()) { 
        return size_type (_RWSTD_SIZE_MAX) / sizeof (value_type)
            ? size_type (size_type (_RWSTD_SIZE_MAX) / sizeof (value_type))
            : size_type (1);
    }

    void construct (pointer __p, const_reference __val) {
        __rw::__rw_construct (__p, __val);
    }
    
    // declared as nothrow since the behavior of programs that
    // instantiate library templates on types whose dtors throw
    // is undefined 
    void destroy (pointer __p) _RWSTD_ATTRIBUTE_NOTHROW {
        _RWSTD_ASSERT (0 != __p);
        __p->~_TypeT ();
    }
};


template <class _TypeT, class _TypeU>
inline bool
operator== (const allocator<_TypeT>&, const allocator<_TypeU>&)  _THROWS (())
{
    return true;
}


template <class _TypeT, class _TypeU>
inline bool
operator!= (const allocator<_TypeT>& __x,
            const allocator<_TypeU>& __y)  _THROWS (())
{
    return !(__x == __y);
}


}   // namespace std


#endif   // _RWSTD_RW_ALLOCATOR_H_INCLUDED
