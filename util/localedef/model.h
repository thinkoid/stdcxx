/***************************************************************************
 *
 * model.h
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

#ifndef RWSTD_UTIL_LOCALEDEF_MODEL_H_INCLUDED
#define RWSTD_UTIL_LOCALEDEF_MODEL_H_INCLUDED

#include <map>
#include <vector>


// The model of a locale definition: one plain struct per category, as
// the source sets it.  Strings are UCS-4 code points; the charmap gives
// their bytes later.  The model holds no default: glibc tells a keyword
// the source leaves unset from one it sets, and supplies the defaults
// after reading, and so will the back end.

// A string of code points, as reader.h spells a code point.
typedef std::vector<long> ucs_string_t;


// A keyword's value and whether the source set it.
template <class T>
struct field_t {

    field_t ()
        : value (), set (false) { }

    T    value;
    bool set;
};


// LC_NUMERIC
struct numeric_def_t {
    field_t<ucs_string_t>     decimal_point;
    field_t<ucs_string_t>     thousands_sep;
    field_t<std::vector<int> > grouping;   // as written: -1 and 0 kept
};


// LC_MONETARY: the keywords of POSIX and ISO C.  The second currency,
// its dates and its conversion rate are read and ignored.
struct monetary_def_t {
    field_t<ucs_string_t>      int_curr_symbol;
    field_t<ucs_string_t>      currency_symbol;
    field_t<ucs_string_t>      mon_decimal_point;
    field_t<ucs_string_t>      mon_thousands_sep;
    field_t<std::vector<int> > mon_grouping;
    field_t<ucs_string_t>      positive_sign;
    field_t<ucs_string_t>      negative_sign;
    field_t<int>               int_frac_digits;
    field_t<int>               frac_digits;
    field_t<int>               p_cs_precedes;
    field_t<int>               p_sep_by_space;
    field_t<int>               n_cs_precedes;
    field_t<int>               n_sep_by_space;
    field_t<int>               p_sign_posn;
    field_t<int>               n_sign_posn;
    field_t<int>               int_p_cs_precedes;
    field_t<int>               int_p_sep_by_space;
    field_t<int>               int_n_cs_precedes;
    field_t<int>               int_n_sep_by_space;
    field_t<int>               int_p_sign_posn;
    field_t<int>               int_n_sign_posn;
};


// The bound of an era that runs without end, -* and +*, as glibc
// stores them in each of the three fields of a date.
enum {
    era_minus_infinity = -2147483647 - 1,   // glibc's 0x80000000
    era_plus_infinity  =  2147483647        // glibc's 0x7fffffff
};

// One era: the operand string as written, which is what nl_langinfo
// returns, and its fields as glibc's time_finish() parses them.  A date
// is glibc's: the year less 1900 with no year 0, so that -1 is 1 BC;
// the month from 0; the day from 1.
struct era_t {
    ucs_string_t text;
    char         direction;   // + or -
    int          offset;
    int          start [3];   // year, month, day
    int          end [3];
    ucs_string_t name;
    ucs_string_t format;
};


// LC_TIME.  alt_mon, ab_alt_mon, date_fmt, week, first_weekday,
// first_workday, cal_direction, timezone and era_year are read and
// ignored.
struct time_def_t {
    field_t<std::vector<ucs_string_t> > abday;
    field_t<std::vector<ucs_string_t> > day;
    field_t<std::vector<ucs_string_t> > abmon;
    field_t<std::vector<ucs_string_t> > mon;
    field_t<std::vector<ucs_string_t> > am_pm;
    field_t<ucs_string_t>               d_t_fmt;
    field_t<ucs_string_t>               d_fmt;
    field_t<ucs_string_t>               t_fmt;
    field_t<ucs_string_t>               t_fmt_ampm;
    std::vector<era_t>                  era;   // empty when unset
    field_t<ucs_string_t>               era_d_fmt;
    field_t<ucs_string_t>               era_d_t_fmt;
    field_t<ucs_string_t>               era_t_fmt;
    field_t<std::vector<ucs_string_t> > alt_digits;
};


// LC_MESSAGES.  yesstr and nostr are read and ignored.
struct messages_def_t {
    field_t<ucs_string_t> yesexpr;
    field_t<ucs_string_t> noexpr;
};


// A run of code points, first to last.  A single character is a run of
// one, and so is an ellipsis over <Uxxxx> names.
struct ucs_range_t {
    long first;
    long last;
};

// A character class as the source writes it: runs, not a table.  They
// are in ascending order, and runs that overlap or touch are merged as
// they are added, so that no two do.
typedef std::vector<ucs_range_t> ucs_class_t;

// One pair of a case map.
struct ucs_pair_t {
    long from;
    long to;
};

// A case map in ascending order of from, one pair per character: a
// later pair for a character replaces an earlier one, as in glibc.
typedef std::vector<ucs_pair_t> ucs_map_t;

// The transliteration table: a source of one or more code points and
// its alternatives in order of preference.  An alternative may be
// empty, and an entry may have none.
typedef std::map<ucs_string_t, std::vector<ucs_string_t> > translit_t;


// LC_CTYPE: the ten classes std::ctype_base gives a bit, blank, which
// it has none for until C++11 and the old compiler skipped, the two
// case maps and the transliteration.  alnum is alpha and digit, in the
// library as in glibc; the alnum keyword, other classes and maps,
// charclass, charconv, outdigit, default_missing and translit_ignore
// are read and ignored.  A class or a map is unset when no statement
// names it; glibc then supplies its default after reading.
struct ctype_def_t {
    field_t<ucs_class_t> upper;
    field_t<ucs_class_t> lower;
    field_t<ucs_class_t> alpha;
    field_t<ucs_class_t> digit;
    field_t<ucs_class_t> xdigit;
    field_t<ucs_class_t> space;
    field_t<ucs_class_t> print;
    field_t<ucs_class_t> graph;
    field_t<ucs_class_t> blank;
    field_t<ucs_class_t> cntrl;
    field_t<ucs_class_t> punct;
    field_t<ucs_map_t>   toupper;
    field_t<ucs_map_t>   tolower;
    translit_t           translit;
};


// The categories read so far.  LC_COLLATE joins when its reader does.
struct locale_def_t {
    ctype_def_t    ctype;
    numeric_def_t  numeric;
    monetary_def_t monetary;
    time_def_t     time;
    messages_def_t messages;
};

#endif   // RWSTD_UTIL_LOCALEDEF_MODEL_H_INCLUDED
