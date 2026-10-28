/***************************************************************************
 *
 * time.cpp - the library's time_put eras against the C library's
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
 ***************************************************************************
 *
 * A differential oracle for localedef: the era conversions of a
 * database the library's localedef built, read through
 * std::time_put<char> and std::time_put<wchar_t> with the facets
 * held to the library's own databases, against the C library's
 * strftime and wcsftime under setlocale for the same name. The C
 * library must have the locale generated. The dates are, for each
 * era record of the database, the day before, the day of and the
 * day after each bound that is not an end of time, and five fixed
 * dates: 0100-01-01, 1911-01-01, 2019-04-30, 2019-05-01 and
 * 2026-10-28. Each is formatted with %EC, %Ey and %EY, narrow and
 * wide. The output lists each disagreement as the date, the
 * conversion, the width and the two results, then one line with
 * the number of dates and the number of disagreements; the exit
 * status is 0 when nothing disagrees. The C library's name for the
 * locale is the library's unless -c gives another (ja_JP.utf8 for
 * ja_JP.UTF-8).
 *
 * The era records are read from the LC_TIME file under
 * RWSTD_LOCALE_ROOT through the library's own layout, _localedef.h,
 * so the bounds the dates are drawn from are the ones the runtime
 * reads: astronomical years, with a year 0 before year 1, the month
 * from 0 and the day from 1.
 *
 * Build, against a build directory $B of the library, with the
 * CPPFLAGS of its makefile.in (15D here, a debug build):
 *
 *   gcc -I$TOPDIR/include -I$TOPDIR/include/ansi -I$B/include \
 *       -D_RWSTDDEBUG -nostdinc++ -pthread -g time.cpp -o time-oracle \
 *       -L$B/lib -Wl,-R$B/lib -lstd15D -lsupc++ -lm
 *
 * Run, with the database for ja_JP.UTF-8 under $ROOT (from
 * "localedef -c -f UTF-8 -i ja_JP ja_JP.UTF-8" run in $ROOT; while
 * localedef2 writes the scalar categories only, its LC_TIME for the
 * same definition replaces the one in $ROOT/ja_JP.UTF-8):
 *
 *   RWSTD_LOCALE_ROOT=$ROOT ./time-oracle -c ja_JP.utf8 ja_JP.UTF-8
 *
 **************************************************************************/

#include <algorithm>
#include <locale>
#include <sstream>
#include <string>
#include <vector>

#include <climits>
#include <clocale>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <cwchar>

#include <loc/_localedef.h>


// a calendar date, the year astronomical (0 before 1), the month
// from 1 and the day from 1, in the proleptic Gregorian calendar
struct date_t
{
    int year;
    int mon;
    int mday;
};


static bool
operator< (const date_t& a, const date_t& b)
{
    if (a.year != b.year)
        return a.year < b.year;

    if (a.mon != b.mon)
        return a.mon < b.mon;

    return a.mday < b.mday;
}


static bool
operator== (const date_t& a, const date_t& b)
{
    return a.year == b.year && a.mon == b.mon && a.mday == b.mday;
}


static int
days_in_month (int year, int mon)
{
    static const int days [12] = {
        31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31
    };

    const bool leap = 0 == year % 4 && (0 != year % 100 || 0 == year % 400);

    return 2 == mon && leap ? 29 : days [mon - 1];
}


static date_t
next_day (date_t d)
{
    if (++d.mday > days_in_month (d.year, d.mon)) {
        d.mday = 1;

        if (++d.mon > 12) {
            d.mon = 1;
            ++d.year;
        }
    }

    return d;
}


static date_t
prev_day (date_t d)
{
    if (--d.mday < 1) {
        if (--d.mon < 1) {
            d.mon = 12;
            --d.year;
        }

        d.mday = days_in_month (d.year, d.mon);
    }

    return d;
}


// the library's LC_TIME for the locale must exist under the root:
// without one the facet would come from the C library and agree
// with it trivially; reads it whole and adds the three dates around
// each finite bound of each era record
static bool
read_era_dates (const char* name, std::vector<date_t>& dates)
{
    const char* const root = std::getenv ("RWSTD_LOCALE_ROOT");

    const std::string path =
        std::string (root ? root : ".") + '/' + name + "/LC_TIME";

    std::FILE* const f = std::fopen (path.c_str (), "rb");
    if (0 == f) {
        std::fprintf (stderr, "no database %s\n", path.c_str ());
        return false;
    }

    std::vector<char> image;

    char buf [4096];
    for (std::size_t n; 0 != (n = std::fread (buf, 1, sizeof buf, f)); )
        image.insert (image.end (), buf, buf + n);

    std::fclose (f);

    if (image.size () < sizeof (__rw::__rw_time_t)) {
        std::fprintf (stderr, "%s is too short\n", path.c_str ());
        return false;
    }

    const __rw::__rw_time_t* const ptime =
        reinterpret_cast< const __rw::__rw_time_t* >(&image [0]);

    for (std::size_t i = 0; i != ptime->era_count (); ++i) {

        const __rw::__rw_time_t::era_t* const pera = ptime->era (i);

        for (int k = 0; k != 2; ++k) {

            if (INT_MIN == pera->year [k] || INT_MAX == pera->year [k])
                continue;

            const date_t bound = {
                pera->year [k], pera->month [k] + 1, pera->day [k]
            };

            dates.push_back (prev_day (bound));
            dates.push_back (bound);
            dates.push_back (next_day (bound));
        }
    }

    return true;
}


