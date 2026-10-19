/************************************************************************
 *
 * 22.locale.messages.mt.cpp
 *
 * test exercising the thread safety of the messages facet's catalog
 * repository, racing open() and close() of one catalog from several
 * threads at once
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

#include <locale>     // for locale, messages, use_facet
#include <string>     // for string, wstring
#include <cstring>    // for strlen()

#include <rw_locale.h>   // for rw_create_catalog(), rw_locales()
#include <rw_new.h>      // for rwt_check_leaks()
#include <rw_process.h>  // for rw_system()
#include <rw_file.h>     // for SHELL_RM_F
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

// the locales the rounds cycle through, the C library's
static const char* locale_names [MAX_LOCALES];

// the number of names in the array
static std::size_t nlocales;

// the catalog, two sets of two messages
static const char* const messages [2][2] = {
    { "First set, first message",  "First set, second message" },
    { "Second set, first message", "Second set, second message" }
};

// the catalog's name, as open () takes it
static std::string catalog_name;

// the facets whose open () the threads race, one per round
static const std::messages<char>*    race_facet;
static const std::messages<wchar_t>* race_wfacet;

// the locale the catalog is opened in
static const std::locale* race_locale;

// whether the round races the wide facet
static bool race_wide;

// the number of threads that have started in the current round,
// and the number that have opened the catalog
static int nstarted;
static int nopened;

// the catalog each thread's open () returned
static std::messages_base::catalog opened [MAX_THREADS];

extern "C" {

// the replacement operator new of rw_new.h keeps no lock: the threads
// open and close, which allocate through the C library and, once per
// process, the repository's page; get () returns a string and is the
// main thread's
static void*
race_func (void *arg)
{
    const long inx = long (arg);

    // start with the other threads so that they enter the catalog
    // repository at the same time
    rw_thread_gate (&nstarted, opt_nthreads);

    const std::messages_base::catalog cat =
        race_wide ? (race_wfacet->open)(catalog_name, *race_locale)
                  : (race_facet->open)(catalog_name, *race_locale);

    opened [inx] = cat;

    // wait until every thread holds its catalog open: the catalogs
    // open at once must be distinct, while a slot closed may be
    // handed out again
    rw_thread_gate (&nopened, opt_nthreads);

    if (0 <= cat) {
        if (race_wide)
            (race_wfacet->close)(cat);
        else
            (race_facet->close)(cat);
    }

    return 0;
}

}   // extern "C"

/**************************************************************************/

// the catalogs the threads held open at once are valid and
// distinct: the repository gave each racing open () a slot of its own
static bool
distinct ()
{
    for (int i = 0; i != opt_nthreads; ++i) {
        if (opened [i] < 0)
            return false;

        for (int j = 0; j != i; ++j)
            if (opened [i] == opened [j])
                return false;
    }

    return true;
}


