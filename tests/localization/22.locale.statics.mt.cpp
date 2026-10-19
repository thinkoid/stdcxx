/************************************************************************
 *
 * 22.locale.statics.mt.cpp
 *
 * test exercising the thread safety of std::locale::global(), racing
 * the swap of the global locale from several threads at once
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

#include <locale>      // for locale
#include <string>      // for string
#include <algorithm>   // for sort()
#include <cstring>     // for strlen()

#include <rw_locale.h>   // for rw_locales()
#include <rw_thread.h>   // for rw_thread_gate(), rw_thread_pool()
#include <rw_driver.h>   // for rw_test()


// maximum number of threads allowed by the command line interface
#define MAX_THREADS   16

// the number of threads (4 in a thread-safe build, set in main ()
// whatever the number of processors)
int opt_nthreads = 1;

// the number of rounds, each a swap of the global locale from
// every thread at once
int opt_nrounds = 200;

// default timeout used by each threaded section of this test
int opt_timeout = 60;

/**************************************************************************/

// the locales the threads make global, the C library's
static std::locale locales [MAX_THREADS];

// their names, for the diagnostics
static const char* locale_names [MAX_THREADS];

// the number of locales in the arrays
static std::size_t nlocales;

// the round being raced
static int race_round;

// the number of threads that have started in the current round
static int nstarted;

// what each thread's call returned, in a slot of its own: the
// locale that was global before, as the call saw it
static std::locale returned [MAX_THREADS];

extern "C" {

static void*
race_func (void *arg)
{
    const long inx = long (arg);

    // start with the other threads so that they swap the global
    // locale at the same time
    rw_thread_gate (&nstarted, opt_nthreads);

    returned [inx] =
        std::locale::global (locales [(race_round + inx) % nlocales]);

    return 0;
}

}   // extern "C"

/**************************************************************************/

static int
run_test (int, char**)
{
    // the installed locales, or those named on the command line
    const char* const locale_list =
        rw_opt_locales ? rw_opt_locales : rw_locales (_RWSTD_LC_ALL);

    const std::size_t maxinx = sizeof locales / sizeof *locales;

    for (const char *name = locale_list;
         *name && nlocales != maxinx;
         name += std::strlen (name) + 1) {

        locale_names [nlocales] = name;
        locales [nlocales++]    = std::locale (name);
    }

    rw_fatal (0 != nlocales, 0, __LINE__, "no locale to make global");

    rw_info (0, 0, 0,
             "racing std::locale::global(const std::locale&) from "
             "%d thread%{?}s%{;}, %d round%{?}s%{;}, in %zu locale%{?}s%{;} "
             "{ %{ .*A@} }",
             opt_nthreads, 1 != opt_nthreads,
             opt_nrounds, 1 != opt_nrounds,
             nlocales, 1 != nlocales, int (nlocales), "%#s", locale_names);

    int result = 0;

    for (int i = 0; 0 == result && i != opt_nrounds; ++i) {

        // the global locale as the round starts
        const std::locale initial;

        race_round = i;
        nstarted   = 0;

        void* args [MAX_THREADS];
        for (int j = 0; j != opt_nthreads; ++j)
            args [j] = (void*)long (j);

        result = rw_thread_pool (0, std::size_t (opt_nthreads), 0,
                                 race_func, args,
                                 std::size_t (opt_timeout));

        rw_error (result == 0, 0, __LINE__,
                  "rw_thread_pool(0, %d, 0, %{#f}, ...) failed",
                  opt_nthreads, race_func);

        if (result)
            break;

        // the global locale as the round ends
        const std::locale final;

        // each call returned the locale that was global just before
        // it made its own global, so the locales returned plus the
        // final global are the locales set plus the initial global,
        // whatever the order of the calls; a return lost or seen
        // twice breaks the equality
        std::string set_names [MAX_THREADS + 1];
        std::string got_names [MAX_THREADS + 1];

        for (int j = 0; j != opt_nthreads; ++j) {
            set_names [j] = locales [(i + j) % nlocales].name ();
            got_names [j] = returned [j].name ();
        }

        set_names [opt_nthreads] = initial.name ();
        got_names [opt_nthreads] = final.name ();

        std::sort (set_names, set_names + opt_nthreads + 1);
        std::sort (got_names, got_names + opt_nthreads + 1);

        bool same = true;
        for (int j = 0; same && j != opt_nthreads + 1; ++j)
            same = set_names [j] == got_names [j];

        rw_assert (same, 0, __LINE__,
                   "std::locale::global() from %d threads in round %d: "
                   "the locales returned and the final global are the "
                   "locales set and the initial global",
                   opt_nthreads, i);
    }

    return result;
}

/**************************************************************************/

int main (int argc, char *argv[])
{
#ifdef _RWSTD_REENTRANT

    // four threads whatever the number of processors, the size
    // chosen for this program under ThreadSanitizer
    // (doc/notes/analysis-locale-mt-sizing.md)
    opt_nthreads = 4;

#endif   // _RWSTD_REENTRANT

    return rw_test (argc, argv, __FILE__,
                    "lib.locale.statics",
                    "thread safety",
                    run_test,
                    "|-soft-timeout#0 "  // must be non-negative
                    "|-nrounds#0 "       // must be non-negative
                    "|-nthreads#0-* "    // must be in [0, MAX_THREADS]
                    "|-locales= ",       // must be provided
                    &opt_timeout,
                    &opt_nrounds,
                    int (MAX_THREADS),
                    &opt_nthreads,
                    &rw_opt_setlocales);
}
