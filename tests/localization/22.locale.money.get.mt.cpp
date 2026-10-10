/************************************************************************
 *
 * 22.locale.money.get.mt.cpp
 *
 * test exercising the thread safety of the money_get facet
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
#include <iterator>   // for istreambuf_iterator
#include <locale>     // for locale, money_get, moneypunct, ctype
#include <string>     // for string, wstring

#include <rw_rounds.h>
#include <rw_driver.h>

/**************************************************************************/

// the locales the rounds cycle through, built from the tree's sources;
// a thread that finds the data of moneypunct or ctype missing falls
// back to the C library, which must not know the name for the parse
// to fail (doc/notes/analysis-facet-first-use.md, chapter 2)
static const char* const locale_sources [][2] = {
    { "de_DE", "ISO-8859-1" },
    { "en_US", "ISO-8859-1" }
};

enum { NLOCALES = sizeof locale_sources / sizeof *locale_sources };

// the inputs, with no currency symbol, which is optional without
// showbase, and the second with a sign, which the parse looks up in
// moneypunct. Both locales' domestic patterns read them to the end
// without reaching a space, which ctype<wchar_t>::is () would classify
// through the C library. Their sources leave the international formats
// unspecified, so the international parses fail on the first character,
// after reading the data of moneypunct<charT, true>.
static const char* const inputs [] = { "1234567", "-1234567" };

enum { NINPUTS = sizeof inputs / sizeof *inputs };


template <class charT>
struct GetIos: std::basic_ios<charT>
{
    GetIos () { this->init (0); }
};


template <class charT>
struct GetStreambuf: std::basic_streambuf<charT>
{
    void pubsetg (const charT* beg, std::streamsize n) {
        charT* const p = _RWSTD_CONST_CAST (charT*, beg);
        this->setg (p, p, p + n);
    }

    int nread () const { return int (this->gptr () - this->eback ()); }
};


// what one call of get () returned
template <class charT>
struct GetResult
{
    int                       state;    // the iostate it set
    int                       nread;    // input characters consumed
    long double               units;    // the long double overload
    std::basic_string<charT>  digits;   // the string overload
};

// what a thread saw in the current round, in a block of its own:
// per character type, input and format (domestic, international)
struct GetData
{
    GetResult<char>     narrow [NINPUTS][2];
    GetResult<wchar_t>  wide [NINPUTS][2];
};

static GetData expected [NLOCALES];
static GetData results [RW_ROUND_MAX_THREADS];


template <class charT>
static void
get_money (const std::locale& loc, const char* input, bool intl,
           GetResult<charT>& res)
{
    typedef std::istreambuf_iterator<charT> Iter;

    const std::money_get<charT>& mg =
        std::use_facet<std::money_get<charT> >(loc);

    charT src [16];
    std::size_t len = 0;

    for (; input [len]; ++len)
        src [len] = charT (input [len]);

    GetIos<charT>       io;
    GetStreambuf<charT> sb;

    io.rdbuf (&sb);
    io.imbue (loc);

    std::ios_base::iostate state = std::ios_base::goodbit;

    sb.pubsetg (src, std::streamsize (len));
    mg.get (Iter (&sb), Iter (), intl, io, state, res.units);

    res.nread = sb.nread ();

    // the string overload records the state of both calls
    sb.pubsetg (src, std::streamsize (len));
    mg.get (Iter (&sb), Iter (), intl, io, state, res.digits);

    res.state = int (state);
}


// parses through the facets of a locale no thread has parsed through
// yet; the first use maps the data of moneypunct and ctype
static void
get_money (const std::locale& loc, void* block)
{
    GetData& data = *_RWSTD_STATIC_CAST (GetData*, block);

    for (int i = 0; i != NINPUTS; ++i) {
        for (int intl = 0; intl != 2; ++intl) {
            get_money (loc, inputs [i], 0 != intl, data.narrow [i][intl]);
            get_money (loc, inputs [i], 0 != intl, data.wide [i][intl]);
        }
    }
}


