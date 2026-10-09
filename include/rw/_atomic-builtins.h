/***************************************************************************
 *
 * _atomic-builtins.h - definitions of inline functions for atomic
 *                      operations using the __atomic_xxx() built-ins
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
 * Copyright 1994-2008 Rogue Wave Software, Inc.
 * 
 **************************************************************************/

// every operation is sequentially consistent, as strong as the __sync
// built-ins it replaces in C++ terms and stronger for the exchange,
// which __sync made an acquire barrier only; a caller that needs less
// gets it from a decision about its own site, not from this header

_RWSTD_NAMESPACE (__rw) {

#ifndef _RWSTD_NO_CHAR_ATOMIC_OPS

inline char
__rw_atomic_preincrement (char &__x, bool)
{
    _RWSTD_COMPILE_ASSERT (1 == sizeof (char));
    return __atomic_add_fetch (&__x, 1, __ATOMIC_SEQ_CST);
}


inline signed char
__rw_atomic_preincrement (signed char &__x, bool)
{
    _RWSTD_COMPILE_ASSERT (1 == sizeof (signed char));
    return __atomic_add_fetch (&__x, 1, __ATOMIC_SEQ_CST);
}


inline unsigned char
__rw_atomic_preincrement (unsigned char &__x, bool)
{
    _RWSTD_COMPILE_ASSERT (1 == sizeof (unsigned char));
    return __atomic_add_fetch (&__x, 1, __ATOMIC_SEQ_CST);
}

#endif   // _RWSTD_NO_CHAR_ATOMIC_OPS


#ifndef _RWSTD_NO_SHORT_ATOMIC_OPS

inline short
__rw_atomic_preincrement (short &__x, bool)
{
    _RWSTD_COMPILE_ASSERT (2 == sizeof (short));
    return __atomic_add_fetch (&__x, 1, __ATOMIC_SEQ_CST);
}


inline unsigned short
__rw_atomic_preincrement (unsigned short &__x, bool)
{
    _RWSTD_COMPILE_ASSERT (2 == sizeof (unsigned short));
    return __atomic_add_fetch (&__x, 1, __ATOMIC_SEQ_CST);
}

#endif   // _RWSTD_NO_SHORT_ATOMIC_OPS


inline int
__rw_atomic_preincrement (int &__x, bool)
{
    _RWSTD_COMPILE_ASSERT (4 == sizeof (int));
    return __atomic_add_fetch (&__x, 1, __ATOMIC_SEQ_CST);
}


inline unsigned int
__rw_atomic_preincrement (unsigned int &__x, bool)
{
    _RWSTD_COMPILE_ASSERT (4 == sizeof (unsigned int));
    return __atomic_add_fetch (&__x, 1, __ATOMIC_SEQ_CST);
}


#ifndef _RWSTD_NO_CHAR_ATOMIC_OPS

inline char
__rw_atomic_predecrement (char &__x, bool)
{
    _RWSTD_COMPILE_ASSERT (1 == sizeof (char));
    return __atomic_sub_fetch (&__x, 1, __ATOMIC_SEQ_CST);
}


inline signed char
__rw_atomic_predecrement (signed char &__x, bool)
{
    _RWSTD_COMPILE_ASSERT (1 == sizeof (signed char));
    return __atomic_sub_fetch (&__x, 1, __ATOMIC_SEQ_CST);
}


inline unsigned char
__rw_atomic_predecrement (unsigned char &__x, bool)
{
    _RWSTD_COMPILE_ASSERT (1 == sizeof (unsigned char));
    return __atomic_sub_fetch (&__x, 1, __ATOMIC_SEQ_CST);
}

#endif   // _RWSTD_NO_CHAR_ATOMIC_OPS


#ifndef _RWSTD_NO_SHORT_ATOMIC_OPS

inline short
__rw_atomic_predecrement (short &__x, bool)
{
    _RWSTD_COMPILE_ASSERT (2 == sizeof (short));
    return __atomic_sub_fetch (&__x, 1, __ATOMIC_SEQ_CST);
}


inline unsigned short
__rw_atomic_predecrement (unsigned short &__x, bool)
{
    _RWSTD_COMPILE_ASSERT (2 == sizeof (unsigned short));
    return __atomic_sub_fetch (&__x, 1, __ATOMIC_SEQ_CST);
}

#endif   // _RWSTD_NO_SHORT_ATOMIC_OPS


inline int
__rw_atomic_predecrement (int &__x, bool)
{
    _RWSTD_COMPILE_ASSERT (4 == sizeof (int));
    return __atomic_sub_fetch (&__x, 1, __ATOMIC_SEQ_CST);
}


inline unsigned int
__rw_atomic_predecrement (unsigned int &__x, bool)
{
    _RWSTD_COMPILE_ASSERT (4 == sizeof (unsigned int));
    return __atomic_sub_fetch (&__x, 1, __ATOMIC_SEQ_CST);
}


#ifndef _RWSTD_NO_CHAR_ATOMIC_OPS

inline char
__rw_atomic_exchange (char &__x, char __y, bool)
{
    _RWSTD_COMPILE_ASSERT (1 == sizeof (char));
    return __atomic_exchange_n (&__x, __y, __ATOMIC_SEQ_CST);
}


inline signed char
__rw_atomic_exchange (signed char &__x, signed char __y, bool)
{
    _RWSTD_COMPILE_ASSERT (1 == sizeof (signed char));
    return __atomic_exchange_n (&__x, __y, __ATOMIC_SEQ_CST);
}


inline unsigned char
__rw_atomic_exchange (unsigned char &__x, unsigned char __y, bool)
{
    _RWSTD_COMPILE_ASSERT (1 == sizeof (unsigned char));
    return __atomic_exchange_n (&__x, __y, __ATOMIC_SEQ_CST);
}

#endif   // _RWSTD_NO_CHAR_ATOMIC_OPS


#ifndef _RWSTD_NO_SHORT_ATOMIC_OPS

inline short
__rw_atomic_exchange (short &__x, short __y, bool)
{
    _RWSTD_COMPILE_ASSERT (2 == sizeof (short));
    return __atomic_exchange_n (&__x, __y, __ATOMIC_SEQ_CST);
}


inline unsigned short
__rw_atomic_exchange (unsigned short &__x, unsigned short __y, bool)
{
    _RWSTD_COMPILE_ASSERT (2 == sizeof (unsigned short));
    return __atomic_exchange_n (&__x, __y, __ATOMIC_SEQ_CST);
}

#endif   // _RWSTD_NO_SHORT_ATOMIC_OPS


inline int
__rw_atomic_exchange (int &__x, int __y, bool)
{
    _RWSTD_COMPILE_ASSERT (4 == sizeof (int));
    return __atomic_exchange_n (&__x, __y, __ATOMIC_SEQ_CST);
}


inline unsigned int
__rw_atomic_exchange (unsigned int &__x, unsigned int __y, bool)
{
    _RWSTD_COMPILE_ASSERT (4 == sizeof (unsigned int));
    return __atomic_exchange_n (&__x, __y, __ATOMIC_SEQ_CST);
}


#if 4 < _RWSTD_LONG_SIZE && !defined (_RWSTD_NO_LONG_ATOMIC_OPS)

inline long
__rw_atomic_preincrement (long &__x, bool)
{
    _RWSTD_COMPILE_ASSERT (8 == sizeof (long));
    return __atomic_add_fetch (&__x, 1, __ATOMIC_SEQ_CST);
}


inline unsigned long
__rw_atomic_preincrement (unsigned long &__x, bool)
{
    _RWSTD_COMPILE_ASSERT (8 == sizeof (unsigned long));
    return __atomic_add_fetch (&__x, 1, __ATOMIC_SEQ_CST);
}


inline long
__rw_atomic_predecrement (long &__x, bool)
{
    _RWSTD_COMPILE_ASSERT (8 == sizeof (long));
    return __atomic_sub_fetch (&__x, 1, __ATOMIC_SEQ_CST);
}


inline unsigned long
__rw_atomic_predecrement (unsigned long &__x, bool)
{
    _RWSTD_COMPILE_ASSERT (8 == sizeof (unsigned long));
    return __atomic_sub_fetch (&__x, 1, __ATOMIC_SEQ_CST);
}


inline long
__rw_atomic_exchange (long &__x, long __y, bool)
{
    _RWSTD_COMPILE_ASSERT (8 == sizeof (long));
    return __atomic_exchange_n (&__x, __y, __ATOMIC_SEQ_CST);
}


inline unsigned long
__rw_atomic_exchange (unsigned long &__x, unsigned long __y, bool)
{
    _RWSTD_COMPILE_ASSERT (8 == sizeof (unsigned long));
    return __atomic_exchange_n (&__x, __y, __ATOMIC_SEQ_CST);
}

#else
#  define _RWSTD_NO_LONG_ATOMIC_OPS
#endif   // 4 < _RWSTD_LONG_SIZE && !_RWSTD_NO_LONG_ATOMIC_OPS


#if    defined (_RWSTD_LONG_LONG)                \
    && _RWSTD_LLONG_SIZE > _RWSTD_LONG_SIZE       \
    && !defined (_RWSTD_NO_LLONG_ATOMIC_OPS)

inline _RWSTD_LONG_LONG
__rw_atomic_preincrement (_RWSTD_LONG_LONG &__x, bool)
{
    _RWSTD_COMPILE_ASSERT (8 == sizeof (_RWSTD_LONG_LONG));
    return __atomic_add_fetch (&__x, 1, __ATOMIC_SEQ_CST);
}


inline unsigned _RWSTD_LONG_LONG
__rw_atomic_preincrement (unsigned _RWSTD_LONG_LONG &__x, bool)
{
    _RWSTD_COMPILE_ASSERT (8 == sizeof (unsigned _RWSTD_LONG_LONG));
    return __atomic_add_fetch (&__x, 1, __ATOMIC_SEQ_CST);
}


inline _RWSTD_LONG_LONG
__rw_atomic_predecrement (_RWSTD_LONG_LONG &__x, bool)
{
    _RWSTD_COMPILE_ASSERT (8 == sizeof (_RWSTD_LONG_LONG));
    return __atomic_sub_fetch (&__x, 1, __ATOMIC_SEQ_CST);
}


inline unsigned _RWSTD_LONG_LONG
__rw_atomic_predecrement (unsigned _RWSTD_LONG_LONG &__x, bool)
{
    _RWSTD_COMPILE_ASSERT (8 == sizeof (unsigned _RWSTD_LONG_LONG));
    return __atomic_sub_fetch (&__x, 1, __ATOMIC_SEQ_CST);
}


inline _RWSTD_LONG_LONG
__rw_atomic_exchange (_RWSTD_LONG_LONG &__x, _RWSTD_LONG_LONG __y, bool)
{
    _RWSTD_COMPILE_ASSERT (8 == sizeof (_RWSTD_LONG_LONG));
    return __atomic_exchange_n (&__x, __y, __ATOMIC_SEQ_CST);
}


inline unsigned _RWSTD_LONG_LONG
__rw_atomic_exchange (unsigned _RWSTD_LONG_LONG &__x,
                      unsigned _RWSTD_LONG_LONG __y, bool)
{
    _RWSTD_COMPILE_ASSERT (8 == sizeof (unsigned _RWSTD_LONG_LONG));
    return __atomic_exchange_n (&__x, __y, __ATOMIC_SEQ_CST);
}

#else
#  define _RWSTD_NO_LLONG_ATOMIC_OPS
#endif   // _RWSTD_LONG_LONG && LLONG_SIZE > LONG_SIZE && !NO_LLONG_ATOMIC_OPS


/********************** ordered loads and stores **********************/

// an object of a type whose width the configuration characterized gets
// the four ordered accesses below: an acquire load and a release store
// to publish data through it, and relaxed forms for a cache that every
// thread fills with the same value and that publishes nothing else

#define _RWSTD_DEFINE_ORDERED_ATOMICS(type)                              \
    inline type                                                          \
    __rw_atomic_load_acquire (const type &__x, bool)                     \
    {                                                                    \
        return __atomic_load_n (&__x, __ATOMIC_ACQUIRE);                 \
    }                                                                    \
                                                                         \
    inline type                                                          \
    __rw_atomic_load_relaxed (const type &__x, bool)                     \
    {                                                                    \
        return __atomic_load_n (&__x, __ATOMIC_RELAXED);                 \
    }                                                                    \
                                                                         \
    inline void                                                          \
    __rw_atomic_store_release (type &__x, type __y, bool)                \
    {                                                                    \
        __atomic_store_n (&__x, __y, __ATOMIC_RELEASE);                  \
    }                                                                    \
                                                                         \
    inline void                                                          \
    __rw_atomic_store_relaxed (type &__x, type __y, bool)                \
    {                                                                    \
        __atomic_store_n (&__x, __y, __ATOMIC_RELAXED);                  \
    }

// a type the width of a smaller type is served by that type's
// characterization; this header defines _RWSTD_NO_LONG_ATOMIC_OPS and
// _RWSTD_NO_LLONG_ATOMIC_OPS for such widths as well as for failed ones
#if    _RWSTD_LONG_SIZE == _RWSTD_INT_SIZE         \
    || !defined (_RWSTD_NO_LONG_ATOMIC_OPS)
#  define _RWSTD_ORDERED_LONG
#endif

#if    defined (_RWSTD_LONG_LONG)                           \
    && (   _RWSTD_LLONG_SIZE == _RWSTD_LONG_SIZE            \
        && defined (_RWSTD_ORDERED_LONG)                    \
        || !defined (_RWSTD_NO_LLONG_ATOMIC_OPS))
#  define _RWSTD_ORDERED_LLONG
#endif

#ifndef _RWSTD_NO_CHAR_ATOMIC_OPS
_RWSTD_DEFINE_ORDERED_ATOMICS (char)
_RWSTD_DEFINE_ORDERED_ATOMICS (signed char)
_RWSTD_DEFINE_ORDERED_ATOMICS (unsigned char)
#endif   // _RWSTD_NO_CHAR_ATOMIC_OPS

#ifndef _RWSTD_NO_SHORT_ATOMIC_OPS
_RWSTD_DEFINE_ORDERED_ATOMICS (short)
_RWSTD_DEFINE_ORDERED_ATOMICS (unsigned short)
#endif   // _RWSTD_NO_SHORT_ATOMIC_OPS

_RWSTD_DEFINE_ORDERED_ATOMICS (int)
_RWSTD_DEFINE_ORDERED_ATOMICS (unsigned int)

#ifdef _RWSTD_ORDERED_LONG
_RWSTD_DEFINE_ORDERED_ATOMICS (long)
_RWSTD_DEFINE_ORDERED_ATOMICS (unsigned long)
#endif   // _RWSTD_ORDERED_LONG

#ifdef _RWSTD_ORDERED_LLONG
_RWSTD_DEFINE_ORDERED_ATOMICS (_RWSTD_LONG_LONG)
_RWSTD_DEFINE_ORDERED_ATOMICS (unsigned _RWSTD_LONG_LONG)
#endif   // _RWSTD_ORDERED_LLONG

#if    !defined (_RWSTD_NO_NATIVE_WCHAR_T)                       \
    && (   _RWSTD_WCHAR_SIZE == _RWSTD_INT_SIZE                  \
        ||    _RWSTD_WCHAR_SIZE == _RWSTD_SHRT_SIZE              \
           && !defined (_RWSTD_NO_SHORT_ATOMIC_OPS)              \
        ||    _RWSTD_WCHAR_SIZE == _RWSTD_CHAR_SIZE              \
           && !defined (_RWSTD_NO_CHAR_ATOMIC_OPS))
_RWSTD_DEFINE_ORDERED_ATOMICS (wchar_t)
#endif   // _RWSTD_WCHAR_SIZE

#undef _RWSTD_DEFINE_ORDERED_ATOMICS

#if    _RWSTD_PTR_SIZE == _RWSTD_INT_SIZE                               \
    || _RWSTD_PTR_SIZE == _RWSTD_LONG_SIZE && defined (_RWSTD_ORDERED_LONG) \
    || _RWSTD_PTR_SIZE == _RWSTD_LLONG_SIZE && defined (_RWSTD_ORDERED_LLONG)

// pointers of every type, preferred by partial ordering over the
// templates of <rw/_atomic-mutex.h>

template <class _TypeT>
inline _TypeT*
__rw_atomic_load_acquire (_TypeT* const &__x, bool)
{
    return __atomic_load_n (&__x, __ATOMIC_ACQUIRE);
}


template <class _TypeT>
inline _TypeT*
__rw_atomic_load_relaxed (_TypeT* const &__x, bool)
{
    return __atomic_load_n (&__x, __ATOMIC_RELAXED);
}


template <class _TypeT>
inline void
__rw_atomic_store_release (_TypeT* &__x,
                           _TYPENAME __rw_atomic_value<_TypeT*>::_C_type __y,
                           bool)
{
    __atomic_store_n (&__x, __y, __ATOMIC_RELEASE);
}


template <class _TypeT>
inline void
__rw_atomic_store_relaxed (_TypeT* &__x,
                           _TYPENAME __rw_atomic_value<_TypeT*>::_C_type __y,
                           bool)
{
    __atomic_store_n (&__x, __y, __ATOMIC_RELAXED);
}

#endif   // _RWSTD_PTR_SIZE

#undef _RWSTD_ORDERED_LONG
#undef _RWSTD_ORDERED_LLONG

}   // namespace __rw
