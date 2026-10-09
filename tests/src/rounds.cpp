/************************************************************************
 *
 * rounds.cpp - definitions of the first-use rounds driver
 *
 * $Id$
 *
 ************************************************************************
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

// expand _TEST_EXPORT macros
#define _RWSTD_TEST_SRC

#include <rw_rounds.h>

#include <rw_locale.h>   // for rw_localedef(), rw_set_locale_root()
#include <rw_thread.h>   // for rw_thread_pool()
#include <rw_driver.h>   // for rw_assert(), ...

#include <exception>     // for exception
#include <locale>        // for locale

#include <clocale>       // for setlocale(), LC_ALL
#include <cstring>       // for strcpy(), strncpy()

/**************************************************************************/

_TEST_EXPORT int rw_round_nthreads = 4;
_TEST_EXPORT int rw_round_nrounds  = 64;
_TEST_EXPORT int rw_round_timeout  = 5;

/**************************************************************************/

#ifdef _RWSTD_REENTRANT

// the locale whose facets the threads of the current round use
static const std::locale* _rw_round_locale;

// the test's work and the blocks the threads record into
static rw_round_work_t* _rw_round_work;
static char*            _rw_round_slots;
static std::size_t      _rw_round_size;

// whether the current round is gated, and the arrivals at its gates
static bool         _rw_round_gated;
static volatile int _rw_round_arrived;

// what a thread threw, if anything: 1 for a std::exception, 2 otherwise
struct _rw_round_exception
{
    int  threw;
    char what [256];
};

static _rw_round_exception _rw_round_threw [RW_ROUND_MAX_THREADS];


static void
_rw_round_call (const std::locale &loc, void *block,
                _rw_round_exception &ex)
{
    ex.threw = 0;

    try {
        _rw_round_work (loc, block);
    }
    catch (const std::exception &e) {
        ex.threw = 1;
        std::strncpy (ex.what, e.what (), sizeof ex.what - 1);
        ex.what [sizeof ex.what - 1] = '\0';
    }
    catch (...) {
        ex.threw = 2;
    }
}


extern "C" {

static void*
_rw_round_thread (void *arg)
{
    const long inx = long (arg);

    _rw_round_call (*_rw_round_locale,
                    _rw_round_slots + inx * _rw_round_size,
                    _rw_round_threw [inx]);

    return 0;
}

}   // extern "C"

#endif   // _RWSTD_REENTRANT

/**************************************************************************/

_TEST_EXPORT void
rw_round_gate ()
{
#ifdef _RWSTD_REENTRANT

    if (!_rw_round_gated)
        return;

    // cast the volatile counter to int& (see STDCXX-792)
    int &count = _RWSTD_CONST_CAST (int&, _rw_round_arrived);

    // every thread passes every gate, so the count of the gate this
    // arrival belongs to ends at the next multiple of the pool size
    const int arrival = _RWSTD_ATOMIC_PREINCREMENT (count, false);
    const int last    = (arrival + rw_round_nthreads - 1)
                      / rw_round_nthreads * rw_round_nthreads;

    // a thread that threw arrives no more; the soft timeout frees
    // the others, and the round fails on the exception
    while (   _RWSTD_ATOMIC_LOAD_RELAXED (count, false) < last
           && !rw_thread_pool_timeout_expired ());

#endif   // _RWSTD_REENTRANT
}

/**************************************************************************/

