/***************************************************************************
 *
 * _iterbase.h - Definitions of iterator primitives
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
 * Copyright 1994-2007 Rogue Wave Software, Inc.
 * 
 **************************************************************************/

#ifndef _RWSTD_RW_ITERBASE_H_INCLUDED
#define _RWSTD_RW_ITERBASE_H_INCLUDED

#ifndef _RWSTD_RW_DEFS_H_INCLUDED
#  include <rw/_defs.h>
#endif   // _RWSTD_RW_DEFS_H_INCLUDED


namespace std { 


// 24.3.1 - Iterator traits
template <class _Iterator>
struct iterator_traits
{
    typedef typename _Iterator::value_type         value_type;
    typedef typename _Iterator::difference_type    difference_type;
    typedef typename _Iterator::pointer            pointer;
    typedef typename _Iterator::reference          reference;
    typedef typename _Iterator::iterator_category iterator_category;
};


// 24.3.3 - Standard iterator tags
struct input_iterator_tag { };
struct output_iterator_tag { };
struct forward_iterator_tag : public input_iterator_tag { };
struct bidirectional_iterator_tag : public forward_iterator_tag { };
struct random_access_iterator_tag : public bidirectional_iterator_tag { };


template <class _TypeT>
struct iterator_traits<_TypeT*>
{
    typedef _TypeT                     value_type;
    typedef _RWSTD_PTRDIFF_T           difference_type;
    typedef value_type*                pointer;
    typedef value_type&                reference;
    typedef random_access_iterator_tag iterator_category;
};


template <class _TypeT>
struct iterator_traits<const _TypeT*>
{
    typedef _TypeT                     value_type;
    typedef _RWSTD_PTRDIFF_T           difference_type;
    typedef const value_type*          pointer;
    typedef const value_type&          reference;
    typedef random_access_iterator_tag iterator_category;
};


// 24.3.2 - Basic iterator
template <class _Category, class _TypeT,  
          class _Distance = _RWSTD_PTRDIFF_T,
          class _Pointer = _TypeT*,
          class _Reference = _TypeT&>
struct iterator
{
    typedef _TypeT     value_type;
    typedef _Distance  difference_type;
    typedef _Pointer   pointer;
    typedef _Reference reference;
    typedef _Category  iterator_category;
};


// returns the category of an iterator
template <class _TypeT>
inline random_access_iterator_tag __iterator_category (const _TypeT*)
{
    return random_access_iterator_tag ();
}


template <class _Category, class _TypeT, class _Distance, 
          class _Pointer, class _Reference> 
inline _Category
__iterator_category (const iterator<_Category, _TypeT,
                                    _Distance, _Pointer, _Reference>&)
{
    typedef typename iterator<_Category, _TypeT, _Distance, _TypeT*,
                               _TypeT&>::iterator_category _IterCategory;

    return _IterCategory ();
}


template <class _Tag>
inline bool __is_input_iterator (_Tag)
{
    return false;
}


template <class _Tag>
inline bool __is_bidirectional_iterator (_Tag)
{
    return false;
}


template <class _Tag>
inline bool __is_random_access_iterator (_Tag)
{
    return false;
}


template <> 
inline bool __is_input_iterator (input_iterator_tag)
{
    return true;
}


template <> 
inline bool __is_bidirectional_iterator (bidirectional_iterator_tag)
{
    return true;
}


template <> 
inline bool __is_bidirectional_iterator (random_access_iterator_tag)
{
    return true;
}


template <> 
inline bool __is_random_access_iterator (random_access_iterator_tag)
{
    return true;
}


template <class _Iterator>
inline typename iterator_traits<_Iterator>::value_type*
__value_type (const _Iterator*)
{ 
    return 0;
}


template <class _Iterator>
inline typename iterator_traits<_Iterator>::difference_type*
__distance_type (_Iterator)
{ 
    return 0;
}


// 24.3.4 - Iterator operations

template <class _InputIterator, class _Distance>
inline void __advance (_InputIterator &__it, _Distance __n, input_iterator_tag)
{
    _RWSTD_ASSERT (__n == 0 || __n > 0);

    while (__n > 0) {
        --__n;
        ++__it;
    }
}


template <class _ForwardIterator, class _Distance>
inline void __advance (_ForwardIterator &__it, _Distance __n,
                       forward_iterator_tag)
{
    __advance (__it, __n, input_iterator_tag ());
}


template <class _BidirectionalIterator, class _Distance>
inline void __advance (_BidirectionalIterator &__it, _Distance __n, 
                       bidirectional_iterator_tag)
{
    if (__n > 0)
        __advance (__it, __n, input_iterator_tag ());
    else
        while (__n) {
            ++__n;
            --__it;
        }
}


template <class _RandomAccessIterator, class _Distance>
inline void __advance (_RandomAccessIterator& __it, _Distance __n, 
                       random_access_iterator_tag)
{
    __it += __n;
}


// 24.3.4, p2
template <class _InputIterator, class _Distance>
inline void advance (_InputIterator& __it, _Distance __n)
{
    __advance (__it, __n, _RWSTD_ITERATOR_CATEGORY (_InputIterator, __it));
}


template <class _InputIterator, class _Distance>
inline void __distance (const _InputIterator &__first,
                        const _InputIterator &__last,
                        _Distance            &__n,
                        input_iterator_tag)
{
    for (_InputIterator __it = __first; !(__it == __last); ++__it)
        ++__n;
}


template <class _ForwardIterator, class _Distance>
inline void __distance (const _ForwardIterator &__first,
                        const _ForwardIterator &__last,
                        _Distance              &__n,
                        forward_iterator_tag)
{
    __distance (__first, __last, __n, input_iterator_tag ());
}

template <class _BidirectionalIterator, class _Distance>
inline void __distance (const _BidirectionalIterator &__first,
                        const _BidirectionalIterator &__last, 
                        _Distance                    &__n,
                        bidirectional_iterator_tag)
{
    __distance (__first, __last, __n, input_iterator_tag ());
}


template <class _RandomAccessIterator, class _Distance>
inline void __distance (const _RandomAccessIterator &__first,
                        const _RandomAccessIterator &__last, 
                        _Distance                   &__n,
                        random_access_iterator_tag)
{
    __n = __last - __first;
}


// 24.3.4, p4
template <class _ForwardIterator>
inline typename iterator_traits<_ForwardIterator>::difference_type
distance (_ForwardIterator __first, _ForwardIterator __last)
{
    typename iterator_traits<_ForwardIterator>::difference_type __n = 0;

    __distance (__first, __last, __n,
                _RWSTD_ITERATOR_CATEGORY (_ForwardIterator, __first));

    return __n;
}


}   // namespace std


