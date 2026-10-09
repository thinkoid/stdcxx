/************************************************************************
 *
 * codecvt_mt.h
 *
 * shared driver for codecvt multithreaded tests
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

#pragma once

#include <locale>
#include <cstring>
#include <cwchar>

#include <rw_locale.h>
#include <rw_thread.h>
#include <rw_driver.h>

// This driver belongs only to the codecvt MT programs. Each program
// supplies serial fixture preparation and checks for one iteration.
enum { MAX_THREADS = 32 };

typedef void codecvt_prepare_t (const std::locale&, std::size_t);
typedef void codecvt_exercise_t (const std::locale&, std::size_t,
                                std::size_t, bool, bool);

static codecvt_prepare_t*  codecvt_prepare;
static codecvt_exercise_t* codecvt_exercise;
static const char*        codecvt_comment;
static bool               codecvt_mixed_only;

static int opt_nthreads = 1;
static int opt_nloops = 5000;
static int opt_nlocales = MAX_THREADS;
static int opt_shared_locale;
static int opt_timeout = 60;

static const char* locales [MAX_THREADS];
static std::locale shared_locales [MAX_THREADS];
static std::size_t nlocales;

static bool test_char;
static bool test_wchar;

extern "C" {

static void*
codecvt_thread (void*)
{
    for (int i = 0; i != opt_nloops; ++i) {
        if (rw_thread_pool_timeout_expired ())
            break;

        const std::size_t inx = i % nlocales;
        const std::locale loc = opt_shared_locale
                              ? shared_locales [inx]
                              : std::locale (locales [inx]);

        // The mixed test advances its operation only after every locale
        // has been visited. Focused tests ignore the round number.
        codecvt_exercise (loc, inx, i / nlocales, test_char, test_wchar);
    }

    return 0;
}

}   // extern "C"

static int
run_codecvt_test (int, char**)
{
    const char* const locale_list =
        rw_opt_locales ? rw_opt_locales : rw_locales (_RWSTD_LC_ALL);

    for (const char* name = locale_list;
         *name; name += std::strlen (name) + 1) {
        try {
            const std::locale loc (name);

            if (codecvt_prepare)
                codecvt_prepare (loc, nlocales);

            if (opt_shared_locale)
                shared_locales [nlocales] = loc;

            locales [nlocales++] = name;
        }
        catch (...) {
            rw_warn (!rw_opt_locales, 0, __LINE__,
                     "failed to create locale(%#s)", name);
        }

        if (nlocales == MAX_THREADS || nlocales == std::size_t (opt_nlocales))
            break;
    }

    rw_fatal (nlocales != 0, 0, __LINE__,
              "failed to create one or more usable locales!");

    rw_info (0, 0, 0,
             "%s with %d thread%{?}s%{;}, "
             "%d iteration%{?}s%{;} each, in %zu locales { %{ .*A@} }",
             codecvt_comment, opt_nthreads, 1 != opt_nthreads,
             opt_nloops, 1 != opt_nloops,
             nlocales, int (nlocales), "%#s", locales);

    const char* const types [] = { "char", "wchar_t", "char and wchar_t" };

    for (int phase = codecvt_mixed_only ? 2 : 0; phase != 3; ++phase) {
        test_char  = phase != 1;
        test_wchar = phase != 0;

        rw_info (0, 0, 0, "exercising std::codecvt for %s", types [phase]);

        const int result =
            rw_thread_pool (0, std::size_t (opt_nthreads), 0,
                            codecvt_thread, 0, std::size_t (opt_timeout));

        rw_error (result == 0, 0, __LINE__,
                  "rw_thread_pool(0, %d, 0, %{#f}, 0) failed",
                  opt_nthreads, codecvt_thread);

        if (result)
            return result;
    }

    return 0;
}

// Preparation runs serially, once per locale. The exercise callback
// receives the locale, fixture index, round, and enabled character types.
// Prepared shared locales are warm; first-use testing has its own driver.
static int
codecvt_test (int argc, char** argv, const char* file, const char* comment,
              codecvt_prepare_t* prepare, codecvt_exercise_t* exercise,
              int nloops = 5000, bool mixed_only = false)
{
    codecvt_prepare = prepare;
    codecvt_exercise = exercise;
    codecvt_comment = comment;
    codecvt_mixed_only = mixed_only;
    opt_nloops = nloops;

#ifdef _RWSTD_REENTRANT

    opt_nthreads = rw_get_cpus ();
    if (opt_nthreads < 2)
        opt_nthreads = 2;

#endif   // _RWSTD_REENTRANT

    return rw_test (argc, argv, file,
                    "lib.locale.codecvt", comment, run_codecvt_test,
                    "|-soft-timeout#0 "
                    "|-nloops#0 "
                    "|-nthreads#0-* "
                    "|-nlocales#0 "
                    "|-locales= "
                    "|-shared-locale# ",
                    &opt_timeout, &opt_nloops,
                    int (MAX_THREADS), &opt_nthreads, &opt_nlocales,
                    &rw_opt_setlocales, &opt_shared_locale);
}
