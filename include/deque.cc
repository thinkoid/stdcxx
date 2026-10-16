/***************************************************************************
 *
 * deque.cc - Non-iniline definitions for the Standard Library deque class
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

_RWSTD_NAMESPACE (std) { 


template <class _TypeT, class _Allocator>
deque<_TypeT, _Allocator>&
deque<_TypeT, _Allocator>::
operator= (const deque &__rhs)
{
    _RWSTD_ASSERT (_C_is_valid ());
    _RWSTD_ASSERT (__rhs._C_is_valid ());

    const size_type __size = size ();

    if (__size < __rhs.size ()) {
        const const_iterator __mid = __rhs.begin () + __size;

        _STD::copy (__mid, __rhs.end (),
                    _STD::inserter (*this, _STD::copy (__rhs.begin (), __mid,
                                                       begin ())));
    }
    else
        erase (_STD::copy (__rhs.begin (), __rhs.end (), begin ()), end ());

    return *this;
}


template <class _TypeT, class _Allocator>
_TYPENAME deque<_TypeT, _Allocator>::iterator 
deque<_TypeT, _Allocator>::
insert (iterator __it, const_reference __x)
{
    _RWSTD_ASSERT_RANGE (begin (), __it);
    _RWSTD_ASSERT (_C_is_valid ());

    if (__it == begin ()) {
        push_front (__x);
        return begin ();
    }

    if (__it == end ()) {
        push_back (__x);
        __it = end () - 1;
        return __it;
    }

    const size_type __inx = _DISTANCE (begin (), __it, size_type);

    _RWSTD_ASSERT (__inx < size ());

    if (__inx < size () - __inx) {
        push_front (front ());
        _STD::copy (begin () + 2, begin () + (__inx + 1), begin () + 1);
    }
    else {
        push_back (back ());

        _STD::copy_backward (begin () + __inx, end () - 2, end () - 1);
    }

    __it = begin () + __inx;

    *__it = __x;

    return __it;
}


template <class _TypeT, class _Allocator>
_TYPENAME deque<_TypeT, _Allocator>::iterator 
deque<_TypeT, _Allocator>::
erase (iterator __it)
{
    // `it' must be a dereferenceable iterator into `*this'

    _RWSTD_ASSERT_RANGE (begin (), __it);
    _RWSTD_ASSERT (!(__it == end ()));
    _RWSTD_ASSERT (_C_is_valid (0 /* valid and non-empty */));

    const iterator __next = ++iterator (__it);

    if (__it - begin () < end () - __it) {

        _STD::copy_backward (begin (), __it, __next);

        pop_front ();

        return empty () ? end () : __next;
    }

    _STD::copy (__next, end (), __it);

    pop_back ();

    return __it;
}
    

template <class _TypeT, class _Allocator>
_TYPENAME deque<_TypeT, _Allocator>::iterator 
deque<_TypeT, _Allocator>::
erase (iterator __first, iterator __last)
{
    // (`first', `last'] must be a valid range into `*this'

    _RWSTD_ASSERT_RANGE (__first, __last);
    _RWSTD_ASSERT_RANGE (begin (), __first);
    _RWSTD_ASSERT (_C_is_valid ());

    size_type __n = _DISTANCE (__first, __last, size_type);

    _RWSTD_ASSERT (__n <= size ());

    if (__first - begin () < end () - __last) {

        _STD::copy_backward (begin (), __first, __last);

        while (__n-- != 0)
            pop_front ();

        return __last;
    }

    _STD::copy (__last, end (), __first);

    while (__n-- != 0)
        pop_back ();

    return __first;
}


template <class _TypeT, class _Allocator>
void deque<_TypeT, _Allocator>::
swap (deque<_TypeT, _Allocator> &__rhs)
{
    _RWSTD_ASSERT (_C_is_valid ());
    _RWSTD_ASSERT (__rhs._C_is_valid ());

    if (get_allocator () == __rhs.get_allocator ()) {

        _STD::swap (_C_beg,       __rhs._C_beg);
        _STD::swap (_C_end,       __rhs._C_end);
        _STD::swap (_C_nodes,     __rhs._C_nodes);
        _STD::swap (_C_node_size, __rhs._C_node_size);

        if (pointer() == _C_beg._C_cur) {
            _RWSTD_ASSERT (pointer() == _C_end._C_cur);
            _RWSTD_ASSERT (   _C_beg._C_node == &__rhs._C_end._C_cur
                           && _C_end._C_node == &__rhs._C_end._C_cur);

            _C_beg._C_node =
            _C_end._C_node = &_C_end._C_cur;
        }
        else {
            _RWSTD_ASSERT (pointer() != _C_end._C_cur);
        }

        if (pointer() == __rhs._C_beg._C_cur) {
            _RWSTD_ASSERT (pointer() == __rhs._C_end._C_cur);
            _RWSTD_ASSERT (   __rhs._C_beg._C_node == &_C_end._C_cur
                           && __rhs._C_end._C_node == &_C_end._C_cur);

            __rhs._C_beg._C_node =
            __rhs._C_end._C_node = &__rhs._C_end._C_cur;
        }
        else {
            _RWSTD_ASSERT (pointer() != __rhs._C_end._C_cur);
        }
    }
    else {
        deque __tmp (*this);
        *this = __rhs;
        __rhs = __tmp;
    }
}


