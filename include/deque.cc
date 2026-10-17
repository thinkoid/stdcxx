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

    return erase (__it, __it + 1);
}


template <class _TypeT, class _Allocator>
_TYPENAME deque<_TypeT, _Allocator>::iterator
deque<_TypeT, _Allocator>::
erase (iterator __first, iterator __last)
{
    _RWSTD_ASSERT_RANGE (__first, __last);
    _RWSTD_ASSERT_RANGE (begin (), __first);
    _RWSTD_ASSERT_RANGE (__last, end ());

    const size_type __off = __first - begin ();
    size_type       __n   = __last - __first;

    if (0 == __n)
        return begin () + __off;

    // the shorter side moves over the erased range and is popped off
    // its end; the pops may recentre an emptied deque, so the result
    // is rebuilt from the offset afterwards
    if (__off < size_type (end () - __last)) {
        _STD::copy_backward (begin (), __first, __last);
        while (__n--)
            pop_front ();
    }
    else {
        _STD::copy (__last, end (), __first);
        while (__n--)
            pop_back ();
    }

    return begin () + __off;
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

    }
    else {
        deque __tmp (*this);
        *this = __rhs;
        __rhs = __tmp;
    }
}


template <class _TypeT, class _Allocator>
void deque<_TypeT, _Allocator>::
_C_push (bool __back, const_reference __x)
{
    _C_reservation __room (*this, __back, 1);
    __room._C_construct (__x);
    __room._C_commit ();
}


template <class _TypeT, class _Allocator>
deque<_TypeT, _Allocator>::_C_reservation::
_C_reservation (deque &__deque, bool __back, size_type __n)
    : _C_deque (__deque), _C_beg (__deque._C_beg), _C_end (__deque._C_end),
      _C_nodes (__deque._C_nodes), _C_node_size (__deque._C_node_size),
      _C_first (0), _C_count (0), _C_pos (0), _C_moved (0), _C_added (0),
      _C_alloc (0), _C_n (__n), _C_back (__back), _C_done (false)
{
    _TRY {
        _C_prepare ();
    }
    _CATCH (...) {
        _C_restore ();
        _RETHROW;
    }

    // the room is in place: construction starts at its near end
    _C_base = __back ? _C_deque._C_end
                    : _C_deque._C_beg - difference_type (__n);
    _C_cur = _C_base;
}


template <class _TypeT, class _Allocator>
deque<_TypeT, _Allocator>::_C_reservation::~_C_reservation ()
{
    if (!_C_done) {
        for ( ; _C_base != _C_cur; ++_C_base)
            _RWSTD_VALUE_ALLOC (_C_value_alloc_type, _C_deque,
                                destroy (_C_base._C_address ()));
        _C_restore ();
    }
}


template <class _TypeT, class _Allocator>
void deque<_TypeT, _Allocator>::_C_reservation::_C_commit ()
{
    if (_C_back)
        _C_deque._C_end += difference_type (_C_n);
    else
        _C_deque._C_beg -= difference_type (_C_n);

    // a map the preparation replaced lived on until now, so that the
    // iterators saved for a failure stayed valid
    if (_C_nodes && _C_nodes != _C_deque._C_nodes)
        _C_node_alloc_type (_C_deque).deallocate (_C_nodes, _C_node_size + 1);

    _C_done = true;
}


// Move a contiguous owned span within the map and clear the vacated slots.
template <class _TypeT, class _Allocator>
void deque<_TypeT, _Allocator>::
_C_move_nodes (size_type __from, size_type __to, size_type __n)
{
    if (__to < __from) {
        _STD::copy (_C_nodes + __from, _C_nodes + __from + __n, _C_nodes + __to);
        const size_type __clear = __from < __to + __n ? __to + __n : __from;
        _STD::fill (_C_nodes + __clear, _C_nodes + __from + __n, pointer ());
    }
    else if (__from < __to) {
        _STD::copy_backward (_C_nodes + __from, _C_nodes + __from + __n,
                             _C_nodes + __to + __n);
        const size_type __clear = __to < __from + __n ? __to : __from + __n;
        _STD::fill (_C_nodes + __from, _C_nodes + __clear, pointer ());
    }
}


