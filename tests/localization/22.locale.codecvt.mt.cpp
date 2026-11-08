/************************************************************************
 *
 * 22.locale.codecvt.mt.cpp
 *
 * test exercising the thread safety of the first conversions through
 * an ISO-2022-JP-2 wide codecvt facet, which map the component
 * databases of the character sets they meet
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

#include <locale>     // for locale, codecvt, codecvt_byname, use_facet
#include <cstring>    // for memset(), strcat(), strcpy(), strncpy()
#include <cwchar>     // for mbstate_t
#include <exception>  // for exception

#include <rw_file.h>
#include <rw_locale.h>
#include <rw_thread.h>
#include <rw_driver.h>


// maximum number of threads allowed by the command line interface
#define MAX_THREADS      32

// the number of threads (4 in a thread-safe build, set in main ()
// whatever the number of processors)
int opt_nthreads = 1;

// default timeout used by each threaded section of this test
int opt_timeout = 60;

/**************************************************************************/

typedef std::codecvt<wchar_t, char, std::mbstate_t>        CodeCvt;
typedef std::codecvt_byname<wchar_t, char, std::mbstate_t> CodeCvtByname;

#define BYTES(s)   s, sizeof s - 1

// one round per fixture, each a conversion through a component
// database that no earlier round has mapped: the converters cache
// the databases for the process, so a database is mapped once
static const struct {
    const char*  bytes;
    std::size_t  nbytes;
    wchar_t      wc;        // the character in () produces
    const char*  dbname;    // the database the conversion maps
} fixtures [] = {
    { BYTES ("a"),                    L'a',      "ANSI_X3.4-1968" },
    // JIS X 0208
    { BYTES ("\x1b$B\x24\x22"),       L'\x3042', "EUC-JP" },
    { BYTES ("\x1b.A\x1bN\x69"),      L'\xe9',   "ISO-8859-1" },
    { BYTES ("\x1b.F\x1bN\x61"),      L'\x3b1',  "ISO-8859-7" },
    { BYTES ("\x1b$A\x30\x21"),       L'\x554a', "GB2312" },
    // KS C 5601
    { BYTES ("\x1b$(C\x30\x21"),      L'\xac00', "EUC-KR" }
};

static const int nrounds = int (sizeof fixtures / sizeof *fixtures);

// the locale whose wide codecvt facet the threads convert through
static const std::locale* race_locale;

// the fixture of the current round
static int race_round;

// the number of threads that have started in the current round
static int nstarted;

// what each thread saw in the current round, in a slot of its own;
// the driver's diagnostics are not thread-safe, so the main thread
// reads the slots after the join and asserts
static struct RaceData {
    int     threw;       // 1 for a std::exception, 2 for anything else
    int     res;         // the result of in ()
    int     nconv;       // the number of characters in () produced
    wchar_t dst [4];     // the characters in () produced
    char    what [512];  // the exception's what ()
} race_data [MAX_THREADS];