template <class _TypeT, class _Allocator>
void deque<_TypeT, _Allocator>::
_C_push (bool __at_back, const_reference __x)
{
    _RWSTD_ASSERT (_C_is_valid ());

    const size_type __bufsize = _C_bufsize ();

    _C_value_alloc_type __alloc (*this);

    if (_C_node_pointer () == _C_nodes) {

        // first element: allocate the vector and its buffers, point
        // both iterators at the middle of the middle buffer and
        // construct the element on the side the caller grows
        _C_grow (__at_back, _C_vecsize (1));
        _C_reset ();

        const pointer __where = __at_back ? _C_end._C_cur
                                          : _C_beg._C_cur - 1;

        __alloc.construct (__where, __x);

        return;
    }

    if (__at_back ? _C_end._C_node == _C_nodes + _C_node_size - 1
                  : _C_beg._C_node == _C_nodes)
        _C_grow (__at_back, _C_vecsize (size () + 1));

    // the node next to the end being grown owns a buffer with no
    // elements in it: construct at its near edge and point the
    // iterator at the element so that the caller's increment or
    // decrement lands on it
    if (__at_back) {
        const _C_node_pointer __node = _C_end._C_node + 1;

        __alloc.construct (*__node, __x);

        _C_end = _C_deque_iter (*__node, __node);
    }
    else {
        const _C_node_pointer __node = _C_beg._C_node - 1;

        __alloc.construct (*__node + (__bufsize - 1), __x);

        _C_beg = _C_deque_iter (*__node + __bufsize, __node);
    }
}


// grows the vector of nodes to `__nnodes' nodes, with a buffer for
// each node added after the existing ones (at the back) or before
// them (at the front), and the null guard after the last node that
// an iterator stepping off the last element reads; a failed
// allocation frees what this call allocated and leaves the container
// as it was
template <class _TypeT, class _Allocator>
void deque<_TypeT, _Allocator>::
_C_grow (bool __at_back, size_type __nnodes)
{
    _RWSTD_ASSERT (_C_node_size < __nnodes);

    const size_type __bufsize = _C_bufsize ();
    const size_type __nnew    = __nnodes - _C_node_size;

    _C_value_alloc_type __alloc (*this);
    _C_node_alloc_type  __node_alloc (*this);

    const _C_node_pointer __nvec =
        __node_alloc.allocate (__nnodes + 1, _C_nodes);

    const _C_node_pointer __added = __nvec + (__at_back ? _C_node_size : 0);
    const _C_node_pointer __kept  = __nvec + (__at_back ? 0 : __nnew);

    size_type __i = 0;

    _TRY {
        for ( ; __i != __nnew; ++__i)
            __added [__i] = __alloc.allocate (__bufsize, pointer ());
    }
    _CATCH (...) {
        while (__i)
            __alloc.deallocate (__added [--__i], __bufsize);

        __node_alloc.deallocate (__nvec, __nnodes + 1);
        _RETHROW;
    }

    // nothing below throws: move the existing nodes over, point
    // the iterators at them and free the old vector
    if (_C_node_pointer () != _C_nodes) {
        _STD::copy (_C_nodes, _C_nodes + _C_node_size, __kept);

        _C_beg._C_node = __kept + (_C_beg._C_node - _C_nodes);
        _C_end._C_node = __kept + (_C_end._C_node - _C_nodes);

        __node_alloc.deallocate (_C_nodes, _C_node_size + 1);
    }

    __nvec [__nnodes] = pointer ();

    _C_nodes     = __nvec;
    _C_node_size = __nnodes;
}


// frees every buffer and the vector of nodes of an empty container
template <class _TypeT, class _Allocator>
void deque<_TypeT, _Allocator>::
_C_dealloc ()
{
    _RWSTD_ASSERT (empty ());

    if (_C_node_pointer () == _C_nodes)
        return;

    const size_type __bufsize = _C_bufsize ();

    _C_value_alloc_type __alloc (*this);

    for (size_type __i = 0; __i != _C_node_size; ++__i)
        __alloc.deallocate (_C_nodes [__i], __bufsize);

    _C_node_alloc_type (*this).deallocate (_C_nodes, _C_node_size + 1);

    _C_init ();
}