namespace __rw { 

// __rw_distance: Same purpose as 3-parameter distance function, but
// with return value.

template <class _ForwardIterator, class _Distance>
inline _Distance
__rw_distance (const _ForwardIterator &__first,
               const _ForwardIterator &__last,
               _Distance               __n)
{
    std::__distance (__first, __last, __n,
                      _RWSTD_ITERATOR_CATEGORY (_ForwardIterator, __first));
    return __n;
}


}   // namespace __rw


#ifndef _RWSTD_NO_DEBUG_ITER

namespace __rw { 

// __rw_debug_iter - iterator adapter with debugging support
// _Iterator is either iterator or const_iterator; if the latter,
// _MutableIterator should be iterator to allow for implicit
// conversions from non-const (mutable) to const_iterator objects


template <class _Container, class _Iterator, class _MutableIterator>
class __rw_debug_iter
{
    typedef _Container                                container_type;
    typedef _Iterator                                 iterator_type;
    typedef std::iterator_traits<iterator_type>       traits_type;

public:

    typedef typename traits_type::value_type          value_type;
    typedef typename traits_type::difference_type     difference_type;
    typedef typename traits_type::reference           reference;
    typedef typename traits_type::pointer             pointer;
    typedef typename traits_type::iterator_category   iterator_category;

    __rw_debug_iter (): _C_cont (0) { }

    __rw_debug_iter (const container_type &__cont, const iterator_type &__it)
        : _C_iter (__it), _C_cont (&__cont) { }

    __rw_debug_iter (const __rw_debug_iter &__rhs)
        : _C_iter (__rhs._C_iter), _C_cont (__rhs._C_cont) { }

    // Only a mutable wrapped iterator participates in this conversion.
    template <class _MutableIter>
    __rw_debug_iter (const __rw_debug_iter<container_type, _MutableIterator,
                                          _MutableIter> &__rhs)
        : _C_iter (__rhs._C_iter), _C_cont (__rhs._C_cont) { }

    __rw_debug_iter& operator= (const __rw_debug_iter &__rhs) {
        _C_iter = __rhs._C_iter;
        _C_cont = __rhs._C_cont;
        return *this;
    }

    reference operator* () const {
        _RWSTD_ASSERT (_C_is_dereferenceable ());
        return *_C_iter;
    }

    reference operator[] (difference_type __n) const {
        _RWSTD_ASSERT ((*this + __n)._C_is_dereferenceable ());
        return _C_iter [__n];
    }

    _RWSTD_OPERATOR_ARROW (pointer operator-> () const);

    __rw_debug_iter& operator++ () {
        _RWSTD_ASSERT (!_C_is_end ());
        return ++_C_iter, *this;
    }

    __rw_debug_iter& operator-- () {
        _RWSTD_ASSERT (!_C_is_begin ());
        return --_C_iter, *this;
    }

    __rw_debug_iter operator++ (int) {
        __rw_debug_iter __tmp = *this;
        return ++*this, __tmp;
    }

    __rw_debug_iter operator-- (int) {
        __rw_debug_iter __tmp = *this;
        return --*this, __tmp;
    }

    __rw_debug_iter& operator+= (difference_type __n) {
        _C_iter += __n;
        _RWSTD_ASSERT (   !(_C_iter < _C_cont->begin ()._C_iter)
                       && !(_C_cont->end ()._C_iter < _C_iter));
        return *this;
    }

