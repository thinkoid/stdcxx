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

    _C_insert_n (__it, 1, __x);

    return begin () + __inx;
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


// pushes `x' at the back or the front of a container whose buffer
// there is full, or that has no storage
template <class _TypeT, class _Allocator>
void deque<_TypeT, _Allocator>::
_C_push (bool __at_back, const_reference __x)
{
    _RWSTD_ASSERT (_C_is_valid ());

    const _C_storage __prior = _C_reserve (__at_back, 1);

    const size_type __bufsize = _C_bufsize ();

    // step onto the neighbouring buffer when the one at the end is
    // full, and point the iterator so that the caller's increment or
    // decrement lands on the element
    _C_deque_iter __it = __at_back ? _C_end : _C_beg;

    if (__at_back) {
        if (__it._C_cur == *__it._C_node + __bufsize) {
            ++__it._C_node;
            __it._C_cur = *__it._C_node;
        }
    }
    else if (__it._C_cur == *__it._C_node) {
        --__it._C_node;
        __it._C_cur = *__it._C_node + __bufsize;
    }

    _C_value_alloc_type __alloc (*this);

    _TRY {
        __alloc.construct (__at_back ? __it._C_cur : __it._C_cur - 1, __x);
    }
    _CATCH (...) {
        _C_restore (__prior);
        _RETHROW;
    }

    if (__at_back)
        _C_end = __it;
    else
        _C_beg = __it;

    _C_discard (__prior);
}


// makes room for `n' more elements at the back or at the front
// without touching an element: shares the spare nodes at the far end
// out to the near one, half of them or as many as the room needs,
// and grows the vector with the new nodes at the near end when they
// do not suffice; a container with no storage first allocates the
// vector for `n' elements. a failed allocation leaves the container
// as it was. returns the storage as it was, for _C_restore after a
// throw from T and for _C_discard once the elements are in place
template <class _TypeT, class _Allocator>
_TYPENAME deque<_TypeT, _Allocator>::_C_storage
deque<_TypeT, _Allocator>::
_C_reserve (bool __at_back, size_type __n)
{
    _RWSTD_ASSERT (_C_is_valid ());
    _RWSTD_ASSERT (0 < __n);

    const size_type __bufsize = _C_bufsize ();

    _C_storage __prior;

    __prior._C_beg       = _C_beg;
    __prior._C_end       = _C_end;
    __prior._C_nodes     = _C_nodes;
    __prior._C_node_size = _C_node_size;
    __prior._C_shift     = 0;
    __prior._C_at_back   = __at_back;

    if (_C_node_pointer () == _C_nodes)
        _C_grow (__at_back, _C_vecsize (__n));

    if (empty ()) {
        // both iterators to the middle buffer, the near side of it
        // holding what the room needs, up to the whole buffer, the
        // rest split between the sides; the spare nodes of the far
        // side then cover any shortfall, so a fresh vector does not
        // grow again below
        const _C_node_pointer __mid = _C_nodes + _C_node_size / 2;

        const size_type __need = __n < __bufsize ? __n : __bufsize;
        const size_type __off  = (__bufsize - __need + 1) / 2;

        const size_type __at = __at_back ? __off : __bufsize - __off;

        _C_beg =
        _C_end = _C_deque_iter (*__mid + __at, __mid);
    }

    // the room at the near end, in elements, and the whole spare
    // nodes at the far one
    size_type __room;
    size_type __spare;

    if (__at_back) {
        __room  =   size_type (*_C_end._C_node + __bufsize - _C_end._C_cur)
                  + size_type (_C_nodes + _C_node_size - 1 - _C_end._C_node)
                  * __bufsize;
        __spare = size_type (_C_beg._C_node - _C_nodes);
    }
    else {
        __room  =   size_type (_C_beg._C_cur - *_C_beg._C_node)
                  + size_type (_C_beg._C_node - _C_nodes) * __bufsize;
        __spare = size_type (_C_nodes + _C_node_size - 1 - _C_end._C_node);
    }

    if (__n <= __room)
        return __prior;

    const size_type __needed = (__n - __room + __bufsize - 1) / __bufsize;

    // share the spare out: at least half of it, more when the room
    // needs more, all of it when the need exceeds it
    size_type __shift = (__spare + 1) / 2;

    if (__shift < __needed)
        __shift = __needed < __spare ? __needed : __spare;

    if (__shift) {
        _C_rotate (__at_back, __shift);
        __prior._C_shift = __shift;
    }

    if (__shift < __needed) {
        // the vector allocated above has the spare for all of `n'
        _RWSTD_ASSERT (_C_node_pointer () != __prior._C_nodes);

        const size_type __least  = _C_node_size + (__needed - __shift);
        const size_type __nnodes = _C_vecsize (size () + __n);

        _TRY {
            _C_grow (__at_back, __nnodes < __least ? __least : __nnodes);
        }
        _CATCH (...) {
            // no effects: the spare shared out goes back too
            _C_restore (__prior);
            _RETHROW;
        }
    }

    return __prior;
}


