/**************************************************************************
 *
 * assert.cpp - definition of debugging assertion functions
 *
 * $Id$
 *
 ***************************************************************************
 *
 * Licensed to the Apache Software  Foundation (ASF) under one or more
 * contributor  license agreements.  See  the NOTICE  file distributed
 * with  this  work  for  additional information  regarding  copyright
 * ownership.   The ASF  licenses this  file to  you under  the Apache
 * License, Version  2.0 (the  License); you may  not use  this file
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
 * Copyright 1994-2006 Rogue Wave Software, Inc.
 * 
 **************************************************************************/

#define _RWSTD_LIB_SRC


#include <stdio.h>    // for fprintf()
#include <stdlib.h>   // for abort()

#include <rw/_defs.h>


#if defined (__GLIBC__)

#  include <execinfo.h>

_RWSTD_NAMESPACE (__rw) {

// declare with attribute((nothrow)) since the function calls
// others that may not be declared nothrow
static void
__rw_stack_trace (int fd) _RWSTD_DECLARE_NOTHROW;


static void
__rw_stack_trace (int fd) _RWSTD_DEFINE_NOTHROW
{
    // limit stacktrace to the depth of 256 calls
    void* array [256];

    const int size = backtrace (array, int (sizeof array / sizeof *array));

    backtrace_symbols_fd (array, size, fd);
}

}   // namespace __rw

#  define STACK_TRACE()   _RW::__rw_stack_trace (2)

#endif

#ifndef STACK_TRACE
#  define STACK_TRACE()   (void)0
#endif   // !defined (STACK_TRACE)


_RWSTD_NAMESPACE (__rw) { 


_RWSTD_EXPORT void
__rw_assert_fail (const char *expr,
                  const char *file, int line, const char *func)
     _RWSTD_DEFINE_NOTHROW
{
    // func may be 0 if the compiler doesn't support the ANSI C predefined
    // identifier `__func__' (see 6.4.2.2 of ISO/IEC 9899:1999) or an
    // equivalent thereof, such as gcc's __PRETTY_FUNCTION__
    if (func)
        fprintf (stderr, "%s:%d: %s: Assertion '%s' failed.\n",
                 file, line, func, expr);
    else
        fprintf (stderr, "%s:%d: Assertion '%s' failed.\n",
                 file, line, expr);

    // print stack trace (if available) before aborting
    STACK_TRACE ();

    abort ();
}


}   // namespace __rw
