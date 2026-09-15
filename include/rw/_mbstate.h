/***************************************************************************
 *
 * _mbstate.h - definition of mbstate_t
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
 * Copyright 2005-2008 Rogue Wave Software, Inc.
 * 
 **************************************************************************/

#ifndef _RWSTD_RW_MBSTATE_H_INCLUDED
#define _RWSTD_RW_MBSTATE_H_INCLUDED

#ifndef _RWSTD_RW_DEFS_H_INCLUDED
#  include <rw/_defs.h>
#endif   // _RWSTD_RW_DEFS_H_INCLUDED


#if !defined (_RWSTD_NO_MBSTATE_T)
/*** not HP-UX that has a mbstate_t ***************************************/

#  if defined (_RWSTD_OS_LINUX)
/*** Linux/glibc **********************************************************/

     // glibc keeps the conversion state in the private __mbstate_t and
     // introduces the public mbstate_t as an alias for it in <wchar.h>
     // (and <uchar.h>); the two are guarded independently so that
     // <stdio.h> can see the former without introducing the latter
#    ifndef _RWSTD_NO_BITS_TYPES___MBSTATE_T_H

     // glibc 2.26 and beyond: the private type has a header of its own
#      include <bits/types/__mbstate_t.h>

#    else   // if defined (_RWSTD_NO_BITS_TYPES___MBSTATE_T_H)

     // earlier glibc: ask <wchar.h> for just the private type, the way
     // <stdio.h> does for fpos_t; <features.h> must come first since
     // the partial header relies on its macros without including it,
     // and the system header must be named by path since <wchar.h>
     // would resolve to our own wrapper
#      include <features.h>
#      define __need_mbstate_t
#      include _RWSTD_ANSI_C_WCHAR_H

#    endif   // _RWSTD_NO_BITS_TYPES___MBSTATE_T_H

#    define _RWSTD_MBSTATE_T __mbstate_t

#  elif defined (_RWSTD_OS_DARWIN)
/*** Apple Darwin/OS X ****************************************************/

     // include a system header for __mbstate_t
#    include <machine/_types.h>
#    define _RWSTD_MBSTATE_T   __mbstate_t

#  else   // !defined (_RWSTD_OS_LINUX)
/*** generic OS ***********************************************************/
     
#    include _RWSTD_CWCHAR
#    define _RWSTD_MBSTATE_T   _RWSTD_C::mbstate_t

#  endif   // defined (_RWSTD_OS_LINUX)

/*** not HP-UX that does not define mbstate_t *****************************/
#elif !defined (_RWSTD_MBSTATE_T_DEFINED)

#  define _RWSTD_MBSTATE_T_DEFINED

/*** SunOS 5.6 and prior **************************************************/

#  ifndef _RWSTD_NO_USING_LIBC_IN_STD

_RWSTD_NAMESPACE (std) {

#  endif   // _RWSTD_NO_USING_LIBC_IN_STD

extern "C" {

// generic definition of mbstate_t
struct mbstate_t
{
#  ifdef _RWSTD_MBSTATE_T_SIZE
    char _C_data [_RWSTD_MBSTATE_T_SIZE];
#  else
    char _C_data [sizeof (long)];
#  endif

};

}   // extern "C"

#  ifndef _RWSTD_NO_USING_LIBC_IN_STD

}   // namespace std

#  endif   // _RWSTD_NO_USING_LIBC_IN_STD

#  define _RWSTD_MBSTATE_T   _STD::mbstate_t

#endif   // _RWSTD_NO_MBSTATE_T && !_RWSTD_MBSTATE_T_DEFINED


#endif   // _RWSTD_RW_MBSTATE_H_INCLUDED
