/***************************************************************************
 *
 * _mutex-pthread.h - definitions of classes for thread safety
 *                    using POSIX threads
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

// LinuxThreads man page:
//   "Variables of type pthread_mutex_t can also be initialized
//    statically, using the constants  PTHREAD_MUTEX_INITIALIZER
//    (for fast mutexes), PTHREAD_RECURSIVE_MUTEX_INITIALIZER_NP
//    (for recursive mutexes), and PTHREAD_ERRORCHECK_MUTEX_INI-
//    TIALIZER_NP (for error checking mutexes)."
//    ...
//    "Attempting to initialize an already initialized mutex results
//    in undefined behavior."

#include <pthread.h>

#define _RWSTD_MUTEX_INIT(mutex)      pthread_mutex_init (&mutex, 0)
#define _RWSTD_MUTEX_DESTROY(mutex)   pthread_mutex_destroy (&mutex)
#define _RWSTD_MUTEX_LOCK(mutex)      pthread_mutex_lock (&mutex)
#define _RWSTD_MUTEX_UNLOCK(mutex)    pthread_mutex_unlock (&mutex)
#define _RWSTD_MUTEX_T                pthread_mutex_t

#define _RWSTD_MUTEX_INITIALIZER PTHREAD_MUTEX_INITIALIZER
