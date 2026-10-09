/***************************************************************************
 *
 * once.cpp - one time initialization
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
 * Copyright 2007 Rogue Wave Software.
 *
 **************************************************************************/

#define _RWSTD_LIB_SRC
#include <rw/_defs.h>
#include <rw/_mutex.h>
#include <rw/_once.h>


_RWSTD_NAMESPACE (__rw) {


// the values of __rw_once_t::_C_state
enum {
    __rw_once_ready,     // func has not run, or exited by an exception
    __rw_once_running,   // func is running
    __rw_once_done       // func has returned
};


#ifdef _RWSTD_REENTRANT


// one mutex and one condition variable serve every flag; the mutex is
// held while a flag changes state, never while func runs, so a func
// may itself initialize through another flag
static pthread_mutex_t
__rw_once_mutex = PTHREAD_MUTEX_INITIALIZER;

static pthread_cond_t
__rw_once_cond = PTHREAD_COND_INITIALIZER;


// releases the mutex on every exit, including the unwinding of a
// thread cancelled while it waits
struct __rw_once_lock
{
    __rw_once_lock () {
        pthread_mutex_lock (&__rw_once_mutex);
    }

    ~__rw_once_lock () {
        pthread_mutex_unlock (&__rw_once_mutex);
    }
};


// returns true to the caller elected to run func, after marking the
// flag running; false once func has returned in another thread
static bool
__rw_once_enter (int &state)
{
    const __rw_once_lock lock;

    for ( ; ; ) {

        // every store happens under the mutex, so a relaxed load
        // under it sees the latest
        switch (_RWSTD_ATOMIC_LOAD_RELAXED (state, false)) {

        case __rw_once_ready:
            _RWSTD_ATOMIC_STORE_RELAXED (state, __rw_once_running, false);
            return true;

        case __rw_once_done:
            return false;
        }

        pthread_cond_wait (&__rw_once_cond, &__rw_once_mutex);
    }
}


// leaves the flag done or ready again and wakes the waiters; the
// store happens under the mutex, or a waiter that has just read
// running could miss the broadcast
static void
__rw_once_leave (int &state, int next)
{
    {
        const __rw_once_lock lock;

        // the release pairs with the acquire of the unlocked test in
        // __rw_once: what func wrote is visible to every thread that
        // sees the flag done
        _RWSTD_ATOMIC_STORE_RELEASE (state, next, false);
    }

    pthread_cond_broadcast (&__rw_once_cond);
}


_RWSTD_EXPORT void
__rw_once (__rw_once_t *once, void (*func)())
{
    _RWSTD_ASSERT (0 != once && 0 != func);

    int &state = once->_C_state;

    if (   __rw_once_done == _RWSTD_ATOMIC_LOAD_ACQUIRE (state, false)
        || !__rw_once_enter (state))
        return;

    _TRY {
        func ();
    }
    _CATCH (...) {
        __rw_once_leave (state, __rw_once_ready);
        _RETHROW;
    }

    __rw_once_leave (state, __rw_once_done);
}


#else   // if !defined (_RWSTD_REENTRANT)


_RWSTD_EXPORT void
__rw_once (__rw_once_t *once, void (*func)())
{
    _RWSTD_ASSERT (0 != once && 0 != func);

    // a func that exits by an exception leaves the flag ready
    if (__rw_once_done != once->_C_state) {
        func ();
        once->_C_state = __rw_once_done;
    }
}


#endif   // _RWSTD_REENTRANT

}   // namespace __rw
