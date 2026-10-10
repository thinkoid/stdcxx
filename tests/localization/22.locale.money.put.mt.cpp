/************************************************************************
 *
 * 22.locale.money.put.mt.cpp
 *
 * test exercising the thread safety of the money_put facet
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

#include <ios>        // for basic_ios, ios_base
#include <iterator>   // for ostreambuf_iterator
#include <locale>     // for locale, money_put, moneypunct, ctype
#include <string>     // for string, wstring

#include <rw_rounds.h>
#include <rw_driver.h>

/**************************************************************************/

// the locales the rounds cycle through, built from the tree's sources;
// a thread that finds the data of moneypunct or ctype missing falls
// back to the C library, which must not know the name for the output
// to differ (doc/notes/analysis-facet-first-use.md, chapter 2)
static const char* const locale_sources [][2] = {
    { "de_DE", "ISO-8859-1" },
    { "en_US", "ISO-8859-1" }
};

enum { NLOCALES = sizeof locale_sources / sizeof *locale_sources };

// the values, a positive and a negative, as the long double overload
// takes them and as the string overload does; put () reads the sign
// and the pattern in moneypunct and widens the digits through ctype.
// Without showbase it writes no currency symbol. Both locales' sources
// leave the international formats unspecified, so the international
// put () writes nothing, after reading the data of
// moneypunct<charT, true>.
static const long double values [] = { 1234567.0L, -1234567.0L };
static const char* const digits [] = { "1234567", "-1234567" };

enum { NVALUES = sizeof values / sizeof *values };

// the overloads of put (); the wide string overload is left out: its
// digit scan calls ctype<wchar_t>::is (), which throws in a locale
// only the database has (TODO, "ctype<wchar_t>::is throws for a locale
// only the database has")
enum { PUT_LDBL, PUT_STRING, NOVERLOADS };

static const char* const overloads [NOVERLOADS] = {
    "long double", "string"
};


template <class charT>
struct PutIos: std::basic_ios<charT>
{
    PutIos () { this->init (0); }
};


template <class charT>
struct PutStreambuf: std::basic_streambuf<charT>
{
    charT buf [32];

    PutStreambuf () { this->setp (buf, buf + sizeof buf / sizeof *buf); }

    std::basic_string<charT> str () const {
        return std::basic_string<charT> (this->pbase (),
                                         this->pptr () - this->pbase ());
    }
};


// what one call of put () produced
template <class charT>
struct PutResult
{
    int                       failed;   // the iterator it returned failed
    std::basic_string<charT>  out;      // what it wrote
};

// what a thread saw in the current round, in a block of its own: per
// character type, value, format (domestic, international) and overload
struct PutData
{
    PutResult<char>     narrow [NVALUES][2][NOVERLOADS];
    PutResult<wchar_t>  wide [NVALUES][2];
};

static PutData expected [NLOCALES];
static PutData results [RW_ROUND_MAX_THREADS];


template <class charT>
static void
put_money (const std::locale& loc, int inx, bool intl, int overload,
           PutResult<charT>& res)
{
    typedef std::ostreambuf_iterator<charT> Iter;

    const std::money_put<charT>& mp =
        std::use_facet<std::money_put<charT> >(loc);

    std::basic_string<charT> str;

    for (const char* s = digits [inx]; *s; ++s)
        str += charT (*s);

    PutIos<charT>       io;
    PutStreambuf<charT> sb;

    io.rdbuf (&sb);
    io.imbue (loc);

    const Iter end = PUT_STRING == overload ?
          mp.put (Iter (&sb), intl, io, charT (' '), str)
        : mp.put (Iter (&sb), intl, io, charT (' '), values [inx]);

    res.failed = end.failed ();
    res.out    = sb.str ();
}


// formats through the facets of a locale no thread has formatted
// through yet; the first use maps the data of moneypunct and ctype
static void
put_money (const std::locale& loc, void* block)
{
    PutData& data = *_RWSTD_STATIC_CAST (PutData*, block);

    for (int i = 0; i != NVALUES; ++i) {
        for (int intl = 0; intl != 2; ++intl) {
            for (int k = 0; k != NOVERLOADS; ++k)
                put_money (loc, i, 0 != intl, k, data.narrow [i][intl][k]);

            put_money (loc, i, 0 != intl, PUT_LDBL, data.wide [i][intl]);
        }
    }
}