template <class charT>
static std::basic_string<charT>
put (const std::locale& loc, const std::tm& t, const char* fmt)
{
    std::basic_ostringstream<charT> os;
    os.imbue (loc);

    charT pat [8];
    charT* pend = pat;
    for (const char* p = fmt; *p; ++p)
        *pend++ = charT (*p);

    const std::time_put<charT>& tp =
        std::use_facet<std::time_put<charT> >(loc);

    tp.put (std::ostreambuf_iterator<charT>(os), os, charT (' '),
            &t, pat, pend);

    return os.str ();
}


static std::string
narrow (const std::wstring& ws)
{
    char buf [256];

    const std::size_t n = std::wcstombs (buf, ws.c_str (), sizeof buf - 1);
    if (std::size_t (-1) == n)
        return "?";

    buf [n] = '\0';
    return buf;
}


int main (int argc, char* argv [])
{
    const char* libc_name = 0;

    int argi = 1;
    if (argi + 1 < argc && 0 == std::strcmp (argv [argi], "-c")) {
        libc_name = argv [argi + 1];
        argi += 2;
    }

    if (argc <= argi) {
        std::fprintf (stderr, "usage: %s [-c name] <locale-name>\n",
                      argv [0]);
        return 2;
    }

    const char* const name = argv [argi];

    if (0 == libc_name)
        libc_name = name;

    if (0 == std::setlocale (LC_ALL, libc_name)) {
        std::fprintf (stderr, "setlocale (LC_ALL, \"%s\") failed\n",
                      libc_name);
        return 2;
    }

    std::vector<date_t> dates;

    if (!read_era_dates (name, dates))
        return 2;

    static const date_t fixed [] = {
        {  100, 1,  1 }, { 1911, 1, 1 }, { 2019, 4, 30 }, { 2019, 5, 1 },
        { 2026, 10, 28 }
    };

    dates.insert (dates.end (), fixed, fixed + sizeof fixed / sizeof *fixed);

    std::sort (dates.begin (), dates.end ());
    dates.erase (std::unique (dates.begin (), dates.end ()), dates.end ());

    const std::locale loc (name);

    // the library's databases only, never the C library behind them
    const std::time_put<char>& tp = std::use_facet<std::time_put<char> >(loc);

    tp._C_opts |=  tp._C_use_libstd;
    tp._C_opts &= ~tp._C_use_libc;

    static const char* const fmts [] = { "%EC", "%Ey", "%EY" };

    unsigned long disagreements = 0;

    for (std::size_t i = 0; i != dates.size (); ++i) {

        const date_t& d = dates [i];

        std::tm t = std::tm ();
        t.tm_year = d.year - 1900;
        t.tm_mon  = d.mon - 1;
        t.tm_mday = d.mday;

        for (std::size_t j = 0; j != sizeof fmts / sizeof *fmts; ++j) {

            char buf [256];
            const std::size_t n = std::strftime (buf, sizeof buf, fmts [j], &t);

            const std::string libc (buf, n);
            const std::string ours = put<char> (loc, t, fmts [j]);

            if (ours != libc) {
                ++disagreements;
                std::printf ("  %d-%02d-%02d %s narrow: library \"%s\" "
                             "C library \"%s\"\n", d.year, d.mon, d.mday,
                             fmts [j], ours.c_str (), libc.c_str ());
            }

            wchar_t wfmt [8];
            std::mbstowcs (wfmt, fmts [j], sizeof wfmt / sizeof *wfmt);

            wchar_t wbuf [256];
            const std::size_t wn =
                std::wcsftime (wbuf, sizeof wbuf / sizeof *wbuf, wfmt, &t);

            const std::wstring wlibc (wbuf, wn);
            const std::wstring wours = put<wchar_t> (loc, t, fmts [j]);

            if (wours != wlibc) {
                ++disagreements;
                std::printf ("  %d-%02d-%02d %s wide: library \"%s\" "
                             "C library \"%s\"\n", d.year, d.mon, d.mday,
                             fmts [j], narrow (wours).c_str (),
                             narrow (wlibc).c_str ());
            }
        }
    }

    std::printf ("%s: %lu dates, %lu disagreements\n", name,
                 static_cast< unsigned long >(dates.size ()), disagreements);

    return disagreements ? 1 : 0;
}
