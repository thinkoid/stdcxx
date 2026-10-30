/***************************************************************************
 *
 * 22.locale.time.get.cpp - tests exercising the std::time_get facet
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
 * Copyright 2001-2008 Rogue Wave Software, Inc.
 * 
 **************************************************************************/

#include <locale>
#include <sstream>   // for stringstream

#include <climits>   // for INT_{MIN,MAX}
#include <clocale>   // for LC_ALL
#include <cstdio>    // for fprintf()
#include <cstdlib>   // for getenv()
#include <cstring>   // for memcmp(), strlen()
#include <ctime>     // for struct tm

#include <langinfo.h>    // for nl_langinfo(), D_FMT

#include <rw_driver.h>   // for rw_test()
#include <rw_file.h>     // for rw_nextfd()
#include <rw_locale.h>   // for rw_locale_query()
#include <rw_printf.h>   // for rw_snprintf()

/**************************************************************************/

template <class charT>
void do_test (int lineno, charT, const char *cname,
              std::tm val, const char *pat, int consumed,
              const char *fmt, int flags, int err_expect = -1)
{
    std::basic_ostringstream<charT> ostrm;

    typedef std::istreambuf_iterator<charT> InIter;
    typedef std::ostreambuf_iterator<charT> OutIter;

    const std::time_get<charT, InIter> &tg =
        std::use_facet<std::time_get<charT, InIter> >(ostrm.getloc ());

    const std::time_put<charT, OutIter> &tp =
        std::use_facet<std::time_put<charT, OutIter> >(ostrm.getloc ());

    ostrm.flags (std::ios_base::fmtflags (flags));

    charT wpatbuf [256];
    for (std::size_t j = 0; !!(wpatbuf [j] = pat [j]); ++j);

    const charT *wpat    = wpatbuf;
    const charT *wpatend = wpat + std::char_traits<charT>::length (wpat);

    // format string
    OutIter it (ostrm);
    tp.put (it, ostrm, charT (' '), &val, wpat, wpatend);

    // working around dumb compilers such as MSVC 6.0 that
    // do not zero initialize POD structs when using tm()
    static std::tm zero_tm;
    std::tm x = zero_tm;

    std::basic_istringstream<charT> istrm (ostrm.str ());

    istrm.flags (std::ios_base::fmtflags (flags));

    std::ios_base::iostate err = std::ios_base::goodbit;

    const char *fname = 0;

    InIter ibeg = InIter (istrm);
    InIter iend;

    try {
        if (   '\0' == fmt [1]
            || ('E' ==  fmt [0] || ('O' == fmt [0] && '\0' == fmt [2]))) {

            // single-character format strings "a", "b", "x", "X", and "Y"
            // exercise the standard time_get interface, i.e., get_weekday(),
            // get_monthname(), and get_year()

            // other single or two character format strings, e.g., "Ec" or "Ex"
            // exercise the single character extension (format specifier and
            // an optional modifier)

            switch (*fmt) {
            case 'a':
                fname = "get_weekday";
                iend  = tg.get_weekday (ibeg, iend, istrm, err, &x);
                break;

            case 'b':
                fname = "get_monthname";
                iend  = tg.get_monthname (ibeg, iend, istrm, err, &x);
                break;

            case 'x':
                fname = "get_date";
                iend  = tg.get_date (ibeg, iend, istrm, err, &x);
                break;

            case 'X':
                fname = "get_time";
                iend  = tg.get_time (ibeg, iend, istrm, err, &x);
                break;

            case 'Y':
                fname = "get_year";
                iend  = tg.get_year (ibeg, iend, istrm, err, &x);
                break;

            default:
                fname = "get";
                if ('E' == fmt [0] || 'O' == fmt [0])
                    iend = tg.get (ibeg, iend, istrm, err, &x,
                                   fmt [1], fmt [0]);
                else
                    iend = tg.get (ibeg, iend, istrm, err, &x, *fmt);
                break;
            }
        }
        else {
            // multi-character format strings exercise the pattern
            // extensions to time_get, ala strptime()

            charT fmtstr [256];
            charT *end;

            const char *pfmt = fmt;
            for (end = fmtstr; *pfmt; *end++ = *pfmt++);

            fname = "get";
            iend  = tg.get (ibeg, iend, istrm, err, &x, fmtstr, end);
        }
    }
    catch (...) {
        rw_error (0, 0, lineno,
                  "line %d. time_get<%s>::%s (%{*Ac}, ..., \"%s\") "
                  "unexpectedly threw an exception",
                  __LINE__, cname, fname,
                  int (sizeof (charT)), ostrm.str ().data (), fmt);

        return;
    }

    int extracted = -1 == consumed ? -1
        : int (istrm.rdbuf ()->pubseekoff (0, std::ios::cur, std::ios::in));

    if (extracted == -1)
        consumed = -1;

    int success =
        extracted == consumed && (-1 == err_expect || err == err_expect);

    rw_assert (success, __FILE__, lineno,
               "line %d. time_get<%s>::%s (%{*Ac}, ..., \"%s\") "
               "ate %d, expected %d, rdstate() == %{Is}, got %{Is}",
               __LINE__, cname, fname,
               int (sizeof (charT)), ostrm.str ().data (), fmt,
               extracted, consumed, err_expect, err);

    success = 0 == std::memcmp (&x, &val, sizeof x);

    rw_assert (success, __FILE__, lineno,
               "line %d. time_get<%s>::%s (%{*Ac}, ..., \"%s\") got %{t}, "
               "expected %{t}, flags = %{If}, locale (%s)",
               __LINE__, cname, fname,
               int (sizeof (charT)), ostrm.str ().data (), fmt, &x,
               &val, flags, std::locale ().name ().data ());
}


/**************************************************************************/

std::tm mktm (int sec = 0, int min = 0, int hour = 0,
              int mday = 0, int mon = 0, int year = 0,
              int wday = 0, int yday = 0, int isdst = 0,
              long gmtoff = 0)
{
    std::tm tmp = std::tm ();

    if (sec < 0) {
        // get the current local time
        std::time_t t = std::time (0);
        std::tm *ptm = std::localtime (&t);
        return ptm ? *ptm : tmp;
    }

    // use arguments to initialize struct
    tmp.tm_sec   = sec;     // seconds after the minute [0, 60]
    tmp.tm_min   = min;     // minutes after the hour   [0, 59]
    tmp.tm_hour  = hour;    // hours since midnight     [0, 23]
    tmp.tm_mday  = mday;    // day of the month         [1, 31]
    tmp.tm_mon   = mon;     // months since January     [0, 11]
    tmp.tm_year  = year;    // years since 1900
    tmp.tm_wday  = wday;    // days since Sunday        [0, 6]
    tmp.tm_yday  = yday;    // days since January 1     [0, 365]
    tmp.tm_isdst = isdst;   // Daylight Saving Time flag

#if !defined (_RWSTD_NO_TM_GMTOFF) && defined (_RWSTD_NO_PURE_C_HEADERS)

    // glibc extension, read by %z
    tmp.tm_gmtoff = gmtoff;   // offset from UTC in seconds

#else   // _RWSTD_NO_TM_GMTOFF || !_RWSTD_NO_PURE_C_HEADERS

    _RWSTD_UNUSED (gmtoff);

#endif   // !_RWSTD_NO_TM_GMTOFF && _RWSTD_NO_PURE_C_HEADERS

    return tmp;
}


// for convenience
#define Boolalpha   std::ios_base::boolalpha
#define Dec         std::ios_base::dec
#define Fixed       std::ios_base::fixed
#define Hex         std::ios_base::hex
#define Internal    std::ios_base::internal
#define Left        std::ios_base::left
#define Oct         std::ios_base::oct
#define Right       std::ios_base::right
#define Scientific  std::ios_base::scientific
#define Showbase    std::ios_base::showbase
#define Showpoint   std::ios_base::showpoint
#define Showpos     std::ios_base::showpos
#define Skipws      std::ios_base::skipws
#define Unitbuf     std::ios_base::unitbuf
#define Uppercase   std::ios_base::uppercase
#define Adjustfield std::ios_base::adjustfield
#define Basefield   std::ios_base::basefield
#define Floatfield  std::ios_base::floatfield

// capitalize to prevent EDG eccp from expanding the macros
// in .cc files when implicit inclusion is used (causes errors
// in constructs such as traits_type::eof ())
#define Bad         std::ios_base::badbit
#define Eof         std::ios_base::eofbit
#define Fail        std::ios_base::failbit
#define Good        std::ios_base::goodbit


#define T __LINE__, charT (), cname, mktm
#define TEST do_test

#define FUNCTION(fun)                                           \
    rw_info (0, 0, __LINE__,                                    \
             "std::time_get<%s>::%s() in locale (\"%s\")",      \
             cname, fun, std::locale ().name ().c_str ())

#define STEP(desc)   rw_info (0, 0, __LINE__, "%s", desc)


