/************************************************************************
 *
 * 22.locale.codecvt.mt.cpp
 *
 * test exercising the thread safety of the first conversion through
 * a wide codecvt facet, which maps the facet's database
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
#include <clocale>    // for setlocale(), LC_CTYPE
#include <cstring>    // for memset(), strcpy(), strncpy()
#include <cwchar>     // for mbstate_t
#include <exception>  // for exception

#include <rw_locale.h>
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

typedef std::codecvt<wchar_t, char, std::mbstate_t> CodeCvt;

// the locale whose wide codecvt facet the threads use for the first time
static const std::locale* race_locale;

// the number of threads that have started in the current round
static int nstarted;

// what each thread saw in the current round, in a slot of its own;
// the driver's diagnostics are not thread-safe, so the main thread
// reads the slots after the join and asserts
static struct RaceData {
    int     threw;       // 1 for a std::exception, 2 for anything else
    int     res;         // the result of in ()
    wchar_t dst [4];     // the characters in () produced
    char    what [512];  // the exception's what ()
} race_data [MAX_THREADS];

extern "C" {

// converts through the facet of `race_locale', which no thread has
// converted through yet; the first use maps the facet's database
// (doc/notes/analysis-facet-first-use.md, chapter 2)
static void*
race_func (void* arg)
{
    RaceData& data = race_data [long (arg)];

    const char     src [] = "abc";
    const char*    from_next = 0;
    wchar_t*       to_next   = 0;
    std::mbstate_t state = std::mbstate_t ();

    // start with the other threads so that they find the facet's
    // data missing at the same time
    rw_thread_gate (&nstarted, opt_nthreads);

    try {
        const CodeCvt& cvt = std::use_facet<CodeCvt>(*race_locale);

        data.res = cvt.in (state, src, src + 3, from_next,
                           data.dst, data.dst + 3, to_next);
    }
    catch (const std::exception& ex) {
        data.threw = 1;
        std::strncpy (data.what, ex.what (), sizeof data.what - 1);
    }
    catch (...) {
        data.threw = 2;
    }

    return 0;
}

}   // extern "C"

/**************************************************************************/

static int
run_test (int, char**)
{
    // a locale that only the library's database has: a thread that
    // finds the facet's data missing falls back to the C library,
    // which must not know the name for the conversion to fail. The
    // database is built under a name of the test's own, so that a
    // host with every C library locale installed keeps the test
    rw_set_locale_root ();

    const char* const locname =
        rw_localedef ("-w", "de_DE", "ISO-8859-1", "rwtest_de_DE.ISO-8859-1");

    if (0 == locname) {
        rw_warn (false, 0, __LINE__,
                 "failed to build locale rwtest_de_DE.ISO-8859-1 from "
                 "de_DE and ISO-8859-1; skipping");
        return 0;
    }

    char namebuf [64];
    std::strcpy (namebuf, locname);

    if (std::setlocale (LC_CTYPE, namebuf)) {
        std::setlocale (LC_CTYPE, "C");
        rw_note (false, 0, __LINE__,
                 "the C library knows %#s; skipping", namebuf);
        return 0;
    }

    rw_info (0, 0, 0,
             "racing the first std::codecvt<wchar_t, char>::in() in %#s "
             "from %d thread%{?}s%{;}, %d locale%{?}s%{;}",
             namebuf, opt_nthreads, 1 != opt_nthreads,
             opt_nrounds, 1 != opt_nrounds);

    int result = 0;

    for (int i = 0; 0 == result && i != opt_nrounds; ++i) {

        // each round a new locale, so that the facet and its data
        // are constructed anew
        const std::locale loc (namebuf);

        // fill the locale's slot from this thread alone, so that the
        // threads race only the facet's data; the race to fill the
        // slot is 22.locale.use_facet.mt's
        // (doc/notes/analysis-facet-first-use.md, chapter 1)
        (void)std::use_facet<CodeCvt>(loc);

        race_locale = &loc;
        nstarted    = 0;
        std::memset (race_data, 0, sizeof race_data);

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

            const RaceData& data = race_data [j];

            if (1 == data.threw) {
                rw_assert (false, 0, __LINE__,
                           "codecvt<wchar_t, char, mbstate_t>::in (\"abc\") "
                           "threw on first use in round %d, thread %d: %s",
                           i, j, data.what);
            }
            else if (2 == data.threw) {
                rw_assert (false, 0, __LINE__,
                           "codecvt<wchar_t, char, mbstate_t>::in (\"abc\") "
                           "threw an unknown exception on first use "
                           "in round %d, thread %d", i, j);
            }
            else {
                rw_assert (std::codecvt_base::ok == data.res, 0, __LINE__,
                           "codecvt<wchar_t, char, mbstate_t>::in (\"abc\") "
                           "== ok, got %d, on first use in round %d, "
                           "thread %d", data.res, i, j);

                rw_assert (   L'a' == data.dst [0] && L'b' == data.dst [1]
                           && L'c' == data.dst [2], 0, __LINE__,
                           "codecvt<wchar_t, char, mbstate_t>::in (\"abc\") "
                           "== L\"abc\", got %{#*ls}, on first use "
                           "in round %d, thread %d", 3, data.dst, i, j);
            }
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
                    "lib.locale.codecvt",
                    "thread safety", run_test,
                    "|-soft-timeout#0 "  // must be non-negative
                    "|-nrounds#0 "       // must be non-negative
                    "|-nthreads#0-* ",   // must be in [0, MAX_THREADS]
                    &opt_timeout,
                    &opt_nrounds,
                    int (MAX_THREADS),
                    &opt_nthreads);
}
