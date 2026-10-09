/************************************************************************
 *
 * once.cpp - test exercising the __rw_once() function
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
 **************************************************************************/

#include <cstddef>       // for size_t

#include <rw/_mutex.h>   // for _RWSTD_ATOMIC_PREINCREMENT()
#include <rw/_once.h>    // for __rw_once()

#include <rw_thread.h>
#include <rw_driver.h>


// maximum number of threads allowed by the command line interface
#define MAX_THREADS      32

// the number of threads racing each round (at most 8 by default,
// fewer on machines with fewer processors)
int opt_nthreads = 1;

// default timeout used by each threaded section of this test
int opt_timeout = 60;

/**************************************************************************/

// the number of rounds, one flag each; the flags are zero-initialized,
// as a flag with static storage duration may be
#define NROUNDS   200

static _RW::__rw_once_t flags [NROUNDS];

// the data each round's initializer publishes
static int data [NROUNDS];

// the round being raced
static int race_round;

// whether the round's first call to the initializer throws
static bool race_throws;

// the number of threads that have started in the current round
static int nstarted;

// the number of threads about to call __rw_once in the current round
static int nentered;

// the number of calls to the initializer in the current round
static int ncalls;

// the number of exceptions the threads caught in the current round
static int nthrown;

// the number of threads that returned from __rw_once and did not see
// the initializer's data
static int nmissed;


// the initializer: holds on until every thread is about to call
// __rw_once, so that the others find the call in progress and wait
static void
init_func ()
{
    const int call = _RWSTD_ATOMIC_PREINCREMENT (ncalls, false);

    while (_RWSTD_ATOMIC_LOAD_RELAXED (nentered, false) < opt_nthreads);

#ifndef _RWSTD_NO_EXCEPTIONS

    if (race_throws && 1 == call)
        throw call;

#endif   // _RWSTD_NO_EXCEPTIONS

    data [race_round] = race_round + 1;
}


extern "C" {

static void*
race_func (void*)
{
    _RWSTD_ATOMIC_PREINCREMENT (nstarted, false);

    // spin until all threads have been created so that they find
    // the flag unset at the same time
    while (_RWSTD_ATOMIC_LOAD_RELAXED (nstarted, false) < opt_nthreads);

    _RWSTD_ATOMIC_PREINCREMENT (nentered, false);

    for ( ; ; ) {

#ifndef _RWSTD_NO_EXCEPTIONS

        try {
            _RW::__rw_once (flags + race_round, init_func);
            break;
        }
        catch (int) {
            // the thread whose call threw calls again, as any
            // caller of a failed initialization may
            _RWSTD_ATOMIC_PREINCREMENT (nthrown, false);
        }

#else   // if defined (_RWSTD_NO_EXCEPTIONS)

        _RW::__rw_once (flags + race_round, init_func);
        break;

#endif   // _RWSTD_NO_EXCEPTIONS

    }

    // a plain read: the flag, not this test, orders it after the
    // initializer's write
    if (race_round + 1 != data [race_round])
        _RWSTD_ATOMIC_PREINCREMENT (nmissed, false);

    return 0;
}

}   // extern "C"

/**************************************************************************/

static int
run_test (int, char**)
{
    rw_info (0, 0, 0,
             "racing the first call through a flag from %d thread%{?}s%{;}, "
             "%d flags",
             opt_nthreads, 1 != opt_nthreads, NROUNDS);

    int result = 0;

    for (int i = 0; 0 == result && i != NROUNDS; ++i) {

#ifndef _RWSTD_NO_EXCEPTIONS
        // every other round, the first call to the initializer throws
        race_throws = 0 != i % 2;
#endif   // _RWSTD_NO_EXCEPTIONS

        race_round = i;
        nstarted   = 0;
        nentered   = 0;
        ncalls     = 0;
        nthrown    = 0;
        nmissed    = 0;

        result = rw_thread_pool (0, std::size_t (opt_nthreads), 0,
                                 race_func, 0,
                                 std::size_t (opt_timeout));

        rw_error (result == 0, 0, __LINE__,
                  "rw_thread_pool(0, %d, 0, %{#f}, 0) failed",
                  opt_nthreads, race_func);

        // a call that throws leaves the flag unset, and the next call
        // runs the initializer again
        const int ncalls_expect = 1 + race_throws;

        rw_assert (ncalls_expect == ncalls, 0, __LINE__,
                   "round %d: the initializer called %d times, expected "
                   "%d, from %d thread%{?}s%{;}",
                   i, ncalls, ncalls_expect,
                   opt_nthreads, 1 != opt_nthreads);

        rw_assert (int (race_throws) == nthrown, 0, __LINE__,
                   "round %d: %d exceptions caught, expected %d",
                   i, nthrown, int (race_throws));

        rw_assert (0 == nmissed, 0, __LINE__,
                   "round %d: %d thread%{?}s%{;} of %d returned from "
                   "__rw_once() before the initializer's data was visible",
                   i, nmissed, 1 != nmissed, opt_nthreads);
    }

    return result;
}

/**************************************************************************/

int main (int argc, char *argv[])
{
#ifdef _RWSTD_REENTRANT

    // race the greater of 2 and the number of processors, at most 8:
    // contention comes from threads released together, not from more
    // of them
    opt_nthreads = rw_get_cpus ();
    if (opt_nthreads < 2)
        opt_nthreads = 2;
    else if (8 < opt_nthreads)
        opt_nthreads = 8;

#endif   // _RWSTD_REENTRANT

    return rw_test (argc, argv, __FILE__,
                    0 /* no clause */,
                    0 /* no comment */,
                    run_test,
                    "|-soft-timeout#0 "  // must be non-negative
                    "|-nthreads#0-* ",   // must be in [0, MAX_THREADS]
                    &opt_timeout,
                    int (MAX_THREADS),
                    &opt_nthreads);
}
