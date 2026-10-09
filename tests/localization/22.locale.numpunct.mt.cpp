/************************************************************************
 *
 * 22.locale.numpunct.mt.cpp
 *
 * test exercising the thread safety of the numpunct facet
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

#include <locale>     // for locale, numpunct

#include <cstring>    // for strcpy(), strncpy()
#include <string>     // for string, wstring

#include <rw_locale.h>
#include <rw_thread.h>
#include <rw_driver.h>


#define MAX_THREADS 32

// a small fixed pool, independent of the processor count
int opt_nthreads = 4;

// the number of locales the test constructs, one per round
int opt_nrounds = 64;

// default timeout used by each threaded section of this test
int opt_timeout = 5;

/**************************************************************************/

#ifdef _RWSTD_REENTRANT

// the locales the rounds cycle through, built from the tree's sources:
// a name the C library does not know cannot hide a facet whose data
// is missing behind the C library's answer
static const char* const locale_sources [][2] = {
    { "de_DE", "ISO-8859-1" },
    { "en_US", "ISO-8859-1" }
};

enum { NLOCALES = sizeof locale_sources / sizeof *locale_sources };

static char locale_names [NLOCALES][64];

template <class charT>
struct PunctValues
{
    charT                    decimal_point;
    charT                    thousands_sep;
    std::string              grouping;
    std::basic_string<charT> truename;
    std::basic_string<charT> falsename;
};

// what each thread saw in the current round, in a slot of its own;
// the driver's diagnostics are not thread-safe, so the main thread
// reads the slots after the join and asserts
struct PunctData
{
    int                   threw;   // 1 for a std::exception, 2 otherwise
    char                  what [256];
    PunctValues<char>     narrow;
    PunctValues<wchar_t>  wide;
};

static PunctData expected [NLOCALES];
static PunctData results [MAX_THREADS];

// the locale whose facets the threads use for the first time
static const std::locale* round_locale;

// even rounds run the threads in lockstep, so that they find each
// member's lazy state empty at once; odd rounds let them start as they
// are created, so that a late thread can find the state half published
// (doc/notes/analysis-facet-first-use.md, 2.4)
static bool gated;

// the number of arrivals at the lockstep gates in the current round
static volatile int narrived;


// in a gated round, holds the thread until every thread has arrived
// at the same gate: one gate per member call keeps the threads from
// drifting apart on the first call, which maps the facet's data under
// the facet's mutex and releases them one by one
static void
lockstep ()
{
    if (!gated)
        return;

    // cast narrived to int& (see STDCXX-792)
    int& count = _RWSTD_CONST_CAST (int&, narrived);

    // every thread passes every gate, so the count of the gate this
    // arrival belongs to ends at the next multiple of the pool size
    const int arrival = _RWSTD_ATOMIC_PREINCREMENT (count, false);
    const int last    = (arrival + opt_nthreads - 1)
                      / opt_nthreads * opt_nthreads;

    // a thread that threw arrives no more; the soft timeout frees
    // the others, and the round fails on the exception
    while (   _RWSTD_ATOMIC_LOAD_RELAXED (count, false) < last
           && !rw_thread_pool_timeout_expired ());
}


template <class charT>
static void
get_punct (const std::locale& loc, PunctValues<charT>& val)
{
    const std::numpunct<charT>& np =
        std::use_facet<std::numpunct<charT> >(loc);

    lockstep ();
    val.decimal_point = np.decimal_point ();

    lockstep ();
    val.thousands_sep = np.thousands_sep ();

    lockstep ();
    val.grouping = np.grouping ();

    lockstep ();
    val.truename = np.truename ();

    lockstep ();
    val.falsename = np.falsename ();
}


static void
get_punct (const std::locale& loc, PunctData& data)
{
    try {
        get_punct (loc, data.narrow);
        get_punct (loc, data.wide);
    }
    catch (const std::exception& ex) {
        data.threw = 1;
        std::strncpy (data.what, ex.what (), sizeof data.what - 1);
    }
    catch (...) {
        data.threw = 2;
    }
}


extern "C" {

static void*
punct_func (void* arg)
{
    get_punct (*round_locale, results [long (arg)]);

    return 0;
}

}   // extern "C"

/**************************************************************************/

static bool
check_name (const char* tname, const char* member,
            const std::string& got, const std::string& exp,
            const char* locname, int round, int thread)
{
    return rw_assert (got == exp, 0, __LINE__,
                      "numpunct<%s>::%s () == %{#S}, got %{#S}, "
                      "in %#s, round %d, thread %d",
                      tname, member, &exp, &got, locname, round, thread);
}


static bool
check_name (const char* tname, const char* member,
            const std::wstring& got, const std::wstring& exp,
            const char* locname, int round, int thread)
{
    return rw_assert (got == exp, 0, __LINE__,
                      "numpunct<%s>::%s () == %{#lS}, got %{#lS}, "
                      "in %#s, round %d, thread %d",
                      tname, member, &exp, &got, locname, round, thread);
}