_TEST_EXPORT int
rw_first_use_rounds (const char* const    sources [][2],
                     std::size_t          nsources,
                     void                *expected,
                     void                *slots,
                     std::size_t          size,
                     rw_round_prepare_t  *prepare,
                     rw_round_work_t     *work,
                     rw_round_check_t    *check,
                     const char          *what,
                     bool                 alternate)
{
#ifdef _RWSTD_REENTRANT

    rw_fatal (nsources <= RW_ROUND_MAX_LOCALES, 0, __LINE__,
              "%s: %zu locale sources, at most %d allowed",
              what, nsources, int (RW_ROUND_MAX_LOCALES));

    _rw_round_work  = work;
    _rw_round_slots = _RWSTD_STATIC_CAST (char*, slots);
    _rw_round_size  = size;
    _rw_round_gated = false;

    rw_set_locale_root ();

    static char names [RW_ROUND_MAX_LOCALES][64];

    // build the locales and take the expected blocks serially, each
    // from a locale that dies before the rounds: a live one would keep
    // its facets for the rounds' locales of the same name to share
    for (std::size_t i = 0; i != nsources; ++i) {

        const char* const locname =
            rw_localedef ("-w", sources [i][0], sources [i][1], 0);

        if (0 == locname) {
            rw_warn (false, 0, __LINE__,
                     "failed to build locale %s.%s; skipping the test",
                     sources [i][0], sources [i][1]);
            return 0;
        }

        std::strcpy (names [i], locname);

        // a thread that finds a facet's data missing falls back to the
        // C library, whose answer for a name it knows would be the right
        // one; the test can tell the two apart only for a name it does not
        if (std::setlocale (LC_ALL, names [i])) {
            std::setlocale (LC_ALL, "C");
            rw_note (false, 0, __LINE__,
                     "the C library knows %#s; skipping the test", names [i]);
            return 0;
        }

        _rw_round_exception ex;

        _rw_round_call (std::locale (names [i]),
                        _RWSTD_STATIC_CAST (char*, expected) + i * size, ex);

        rw_fatal (0 == ex.threw, 0, __LINE__,
                  "%s threw in %#s in a single thread: %s",
                  what, names [i], ex.what);
    }

    rw_info (0, 0, 0,
             "exercising the first use of %s from %d thread%{?}s%{;}, "
             "%d locale%{?}s%{;}%{?}, alternately gated%{;}",
             what, rw_round_nthreads, 1 != rw_round_nthreads,
             rw_round_nrounds, 1 != rw_round_nrounds, alternate);

    int result = 0;

    for (int i = 0; 0 == result && i != rw_round_nrounds; ++i) {

        const std::size_t inx     = std::size_t (i) % nsources;
        const char* const locname = names [inx];

        // each round a new locale, so that its facets and their data
        // are constructed anew
        const std::locale loc (locname);

        prepare (loc);

        _rw_round_locale  = &loc;
        _rw_round_gated   = alternate && 0 == i % 2;
        _rw_round_arrived = 0;

        void* args [RW_ROUND_MAX_THREADS];

        for (int j = 0; j != rw_round_nthreads; ++j) {
            args [j]               = (void*)long (j);
            _rw_round_threw [j].threw = 0;
        }

        result = rw_thread_pool (0, std::size_t (rw_round_nthreads), 0,
                                 _rw_round_thread, args,
                                 std::size_t (rw_round_timeout));

        _rw_round_locale = 0;

        rw_error (result == 0, 0, __LINE__,
                  "rw_thread_pool(0, %d, 0, %{#f}, ...) failed",
                  rw_round_nthreads, _rw_round_thread);

        if (result)
            break;

        // check before the timeout: a thread that threw out of a gated
        // round is why the others waited for it
        bool success = true;

        for (int j = 0; j != rw_round_nthreads; ++j) {

            const _rw_round_exception &ex = _rw_round_threw [j];

            if (1 == ex.threw) {
                success = rw_assert (false, 0, __LINE__,
                                     "%s threw in %#s, round %d, "
                                     "thread %d: %s",
                                     what, locname, i, j, ex.what);
            }
            else if (2 == ex.threw) {
                success = rw_assert (false, 0, __LINE__,
                                     "%s threw an unknown exception "
                                     "in %#s, round %d, thread %d",
                                     what, locname, i, j);
            }
            else {
                const void* const exp =
                    _RWSTD_STATIC_CAST (const char*, expected) + inx * size;
                const void* const got = _rw_round_slots + j * size;

                success = check (locname, i, j, exp, got) && success;
            }
        }

        if (rw_thread_pool_timeout_expired ()) {
            rw_error (false, 0, __LINE__,
                      "round %d exceeded the soft timeout", i);
            result = 1;
        }

        // the first round that fails is the finding; later rounds
        // would only repeat it
        if (!success)
            break;
    }

    return result;

#else   // !_RWSTD_REENTRANT

    _RWSTD_UNUSED (sources);
    _RWSTD_UNUSED (nsources);
    _RWSTD_UNUSED (expected);
    _RWSTD_UNUSED (slots);
    _RWSTD_UNUSED (size);
    _RWSTD_UNUSED (prepare);
    _RWSTD_UNUSED (work);
    _RWSTD_UNUSED (check);
    _RWSTD_UNUSED (alternate);

    rw_note (0, 0, 0, "%s: test disabled in a non-thread-safe build", what);
    return 0;

#endif   // _RWSTD_REENTRANT
}

/**************************************************************************/

_TEST_EXPORT int
rw_round_test (int           argc,
               char         *argv[],
               const char   *filename,
               const char   *clause,
               const char   *comment,
               int         (*testfun)(int, char**))
{
    return rw_test (argc, argv, filename, clause, comment, testfun,
                    "|-soft-timeout#0 "  // must be non-negative
                    "|-nthreads#0-* "    // must be in [0, RW_ROUND_MAX_THREADS]
                    "|-nrounds#1 ",      // fresh-locale rounds
                    &rw_round_timeout,
                    int (RW_ROUND_MAX_THREADS),
                    &rw_round_nthreads,
                    &rw_round_nrounds);
}
