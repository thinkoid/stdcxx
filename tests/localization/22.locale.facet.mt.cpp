/************************************************************************
 *
 * 22.locale.facet.mt.cpp
 *
 * test exercising the thread safety of facet reference counts
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

#include <locale>     // for locale, has_facet

#include <rw_thread.h>
#include <rw_driver.h>


// maximum number of threads allowed by the command line interface
#define MAX_THREADS      32

// the number of threads racing each round (at most 8 by default,
// fewer on machines with fewer processors)
int opt_nthreads = 1;

// the number of rounds, each with a new base locale
int opt_nrounds = 20;

// the number of locales each thread builds and destroys per round
int opt_nloops = 1000;

// default timeout used by each threaded section of this test
int opt_timeout = 60;

/**************************************************************************/

// a user-defined facet that counts its destructions; the locale that
// holds it deletes it when the facet's last reference goes
struct Counted: std::locale::facet
{
    static std::locale::id id;

    static int ndestroyed;

    ~Counted () {
        _RWSTD_ATOMIC_PREINCREMENT (ndestroyed, false);
    }
};

std::locale::id Counted::id;
int             Counted::ndestroyed;


// the facet each thread adds to the base to build a new locale
struct Added: std::locale::facet
{
    static std::locale::id id;
};

std::locale::id Added::id;

/**************************************************************************/

// the locale holding the counted facet, shared by the threads
static const std::locale* base_locale;

// the number of threads that have started in the current round
static volatile int nstarted;

extern "C" {

static void*
race_func (void*)
{
    // cast nstarted to int& (see STDCXX-792)
    _RWSTD_ATOMIC_PREINCREMENT (_RWSTD_CONST_CAST (int&, nstarted), false);

    // spin until all threads have been created so that they update
    // the reference counts of the base's facets at the same time
    while (_RWSTD_ATOMIC_LOAD_RELAXED (nstarted) < opt_nthreads);

    // each locale built from the base takes a reference to each of
    // its facets, and its destruction returns them
    for (int i = 0; i != opt_nloops; ++i) {
        const std::locale loc (*base_locale, new Added);
        (void)std::has_facet<Counted>(loc);
    }

    return 0;
}

}   // extern "C"

/**************************************************************************/

static int
run_test (int, char**)
{
    rw_info (0, 0, 0,
             "building and destroying %d locale%{?}s%{;} from a shared one "
             "in each of %d thread%{?}s%{;}, %d round%{?}s%{;}",
             opt_nloops, 1 != opt_nloops, opt_nthreads, 1 != opt_nthreads,
             opt_nrounds, 1 != opt_nrounds);

    int result = 0;

    for (int i = 0; 0 == result && i != opt_nrounds; ++i) {

        Counted::ndestroyed = 0;

        base_locale = new std::locale (std::locale::classic (), new Counted);
        nstarted    = 0;

        result = rw_thread_pool (0, std::size_t (opt_nthreads), 0,
                                 race_func, 0,
                                 std::size_t (opt_timeout));

        rw_error (result == 0, 0, __LINE__,
                  "rw_thread_pool(0, %d, 0, %{#f}, 0) failed",
                  opt_nthreads, race_func);

        // a lost increment deletes the facet while the base holds it
        const int early = Counted::ndestroyed;

        delete base_locale;
        base_locale = 0;

        // a lost decrement keeps it alive after its last locale
        const int total = Counted::ndestroyed;

        rw_assert (0 == early, 0, __LINE__,
                   "facet destroyed %d time%{?}s%{;} while its locale was "
                   "alive in round %d", early, 1 != early, i);

        if (!rw_assert (1 == total, 0, __LINE__,
                        "facet destroyed %d time%{?}s%{;} in all, expected "
                        "once, in round %d", total, 1 != total, i))
            break;
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
                    "lib.locale.facet",
                    "thread safety", run_test,
                    "|-soft-timeout#0 "  // must be non-negative
                    "|-nrounds#0 "       // must be non-negative
                    "|-nloops#0 "        // must be non-negative
                    "|-nthreads#0-* ",   // must be in [0, MAX_THREADS]
                    &opt_timeout,
                    &opt_nrounds,
                    &opt_nloops,
                    int (MAX_THREADS),
                    &opt_nthreads);
}
