/************************************************************************
 *
 * 22.locale.codecvt.in.mt.cpp
 *
 * test exercising the thread safety of the first input conversion
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

#include <cstring>    // for memset()
#include <cwchar>     // for mbstate_t

#include <rw_rounds.h>
#include <rw_driver.h>

/**************************************************************************/

typedef std::codecvt<char, char, std::mbstate_t>    NCodeCvt;
typedef std::codecvt<wchar_t, char, std::mbstate_t> WCodeCvt;

// a locale that only the library's database has: a thread that finds
// the facet's data missing falls back to the C library, which must not
// know the name for the conversion to fail
// (doc/notes/analysis-facet-first-use.md, chapter 2)
static const char* const locale_sources [][2] = {
    { "de_DE", "ISO-8859-1" }
};

// the external sequence, with a byte above the ASCII range so that the
// conversion reads the facet's table
static const char src [] = "a\xe4" "c";

enum { SRC_LEN = sizeof src - 1 };

// what a thread saw in the current round, in a block of its own
struct InData
{
    int     wres;              // the result of the wide in ()
    int     wfrom;             // external characters consumed
    int     wto;               // internal characters produced
    wchar_t wdst [SRC_LEN];

    int     nres;              // the same for codecvt<char, char>
    int     nfrom;
    int     nto;
    char    ndst [SRC_LEN];
};

static InData expected [1];
static InData results [RW_ROUND_MAX_THREADS];


template <class internT>
static void
convert (const std::codecvt<internT, char, std::mbstate_t>& cvt,
         internT* dst, int& res, int& nfrom, int& nto)
{
    const char* from_next = 0;
    internT*    to_next   = 0;

    std::mbstate_t state = std::mbstate_t ();

    res = cvt.in (state, src, src + SRC_LEN, from_next,
                  dst, dst + SRC_LEN, to_next);

    nfrom = int (from_next - src);
    nto   = int (to_next - dst);
}


// converts through the facets of a locale no thread has converted
// through yet; the first use maps the wide facet's database
static void
convert (const std::locale& loc, void* block)
{
    InData& data = *_RWSTD_STATIC_CAST (InData*, block);

    convert (std::use_facet<WCodeCvt>(loc),
             data.wdst, data.wres, data.wfrom, data.wto);

    convert (std::use_facet<NCodeCvt>(loc),
             data.ndst, data.nres, data.nfrom, data.nto);
}


// fills the locale's slots from the main thread alone, so that the
// threads race only the facets' data
static void
prepare (const std::locale& loc)
{
    (void)std::use_facet<WCodeCvt>(loc);
    (void)std::use_facet<NCodeCvt>(loc);

    std::memset (results, 0, sizeof results);
}

/**************************************************************************/

static bool
check (const char* locname, int round, int thread,
       const void* expected_block, const void* got_block)
{
    const InData& exp = *_RWSTD_STATIC_CAST (const InData*, expected_block);
    const InData& got = *_RWSTD_STATIC_CAST (const InData*, got_block);

    bool success = true;

    success = rw_assert (exp.wres == got.wres, 0, __LINE__,
                         "codecvt<wchar_t, char>::in (%{#*s}) == %d, got %d, "
                         "in %#s, round %d, thread %d",
                         SRC_LEN, src, exp.wres, got.wres,
                         locname, round, thread) && success;

    success = rw_assert (exp.wfrom == got.wfrom && exp.wto == got.wto,
                         0, __LINE__,
                         "codecvt<wchar_t, char>::in (%{#*s}) consumed %d "
                         "and produced %d, got %d and %d, in %#s, round %d, "
                         "thread %d",
                         SRC_LEN, src, exp.wfrom, exp.wto, got.wfrom, got.wto,
                         locname, round, thread) && success;

    success = rw_assert (0 == std::memcmp (exp.wdst, got.wdst,
                                           exp.wto * sizeof (wchar_t)),
                         0, __LINE__,
                         "codecvt<wchar_t, char>::in (%{#*s}) == %{#*ls}, "
                         "got %{#*ls}, in %#s, round %d, thread %d",
                         SRC_LEN, src, exp.wto, exp.wdst, got.wto, got.wdst,
                         locname, round, thread) && success;

    success = rw_assert (   exp.nres == got.nres
                         && exp.nfrom == got.nfrom && exp.nto == got.nto,
                         0, __LINE__,
                         "codecvt<char, char>::in (%{#*s}) == %d, consumed "
                         "%d and produced %d, got %d, %d and %d, in %#s, "
                         "round %d, thread %d",
                         SRC_LEN, src, exp.nres, exp.nfrom, exp.nto,
                         got.nres, got.nfrom, got.nto,
                         locname, round, thread) && success;

    return success;
}

/**************************************************************************/

static int
run_test (int, char**)
{
    // the threads start as they are created: a gate before the
    // conversion sends every thread into it before the data can be
    // published too early, and hides the fault this test covers
    // (doc/notes/analysis-facet-first-use.md, 2.4)
    return rw_first_use_rounds (locale_sources, 1,
                                expected, results, sizeof (InData),
                                prepare, convert, check,
                                "std::codecvt<charT, char>::in ()", false);
}

/**************************************************************************/

int main (int argc, char *argv[])
{
    return rw_round_test (argc, argv, __FILE__,
                          "lib.locale.codecvt",
                          "thread safety of the first input conversion",
                          run_test);
}