extern "C" {

// converts the round's fixture, whose character set no thread has
// converted from yet; the first conversion maps the set's component
// database into the converters' process-wide cache
static void*
race_func (void* arg)
{
    RaceData& data = race_data [long (arg)];

    const char* const src       = fixtures [race_round].bytes;
    const char* const src_end   = src + fixtures [race_round].nbytes;
    const char*       from_next = 0;
    wchar_t*          to_next   = 0;
    std::mbstate_t    state     = std::mbstate_t ();

    // start with the other threads so that they find the database
    // unmapped at the same time
    rw_thread_gate (&nstarted, opt_nthreads);

    try {
        const CodeCvt& cvt = std::use_facet<CodeCvt>(*race_locale);

        data.res = cvt.in (state, src, src_end, from_next,
                           data.dst, data.dst + 4, to_next);
        data.nconv = int (to_next - data.dst);
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
    // the codecvt databases ISO-2022-JP-2 draws on; localedef writes
    // one for each character map it processes, and an empty locale
    // definition makes the quickest locale to process
    static const char* const charmaps [] = {
        "ANSI_X3.4-1968", "EUC-JP",
        "ISO-8859-1", "ISO-8859-7", "GB2312", "EUC-KR"
    };

    const std::size_t ncharmaps = sizeof charmaps / sizeof *charmaps;

    const char* const locale_root = rw_set_locale_root ();

    if (!rw_warn (0 != locale_root, 0, __LINE__,
                  "rw_set_locale_root () failed, test skipped"))
        return 0;

    char src [1024];
    std::strcpy (src, locale_root);
    std::strcat (src, "/empty.src");

    if (!rw_warn (std::size_t (-1) != rw_fwrite (src, ""), 0, __LINE__,
                  "rw_fwrite (%#s) failed, test skipped", src))
        return 0;

    for (std::size_t i = 0; i != ncharmaps; ++i) {
        char locname [64];
        std::strcpy (locname, "empty.");
        std::strcat (locname, charmaps [i]);

        if (!rw_warn (0 != rw_localedef ("-w", src, charmaps [i], locname),
                      0, __LINE__,
                      "rw_localedef (\"-w\", %#s, %#s, %#s) failed, "
                      "test skipped", src, charmaps [i], locname))
            return 0;
    }

    rw_info (0, 0, 0,
             "racing the first mapping of each component database of "
             "ISO-2022-JP-2 from %d thread%{?}s%{;}, %d round%{?}s%{;}",
             opt_nthreads, 1 != opt_nthreads, nrounds, 1 != nrounds);

    int result = 0;

    for (int i = 0; 0 == result && i != nrounds; ++i) {

        // each round a new facet, as a program constructs one
        const std::locale loc (std::locale::classic (),
                               new CodeCvtByname ("ISO-2022-JP-2"));

        // fill the locale's slot from this thread alone, so that the
        // threads race only the database cache; the race to fill the
        // slot is 22.locale.use_facet.mt's
        // (doc/notes/analysis-facet-first-use.md, chapter 1)
        (void)std::use_facet<CodeCvt>(loc);

        race_locale = &loc;
        race_round  = i;
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

        const char* const bytes  = fixtures [i].bytes;
        const int         nbytes = int (fixtures [i].nbytes);
        const char* const dbname = fixtures [i].dbname;

        for (int j = 0; 0 == result && j != opt_nthreads; ++j) {

            const RaceData& data = race_data [j];

            if (1 == data.threw) {
                rw_assert (false, 0, __LINE__,
                           "codecvt<wchar_t, char, mbstate_t>::in (%{#*s}) "
                           "threw on the first use of %s in round %d, "
                           "thread %d: %s",
                           nbytes, bytes, dbname, i, j, data.what);
            }
            else if (2 == data.threw) {
                rw_assert (false, 0, __LINE__,
                           "codecvt<wchar_t, char, mbstate_t>::in (%{#*s}) "
                           "threw an unknown exception on the first use "
                           "of %s in round %d, thread %d",
                           nbytes, bytes, dbname, i, j);
            }
            else {
                rw_assert (std::codecvt_base::ok == data.res, 0, __LINE__,
                           "codecvt<wchar_t, char, mbstate_t>::in (%{#*s}) "
                           "== ok, got %d, on the first use of %s in "
                           "round %d, thread %d",
                           nbytes, bytes, data.res, dbname, i, j);

                rw_assert (   1 == data.nconv
                           && fixtures [i].wc == data.dst [0], 0, __LINE__,
                           "codecvt<wchar_t, char, mbstate_t>::in (%{#*s}) "
                           "== %{#lc}, got %{#*ls}, on the first use of %s "
                           "in round %d, thread %d",
                           nbytes, bytes, fixtures [i].wc,
                           data.nconv, data.dst, dbname, i, j);
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
                    "|-nthreads#0-* ",   // must be in [0, MAX_THREADS]
                    &opt_timeout,
                    int (MAX_THREADS),
                    &opt_nthreads);
}
