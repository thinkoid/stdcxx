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

#include <string>     // for string, wstring

#include <rw_rounds.h>
#include <rw_driver.h>

/**************************************************************************/

// the locales the rounds cycle through, built from the tree's sources
static const char* const locale_sources [][2] = {
    { "de_DE", "ISO-8859-1" },
    { "en_US", "ISO-8859-1" }
};

enum { NLOCALES = sizeof locale_sources / sizeof *locale_sources };

template <class charT>
struct PunctValues
{
    charT                    decimal_point;
    charT                    thousands_sep;
    std::string              grouping;
    std::basic_string<charT> truename;
    std::basic_string<charT> falsename;
};

// what a thread saw in the current round, in a block of its own
struct PunctData
{
    PunctValues<char>     narrow;
    PunctValues<wchar_t>  wide;
};

static PunctData expected [NLOCALES];
static PunctData results [RW_ROUND_MAX_THREADS];


template <class charT>
static void
get_punct (const std::locale& loc, PunctValues<charT>& val)
{
    const std::numpunct<charT>& np =
        std::use_facet<std::numpunct<charT> >(loc);

    rw_round_gate ();
    val.decimal_point = np.decimal_point ();

    rw_round_gate ();
    val.thousands_sep = np.thousands_sep ();

    rw_round_gate ();
    val.grouping = np.grouping ();

    rw_round_gate ();
    val.truename = np.truename ();

    rw_round_gate ();
    val.falsename = np.falsename ();
}


static void
get_punct (const std::locale& loc, void* block)
{
    PunctData& data = *_RWSTD_STATIC_CAST (PunctData*, block);

    get_punct (loc, data.narrow);
    get_punct (loc, data.wide);
}


// fills the locale's slots from the main thread alone, so that the
// threads race only the facets
static void
prepare (const std::locale& loc)
{
    (void)std::use_facet<std::numpunct<char> >(loc);
    (void)std::use_facet<std::numpunct<wchar_t> >(loc);

    for (int j = 0; j != rw_round_nthreads; ++j)
        results [j] = PunctData ();
}

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
check (const char* locname, int round, int thread,
       const void* expected_block, const void* got_block)
{
    const PunctData& exp = *_RWSTD_STATIC_CAST (const PunctData*, expected_block);
    const PunctData& got = *_RWSTD_STATIC_CAST (const PunctData*, got_block);

    bool success = true;

    success = check_punct ("char", got.narrow, exp.narrow,
                           locname, round, thread) && success;

    success = check_punct ("wchar_t", got.wide, exp.wide,
                           locname, round, thread) && success;

    return success;
}

/**************************************************************************/

static int
run_test (int, char**)
{
    // even rounds run the threads in lockstep, so that they find each
    // member's lazy state empty at once; odd rounds let them start as
    // they are created, so that a late thread can find the state half
    // published (doc/notes/analysis-facet-first-use.md, 2.4)
    return rw_first_use_rounds (locale_sources, NLOCALES,
                                expected, results, sizeof (PunctData),
                                prepare, get_punct, check,
                                "std::numpunct<charT>", true);
}

/**************************************************************************/

int main (int argc, char *argv[])
{
    return rw_round_test (argc, argv, __FILE__,
                          "lib.locale.numpunct",
                          "thread safety of the first use", run_test);
}
