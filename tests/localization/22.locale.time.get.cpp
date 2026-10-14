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

#include <clocale>   // for LC_ALL
#include <cstring>   // for memcmp(), strlen()
#include <ctime>     // for struct tm

#include <rw_driver.h>   // for rw_test()
#include <rw_file.h>     // for rw_nextfd()
#include <rw_locale.h>   // for rw_locale_query()

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

    _TRY {
        // the single-character format strings "a", "b", "x", "X", and "Y"
        // select get_weekday(), get_monthname(), get_date(), get_time()
        // and get_year()

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
            rw_error (0, 0, lineno, "line %d. unexpected format \"%s\"",
                      __LINE__, fmt);
            return;
        }
    }
    _CATCH (...) {
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
              int wday = 0, int yday = 0, int isdst = 0)
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
    TEST (T (0, 0, 0, 0, 0, 98099), "99999", 5, "Y", 0, Eof);
}

/**************************************************************************/


template <class charT>
void test_english (charT, const char *cname, const char *locname)
{
    _TRY {
        // set the global locale object to the german locale
        std::locale::global (std::locale (locname));
    }
    _CATCH (...) {
        rw_error (0, 0, __LINE__,
                  "locale(%#s) unexpectedly threw an exception", locname);

        return;
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
    _TRY {
        // set the global locale object to the german locale
        std::locale::global (std::locale (locname));
    }
    _CATCH (...) {
        rw_error (0, 0, __LINE__,
                  "locale(%#s) unexpectedly threw an exception", locname);

        return;
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
    _TRY {
        // set the global locale object to the german locale
        std::locale::global (std::locale (locname));
    }
    _CATCH (...) {
        rw_error (0, 0, __LINE__,
                  "locale(%#s) unexpectedly threw an exception", locname);

        return;
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

static int
run_test (int, char**)
{
    // set up RWSTD_LOCALE_ROOT and other environment variables
    // here as opposed to at program startup to work around a
    // SunOS 5.7 bug in putenv() (PR ##30017)
    rw_set_locale_root ();

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

#ifndef _RWSTD_NO_WCHAR_T

    test_posix (wchar_t (), "wchar_t");

#endif   // _RWSTD_NO_WCHAR_T

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
