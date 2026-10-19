/************************************************************************
 *
 * 22.locale.cons.mt.cpp
 *
 * test exercising the thread safety of the locale constructors,
 * racing the construction of one named locale, and its combination
 * with another, from several threads at once
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

#include <locale>     // for locale, ctype, use_facet
#include <cstring>    // for strlen()

#include <rw_locale.h>   // for rw_locales()
#include <rw_thread.h>   // for rw_thread_gate(), rw_thread_pool()
#include <rw_driver.h>   // for rw_test()


// maximum number of threads allowed by the command line interface
#define MAX_THREADS      32

// maximum number of locales the test cycles through
#define MAX_LOCALES      64

// the number of threads (4 in a thread-safe build, set in main ()
// whatever the number of processors)
int opt_nthreads = 1;

// the number of rounds, each a name constructed from every thread
int opt_nrounds = 200;

// default timeout used by each threaded section of this test
int opt_timeout = 60;

/**************************************************************************/

// the locales the rounds cycle through, the C library's
static const char* locale_names [MAX_LOCALES];

// the number of names in the array
static std::size_t nlocales;

// the categories the rounds cycle through
static const std::locale::category categories [] = {
    std::locale::all,
    std::locale::collate,
    std::locale::ctype,
    std::locale::messages,
    std::locale::monetary,
    std::locale::numeric,
    std::locale::time,
    std::locale::none
};

static const std::size_t ncategories =
    sizeof categories / sizeof *categories;

// the round's name, the other name it is combined with, and the
// category of the combination
static const char* race_name;
static const char* race_other;
static std::locale::category race_category;

// the number of threads that have started in the current round
static int nstarted;

// the locale each thread constructed, held until the main thread
// has compared them: while every one is alive, one body per name
// means one facet per slot
static std::locale held [MAX_THREADS];

// per thread, the checks of the combinations that failed
static int mismatches [MAX_THREADS];

extern "C" {

static void*
race_func (void *arg)
{
    const long inx = long (arg);

    // start with the other threads so that they construct the body
    // of the same name at the same time
    rw_thread_gate (&nstarted, opt_nthreads);

    const std::locale first (race_name);
    const std::locale second (race_name);
    const std::locale other (race_other);

    int bad = 0;

    if (first != second)
        ++bad;

    // the combination by name and by locale, and what each must be
    const std::locale by_name (first, race_other, race_category);
    const std::locale by_loc (first, other, race_category);

    if (std::locale::none == race_category || first == other) {
        if (by_name != first)
            ++bad;
        if (by_loc != first)
            ++bad;
    }
    else if (std::locale::all == race_category) {
        if (by_name != other)
            ++bad;
        if (by_loc != other)
            ++bad;
    }
    else {
        if (by_name == first || by_name == other)
            ++bad;
        if (by_loc == first || by_loc == other)
            ++bad;
    }

    mismatches [inx] = bad;
    held [inx]       = first;

    return 0;
}

}   // extern "C"

/**************************************************************************/

static int
run_test (int, char**)
{
    const char* const locale_list =
        rw_opt_locales ? rw_opt_locales : rw_locales (_RWSTD_LC_ALL);

    for (const char *name = locale_list;
         *name && nlocales != MAX_LOCALES;
         name += std::strlen (name) + 1) {

        locale_names [nlocales++] = name;
    }

    rw_fatal (0 != nlocales, 0, __LINE__, "no locale to construct");

    rw_info (0, 0, 0,
             "racing the construction of a named locale, and its "
             "combination with another, from %d thread%{?}s%{;}, "
             "%d round%{?}s%{;}, in %zu locale%{?}s%{;}",
             opt_nthreads, 1 != opt_nthreads,
             opt_nrounds, 1 != opt_nrounds,
             nlocales, 1 != nlocales);

    int result = 0;

    for (int i = 0; 0 == result && i != opt_nrounds; ++i) {

        // the body of the round's name dies with the last locale that
        // holds it, so each round constructs it anew
        race_name     = locale_names [std::size_t (i) % nlocales];
        race_other    = locale_names [std::size_t (i + 1) % nlocales];
        race_category = categories [std::size_t (i) % ncategories];
        nstarted      = 0;

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

        for (int j = 0; j != opt_nthreads; ++j) {
            rw_assert (0 == mismatches [j], 0, __LINE__,
                       "std::locale (%#s) combined with %#s for category "
                       "%d, round %d, thread %d: %d checks failed",
                       race_name, race_other, int (race_category), i, j,
                       mismatches [j]);
        }

        // one body per name: with every thread's locale alive, the
        // facet in a slot of the body is the same object through
        // each of them
        const std::ctype<char>* const facet =
            &std::use_facet<std::ctype<char> >(held [0]);

        bool same = true;

        for (int j = 1; same && j != opt_nthreads; ++j)
            same = facet == &std::use_facet<std::ctype<char> >(held [j]);

        rw_assert (same, 0, __LINE__,
                   "std::locale (%#s) constructed from %d threads at once, "
                   "round %d: one body, the same ctype<char> facet "
                   "through every locale",
                   race_name, opt_nthreads, i);

        for (int j = 0; j != opt_nthreads; ++j)
            held [j] = std::locale::classic ();
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
                    "lib.locale.cons",
                    "thread safety", run_test,
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
