/************************************************************************
 *
 * 22.locale.codecvt.properties.mt.cpp
 *
 * test exercising the thread safety of the first encoding queries
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

// what a thread saw in the current round, in a block of its own
struct Data
{
    int  wencoding;        // the wide facet's encoding ()
    int  wmax_length;      // its max_length ()
    bool walways_noconv;   // its always_noconv ()

    int  nencoding;        // the same for codecvt<char, char>
    int  nmax_length;
    bool nalways_noconv;
};

static Data expected [1];
static Data results [RW_ROUND_MAX_THREADS];


template <class internT>
static void
query (const std::codecvt<internT, char, std::mbstate_t>& cvt,
       int& encoding, int& max_length, bool& always_noconv)
{
    encoding      = cvt.encoding ();
    max_length    = cvt.max_length ();
    always_noconv = cvt.always_noconv ();
}


// queries the facets of a locale no thread has used yet; the first
// use maps the wide facet's database
static void
query (const std::locale& loc, void* block)
{
    Data& data = *_RWSTD_STATIC_CAST (Data*, block);

    query (std::use_facet<WCodeCvt>(loc),
           data.wencoding, data.wmax_length, data.walways_noconv);

    query (std::use_facet<NCodeCvt>(loc),
           data.nencoding, data.nmax_length, data.nalways_noconv);
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

    success = rw_assert (exp.wencoding == got.wencoding, 0, __LINE__,
                         "codecvt<wchar_t, char>::encoding () == %d, got %d, "
                         "in %#s, round %d, thread %d",
                         exp.wencoding, got.wencoding,
                         locname, round, thread) && success;

    success = rw_assert (exp.wmax_length == got.wmax_length, 0, __LINE__,
                         "codecvt<wchar_t, char>::max_length () == %d, "
                         "got %d, in %#s, round %d, thread %d",
                         exp.wmax_length, got.wmax_length,
                         locname, round, thread) && success;

    success = rw_assert (exp.walways_noconv == got.walways_noconv,
                         0, __LINE__,
                         "codecvt<wchar_t, char>::always_noconv () == %b, "
                         "got %b, in %#s, round %d, thread %d",
                         exp.walways_noconv, got.walways_noconv,
                         locname, round, thread) && success;

    success = rw_assert (   exp.nencoding == got.nencoding
                         && exp.nmax_length == got.nmax_length
                         && exp.nalways_noconv == got.nalways_noconv,
                         0, __LINE__,
                         "codecvt<char, char>::encoding (), max_length () "
                         "and always_noconv () == %d, %d and %b, got %d, %d "
                         "and %b, in %#s, round %d, thread %d",
                         exp.nencoding, exp.nmax_length, exp.nalways_noconv,
                         got.nencoding, got.nmax_length, got.nalways_noconv,
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
                                prepare, query, check,
                                "std::codecvt<charT, char>::encoding (), max_length () and always_noconv ()", false);
}

/**************************************************************************/

int main (int argc, char *argv[])
{
    return rw_round_test (argc, argv, __FILE__,
                          "lib.locale.codecvt",
                          "thread safety of the first encoding queries",
                          run_test);
}
