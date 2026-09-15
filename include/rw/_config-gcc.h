/***************************************************************************
 *
 * _config-gcc.h - GCC configuration definitions
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

// _REENTRANT is defined by the -pthread compiler option

#ifndef _RWSTD_NO_IMPLICIT_INCLUSION
#  define _RWSTD_NO_IMPLICIT_INCLUSION
#endif

#ifndef _RWSTD_NO_EXPORT
     // not implemented
#  define _RWSTD_NO_EXPORT
#endif

#ifndef __EXCEPTIONS
   // disable exceptions when the macro __EXCEPTIONS
   // is not #defined by the compiler, e.g., when
   // the -fno-exceptions option is used
#  ifndef _RWSTD_NO_EXCEPTIONS
#    define _RWSTD_NO_EXCEPTIONS
#  endif   // _RWSTD_NO_EXCEPTIONS
#endif   // __EXCEPTIONS

#if !defined (_RWSTD_USE_PURE_C_HEADERS)
     // disabled except when requested
#  define _RWSTD_NO_PURE_C_HEADERS
#endif   // !_RWSTD_USE_PURE_C_HEADERS

#ifdef _RWSTD_NO_PURE_C_HEADERS
     // deprecated C headers (the <xxx.h> kind) are fully implemented
     // in terms of the pure C++ C library headers (the <cxxx> kind)
#  define _RWSTD_NO_DEPRECATED_C_HEADERS
#endif   // _RWSTD_NO_PURE_C_HEADERS

#if !defined (_RWSTD_STRICT_ANSI) && __GNUG__ <= 2 && __GNUC_MINOR__ < 97
     // prevent gcc 2.9x ICE
#  define _RWSTD_NO_PLACEMENT_DELETE
#endif   // !_RWSTD_STRICT_ANSI

#if __GNUG__ < 3
     // gcc 2.x doesn't understand the extern template syntax
     // but only issues a warning: ANSI C++ forbids the use
     // of `extern' on explicit instantiations
#  ifndef _RWSTD_NO_EXTERN_TEMPLATE
#    define _RWSTD_NO_EXTERN_TEMPLATE
#  endif   // _RWSTD_NO_EXTERN_TEMPLATE
#endif   // gcc 2.x

#if __GNUG__ <= 2 && __GNUC_MINOR__ < 97 && defined (_RWSTD_NO_HONOR_STD)
     // standard iostream objects are declared as objects of their respective
     // types by defined as POD's to prevent their destruction during program
     // lifetime (done to work around a g++ 2.95.2 bug that prevents g++
     // from deducing the type of references to the objects in template code)
#  define _RWSTD_NO_IOSTREAM_OBJECT_REFS
#endif   //__GNUG__ <= 2 && __GNUC_MINOR__ < 97 && _RWSTD_NO_HONOR_STD

#define _RWSTD_GNUC_ATTRIBUTE(attr)   __attribute__ (attr)

// gcc attribute((noreturn)) indicates that a function doesn't return
// (it may still exit by throwing an exception or by calling longjmp)
#define _RWSTD_NORETURN __attribute__ ((noreturn))

#if __GNUG__ > 3 || __GNUG__ == 3 && __GNUC_MINOR__ >= 3
  // gcc attribute((nothrow)) to indicate that a function doesn't throw
  // exceptions; unlike the emtpy exception specification the attribute
  // avoids the cost of checking for exceptions and calling unexpected()
#  define _RWSTD_ATTRIBUTE_NOTHROW   _RWSTD_GNUC_ATTRIBUTE ((nothrow))
#endif   // gcc >= 3.3

#ifdef _RWSTD_OS_LINUX

#  ifdef _RWSTD_NO_NEW_HEADER
#    undef _RWSTD_NO_NEW_HEADER
#  endif   // _RWSTD_NO_NEW_HEADER

     // our <cxxx> libc headers put the libc functions in namespace std
#  ifdef _RWSTD_NO_LIBC_IN_STD
#    undef _RWSTD_NO_LIBC_IN_STD
#  endif   // _RWSTD_NO_LIBC_IN_STD

#  ifdef _RWSTD_ANSI_C_LIMITS_H
       // prevent system header warnings
#    undef _RWSTD_ANSI_C_LIMITS_H
#    define _RWSTD_ANSI_C_LIMITS_H <../include/limits.h>
#  endif  // _RWSTD_ANSI_C_LIMITS_H
#endif   // _RWSTD_OS_LINUX

   // force using /usr/include/math.h 
   // prevent recursion caused by pulling in gcc's own "fixed" header
#undef _RWSTD_ANSI_C_MATH_H

   // use new C++ libc headers
#undef _RWSTD_NO_NEW_HEADER

#undef _RWSTD_NO_LIBC_IN_STD

#undef _RWSTD_NO_DEPRECATED_LIBC_IN_STD
