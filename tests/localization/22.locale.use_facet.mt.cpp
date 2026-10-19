/************************************************************************
 *
 * 22.locale.use_facet.mt.cpp
 *
 * test exercising the thread safety of the first use_facet call,
 * racing it for each standard facet in turn
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

#include <locale>     // for locale, use_facet, the facets

#include <cstring>    // for strcpy()
#include <cwchar>     // for mbstate_t

#include <rw_locale.h>
#include <rw_new.h>       // for rwt_check_leaks ()
#include <rw_thread.h>
#include <rw_driver.h>


// maximum number of threads allowed by the command line interface
#define MAX_THREADS      32

// the number of threads (4 in a thread-safe build, set in main ()
// whatever the number of processors)
int opt_nthreads = 1;

// the number of locales the test constructs, one per round
int opt_nrounds = 200;

// default timeout used by each threaded section of this test
int opt_timeout = 60;

/**************************************************************************/

// calls use_facet for one standard facet
template <class Facet>
static void
use (const std::locale &loc)
{
    (void)std::use_facet<Facet>(loc);
}

typedef void use_func_t (const std::locale&);

// the standard facets, one slot of the locale body each, both
// character types; a named locale fills a slot with the _byname
// facet, the one use_facet<Base> and use_facet<Byname> both yield
static const struct {
    use_func_t* use;
    const char* name;
} facets [] = {
    { use<std::collate<char> >,
      "std::collate<char>" },
    { use<std::ctype<char> >,
      "std::ctype<char>" },
    { use<std::codecvt<char, char, std::mbstate_t> >,
      "std::codecvt<char, char, std::mbstate_t>" },
    { use<std::numpunct<char> >,
      "std::numpunct<char>" },
    { use<std::num_get<char> >,
      "std::num_get<char>" },
    { use<std::num_put<char> >,
      "std::num_put<char>" },
    { use<std::moneypunct<char, false> >,
      "std::moneypunct<char, false>" },
    { use<std::moneypunct<char, true> >,
      "std::moneypunct<char, true>" },
    { use<std::money_get<char> >,
      "std::money_get<char>" },
    { use<std::money_put<char> >,
      "std::money_put<char>" },
    { use<std::time_get<char> >,
      "std::time_get<char>" },
    { use<std::time_put<char> >,
      "std::time_put<char>" },
    { use<std::messages<char> >,
      "std::messages<char>" },
    { use<std::collate<wchar_t> >,
      "std::collate<wchar_t>" },
    { use<std::ctype<wchar_t> >,
      "std::ctype<wchar_t>" },
    { use<std::codecvt<wchar_t, char, std::mbstate_t> >,
      "std::codecvt<wchar_t, char, std::mbstate_t>" },
    { use<std::numpunct<wchar_t> >,
      "std::numpunct<wchar_t>" },
    { use<std::num_get<wchar_t> >,
      "std::num_get<wchar_t>" },
    { use<std::num_put<wchar_t> >,
      "std::num_put<wchar_t>" },
    { use<std::moneypunct<wchar_t, false> >,
      "std::moneypunct<wchar_t, false>" },
    { use<std::moneypunct<wchar_t, true> >,
      "std::moneypunct<wchar_t, true>" },
    { use<std::money_get<wchar_t> >,
      "std::money_get<wchar_t>" },
    { use<std::money_put<wchar_t> >,
      "std::money_put<wchar_t>" },
    { use<std::time_get<wchar_t> >,
      "std::time_get<wchar_t>" },
    { use<std::time_put<wchar_t> >,
      "std::time_put<wchar_t>" },
    { use<std::messages<wchar_t> >,
      "std::messages<wchar_t>" }
};

static const std::size_t nfacets = sizeof facets / sizeof *facets;

// the locale whose facet slot the threads race to fill
static const std::locale* race_locale;

// the facet whose slot is raced in the current round
static std::size_t race_facet;

// the number of threads that have started in the current round
static int nstarted;

extern "C" {

// the replacement operator new of rw_new.h keeps no lock: the only
// allocations the threads make are the facet's and the facet
// repository's, both under the repository's lock
static void*
race_func (void*)
{
    // start with the other threads so that they find
    // the slot empty at the same time
    rw_thread_gate (&nstarted, opt_nthreads);

    facets [race_facet].use (*race_locale);

    return 0;
}

}   // extern "C"

/**************************************************************************/

static int
run_test (int, char**)
{
    rw_set_locale_root ();

    const char* const locname = rw_localedef ("-w", "de_DE", "ISO-8859-1", 0);

    if (0 == locname) {
        rw_warn (false, 0, __LINE__,
                 "failed to build locale de_DE.ISO-8859-1; skipping");
        return 0;
    }

    char namebuf [64];
    std::strcpy (namebuf, locname);

    rw_info (0, 0, 0,
             "racing the first std::use_facet of each of %zu standard "
             "facets in %#s from %d thread%{?}s%{;}, %d locale%{?}s%{;}",
             nfacets, namebuf, opt_nthreads, 1 != opt_nthreads,
             opt_nrounds, 1 != opt_nrounds);

    // a first round in this thread alone, through every facet, for
    // the allocations the library makes once per process
    {
        const std::locale loc (namebuf);

        for (std::size_t i = 0; i != nfacets; ++i)
            facets [i].use (loc);
    }

    int result = 0;

    for (int i = 0; 0 == result && i != opt_nrounds; ++i) {

        rwt_check_leaks (0, 0);

        {
            // each round a new locale, whose facet slots are empty,
            // and the next facet
            const std::locale loc (namebuf);

            race_locale = &loc;
            race_facet  = std::size_t (i) % nfacets;
            nstarted    = 0;

            result = rw_thread_pool (0, std::size_t (opt_nthreads), 0,
                                     race_func, 0,
                                     std::size_t (opt_timeout));

            rw_error (result == 0, 0, __LINE__,
                      "rw_thread_pool(0, %d, 0, %{#f}, 0) failed",
                      opt_nthreads, race_func);

            race_locale = 0;
        }

        // the facet dies with the only locale that holds it: every
        // reference the racing threads took has been returned
        std::size_t nbytes;
        const std::size_t nblocks = rwt_check_leaks (&nbytes, 0);

        if (!rw_assert (0 == nblocks, 0, __LINE__,
                        "std::use_facet<%s>(std::locale (%#s)) from "
                        "%d threads: %zu blocks (%zu bytes) outlive "
                        "the locale in round %d",
                        facets [std::size_t (i) % nfacets].name, namebuf,
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
                    "lib.locale.global.templates",
                    "thread safety", run_test,
                    "|-soft-timeout#0 "  // must be non-negative
                    "|-nrounds#0 "       // must be non-negative
                    "|-nthreads#0-* ",   // must be in [0, MAX_THREADS]
                    &opt_timeout,
                    &opt_nrounds,
                    int (MAX_THREADS),
                    &opt_nthreads);
}