template <class _TypeT, class _Allocator>
void deque<_TypeT, _Allocator>::
_C_assign_n (size_type __n, const_reference __x)
{
    _RWSTD_ASSERT (_C_is_valid ());

    // assign over existing elements for better efficiency
    // and exception safety

    // complexity: linear in n
    // exception safety:
    // -- nothrow if size() >= n, and value_type's assignment operator
    //    and iterator operations do not throw
    // -- basic otherwise

    const iterator __end = end ();

    // avoid using the name __i or __it below so as not to trigger
    // a (bogus) gcc 2.95.2 -Wshadow warning: declaration of `__i'
    // shadows previous local
    for (iterator __ix = begin (); !(__ix == __end); ++__ix, --__n) {
        if (size_type () == __n) {
            erase (__ix, __end);
            return;
        }
        *__ix = __x;
    }

    insert (__end, __n, __x);
}


template <class _TypeT, class _Allocator>
void deque<_TypeT, _Allocator>::
_C_insert_n (const iterator &__it, size_type __n, const_reference __x)
{
    _RWSTD_ASSERT_RANGE (begin (), __it);
    _RWSTD_ASSERT (_C_is_valid ());

    const size_type __inx = _DISTANCE (begin (), __it, size_type);
    const size_type __rem = size () - __inx;

    _RWSTD_ASSERT (__inx <= size ());

    if (__inx < __rem) {

        // inserting `x' into the first half of the container

        if (__inx < __n) {
            
            for (size_type __i = __n - __inx; __i; --__i)
                push_front (__x);

            for (size_type __j = __inx; __j; --__j)
                push_front (*(begin () + (__n - 1)));

            _STD::fill (begin () + __n, begin () + (__n + __inx), __x);
        }
        else if (__n) {

            for (size_type __i = __n; __i; --__i)
                push_front (*(begin () + __n - 1));

            const iterator __tmp = begin () + __n;

            _STD::copy (__tmp + __n, __tmp + __inx, __tmp);

            _STD::fill (begin () + __inx, __tmp + __inx, __x);
        }
    }
    else {

        // inserting `x' into the second half of the container

        const size_type __size0 = size ();

        if (__rem < __n) {

            for (size_type __i = __n - __rem; __i; --__i)
                push_back (__x);

            for (size_type __j = 0; __j != __rem; ++__j)
                push_back (*(begin () + (__inx + __j)));

            _STD::fill (begin () + __inx, begin () + __size0, __x);
        }
        else if (__n) {

            for (size_type __i = 0;__i != __n; ++__i)
                push_back (*(begin () + __size0 - __n + __i));

            const iterator __tmp = begin () + __inx;

            _STD::copy_backward (__tmp, begin () + __size0 - __n, 
                                 begin () + __size0);

            _STD::fill (__tmp, __tmp + __n, __x);
        }
    }
}


#ifndef _RWSTD_NO_MEMBER_TEMPLATES