template <class charT>
void test_posix (charT, const char *cname)
{
    // set the global locale object
    std::locale::global (std::locale ("C"));

    // 22.2.5.1.2, p1: the order of the components of the x format
    rw_assert (std::time_base::mdy == std::use_facet<std::time_get<charT> >(
                   std::locale ()).date_order (), 0, __LINE__,
               "std::time_get<%s>::date_order() in locale (\"%s\") "
               "== %d, got %d", cname, std::locale ().name ().c_str (),
               std::time_base::mdy, std::use_facet<std::time_get<charT> >(
                   std::locale ()).date_order ());

    // exercise abbreviated weekday names (time_get::get_weekday())
    FUNCTION ("get_weekday");

    TEST (T (0, 0, 0, 0, 0, 0, 0), "%a", -1, "a", 0, Eof);
    TEST (T (0, 0, 0, 0, 0, 0, 1), "%a", -1, "a", 0, Eof);
    TEST (T (0, 0, 0, 0, 0, 0, 2), "%a", -1, "a", 0, Eof);
    TEST (T (0, 0, 0, 0, 0, 0, 3), "%a", -1, "a", 0, Eof);
    TEST (T (0, 0, 0, 0, 0, 0, 4), "%a", -1, "a", 0, Eof);
    TEST (T (0, 0, 0, 0, 0, 0, 5), "%a", -1, "a", 0, Eof);
    TEST (T (0, 0, 0, 0, 0, 0, 6), "%a", -1, "a", 0, Eof);

    // comparison is case insensitive
    TEST (T (0, 0, 0, 0, 0, 0, 0), "Sun", 3, "a", 0, Eof);
    TEST (T (0, 0, 0, 0, 0, 0, 0), "SUN", 3, "a", 0, Eof);
    TEST (T (0, 0, 0, 0, 0, 0, 0), "sun", 3, "a", 0, Eof);
    TEST (T (0, 0, 0, 0, 0, 0, 1), "Mon", 3, "a", 0, Eof);
    TEST (T (0, 0, 0, 0, 0, 0, 2), "Tue", 3, "a", 0, Eof);
    TEST (T (0, 0, 0, 0, 0, 0, 3), "Wed", 3, "a", 0, Eof);
    TEST (T (0, 0, 0, 0, 0, 0, 4), "Thu", 3, "a", 0, Eof);
    TEST (T (0, 0, 0, 0, 0, 0, 5), "Fri", 3, "a", 0, Eof);
    TEST (T (0, 0, 0, 0, 0, 0, 6), "Sat", 3, "a", 0, Eof);

    // abbreviation followed by a letter other than the one
    // that follows in a full name is acepted
    TEST (T (0, 0, 0, 0, 0, 0, 0), "suna", 3, "a", 0, Good);
    TEST (T (0, 0, 0, 0, 0, 0, 1), "monb", 3, "a", 0, Good);
    TEST (T (0, 0, 0, 0, 0, 0, 2), "tuec", 3, "a", 0, Good);
    TEST (T (0, 0, 0, 0, 0, 0, 3), "wedd", 3, "a", 0, Good);
    TEST (T (0, 0, 0, 0, 0, 0, 4), "thue", 3, "a", 0, Good);
    TEST (T (0, 0, 0, 0, 0, 0, 5), "frif", 3, "a", 0, Good);
    TEST (T (0, 0, 0, 0, 0, 0, 6), "satg", 3, "a", 0, Good);

    // exercise failure on invalid input including incomplete
    // full names (other than valid cases exercised above)
    TEST (T (0, 0, 0, 0, 0, 0, 0), " Sun",      0, "a", 0, Fail);
    TEST (T (0, 0, 0, 0, 0, 0, 0), "Mond",      4, "a", 0, Eof | Fail);
    TEST (T (0, 0, 0, 0, 0, 0, 0), "Tuesd",     5, "a", 0, Eof | Fail);
    TEST (T (0, 0, 0, 0, 0, 0, 0), "Wednesd",   7, "a", 0, Eof | Fail);
    TEST (T (0, 0, 0, 0, 0, 0, 0), "Thursda.",  7, "a", 0, Fail);
    TEST (T (0, 0, 0, 0, 0, 0, 0), "Fridax",    5, "a", 0, Fail);
    TEST (T (0, 0, 0, 0, 0, 0, 0), "6Saturday", 0, "a", 0, Fail);

    // exercise full weekday names
    TEST (T (0, 0, 0, 0, 0, 0, 0), "%A", -1, "a", 0, -1);
    TEST (T (0, 0, 0, 0, 0, 0, 1), "%A", -1, "a", 0, -1);
    TEST (T (0, 0, 0, 0, 0, 0, 2), "%A", -1, "a", 0, -1);
    TEST (T (0, 0, 0, 0, 0, 0, 3), "%A", -1, "a", 0, -1);
    TEST (T (0, 0, 0, 0, 0, 0, 4), "%A", -1, "a", 0, -1);
    TEST (T (0, 0, 0, 0, 0, 0, 5), "%A", -1, "a", 0, -1);
    TEST (T (0, 0, 0, 0, 0, 0, 6), "%A", -1, "a", 0, -1);

    // comparison is case insensitive
    TEST (T (0, 0, 0, 0, 0, 0, 0), "Sunday",    6, "a", 0, Good);
    TEST (T (0, 0, 0, 0, 0, 0, 0), "SUNDAY",    6, "a", 0, Good);
    TEST (T (0, 0, 0, 0, 0, 0, 0), "sunday",    6, "a", 0, Good);
    TEST (T (0, 0, 0, 0, 0, 0, 1), "Monday",    6, "a", 0, Good);
    TEST (T (0, 0, 0, 0, 0, 0, 2), "Tuesday",   7, "a", 0, Good);
    TEST (T (0, 0, 0, 0, 0, 0, 3), "Wednesday", 9, "a", 0, Good);
    TEST (T (0, 0, 0, 0, 0, 0, 4), "Thursday",  8, "a", 0, Good);
    TEST (T (0, 0, 0, 0, 0, 0, 5), "Friday",    6, "a", 0, Good);
    TEST (T (0, 0, 0, 0, 0, 0, 6), "Saturday",  8, "a", 0, Good);

    TEST (T (0, 0, 0, 0, 0, 0, 0), "Sunday ",    6, "a", 0, Good);
    TEST (T (0, 0, 0, 0, 0, 0, 1), "Monday 0",   6, "a", 0, Good);
    TEST (T (0, 0, 0, 0, 0, 0, 2), "Tuesdayy",   7, "a", 0, Good);
    TEST (T (0, 0, 0, 0, 0, 0, 3), "Wednesday.", 9, "a", 0, Good);
    TEST (T (0, 0, 0, 0, 0, 0, 4), "Thursday,",  8, "a", 0, Good);
    TEST (T (0, 0, 0, 0, 0, 0, 5), "Friday/",    6, "a", 0, Good);
    TEST (T (0, 0, 0, 0, 0, 0, 6), "Saturday:",  8, "a", 0, Good);

    // exercise abbreviated month names (time_get::get_monthname())
    FUNCTION ("get_monthname");
    TEST (T (0, 0, 0, 0,  0), "%b", -1, "b", 0, -1);
    TEST (T (0, 0, 0, 0,  1), "%b", -1, "b", 0, -1);
    TEST (T (0, 0, 0, 0,  2), "%b", -1, "b", 0, -1);
    TEST (T (0, 0, 0, 0,  3), "%b", -1, "b", 0, -1);
    TEST (T (0, 0, 0, 0,  4), "%b", -1, "b", 0, -1);
    TEST (T (0, 0, 0, 0,  5), "%b", -1, "b", 0, -1);
    TEST (T (0, 0, 0, 0,  6), "%b", -1, "b", 0, -1);
    TEST (T (0, 0, 0, 0,  7), "%b", -1, "b", 0, -1);
    TEST (T (0, 0, 0, 0,  8), "%b", -1, "b", 0, -1);
    TEST (T (0, 0, 0, 0,  9), "%b", -1, "b", 0, -1);
    TEST (T (0, 0, 0, 0, 10), "%b", -1, "b", 0, -1);
    TEST (T (0, 0, 0, 0, 11), "%b", -1, "b", 0, -1);

    TEST (T (0, 0, 0, 0,  0), "Jan", 3, "b", 0, Eof);
    TEST (T (0, 0, 0, 0,  1), "Feb", 3, "b", 0, Eof);
    TEST (T (0, 0, 0, 0,  2), "Mar", 3, "b", 0, Eof);
    TEST (T (0, 0, 0, 0,  3), "Apr", 3, "b", 0, Eof);
    TEST (T (0, 0, 0, 0,  4), "May", 3, "b", 0, Good);
    TEST (T (0, 0, 0, 0,  5), "Jun", 3, "b", 0, Eof);
    TEST (T (0, 0, 0, 0,  6), "Jul", 3, "b", 0, Eof);
    TEST (T (0, 0, 0, 0,  7), "Aug", 3, "b", 0, Eof);
    TEST (T (0, 0, 0, 0,  8), "Sep", 3, "b", 0, Eof);
    TEST (T (0, 0, 0, 0,  9), "Oct", 3, "b", 0, Eof);
    TEST (T (0, 0, 0, 0, 10), "Nov", 3, "b", 0, Eof);
    TEST (T (0, 0, 0, 0, 11), "Dec", 3, "b", 0, Eof);

    // exercise invalid abbreviated month names
    TEST (T (0, 0, 0, 0,  0), "J",   1, "b", 0, Eof | Fail);
    TEST (T (0, 0, 0, 0,  0), "J ",  1, "b", 0, Fail);
    TEST (T (0, 0, 0, 0,  0), "Ja",  2, "b", 0, Eof | Fail);
    TEST (T (0, 0, 0, 0,  0), "Ju",  2, "b", 0, Eof | Fail);

    // exercise full month names
    TEST (T (0, 0, 0, 0,  0), "%B", -1, "b", 0, -1);
    TEST (T (0, 0, 0, 0,  1), "%B", -1, "b", 0, -1);
    TEST (T (0, 0, 0, 0,  2), "%B", -1, "b", 0, -1);
    TEST (T (0, 0, 0, 0,  3), "%B", -1, "b", 0, -1);
    TEST (T (0, 0, 0, 0,  4), "%B", -1, "b", 0, -1);
    TEST (T (0, 0, 0, 0,  5), "%B", -1, "b", 0, -1);
    TEST (T (0, 0, 0, 0,  6), "%B", -1, "b", 0, -1);
    TEST (T (0, 0, 0, 0,  7), "%B", -1, "b", 0, -1);
    TEST (T (0, 0, 0, 0,  8), "%B", -1, "b", 0, -1);
    TEST (T (0, 0, 0, 0,  9), "%B", -1, "b", 0, -1);
    TEST (T (0, 0, 0, 0, 10), "%B", -1, "b", 0, -1);
    TEST (T (0, 0, 0, 0, 11), "%B", -1, "b", 0, -1);

    TEST (T (0, 0, 0, 0,  0), "January",   7, "b", 0, Good);
    TEST (T (0, 0, 0, 0,  0), "January ",  7, "b", 0, Good);
    TEST (T (0, 0, 0, 0,  1), "February",  8, "b", 0, Good);
    TEST (T (0, 0, 0, 0,  2), "March",     5, "b", 0, Good);
    TEST (T (0, 0, 0, 0,  2), "Marchh",    5, "b", 0, Good);
    TEST (T (0, 0, 0, 0,  3), "April",     5, "b", 0, Good);
    TEST (T (0, 0, 0, 0,  4), "May",       3, "b", 0, Good);
    TEST (T (0, 0, 0, 0,  5), "June",      4, "b", 0, Good);
    TEST (T (0, 0, 0, 0,  6), "July",      4, "b", 0, Good);
    TEST (T (0, 0, 0, 0,  6), "Jull",      3, "b", 0, Good);
    TEST (T (0, 0, 0, 0,  7), "August",    6, "b", 0, Good);
    TEST (T (0, 0, 0, 0,  8), "September", 9, "b", 0, Good);
    TEST (T (0, 0, 0, 0,  9), "October",   7, "b", 0, Good);
    TEST (T (0, 0, 0, 0, 10), "November",  8, "b", 0, Good);
    TEST (T (0, 0, 0, 0, 11), "December",  8, "b", 0, Good);

    // exercise invalid full month names
    TEST (T (0, 0, 0, 0, 0), "Januar",    6, "b", 0, Eof | Fail);
    TEST (T (0, 0, 0, 0, 0), "Februar",   7, "b", 0, Eof | Fail);
    TEST (T (0, 0, 0, 0, 0), " March",    0, "b", 0, Fail);
    TEST (T (0, 0, 0, 0, 0), "apri/",     4, "b", 0, Fail);
    TEST (T (0, 0, 0, 0, 0), "/MAY",      0, "b", 0, Fail);
    TEST (T (0, 0, 0, 0, 0), ":Jun",      0, "b", 0, Fail);
    TEST (T (0, 0, 0, 0, 0), "Ju ",       2, "b", 0, Fail);

    // exercise date (time_get::get_date())
    FUNCTION ("get_date");
    TEST (T (0, 0, 0,  1, 0, 100), "%x", 8, "x", 0, Eof);
    TEST (T (0, 0, 0,  2, 1, 111), "%x", 8, "x", 0, Eof);
    TEST (T (0, 0, 0,  3, 2, 122), "%x", 8, "x", 0, Eof);
    TEST (T (0, 0, 0,  4, 3, 133), "%x", 8, "x", 0, Eof);
    TEST (T (0, 0, 0,  5, 4, 144), "%x", 8, "x", 0, Eof);
    TEST (T (0, 0, 0,  6, 5, 155), "%x", 8, "x", 0, Eof);
    TEST (T (0, 0, 0,  7, 6, 166), "%x", 8, "x", 0, Eof);
    TEST (T (0, 0, 0,  8, 7, 168), "%x", 8, "x", 0, Eof);
    TEST (T (0, 0, 0,  9, 8,  69), "%x", 8, "x", 0, Eof);
    TEST (T (0, 0, 0, 10, 9,  99), "%x", 8, "x", 0, Eof);

    TEST (T (0, 0, 0, 11,  5, 155), "06/11/55", 8, "x", 0, Eof);
    TEST (T (0, 0, 0, 12,  6, 167), "07/12/67", 8, "x", 0, Eof);
    TEST (T (0, 0, 0, 23,  7,  78), "08/23/78", 8, "x", 0, Eof);
    TEST (T (0, 0, 0, 31, 11,  89), "12/31/89", 8, "x", 0, Eof);

    // exercise time (time_get::get_time())
    FUNCTION ("get_time");
    TEST (T ( 0,  0,  0), "%X", 8, "X", 0, Eof);
    TEST (T ( 1, 59, 11), "%X", 8, "X", 0, Eof);
    TEST (T (12, 48, 10), "%X", 8, "X", 0, Eof);
    TEST (T (23, 37,  9), "%X", 8, "X", 0, Eof);
    TEST (T (34, 26,  8), "%X", 8, "X", 0, Eof);
    TEST (T (45, 15,  7), "%X", 8, "X", 0, Eof);
    TEST (T (56,  4,  6), "%X", 8, "X", 0, Eof);

    TEST (T ( 0,  0,  0), "00:00:00", 8, "X", 0, Eof);
    TEST (T (60,  9, 23), "23:09:60", 8, "X", 0, Eof);
    TEST (T (59, 18, 22), "22:18:59", 8, "X", 0, Eof);
    TEST (T (58, 27, 21), "21:27:58", 8, "X", 0, Eof);
    TEST (T (57, 36, 20), "20:36:57", 8, "X", 0, Eof);
    TEST (T (56, 45, 19), "19:45:56", 8, "X", 0, Eof);
    TEST (T (55, 54, 18), "18:54:55", 8, "X", 0, Eof);

    // exercise year (time_get::get_year())
    FUNCTION ("get_year");
    TEST (T (0, 0, 0, 0, 0, -1900),     "0", 1, "Y", 0, Eof);
    TEST (T (0, 0, 0, 0, 0, -1900),    "00", 2, "Y", 0, Eof);
    TEST (T (0, 0, 0, 0, 0, -1899),   "001", 3, "Y", 0, Eof);
    TEST (T (0, 0, 0, 0, 0,  -124),  "1776", 4, "Y", 0, Eof);
    TEST (T (0, 0, 0, 0, 0,     0),  "1900", 4, "Y", 0, Eof);
    TEST (T (0, 0, 0, 0, 0,     1),  "1901", 4, "Y", 0, Eof);
    TEST (T (0, 0, 0, 0, 0,    99),  "1999", 4, "Y", 0, Eof);
    TEST (T (0, 0, 0, 0, 0,   102),  "2002", 4, "Y", 0, Eof);
    TEST (T (0, 0, 0, 0, 0,  8099), "99999", 4, "Y", 0, Good);

    FUNCTION ("get");

    // exercise extensions implementing complete IEEE Std 1003.1-2001 support
    // http://www.opengroup.org/onlinepubs/007904975/functions/strptime.html

    // capital format specifier (other than "X") implies the
    // testing of std::time_get<>::get (..., char, char)

    // %a The day of the week, using the locale's weekday names;
    //    either the abbreviated or full name may be specified.
    // %A Equivalent to %a.
    STEP ("%a, %A: the day of the week");
    TEST (T (0, 0, 0, 0, 0, 0, 0), "Sun", 3, "A", 0, Eof);
    TEST (T (0, 0, 0, 0, 0, 0, 1), "Mon", 3, "A", 0, Eof);
    TEST (T (0, 0, 0, 0, 0, 0, 2), "Tue", 3, "A", 0, Eof);
    TEST (T (0, 0, 0, 0, 0, 0, 3), "Wed", 3, "A", 0, Eof);
    TEST (T (0, 0, 0, 0, 0, 0, 4), "Thu", 3, "A", 0, Eof);
    TEST (T (0, 0, 0, 0, 0, 0, 5), "Fri", 3, "A", 0, Eof);
    TEST (T (0, 0, 0, 0, 0, 0, 6), "Sat", 3, "A", 0, Eof);

    TEST (T (0, 0, 0, 0, 0, 0, 0), "Sunday",    6, "A", 0, Good);
    TEST (T (0, 0, 0, 0, 0, 0, 1), "Monday",    6, "A", 0, Good);
    TEST (T (0, 0, 0, 0, 0, 0, 2), "Tuesday",   7, "A", 0, Good);
    TEST (T (0, 0, 0, 0, 0, 0, 3), "Wednesday", 9, "A", 0, Good);
    TEST (T (0, 0, 0, 0, 0, 0, 4), "Thursday",  8, "A", 0, Good);
    TEST (T (0, 0, 0, 0, 0, 0, 5), "Friday",    6, "A", 0, Good);
    TEST (T (0, 0, 0, 0, 0, 0, 6), "Saturday",  8, "A", 0, Good);

    // %b The month, using the locale's month names;
    //    either the abbreviated or full name may be specified.
    // %B Equivalent to %b.
    STEP ("%b, %B: the month");
    TEST (T (0, 0, 0, 0,  0), "January",   7, "B", 0, Good);
    TEST (T (0, 0, 0, 0,  1), "February",  8, "B", 0, Good);
    TEST (T (0, 0, 0, 0,  2), "March",     5, "B", 0, Good);
    TEST (T (0, 0, 0, 0,  3), "April",     5, "B", 0, Good);
    TEST (T (0, 0, 0, 0,  4), "May",       3, "B", 0, Good);
    TEST (T (0, 0, 0, 0,  5), "June",      4, "B", 0, Good);
    TEST (T (0, 0, 0, 0,  6), "July",      4, "B", 0, Good);
    TEST (T (0, 0, 0, 0,  7), "August",    6, "B", 0, Good);
    TEST (T (0, 0, 0, 0,  8), "September", 9, "B", 0, Good);
    TEST (T (0, 0, 0, 0,  9), "October",   7, "B", 0, Good);
    TEST (T (0, 0, 0, 0, 10), "November",  8, "B", 0, Good);
    TEST (T (0, 0, 0, 0, 11), "December",  8, "B", 0, Good);

    // %c Replaced by the locale's appropriate date and time representation.
    STEP ("%c: date and time representation");
    rw_warn (0, 0, __LINE__, "%%c not being exercised");


    // %C: The century number [00,99]; leading zeros are permitted
    //     but not required.
    STEP ("%C: the century number");
    TEST (T (0, 0, 0, 0, 0, -1900),   "0", 1, "C", 0, Eof);
    TEST (T (0, 0, 0, 0, 0, -1800),   "1", 1, "C", 0, Eof);
    TEST (T (0, 0, 0, 0, 0, -1700),   "2", 1, "C", 0, Eof);
    TEST (T (0, 0, 0, 0, 0, -1600),   "3", 1, "C", 0, Eof);
    TEST (T (0, 0, 0, 0, 0, -1500),  "04", 2, "C", 0, Eof);
    TEST (T (0, 0, 0, 0, 0,     0),  "19", 2, "C", 0, Eof);
    TEST (T (0, 0, 0, 0, 0,   100),  "20", 2, "C", 0, Eof);
    TEST (T (0, 0, 0, 0, 0,   200),  "21", 2, "C", 0, Eof);
    TEST (T (0, 0, 0, 0, 0,  1300),  "32", 2, "C", 0, Eof);
    TEST (T (0, 0, 0, 0, 0,  8000),  "99", 2, "C", 0, Eof);
    // at most two digits, as in glibc's strptime
    TEST (T (0, 0, 0, 0, 0,  -900),  "100", 2, "C", 0, Good);
    TEST (T (0, 0, 0, 0, 0,     0), "1926", 2, "C", 0, Good);

    // %d The day of the month [01,31]; leading zeros are permitted but not
    //    required.
    STEP ("%d: day of the month");
    TEST (T (0, 0, 0,  1),    "1", 1, "d", 0, Eof);
    TEST (T (0, 0, 0,  2),   "02", 2, "d", 0, Eof);
    TEST (T (0, 0, 0, 28),   "28", 2, "d", 0, Eof);
    TEST (T (0, 0, 0, 29),   "29", 2, "d", 0, Eof);
    // at most two digits, as in glibc's strptime
    TEST (T (0, 0, 0,  3),  "030", 2, "d", 0, Good);
    TEST (T (0, 0, 0,  0), "0031", 2, "d", 0, Fail);
    TEST (T (0, 0, 0,  0),    "0", 1, "d", 0, Eof | Fail);
    TEST (T (0, 0, 0,  0),   "32", 2, "d", 0, Eof | Fail);
    // a digit that would pass the bound is left, as in glibc's strptime
    TEST (T (0, 0, 0,  4),   "45", 1, "d", 0, Good);
    // leading whitespace skipped, as in glibc's strptime
    TEST (T (0, 0, 0,  3),   " 3", 2, "d", 0, Eof);
    TEST (T (0, 0, 0,  3),  "\t3", 2, "d", 0, Eof);
    TEST (T (0, 0, 0, 12), "  12", 4, "d", 0, Eof);
    TEST (T (0, 0, 0,  0),    " ", 1, "d", 0, Eof | Fail);
    TEST (T (0, 0, 0,  0),   " x", 1, "d", 0, Fail);

    // %D The date as %m/%d/%y.
    STEP ("%D: the date as %m/%d/%y");
    TEST (T (0, 0, 0, 14, 3, 102),      "4/14/2",  6, "D", 0, Eof);
    TEST (T (0, 0, 0, 14, 3, 102),    "04/14/02",  8, "D", 0, Eof);
    TEST (T (0, 0, 0, 14, 3, 100),   "04/14/002",  8, "D", 0, Good);
    TEST (T (0, 0, 0,  0, 0,   0),    "0/1/2002",  1, "D", 0, Fail);
    TEST (T (0, 0, 0,  0, 0,   0),    "4/0/2002",  3, "D", 0, Fail);
    TEST (T (0, 0, 0,  0, 0,   0),   "4/32/2002",  4, "D", 0, Fail);

    // %e Equivalent to %d; leading zeros are permitted but not required.
    STEP ("%e: equivalent to %d");
    TEST (T (0, 0, 0,  1),  "01", 2, "e", 0, Eof);
    TEST (T (0, 0, 0,  9),   "9", 1, "e", 0, Eof);
    TEST (T (0, 0, 0, 31),  "31", 2, "e", 0, Eof);
    TEST (T (0, 0, 0,  0),   "0", 1, "e", 0, Eof | Fail);
    // leading whitespace skipped, as in glibc's strptime
    TEST (T (0, 0, 0,  2),  " 2", 2, "e", 0, Eof);
    // a digit that would pass the bound is left, as in glibc's strptime
    TEST (T (0, 0, 0,  9),  "99", 1, "e", 0, Good);

    // %h Equivalent to %b.
    STEP ("%h: equivalent to %b");
    TEST (T (0, 0, 0, 0,  0), "Jan",      3, "h", 0, Eof);
    TEST (T (0, 0, 0, 0,  1), "February", 8, "h", 0, Good);
    TEST (T (0, 0, 0, 0,  2), "Mar",      3, "h", 0, Eof);
    TEST (T (0, 0, 0, 0,  3), "April",    5, "h", 0, Good);
    TEST (T (0, 0, 0, 0,  4), "May",      3, "h", 0, Good);
    TEST (T (0, 0, 0, 0,  5), "June",     4, "h", 0, Good);
    TEST (T (0, 0, 0, 0,  6), "Jul",      3, "h", 0, Eof);
    TEST (T (0, 0, 0, 0,  7), "August",   6, "h", 0, Good);
    TEST (T (0, 0, 0, 0,  8), "Sep",      3, "h", 0, Eof);
    TEST (T (0, 0, 0, 0,  9), "October",  7, "h", 0, Good);
    TEST (T (0, 0, 0, 0, 10), "Nov",      3, "h", 0, Eof);
    TEST (T (0, 0, 0, 0, 11), "December", 8, "h", 0, Good);

    // %H The hour (24-hour clock) [00,23]; leading zeros are permitted
    //    but not required.
    STEP ("%H: the hour (24-hour clock)");
    TEST (T (0, 0,  0),   "0", 1, "H", 0, Eof);
    TEST (T (0, 0,  0),  "00", 2, "H", 0, Eof);
    TEST (T (0, 0,  0), "000", 2, "H", 0, Good);
    TEST (T (0, 0,  1),   "1", 1, "H", 0, Eof);
    TEST (T (0, 0,  2),   "2", 1, "H", 0, Eof);
    TEST (T (0, 0,  3),   "3", 1, "H", 0, Eof);
    TEST (T (0, 0, 11),  "11", 2, "H", 0, Eof);
    TEST (T (0, 0, 12),  "12", 2, "H", 0, Eof);
    TEST (T (0, 0, 13),  "13", 2, "H", 0, Eof);
    TEST (T (0, 0, 23),  "23", 2, "H", 0, Eof);
    // at most two digits, as in glibc's strptime
    TEST (T (0, 0,  2), "023", 2, "H", 0, Good);
    TEST (T (0, 0,  2), "024", 2, "H", 0, Good);
    // one more digit is read only while ten times the value stays
    // within the bound, as in glibc's strptime: 9 stops, 2 reads on
    TEST (T (0, 0,  9), "987", 1, "H", 0, Good);
    TEST (T (0, 0,  0),  "25", 2, "H", 0, Eof | Fail);

    // %I The hour (12-hour clock) [01,12]; leading zeros are permitted
    //    but not required.
    STEP ("%I: the hour (12-hour clock)");
    TEST (T (0, 0,  0),   "1", 1, "I", 0, Eof);
    TEST (T (0, 0,  1),   "2", 1, "I", 0, Eof);
    TEST (T (0, 0,  2),   "3", 1, "I", 0, Eof);
    TEST (T (0, 0,  3),   "4", 1, "I", 0, Eof);
    TEST (T (0, 0,  4),   "5", 1, "I", 0, Eof);
    TEST (T (0, 0,  5),   "6", 1, "I", 0, Eof);
    TEST (T (0, 0,  6),   "7", 1, "I", 0, Eof);
    TEST (T (0, 0,  7),   "8", 1, "I", 0, Eof);
    TEST (T (0, 0,  8),   "9", 1, "I", 0, Eof);
    TEST (T (0, 0,  9),  "10", 2, "I", 0, Eof);
    TEST (T (0, 0, 10),  "11", 2, "I", 0, Eof);
    TEST (T (0, 0, 11),  "12", 2, "I", 0, Eof);
    TEST (T (0, 0,  0),  "13", 2, "I", 0, Eof | Fail);
    // at most two digits, as in glibc's strptime
    TEST (T (0, 0,  0), "013", 2, "I", 0, Good);
    TEST (T (0, 0, 11), "123", 2, "I", 0, Good);
    TEST (T (0, 0,  0),   "0", 1, "I", 0, Eof | Fail);

    // %j The day number of the year [001,366]; leading zeros are permitted
    //    but not required.
    TEST (T (0, 0, 0, 0, 0, 0, 0,   0),   "1", 1, "j", 0, Eof);
    TEST (T (0, 0, 0, 0, 0, 0, 0,   1),   "2", 1, "j", 0, Eof);
    TEST (T (0, 0, 0, 0, 0, 0, 0,   2),  "03", 2, "j", 0, Eof);
    TEST (T (0, 0, 0, 0, 0, 0, 0,   9),  "10", 2, "j", 0, Eof);
    TEST (T (0, 0, 0, 0, 0, 0, 0, 364), "365", 3, "j", 0, Eof);
    TEST (T (0, 0, 0, 0, 0, 0, 0, 365), "366", 3, "j", 0, Eof);
    // at most three digits, as in glibc's strptime
    TEST (T (0, 0, 0, 0, 0, 0, 0, 122), "1234", 3, "j", 0, Good);
    TEST (T (0, 0, 0, 0, 0, 0, 0,   0),   "0", 1, "j", 0, Eof | Fail);
    TEST (T (0, 0, 0, 0, 0, 0, 0,   0), "367", 3, "j", 0, Eof | Fail);

#ifdef __GLIBC__

    STEP ("%k: equivalent to %H (GNU glibc extension)");
    
    TEST (T (0, 0,  0),   "0", 1, "k", 0, Eof);
    TEST (T (0, 0,  1),   "1", 1, "k", 0, Eof);
    TEST (T (0, 0,  9),   "9", 1, "k", 0, Eof);
    TEST (T (0, 0, 10),  "10", 2, "k", 0, Eof);
    TEST (T (0, 0, 11),  "11", 2, "k", 0, Eof);
    TEST (T (0, 0, 12),  "12", 2, "k", 0, Eof);
    TEST (T (0, 0, 13),  "13", 2, "k", 0, Eof);
    TEST (T (0, 0, 23),  "23", 2, "k", 0, Eof);
    TEST (T (0, 0,  2), "023", 2, "k", 0, Good);
    TEST (T (0, 0,  2), "024", 2, "k", 0, Good);
    TEST (T (0, 0,  9), "987", 1, "k", 0, Good);

#endif   // __GLIBC__

    // %m The month number [01,12]; leading zeros are permitted
    //    but not required.
    STEP ("%m: the month number");
    TEST (T (0, 0, 0, 0,  0, 0),  "1", 1, "m", 0, Eof);
    TEST (T (0, 0, 0, 0,  1, 0),  "2", 1, "m", 0, Eof);
    TEST (T (0, 0, 0, 0,  2, 0),  "3", 1, "m", 0, Eof);
    TEST (T (0, 0, 0, 0,  3, 0),  "4", 1, "m", 0, Eof);
    TEST (T (0, 0, 0, 0,  4, 0),  "5", 1, "m", 0, Eof);
    TEST (T (0, 0, 0, 0,  5, 0),  "6", 1, "m", 0, Eof);
    TEST (T (0, 0, 0, 0,  6, 0),  "7", 1, "m", 0, Eof);
    TEST (T (0, 0, 0, 0,  7, 0),  "8", 1, "m", 0, Eof);
    TEST (T (0, 0, 0, 0,  8, 0),  "9", 1, "m", 0, Eof);
    TEST (T (0, 0, 0, 0,  9, 0), "10", 2, "m", 0, Eof);
    TEST (T (0, 0, 0, 0, 10, 0), "11", 2, "m", 0, Eof);
    TEST (T (0, 0, 0, 0, 11, 0), "12", 2, "m", 0, Eof);
    TEST (T (0, 0, 0, 0,  0, 0), "01", 2, "m", 0, Eof);
    TEST (T (0, 0, 0, 0,  1, 0), "02", 2, "m", 0, Eof);
    TEST (T (0, 0, 0, 0,  0, 0),  "0", 1, "m", 0, Eof | Fail);
    TEST (T (0, 0, 0, 0,  0, 0), "13", 2, "m", 0, Eof | Fail);
    // a digit that would pass the bound is left, as in glibc's strptime
    TEST (T (0, 0, 0, 0,  3, 0), "41", 1, "m", 0, Good);

    // %M The minute [00,59]; leading zeros are permitted but not required.
    STEP ("%M: the minute");
    TEST (T (0,  0),   "0", 1, "M", 0, Eof);
    TEST (T (0,  0),  "00", 2, "M", 0, Eof);
    TEST (T (0,  0), "000", 2, "M", 0, Good);
    TEST (T (0,  1),   "1", 1, "M", 0, Eof);
    TEST (T (0,  2),   "2", 1, "M", 0, Eof);
    TEST (T (0, 19),  "19", 2, "M", 0, Eof);
    TEST (T (0, 29),  "29", 2, "M", 0, Eof);
    TEST (T (0, 59),  "59", 2, "M", 0, Eof);
    // a digit that would pass the bound is left, as in glibc's strptime
    TEST (T (0,  6),  "60", 1, "M", 0, Good);

    // %n Any white space.
    STEP ("%n: any whitespace");
    TEST (T (),     "", 0, "n", 0, Eof);
    TEST (T (),    " ", 1, "n", 0, Eof);
    TEST (T (),   "  ", 2, "n", 0, Eof);
    TEST (T (),  "   ", 3, "n", 0, Eof);
    TEST (T (),  "\n ", 2, "n", 0, Eof);
    TEST (T (),  " \t", 2, "n", 0, Eof);
    TEST (T (), "\n\t", 2, "n", 0, Eof);
    TEST (T (),    ".", 0, "n", 0, Good);
    TEST (T (),   " 1", 1, "n", 0, Good);
    TEST (T (), "\t 2", 2, "n", 0, Good);

    // %p The locale's equivalent of a.m or p.m.
    STEP ("%p: the equivalent of AM/PM");
    TEST (T (0, 0,  1), "AM", 2, "p", 0, Good);
    TEST (T (0, 0, 13), "PM", 2, "p", 0, Good);
    TEST (T (0, 0,  0),  "",  0, "p", 0, Eof | Fail);
    TEST (T (0, 0,  0),  "A", 1, "p", 0, Eof | Fail);
    TEST (T (0, 0,  0),  "P", 1, "p", 0, Eof | Fail);
    TEST (T (0, 0,  0),  "M", 0, "p", 0, Fail);
    TEST (T (0, 0,  0), "A ", 1, "p", 0, Fail);
    TEST (T (0, 0,  0), "P ", 1, "p", 0, Fail);

    // %Op Equivalent to %p (an extension of the C library's strftime).
    STEP ("%Op: equivalent to %p");
    TEST (T (0, 0,  1), "AM", 2, "Op", 0, Good);
    TEST (T (0, 0, 13), "PM", 2, "Op", 0, Good);
    TEST (T (0, 0,  0),  "",  0, "Op", 0, Eof | Fail);
    TEST (T (0, 0,  0),  "A", 1, "Op", 0, Eof | Fail);
    TEST (T (0, 0,  0),  "P", 1, "Op", 0, Eof | Fail);
    TEST (T (0, 0,  0),  "M", 0, "Op", 0, Fail);
    TEST (T (0, 0,  0), "A ", 1, "Op", 0, Fail);
    TEST (T (0, 0,  0), "P ", 1, "Op", 0, Fail);
    TEST (T (0, 0,  0),  "1", 0, "Op", 0, Fail);

    // %r 12-hour clock time using the AM/PM notation if t_fmt_ampm is
    //    not an empty string in the LC_TIME portion of the current locale;
    //    in the POSIX locale, this shall be equivalent to %I:%M:%S %p.
    STEP ("%r: 12-hour clock time using the AM/PM notation");
    TEST (T ( 0,  0,  0), "12:00:00 AM", 11, "r", 0, Good);
    TEST (T ( 9,  5,  9), "09:05:09 AM", 11, "r", 0, Good);
    TEST (T (59, 59, 11), "11:59:59 AM", 11, "r", 0, Good);
    TEST (T ( 0,  0, 12), "12:00:00 PM", 11, "r", 0, Good);
    TEST (T ( 9,  5, 21), "09:05:09 PM", 11, "r", 0, Good);
    TEST (T (59, 59, 23), "11:59:59 PM", 11, "r", 0, Good);
    TEST (T ( 3,  2,  1),    "1:2:3 am",  8, "r", 0, Good);
    TEST (T ( 0,  0,  0),    "09:05:09",  8, "r", 0, Eof | Fail);
    TEST (T ( 0,  0,  0), "13:00:00 PM",  2, "r", 0, Fail);

    // %p settles the hour at the end of the pattern whatever the order,
    // as in the C library's strptime: it adds the 12 hours to a %I and
    // is parsed and ignored after a %H
    STEP ("%p with the hour: the 12 hours of a %I, none of a %H");
    TEST (T ( 9,  5,  9), "09:05:09 AM", 11, "%H:%M:%S %p", 0, Good);
    TEST (T ( 9,  5,  9), "09:05:09 PM", 11, "%H:%M:%S %p", 0, Good);
    TEST (T ( 9,  5, 21), "21:05:09 PM", 11, "%H:%M:%S %p", 0, Good);
    TEST (T ( 9,  5, 21), "09:05:09 PM", 11, "%I:%M:%S %p", 0, Good);
    TEST (T ( 0,  0,  0), "12:00:00 AM", 11, "%I:%M:%S %p", 0, Good);
    TEST (T ( 0,  0, 12), "12:00:00 PM", 11, "%I:%M:%S %p", 0, Good);
    TEST (T ( 0,  0, 21),       "PM 09",  5, "%p %I",       0, Eof);
    TEST (T ( 0,  0,  0),       "AM 12",  5, "%p %I",       0, Eof);


    // %R The time as %H:%M.
    STEP ("%R: the time as %H:%M");
    TEST (T (0,  0,  0),      "0:0", 3, "R", 0, Eof);
    TEST (T (0,  1,  0),      "0:1", 3, "R", 0, Eof);
    TEST (T (0,  2,  0),     "0:02", 4, "R", 0, Eof);
    TEST (T (0, 59,  0),     "0:59", 4, "R", 0, Eof);
    TEST (T (0, 48,  1),     "1:48", 4, "R", 0, Eof);
    TEST (T (0, 37, 11),    "11:37", 5, "R", 0, Eof);
    TEST (T (0, 26, 19),    "19:26", 5, "R", 0, Eof);
    TEST (T (0, 15, 22),    "22:15", 5, "R", 0, Eof);
    TEST (T (0,  4, 23),    "23:04", 5, "R", 0, Eof);
    TEST (T (0,  3, 23), "23:03:00", 5, "R", 0, Good);
    TEST (T (0,  2, 23),  "23:2:01", 4, "R", 0, Good);
    TEST (T (0,  6, 23),    "23:60", 4, "R", 0, Good);
    TEST (T (0,  0,  0),    "24:00", 2, "R", 0, Fail);

    // %S The seconds [00,60]; leading zeros are permitted but not required.
    STEP ("%S: the seconds");
    TEST (T ( 0),   "0", 1, "S", 0, Eof);
    TEST (T ( 1),  "01", 2, "S", 0, Eof);
    TEST (T ( 0), "002", 2, "S", 0, Good);
    TEST (T (13),  "13", 2, "S", 0, Eof);
    TEST (T (24),  "24", 2, "S", 0, Eof);
    TEST (T (34),  "34", 2, "S", 0, Eof);
    TEST (T (45),  "45", 2, "S", 0, Eof);
    TEST (T (56),  "56", 2, "S", 0, Eof);
    TEST (T (60),  "60", 2, "S", 0, Eof);
    TEST (T ( 0),  "61", 2, "S", 0, Eof | Fail);

    // %t Any white space.
    STEP ("%t: any whitespace");
    TEST (T (),   "\t", 1, "t", 0, Eof);
    TEST (T (),    " ", 1, "t", 0, Eof);
    TEST (T (),   "  ", 2, "t", 0, Eof);
    TEST (T (),  "\n ", 2, "t", 0, Eof);
    
    // %T The time as %H:%M:%S.
    STEP ("%T: the time as %H:%M:%S");
    TEST (T ( 0,  0,  0),    "0:0:00", 6, "T", 0, Eof);
    TEST (T ( 1,  1,  0),     "0:1:1", 5, "T", 0, Eof);
    TEST (T ( 2,  2,  0),    "0:02:2", 6, "T", 0, Eof);
    TEST (T ( 3, 59,  0),    "0:59:3", 6, "T", 0, Eof);
    TEST (T ( 4, 48,  1),   "1:48:04", 7, "T", 0, Eof);
    TEST (T ( 5, 37, 11),   "11:37:5", 7, "T", 0, Eof);
    TEST (T ( 0, 26, 19), "19:26:006", 8, "T", 0, Good);
    TEST (T ( 0,  0,  0),  "22:015:7", 5, "T", 0, Fail);
    TEST (T ( 0,  0,  0), "023:004:8", 2, "T", 0, Fail);
    TEST (T ( 0,  0,  0), "023:59:60", 2, "T", 0, Fail);
    TEST (T ( 0,  0,  0),  "23:59:61", 8, "T", 0, Eof | Fail);
    TEST (T ( 0,  0,  0),    "0:0:62", 6, "T", 0, Eof | Fail);
    TEST (T ( 0,  0,  0),       "0:0", 3, "T", 0, Eof | Fail);
    TEST (T ( 0,  0,  0),       "999", 1, "T", 0, Fail);

    // %U The week number of the year (Sunday as the first day of the week)
    //    as a decimal number [00,53]; leading zeros are permitted but not
    //    required.
    STEP ("%U: the Sunday-based week of the year");
    rw_warn (0, 0, __LINE__, "%%U not being exercised");

    // %w The weekday as a decimal number [0,6], with 0 representing
    //    Sunday; leading zeros are permitted but not required.
    STEP ("%w: the Sunday-based weekday as a decimal number");
    TEST (T (0, 0, 0, 0, 0, 0, 0),  "0", 1, "w", 0, Eof);
    TEST (T (0, 0, 0, 0, 0, 0, 0), "00", 1, "w", 0, Good);
    TEST (T (0, 0, 0, 0, 0, 0, 1),  "1", 1, "w", 0, Eof);
    TEST (T (0, 0, 0, 0, 0, 0, 0), "01", 1, "w", 0, Good);
    TEST (T (0, 0, 0, 0, 0, 0, 2),  "2", 1, "w", 0, Eof);
    TEST (T (0, 0, 0, 0, 0, 0, 3),  "3", 1, "w", 0, Eof);
    TEST (T (0, 0, 0, 0, 0, 0, 4),  "4", 1, "w", 0, Eof);
    TEST (T (0, 0, 0, 0, 0, 0, 5),  "5", 1, "w", 0, Eof);
    TEST (T (0, 0, 0, 0, 0, 0, 6),  "6", 1, "w", 0, Eof);
    TEST (T (0, 0, 0, 0, 0, 0, 0),  "7", 1, "w", 0, Eof | Fail);
    // one digit, as in glibc's strptime
    TEST (T (0, 0, 0, 0, 0, 0, 1), "12", 1, "w", 0, Good);

    // %W The week number of the year (Monday as the first day of the
    //    week) as a decimal number [00,53]; leading zeros are permitted
    //    but not required.
    STEP ("%W: the Monday-based week of the year");
    rw_warn (0, 0, __LINE__, "%%W not being exercised");

    // %x The date, using the locale's date format.
    STEP ("%x: the date");
    TEST (T (0, 0, 0, 25, 6, 167),   "7/25/67", 7, "%x", 0, Eof);
    TEST (T (0, 0, 0, 25, 6, 119), "7/25/1967", 7, "%x", 0, Good);

    // %X The time, using the locale's time format.
    STEP ("%X: the time");
    TEST (T (12, 34, 5),  "5:34:12", 7,  "%X", 0, Eof);
    TEST (T (23, 45, 6), " 6:45:23", 8, " %X", 0, Eof);

    // %y The year within century. When a century is not otherwise
    //    specified, values in the range [69,99] shall refer to years 1969
    //    to 1999 inclusive, and values in the range [00,68] shall refer
    //    to years 2000 to 2068 inclusive; leading zeros shall be permitted
    //    but shall not be required.
    //    Note: It is expected that in a future version of
    //          IEEE Std 1003.1-2001 the default century inferred from a
    //          2-digit year will change. (This would apply to all commands
    //          accepting a 2-digit year as input.)
    STEP ("%y: the year within century");
    TEST (T (0, 0, 0, 0, 0,   100),    "0", 1, "%y", 0, Eof);
    TEST (T (0, 0, 0, 0, 0,   100),   "00", 2, "%y", 0, Eof);
    TEST (T (0, 0, 0, 0, 0,   101),    "1", 1, "%y", 0, Eof);
    TEST (T (0, 0, 0, 0, 0,   102),    "2", 1, "%y", 0, Eof);
    TEST (T (0, 0, 0, 0, 0,   103),   "03", 2, "%y", 0, Eof);
    TEST (T (0, 0, 0, 0, 0,   168),   "68", 2, "%y", 0, Eof);
    TEST (T (0, 0, 0, 0, 0,    69),   "69", 2, "%y", 0, Eof);
    TEST (T (0, 0, 0, 0, 0,    99),   "99", 2, "%y", 0, Eof);
    // at most two digits, as in glibc's strptime
    TEST (T (0, 0, 0, 0, 0,   110),  "100", 2, "%y", 0, Good);
    TEST (T (0, 0, 0, 0, 0,   120), "2026", 2, "%y", 0, Good);

    // %Y The year, including the century (for example, 1988).
    STEP ("%Y: the year, including the century");
    TEST (T (0, 0, 0, 0, 0, -1900),      "0", 1, "%Y",  0, Eof);
    TEST (T (0, 0, 0, 0, 0, -1900),     " 0", 2, " %Y", 0, Eof);
    TEST (T (0, 0, 0, 0, 0,    99), "\t1999", 5, " %Y", 0, Eof);

    // %C and %y combine in either order: the century and the year
    // within it; a %Y is a full year and keeps its value
    STEP ("%C with %y: the year within the century");
    TEST (T (0, 0, 0,  0, 0,    26),       "19 26",  5, "%C %y",    0, Eof);
    TEST (T (0, 0, 0,  0, 0,   126),       "20 26",  5, "%C %y",    0, Eof);
    TEST (T (0, 0, 0,  0, 0, -1801),       "00 99",  5, "%C %y",    0, Eof);
    TEST (T (0, 0, 0,  0, 0,    26),       "19/26",  5, "%C/%y",    0, Eof);
    TEST (T (0, 0, 0,  0, 0,    26),        "1926",  4, "%C%y",     0, Eof);
    TEST (T (0, 0, 0,  0, 0,    26),       "26 19",  5, "%y %C",    0, Eof);
    TEST (T (0, 0, 0, 14, 3,    26), "19 04/14/26", 11, "%C %D",    0, Eof);
    TEST (T (0, 0, 0, 14, 3,    26), "04/14/26 19", 11, "%D %C",    0, Eof);
    TEST (T (0, 0, 0,  0, 0,    26),     "20 1926",  7, "%C %Y",    0, Eof);
    TEST (T (0, 0, 0,  0, 0,    26),  "20 99 1926", 10, "%C %y %Y", 0, Eof);

    // each numeric conversion reads at most two digits, %j three and %Y
    // four, as in glibc's strptime, so that adjacent ones separate
    STEP ("adjacent numeric conversions");
    TEST (T ( 0,  0,  0, 14, 3, 126),   "041426", 6, "%m%d%y", 0, Eof);
    TEST (T ( 0,  0,  0, 28, 9, 126), "20261028", 8, "%Y%m%d", 0, Eof);
    TEST (T (60, 59, 23),               "235960", 6, "%H%M%S", 0, Eof);
    TEST (T ( 0,  0,  0,  0, 0, 126, 0, 300),
          "2026301", 7, "%Y%j", 0, Eof);
    // and a field reads one more digit only while ten times its value
    // stays within its bound
    TEST (T ( 0,  0,  0,  4, 4),          "4512", 2, "%d%m",  0, Good);
    TEST (T ( 0,  0,  0),                "25:30", 2, "%H:%M", 0, Fail);

    // %Z The time zone name, read and not converted, as the C library's
    //    strptime reads it: leading whitespace, then a run of other
    //    characters, which may be empty.
    STEP ("%Z: the time zone name");
    TEST (T (),         "UTC", 3, "Z",  0, Eof);
    TEST (T (),     "EST5EDT", 7, "Z",  0, Eof);
    TEST (T (),        "UTC ", 3, "Z",  0, Good);
    TEST (T (),            "", 0, "Z",  0, Eof);
    TEST (T (),        " UTC", 4, "Z",  0, Eof);
    TEST (T (),         "UTC", 0, "EZ", 0, Fail);
    TEST (T (),         "UTC", 0, "OZ", 0, Fail);
    TEST (T (0, 5, 9),     "09:05 UTC",  9, "%H:%M %Z",    0, Eof);
    TEST (T (0, 5, 9), "09:05 EST5EDT", 13, "%H:%M %Z",    0, Eof);
    TEST (T (7, 5, 9),  "09:05 UTC 07", 12, "%H:%M %Z %S", 0, Eof);

    // %z The offset from UTC, +hhmm, +hh:mm or +hh, or with a minus sign,
    //    or Z for +0000, stored in tm_gmtoff where std::tm has it; the
    //    minutes are 0..59 and the hours any two digits, as in glibc.
    STEP ("%z: the offset from UTC");
    TEST (T (0, 0, 0, 0, 0, 0, 0, 0, 0,   5400),  "+0130", 5, "z", 0, Eof);
    TEST (T (0, 0, 0, 0, 0, 0, 0, 0, 0, -18000),  "-0500", 5, "z", 0, Eof);
    TEST (T (0, 0, 0, 0, 0, 0, 0, 0, 0,   5400), "+01:30", 6, "z", 0, Eof);
    TEST (T (0, 0, 0, 0, 0, 0, 0, 0, 0,   3600),    "+01", 3, "z", 0, Eof);
    TEST (T (0, 0, 0, 0, 0, 0, 0, 0, 0,   5400), "+0130 ", 5, "z", 0, Good);
    TEST (T (0, 0, 0, 0, 0, 0, 0, 0, 0,   3600), " +0100", 6, "z", 0, Eof);
    TEST (T (0, 0, 0, 0, 0, 0, 0, 0, 0,  89940),  "+2459", 5, "z", 0, Eof);
    TEST (T (),      "Z", 1, "z",  0, Eof);
    TEST (T (),     "Z5", 1, "z",  0, Good);
    TEST (T (),  "+2460", 5, "z",  0, Eof | Fail);
    TEST (T (),   "+013", 4, "z",  0, Eof | Fail);
    TEST (T (),   "+01:", 4, "z",  0, Eof | Fail);
    TEST (T (),   "0130", 0, "z",  0, Fail);
    TEST (T (),      "z", 0, "z",  0, Fail);
    TEST (T (),  "+0130", 0, "Ez", 0, Fail);
    TEST (T (),  "+0130", 0, "Oz", 0, Fail);
    TEST (T (9, 5, 9, 0, 0, 0, 0, 0, 0, -18000),
          "09:05:09 -0500", 14, "%T %z", 0, Eof);

    STEP ("invalid pattern");
    TEST (T (0), "0", 0, "E", 0, Fail);
    TEST (T (0), "0", 0, "i", 0, Fail);
    TEST (T (0), "0", 0, "J", 0, Fail);
    TEST (T (0), "0", 0, "N", 0, Fail);
    TEST (T (0), "0", 0, "P", 0, Fail);
    TEST (T (0), "0", 0, "s", 0, Fail);
    TEST (T (0), "0", 0, "u", 0, Fail);
    TEST (T (0), "0", 0, "z", 0, Fail);


    // %Ec The locale's alternative date and time representation. 
    // %EC The name of the base year (period) in the locale's alternative
    //     representation.
    // %Ex The locale's alternative date representation.
    // %EX The locale's alternative time representation.
    // %Ey The offset from %EC (year only) in the locale's alternative
    // representation.
    // %EY The full alternative year representation.
    // %Od The day of the month using the locale's alternative numeric
    //     symbols; leading zeros are permitted but not required.
    // %Oe Equivalent to %Od.
    // %OH The hour (24-hour clock) using the locale's alternative numeric
    //     symbols.
    // %OI The hour (12-hour clock) using the locale's alternative numeric
    //     symbols.
    // %Om The month using the locale's alternative numeric symbols.
    // %OM The minutes using the locale's alternative numeric symbols.
    // %OS The seconds using the locale's alternative numeric symbols.
    // %OU The week number of the year (Sunday as the first day of the week)
    //     using the locale's alternative numeric symbols.
    // %Ow The number of the weekday (Sunday=0) using the locale's
    //     alternative numeric symbols.
    // %OW The week number of the year (Monday as the first day of the
    //     week) using the locale's alternative numeric symbols.
    // %Oy The year (offset from %C ) using the locale's alternative numeric
    // symbols.
    rw_warn (0, 0, __LINE__, "E modifier not being exercised");
    rw_warn (0, 0, __LINE__, "O modifier not being exercised");

    // exercise pattern strings containing multiple format specifiers
    TEST (T (0, 0, 0, 0, 0, 0, 0), "Sun Jan",       7, "%a %b", 0, Eof);
    TEST (T (0, 0, 0, 0, 1, 0, 1), "Mon  Feb",      8, "%a %b", 0, Eof);
    TEST (T (0, 0, 0, 0, 2, 0, 2), "Tue%nMar",      7, "%a %b", 0, Eof);
    TEST (T (0, 0, 0, 0, 3, 0, 3), "Wed%t Apr",     8, "%a %b", 0, Eof);
    TEST (T (0, 0, 0, 0, 2, 0, 5), "Friday March", 12, "%a %b", 0, Good);
}