template <class _TypeT, class _Allocator>
void deque<_TypeT, _Allocator>::_C_reservation::_C_prepare ()
{
    deque &__d = _C_deque;

    // the room left in the partial buffer at this end
    const size_type __b = __d._C_bufsize ();
    const difference_type __begoff = _C_beg._C_offset ();
    const difference_type __endoff = _C_end._C_offset ();
    const size_type __partial = _C_back
        ? (__endoff ? __b - size_type (__endoff) : 0)
        : size_type (__begoff);

    if (_C_n <= __partial)
        return;

    // whole buffers wanted beyond it, and where the live range sits:
    // its first slot, its end slot, and the first slot past its last
    // live element
    size_type __needed = (_C_n - __partial) / __b
                      + (0 != (_C_n - __partial) % __b);
    const size_type __beg = _C_nodes ? _C_beg._C_node - _C_nodes : 0;
    const size_type __end = _C_nodes ? _C_end._C_node - _C_nodes : 0;
    const size_type __past = __end + (0 != __endoff);

    // the owned span, found by scanning outwards from the live range
    // across spare buffers only; a dense queue has few to walk
    _C_first = __beg;
    while (_C_first && _C_nodes [_C_first - 1])
        --_C_first;

    size_type __last = __past;
    while (__last != _C_node_size && _C_nodes [__last])
        ++__last;

    _C_count = __last - _C_first;
    _C_pos = _C_first;

    // the spares on this side come first
    const size_type __near = _C_back ? __last - __past : __beg - _C_first;

    if (__needed <= __near)
        return;

    __needed -= __near;

    // then the far side's spares, moved over one pointer each; what is
    // still short is new buffers, within the allocator's and the
    // difference type's limits
    const size_type __far = _C_back ? __beg - _C_first : __last - __past;
    const size_type __reuse = __far < __needed ? __far : __needed;
    const size_type __nnew = __needed - __reuse;
    const size_type __limit = __d._C_max_nodes ();

    _RWSTD_REQUIRES (__nnew <= __limit - _C_count,
                     (_RWSTD_ERROR_LENGTH_ERROR,
                      _RWSTD_FUNC ("deque::insert"), _C_n, __d.max_size ()));

    // the moved and the new buffers need null slots on this side; short
    // of them, recentre the span when the null slack pays for moving it,
    // otherwise a larger map, the old one kept until commit
    const size_type __slack = _C_node_size - _C_count;
    const size_type __near_null = _C_back ? _C_node_size - __last : _C_first;

    if (__near_null < __needed) {
        if (__needed <= __slack && _C_count <= __slack) {
            _C_pos = (__slack - __needed) / 2 + (_C_back ? 0 : __needed);
            __d._C_move_nodes (_C_first, _C_pos, _C_count);
            __d._C_beg._C_node = __d._C_nodes + _C_pos + (__beg - _C_first);
            __d._C_end._C_node = __d._C_nodes + _C_pos + (__end - _C_first);
        }
        else {
            const size_type __required = _C_count + __nnew;
            const size_type __from = _C_node_size < __required
                ? __required : _C_node_size;
            size_type __capacity = _RWSTD_NEW_CAPACITY (deque, &__d, __from);
            if (__capacity < __required || __capacity <= _C_node_size
                || __limit < __capacity)
                __capacity = __limit;

            _C_node_pointer __map =
                _C_node_alloc_type (__d).allocate (__capacity + 1, _C_nodes);
            _STD::fill (__map, __map + __capacity + 1, pointer ());

            const size_type __first = (__capacity - __required) / 2
                                   + (_C_back ? 0 : __nnew);
            const size_type __cut = _C_back ? __reuse : _C_count - __reuse;
            if (_C_count) {
                _STD::copy (_C_nodes + _C_first + __cut, _C_nodes + __last,
                            __map + __first);
                _STD::copy (_C_nodes + _C_first, _C_nodes + _C_first + __cut,
                            __map + __first + _C_count - __cut);
            }

            const difference_type __shift = _C_back
                ? -difference_type (__reuse) : difference_type (__reuse);
            __d._C_nodes = __map;
            __d._C_node_size = __capacity;
            __d._C_beg._C_node = __map + __first + (__beg - _C_first) + __shift;
            __d._C_end._C_node = __map + __first + (__end - _C_first) + __shift;
            _C_alloc = _C_back ? __first + _C_count : __first - __nnew;
        }
    }

    // on the same map the far spares move over now, one pointer each,
    // from the span's far end to the slots next to its near end; a new
    // map received them already rotated
    if (__d._C_nodes == _C_nodes) {
        for (size_type __i = 0; __i != __reuse; ++__i) {
            const size_type __from = _C_back ? _C_pos + __i
                : _C_pos + _C_count - 1 - __i;
            const size_type __to = _C_back ? _C_pos + _C_count + __i
                : _C_pos - 1 - __i;
            __d._C_nodes [__to] = __d._C_nodes [__from];
            __d._C_nodes [__from] = pointer ();
        }

        _C_moved = __reuse;
        _C_alloc = _C_back ? _C_pos + _C_count + __reuse
                          : _C_pos - __reuse - __nnew;
    }

    // the buffers still missing, one per slot, each owned as soon as
    // its pointer is written
    _C_value_alloc_type __alloc (__d);
    for ( ; _C_added != __nnew; ++_C_added)
        __d._C_nodes [_C_alloc + _C_added] = __alloc.allocate (__b, pointer ());

    // both iterators canonical again: a boundary slot that gained a
    // buffer now carries its element pointer
    __d._C_beg = _C_deque_iter (__d._C_beg._C_node, __begoff);
    __d._C_end = _C_deque_iter (__d._C_end._C_node, __endoff);
}


