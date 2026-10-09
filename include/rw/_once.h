/***************************************************************************
 *
 * _once.h - one time initialization helpers
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
 * License, Version  2.0 (the License); you may  not use  this file
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
 * Copyright 2007 Rogue Wave Software, Inc.
 *
 **************************************************************************/

#ifndef _RWSTD_RW_ONCE_H_INCLUDED
#define _RWSTD_RW_ONCE_H_INCLUDED


#ifndef _RWSTD_RW_DEFS_H_INCLUDED
#  include <rw/_defs.h>
#endif   // _RWSTD_RW_DEFS_H_INCLUDED


_RWSTD_NAMESPACE (__rw) {

// the state of a one-time initialization: valid when zero-initialized,
// so a flag with static storage duration needs no initializer and is
// ready before any constructor runs
struct __rw_once_t { int _C_state; };

#define _RWSTD_ONCE_INIT   { 0 }

// calls func unless a call through the same flag has returned; callers
// that find the call in progress wait for it to return. If func exits
// by an exception, the flag is left as it was found, the exception
// propagates, and one of the waiters calls func in turn. A call from
// func through the same flag never returns.
_RWSTD_EXPORT void
__rw_once (__rw_once_t*, void (*)());

}   // namespace __rw


#endif   // _RWSTD_RW_ONCE_H_INCLUDED
