/***************************************************************************
 *
 * _iterator.h - Iterator declarations for the Standard Library
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
 * Copyright 1994-2006 Rogue Wave Software.
 * 
 **************************************************************************/

#ifndef _RWSTD_RW_ITERATOR_H_INCLUDED
#define _RWSTD_RW_ITERATOR_H_INCLUDED

#ifndef _RWSTD_RW_ITERBASE_H_INCLUDED
#  include <rw/_iterbase.h>
#endif   // _RWSTD_RW_ITERBASE_H_INCLUDED


namespace std { 


// Reverse iterator.     

//
//  Macros for reverse iterators to accomodate non-standard compilers
//
#define _RWSTD_ITER_TEMPLATE template <class _Iterator>
#define _RWSTD_ITER_ID(i)    i <_Iterator>
#define _RWSTD_ITER_DIFF_TYPE(i, ignore)                             \
          typename iterator_traits<_Iterator>::difference_type


template <class _Iterator>
class reverse_iterator
    : public iterator<typename iterator_traits<_Iterator>::iterator_category, 
                      typename iterator_traits<_Iterator>::value_type, 
                      typename iterator_traits<_Iterator>::difference_type,
                      typename iterator_traits<_Iterator>::pointer, 
                      typename iterator_traits<_Iterator>::reference>
{
    typedef iterator_traits<_Iterator>             traits_type;
public:
    typedef typename traits_type::difference_type difference_type;
    typedef typename traits_type::value_type       value_type;
    typedef typename traits_type::pointer          pointer;
    typedef typename traits_type::reference        reference;

    typedef _Iterator iterator_type;

    reverse_iterator () { }

    // 24.4.1.3.1, p1
    explicit reverse_iterator (iterator_type __rhs) : current (__rhs) { }
 

    // 24.4.1.3.1, p2
    template <class _TypeU>
    reverse_iterator (const reverse_iterator<_TypeU>& __rhs)
        : current (__rhs.base ()) { }


    // 24.4.1.3.2, p1
    iterator_type base () const {
        return current;
    }

    // 24.4.1.3.3, p1
    reference operator* () const {
        iterator_type __tmp (base ());
        return *--__tmp;
    }

    // 24.4.1.3.4, p1
    pointer operator->() const { return &**this; };

    // 24.4.1.3.5, p1
    reverse_iterator& operator++ () {
        return --current, *this;
    }

    // 24.4.1.3.5, p3
    reverse_iterator operator++ (int) {
        reverse_iterator __tmp (*this);
        return ++*this, __tmp;
    }

    // 24.4.1.3.6, p1
    reverse_iterator& operator-- () {
        return ++current, *this;
    }

    // 24.4.1.3.6, p3
    reverse_iterator operator-- (int) {
        reverse_iterator __tmp (*this);
        return --*this, __tmp;
    }

    // 24.4.1.3.8, p1
    reverse_iterator& operator+= (difference_type __n) {
        return current -= __n, *this;
    }

    // 24.4.1.3.10, p1
    reverse_iterator& operator-= (difference_type __n) {
        return *this += -__n;
    }

    // 24.4.1.3.7, p1
    reverse_iterator operator+ (difference_type __n) const {
        return reverse_iterator (*this) += __n;
    }

    // 24.4.1.3.9, p1
    reverse_iterator operator- (difference_type __n) const {
        return reverse_iterator (*this) -= __n;
    }

    // 24.4.1.3.11, p1
    reference operator[] (difference_type __n) const {
        return *(*this + __n);
    }

protected:

    iterator_type current;
};


// 24.4.1.3.12
_RWSTD_ITER_TEMPLATE
inline bool operator== (const _RWSTD_ITER_ID (reverse_iterator)& __x,
                        const _RWSTD_ITER_ID (reverse_iterator)& __y)
{
    return __x.base () == __y.base ();
}


// 24.4.1.3.13
_RWSTD_ITER_TEMPLATE
inline bool operator< (const _RWSTD_ITER_ID (reverse_iterator)& __x,
                       const _RWSTD_ITER_ID (reverse_iterator)& __y)
{
    return __y.base() < __x.base();
}


// 24.4.1.3.14
_RWSTD_ITER_TEMPLATE
inline bool operator!= (const _RWSTD_ITER_ID (reverse_iterator)& __x, 
                        const _RWSTD_ITER_ID (reverse_iterator)& __y)
{
    return !(__x == __y);
}


// 24.4.1.3.15
_RWSTD_ITER_TEMPLATE
inline bool operator> (const _RWSTD_ITER_ID (reverse_iterator)& __x, 
                       const _RWSTD_ITER_ID (reverse_iterator)& __y)
{
    return __y < __x;
}


// 24.4.1.3.16
_RWSTD_ITER_TEMPLATE
inline bool operator>= (const _RWSTD_ITER_ID (reverse_iterator)& __x, 
                        const _RWSTD_ITER_ID (reverse_iterator)& __y)
{
    return !(__x < __y);
}


// 24.4.1.3.17
_RWSTD_ITER_TEMPLATE
inline bool operator<= (const _RWSTD_ITER_ID (reverse_iterator)& __x,
                        const _RWSTD_ITER_ID (reverse_iterator)& __y)
{
    return !(__y < __x);
}


// 24.4.1.3.18
_RWSTD_ITER_TEMPLATE
inline _RWSTD_ITER_DIFF_TYPE (_Iterator, _Distance)
operator- (const _RWSTD_ITER_ID (reverse_iterator)& __x, 
           const _RWSTD_ITER_ID (reverse_iterator)& __y)
{
    return __y.base () - __x.base ();
}


// 24.4.1.3.19
_RWSTD_ITER_TEMPLATE
inline _RWSTD_ITER_ID (reverse_iterator)
operator+ (_RWSTD_ITER_DIFF_TYPE (_Iterator, _Distance) __n, 
           const _RWSTD_ITER_ID (reverse_iterator)&     __x)
{
    return __x + __n;
}

#undef _RWSTD_ITER_DIFF_TYPE


#define _RWSTD_INSERT_ITERATOR_BASE(ignore)       \
          iterator<output_iterator_tag, void, void, void, void>


template <class _Container>
class back_insert_iterator: public _RWSTD_INSERT_ITERATOR_BASE (_Container)
{
public:
    typedef _Container container_type;

