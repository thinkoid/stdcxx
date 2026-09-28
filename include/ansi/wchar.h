/***************************************************************************
 *
 * wchar.h
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
 * Copyright 1994-2006 Rogue Wave Software.
 * 
 **************************************************************************/

#include <rw/_defs.h>

#ifndef _RWSTD_NO_DEPRECATED_C_HEADERS
#  ifndef _RWSTD_WCHAR_H_INCLUDED
#    define _RWSTD_WCHAR_H_INCLUDED

#    include <cwchar>

#    ifndef _RWSTD_NO_NAMESPACE

#ifndef _RWSTD_NO_MBSTATE_T
using std::mbstate_t;
#endif
#ifndef _RWSTD_NO_WINT_T
using std::wint_t;
#endif
using std::size_t;
using std::btowc;
using std::fgetwc;
using std::fgetws;
using std::fputwc;
using std::fputws;
using std::fwprintf;
using std::fwscanf;
using std::getwc;
using std::getwchar;
using std::mbrlen;
using std::mbrtowc;
using std::mbsinit;
using std::mbsrtowcs;
using std::putwc;
using std::putwchar;
using std::swprintf;
using std::swscanf;
using std::ungetwc;
using std::vfwprintf;
using std::vwprintf;
using std::wcrtomb;
using std::wcscat;
using std::wcschr;
using std::wcscmp;
using std::wcscoll;
using std::wcscpy;
using std::wcscspn;
using std::wcslen;
using std::wcsncat;
using std::wcsncmp;
using std::wcsncpy;
using std::wcspbrk;
using std::wcsrchr;
using std::wcsrtombs;
using std::wcsspn;
using std::wcsstr;
using std::wcstod;
using std::wcstok;
using std::wcstol;
using std::wcstoul;
using std::wcsxfrm;
using std::wctob;
using std::wmemchr;
using std::wmemcmp;
using std::wmemcpy;
using std::wmemmove;
using std::wmemset;
using std::wprintf;
using std::wscanf;

#    endif   // _RWSTD_NO_NAMESPACE
#  endif   // _RWSTD_WCHAR_H_INCLUDED

#else   // if defined (_RWSTD_NO_DEPRECATED_C_HEADERS)

// glibc declares the C++ overloads of wcschr, wcspbrk, wcsrchr, wcsstr
// and wmemchr only when this is defined, and defines it only for GCC
#  ifndef __CORRECT_ISO_CPP_WCHAR_H_PROTO
#    define __CORRECT_ISO_CPP_WCHAR_H_PROTO
#  endif   // __CORRECT_ISO_CPP_WCHAR_H_PROTO

#  include _RWSTD_ANSI_C_WCHAR_H

#  include <rw/_mbstate.h>

#  ifndef WCHAR_MAX
#    define WCHAR_MAX   _RWSTD_WCHAR_MAX
#  endif

#  ifndef WCHAR_MIN
#    define WCHAR_MIN   _RWSTD_WCHAR_MIN
#  endif

extern "C" {

#  if defined (_RWSTD_NO_MBSTATE_T) && defined (_RWSTD_MBSTATE_T)
typedef _RWSTD_MBSTATE_T mbstate_t;
#  endif   // _RWSTD_NO_MBSTATE_T && _RWSTD_MBSTATE_T

}   // extern "C"

#endif   // _RWSTD_NO_DEPRECATED_C_HEADERS