// fills the locale's slots from the main thread alone, so that the
// threads race only the facets' data
static void
prepare (const std::locale& loc)
{
    (void)std::use_facet<std::money_get<char> >(loc);
    (void)std::use_facet<std::moneypunct<char, false> >(loc);
    (void)std::use_facet<std::moneypunct<char, true> >(loc);
    (void)std::use_facet<std::ctype<char> >(loc);

    (void)std::use_facet<std::money_get<wchar_t> >(loc);
    (void)std::use_facet<std::moneypunct<wchar_t, false> >(loc);
    (void)std::use_facet<std::moneypunct<wchar_t, true> >(loc);
    (void)std::use_facet<std::ctype<wchar_t> >(loc);

    for (int j = 0; j != rw_round_nthreads; ++j)
        results [j] = GetData ();
}

/**************************************************************************/

static bool
check_digits (const char* tname, const std::string& got,
              const std::string& exp, const char* input, bool intl,
              const char* locname, int round, int thread)
{
    return rw_assert (got == exp, 0, __LINE__,
                      "money_get<%s>::get (%#s, intl = %b, string&) "
                      "== %{#S}, got %{#S}, in %#s, round %d, thread %d",
                      tname, input, intl, &exp, &got,
                      locname, round, thread);
}


static bool
check_digits (const char* tname, const std::wstring& got,
              const std::wstring& exp, const char* input, bool intl,
              const char* locname, int round, int thread)
{
    return rw_assert (got == exp, 0, __LINE__,
                      "money_get<%s>::get (%#s, intl = %b, wstring&) "
                      "== %{#lS}, got %{#lS}, in %#s, round %d, thread %d",
                      tname, input, intl, &exp, &got,
                      locname, round, thread);
}


template <class charT>
static bool
check_result (const char* tname, const GetResult<charT>& got,
              const GetResult<charT>& exp, const char* input, bool intl,
              const char* locname, int round, int thread)
{
    bool success = true;

    success = rw_assert (exp.state == got.state, 0, __LINE__,
                         "money_get<%s>::get (%#s, intl = %b) state "
                         "== %{Is}, got %{Is}, in %#s, round %d, thread %d",
                         tname, input, intl, exp.state, got.state,
                         locname, round, thread) && success;

    success = rw_assert (exp.nread == got.nread, 0, __LINE__,
                         "money_get<%s>::get (%#s, intl = %b) consumed %d, "
                         "got %d, in %#s, round %d, thread %d",
                         tname, input, intl, exp.nread, got.nread,
                         locname, round, thread) && success;

    success = rw_assert (exp.units == got.units, 0, __LINE__,
                         "money_get<%s>::get (%#s, intl = %b, long double&) "
                         "== %Lg, got %Lg, in %#s, round %d, thread %d",
                         tname, input, intl, exp.units, got.units,
                         locname, round, thread) && success;

    success = check_digits (tname, got.digits, exp.digits, input, intl,
                            locname, round, thread) && success;

    return success;
}


static bool
check (const char* locname, int round, int thread,
       const void* expected_block, const void* got_block)
{
    const GetData& exp = *_RWSTD_STATIC_CAST (const GetData*, expected_block);
    const GetData& got = *_RWSTD_STATIC_CAST (const GetData*, got_block);

    bool success = true;

    for (int i = 0; i != NINPUTS; ++i) {
        for (int intl = 0; intl != 2; ++intl) {

            success = check_result ("char", got.narrow [i][intl],
                                    exp.narrow [i][intl], inputs [i],
                                    0 != intl, locname, round, thread)
                && success;

            success = check_result ("wchar_t", got.wide [i][intl],
                                    exp.wide [i][intl], inputs [i],
                                    0 != intl, locname, round, thread)
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
    // state of its own, and a gate before the parse would send every
    // thread into it before the data can be published too early
    // (doc/notes/analysis-facet-first-use.md, 2.4)
    return rw_first_use_rounds (locale_sources, NLOCALES,
                                expected, results, sizeof (GetData),
                                prepare, get_money, check,
                                "std::money_get<charT>::get ()", false);
}

/**************************************************************************/

int main (int argc, char *argv[])
{
    return rw_round_test (argc, argv, __FILE__,
                          "lib.locale.money.get",
                          "thread safety of the first parse", run_test);
}