template <class _TypeT, class _Allocator>
void deque<_TypeT, _Allocator>::_C_reservation::_C_restore ()
{
    deque &__d = _C_deque;

    // the new buffers go first
    _C_value_alloc_type __alloc (__d);
    for (size_type __i = 0; __i != _C_added; ++__i) {
        __alloc.deallocate (__d._C_nodes [_C_alloc + __i], __d._C_bufsize ());
        __d._C_nodes [_C_alloc + __i] = pointer ();
    }

    // a replaced map goes whole; the same map has its transfers undone
    // first and its recentring after, the reverse of the preparation
    if (__d._C_nodes != _C_nodes)
        _C_node_alloc_type (__d).deallocate (__d._C_nodes, __d._C_node_size + 1);
    else {
        for (size_type __i = 0; __i != _C_moved; ++__i) {
            const size_type __to = _C_back ? _C_pos + __i
                : _C_pos + _C_count - 1 - __i;
            const size_type __from = _C_back ? _C_pos + _C_count + __i
                : _C_pos - 1 - __i;
            __d._C_nodes [__to] = __d._C_nodes [__from];
            __d._C_nodes [__from] = pointer ();
        }
        __d._C_move_nodes (_C_pos, _C_first, _C_count);
    }

    // the four fields as saved; the iterators point into buffers that
    // never moved
    __d._C_nodes = _C_nodes;
    __d._C_node_size = _C_node_size;
    __d._C_beg = _C_beg;
    __d._C_end = _C_end;
}


template <class _TypeT, class _Allocator>
void deque<_TypeT, _Allocator>::_C_dealloc ()
{
    _RWSTD_ASSERT (empty ());

    if (!_C_nodes)
        return;

    // every owned buffer, live or spare, then the map
    _C_value_alloc_type __alloc (*this);
    for (size_type __i = 0; __i != _C_node_size; ++__i) {
        if (_C_nodes [__i])
            __alloc.deallocate (_C_nodes [__i], _C_bufsize ());
    }

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

    for (iterator __it = begin (); !(__it == __end); ++__it, --__n) {
        if (size_type () == __n) {
            erase (__it, __end);
            return;
        }
        *__it = __x;
    }

    insert (__end, __n, __x);
}


