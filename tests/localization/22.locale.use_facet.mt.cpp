/************************************************************************
 *
 * 22.locale.use_facet.mt.cpp
 *
 * test exercising the thread safety of the first use_facet call
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

#include <locale>     // for locale, codecvt, use_facet

#include <cstring>    // for strcpy()
#include <cwchar>     // for mbstate_t

#include <rw_locale.h>
#include <rw_new.h>       // for rwt_check_leaks ()
#include <rw_thread.h>
#include <rw_driver.h>


// maximum number of threads allowed by the command line interface
#define MAX_THREADS      32

// default number of threads (will be adjusted to the number
// of processors/cores later)
int opt_nthreads = 1;

// the number of locales the test constructs, one per round
int opt_nrounds = 200;

// default timeout used by each threaded section of this test
int opt_timeout = 60;

/**************************************************************************/

typedef std::codecvt<wchar_t, char, std::mbstate_t> CodeCvt;

// the locale whose facet slot the threads race to fill
static const std::locale* race_locale;

// the number of threads that have started in the current round
static volatile int nstarted;

extern "C" {

// the replacement operator new of rw_new.h keeps no lock: the only
// allocations the threads make are the facet's and the facet
// repository's, both under the repository's lock
static void*
race_func (void*)
{
    // cast nstarted to int& (see STDCXX-792)
    _RWSTD_ATOMIC_PREINCREMENT (_RWSTD_CONST_CAST (int&, nstarted), false);

    // spin until all threads have been created so that they find
    // the slot empty at the same time
    while (nstarted < opt_nthreads);

    (void)std::use_facet<CodeCvt>(*race_locale);

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
             "racing the first std::use_facet<std::codecvt<wchar_t, char> >"
             " in %#s from %d thread%{?}s%{;}, %d locale%{?}s%{;}",
             namebuf, opt_nthreads, 1 != opt_nthreads,
             opt_nrounds, 1 != opt_nrounds);

    // a first round in this thread alone, for the allocations
    // the library makes once per process
    {
        const std::locale loc (namebuf);
        (void)std::use_facet<CodeCvt>(loc);
    }

    int result = 0;

    for (int i = 0; 0 == result && i != opt_nrounds; ++i) {

        rwt_check_leaks (0, 0);

        {
            // each round a new locale, whose facet slot is empty
            const std::locale loc (namebuf);

            race_locale = &loc;
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
                        "std::use_facet<std::codecvt<wchar_t, char> >"
                        "(std::locale (%#s)) from %d threads: "
                        "%zu blocks (%zu bytes) outlive the locale "
                        "in round %d",
                        namebuf, opt_nthreads, nblocks, nbytes, i))
            break;
    }

    return result;
}

/**************************************************************************/

int main (int argc, char *argv[])
{
#ifdef _RWSTD_REENTRANT

    // set nthreads to the greater of the number of processors
    // and 2 (for uniprocessor systems) by default
    opt_nthreads = rw_get_cpus ();
    if (opt_nthreads < 2)
        opt_nthreads = 2;

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
