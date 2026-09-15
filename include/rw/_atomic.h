/***************************************************************************
 *
 * _atomic.h - definitions of inline functions for atomic operations
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

#ifndef _RWSTD_RW_ATOMIC_H_INCLUDED
#define _RWSTD_RW_ATOMIC_H_INCLUDED


#if __GNUG__ >= 3
#  pragma GCC system_header
#endif   // gcc >= 3


#ifndef _RWSTD_RW_DEFS_H_INCLUDED
#  include <rw/_defs.h>
#endif   // _RWSTD_RW_DEFS_H_INCLUDED


#ifdef _RWSTD_REENTRANT

#  if defined (_RWSTD_NO_ATOMIC_OPS)
     // do nothing
#  elif   defined (__GNUG__)                        \
      && (__GNUC__ * 100 + __GNUC_MINOR__ >= 401)   \
      && (defined (__i486__) || defined (__x86_64))
#    include <rw/_atomic-sync.h>
#  elif defined (__i386__) && defined (__GNUG__)
#    include <rw/_atomic-x86.h>
#  elif defined (__x86_64)
#    include <rw/_atomic-x64.h>
#  else
#    define _RWSTD_NO_ATOMIC_OPS
#  endif

#  ifndef _RWSTD_NO_ATOMIC_OPS

/********************** generic bool functions ************************/

#    ifndef _RWSTD_NO_BOOL

#      if   _RWSTD_BOOL_SIZE == _RWSTD_CHAR_SIZE  \
         && !defined (_RWSTD_NO_CHAR_ATOMIC_OPS)
#        define _RWSTD_BOOL_TYPE char
#      elif _RWSTD_BOOL_SIZE == _RWSTD_SHORT_SIZE \
         && !defined (_RWSTD_NO_SHORT_ATOMIC_OPS)
#        define _RWSTD_BOOL_TYPE short
#      elif _RWSTD_BOOL_SIZE == _RWSTD_INT_SIZE   \
         && !defined (_RWSTD_NO_INT_ATOMIC_OPS)
#        define _RWSTD_BOOL_TYPE int
#      endif

#      ifdef _RWSTD_BOOL_TYPE

_RWSTD_NAMESPACE (__rw) {

inline bool
__rw_atomic_exchange (bool &__x, bool __y, bool)
{
    return 0 != __rw_atomic_exchange (
                    _RWSTD_REINTERPRET_CAST (_RWSTD_BOOL_TYPE&, __x),
                    _RWSTD_STATIC_CAST (_RWSTD_BOOL_TYPE, __y),
                    false);
}

}   // namespace __rw

#        undef _RWSTD_BOOL_TYPE
#      endif   // _RWSTD_BOOL_TYPE

#    endif   // _RWSTD_NO_BOOL


/********************** generic long functions ************************/

#    if   _RWSTD_LONG_SIZE == _RWSTD_INT_SIZE \
       && defined (_RWSTD_NO_LONG_ATOMIC_OPS) \
       && !defined (_RWSTD_NO_INT_ATOMIC_OPS)

#      undef _RWSTD_NO_LONG_ATOMIC_OPS

_RWSTD_NAMESPACE (__rw) {

inline long
__rw_atomic_preincrement (long &__x, bool)
{
    return __rw_atomic_preincrement (_RWSTD_REINTERPRET_CAST (int&, __x),
                                     false);
}

inline unsigned long
__rw_atomic_preincrement (unsigned long &__x, bool)
{
    return __rw_atomic_preincrement (_RWSTD_REINTERPRET_CAST (int&, __x),
                                     false);
}

inline long
__rw_atomic_predecrement (long &__x, bool)
{
    return __rw_atomic_predecrement (_RWSTD_REINTERPRET_CAST (int&, __x),
                                     false);
}

inline unsigned long
__rw_atomic_predecrement (unsigned long &__x, bool)
{
    return __rw_atomic_predecrement (_RWSTD_REINTERPRET_CAST (int&, __x),
                                     false);
}

inline long
__rw_atomic_exchange (long &__x, long __y, bool)
{
    return __rw_atomic_exchange (_RWSTD_REINTERPRET_CAST (int&, __x),
                                 _RWSTD_STATIC_CAST (int, __y),
                                 false);
}

inline unsigned long
__rw_atomic_exchange (unsigned long &__x,
                      unsigned long __y, bool)
{
    return __rw_atomic_exchange (_RWSTD_REINTERPRET_CAST (int&, __x),
                                 _RWSTD_STATIC_CAST (int, __y),
                                 false);
}

}   // namespace __rw

