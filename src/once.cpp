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


// _RWSTD_NO_ATOMIC_OPS, rather than _RWSTD_NO_INT_ATOMIC_OPS, which
// <rw/_mutex.h> undefines: the atomic backend defines it where int
// is not lock-free
#ifndef _RWSTD_NO_ATOMIC_OPS

// __rw_once tests the flag without the mutex first, with acquire;
// every store, made under the mutex, is a release store, so that what
// func wrote is visible to every thread that sees the flag done
static inline bool
__rw_once_is_done (int &state)
{
    return __rw_once_done == _RWSTD_ATOMIC_LOAD_ACQUIRE (state, false);
}

static inline void
__rw_once_store (int &state, int value)
{
    _RWSTD_ATOMIC_STORE_RELEASE (state, value, false);
}

#else   // if defined (_RWSTD_NO_ATOMIC_OPS)

// the atomic accesses to an int lock the static mutex of int, which
// is itself constructed through __rw_once where mutexes cannot be
// initialized statically; every access to the flag holds the mutex
static inline bool
__rw_once_is_done (int&)
{
    return false;
}

static inline void
__rw_once_store (int &state, int value)
{
    state = value;
}

#endif   // _RWSTD_NO_ATOMIC_OPS


// returns true to the caller elected to run func, after marking the
// flag running; false once func has returned in another thread
static bool
__rw_once_enter (int &state)
{
    const __rw_once_lock lock;

    for ( ; ; ) {

        // every store happens under the mutex, so a plain load under
        // it sees the latest
        switch (state) {

        case __rw_once_ready:
            __rw_once_store (state, __rw_once_running);
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

        __rw_once_store (state, next);
    }

    pthread_cond_broadcast (&__rw_once_cond);
}


_RWSTD_EXPORT void
__rw_once (__rw_once_t *once, void (*func)())
{
    _RWSTD_ASSERT (0 != once && 0 != func);

    int &state = once->_C_state;

    if (__rw_once_is_done (state) || !__rw_once_enter (state))
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