// moves `k' spare nodes from the far end of the vector to the near
// one and the other nodes over by `k' toward the far end, in place:
// no buffer changes hands and the spare nodes come out in some
// order, which is no matter, a buffer is a buffer
template <class _TypeT, class _Allocator>
void deque<_TypeT, _Allocator>::
_C_rotate (bool __at_back, size_type __k)
{
    _RWSTD_ASSERT (__k <= _C_node_size);

    if (__at_back) {
        for (size_type __i = 0; __i != _C_node_size - __k; ++__i)
            _STD::swap (_C_nodes [__i], _C_nodes [__i + __k]);

        _C_beg._C_node -= __k;
        _C_end._C_node -= __k;
    }
    else {
        for (size_type __i = _C_node_size; __k < __i; ) {
            --__i;
            _STD::swap (_C_nodes [__i], _C_nodes [__i - __k]);
        }

        _C_beg._C_node += __k;
        _C_end._C_node += __k;
    }
}


// puts the storage back as it was before a reserve, after a throw
// from T: frees the buffers of the nodes a growth added with the
// vector it made, reinstates the vector replaced, whose nodes are as
// they were, and turns the spare shared out back
template <class _TypeT, class _Allocator>
void deque<_TypeT, _Allocator>::
_C_restore (const _C_storage &__prior)
{
    const size_type __bufsize = _C_bufsize ();

    if (_C_nodes != __prior._C_nodes) {
        _C_value_alloc_type __alloc (*this);

        const size_type __nnew = _C_node_size - __prior._C_node_size;

        const _C_node_pointer __added =
            _C_nodes + (__prior._C_at_back ? __prior._C_node_size : 0);

        for (size_type __i = 0; __i != __nnew; ++__i)
            __alloc.deallocate (__added [__i], __bufsize);

        _C_node_alloc_type (*this).deallocate (_C_nodes, _C_node_size + 1);

        _C_nodes     = __prior._C_nodes;
        _C_node_size = __prior._C_node_size;
    }

    if (__prior._C_shift && _C_node_pointer () != _C_nodes)
        _C_rotate (!__prior._C_at_back, __prior._C_shift);

    _C_beg = __prior._C_beg;
    _C_end = __prior._C_end;
}