template <class _TypeT, class _Allocator>
template <class _InputIter>
void deque<_TypeT, _Allocator>::
_C_assign_range (_InputIter __first, _InputIter __last, input_iterator_tag)
{
    typedef deque _Deque;

    deque* const __self = this;

#else   // if defined (_RWSTD_NO_MEMBER_TEMPLATES)

template <class _TypeT, class _Allocator, class _InputIter>
void
__rw_assign_range (deque<_TypeT, _Allocator> *__self,
                   _InputIter __first, _InputIter __last, input_iterator_tag)
{
    typedef deque<_TypeT, _Allocator> _Deque;
    typedef _TYPENAME _Deque::iterator iterator;
    
#endif   // _RWSTD_NO_MEMBER_TEMPLATES

    _RWSTD_ASSERT_RANGE (__first, __last);
    _RWSTD_ASSERT (__self->_C_is_valid ());

    // assign over existing elements for better efficiency
    // and exception safety

    // complexity: linear in distance(first, last)
    // exception safety:
    // -- nothrow if size() >= distance(first, last), and value_type's
    //    assignment operator and iterator operations do not throw
    // -- basic otherwise

    const iterator __end = __self->end ();

    // avoid using the name __i or __it below so as not to trigger
    // a (bogus) gcc 2.95.2 -Wshadow warning: declaration of `__i'
    // shadows previous local
    for (iterator __ix = __self->begin (); !(__ix == __end);
         ++__ix, ++__first) {
        if (__first == __last) {
            __self->erase (__ix, __end);
            return;
        }
        *__ix = *__first;
    }

    // append the rest of the range one element at a time; the input
    // iterator overload of insert copies the whole container for the
    // strong guarantee this function does not make
    for ( ; !(__first == __last); ++__first)
        __self->push_back (*__first);
}


#ifndef _RWSTD_NO_MEMBER_TEMPLATES

template<class _TypeT, class _Allocator>
template <class _InputIter>
void deque<_TypeT, _Allocator>::
_C_insert_range (iterator __it,
                 _InputIter __first, _InputIter __last,
                 input_iterator_tag)
{
    typedef deque _Deque;

    _Deque* const __self = this;

#else   // if defined (_RWSTD_NO_MEMBER_TEMPLATES)

template <class _TypeT, class _Allocator, class _DequeIter, class _InputIter>
void
__rw_insert_range (deque<_TypeT, _Allocator> *__self, _DequeIter __it,
                   _InputIter __first, _InputIter __last, input_iterator_tag)
{
    typedef deque<_TypeT, _Allocator> _Deque;

#endif   // _RWSTD_NO_MEMBER_TEMPLATES

    _RWSTD_ASSERT_RANGE (__it, __self->end ());
    _RWSTD_ASSERT_RANGE (__first, __last);

    // 23.2.1.3, p2: if an exception is thrown other than by the copy
    // constructor or assignment operator of T there are no effects.

    if (__first == __last)
        return;

    _Deque __tmp (__self->begin (), __it, __self->get_allocator ());

    for (; !(__first == __last); ++__first)
        __tmp.push_back (*__first);

    __tmp.insert (__tmp.end (), __it, __self->end ());

    __self->swap (__tmp);
}


// #ifndef _RWSTD_NO_MEMBER_TEMPLATES

// template<class _TypeT, class _Allocator>
// template <class _FwdIter>
// void deque<_TypeT, _Allocator>::
// _C_insert_range (iterator __it,
//                  _FwdIter __first, _FwdIter __last,
//                  forward_iterator_tag)
// {
//     typedef deque _Deque;

//     _Deque* const __self = this;

// #else   // if defined (_RWSTD_NO_MEMBER_TEMPLATES)

// template <class _TypeT, class _Allocator, class _DequeIter, class _FwdIter>
// void
// __rw_insert_range (deque<_TypeT, _Allocator> *__self, _DequeIter __it,
//                    _FwdIter __first, _FwdIter __last, forward_iterator_tag)
// {
//     typedef deque<_TypeT, _Allocator> _Deque;

// #endif   // _RWSTD_NO_MEMBER_TEMPLATES

//     // implemented in terms of the Input Iterator overload
//     _RWSTD_INSERT_RANGE (__it, __first, __last, input_iterator_tag ());
// }


#ifndef _RWSTD_NO_MEMBER_TEMPLATES

template <class _TypeT, class _Allocator>
template <class _BidirIter>
void deque<_TypeT, _Allocator>::
_C_insert_range (iterator __it,
                 _BidirIter __first, _BidirIter __last,
                 bidirectional_iterator_tag)
{
    typedef deque _Deque;

    _Deque* const __self = this;

#else   // if defined (_RWSTD_NO_MEMBER_TEMPLATES)

template <class _TypeT, class _Allocator, class _DequeIter, class _BidirIter>
void
__rw_insert_range (deque<_TypeT, _Allocator> *__self, _DequeIter __it,
                   _BidirIter __first, _BidirIter __last,
                   bidirectional_iterator_tag)

{
    typedef deque<_TypeT, _Allocator>         _Deque;
    typedef _TYPENAME _Deque::difference_type difference_type;
    typedef _TYPENAME _Deque::size_type       size_type;
    typedef _TYPENAME _Deque::iterator        iterator;

#endif   // _RWSTD_NO_MEMBER_TEMPLATES

    _RWSTD_ASSERT_RANGE (__self->begin (), __it);
    _RWSTD_ASSERT_RANGE (__first, __last);
    _RWSTD_ASSERT (__self->_C_is_valid ());

    // compute the distance of `it' from the beginning of the container,
    // the distance of `it' to the end of the container, and the number
    // of elements to be inserted
    const size_type __sz0 = __self->size ();
    const size_type __inx = _DISTANCE (__self->begin (), __it, size_type);
    const size_type __rem = __sz0 - __inx;
    const size_type __n = _DISTANCE (__first, __last, size_type);

    _RWSTD_ASSERT (__inx <= __sz0);

    if (__inx < __rem) {

        // inserting into the first half of the container

        if (__inx < __n) {

            // the number of elements in the range controlled by *this,
            // [begin, it), is less than the number of elements in the
            // range [first, last)

            _BidirIter __from (__last);

            _STD::advance (__from, -difference_type (__inx));

            const _BidirIter __mid (__from);

            while (!(__from == __first))
                __self->push_front (*--__from);

            for (size_type __i = __inx; __i; --__i)
                __self->push_front (*(__self->begin () + (__n - 1)));

            _STD::copy (__mid, __last, __self->begin () + __n);
        }
        else {

            // the number of elements in the range to be inserted,
            // [first, last), is less than or equal to the number
            // of elements in the range  [begin, it)

            for (size_type __i = __n; __i; --__i)
                __self->push_front (*(__self->begin () + (__n - 1)));

            const iterator __to = __self->begin () + __n;
            
            _STD::copy (__to + __n, __to + __inx, __to);
            _STD::copy (__first, __last, __self->begin () + __inx);
        }
    }
    else {
        // inserting into the second half of the container

        if (__rem < __n) {

            _BidirIter __from (__first);

            _STD::advance (__from, __rem);

            const _BidirIter __to (__from);

            for ( ; !(__from == __last); ++__from)
                __self->push_back (*__from);

            for (size_type __i = 0; __i != __rem; ++__i)
                __self->push_back (*(__self->begin () + (__inx + __i)));

            _STD::copy (__first, __to, __self->begin () + __inx);
        }
        else {
            for (size_type __i = 0; __i != __n; ++__i)
                __self->push_back (*(__self->begin () + (__sz0 - __n + __i)));

            const iterator __to       = __self->begin () + __sz0;
            const iterator __from     = __self->begin () + __inx;
            const iterator __from_end = __self->begin () + (__sz0 - __n);

            _STD::copy_backward (__from, __from_end, __to);
            _STD::copy (__first, __last, __from);
        }
    }
}


template<class _TypeT, class _Allocator>
bool deque<_TypeT, _Allocator>::
_C_is_valid (int __emptyfl) const 
{
#ifdef _RWSTDDEBUG

    if (__emptyfl < 0 /* don't care */)
        return _C_is_valid (0 + empty ());

    if (_C_node_pointer () == _C_nodes) {

        // no storage: the container is empty, both `current' pointers
        // are 0 and both node pointers dereference `end.cur'
        _RWSTD_ASSERT (0 == _C_node_size);
        _RWSTD_ASSERT (__emptyfl && empty ());

        _RWSTD_ASSERT (pointer () == _C_beg._C_cur);
        _RWSTD_ASSERT (pointer () == _C_end._C_cur);

        _RWSTD_ASSERT (&_C_end._C_cur == _C_beg._C_node);
        _RWSTD_ASSERT (&_C_end._C_cur == _C_end._C_node);

        return true;
    }

    const size_type __bufsize = _C_bufsize ();

    if (__emptyfl)
        _RWSTD_ASSERT (empty ());
    else
        _RWSTD_ASSERT (!empty ());

    // `begin' and `end' must point into the vector of nodes, in order
    _RWSTD_ASSERT (_C_nodes <= _C_beg._C_node);
    _RWSTD_ASSERT (_C_beg._C_node <= _C_end._C_node);
    _RWSTD_ASSERT (_C_end._C_node < _C_nodes + _C_node_size);

    // every node owns a buffer; the first, the last and the two
    // the iterators point to stand for all (the check is constant
    // time and runs on every push, pop and subscript); the guard
    // after the last node is null
    _RWSTD_ASSERT (pointer () != _C_nodes [0]);
    _RWSTD_ASSERT (pointer () != _C_nodes [_C_node_size - 1]);
    _RWSTD_ASSERT (pointer () != *_C_beg._C_node);
    _RWSTD_ASSERT (pointer () != *_C_end._C_node);
    _RWSTD_ASSERT (pointer () == _C_nodes [_C_node_size]);

    // `begin' must point into an array of `bufsize' elements
    _RWSTD_ASSERT (_C_beg._C_cur >= *_C_beg._C_node);
    _RWSTD_ASSERT (_C_beg._C_cur <= *_C_beg._C_node + __bufsize);

    // `end' must point into an array of `bufsize' elements
    _RWSTD_ASSERT (_C_end._C_cur >= *_C_end._C_node);
    _RWSTD_ASSERT (_C_end._C_cur <= *_C_end._C_node + __bufsize);

#else   // if !defined (_RWSTDDEBUG)

    _RWSTD_UNUSED (__emptyfl);

#endif   // _RWSTDDEBUG

    return true;
}


}   // namespace std
