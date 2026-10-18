/***************************************************************************
 *
 * _config.h - Compiler and C library configuration definitions
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
 * Copyright 1994-2007 Rogue Wave Software, Inc.
 * 
 **************************************************************************/

#ifndef _RWSTD_RW_CONFIG_H_INCLUDED
#define _RWSTD_RW_CONFIG_H_INCLUDED

// include generated configuration header
#include <config.h>

/*** library version numbers and ids **************************************/

#define _RWSTD_VER 0x04020200
//                    | | | |
//                   MMmmuupp
//                    | | | |
//                    | | | +--- pp = patch version number
//                    | | +----- uu = micro version number
//                    | +------- mm = minor version number
//                    +--------- MM = major version number

// library version string (patch number included only if non-zero)
#define _RWSTD_VER_STR   "4.2.2"

// library version numbers
#define _RWSTD_VER_MAJOR ((_RWSTD_VER >> 24) & 0xff)
#define _RWSTD_VER_MINOR ((_RWSTD_VER >> 16) & 0xff)
#define _RWSTD_VER_MICRO ((_RWSTD_VER >>  8) & 0xff)
#define _RWSTD_VER_PATCH (_RWSTD_VER & 0xff)

// a newer release of stdcxx is backward source and binary compatible
// with an older release if both releases share the same major version
// number; stdcxx releases whose major version numbers differ may or
// may not be source or binary compatible

// two releases of stdcxx are forward (as well as backward) source and
// binary compatible if both releases share the same major and minor
// version numbers

// backward compatibility means that a program linked with an older
// release of a library will not be adversely affected by upgrading
// to a more recent release of the same library

// forward compatibility means that a program linked with a newer
// release of a library will not be adversely affected by downgrading
// to an older release of the same library

/**************************************************************************
 * OVERRIDES FOR CONFIGURATION MACROS                                     *
 **************************************************************************/

// -D _REENTRANT or any one of the other thread safety macros
// used in the conditional below turns on thread safety features
#if   defined (_REENTRANT)            \
    || defined (_THREAD_SAFE)         \
    || defined (_RWSTD_MULTI_THREAD)  \
    || defined (_RWSTD_POSIX_THREADS)
#  ifndef _RWSTD_REENTRANT
#    define _RWSTD_REENTRANT
#  endif   // _RWSTD_REENTRANT
#endif

/**************************************************************************
 * COMPILER-SPECIFIC OVERRIDES                                            * 
 **************************************************************************/

/*** GCC ******************************************************************/

#ifdef __GNUG__
#  include "_config-gcc.h"
#else   // if !defined (__GNUG__)
#  define _RWSTD_GNUC_ATTRIBUTE(ignore)
#endif   // __GNUG__

/**************************************************************************
 * OPERATING SYSTEM-SPECIFIC OVERRIDES                                    * 
 **************************************************************************/

#define _RWSTD_PATH_SEP      '/'

/*** Non-10646 platforms **************************************************/

#ifndef _RWSTD_OS_LINUX
   // Linux glibc uses ISO 10646 (Unicode) as the internal
   // wchar_t encoding in all locales
#  ifndef _RWSTD_NO_ISO_10646_WCHAR_T
#    define _RWSTD_NO_ISO_10646_WCHAR_T
#  endif   // _RWSTD_NO_ISO_10646_WCHAR_T
#endif   // !_RWSTD_OS_LINUX

/********************** Threads *******************************************/

#ifdef _RWSTD_REENTRANT
#  if !defined (_RWSTD_POSIX_THREADS)

   // default to POSIX threads except on Win32 or Win64

#    define _RWSTD_POSIX_THREADS
#  endif   // _RWSTD_*_THREADS
#endif   // _RWSTD_REENTRANT

/********************** Miscellaneous *************************************/

