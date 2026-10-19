/************************************************************************
 *
 * 22.locale.moneypunct.mt.cpp
 *
 * test exercising the thread safety of the first use of a moneypunct
 * facet, which builds the facet's data from the C library
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

#include <locale>     // for locale, moneypunct, use_facet
#include <string>     // for basic_string
#include <cstring>    // for memcmp(), strcmp(), strlen()

#include <rw_locale.h>   // for rw_locales()
#include <rw_new.h>      // for rwt_check_leaks()
#include <rw_thread.h>   // for rw_thread_gate(), rw_thread_pool()
#include <rw_driver.h>   // for rw_test()


// maximum number of threads allowed by the command line interface
#define MAX_THREADS      32

// maximum number of locales the test cycles through
#define MAX_LOCALES      64

// the number of threads (4 in a thread-safe build, set in main ()
// whatever the number of processors)
int opt_nthreads = 1;

// the number of rounds, each a fresh locale
int opt_nrounds = 200;

// default timeout used by each threaded section of this test
int opt_timeout = 60;

/**************************************************************************/

// the locales the rounds cycle through, the C library's other than
// C and POSIX: the library's database has no data for them, so the
// facets build theirs from the C library under __rw_setlocale
static const char* locale_names [MAX_LOCALES];

// the number of names in the array
static std::size_t nlocales;

// one of the four facets, the round's: its record, taken by the main
// thread through a locale that is gone before the round's is
// constructed, the facet of the round's locale, and the checks
template <class charT, bool Intl>
struct Race
{
    typedef std::moneypunct<charT, Intl> Facet;
    typedef std::basic_string<charT>     String;

    static const Facet* facet;

    static charT                   decimal_point;
    static charT                   thousands_sep;
    static int                     frac_digits;
    static std::money_base::pattern pos_format;
    static std::money_base::pattern neg_format;
    static std::string             grouping;
    static String                  curr_symbol;
    static String                  positive_sign;
    static String                  negative_sign;

    static void record (const std::locale &loc) {
        const Facet &mp = std::use_facet<Facet>(loc);

        decimal_point = mp.decimal_point ();
        thousands_sep = mp.thousands_sep ();
        frac_digits   = mp.frac_digits ();
        pos_format    = mp.pos_format ();
        neg_format    = mp.neg_format ();
        grouping      = mp.grouping ();
        curr_symbol   = mp.curr_symbol ();
        positive_sign = mp.positive_sign ();
        negative_sign = mp.negative_sign ();
    }

    static void take (const std::locale &loc) {
        facet = &std::use_facet<Facet>(loc);
    }

    static void drop () {
        facet = 0;
    }

    // the members that allocate nothing, called by the threads
    static int race () {
        int bad = 0;

        if (facet->decimal_point () != decimal_point)
            ++bad;
        if (facet->thousands_sep () != thousands_sep)
            ++bad;
        if (facet->frac_digits () != frac_digits)
            ++bad;

        const std::money_base::pattern pos = facet->pos_format ();
        if (std::memcmp (&pos, &pos_format, sizeof pos))
            ++bad;

        const std::money_base::pattern neg = facet->neg_format ();
        if (std::memcmp (&neg, &neg_format, sizeof neg))
            ++bad;

        return bad;
    }

    // the string members, checked by the main thread after the round
    static bool strings () {
        return    facet->grouping ()      == grouping
               && facet->curr_symbol ()   == curr_symbol
               && facet->positive_sign () == positive_sign
               && facet->negative_sign () == negative_sign;
    }
};

template <class charT, bool Intl>
const std::moneypunct<charT, Intl>* Race<charT, Intl>::facet;

template <class charT, bool Intl>
charT Race<charT, Intl>::decimal_point;

template <class charT, bool Intl>
charT Race<charT, Intl>::thousands_sep;

template <class charT, bool Intl>
int Race<charT, Intl>::frac_digits;

template <class charT, bool Intl>
std::money_base::pattern Race<charT, Intl>::pos_format;

template <class charT, bool Intl>
std::money_base::pattern Race<charT, Intl>::neg_format;

template <class charT, bool Intl>
std::string Race<charT, Intl>::grouping;

template <class charT, bool Intl>
std::basic_string<charT> Race<charT, Intl>::curr_symbol;

template <class charT, bool Intl>
std::basic_string<charT> Race<charT, Intl>::positive_sign;

template <class charT, bool Intl>
std::basic_string<charT> Race<charT, Intl>::negative_sign;


// the four facets, in the order the rounds cycle through them
static const struct {
    void (*record)(const std::locale&);
    void (*take)(const std::locale&);
    void (*drop)();
    int  (*race)();
    bool (*strings)();
    const char* name;
} kinds [] = {
    { Race<char, false>::record,    Race<char, false>::take,
      Race<char, false>::drop,      Race<char, false>::race,
      Race<char, false>::strings,   "std::moneypunct<char, false>" },
    { Race<char, true>::record,     Race<char, true>::take,
      Race<char, true>::drop,       Race<char, true>::race,
      Race<char, true>::strings,    "std::moneypunct<char, true>" },
    { Race<wchar_t, false>::record, Race<wchar_t, false>::take,
      Race<wchar_t, false>::drop,   Race<wchar_t, false>::race,
      Race<wchar_t, false>::strings, "std::moneypunct<wchar_t, false>" },
    { Race<wchar_t, true>::record,  Race<wchar_t, true>::take,
      Race<wchar_t, true>::drop,    Race<wchar_t, true>::race,
      Race<wchar_t, true>::strings, "std::moneypunct<wchar_t, true>" }
};