/**************************************************************************/

// the order of the day, month and year in the C library's date format
// of the named locale, by the rule time_get::do_date_order applies to
// the facet's own format: the expectation comes from the C library's
// copy of the same data; -1 when the C library does not know the name
static int
expected_date_order (const char *locname)
{
    char saved [256];
    const char* const cur = std::setlocale (LC_TIME, 0);
    std::strncpy (saved, cur ? cur : "C", sizeof saved - 1);
    saved [sizeof saved - 1] = '\0';

    if (0 == std::setlocale (LC_TIME, locname))
        return -1;

    const char *fmt = nl_langinfo (D_FMT);

    char order [3] = { '\0', '\0', '\0' };
    int  n = 0;
    bool century = false;
    int  result = std::time_base::no_order;

    for ( ; fmt && *fmt; ++fmt) {

        if ('%' != *fmt)
            continue;

        // past the flags and the width, then the modifier
        do {
            ++fmt;
        } while (   '-' == *fmt || '_' == *fmt || '0' == *fmt
                 || '^' == *fmt || '#' == *fmt || ('1' <= *fmt && *fmt <= '9'));

        if ('E' == *fmt || 'O' == *fmt)
            ++fmt;

        const char *comp = "";

        switch (*fmt) {
        case 'd': case 'e':                     comp = "d";   break;
        case 'b': case 'B': case 'h': case 'm': comp = "m";   break;
        case 'C': case 'Y':                     comp = "y";   break;
        case 'y':             comp = century ? "" : "y";      break;
        case 'D':                               comp = "mdy"; break;
        case 'F':                               comp = "ymd"; break;
        case 'a': case 'A': case 'n': case 't': case '%':     break;
        default:
            n = -1;
        }

        if (n < 0)
            break;

        century = 'C' == *fmt;

        for ( ; *comp; ++comp) {
            if (3 == n || std::memchr (order, *comp, n)) {
                n = -1;
                break;
            }
            order [n++] = *comp;
        }

        if (n < 0 || '\0' == *fmt)
            break;
    }

    if (3 == n) {
        if ('d' == order [0])
            result = 'm' == order [1] ? std::time_base::dmy : std::time_base::no_order;
        else if ('m' == order [0])
            result = 'd' == order [1] ? std::time_base::mdy : std::time_base::no_order;
        else
            result = 'm' == order [1] ? std::time_base::ymd : std::time_base::ydm;
    }

    std::setlocale (LC_TIME, saved);

    return result;
}