template <class charT>
static bool
check_punct (const char* tname,
             const PunctValues<charT>& got, const PunctValues<charT>& exp,
             const char* locname, int round, int thread)
{
    typedef typename std::char_traits<charT>::int_type IntT;

    bool success = true;

    success = rw_assert (exp.decimal_point == got.decimal_point,
                         0, __LINE__,
                         "numpunct<%s>::decimal_point () == %{#lc}, "
                         "got %{#lc}, in %#s, round %d, thread %d",
                         tname, IntT (exp.decimal_point),
                         IntT (got.decimal_point),
                         locname, round, thread) && success;

    success = rw_assert (exp.thousands_sep == got.thousands_sep,
                         0, __LINE__,
                         "numpunct<%s>::thousands_sep () == %{#lc}, "
                         "got %{#lc}, in %#s, round %d, thread %d",
                         tname, IntT (exp.thousands_sep),
                         IntT (got.thousands_sep),
                         locname, round, thread) && success;

    success = check_name (tname, "grouping", got.grouping, exp.grouping,
                          locname, round, thread) && success;

    success = check_name (tname, "truename", got.truename, exp.truename,
                          locname, round, thread) && success;

    success = check_name (tname, "falsename", got.falsename, exp.falsename,
                          locname, round, thread) && success;

    return success;
}


static bool
check_round (const char* locname, const PunctData& exp, int round)
{
    bool success = true;

    for (int j = 0; j != opt_nthreads; ++j) {

        const PunctData& got = results [j];

        if (1 == got.threw) {
            success = rw_assert (false, 0, __LINE__,
                                 "numpunct threw in %#s, round %d, "
                                 "thread %d: %s",
                                 locname, round, j, got.what);
        }
        else if (2 == got.threw) {
            success = rw_assert (false, 0, __LINE__,
                                 "numpunct threw an unknown exception "
                                 "in %#s, round %d, thread %d",
                                 locname, round, j);
        }
        else {
            success = check_punct ("char", got.narrow, exp.narrow,
                                   locname, round, j) && success;

            success = check_punct ("wchar_t", got.wide, exp.wide,
                                   locname, round, j) && success;
        }
    }

    return success;
}

/**************************************************************************/

static int
test_first_use ()
{
    rw_set_locale_root ();

    // build the locales and take the expected values serially, each
    // from a locale that dies before the rounds: a live one would keep
    // its facets for the rounds' locales of the same name to share
    for (int i = 0; i != NLOCALES; ++i) {

        const char* const locname =
            rw_localedef ("-w", locale_sources [i][0],
                          locale_sources [i][1], 0);

        if (0 == locname) {
            rw_warn (false, 0, __LINE__,
                     "failed to build locale %s.%s; skipping the test",
                     locale_sources [i][0], locale_sources [i][1]);
            return 0;
        }

        std::strcpy (locale_names [i], locname);

        get_punct (std::locale (locale_names [i]), expected [i]);

        rw_fatal (0 == expected [i].threw, 0, __LINE__,
                  "numpunct threw in %#s in a single thread: %s",
                  locale_names [i], expected [i].what);
    }

    rw_info (0, 0, 0,
             "exercising the first use of std::numpunct<charT> "
             "from %d thread%{?}s%{;}, %d locale%{?}s%{;}, "
             "alternately gated",
             opt_nthreads, 1 != opt_nthreads,
             opt_nrounds, 1 != opt_nrounds);

    int result = 0;

    for (int i = 0; 0 == result && i != opt_nrounds; ++i) {

        const int         inx     = i % NLOCALES;
        const char* const locname = locale_names [inx];

        // each round a new locale, so that its facets and their data
        // are constructed anew
        const std::locale loc (locname);

        // fill the locale's slots from this thread alone, so that the
        // threads race only the facets; the race to fill a slot is
        // 22.locale.use_facet.mt's
        (void)std::use_facet<std::numpunct<char> >(loc);
        (void)std::use_facet<std::numpunct<wchar_t> >(loc);

        round_locale = &loc;
        gated        = 0 == i % 2;
        narrived     = 0;

        // the join overwrites each argument with the thread's result
        void* args [MAX_THREADS];

        for (int j = 0; j != opt_nthreads; ++j) {
            args [j]    = (void*)long (j);
            results [j] = PunctData ();
        }

        result = rw_thread_pool (0, std::size_t (opt_nthreads), 0,
                                 punct_func, args,
                                 std::size_t (opt_timeout));

        round_locale = 0;

        rw_error (result == 0, 0, __LINE__,
                  "rw_thread_pool(0, %d, 0, %{#f}, ...) failed",
                  opt_nthreads, punct_func);

        if (result)
            break;

        // check before the timeout: a thread that threw out of a
        // lockstep round is why the others waited for it
        const bool success = check_round (locname, expected [inx], i);

        if (rw_thread_pool_timeout_expired ()) {
            rw_error (false, 0, __LINE__,
                      "round %d exceeded the soft timeout", i);
            result = 1;
        }

        // the first round that fails is the finding; later rounds
        // would only repeat it
        if (!success)
            break;
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

    rw_note (0, 0, 0, "test disabled in a non-thread-safe build");
    return 0;

#endif   // _RWSTD_REENTRANT
}

/**************************************************************************/

int main (int argc, char *argv[])
{
    return rw_test (argc, argv, __FILE__,
                    "lib.locale.numpunct",
                    "thread safety of the first use", run_test,
                    "|-soft-timeout#0 "  // must be non-negative
                    "|-nthreads#0-* "    // must be in [0, MAX_THREADS]
                    "|-nrounds#1 ",      // fresh-locale rounds
                    &opt_timeout,
                    int (MAX_THREADS),
                    &opt_nthreads,
                    &opt_nrounds);
}
