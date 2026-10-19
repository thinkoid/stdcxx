/************************************************************************
 *
 * 22.locale.ctype.mt.cpp
 *
 * test exercising the thread safety of the ctype facet's widen() and
 * narrow() caches, racing their first fill on a fresh facet
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
#include <cstring>    // for strcmp(), strlen()

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

// the number of rounds, each a fresh locale
int opt_nrounds = 200;

// default timeout used by each threaded section of this test
int opt_timeout = 60;

/**************************************************************************/

// the locales the rounds cycle through, the C library's other than
// C and POSIX: the classic locale's facets live for the process,
// with their caches filled
static const char* locale_names [MAX_LOCALES];

// the number of names in the array
static std::size_t nlocales;

// the answers of the round's locale, computed by the main thread
// through a locale that is gone before the round's is constructed,
// so that the threads' facets are new; the caches hold the 256
// values of char and the 256 wide characters below 256
static struct {
    char    widened [256];     // ctype<char>::widen (char)
    char    narrowed [256];    // ctype<char>::narrow (char)
    wchar_t wwidened [256];    // ctype<wchar_t>::widen (char)
    char    wnarrowed [256];   // ctype<wchar_t>::narrow (wchar_t)
} record;

// the locale whose ctype facets the threads use
static const std::locale* race_locale;

// whether the round races ctype<wchar_t> rather than ctype<char>
static bool race_wide;

// the number of threads that have started in the current round
static int nstarted;

// per thread, the calls that disagreed with the record
static int mismatches [MAX_THREADS];

extern "C" {

static void*
race_func (void *arg)
{
    const long inx = long (arg);

    // start with the other threads so that they fill the facet's
    // caches at the same time
    rw_thread_gate (&nstarted, opt_nthreads);

    int bad = 0;

    if (race_wide) {
        const std::ctype<wchar_t> &wct =
            std::use_facet<std::ctype<wchar_t> >(*race_locale);

        for (std::size_t i = 0; i != 256; ++i) {
            if (wct.widen (char (i)) != record.wwidened [i])
                ++bad;
            if (wct.narrow (wchar_t (i), '\0') != record.wnarrowed [i])
                ++bad;
        }
    }
    else {
        const std::ctype<char> &ct =
            std::use_facet<std::ctype<char> >(*race_locale);

        for (std::size_t i = 0; i != 256; ++i) {
            if (ct.widen (char (i)) != record.widened [i])
                ++bad;
            if (ct.narrow (char (i), '\0') != record.narrowed [i])
                ++bad;
        }
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
             "racing the first widen() and narrow() of std::ctype<char> "
             "and std::ctype<wchar_t>, in turn, from %d thread%{?}s%{;}, "
             "%d round%{?}s%{;}, in %zu locale%{?}s%{;}",
             opt_nthreads, 1 != opt_nthreads,
             opt_nrounds, 1 != opt_nrounds,
             nlocales, 1 != nlocales);

    int result = 0;

    for (int i = 0; 0 == result && i != opt_nrounds; ++i) {

        const char* const name = locale_names [std::size_t (i) % nlocales];

        race_wide = 0 != i % 2;

        // the record, through a locale of its own
        {
            const std::locale loc (name);

            const std::ctype<char> &ct =
                std::use_facet<std::ctype<char> >(loc);
            const std::ctype<wchar_t> &wct =
                std::use_facet<std::ctype<wchar_t> >(loc);

            for (std::size_t j = 0; j != 256; ++j) {
                record.widened   [j] = ct.widen (char (j));
                record.narrowed  [j] = ct.narrow (char (j), '\0');
                record.wwidened  [j] = wct.widen (char (j));
                record.wnarrowed [j] = wct.narrow (wchar_t (j), '\0');
            }
        }

        // each round a new locale, whose facets have empty caches;
        // the slots are filled from this thread alone, so that the
        // threads race only the caches (the slot race is
        // 22.locale.use_facet.mt's)
        const std::locale loc (name);

        (void)std::use_facet<std::ctype<char> >(loc);
        (void)std::use_facet<std::ctype<wchar_t> >(loc);

        race_locale = &loc;
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
                       "std::ctype<%s> in std::locale (%#s), round %d, "
                       "thread %d: %d calls to widen() and narrow() "
                       "disagree with the record",
                       race_wide ? "wchar_t" : "char", name, i, j,
                       mismatches [j]);
        }

        race_locale = 0;
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
                    "lib.locale.ctype",
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