/**************************************************************************/


template <class charT>
void test_english (charT, const char *cname, const char *locname)
{
    try {
        // set the global locale object to the german locale
        std::locale::global (std::locale (locname));
    }
    catch (...) {
        rw_error (0, 0, __LINE__,
                  "locale(%#s) unexpectedly threw an exception", locname);

        return;
    }

    // 22.2.5.1.2, p1: the order of the components of the x format,
    // expected from the C library's format of the same locale
    {
        const int expect = expected_date_order (locname);
        const int order  = std::use_facet<std::time_get<charT> >(
                               std::locale ()).date_order ();

        rw_assert (expect < 0 || expect == order, 0, __LINE__,
                   "std::time_get<%s>::date_order() in locale (\"%s\") "
                   "== %d from the C library's D_FMT, got %d",
                   cname, locname, expect, order);
    }

    // exercise abbreviated weekday names
    FUNCTION ("get_weekday");
    TEST (T (0, 0, 0, 0, 0, 0, 0), "Sun", 3, "a", 0, Eof);
    TEST (T (0, 0, 0, 0, 0, 0, 1), "Mon", 3, "a", 0, Eof);
    TEST (T (0, 0, 0, 0, 0, 0, 2), "Tue", 3, "a", 0, Eof);
    TEST (T (0, 0, 0, 0, 0, 0, 3), "Wed", 3, "a", 0, Eof);
    TEST (T (0, 0, 0, 0, 0, 0, 4), "Thu", 3, "a", 0, Eof);
    TEST (T (0, 0, 0, 0, 0, 0, 5), "Fri", 3, "a", 0, Eof);
    TEST (T (0, 0, 0, 0, 0, 0, 6), "Sat", 3, "a", 0, Eof);

    // exercise full weekday names
    TEST (T (0, 0, 0, 0, 0, 0, 0), "Sunday",    6, "a", 0, Good);
    TEST (T (0, 0, 0, 0, 0, 0, 1), "Monday",    6, "a", 0, Good);
    TEST (T (0, 0, 0, 0, 0, 0, 2), "Tuesday",   7, "a", 0, Good);
    TEST (T (0, 0, 0, 0, 0, 0, 3), "Wednesday", 9, "a", 0, Good);
    TEST (T (0, 0, 0, 0, 0, 0, 4), "Thursday",  8, "a", 0, Good);
    TEST (T (0, 0, 0, 0, 0, 0, 5), "Friday",    6, "a", 0, Good);
    TEST (T (0, 0, 0, 0, 0, 0, 6), "Saturday",  8, "a", 0, Good);

    // exercise full month names
    FUNCTION ("get_monthname");
    TEST (T (0, 0, 0, 0,  0), "%B", -1, "b", 0, -1);
    TEST (T (0, 0, 0, 0,  1), "%B", -1, "b", 0, -1);
    TEST (T (0, 0, 0, 0,  2), "%B", -1, "b", 0, -1);
    TEST (T (0, 0, 0, 0,  3), "%B", -1, "b", 0, -1);
    TEST (T (0, 0, 0, 0,  4), "%B", -1, "b", 0, -1);
    TEST (T (0, 0, 0, 0,  5), "%B", -1, "b", 0, -1);
    TEST (T (0, 0, 0, 0,  6), "%B", -1, "b", 0, -1);
    TEST (T (0, 0, 0, 0,  7), "%B", -1, "b", 0, -1);
    TEST (T (0, 0, 0, 0,  8), "%B", -1, "b", 0, -1);
    TEST (T (0, 0, 0, 0,  9), "%B", -1, "b", 0, -1);
    TEST (T (0, 0, 0, 0, 10), "%B", -1, "b", 0, -1);
    TEST (T (0, 0, 0, 0, 11), "%B", -1, "b", 0, -1);

    int len;

    // exercise date (time_get::get_date())
    FUNCTION ("get_date");

    {
        // determine the length of the date in the locale's format,
        // "%x" is "%m/%d/%y" on some platforms and "%m/%d/%Y" on
        // others (e.g., glibc)
        char buf [32];
        const std::tm tmb = mktm (0, 0, 0, 1, 0, 100);
        len = int (std::strftime (buf, sizeof buf, "%x", &tmb));
    }

    TEST (T (0, 0, 0, 1, 0, 100), "%x", len, "x", 0, Eof);

    // exercise time (time_get::get_time())
    FUNCTION ("get_time");

    {
        // determine whether "%X" is equivalent to "%I:%M:%S %p"
        // (e.g., HP-UX or Linux) or to "%I:%M:%S" (e.g., Compaq
        // Tru64 UNIX)
        char buf [32];
        const std::tm tmb = mktm (0, 0, 1);
        len = int (std::strftime (buf, sizeof buf, "%X", &tmb));
    }

    const int hour = 11 == len ? 12 : 0;

    TEST (T ( 0, 0,  0), "%X", len, "X", 0, 11 == len ? Good : Eof);
    TEST (T ( 0, 0,  1), "%X", len, "X", 0, 11 == len ? Good : Eof);
    TEST (T ( 0, 0, 11), "%X", len, "X", 0, 11 == len ? Good : Eof);
    TEST (T ( 0, 0, 12), "%X", len, "X", 0, 11 == len ? Good : Eof);
    TEST (T ( 0, 0, 13), "%X", len, "X", 0, 11 == len ? Good : Eof);
    TEST (T ( 0, 0, 23), "%X", len, "X", 0, 11 == len ? Good : Eof);

    TEST (T ( 0, 0, 12 - hour), "12:00:00 AM", len, "X", 0, Good);
    TEST (T ( 0, 0,         1), "01:00:00 AM", len, "X", 0, Good);
    TEST (T ( 0, 0,         2), "02:00:00 AM", len, "X", 0, Good);
    TEST (T ( 0, 0,         3), "03:00:00 AM", len, "X", 0, Good);
    TEST (T ( 0, 0,         4), "04:00:00 AM", len, "X", 0, Good);
    TEST (T ( 0, 0,         5), "05:00:00 AM", len, "X", 0, Good);
    TEST (T ( 0, 0,         6), "06:00:00 AM", len, "X", 0, Good);
    TEST (T ( 0, 0,         7), "07:00:00 AM", len, "X", 0, Good);
    TEST (T ( 0, 0,         8), "08:00:00 AM", len, "X", 0, Good);
    TEST (T ( 0, 0,         9), "09:00:00 AM", len, "X", 0, Good);
    TEST (T ( 0, 0,        10), "10:00:00 AM", len, "X", 0, Good);
    TEST (T ( 0, 0,        11), "11:00:00 AM", len, "X", 0, Good);

    TEST (T ( 0, 0,        12), "12:00:00 PM", len, "X", 0, Good);
    TEST (T ( 0, 0, hour +  1), "01:00:00 PM", len, "X", 0, Good);
    TEST (T ( 0, 0, hour +  2), "02:00:00 PM", len, "X", 0, Good);
    TEST (T ( 0, 0, hour +  3), "03:00:00 PM", len, "X", 0, Good);
    TEST (T ( 0, 0, hour +  4), "04:00:00 PM", len, "X", 0, Good);
    TEST (T ( 0, 0, hour +  5), "05:00:00 PM", len, "X", 0, Good);
    TEST (T ( 0, 0, hour +  6), "06:00:00 PM", len, "X", 0, Good);
    TEST (T ( 0, 0, hour +  7), "07:00:00 PM", len, "X", 0, Good);
    TEST (T ( 0, 0, hour +  8), "08:00:00 PM", len, "X", 0, Good);
    TEST (T ( 0, 0, hour +  9), "09:00:00 PM", len, "X", 0, Good);
    TEST (T ( 0, 0, hour + 10), "10:00:00 PM", len, "X", 0, Good);
    TEST (T ( 0, 0, hour + 11), "11:00:00 PM", len, "X", 0, Good);

    TEST (T (60, 9, hour + 11), "11:09:60 PM", len, "X", 0, Good);
}