// fills the locale's slots from the main thread alone, so that the
// threads race only the facets' data
static void
prepare (const std::locale& loc)
{
    (void)std::use_facet<std::money_put<char> >(loc);
    (void)std::use_facet<std::moneypunct<char, false> >(loc);
    (void)std::use_facet<std::moneypunct<char, true> >(loc);
    (void)std::use_facet<std::ctype<char> >(loc);

    (void)std::use_facet<std::money_put<wchar_t> >(loc);
    (void)std::use_facet<std::moneypunct<wchar_t, false> >(loc);
    (void)std::use_facet<std::moneypunct<wchar_t, true> >(loc);
    (void)std::use_facet<std::ctype<wchar_t> >(loc);

    for (int j = 0; j != rw_round_nthreads; ++j)
        results [j] = PutData ();
}

/**************************************************************************/

static bool
check_out (const char* tname, const std::string& got,
           const std::string& exp, const char* value, bool intl,
           const char* overload, const char* locname, int round, int thread)
{
    return rw_assert (got == exp, 0, __LINE__,
                      "money_put<%s>::put (%s, intl = %b, %s) wrote "
                      "%{#S}, got %{#S}, in %#s, round %d, thread %d",
                      tname, value, intl, overload, &exp, &got,
                      locname, round, thread);
}


static bool
check_out (const char* tname, const std::wstring& got,
           const std::wstring& exp, const char* value, bool intl,
           const char* overload, const char* locname, int round, int thread)
{
    return rw_assert (got == exp, 0, __LINE__,
                      "money_put<%s>::put (%s, intl = %b, %s) wrote "
                      "%{#lS}, got %{#lS}, in %#s, round %d, thread %d",
                      tname, value, intl, overload, &exp, &got,
                      locname, round, thread);
}


template <class charT>
static bool
check_result (const char* tname, const PutResult<charT>& got,
              const PutResult<charT>& exp, const char* value, bool intl,
              const char* overload, const char* locname, int round, int thread)
{
    bool success = true;

    success = rw_assert (exp.failed == got.failed, 0, __LINE__,
                         "money_put<%s>::put (%s, intl = %b, %s) failed "
                         "== %b, got %b, in %#s, round %d, thread %d",
                         tname, value, intl, overload,
                         0 != exp.failed, 0 != got.failed,
                         locname, round, thread) && success;

    success = check_out (tname, got.out, exp.out, value, intl, overload,
                         locname, round, thread) && success;

    return success;
}


static bool
check (const char* locname, int round, int thread,
       const void* expected_block, const void* got_block)
{
    const PutData& exp = *_RWSTD_STATIC_CAST (const PutData*, expected_block);
    const PutData& got = *_RWSTD_STATIC_CAST (const PutData*, got_block);

    bool success = true;

    for (int i = 0; i != NVALUES; ++i) {
        for (int intl = 0; intl != 2; ++intl) {

            for (int k = 0; k != NOVERLOADS; ++k)
                success = check_result ("char", got.narrow [i][intl][k],
                                        exp.narrow [i][intl][k], digits [i],
                                        0 != intl, overloads [k],
                                        locname, round, thread)
                    && success;

            success = check_result ("wchar_t", got.wide [i][intl],
                                    exp.wide [i][intl], digits [i],
                                    0 != intl, overloads [PUT_LDBL],
                                    locname, round, thread)
                && success;
        }
    }

    return success;
}

/**************************************************************************/

static int
run_test (int, char**)
{
    // the threads start as they are created: moneypunct keeps no lazy
    // state of its own, and a gate before the call would send every
    // thread into it before the data can be published too early
    // (doc/notes/analysis-facet-first-use.md, 2.4)
    return rw_first_use_rounds (locale_sources, NLOCALES,
                                expected, results, sizeof (PutData),
                                prepare, put_money, check,
                                "std::money_put<charT>::put ()", false);
}

/**************************************************************************/

int main (int argc, char *argv[])
{
    return rw_round_test (argc, argv, __FILE__,
                          "lib.locale.money.put",
                          "thread safety of the first format", run_test);
}