static int
run_test (int, char**)
{
    // write the catalog
    std::string text;

    for (int i = 0; i != 2; ++i) {
        for (int j = 0; j != 2; ++j)
            text.append (messages [i][j], std::strlen (messages [i][j]) + 1);

        text.append (1, '\0');
    }

    const int failed = rw_create_catalog ("rwstdmessages_0.msg", text.c_str ());

    rw_fatal (!failed, 0, __LINE__,
              "failed to create message catalog from rwstdmessages_0.msg");

    catalog_name = "./rwstdmessages_0.cat";

    const char* const locale_list =
        rw_opt_locales ? rw_opt_locales : rw_locales (_RWSTD_LC_ALL);

    for (const char *name = locale_list;
         *name && nlocales != MAX_LOCALES;
         name += std::strlen (name) + 1) {

        locale_names [nlocales++] = name;
    }

    rw_fatal (0 != nlocales, 0, __LINE__, "no locale to open the catalog in");

    rw_info (0, 0, 0,
             "racing std::messages<char>::open() and "
             "std::messages<wchar_t>::open(), in turn, on %#s "
             "from %d thread%{?}s%{;}, %d round%{?}s%{;}, "
             "in %zu locale%{?}s%{;}",
             catalog_name.c_str (),
             opt_nthreads, 1 != opt_nthreads,
             opt_nrounds, 1 != opt_nrounds,
             nlocales, 1 != nlocales);

    const std::string  expect  (messages [0][0]);
    const std::wstring wexpect (expect.begin (), expect.end ());

    // a first round in this thread alone, both facets, for the
    // allocations the library makes once per process
    {
        const std::locale loc (locale_names [0]);

        const std::messages<char> &nm =
            std::use_facet<std::messages<char> >(loc);
        const std::messages<wchar_t> &wm =
            std::use_facet<std::messages<wchar_t> >(loc);

        std::messages_base::catalog cat = (nm.open)(catalog_name, loc);
        rw_fatal (0 <= cat, 0, __LINE__, "failed to open %#s",
                  catalog_name.c_str ());
        (void)nm.get (cat, 1, 1, "");
        (nm.close)(cat);

        cat = (wm.open)(catalog_name, loc);
        rw_fatal (0 <= cat, 0, __LINE__, "failed to open %#s",
                  catalog_name.c_str ());
        (void)wm.get (cat, 1, 1, L"");
        (wm.close)(cat);
    }

    int result = 0;

    for (int i = 0; 0 == result && i != opt_nrounds; ++i) {

        const char* const name = locale_names [std::size_t (i) % nlocales];

        race_wide = 0 != i % 2;

        rwt_check_leaks (0, 0);

        {
            // each round a new locale; the slots are filled from this
            // thread alone, so that the threads race only the
            // repository (the slot race is 22.locale.use_facet.mt's)
            const std::locale loc (name);

            race_facet  = &std::use_facet<std::messages<char> >(loc);
            race_wfacet = &std::use_facet<std::messages<wchar_t> >(loc);
            race_locale = &loc;
            nstarted    = 0;
            nopened     = 0;

            void* args [MAX_THREADS];
            for (int j = 0; j != opt_nthreads; ++j) {
                opened [j] = -1;
                args [j]   = (void*)long (j);
            }

            result = rw_thread_pool (0, std::size_t (opt_nthreads), 0,
                                     race_func, args,
                                     std::size_t (opt_timeout));

            rw_error (result == 0, 0, __LINE__,
                      "rw_thread_pool(0, %d, 0, %{#f}, ...) failed",
                      opt_nthreads, race_func);

            if (result)
                break;

            rw_assert (distinct (), 0, __LINE__,
                       "std::messages<%s>::open(%#s) in std::locale (%#s) "
                       "from %d threads, round %d: every call returned "
                       "a valid catalog of its own",
                       race_wide ? "wchar_t" : "char",
                       catalog_name.c_str (), name, opt_nthreads, i);

            // the repository after the round: a catalog opened now
            // reads the message
            bool same;

            if (race_wide) {
                const std::messages_base::catalog cat =
                    (race_wfacet->open)(catalog_name, loc);
                same = 0 <= cat && wexpect == race_wfacet->get (cat, 1, 1, L"");
                if (0 <= cat)
                    (race_wfacet->close)(cat);
            }
            else {
                const std::messages_base::catalog cat =
                    (race_facet->open)(catalog_name, loc);
                same = 0 <= cat && expect == race_facet->get (cat, 1, 1, "");
                if (0 <= cat)
                    (race_facet->close)(cat);
            }

            rw_assert (same, 0, __LINE__,
                       "std::messages<%s>::get(open(%#s), 1, 1) in "
                       "std::locale (%#s) after round %d == %#s",
                       race_wide ? "wchar_t" : "char",
                       catalog_name.c_str (), name, i, messages [0][0]);

            race_facet  = 0;
            race_wfacet = 0;
            race_locale = 0;
        }

        // every catalog opened was closed, and the facets die with
        // the only locale that holds them
        std::size_t nbytes;
        const std::size_t nblocks = rwt_check_leaks (&nbytes, 0);

        if (!rw_assert (0 == nblocks, 0, __LINE__,
                        "std::messages<%s> in std::locale (%#s) from "
                        "%d threads: %zu blocks (%zu bytes) outlive "
                        "the locale in round %d",
                        race_wide ? "wchar_t" : "char", name,
                        opt_nthreads, nblocks, nbytes, i))
            break;
    }

    rw_system (SHELL_RM_F "rwstdmessages_*");

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
                    "lib.locale.messages",
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