/**************************************************************************/


template <class charT>
void test_german (charT, const char *cname, const char *locname)
{
    try {
        // set the global locale object to the german locale
        std::locale::global (std::locale (locname));
    }
    catch (...) {
        rw_error (0, 0, __LINE__,
                  "locale(%#s) unexpectedly threw an exception", locname);

        return;
    }

    // 22.2.5.1.2, p1: the order of the components of the x format,
    // expected from the C library's format of the same locale
    {
        const int expect = expected_date_order (locname);
        const int order  = std::use_facet<std::time_get<charT> >(
                               std::locale ()).date_order ();

        rw_assert (expect < 0 || expect == order, 0, __LINE__,
                   "std::time_get<%s>::date_order() in locale (\"%s\") "
                   "== %d from the C library's D_FMT, got %d",
                   cname, locname, expect, order);
    }

    // exercise abbreviated weekday names
    // these may be { So, Mo, Di, Mi, Do, Sa } (e.g., Compaq Tru64 UNIX)
    // or { Son, Mon, Die, Mit, Don, Sam } (e.g., Linux, SunOS, or Win32)
    FUNCTION ("get_weekday");
    TEST (T (0, 0, 0, 0, 0, 0, 0), "%a", -1, "a", 0, Eof);
    TEST (T (0, 0, 0, 0, 0, 0, 1), "%a", -1, "a", 0, Eof);
    TEST (T (0, 0, 0, 0, 0, 0, 2), "%a", -1, "a", 0, Eof);
    TEST (T (0, 0, 0, 0, 0, 0, 3), "%a", -1, "a", 0, Eof);
    TEST (T (0, 0, 0, 0, 0, 0, 4), "%a", -1, "a", 0, Eof);
    TEST (T (0, 0, 0, 0, 0, 0, 5), "%a", -1, "a", 0, Eof);
    TEST (T (0, 0, 0, 0, 0, 0, 6), "%a", -1, "a", 0, Eof);

    // exercise full weekday names
    TEST (T (0, 0, 0, 0, 0, 0, 0), "Sonntag",     7, "a", 0, Good);
    TEST (T (0, 0, 0, 0, 0, 0, 1), "Montag",      6, "a", 0, Good);
    TEST (T (0, 0, 0, 0, 0, 0, 2), "Dienstag",    8, "a", 0, Good);
    TEST (T (0, 0, 0, 0, 0, 0, 3), "Mittwoch",    8, "a", 0, Good);
    TEST (T (0, 0, 0, 0, 0, 0, 4), "Donnerstag", 10, "a", 0, Good);
    TEST (T (0, 0, 0, 0, 0, 0, 5), "Freitag",     7, "a", 0, Good);
    TEST (T (0, 0, 0, 0, 0, 0, 6), "Samstag",     7, "a", 0, Good);

    // exercise full month names
    FUNCTION ("get_monthname");
    TEST (T (0, 0, 0, 0,  0), "%B", -1, "b", 0, -1);
    TEST (T (0, 0, 0, 0,  1), "%B", -1, "b", 0, -1);
    TEST (T (0, 0, 0, 0,  2), "%B", -1, "b", 0, -1);
    TEST (T (0, 0, 0, 0,  3), "%B", -1, "b", 0, -1);
    TEST (T (0, 0, 0, 0,  4), "%B", -1, "b", 0, -1);
    TEST (T (0, 0, 0, 0,  5), "%B", -1, "b", 0, -1);
    TEST (T (0, 0, 0, 0,  6), "%B", -1, "b", 0, -1);
    TEST (T (0, 0, 0, 0,  7), "%B", -1, "b", 0, -1);
    TEST (T (0, 0, 0, 0,  8), "%B", -1, "b", 0, -1);
    TEST (T (0, 0, 0, 0,  9), "%B", -1, "b", 0, -1);
    TEST (T (0, 0, 0, 0, 10), "%B", -1, "b", 0, -1);
    TEST (T (0, 0, 0, 0, 11), "%B", -1, "b", 0, -1);
}