template <class _TypeT, class _Allocator>
void deque<_TypeT, _Allocator>::
_C_insert_n (const iterator &__it, size_type __n, const_reference __x)
{
    _RWSTD_ASSERT_RANGE (begin (), __it);
    _RWSTD_ASSERT_RANGE (__it, end ());

    if (0 == __n)
        return;

    _RWSTD_REQUIRES (__n <= max_size () - size (),
                     (_RWSTD_ERROR_LENGTH_ERROR,
                      _RWSTD_FUNC ("deque::insert(iterator, size_type, "
                                   "const_reference)"),
                      __n, max_size ()));

    const size_type __off = __it - begin ();

    if (__off && __off != size ()) {
        // A shift may overwrite the element supplying the fill value.
        const value_type __saved (__x);
        _C_insert_fill (__off, __n, __saved);
    }
    else
        _C_insert_fill (__off, __n, __x);
}


template <class _TypeT, class _Allocator>
void deque<_TypeT, _Allocator>::
_C_insert_fill (size_type __off, size_type __n, const_reference __x)
{
    // the shorter side shifts; the reservation may move map slots, so
    // the position is taken after it
    const size_type __rem = size () - __off;
    const bool __back = __rem <= __off;
    _C_reservation __room (*this, __back, __n);
    const _C_deque_iter __pos = _C_beg + difference_type (__off);

    if (__back) {
        if (__rem < __n) {
            // the fill outruns the tail: the part of it past the old end
            // and then the tail are constructed, the tail's old place
            // takes the rest of the fill
            for (size_type __i = __n - __rem; __i; --__i)
                __room._C_construct (__x);
            for (_C_deque_iter __from = __pos; __from != _C_end; ++__from)
                __room._C_construct (*__from);
            _STD::fill (__pos, _C_end, __x);
        }
        else {
            // the tail's last n are constructed past the old end, the
            // rest of the tail shifts up, the hole takes the fill
            const _C_deque_iter __tail = _C_end - difference_type (__n);
            for (_C_deque_iter __from = __tail; __from != _C_end; ++__from)
                __room._C_construct (*__from);
            _STD::copy_backward (__pos, __tail, _C_end);
            _STD::fill (__pos, __pos + difference_type (__n), __x);
        }
    }
    else {
        if (__off < __n) {
            // the fill outruns the prefix: the prefix and then the part
            // of the fill before the old beginning are constructed, the
            // prefix's old place takes the rest of the fill
            for (_C_deque_iter __from = _C_beg; __from != __pos; ++__from)
                __room._C_construct (*__from);
            while (__room._C_cur != _C_beg)
                __room._C_construct (__x);
            _STD::fill (_C_beg, __pos, __x);
        }
        else {
            // the prefix's first n are constructed before the old
            // beginning, the rest of the prefix shifts down, the hole
            // takes the fill
            for (_C_deque_iter __from = _C_beg;
                 __room._C_cur != _C_beg; ++__from)
                __room._C_construct (*__from);
            _STD::copy (_C_beg + difference_type (__n), __pos, _C_beg);
            _STD::fill (__pos - difference_type (__n), __pos, __x);
        }
    }

    __room._C_commit ();

    _RWSTD_ASSERT (_C_is_valid (0));
}


#ifndef _RWSTD_NO_MEMBER_TEMPLATES