// g++ cannot inline functions that take a variable number of arguments
// or functions that contain static (local) variables
#if !defined (__GNUG__) || __GNUG__ > 2 || __GNUG_MINOR__ > 96
#  define _INLINE_VARARGS      inline
#  define _INLINE_WITH_STATICS inline
#endif   // !__GNUG__ || __GNUG__ > 2 || __GNUG_MINOR__ > 96

#ifdef _RWSTD_NO_COLLAPSE_TEMPLATE_STATICS
   // static mutex initialization depends on the compiler's (and the
   // linker's in gcc's case) ability to correctly handle explicitly
   // initialized static members of class templates
#  ifndef _RWSTD_NO_STATIC_MUTEX_INIT
#    define _RWSTD_NO_STATIC_MUTEX_INIT
#  endif
#endif   // _RWSTD_NO_COLLAPSE_TEMPLATE_STATICS

/********************** Environment ***************************************/

#if defined (_RWSTD_NO_WCHAR_T) && !defined (_RWSTD_NO_NATIVE_WCHAR_T)
#  define _RWSTD_NO_NATIVE_WCHAR_T
#endif


#ifndef _RWSTD_NO_STL_SPECIALIZATION
// #ifdef _RWSTD_NO_CLASS_PARTIAL_SPEC
// disable partial specialization for void* of STL sequences
#  define _RWSTD_NO_STL_SPECIALIZATION
// #endif   // _RWSTD_NO_CLASS_PARTIAL_SPEC
#endif   // _RWSTD_NO_STL_SPECIALIZATION

// 
// Macro for path to the ANSI 'C' headers 
// Must be set specifically for each platform when the
// C++ wrappers for 'C' headers are used.
//
#define _RWSTD_ANSIC(x) </usr/include/x>

// the default long double printf format prefix is "L" (unless #defined above)
#ifndef _RWSTD_LDBL_PRINTF_PREFIX
#  define _RWSTD_LDBL_PRINTF_PREFIX   "L"
#endif   // _RWSTD_LDBL_PRINTF_PREFIX

// the default long double scanf format prefix is "L" (unless #defined above)
#ifndef _RWSTD_LDBL_SCANF_PREFIX
#  define _RWSTD_LDBL_SCANF_PREFIX   "L"
#endif   // _RWSTD_LDBL_SCANF_PREFIX

// the default expansion of dllimport is empty (unless #defined above)
#ifndef _RWSTD_DLLIMPORT
#  define _RWSTD_DLLIMPORT   /* empty */
#endif   // _RWSTD_DLLIMPORT

// the default name of a libc symbol is itself (unless #defined above)
#ifndef _RWSTD_LIBC_SYM
#  define _RWSTD_LIBC_SYM(name)   name
#endif   // _RWSTD_LIBC_SYM

/********************** Library Option Dependencies ***********************/

/*
 * The following macro sets the default size of file stream internal buffers
 */

#ifndef _RWSTD_DEFAULT_BUFSIZE         // specified on command line?
#  define _RWSTD_DEFAULT_BUFSIZE   512
#endif

#ifndef _RWSTD_NO_LONG_LONG
#  ifndef _RWSTD_LONG_LONG
#    define _RWSTD_LONG_LONG long long
#  endif   // _RWSTD_LONG_LONG
#endif   // _RWSTD_NO_LONG_LONG


   // macros that are enabled in library source files
#ifdef _RWSTD_LIB_SRC
   // don't instantiate templates in library source code
   //  (but see _RWSTD_INSTANTIATE_TEMPLATES macro below) 
#  define _RWSTD_NO_TEMPLATE_DEFINITIONS
#endif  // _RWSTD_LIB_SRC

#ifdef _RWSTD_INSTANTIATE_TEMPLATES
   // instantiate templates (this macro is defined at the top of each
   // library source file (.cpp file) that explicitly instantiates class
   // or function templates using the _RWSTD_INSTANTIATE_N() macros
#  undef _RWSTD_NO_TEMPLATE_DEFINITIONS
#endif


#endif   // _RWSTD_RW_CONFIG_H_INCLUDED