    __rw_debug_iter& operator-= (difference_type __n) {
        _C_iter -= __n;
        _RWSTD_ASSERT (   !(_C_iter < _C_cont->begin ()._C_iter)
                       && !(_C_cont->end ()._C_iter < _C_iter));
        return *this;
    }

    __rw_debug_iter operator+ (difference_type __n) const {
        return __rw_debug_iter (*this) += __n;
    }

    __rw_debug_iter operator- (difference_type __n) const {
        return __rw_debug_iter (*this) -= __n;
    }

    bool _C_is_begin () const {
        return _C_cont && _C_cont->begin () == *this;
    }

    bool _C_is_end () const {
        return _C_cont && _C_cont->end () == *this;
    }

    bool _C_is_dereferenceable () const {
        return !_C_is_end ();
    }

    bool _C_valid_range (const __rw_debug_iter &__it) const {
        return _C_cont && _C_cont == __it._C_cont;
    }

    const iterator_type& base () const {
        return _C_iter;
    }

    iterator_type& base () {
        return _C_iter;
    }

    // operators are templatized to assure const/non-const symmetry

    template <class _Iter>
    difference_type
    operator- (const __rw_debug_iter<container_type, _Iter,
                                     _MutableIterator> &__rhs) const {
        _RWSTD_ASSERT (_C_cont && _C_cont == __rhs._C_cont);
        return _C_iter - __rhs._C_iter;
    }

    template <class _Iter>
    bool
    operator== (const __rw_debug_iter<container_type, _Iter,
                                      _MutableIterator> &__rhs) const {
        return _C_iter == __rhs._C_iter;
    }
    
    template <class _Iter>
    bool
    operator< (const __rw_debug_iter<container_type, _Iter,
                                     _MutableIterator> &__rhs) const {
        return _C_iter < __rhs._C_iter;
    }

    template <class _Iter>
    bool
    operator!= (const __rw_debug_iter<container_type, _Iter,
                                      _MutableIterator> &__rhs) const {
        return !(_C_iter == __rhs._C_iter);
    }

    template <class _Iter>
    bool
    operator<= (const __rw_debug_iter<container_type, _Iter,
                                      _MutableIterator> &__rhs) const {
        return !(__rhs._C_iter < _C_iter);
    }

    template <class _Iter>
    bool
    operator> (const __rw_debug_iter<container_type, _Iter,
                                     _MutableIterator> &__rhs) const {
        return __rhs._C_iter < _C_iter;
    }

    template <class _Iter>
    bool
    operator>= (const __rw_debug_iter<container_type, _Iter,
                                      _MutableIterator> &__rhs) const {
        return !(_C_iter < __rhs._C_iter);
    }

    iterator_type         _C_iter;   // wrapped iterator
    const container_type *_C_cont;   // associated container
};


}   // namespace __rw


namespace std { 


template <class _Cont, class _Iter, class _MutIter>
inline __rw::__rw_debug_iter<_Cont, _Iter, _MutIter> 
operator+ (typename _Cont::difference_type                     __n,
           const __rw::__rw_debug_iter<_Cont, _Iter, _MutIter> &__x)
{
    return __x + __n;
}

}   // namespace std


namespace __rw { 


#define _RWSTD_DEBUG_ITER(cont, it, mutit) __rw_debug_iter< cont, it, mutit >


template <class _Cont, class _Iter, class _MutIter>
inline bool
__rw_valid_range (const _RWSTD_DEBUG_ITER(_Cont, _Iter, _MutIter) &__first,
                  const _RWSTD_DEBUG_ITER(_Cont, _Iter, _MutIter) &__last)
{
    return __first._C_cont && __first._C_cont == __last._C_cont;
}


template <class _Iterator>
inline bool
__rw_valid_range (const _Iterator &, const _Iterator &)
{
    return true;
}


template <class _Cont, class _Iter, class _MutIter>
inline bool
__rw_in_range (const _RWSTD_DEBUG_ITER(_Cont, _Iter, _MutIter) &__it,
               const _RWSTD_DEBUG_ITER(_Cont, _Iter, _MutIter) &__first,
               const _RWSTD_DEBUG_ITER(_Cont, _Iter, _MutIter) &__last)
{
    return    __rw_valid_range (__first, __it)
           && __rw_valid_range (__it, __last);
}


template <class _Iterator>
inline bool
__rw_in_range (const _Iterator&, const _Iterator&, const _Iterator&)
{
    return true;
}


template <class _Cont, class _Iter, class _MutIter>
inline bool
__rw_dereferenceable (const _RWSTD_DEBUG_ITER(_Cont, _Iter, _MutIter) &__it)
{
    return __it._C_is_dereferenceable ();
}


template <class _Iterator>
inline bool
__rw_dereferenceable (const _Iterator&)
{
    return true;
}


template <class _TypeT>
inline bool
__rw_dereferenceable (const _TypeT *__ptr)
{
    return 0 != __ptr;
}

}   // namespace __rw

#undef _RWSTD_DEBUG_ITER

#endif   // _RWSTD_NO_DEBUG_ITER


#endif   // _RWSTD_RW_ITERBASE_H_INCLUDED
