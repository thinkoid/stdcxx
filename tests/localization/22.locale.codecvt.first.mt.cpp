/************************************************************************
 *
 * 22.locale.codecvt.first.mt.cpp
 *
 * test exercising the first conversion through a shared codecvt facet
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

#include <locale>     // for locale, codecvt

#include <clocale>    // for setlocale(), LC_CTYPE
#include <cstring>    // for memset(), strcpy(), strncpy()
#include <cwchar>     // for mbstate_t

#include <rw_locale.h>
#include <rw_thread.h>
#include <rw_driver.h>


#define MAX_THREADS 32

// a small fixed pool, independent of the processor count
int opt_nthreads = 4;

// default timeout used by each threaded section of this test
int opt_timeout = 5;

// the number of locales test_first_use constructs, one per round
int opt_first_use_rounds = 64;

/**************************************************************************/

#ifdef _RWSTD_REENTRANT

extern "C" {

// the locale whose wide codecvt facet the threads use for the first time
static const std::locale* first_use_locale;

// what each thread saw in the current round, in a slot of its own;
// the driver's diagnostics are not thread-safe, so the main thread
// reads the slots after the join and asserts
static struct FirstUseData {
    int     threw;       // 1 for a std::exception, 2 for anything else
    int     res;         // the result of in ()
    wchar_t dst [4];     // the characters in () produced
    char    what [512];  // the exception's what ()
} first_use_data [MAX_THREADS];

// converts through the facet of `first_use_locale', which no thread
// has converted through yet; the first use maps the facet's database
// (doc/notes/analysis-facet-first-use.md, chapter 2)
static void*
first_use_func (void* arg)
{
    typedef std::codecvt<wchar_t, char, std::mbstate_t> CodeCvt;

    FirstUseData& data = first_use_data [long (arg)];

    const char     src [] = "abc";
    const char*    from_next = 0;
    wchar_t*       to_next   = 0;
    std::mbstate_t state = std::mbstate_t ();

    try {
        const CodeCvt& cvt = std::use_facet<CodeCvt>(*first_use_locale);

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
test_first_use ()
{
    typedef std::codecvt<wchar_t, char, std::mbstate_t> CodeCvt;

    // a locale that only the library's database has: a thread that
    // finds the facet's data missing falls back to the C library,
    // which must not know the name for the conversion to fail
    rw_set_locale_root ();

    const char* const locname = rw_localedef ("-w", "de_DE", "ISO-8859-1", 0);

    if (0 == locname) {
        rw_warn (false, 0, __LINE__,
                 "failed to build locale de_DE.ISO-8859-1; "
                 "skipping the first use test");
        return 0;
    }

    char namebuf [64];
    std::strcpy (namebuf, locname);

    if (std::setlocale (LC_CTYPE, namebuf)) {
        std::setlocale (LC_CTYPE, "C");
        rw_note (false, 0, __LINE__,
                 "the C library knows %#s; skipping the first use test",
                 namebuf);
        return 0;
    }

    rw_info (0, 0, 0,
             "exercising the first use of std::codecvt<wchar_t, char> "
             "in %#s from %d thread%{?}s%{;}, %d locale%{?}s%{;}",
             namebuf, opt_nthreads, 1 != opt_nthreads,
             opt_first_use_rounds, 1 != opt_first_use_rounds);

    int result = 0;

    for (int i = 0; 0 == result && i != opt_first_use_rounds; ++i) {

        // each round a new locale, so that the facet and its data
        // are constructed anew
        const std::locale loc (namebuf);

        // fill the locale's slot from this thread alone, so that the
        // threads race only the facet's data; the race to fill the
        // slot is 22.locale.use_facet.mt's
        // (doc/notes/analysis-facet-first-use.md, chapter 1)
        (void)std::use_facet<CodeCvt>(loc);

        first_use_locale = &loc;
        std::memset (first_use_data, 0, sizeof first_use_data);

        void* args [MAX_THREADS];
        for (int j = 0; j != opt_nthreads; ++j)
            args [j] = (void*)long (j);

        // An arrival gate here hid the original fault by sending readers
        // into conversion too early. Keep natural thread-start staggering
        // without delays or inspection of the facet's internal state.
        result = rw_thread_pool (0, std::size_t (opt_nthreads), 0,
                                 first_use_func, args,
                                 std::size_t (opt_timeout));

        rw_error (result == 0, 0, __LINE__,
                  "rw_thread_pool(0, %d, 0, %{#f}, ...) failed",
                  opt_nthreads, first_use_func);

        if (0 == result && rw_thread_pool_timeout_expired ()) {
            rw_error (false, 0, __LINE__,
                      "first-use round %d exceeded the soft timeout", i);
            result = 1;
        }

        for (int j = 0; 0 == result && j != opt_nthreads; ++j) {

            const FirstUseData& data = first_use_data [j];

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

        first_use_locale = 0;
    }

    return result;
}

#endif   // _RWSTD_REENTRANT

/**************************************************************************/

static int
run_test (int, char**)
{
#ifdef _RWSTD_REENTRANT

    return test_first_use ();

#else   // !_RWSTD_REENTRANT

    rw_note (0, 0, 0, "first use test disabled in a non-thread-safe build");
    return 0;

#endif   // _RWSTD_REENTRANT
}

/**************************************************************************/

int main (int argc, char *argv[])
{
    return rw_test (argc, argv, __FILE__,
                    "lib.locale.codecvt",
                    "thread safety of the first conversion", run_test,
                    "|-soft-timeout#0 "  // must be non-negative
                    "|-nthreads#0-* "    // must be in [0, MAX_THREADS]
                    "|-nloops#1 ",       // fresh-locale rounds
                    &opt_timeout,
                    int (MAX_THREADS),
                    &opt_nthreads,
                    &opt_first_use_rounds);
}