/**************************************************************************/


template <class charT>
void test_danish (charT, const char *cname, const char *locname)
{
    try {
        // set the global locale object to the german locale
        std::locale::global (std::locale (locname));
    }
    catch (...) {
        rw_error (0, 0, __LINE__,
                  "locale(%#s) unexpectedly threw an exception", locname);

        return;
    }

    // 22.2.5.1.2, p1: the order of the components of the x format,
    // expected from the C library's format of the same locale
    {
        const int expect = expected_date_order (locname);
        const int order  = std::use_facet<std::time_get<charT> >(
                               std::locale ()).date_order ();

        rw_assert (expect < 0 || expect == order, 0, __LINE__,
                   "std::time_get<%s>::date_order() in locale (\"%s\") "
                   "== %d from the C library's D_FMT, got %d",
                   cname, locname, expect, order);
    }

    // exercise abbreviated weekday names
    // the length of <s><o/><n> and <l><o/><r> depends on the codeset
    FUNCTION ("get_weekday");
    TEST (T (0, 0, 0, 0, 0, 0, 0), "%a", -1, "a", 0, Eof);
    TEST (T (0, 0, 0, 0, 0, 0, 1), "%a", -1, "a", 0, Eof);
    TEST (T (0, 0, 0, 0, 0, 0, 2), "%a", -1, "a", 0, Eof);
    TEST (T (0, 0, 0, 0, 0, 0, 3), "%a", -1, "a", 0, Eof);
    TEST (T (0, 0, 0, 0, 0, 0, 4), "%a", -1, "a", 0, Eof);
    TEST (T (0, 0, 0, 0, 0, 0, 5), "%a", -1, "a", 0, Eof);
    TEST (T (0, 0, 0, 0, 0, 0, 6), "%a", -1, "a", 0, Eof);

    // avoid using <s><o/><n> or <s><o/><n><d><a><g> since it
    // contains the non-ASCII character <o/> (o with a slash)
    TEST (T (0, 0, 0, 0, 0, 0, 1), "man", 3, "a", 0, Eof);
    TEST (T (0, 0, 0, 0, 0, 0, 2), "tir", 3, "a", 0, Eof);
    TEST (T (0, 0, 0, 0, 0, 0, 3), "ons", 3, "a", 0, Eof);
    TEST (T (0, 0, 0, 0, 0, 0, 4), "tor", 3, "a", 0, Eof);
    TEST (T (0, 0, 0, 0, 0, 0, 5), "fre", 3, "a", 0, Eof);

    TEST (T (0, 0, 0, 0, 0, 0, 1), "mandag",   6, "a", 0, Good);
    TEST (T (0, 0, 0, 0, 0, 0, 2), "tirsdag",  7, "a", 0, Good);
    TEST (T (0, 0, 0, 0, 0, 0, 3), "onsdag",   6, "a", 0, Good);
    TEST (T (0, 0, 0, 0, 0, 0, 4), "torsdag",  7, "a", 0, Good);
    TEST (T (0, 0, 0, 0, 0, 0, 5), "fredag",   6, "a", 0, Good);

    // exercise full month names
    FUNCTION ("get_monthname");
    TEST (T (0, 0, 0, 0,  0), "%B", -1, "b", 0, -1);
    TEST (T (0, 0, 0, 0,  1), "%B", -1, "b", 0, -1);
    TEST (T (0, 0, 0, 0,  2), "%B", -1, "b", 0, -1);
    TEST (T (0, 0, 0, 0,  3), "%B", -1, "b", 0, -1);
    TEST (T (0, 0, 0, 0,  4), "%B", -1, "b", 0, -1);
    TEST (T (0, 0, 0, 0,  5), "%B", -1, "b", 0, -1);
    TEST (T (0, 0, 0, 0,  6), "%B", -1, "b", 0, -1);
    TEST (T (0, 0, 0, 0,  7), "%B", -1, "b", 0, -1);
    TEST (T (0, 0, 0, 0,  8), "%B", -1, "b", 0, -1);
    TEST (T (0, 0, 0, 0,  9), "%B", -1, "b", 0, -1);
    TEST (T (0, 0, 0, 0, 10), "%B", -1, "b", 0, -1);
    TEST (T (0, 0, 0, 0, 11), "%B", -1, "b", 0, -1);
}

/**************************************************************************/


struct time_data
{
    const char*        abday [7];     // %a   abbreviated day name
    const char*        day [7];       // %A   full day name
    const char*        abmon [12];    // %b   abbreviated month name
    const char*        mon [12];      // %B   full month name
    const char*        am_pm [2];     // %p   AM/PM designator
    const char*        d_t_fmt;       // %c   date and time
    const char*        d_fmt;         // %x   date
    const char*        t_fmt;         // %X   time
    const char*        t_fmt_ampm;    // %r   time with the AM/PM designaror
    const char*        era_d_t_fmt;   // %Ec  alternative date and time
    const char*        era_d_fmt;     // %EX  alternative date
    const char*        era_t_fmt;     // %Ex  alternative time
    const char* const* alt_digits;    //      alternative numeric symbols

    struct era_data {
        int         offset;
        int         start_day;   // [1..31]
        int         start_mon;   // [0..11]
        int         start_year;
        int         end_day;     // [1..31]
        int         end_mon;     // [0..11]
        int         end_year;    // INT_MIN, [0..9999], INT_MAX
        const char *name;
        const char *format;
    };

    const era_data* era;
};


// the root of the locale directory (RWSTD_LOCALE_ROOT)
// set in main() instead of here to avoid Solaris 7 putenv() bug (PR #30017)
static const char*
locale_root;


static const char*
make_LC_TIME (const time_data *td, const char *name,
              char *locnamebuf, std::size_t bufsize)
{
    // create a temporary locale definition file
    char srcfname [1024];
    if (rw_snprintf (srcfname, sizeof srcfname, "%s%c%s",
                     locale_root, _RWSTD_PATH_SEP, "LC_TIME.src") < 0)
        return 0;

    std::FILE *fout = std::fopen (srcfname, "w");
    rw_fatal (0 != fout, 0, __LINE__,
              "fopen(%#s, \"w\") failed: %{#m} - %m", srcfname);

    // rw_fatal() doesn't return on failure
    RW_ASSERT (0 != fout);

    std::fprintf (fout, "LC_TIME\n");

    unsigned i;

    std::fprintf (fout, "abday ");

    for (i = 0; i != sizeof td->abday / sizeof *td->abday; ++i) {
        std::fprintf (fout, "\"");
        pcs_write (fout, td->abday [i]);
        std::fprintf (fout, "\"%c", i < 6 ? ';' : '\n');
    }            

    std::fprintf (fout, "day ");

    for (i = 0; i != sizeof td->day / sizeof *td->day; ++i) {
        std::fprintf (fout, "\"");
        pcs_write (fout, td->day [i]);
        std::fprintf (fout, "\"%c", i < 6 ? ';' : '\n');
    }            

    std::fprintf (fout, "abmon ");

    for (i = 0; i != sizeof td->abmon / sizeof *td->abmon; ++i) {
        std::fprintf (fout, "\"");
        pcs_write (fout, td->abmon [i]);
        std::fprintf (fout, "\"%c", i < 11 ? ';' : '\n');
    }            

    std::fprintf (fout, "mon ");

    for (i = 0; i != sizeof td->mon / sizeof *td->mon; ++i) {
        std::fprintf (fout, "\"");
        pcs_write (fout, td->mon [i]);
        std::fprintf (fout, "\"%c", i < 11 ? ';' : '\n');
    }            

    std::fprintf (fout, "am_pm ");

    for (i = 0; i != sizeof td->am_pm / sizeof *td->am_pm; ++i) {
        std::fprintf (fout, "\"");
        pcs_write (fout, td->am_pm [i]);
        std::fprintf (fout, "\"%c", i < 1 ? ';' : '\n');
    }

    // write out d_t_fmt, d_fmt, t_fmt, etc.
    // preface each line with a comment giving
    // the specification in a human-readable format
    std::fprintf (fout, "\n# d_t_fmt \"%s\"\nd_t_fmt \"",
                  td->d_t_fmt);
    pcs_write (fout, td->d_t_fmt);

    std::fprintf (fout, "\"\n\n# d_fmt \"%s\"\nd_fmt \"",
                  td->d_fmt);
    pcs_write (fout, td->d_fmt);

    std::fprintf (fout, "\"\n\n# t_fmt \"%s\"\nt_fmt \"",
                  td->t_fmt);
    pcs_write (fout, td->t_fmt);

    std::fprintf (fout, "\"\n\n# t_fmt_ampm \"%s\"\nt_fmt_ampm \"",
                  td->t_fmt_ampm);
    pcs_write (fout, td->t_fmt_ampm);

    std::fprintf (fout, "\"\n\n# era_d_t_fmt \"%s\"\nera_d_t_fmt \"",
                  td->era_d_t_fmt);
    pcs_write (fout, td->era_d_t_fmt);

    std::fprintf (fout, "\"\n\n# era_d_fmt \"%s\"\nera_d_fmt \"",
                  td->era_d_fmt);
    pcs_write (fout, td->era_d_fmt);

    std::fprintf (fout, "\"\n\n# era_t_fmt \"%s\"\nera_t_fmt \"",
                  td->era_t_fmt);
    pcs_write (fout, td->era_t_fmt);

    std::fprintf (fout, "\"\nalt_digits ");

    for (i = 0; td->alt_digits [i]; ++i) {
        std::fprintf (fout, "\"");
        pcs_write (fout, td->alt_digits [i]);
        std::fprintf (fout, "\"%c", td->alt_digits [i + 1] ? ';' : '\n');
    }

    std::fprintf (fout, "\n\nera ");

    for (i = 0; INT_MIN != td->era [i].offset; ++i) {

        char segment [256];

        std::sprintf (segment, "%c:%d:%04d/%02d/%02d:",
                      td->era [i].offset < 0 ? '-' : '+',
                      td->era [i].offset < 0 ? -td->era [i].offset
                                             : td->era [i].offset,
                      td->era [i].start_year + 1900,
                      td->era [i].start_mon + 1,
                      td->era [i].start_day);

        const char *end_date = 0;

        if (INT_MIN == td->era [i].end_year)
            end_date = "-*";   // beginning of time
        else if (INT_MAX == td->era [i].end_year)
            end_date = "+*";   // end of time

        if (end_date) {
            std::sprintf (segment + std::strlen (segment), "%s:%s:%s",
                          end_date, td->era [i].name, td->era [i].format);
        }
        else {
            std::sprintf (segment + std::strlen (segment),
                          "%04d/%02d/%02d:%s:%s",
                          td->era [i].end_year + 1900,
                          td->era [i].end_mon + 1,
                          td->era [i].end_day,
                          td->era [i].name,
                          td->era [i].format);
        }

        std::fprintf (fout, "\"");
        pcs_write (fout, segment);
        std::fprintf (fout, "\"%c",
                      INT_MIN == td->era [i + 1].offset ? '\n' : ';');
    }

    std::fprintf (fout, "\nEND LC_TIME\n");

    std::fclose (fout);

    // create a temporary character map file
    char cmfname [1024];
    if (rw_snprintf (cmfname, sizeof cmfname, "%s%c%s",
                     locale_root, _RWSTD_PATH_SEP, "LC_TIME.cm") < 0)
        return 0;

    fout = std::fopen (cmfname, "w");
    rw_fatal (0 != fout, 0, __LINE__,
              "fopen(%#s, \"w\") failed: %{#m} - %m", cmfname);

    // rw_fatal() doesn't return on failure
    RW_ASSERT (0 != fout);

    pcs_write (fout, 0);

    std::fclose (fout);

    // process the locale definition file and the character map
    // rw_localedef() returns its own buffer, which the next call reuses
    const char* const locname =
        rw_localedef ("-w ", srcfname, cmfname, name);

    if (locname && (std::strlen(locname) < bufsize)) {
        std::strcpy (locnamebuf, locname);
        return locnamebuf;
    }

    return 0;
}