#    endif   // _RWSTD_LONG_SIZE == _RWSTD_INT_SIZE


/********************** generic long long functions *******************/

#    if   defined (_RWSTD_LONG_LONG)            \
       && _RWSTD_LLONG_SIZE == _RWSTD_LONG_SIZE \
       && defined (_RWSTD_NO_LLONG_ATOMIC_OPS)  \
       && !defined (_RWSTD_NO_LONG_ATOMIC_OPS)

#      undef _RWSTD_NO_LLONG_ATOMIC_OPS

_RWSTD_NAMESPACE (__rw) {
    
inline _RWSTD_LONG_LONG
__rw_atomic_preincrement (_RWSTD_LONG_LONG &__x, bool)
{
    return __rw_atomic_preincrement (_RWSTD_REINTERPRET_CAST (long&, __x),
                                     false);
}

inline unsigned _RWSTD_LONG_LONG
__rw_atomic_preincrement (unsigned _RWSTD_LONG_LONG &__x, bool)
{
    return __rw_atomic_preincrement (_RWSTD_REINTERPRET_CAST (long&, __x),
                                     false);
}

inline _RWSTD_LONG_LONG
__rw_atomic_predecrement (_RWSTD_LONG_LONG &__x, bool)
{
    return __rw_atomic_predecrement (_RWSTD_REINTERPRET_CAST (long&, __x),
                                     false);
}

inline unsigned _RWSTD_LONG_LONG
__rw_atomic_predecrement (unsigned _RWSTD_LONG_LONG &__x, bool)
{
    return __rw_atomic_predecrement (_RWSTD_REINTERPRET_CAST (long&, __x),
                                     false);
}

inline _RWSTD_LONG_LONG
__rw_atomic_exchange (_RWSTD_LONG_LONG &__x, _RWSTD_LONG_LONG __y, bool)
{
    return __rw_atomic_exchange (_RWSTD_REINTERPRET_CAST (long&, __x),
                                 _RWSTD_STATIC_CAST (long, __y),
                                 false);
}

inline unsigned _RWSTD_LONG_LONG
__rw_atomic_exchange (unsigned _RWSTD_LONG_LONG &__x,
                      unsigned _RWSTD_LONG_LONG __y, bool)
{
    return __rw_atomic_exchange (_RWSTD_REINTERPRET_CAST (long&, __x),
                                 _RWSTD_STATIC_CAST (long, __y),
                                 false);
}

}   // namespace __rw

#    endif   // _RWSTD_LONG_LONG && _RWSTD_LLONG_SIZE == _RWSTD_LONG_SIZE

#  endif   // _RWSTD_NO_ATOMIC_OPS

#else   // if !defined (_RWSTD_REENTRANT)

_RWSTD_NAMESPACE (__rw) {
    
// atomic in a single-threaded environment
template <class _TypeT, class _TypeU>
inline
_TypeT __rw_atomic_exchange (_TypeT &__t, const _TypeU &__u, bool)
{
    _TypeT __tmp = __t;
    __t = _TypeT (__u);
    return __tmp;
}

}   // namespace __rw

#endif   // _RWSTD_REENTRANT


_RWSTD_NAMESPACE (__rw) {

// available in all environments (ST and MT), used along with
// __rw_atomic_exchange<>() from conditional expressions in iostreams
template <class _TypeT, class _TypeU>
inline
_TypeT __rw_ordinary_exchange (_TypeT &__t, const _TypeU &__u)
{
    _TypeT __tmp = __t;
    __t = __u;
    return __tmp;
}

}   // namespace __rw


#endif   // _RWSTD_RW_ATOMIC_H_INCLUDED