static const std::size_t nkinds = sizeof kinds / sizeof *kinds;

// the round's facet
static std::size_t race_kind;

// the number of threads that have started in the current round
static int nstarted;

// per thread, the calls that disagreed with the record
static int mismatches [MAX_THREADS];

extern "C" {

// the replacement operator new of rw_new.h keeps no lock: the threads
// call the members that allocate nothing, and the only allocation
// they cause is the facet data's, under the lock of __rw_setlocale;
// the main thread checks the string members after the round
static void*
race_func (void *arg)
{
    const long inx = long (arg);

    // start with the other threads so that they find the facet's
    // data missing at the same time
    rw_thread_gate (&nstarted, opt_nthreads);

    mismatches [inx] = kinds [race_kind].race ();

    return 0;
}

}   // extern "C"

/**************************************************************************/

static int
run_test (int, char**)
{
    const char* const locale_list =
        rw_opt_locales ? rw_opt_locales : rw_locales (_RWSTD_LC_ALL);

    for (const char *name = locale_list;
         *name && nlocales != MAX_LOCALES;
         name += std::strlen (name) + 1) {

        if (std::strcmp (name, "C") && std::strcmp (name, "POSIX"))
            locale_names [nlocales++] = name;
    }

    if (0 == nlocales) {
        rw_warn (false, 0, __LINE__,
                 "no installed locale other than C and POSIX; skipping");
        return 0;
    }

    rw_info (0, 0, 0,
             "racing the first use of the four std::moneypunct facets, "
             "in turn, from %d thread%{?}s%{;}, %d round%{?}s%{;}, "
             "in %zu locale%{?}s%{;}",
             opt_nthreads, 1 != opt_nthreads,
             opt_nrounds, 1 != opt_nrounds,
             nlocales, 1 != nlocales);

    // a first round in this thread alone, every facet, for the
    // allocations the library makes once per process
    {
        const std::locale loc (locale_names [0]);

        for (std::size_t k = 0; k != nkinds; ++k)
            kinds [k].record (loc);
    }

    int result = 0;

    for (int i = 0; 0 == result && i != opt_nrounds; ++i) {

        const char* const name = locale_names [std::size_t (i) % nlocales];

        race_kind = std::size_t (i) % nkinds;

        // the record, through a locale of its own, before the
        // checkpoint: its strings outlive the round
        {
            const std::locale loc (name);
            kinds [race_kind].record (loc);
        }

        rwt_check_leaks (0, 0);

        {
            // each round a new locale, whose facet has no data yet;
            // the slot is filled from this thread alone, so that the
            // threads race only the data (the slot race is
            // 22.locale.use_facet.mt's)
            const std::locale loc (name);

            kinds [race_kind].take (loc);
            nstarted = 0;

            void* args [MAX_THREADS];
            for (int j = 0; j != opt_nthreads; ++j)
                args [j] = (void*)long (j);

            result = rw_thread_pool (0, std::size_t (opt_nthreads), 0,
                                     race_func, args,
                                     std::size_t (opt_timeout));

            rw_error (result == 0, 0, __LINE__,
                      "rw_thread_pool(0, %d, 0, %{#f}, ...) failed",
                      opt_nthreads, race_func);

            for (int j = 0; 0 == result && j != opt_nthreads; ++j) {
                rw_assert (0 == mismatches [j], 0, __LINE__,
                           "%s in std::locale (%#s), round %d, thread "
                           "%d: %d of the five scalar members disagree "
                           "with the record",
                           kinds [race_kind].name, name, i, j,
                           mismatches [j]);
            }

            // the string members, from the data the threads built
            rw_assert (kinds [race_kind].strings (), 0, __LINE__,
                       "%s in std::locale (%#s), round %d: grouping(), "
                       "curr_symbol(), positive_sign() and "
                       "negative_sign() agree with the record",
                       kinds [race_kind].name, name, i);

            kinds [race_kind].drop ();
        }

        // the facet and its data die with the only locale that holds
        // them: no thread built data that was then overwritten
        std::size_t nbytes;
        const std::size_t nblocks = rwt_check_leaks (&nbytes, 0);

        if (!rw_assert (0 == nblocks, 0, __LINE__,
                        "%s in std::locale (%#s) from %d threads: "
                        "%zu blocks (%zu bytes) outlive the locale "
                        "in round %d",
                        kinds [race_kind].name, name, opt_nthreads,
                        nblocks, nbytes, i))
            break;
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
                    "lib.locale.moneypunct",
                    "thread safety", run_test,
                    "|-soft-timeout#0 "  // must be non-negative
                    "|-nrounds#0 "       // must be non-negative
                    "|-nthreads#0-* "    // must be in [0, MAX_THREADS]
                    "|-locales= ",       // must be provided
                    &opt_timeout,
                    &opt_nrounds,
                    int (MAX_THREADS),
                    &opt_nthreads,
                    &rw_opt_setlocales);
}