// grows the vector of nodes to `__nnodes' nodes, with a buffer for
// each node added after the existing ones (at the back) or before
// them (at the front), and the null guard after the last node that
// an iterator stepping off the last element reads; a failed
// allocation frees what this call allocated and leaves the container
// as it was. returns the vector replaced, null if there was none,
// for the caller to free or to put back
template <class _TypeT, class _Allocator>
_TYPENAME deque<_TypeT, _Allocator>::_C_node_pointer
deque<_TypeT, _Allocator>::
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

    // nothing below throws: move the existing nodes over and point
    // the iterators at them; the vector they came from stays intact
    const _C_node_pointer __old = _C_nodes;

    if (_C_node_pointer () != __old) {
        _STD::copy (__old, __old + _C_node_size, __kept);

        _C_beg._C_node = __kept + (_C_beg._C_node - __old);
        _C_end._C_node = __kept + (_C_end._C_node - __old);
    }

    __nvec [__nnodes] = pointer ();

    _C_nodes     = __nvec;
    _C_node_size = __nnodes;

    return __old;
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

    if (0 == __n)
        return;

    const size_type __inx = _DISTANCE (begin (), __it, size_type);
    const size_type __rem = size () - __inx;

    _RWSTD_ASSERT (__inx <= size ());

    // the shorter side of the insertion point moves over by `n'. the
    // elements that land beyond that end, the new ones and the moved
    // ones, are constructed into the reserve before any element is
    // assigned, so that a throw from T leaves the size and both ends
    // where they were: what was constructed is destroyed, what was
    // assigned keeps the value it was given. complexity: n copies
    // and min (inx, rem) assignments
    const bool __at_back = !(__inx < __rem);

    const _C_storage __prior = _C_reserve (__at_back, __n);

    const size_type __bufsize = _C_bufsize ();

    _C_value_alloc_type __alloc (*this);

    if (__at_back) {
        const _C_deque_iter __pos =
            __rem ? _C_end - difference_type (__rem) : _C_end;

        // the first raw element past the back, and the one past the
        // last constructed; `end' is a past-the-end iterator and may
        // sit at the edge of a full buffer, where the increment does
        // not start
        _C_deque_iter __base = _C_end;

        if (__base._C_cur == *__base._C_node + __bufsize) {
            ++__base._C_node;
            __base._C_cur = *__base._C_node;
        }

        _C_deque_iter __cur = __base;

        _TRY {
            if (__rem < __n) {
                // the n - rem copies of `x' past the back, the rem
                // elements of [pos, end) after them, and `x' over
                // [pos, end)
                for (size_type __i = __n - __rem; __i; --__i, ++__cur)
                    __alloc.construct (__cur._C_cur, __x);

                for (_C_deque_iter __from = __pos; !(__from == _C_end);
                     ++__from, ++__cur)
                    __alloc.construct (__cur._C_cur, *__from);

                _STD::fill (__pos, _C_end, __x);
            }
            else {
                // the last n elements past the back, the rest of
                // [pos, end) up by n, and `x' over [pos, pos + n)
                const _C_deque_iter __tail = _C_end - difference_type (__n);

                for (_C_deque_iter __from = __tail; !(__from == _C_end);
                     ++__from, ++__cur)
                    __alloc.construct (__cur._C_cur, *__from);

                _STD::copy_backward (__pos, __tail, _C_end);
                _STD::fill (__pos, __pos + difference_type (__n), __x);
            }
        }
        _CATCH (...) {
            for ( ; !(__base == __cur); ++__base)
                __alloc.destroy (__base._C_cur);

            _C_restore (__prior);
            _RETHROW;
        }

        _C_end = _C_end + difference_type (__n);

        // keep `end' in its form, past the last element of its buffer
        if (_C_end._C_cur == *_C_end._C_node) {
            --_C_end._C_node;
            _C_end._C_cur = *_C_end._C_node + __bufsize;
        }
    }
    else {
        const _C_deque_iter __pos     = _C_beg + difference_type (__inx);
        const _C_deque_iter __new_beg = _C_beg - difference_type (__n);

        _C_deque_iter __cur = __new_beg;

        _TRY {
            if (__inx < __n) {
                // the inx elements of [beg, pos) at the new front, the
                // n - inx copies of `x' after them up to the old front,
                // and `x' over [beg, pos)
                for (_C_deque_iter __from = _C_beg; !(__from == __pos);
                     ++__from, ++__cur)
                    __alloc.construct (__cur._C_cur, *__from);

                for ( ; !(__cur == _C_beg); ++__cur)
                    __alloc.construct (__cur._C_cur, __x);

                _STD::fill (_C_beg, __pos, __x);
            }
            else {
                // the first n elements at the new front, the rest of
                // [beg, pos) down by n, and `x' over [pos - n, pos)
                for (_C_deque_iter __from = _C_beg; !(__cur == _C_beg);
                     ++__from, ++__cur)
                    __alloc.construct (__cur._C_cur, *__from);

                _STD::copy (_C_beg + difference_type (__n), __pos, _C_beg);
                _STD::fill (__pos - difference_type (__n), __pos, __x);
            }
        }
        _CATCH (...) {
            for (_C_deque_iter __made = __new_beg; !(__made == __cur);
                 ++__made)
                __alloc.destroy (__made._C_cur);

            _C_restore (__prior);
            _RETHROW;
        }

        _C_beg = __new_beg;
    }

    _C_discard (__prior);

    _RWSTD_ASSERT (_C_is_valid (0 /* valid and non-empty */));
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

    // append the rest of the range one element at a time
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

    if (__first == __last)
        return;

    // the range can be read once and its length is not known ahead:
    // copy it into a temporary, then insert the temporary in one go,
    // with the room reserved before an element is touched (23.2.1.3,
    // p2); a throw while copying leaves *this untouched
    _Deque __tmp (__self->get_allocator ());

    for (; !(__first == __last); ++__first)
        __tmp.push_back (*__first);

    __self->insert (__it, __tmp.begin (), __tmp.end ());
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

    const size_type __n = _DISTANCE (__first, __last, size_type);

    if (0 == __n)
        return;

    const size_type __inx = _DISTANCE (__self->begin (), __it, size_type);
    const size_type __rem = __self->size () - __inx;

    _RWSTD_ASSERT (__inx <= __self->size ());

    // the shape of _C_insert_n with the range for the repeated value:
    // the shorter side moves over, what lands beyond that end is
    // constructed into the reserve before any element is assigned,
    // a throw from T leaves the size and both ends where they were.
    // complexity: n copies and min (inx, rem) assignments
    const bool __at_back = !(__inx < __rem);

    const _TYPENAME _Deque::_C_storage __prior =
        __self->_C_reserve (__at_back, __n);

    const size_type __bufsize = _Deque::_C_bufsize ();

    _TYPENAME _Deque::_C_value_alloc_type __alloc (*__self);

    _TYPENAME _Deque::_C_deque_iter &__beg = __self->_C_beg;
    _TYPENAME _Deque::_C_deque_iter &__end = __self->_C_end;

    if (__at_back) {
        const _TYPENAME _Deque::_C_deque_iter __pos =
            __rem ? __end - difference_type (__rem) : __end;

        _TYPENAME _Deque::_C_deque_iter __base = __end;

        if (__base._C_cur == *__base._C_node + __bufsize) {
            ++__base._C_node;
            __base._C_cur = *__base._C_node;
        }

        _TYPENAME _Deque::_C_deque_iter __cur = __base;

        _TRY {
            if (__rem < __n) {
                // the last n - rem of the range past the back, the
                // rem elements of [pos, end) after them, and the
                // first rem of the range over [pos, end)
                _BidirIter __mid = __first;

                _STD::advance (__mid, difference_type (__rem));

                for (_BidirIter __from = __mid; !(__from == __last);
                     ++__from, ++__cur)
                    __alloc.construct (__cur._C_cur, *__from);

                for (_TYPENAME _Deque::_C_deque_iter __from = __pos;
                     !(__from == __end); ++__from, ++__cur)
                    __alloc.construct (__cur._C_cur, *__from);

                _STD::copy (__first, __mid, __pos);
            }
            else {
                // the last n elements past the back, the rest of
                // [pos, end) up by n, and the range over [pos, pos + n)
                const _TYPENAME _Deque::_C_deque_iter __tail =
                    __end - difference_type (__n);

                for (_TYPENAME _Deque::_C_deque_iter __from = __tail;
                     !(__from == __end); ++__from, ++__cur)
                    __alloc.construct (__cur._C_cur, *__from);

                _STD::copy_backward (__pos, __tail, __end);
                _STD::copy (__first, __last, __pos);
            }
        }
        _CATCH (...) {
            for ( ; !(__base == __cur); ++__base)
                __alloc.destroy (__base._C_cur);

            __self->_C_restore (__prior);
            _RETHROW;
        }

        __end = __end + difference_type (__n);

        if (__end._C_cur == *__end._C_node) {
            --__end._C_node;
            __end._C_cur = *__end._C_node + __bufsize;
        }
    }
    else {
        const _TYPENAME _Deque::_C_deque_iter __pos =
            __beg + difference_type (__inx);
        const _TYPENAME _Deque::_C_deque_iter __new_beg =
            __beg - difference_type (__n);

        _TYPENAME _Deque::_C_deque_iter __cur = __new_beg;

        _TRY {
            if (__inx < __n) {
                // the inx elements of [beg, pos) at the new front, the
                // first n - inx of the range after them up to the old
                // front, and the rest of the range over [beg, pos)
                _BidirIter __mid = __first;

                _STD::advance (__mid, difference_type (__n - __inx));

                for (_TYPENAME _Deque::_C_deque_iter __from = __beg;
                     !(__from == __pos); ++__from, ++__cur)
                    __alloc.construct (__cur._C_cur, *__from);

                for (_BidirIter __from = __first; !(__from == __mid);
                     ++__from, ++__cur)
                    __alloc.construct (__cur._C_cur, *__from);

                _STD::copy (__mid, __last, __beg);
            }
            else {
                // the first n elements at the new front, the rest of
                // [beg, pos) down by n, and the range over [pos - n, pos)
                for (_TYPENAME _Deque::_C_deque_iter __from = __beg;
                     !(__cur == __beg); ++__from, ++__cur)
                    __alloc.construct (__cur._C_cur, *__from);

                _STD::copy (__beg + difference_type (__n), __pos, __beg);
                _STD::copy (__first, __last, __pos - difference_type (__n));
            }
        }
        _CATCH (...) {
            for (_TYPENAME _Deque::_C_deque_iter __made = __new_beg;
                 !(__made == __cur); ++__made)
                __alloc.destroy (__made._C_cur);

            __self->_C_restore (__prior);
            _RETHROW;
        }

        __beg = __new_beg;
    }

    __self->_C_discard (__prior);

    _RWSTD_ASSERT (__self->_C_is_valid (0 /* valid and non-empty */));
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