template <class _TypeT, class _Allocator>
template <class _Iterator>
void deque<_TypeT, _Allocator>::
_C_assign_range (_Iterator __first, _Iterator __last, input_iterator_tag)
{
    deque* const __self = this;

#else   // if defined (_RWSTD_NO_MEMBER_TEMPLATES)

template <class _TypeT, class _Allocator, class _Iterator>
void
__rw_assign_range (deque<_TypeT, _Allocator> *__self,
                   _Iterator __first, _Iterator __last, input_iterator_tag)
{
    typedef _TYPENAME deque<_TypeT, _Allocator>::iterator iterator;
    
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

    for (iterator __it = __self->begin (); !(__it == __end);
         ++__it, ++__first) {
        if (__first == __last) {
            __self->erase (__it, __end);
            return;
        }
        *__it = *__first;
    }

    // append the rest of the range one element at a time
    for ( ; !(__first == __last); ++__first)
        __self->push_back (*__first);
}


#ifndef _RWSTD_NO_MEMBER_TEMPLATES

template<class _TypeT, class _Allocator>
template <class _Iterator>
void deque<_TypeT, _Allocator>::
_C_insert_range (iterator __it,
                 _Iterator __first, _Iterator __last,
                 input_iterator_tag)
{
    typedef deque _Deque;

    _Deque* const __self = this;

#else   // if defined (_RWSTD_NO_MEMBER_TEMPLATES)

template <class _TypeT, class _Allocator, class _DequeIter, class _Iterator>
void
__rw_insert_range (deque<_TypeT, _Allocator> *__self, _DequeIter __it,
                   _Iterator __first, _Iterator __last, input_iterator_tag)
{
    typedef deque<_TypeT, _Allocator> _Deque;

#endif   // _RWSTD_NO_MEMBER_TEMPLATES

    _RWSTD_ASSERT_RANGE (__it, __self->end ());
    _RWSTD_ASSERT_RANGE (__first, __last);

    if (__first == __last)
        return;

    // Iterator category does not promise non-throwing source operations.
    // Consume an arbitrary source before overwriting destination elements.
    _Deque __tmp (__self->get_allocator ());

    const _TYPENAME _Deque::size_type __limit =
        __self->max_size () - __self->size ();
    for ( ; !(__first == __last); ++__first) {
        _RWSTD_REQUIRES (__tmp.size () < __limit,
                         (_RWSTD_ERROR_LENGTH_ERROR,
                          _RWSTD_FUNC ("deque::insert"), __tmp.size (), __limit));
        __tmp.push_back (*__first);
    }

    __self->insert (__it, __tmp.begin (), __tmp.end ());
}


#ifndef _RWSTD_NO_MEMBER_TEMPLATES

template <class _TypeT, class _Allocator>
template <class _Iterator>
void deque<_TypeT, _Allocator>::
_C_insert_range (iterator __it,
                 _Iterator __first, _Iterator __last,
                 bidirectional_iterator_tag)
{
    typedef deque _Deque;

    _Deque* const __self = this;

#else   // if defined (_RWSTD_NO_MEMBER_TEMPLATES)

template <class _TypeT, class _Allocator, class _DequeIter, class _Iterator>
void
__rw_insert_range (deque<_TypeT, _Allocator> *__self, _DequeIter __it,
                   _Iterator __first, _Iterator __last,
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

    _RWSTD_REQUIRES (__n <= __self->max_size () - __self->size (),
                     (_RWSTD_ERROR_LENGTH_ERROR,
                      _RWSTD_FUNC ("deque::insert(iterator, Iterator, "
                                   "Iterator)"),
                      __n, __self->max_size ()));

    const size_type __inx = _DISTANCE (__self->begin (), __it, size_type);
    const size_type __rem = __self->size () - __inx;

    _RWSTD_ASSERT (__inx <= __self->size ());

    // This overload receives pointers or this deque's raw iterators.
    // Their operations cannot throw after a shift of existing values.
    const bool __back = __rem <= __inx;
    _TYPENAME _Deque::_C_reservation __room (*__self, __back, __n);

    typedef _TYPENAME _Deque::_C_deque_iter _Iter;
    const _Iter __beg = __self->_C_beg;
    const _Iter __end = __self->_C_end;
    const _Iter __pos = __beg + difference_type (__inx);

    if (__back) {
        if (__rem < __n) {
            // the range outruns the tail: its part past the old end and
            // then the tail are constructed, the tail's old place takes
            // the rest of the range
            _Iterator __mid = __first;
            _STD::advance (__mid, difference_type (__rem));
            for (_Iterator __from = __mid; __from != __last; ++__from)
                __room._C_construct (*__from);
            for (_Iter __from = __pos; __from != __end; ++__from)
                __room._C_construct (*__from);
            _STD::copy (__first, __mid, __pos);
        }
        else {
            // the tail's last n are constructed past the old end, the
            // rest of the tail shifts up, the hole takes the range
            const _Iter __tail = __end - difference_type (__n);
            for (_Iter __from = __tail; __from != __end; ++__from)
                __room._C_construct (*__from);
            _STD::copy_backward (__pos, __tail, __end);
            _STD::copy (__first, __last, __pos);
        }
    }
    else {
        if (__inx < __n) {
            // the range outruns the prefix: the prefix and then the
            // range's part before the old beginning are constructed,
            // the prefix's old place takes the rest of the range
            _Iterator __mid = __first;
            _STD::advance (__mid, difference_type (__n - __inx));
            for (_Iter __from = __beg; __from != __pos; ++__from)
                __room._C_construct (*__from);
            for (_Iterator __from = __first; __from != __mid; ++__from)
                __room._C_construct (*__from);
            _STD::copy (__mid, __last, __beg);
        }
        else {
            // the prefix's first n are constructed before the old
            // beginning, the rest of the prefix shifts down, the hole
            // takes the range
            for (_Iter __from = __beg; __room._C_cur != __beg; ++__from)
                __room._C_construct (*__from);
            _STD::copy (__beg + difference_type (__n), __pos, __beg);
            _STD::copy (__first, __last, __pos - difference_type (__n));
        }
    }

    __room._C_commit ();

    _RWSTD_ASSERT (__self->_C_is_valid (0));
}


template<class _TypeT, class _Allocator>
bool deque<_TypeT, _Allocator>::
_C_is_valid (int __emptyfl) const 
{
#ifdef _RWSTDDEBUG

    if (0 <= __emptyfl)
        _RWSTD_ASSERT (bool (__emptyfl) == empty ());

    if (!_C_nodes) {
        _RWSTD_ASSERT (0 == _C_node_size);
        _RWSTD_ASSERT (empty ());
        _RWSTD_ASSERT (0 == _C_beg._C_node && 0 == _C_beg._C_offset ());
        return true;
    }

    // the live range lies within the map, the sentinel slot included
    // for the end
    _RWSTD_ASSERT (_C_nodes <= _C_beg._C_node);
    _RWSTD_ASSERT (_C_beg._C_node <= _C_end._C_node);
    _RWSTD_ASSERT (_C_end._C_node <= _C_nodes + _C_node_size);

    // offsets canonical, and the sentinel null and reached at offset 0
    _RWSTD_ASSERT (size_type (_C_beg._C_offset ()) < _C_bufsize ());
    _RWSTD_ASSERT (size_type (_C_end._C_offset ()) < _C_bufsize ());
    _RWSTD_ASSERT (   _C_end._C_node != _C_nodes + _C_node_size
                  || 0 == _C_end._C_offset ());
    _RWSTD_ASSERT (0 == _C_nodes [_C_node_size]);

    // the slots of the first and the last live element own buffers
    if (!empty ()) {
        _RWSTD_ASSERT (_C_beg < _C_end);
        _RWSTD_ASSERT (0 != *_C_beg._C_node);
        _RWSTD_ASSERT (0 != *(_C_end - 1)._C_node);
    }

#else   // if !defined (_RWSTDDEBUG)

    _RWSTD_UNUSED (__emptyfl);

#endif   // _RWSTDDEBUG

    return true;
}


}   // namespace std
