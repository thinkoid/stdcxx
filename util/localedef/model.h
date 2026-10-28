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

#include "reader.h"   // for name_range_t

#include <map>
#include <string>
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

    // The locale whose LC_CTYPE tables these are: the name a copy
    // statement gives when no class or map statement follows it, the
    // innermost such copy when the copied locale is itself one; empty
    // otherwise.  The back end shares one file between the locales
    // that copy the same one.
    std::string          copied;
};


// LC_COLLATE stays symbolic: an element and its weights are names, as
// the source writes them, and resolving them to numbers is the back
// end's.  A name is the text between the brackets, but that a <Uxxxx>
// name, or a character written raw, is U and 4 hexadecimal digits
// below 0x10000 and 8 from there, as range_member() spells it, so that
// <U00E5> and <U000000E5> are one name.

// The directions of one level of a section, as bits.  position alone
// reads as position and forward, as in glibc.
enum {
    dir_forward  = 1,
    dir_backward = 2,
    dir_position = 4
};

// A section, order_start to order_end: the script it is named for,
// empty for the unnamed section, and the directions of each level.
struct coll_section_t {
    std::string      name;
    std::vector<int> rules;
};

// collating-element <name> from "string"
struct coll_element_t {
    std::string  name;
    ucs_string_t value;
};

// A weight of one level: names, more than one when a string writes
// them, IGNORE, or the element itself, which a level the line leaves
// out is.  For a range the element itself is each member.
enum coll_weight_kind_t {
    cw_names,
    cw_ignore,
    cw_self
};

struct coll_weight_t {
    coll_weight_kind_t       kind;
    std::vector<std::string> names;   // cw_names only
};

// What an entry orders.  A name nothing declares is a symbol, as in
// glibc.  A range is the members an ellipsis line stands for, those
// between the entries before and after it, kept as one entry.
enum coll_kind_t {
    ck_char,        // a <Uxxxx> name, a character raw, or a charmap name
    ck_symbol,      // a collating symbol, declared or not
    ck_element,     // a collating element
    ck_undefined,   // UNDEFINED
    ck_range        // an ellipsis line
};

// One entry of the sequence: what it orders, its weights, one per
// level, the section it was placed in, and the line that placed it.
struct coll_entry_t {
    coll_kind_t                kind;
    std::string                name;      // empty for UNDEFINED and a range
    long                       ucs;       // a character's code point, or -1
    name_range_t               range;     // ck_range: the members, first and
                                          // last included
    std::vector<coll_weight_t> weights;
    int                        section;   // in sections; -1 before any
    int                        file;      // in files
    int                        line;
};

// LC_COLLATE: the declarations, the sections in the order they were
// opened, and the sequence, one list in collation order with the
// reorder-after blocks placed.  A collating symbol is declared by name
// or by range, and a single name is a range of one with no digits.
// symbol-equivalence, coll_weight_max and section-symbol are read and
// ignored.  codepoint_collation orders every character by its value,
// the back end's to apply.
struct collate_def_t {

    collate_def_t ()
        : codepoint_collation (false) { }

    bool                        codepoint_collation;
    std::vector<name_range_t>   symbols;
    std::vector<coll_element_t> elements;
    std::vector<std::string>    scripts;
    std::vector<coll_section_t> sections;
    std::vector<coll_entry_t>   sequence;
    std::vector<std::string>    files;   // the sources the entries name
};


// The categories read.
struct locale_def_t {
    collate_def_t  collate;
    ctype_def_t    ctype;
    numeric_def_t  numeric;
    monetary_def_t monetary;
    time_def_t     time;
    messages_def_t messages;
};

#endif   // RWSTD_UTIL_LOCALEDEF_MODEL_H_INCLUDED