    explicit back_insert_iterator (container_type& __rhs)
        : container (&__rhs) { }

    back_insert_iterator&
    operator= (typename container_type::const_reference __x) {
        return container->push_back (__x), *this;
    }

    back_insert_iterator& operator*  () {
        return *this;
    }

    back_insert_iterator& operator++ () {
        return *this;
    }

    back_insert_iterator operator++ (int) {
        return *this;
    }

protected:

    container_type* container;
};


template <class _Container>
inline back_insert_iterator<_Container> back_inserter (_Container& __x)
{
    return back_insert_iterator<_Container>(__x);
}


template <class _Container>
class front_insert_iterator: public _RWSTD_INSERT_ITERATOR_BASE (_Container)
{
public:
    typedef _Container container_type;

    explicit front_insert_iterator (container_type& __rhs)
        : container (&__rhs) { }

    front_insert_iterator&
    operator= (typename container_type::const_reference __x) { 
        return container->push_front (__x), *this;
    }

    front_insert_iterator& operator* () {
        return *this;
    }

    front_insert_iterator& operator++ () {
        return *this;
    }

    front_insert_iterator operator++ (int) {
        return *this;
    }

protected:

    container_type* container;
};


template <class _Container>
inline front_insert_iterator<_Container> front_inserter (_Container& __x)
{
    return front_insert_iterator<_Container>(__x);
}


template <class _Container>
class insert_iterator: public _RWSTD_INSERT_ITERATOR_BASE (_Container)
{
public:
    typedef _Container container_type;

    insert_iterator (container_type                    &__x,
                     typename container_type::iterator __it)
      : iter (__it), container (&__x) { }

    insert_iterator&
    operator= (typename container_type::const_reference __x) { 
        iter = container->insert (iter, __x);
        return ++iter, *this;
    }

    insert_iterator& operator* () {
        return *this;
    }

    insert_iterator& operator++ () {
        return *this;
    }

    insert_iterator& operator++ (int) {
        return *this;
    }

protected:

    typename container_type::iterator iter;
    container_type*                    container;
};


template <class _Container, class _Iterator>
inline insert_iterator<_Container> inserter (_Container& __x, _Iterator __it)
{
    typedef typename _Container::iterator _Iter;

    return insert_iterator<_Container> (__x, _Iter (__it));
}


}   // namespace std

#endif   // _RWSTD_RW_ITERATOR_H_INCLUDED