// with empty_ampm, the twin of the user-defined locale whose am_pm is
// two empty strings and whose %X is %r with an empty t_fmt_ampm, as
// in hy_AM, sm_WS and su_ID
static const char*
make_LC_TIME (bool empty_ampm)
{
    static char locnamebuf [2][1024];

    if (*locnamebuf [empty_ampm])
        return locnamebuf [empty_ampm];

    // 70 symbols, more than the 64 bits of one size_t that once held
    // the names, short of the 100 that localedef allows; time_get
    // reads plain digits only for a value that has no symbol, or for
    // 0 when its symbol is empty, so %OC on 70..99 reads digits;
    // the symbol for 40 begins the one for 41, as lzh_TW's 20 begins
    // its 21 through 29
    const char* const alt_digits[] = {
        "",      "1st",  "2nd",  "3rd",  "4th",
         "5th",  "6th",  "7th",  "8th",  "9th",
        "10th", "11th", "12th", "13th", "14th",
        "15th", "16th", "17th", "18th", "19th",
        "20th", "21st", "22nd", "23rd", "24th",
        "25th", "26th", "27th", "28th", "29th",
        "30th", "31st", "32nd", "33rd", "34th",
        "35th", "36th", "37th", "38th", "39th",
        "forty", "forty-one", "42nd", "43rd", "44th",
        "45th", "46th", "47th", "48th", "49th",
        "50th", "51st", "52nd", "53rd", "54th",
        "55th", "56th", "57th", "58th", "59th",
        "60th", "61st", "62nd", "63rd", "64th",
        "65th", "66th", "67th", "68th", "69th",
        0   // sentinel
    };

    const time_data::era_data era[] = {
        // some entries copied and modified from HP-UX 11.11 ja_JP.utf8
        //
        // # 1990-01-02 and onward:    "Heisei%EyNen" (1990 is Heisei 2 Nen.)
        // # 1990-01-01 to 1990-01-02: "%C"           (no era specified)
        // # 1989-01-08 to 1989-12-31: "HeiseiGannen"
        // # 1927-01-01 to 1989-01-07: "Showa%EyNen"  (1927 is Showa 2 Nen.)
        // # 1927-01-01 and prior:     "foo"
        // era "+:2:1990/01/02:+*:<U????><U????>:%EC%Ey<U????>";
        //     "+:1:1989/01/08:1989/12/31:<U????>:%EC<U????><U????>";
        //     "+:2:1927/01/01:1989/01/07:<U????><U????>:%EC%Ey<U????>";
        //     "-:7:1926/12/31:-*:foo:bar%EC%Ey"

        //   7|2        |1        | |2   starting year in an era
        // <<<|>>>>>>>>>|>>>>>>>>>|.|>>> direction in which years increase
        //    ^         ^         ^ ^
        //    |         |         | |
        //    |         |         | +-> Jan 2, 1990 (Heisei %Ey Nen)
        //    |         |         +-> Dec 31, 1989 (%C)
        //    |         +-> Jan 7, 1989 (Heisei Gannen)
        //    +-> Jan 1, 1927 (Showa %Ey Nen)
        //    +-< Dec 31, 1926 (foobar %Ey)

        {  2,  2,  0, 90,  0,  0, INT_MAX, "Heisei", "%EC %Ey Nen" },
        {  1,  8,  0, 89, 31, 11,      89, "Heisei", "%EC Gannen" },
        {  2,  1,  0, 27,  7,  0,      89, "Showa",  "%EC %Ey Nen" },
        { -7, 31, 11, 26,  0,  0, INT_MIN, "bar",    "foo%EC %Ey" },
        { INT_MIN /* sentinel entry */, 0, 0, 0, 0, 0, 0, 0, 0 }
    };

    time_data td = {
        // %a
        { "[Sun]", "[Mon]", "[Tue]", "[Wed]", "[Thu]", "[Fri]", "[Sat]" },

        // %A
        {
            "[Sunday]", "[Monday]", "[Tuesday]", "[Wednesday]", "[Thursday]",
            "[Friday]", "[Saturday]"
        },

        // %b
        {
            "[Jan]", "[Feb]", "[Mar]", "[Apr]", "[May]", "[Jun]",
            "[Jul]", "[Aug]", "[Sep]", "[Oct]", "[Nov]", "[Dec]",
        },

        // %B
        {
            "[January]", "[February]", "[March]", "[April]", "[May]", "[June]",
            "[July]", "[August]", "[September]", "[October]", "[November]",
            "[December]",
        },

        { "[A.M.]", "[P.M.]" },                 // %p

        "[[%a][%b][%d][%H][%M][%S][%y]]",       // %c
        "[[%3m][%.4d][%4.3y]]",                 // %x
        "[[%3H][%.4M][%4.3S]]",                 // %X
        "[[%I][%M][%S][%p]]",                   // %r
        "[[%A][%B][%Od][%H][%M][%S][%Y]]",      // %Ec
        "%Ow weekday, %OU week, %Oy year",      // %Ex
        "%OH hour, %OM minute, %OS second",     // %EX

        alt_digits, era
    };

    if (empty_ampm) {
        td.am_pm [0]  = "";
        td.am_pm [1]  = "";
        td.day [0]    = "";   // a name directive with an empty first name
        td.t_fmt      = "%r";
        td.t_fmt_ampm = "";
    }

    return make_LC_TIME (&td, empty_ampm ? "test-locale-ampm" : "test-locale",
                         locnamebuf [empty_ampm], sizeof *locnamebuf);
}

/**************************************************************************/


