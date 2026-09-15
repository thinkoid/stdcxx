/***************************************************************************
 *
 * _atomic-x86.h - definitions of inline functions for atomic
 *                 operations on Intel X86 platform
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

#define _RWSTD_NO_LONG_ATOMIC_OPS
#define _RWSTD_NO_LLONG_ATOMIC_OPS


_RWSTD_NAMESPACE (__rw) {

extern "C" {

_RWSTD_EXPORT char __rw_atomic_add8 (char*, int);
_RWSTD_EXPORT short __rw_atomic_add16 (short*, short);
_RWSTD_EXPORT int __rw_atomic_add32 (int*, int);

_RWSTD_EXPORT char __rw_atomic_xchg8 (char*, char);
_RWSTD_EXPORT short __rw_atomic_xchg16 (short*, short);
_RWSTD_EXPORT int __rw_atomic_xchg32 (int*, int);

}   // extern "C"


inline char
__rw_atomic_preincrement (char &__x, bool)
{
    _RWSTD_COMPILE_ASSERT (1 == sizeof (char));
    return __rw_atomic_add8 (&__x, +1);
}


inline signed char
__rw_atomic_preincrement (signed char &__x, bool)
{
    _RWSTD_COMPILE_ASSERT (1 == sizeof (signed char));
    return __rw_atomic_add8 (_RWSTD_REINTERPRET_CAST (char*, &__x), +1);
}


inline unsigned char
__rw_atomic_preincrement (unsigned char &__x, bool)
{
    _RWSTD_COMPILE_ASSERT (1 == sizeof (unsigned char));
    return __rw_atomic_add8 (_RWSTD_REINTERPRET_CAST (char*, &__x), +1);
}


inline short
__rw_atomic_preincrement (short &__x, bool)
{
    _RWSTD_COMPILE_ASSERT (2 == sizeof (short));

    return __rw_atomic_add16 (&__x, +1);
}


inline unsigned short
__rw_atomic_preincrement (unsigned short &__x, bool)
{
    _RWSTD_COMPILE_ASSERT (2 == sizeof (unsigned short));

    return __rw_atomic_add16 (_RWSTD_REINTERPRET_CAST (short*, &__x), +1);
}


inline int
__rw_atomic_preincrement (int &__x, bool)
{
    _RWSTD_COMPILE_ASSERT (4 == sizeof (int));

    return __rw_atomic_add32 (&__x, 1);
}


inline unsigned int
__rw_atomic_preincrement (unsigned int &__x, bool)
{
    _RWSTD_COMPILE_ASSERT (4 == sizeof (unsigned int));

    return __rw_atomic_add32 (_RWSTD_REINTERPRET_CAST (int*, &__x), 1);
}


inline char
__rw_atomic_predecrement (char &__x, bool)
{
    _RWSTD_COMPILE_ASSERT (1 == sizeof (char));
    return __rw_atomic_add8 (&__x, -1);
}


inline signed char
__rw_atomic_predecrement (signed char &__x, bool)
{
    _RWSTD_COMPILE_ASSERT (1 == sizeof (signed char));
    return __rw_atomic_add8 (_RWSTD_REINTERPRET_CAST (char*, &__x), -1);
}


inline unsigned char
__rw_atomic_predecrement (unsigned char &__x, bool)
{
    _RWSTD_COMPILE_ASSERT (1 == sizeof (unsigned char));
    return __rw_atomic_add8 (_RWSTD_REINTERPRET_CAST (char*, &__x), -1);
}


inline short
__rw_atomic_predecrement (short &__x, bool)
{
    _RWSTD_COMPILE_ASSERT (2 == sizeof (short));

    return __rw_atomic_add16 (&__x, -1);
}


inline unsigned short
__rw_atomic_predecrement (unsigned short &__x, bool)
{
    _RWSTD_COMPILE_ASSERT (2 == sizeof (unsigned short));

    return __rw_atomic_add16 (_RWSTD_REINTERPRET_CAST (short*, &__x), -1);
}


inline int
__rw_atomic_predecrement (int &__x, bool)
{
    _RWSTD_COMPILE_ASSERT (4 == sizeof (int));

    return __rw_atomic_add32 (&__x, -1);
}


inline unsigned int
__rw_atomic_predecrement (unsigned int &__x, bool)
{
    _RWSTD_COMPILE_ASSERT (4 == sizeof (unsigned int));

    return __rw_atomic_add32 (_RWSTD_REINTERPRET_CAST (int*, &__x), -1);
}


inline char
__rw_atomic_exchange (char &__x, char __y, bool)
{
    _RWSTD_COMPILE_ASSERT (1 == sizeof (char));
    return __rw_atomic_xchg8 (&__x, __y);
}


inline signed char
__rw_atomic_exchange (signed char &__x, signed char __y, bool)
{
    _RWSTD_COMPILE_ASSERT (1 == sizeof (signed char));
    return __rw_atomic_xchg8 (_RWSTD_REINTERPRET_CAST (char*, &__x),
                              _RWSTD_STATIC_CAST (char, __y));
}


inline unsigned char
__rw_atomic_exchange (unsigned char &__x, unsigned char __y, bool)
{
    _RWSTD_COMPILE_ASSERT (1 == sizeof (unsigned char));
    return __rw_atomic_xchg8 (_RWSTD_REINTERPRET_CAST (char*, &__x),
                              _RWSTD_STATIC_CAST (char, __y));
}


inline short
__rw_atomic_exchange (short &__x, short __y, bool)
{
    _RWSTD_COMPILE_ASSERT (2 == sizeof (short));
    return __rw_atomic_xchg16 (&__x, __y);
}


inline unsigned short
__rw_atomic_exchange (unsigned short &__x, unsigned short __y, bool)
{
    _RWSTD_COMPILE_ASSERT (2 == sizeof (unsigned short));
    return __rw_atomic_xchg16 (_RWSTD_REINTERPRET_CAST (short*, &__x),
                               _RWSTD_STATIC_CAST (short, __y));
}


inline int
__rw_atomic_exchange (int &__x, int __y, bool)
{
    _RWSTD_COMPILE_ASSERT (4 == sizeof (int));

    return __rw_atomic_xchg32 (&__x, __y);
}


inline unsigned int
__rw_atomic_exchange (unsigned int &__x, unsigned int __y, bool)
{
    _RWSTD_COMPILE_ASSERT (4 == sizeof (unsigned int));

    return __rw_atomic_xchg32 (_RWSTD_REINTERPRET_CAST (int*, &__x),
                               _RWSTD_STATIC_CAST (int, __y));
}

}   // namespace __rw
