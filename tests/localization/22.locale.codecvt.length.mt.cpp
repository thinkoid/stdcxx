/************************************************************************
 *
 * 22.locale.codecvt.length.mt.cpp
 *
 * test exercising the thread safety of the first length query
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
// know the name for the operation to fail
// (doc/notes/analysis-facet-first-use.md, chapter 2)
static const char* const locale_sources [][2] = {
    { "de_DE", "ISO-8859-1" }
};

// the external sequence, with a byte above the ASCII range so that the
// facet reads its table
static const char src [] = "a\xe4" "c";

enum { SRC_LEN = sizeof src - 1 };

// what a thread saw in the current round, in a block of its own
struct Data
{
    int wlen;   // the result of the wide length ()
    int nlen;   // the same for codecvt<char, char>
};

static Data expected [1];
static Data results [RW_ROUND_MAX_THREADS];


template <class internT>
static void
length (const std::codecvt<internT, char, std::mbstate_t>& cvt, int& len)
{
    std::mbstate_t state = std::mbstate_t ();

    len = cvt.length (state, src, src + SRC_LEN, 32);
}


// measures through the facets of a locale no thread has used yet; the
// first use maps the wide facet's database
static void
length (const std::locale& loc, void* block)
{
    Data& data = *_RWSTD_STATIC_CAST (Data*, block);

    length (std::use_facet<WCodeCvt>(loc), data.wlen);
    length (std::use_facet<NCodeCvt>(loc), data.nlen);
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
    const Data& exp = *_RWSTD_STATIC_CAST (const Data*, expected_block);
    const Data& got = *_RWSTD_STATIC_CAST (const Data*, got_block);

    bool success = true;

    success = rw_assert (exp.wlen == got.wlen, 0, __LINE__,
                         "codecvt<wchar_t, char>::length (%{#*s}) == %d, "
                         "got %d, in %#s, round %d, thread %d",
                         SRC_LEN, src, exp.wlen, got.wlen,
                         locname, round, thread) && success;

    success = rw_assert (exp.nlen == got.nlen, 0, __LINE__,
                         "codecvt<char, char>::length (%{#*s}) == %d, "
                         "got %d, in %#s, round %d, thread %d",
                         SRC_LEN, src, exp.nlen, got.nlen,
                         locname, round, thread) && success;

    return success;
}

/**************************************************************************/

static int
run_test (int, char**)
{
    // the threads start as they are created: a gate before the call
    // sends every thread into it before the data can be published too
    // early, and hides the fault this test covers
    // (doc/notes/analysis-facet-first-use.md, 2.4)
    return rw_first_use_rounds (locale_sources, 1,
                                expected, results, sizeof (Data),
                                prepare, length, check,
                                "std::codecvt<charT, char>::length ()", false);
}

/**************************************************************************/

int main (int argc, char *argv[])
{
    return rw_round_test (argc, argv, __FILE__,
                          "lib.locale.codecvt",
                          "thread safety of the first length query",
                          run_test);
}
