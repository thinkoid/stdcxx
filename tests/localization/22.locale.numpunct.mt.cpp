/************************************************************************
 *
 * 22.locale.numpunct.mt.cpp
 *
 * test exercising the thread safety of the first use of a numpunct
 * facet, which builds the facet's data from the C library
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

#include <locale>     // for locale, numpunct, use_facet
#include <string>     // for string, wstring
#include <cstring>    // for strcmp(), strlen()

#include <rw_locale.h>   // for rw_locales()
#include <rw_new.h>      // for rwt_check_leaks()
#include <rw_thread.h>   // for rw_thread_gate(), rw_thread_pool()
#include <rw_driver.h>   // for rw_test()


// maximum number of threads allowed by the command line interface
#define MAX_THREADS      32

// maximum number of locales the test cycles through
#define MAX_LOCALES      64

// the number of threads (4 in a thread-safe build, set in main ()
// whatever the number of processors)
int opt_nthreads = 1;

// the number of rounds, each a fresh locale
int opt_nrounds = 200;

// default timeout used by each threaded section of this test
int opt_timeout = 60;

/**************************************************************************/

// the locales the rounds cycle through, the C library's other than
// C and POSIX: the library's database has no data for them, so the
// facets build theirs from the C library under __rw_setlocale
static const char* locale_names [MAX_LOCALES];

// the number of names in the array
static std::size_t nlocales;

// the answers of the round's locale, computed by the main thread
// through a locale that is gone before the round's is constructed,
// so that the threads' facets are new
static struct {
    char         decimal_point;
    char         thousands_sep;
    std::string  grouping;
    std::string  truename;
    std::string  falsename;
    wchar_t      wdecimal_point;
    wchar_t      wthousands_sep;
    std::wstring wtruename;
    std::wstring wfalsename;
} record;

// the facets whose data the threads race to build, one per round
static const std::numpunct<char>*    race_facet;
static const std::numpunct<wchar_t>* race_wfacet;

// whether the round races the wide facet
static bool race_wide;

// the number of threads that have started in the current round
static int nstarted;

// per thread, the calls that disagreed with the record
static int mismatches [MAX_THREADS];

extern "C" {

// the replacement operator new of rw_new.h keeps no lock: the threads
// call the two members that allocate nothing, and the only allocation
// they cause is the facet data's, under the lock of __rw_setlocale;
// the main thread checks the string members after the round
static void*
race_func (void *arg)
{
    const long inx = long (arg);

    // start with the other threads so that they find the facet's
    // data missing at the same time
    rw_thread_gate (&nstarted, opt_nthreads);

    int bad = 0;

    if (race_wide) {
        if (race_wfacet->decimal_point () != record.wdecimal_point)
            ++bad;
        if (race_wfacet->thousands_sep () != record.wthousands_sep)
            ++bad;
    }
    else {
        if (race_facet->decimal_point () != record.decimal_point)
            ++bad;
        if (race_facet->thousands_sep () != record.thousands_sep)
            ++bad;
    }

    mismatches [inx] = bad;

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

        if (std::strcmp (name, "C") && std::strcmp (name, "POSIX"))
            locale_names [nlocales++] = name;
    }

    if (0 == nlocales) {
        rw_warn (false, 0, __LINE__,
                 "no installed locale other than C and POSIX; skipping");
        return 0;
    }

    rw_info (0, 0, 0,
             "racing the first use of std::numpunct<char> and "
             "std::numpunct<wchar_t>, in turn, from %d thread%{?}s%{;}, "
             "%d round%{?}s%{;}, in %zu locale%{?}s%{;}",
             opt_nthreads, 1 != opt_nthreads,
             opt_nrounds, 1 != opt_nrounds,
             nlocales, 1 != nlocales);

    // a first round in this thread alone, for the allocations the
    // library makes once per process
    {
        const std::locale loc (locale_names [0]);

        (void)std::use_facet<std::numpunct<char> >(loc).grouping ();
        (void)std::use_facet<std::numpunct<wchar_t> >(loc).grouping ();
    }

    int result = 0;

    for (int i = 0; 0 == result && i != opt_nrounds; ++i) {

        const char* const name = locale_names [std::size_t (i) % nlocales];

        race_wide = 0 != i % 2;

        // the record, through a locale of its own, before the
        // checkpoint: its strings outlive the round
        {
            const std::locale loc (name);

            const std::numpunct<char> &np =
                std::use_facet<std::numpunct<char> >(loc);
            const std::numpunct<wchar_t> &wp =
                std::use_facet<std::numpunct<wchar_t> >(loc);

            record.decimal_point  = np.decimal_point ();
            record.thousands_sep  = np.thousands_sep ();
            record.grouping       = np.grouping ();
            record.truename       = np.truename ();
            record.falsename      = np.falsename ();
            record.wdecimal_point = wp.decimal_point ();
            record.wthousands_sep = wp.thousands_sep ();
            record.wtruename      = wp.truename ();
            record.wfalsename     = wp.falsename ();
        }

        rwt_check_leaks (0, 0);

        {
            // each round a new locale, whose facets have no data yet;
            // the slots are filled from this thread alone, so that
            // the threads race only the data (the slot race is
            // 22.locale.use_facet.mt's)
            const std::locale loc (name);

            race_facet  = &std::use_facet<std::numpunct<char> >(loc);
            race_wfacet = &std::use_facet<std::numpunct<wchar_t> >(loc);
            nstarted    = 0;

            void* args [MAX_THREADS];
            for (int j = 0; j != opt_nthreads; ++j)
                args [j] = (void*)long (j);

            result = rw_thread_pool (0, std::size_t (opt_nthreads), 0,
                                     race_func, args,
                                     std::size_t (opt_timeout));

            rw_error (result == 0, 0, __LINE__,
                      "rw_thread_pool(0, %d, 0, %{#f}, ...) failed",
                      opt_nthreads, race_func);

            for (int j = 0; 0 == result && j != opt_nthreads; ++j) {
                rw_assert (0 == mismatches [j], 0, __LINE__,
                           "std::numpunct<%s> in std::locale (%#s), "
                           "round %d, thread %d: %d of decimal_point() "
                           "and thousands_sep() disagree with the record",
                           race_wide ? "wchar_t" : "char", name, i, j,
                           mismatches [j]);
            }

            // the string members, from the data the threads built
            const bool same =
                race_wide
                ? (   race_wfacet->grouping ()  == record.grouping
                   && race_wfacet->truename ()  == record.wtruename
                   && race_wfacet->falsename () == record.wfalsename)
                : (   race_facet->grouping ()   == record.grouping
                   && race_facet->truename ()   == record.truename
                   && race_facet->falsename ()  == record.falsename);

            rw_assert (same, 0, __LINE__,
                       "std::numpunct<%s> in std::locale (%#s), round %d: "
                       "grouping(), truename() and falsename() agree "
                       "with the record",
                       race_wide ? "wchar_t" : "char", name, i);

            race_facet  = 0;
            race_wfacet = 0;
        }

        // the facets and their data die with the only locale that
        // holds them: no thread built data that was then overwritten
        std::size_t nbytes;
        const std::size_t nblocks = rwt_check_leaks (&nbytes, 0);

        if (!rw_assert (0 == nblocks, 0, __LINE__,
                        "std::numpunct<%s> in std::locale (%#s) from "
                        "%d threads: %zu blocks (%zu bytes) outlive "
                        "the locale in round %d",
                        race_wide ? "wchar_t" : "char", name,
                        opt_nthreads, nblocks, nbytes, i))
            break;
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
                    "lib.locale.numpunct",
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