template <class charT>
void test_user (charT, const char *cname, const char *locname)
{
    const char* const envar = std::getenv (LOCALE_ROOT_ENVAR);

    rw_info (0, 0, __LINE__,
             "std::time_get<%s>, in locale(\"%s\") "
             "created from a generated LC_TIME file; "
             LOCALE_ROOT_ENVAR "=%s",
             cname, locname, envar);

    // construct a locale object from the binary database
    std::locale loc;

    try {
        loc = std::locale (loc, locname, std::locale::time);
    }
    catch (...) {
        rw_error (0, 0, __LINE__,
                  "locale (locale (), \"%s\", locale::time) "
                  "unexpectedly threw an exception", locname);
        return;
    }

    // set the global locale (used by the test function)
    std::locale::global (loc);

    
    // %Ec: The locale's alternative date and time representation.
    STEP ("%Ec: alternative date and time representation");
    TEST (T (0, 0, 0, 1, 0, 0, 0),
          // "[[%A][%B][%Od][%H][%M][%S][%Y]]"
          "[[[Sunday]][[January]][1st][00][00][00][1900]]", 46,
          "%Ec", 0, Good);

    TEST (T (5, 4, 3, 2, 1, 1, 1),
          "[[[monday]][[february]][2nd][03][04][05][1901]]", 47,
          "%Ec", 0, Good);

    TEST (T (60, 59, 23, 31, 2, -1898, 2),
          // %Y requires a four digit year, 02 is not treated as 2002
          "[[[TUE]][[MAR]][31st][23][59][60][02]]", 38,
          "%Ec", 0, Good);

    // %EC: The name of the base year (period) in the locale's
    //      alternative representation.

    // %Ex: The locale's alternative date representation.
    STEP ("%Ex: alternative date representation");
    rw_warn (0, 0, __LINE__, "%%Ex not being exercised");

    // verify that facet computes the remaining tm members from
    // the supplied data
    //TEST (T (0, 0, 0, 21, 4, 2002, 2, 141),
    //      // "%Ow weekday, %OU week, %Oy year"
    //      "2nd weekday, 21st week, 2nd year", 32,
    //      // i.e., May 21, 2002
    //      "%Ex", 0, Good);

    // %EX: The locale's alternative time representation.
    STEP ("%EX: alternative time representation");
    // verify that 0 is correctly parsed if alternative numeric
    // symbols are expected and the symbol for zero is empty
    TEST (T (0, 1, 2),
          // "%OH hour, %OM minute, %OS second"
          "2nd hour, 1st minute, 0 second", 30,
          "%EX", 0, Good);

    TEST (T (21, 22, 23),
          // "%OH hour, %OM minute, %OS second"
          "23rd hour, 22nd minute, 21st second", 35,
          "%EX", 0, Good);

    // %Ey: The offset from %EC (year only) in the locale's
    //      alternative representation.
    STEP ("%Ey: offset from %EC in alternative representation.");
    rw_warn (0, 0, __LINE__, "%%Ey not being exercised");

    // %EY: The full alternative year representation.
    STEP ("%EY: the full alternative year representation");
    rw_warn (0, 0, __LINE__, "%%EY not being exercised");

    // %OC: The century using the locale's alternative numeric symbols
    //      (an extension of the C library's strftime).
    STEP ("%OC: the century using alternative numeric symbols");
    TEST (T (0, 0, 0, 0, 0, -1800),  "1st", 3, "%OC", 0, Good);
    TEST (T (0, 0, 0, 0, 0,     0), "19th", 4, "%OC", 0, Good);
    TEST (T (0, 0, 0, 0, 0,   100), "20th", 4, "%OC", 0, Good);
    TEST (T (0, 0, 0, 0, 0,   200), "21st", 4, "%OC", 0, Good);
    TEST (T (0, 0, 0, 0, 0,  1200), "31st", 4, "%OC", 0, Good);
    // names past the 64th, more than one size_t has bits for
    TEST (T (0, 0, 0, 0, 0,  4600), "65th", 4, "%OC", 0, Good);
    TEST (T (0, 0, 0, 0, 0,  5000), "69th", 4, "%OC", 0, Good);
    // a name that a later, longer name begins with, complete when the
    // longer one dies on the next character, which stays unread
    TEST (T (0, 0, 0, 0, 0,  2100),     "fortyX", 5, "%OC", 0, Good);
    TEST (T (0, 0, 0, 0, 0,  2200), "forty-oneX", 9, "%OC", 0, Good);
    // the longer name dies a character later: taking the shorter one
    // would need the consumed '-' put back, which an input iterator
    // cannot do
    TEST (T (0, 0, 0, 0, 0,     0),    "forty-X", 6, "%OC", 0, Fail);
    // digits past the list of names
    TEST (T (0, 0, 0, 0, 0,  5100), "70th", 2, "%OC", 0, Good);
    TEST (T (0, 0, 0, 0, 0,  6100),   "80", 2, "%OC", 0, Eof);
    TEST (T (0, 0, 0, 0, 0,  8000),  "099", 3, "%OC", 0, Eof);
    TEST (T (0, 0, 0, 0, 0,     0), "100th", 3, "%OC", 0, Fail);
    TEST (T (0, 0, 0, 0, 0,     0),   "32", 2, "%OC", 0, Eof | Fail);
    TEST (T (0, 0, 0, 0, 0,     0),  "100", 3, "%OC", 0, Eof | Fail);

    // %Od: The day of the month using the locale's alternative
    //      numeric symbols; leading zeros are permitted but not required.
    STEP ("%Od: the day of the month using alternative numeric symbols");
    TEST (T (0, 0, 0,  1),  "1st", 3, "%Od", 0, Good);
    TEST (T (0, 0, 0,  2),  "2nd", 3, "%Od", 0, Good);
    TEST (T (0, 0, 0, 11), "11th", 4, "%Od", 0, Good);
    TEST (T (0, 0, 0, 21), "21st", 4, "%Od", 0, Good);
    TEST (T (0, 0, 0, 30), "30th", 4, "%Od", 0, Good);
    TEST (T (0, 0, 0, 31), "31st", 4, "%Od", 0, Good);
    TEST (T (0, 0, 0,  0),   "32", 2, "%Od", 0, Eof | Fail);

    // %Oe: Equivalent to %Od.
    STEP ("%Oe: equivalent to %Od");
    TEST (T (0, 0, 0,  3),  "3rd", 3, "%Oe", 0, Good);
    TEST (T (0, 0, 0,  4),  "4th", 3, "%Oe", 0, Good);
    TEST (T (0, 0, 0,  9),  "9th", 3, "%Oe", 0, Good);
    TEST (T (0, 0, 0, 10), "10th", 4, "%Oe", 0, Good);
    TEST (T (0, 0, 0,  0), "32nd", 4, "%Oe", 0, Fail);
    TEST (T (0, 0, 0,  0),   "33", 2, "%Oe", 0, Eof | Fail);

    // %OH: The hour (24-hour clock) using the locale's alternative
    //      numeric symbols.
    STEP ("%OH: the hour (24-hour clock) using alternative numeric symbols");
    TEST (T (0, 0,  0),    "0", 1, "%OH", 0, Eof);
    TEST (T (0, 0,  0),   "00", 2, "%OH", 0, Eof);
    TEST (T (0, 0,  1),  "1st", 3, "%OH", 0, Good);
    TEST (T (0, 0,  2),  "2nd", 3, "%OH", 0, Good);
    TEST (T (0, 0, 11), "11th", 4, "%OH", 0, Good);
    TEST (T (0, 0, 12), "12th", 4, "%OH", 0, Good);
    TEST (T (0, 0, 13), "13th", 4, "%OH", 0, Good);
    TEST (T (0, 0, 23), "23rd", 4, "%OH", 0, Good);

    // %OI: The hour (12-hour clock) using the locale's alternative
    //      numeric symbols.
    STEP ("%OI: the hour (12-hour clock) using alternative numeric symbols");
    // stored as %I is, the hour less one; there is no hour 0
    TEST (T (0, 0,  0),    "0", 1, "%OI", 0, Eof | Fail);
    TEST (T (0, 0,  0),  "000", 3, "%OI", 0, Eof | Fail);
    TEST (T (0, 0,  0),  "1st", 3, "%OI", 0, Good);
    TEST (T (0, 0,  2),  "3rd", 3, "%OI", 0, Good);
    TEST (T (0, 0,  8),  "9th", 3, "%OI", 0, Good);
    TEST (T (0, 0, 10), "11th", 4, "%OI", 0, Good);
    TEST (T (0, 0, 11), "12th", 4, "%OI", 0, Good);
    TEST (T (0, 0,  0), "13th", 4, "%OI", 0, Fail);
    TEST (T (0, 0,  0),   "14", 2, "%OI", 0, Eof | Fail);

    // %p after %OI adds the 12 hours, after %OH it is parsed and ignored
    STEP ("%p with %OI and %OH");
    TEST (T (9, 5,  9),  "9th:5th:9th [A.M.]", 18, "%OI:%OM:%OS %p", 0, Good);
    TEST (T (9, 5, 21),  "9th:5th:9th [P.M.]", 18, "%OI:%OM:%OS %p", 0, Good);
    TEST (T (9, 5, 21), "21st:5th:9th [P.M.]", 19, "%OH:%OM:%OS %p", 0, Good);

    // %Om: The month using the locale's alternative numeric symbols.
    STEP ("%Om: the month using alternative numeric symbols");
    TEST (T (0, 0, 0, 0,  0),  "1st", 3, "%Om", 0, Good);
    TEST (T (0, 0, 0, 0,  1),  "2nd", 3, "%Om", 0, Good);
    TEST (T (0, 0, 0, 0,  2),  "3rd", 3, "%Om", 0, Good);
    TEST (T (0, 0, 0, 0,  3),  "4th", 3, "%Om", 0, Good);
    TEST (T (0, 0, 0, 0,  4),  "5th", 3, "%Om", 0, Good);
    TEST (T (0, 0, 0, 0,  5),  "6th", 3, "%Om", 0, Good);
    TEST (T (0, 0, 0, 0,  6),  "7th", 3, "%Om", 0, Good);
    TEST (T (0, 0, 0, 0,  7),  "8th", 3, "%Om", 0, Good);
    TEST (T (0, 0, 0, 0,  8),  "9th", 3, "%Om", 0, Good);
    TEST (T (0, 0, 0, 0,  9), "10th", 4, "%Om", 0, Good);
    TEST (T (0, 0, 0, 0, 10), "11th", 4, "%Om", 0, Good);
    TEST (T (0, 0, 0, 0, 11), "12th", 4, "%Om", 0, Good);
    TEST (T (0, 0, 0, 0,  0), "13th", 4, "%Om", 0, Fail);
    TEST (T (0, 0, 0, 0,  0),    "0", 1, "%Om", 0, Eof | Fail);
    TEST (T (0, 0, 0, 0,  0),   "00", 2, "%Om", 0, Eof | Fail);
    TEST (T (0, 0, 0, 0,  0),   "01", 2, "%Om", 0, Eof | Fail);

    // %OM: The minutes using the locale's alternative numeric symbols.
    STEP ("%OM: the minutes using alternative numeric symbols");
    TEST (T (0,  0),    "0", 1, "%OM", 0, Eof);
    TEST (T (0,  1),  "1st", 3, "%OM", 0, Good);
    TEST (T (0,  2),  "2nd", 3, "%OM", 0, Good);
    TEST (T (0,  3),  "3rd", 3, "%OM", 0, Good);
    TEST (T (0,  4),  "4th", 3, "%OM", 0, Good);
    TEST (T (0, 29), "29th", 4, "%OM", 0, Good);
    TEST (T (0, 30), "30th", 4, "%OM", 0, Good);
    TEST (T (0, 31), "31st", 4, "%OM", 0, Good);
    TEST (T (0,  0),   "32", 2, "%OM", 0, Eof | Fail);
    TEST (T (0,  0),   "60", 2, "%OM", 0, Eof | Fail);
    TEST (T (0,  0),    "2", 1, "%OM", 0, Eof | Fail);

    // %OS: The seconds using the locale's alternative numeric symbols.
    STEP ("%OS: the seconds using alternative numeric symbols");
    TEST (T ( 0),    "0", 1, "%OS", 0, Eof);
    TEST (T ( 1),  "1st", 3, "%OS", 0, Good);
    TEST (T ( 2),  "2nd", 3, "%OS", 0, Good);
    TEST (T ( 3),  "3rd", 3, "%OS", 0, Good);
    TEST (T ( 4),  "4th", 3, "%OS", 0, Good);
    TEST (T (29), "29th", 4, "%OS", 0, Good);
    TEST (T (30), "30th", 4, "%OS", 0, Good);
    TEST (T (31), "31st", 4, "%OS", 0, Good);
    TEST (T ( 0),   "32", 2, "%OS", 0, Eof | Fail);
    TEST (T ( 0),  "059", 3, "%OS", 0, Eof | Fail);
    TEST (T ( 0), "0060", 4, "%OS", 0, Eof | Fail);
    TEST (T ( 0),   "61", 2, "%OS", 0, Eof | Fail);
    TEST (T ( 0),   "21", 2, "%OS", 0, Eof | Fail);

    // %OU: The week number of the year (Sunday as the first day of the week)
    //      using the locale's alternative numeric symbols.
    STEP ("%OU: the Sunday-based week using alternative numeric symbols");
    rw_warn (0, 0, __LINE__, "%%OU not being exercised");

    // %Ow: The number of the weekday (Sunday=0) using the locale's
    //      alternative numeric symbols.
    STEP ("%Ow: Sunday-based weekday using alternative numeric symbols");
    TEST (T (0, 0, 0, 0, 0, 0, 0),   "0", 1, "%Ow", 0, Eof);
    TEST (T (0, 0, 0, 0, 0, 0, 1), "1st", 3, "%Ow", 0, Good);
    TEST (T (0, 0, 0, 0, 0, 0, 2), "2nd", 3, "%Ow", 0, Good);
    TEST (T (0, 0, 0, 0, 0, 0, 6), "6th", 3, "%Ow", 0, Good);
    TEST (T (0, 0, 0, 0, 0, 0, 0), "7th", 3, "%Ow", 0, Fail);
    TEST (T (0, 0, 0, 0, 0, 0, 0),   "8", 1, "%Ow", 0, Eof | Fail);

    // %OW: The week number of the year (Monday as the first day of the week)
    //      using the locale's alternative numeric symbols.
    STEP ("%OW: the Monday-based week using alternative numeric symbols");
    rw_warn (0, 0, __LINE__, "%%OW not being exercised");

    // %Oy: The year (offset from %C ) using the locale's alternative
    //      numeric symbols.
    STEP ("%Oy: year offset from %C using alternative numeric symbols");
    // with no century, 69..99 are 1969..1999 and 0..68 2000..2068
    TEST (T (0, 0, 0, 0, 0, 100),        "0", 1, "%Oy",    0, Eof);
    TEST (T (0, 0, 0, 0, 0, 101),      "1st", 3, "%Oy",    0, Good);
    TEST (T (0, 0, 0, 0, 0, 131),     "31st", 4, "%Oy",    0, Good);
    TEST (T (0, 0, 0, 0, 0, 168),     "68th", 4, "%Oy",    0, Good);
    TEST (T (0, 0, 0, 0, 0,  69),     "69th", 4, "%Oy",    0, Good);
    TEST (T (0, 0, 0, 0, 0,  99),       "99", 2, "%Oy",    0, Eof);
    TEST (T (0, 0, 0, 0, 0,   0),      "100", 3, "%Oy",    0, Eof | Fail);
    // a century combines with the year in either order
    TEST (T (0, 0, 0, 0, 0,  26), "19th26th", 8, "%OC%Oy", 0, Good);
    TEST (T (0, 0, 0, 0, 0, 126), "20th26th", 8, "%OC%Oy", 0, Good);
    TEST (T (0, 0, 0, 0, 0,  26), "26th19th", 8, "%Oy%OC", 0, Good);
}

/**************************************************************************/


template <class charT>
void test_empty_ampm (charT, const char *cname, const char *locname)
{
    rw_info (0, 0, __LINE__,
             "std::time_get<%s>, in locale(\"%s\") "
             "created from a generated LC_TIME file with an empty am_pm",
             cname, locname);

    std::locale loc;

    try {
        loc = std::locale (loc, locname, std::locale::time);
    }
    catch (...) {
        rw_error (0, 0, __LINE__,
                  "locale (locale (), \"%s\", locale::time) "
                  "unexpectedly threw an exception", locname);
        return;
    }

    std::locale::global (loc);

    // an empty AM name matches at once, as in the C library's strptime,
    // which tries the AM name first; the PM name is never reached, so
    // time_put's "09:05:09 " of 21:05:09 reads back as 09:05:09; nor
    // does %p read the zeros of an empty alternative digit 0
    STEP ("%p with an empty am_pm: the empty AM name matches");
    TEST (T (0, 0, 1),            "", 0, "p",           0, Eof);
    TEST (T (0, 0, 1),           "x", 0, "p",           0, Good);
    TEST (T (0, 0, 1),           "0", 0, "p",           0, Good);
    TEST (T (9, 5, 9),   "09:05:09 ", 9, "%I:%M:%S %p", 0, Eof);
    TEST (T (9, 5, 9), "09:05:09 PM", 9, "%I:%M:%S %p", 0, Good);
    TEST (T (0, 0, 0),   "21:05:09 ", 1, "%I:%M:%S %p", 0, Fail);
    TEST (T (9, 5, 9),   "09:05:09 ", 9, "r",           0, Eof);
    TEST (T (9, 5, 9),   "09:05:09 ", 9, "X",           0, Eof);

    // a value without an alternative symbol is read as digits by the
    // name matcher; a name directive whose first name is empty fails
    // on a digit instead of consuming a run of zeros
    STEP ("a digit under an empty first name");
    TEST (T (0, 0),                   "0", 1, "OM", 0, Eof);
    TEST (T (0, 0, 0, 0, 0, 0, 0),    "0", 0, "a",  0, Fail);
}

/**************************************************************************/

static int
run_test (int, char**)
{
    // set up RWSTD_LOCALE_ROOT and other environment variables
    // here as opposed to at program startup to work around a
    // SunOS 5.7 bug in putenv() (PR ##30017)
    locale_root = rw_set_locale_root ();

    // store the next available file descriptor
    // in order to detect file descriptor leaks
    const int fd0 = rw_nextfd (0);

    test_posix (char (), "char");

    const char *locname;

    const char en[] = "en-*-*-*";
    const char de[] = "de-*-*-*";
    const char da[] = "da-*-*-*";

    int nnamed = 0;

    // try to find and exercise the english locale
    if ((locname = rw_locale_query (LC_ALL, en, 1)) && *locname) {
        test_english (char (), "char", locname);
        ++nnamed;
    }

    // try to find and exercise the german locale
    if ((locname = rw_locale_query (LC_ALL, de, 1)) && *locname) {
        test_german (char (), "char", locname);
        ++nnamed;
    }

    // try to find and exercise the danish locale
    if ((locname = rw_locale_query (LC_ALL, da, 1)) && *locname) {
        test_danish (char (), "char", locname);
        ++nnamed;
    }

    //////////////////////////////////////////////////////////////////
    // exercise the time_get facets with a user-defined locale
    std::locale::global (std::locale ("C"));

    // create a LC_TIME database based on user-defined data
    locname = make_LC_TIME (false);

    // and its twin with an empty am_pm
    const char* const ampm_locname = make_LC_TIME (true);

    if (!locname || !ampm_locname) {
        rw_error (0, 0, __LINE__, "failed to generate LC_TIME locale data");
        return 0;
    }

    test_user (char (), "char", locname);

    test_empty_ampm (char (), "char", ampm_locname);

    test_posix (wchar_t (), "wchar_t");

    test_user (wchar_t (), "wchar_t", locname);

    test_empty_ampm (wchar_t (), "wchar_t", ampm_locname);

    // reset the gloab locale to free up resources (such as file
    // descriptors used by time_get_byname to map locale databases)
    std::locale::global (std::locale ("C"));

    const int fd1 = rw_nextfd (0);

    rw_assert (0 == fd1 - fd0, 0, __LINE__,
               "%d file descriptor leaks detected", fd1 - fd0);

    rw_assert (0 != nnamed, 0, __LINE__,
               "time_get not exercised in named locales");

    return 0;
}

/**************************************************************************/

int main (int argc, char *argv[])
{
    return rw_test (argc, argv, __FILE__,
                    "lib.locale.time.get",
                    0 /* no comment */,
                    run_test,
                    "",
                    0);
}
