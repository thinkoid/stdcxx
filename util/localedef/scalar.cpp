/***************************************************************************
 *
 * scalar.cpp
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

// The semantics are those of glibc's ld-numeric.c, ld-monetary.c,
// ld-time.c and ld-messages.c, up to their *_finish() functions: those
// supply the defaults, and that is the back end's to do.  A statement
// that fails is reported and skipped whole, so a field is either as
// the source sets it or unset.  glibc sets a string field it cannot
// resolve to "" and pads a short list with empty strings; either way
// it has counted an error and writes no locale, so nothing depends on
// what the field holds.

#include "scalar.h"

#include <climits>   // for INT_MAX
#include <cstdlib>   // for strtoul()


/**************************************************************************/
// strings

// What a string resolves against: the charmap, for a symbolic name,
// and the escape character in effect on the string's line.
struct charset_t {
    const cmap_t *cmap;
    char          escape;
};


// The code point a symbolic name gives, as glibc's get_string() takes
// it: U and 4 or 8 hexadecimal digits is that code point whether or
// not the charmap has the name; any other name is the charmap's.
static bool
name_value (const std::string &name, const charset_t &cs,
            const where_t &where, long &ucs, diag_t &diag)
{
    const bool u_form =
           (5 == name.size () || 9 == name.size ()) && 'U' == name [0]
        && name.find_first_not_of ("0123456789ABCDEFabcdef", 1)
           == std::string::npos;

    if (u_form) {

        const unsigned long value = std::strtoul (name.c_str () + 1, 0, 16);

        // the reader's limit for a <Uxxxx> symbol, and the charmap's
        if (value < 0x80000000UL) {
            ucs = long (value);
            return true;
        }
    }

    const cmap_entry_t* const entry = cs.cmap->find_name (name);

    if (0 == entry)
        return diag.error (e_ucs, where, "symbol `%s' not in charmap",
                           name.c_str ());

    if (entry->ucs < 0)
        return diag.error (e_ucs, where, "symbol `%s' has no code point",
                           name.c_str ());

    ucs = entry->ucs;

    return true;
}


// The code points of a string token, by glibc's get_string() for a
// locale file.  The escape character takes the next character as
// itself: "//" is one slash under escape_char /.  A string has no
// charcode form, so "\x41" is the three characters x41, as in glibc.
// A <name> is the code point name_value() gives.  A byte from 0x80
// starts a character written raw in UTF-8, which reads as its <Uxxxx>
// name.
static bool
resolve (const token_t &tok, const charset_t &cs, ucs_string_t &ucs,
         diag_t &diag)
{
    const std::string &s = tok.text;

    ucs.clear ();

    for (std::size_t pos = 0; pos != s.size (); ) {

        const unsigned char c = s [pos];

        if (cs.escape == char (c)) {

            // the reader keeps an escape only with the character after
            // it; an escape that ends the text ends the string
            if (pos + 1 == s.size ())
                break;

            const unsigned char next = s [pos + 1];

            if (0x80 & next)
                return diag.error (e_syntax, tok.where,
                                   "illegal 8-bit escape sequence");

            ucs.push_back (next);
            pos += 2;
            continue;
        }

        if ('<' == c) {

            std::string name;
            std::size_t end = pos + 1;

            for (; end < s.size () && '>' != s [end]; ++end) {

                if (cs.escape == s [end] && end + 1 < s.size ())
                    ++end;

                name += s [end];
            }

            if (end == s.size ())
                return diag.error (e_symend, tok.where,
                                   "unterminated symbolic name in string");

            long value;

            if (!name_value (name, cs, tok.where, value, diag))
                return false;

            ucs.push_back (value);
            pos = end + 1;
            continue;
        }

        if (0x80 & c) {

            long              value;
            const std::size_t len = scan_utf8 (s, pos, value);

            if (0 == len)
                return diag.error (e_mbchar, tok.where,
                                   "invalid UTF-8 sequence in string");

            ucs.push_back (value);
            pos += len;
            continue;
        }

        ucs.push_back (c);
        ++pos;
    }

    return true;
}


/**************************************************************************/
// eras

// glibc's strsep (&str, ":"): the position past the next colon, or
// npos when there is none, glibc's null pointer.
static std::size_t
past_colon (const ucs_string_t &s, std::size_t pos)
{
    if (pos == std::string::npos)
        return pos;

    for (; pos < s.size (); ++pos)
        if (':' == s [pos])
            return pos + 1;

    return std::string::npos;
}


// glibc's strtol() in base 10 over code points: blanks, a sign, the
// digits.  The position past the number, or pos when there is none.
// A value beyond the range of int is held at its bound.
static std::size_t
number_at (const ucs_string_t &s, std::size_t pos, int &value)
{
    if (pos == std::string::npos)
        return pos;

    std::size_t i = pos;

    while (i < s.size () && (' ' == s [i] || ('\t' <= s [i] && s [i] <= '\r')))
        ++i;

    bool negative = false;

    if (i < s.size () && ('+' == s [i] || '-' == s [i]))
        negative = '-' == s [i++];

    const std::size_t digits = i;

    long number = 0;

    for (; i < s.size () && '0' <= s [i] && s [i] <= '9'; ++i)
        if (number <= INT_MAX)
            number = number * 10 + (s [i] - '0');

    if (i == digits)
        return pos;

    if (INT_MAX < number)
        number = INT_MAX;

    value = int (negative ? -number : number);

    return i;
}


static bool
is_leap (long year)
{
    return 0 == year % 4 && (0 != year % 100 || 0 == year % 400);
}


// One date of an era and the colon after it: -*, +*, or year/month/day
// stored as glibc stores it.  False when it does not parse; the date is
// then all zero and pos is past the next colon, glibc's recovery.
static bool
era_date (const ucs_string_t &s, std::size_t &pos, int date [3])
{
    const std::size_t start = pos;

    date [0] = date [1] = date [2] = 0;

    if (   pos != std::string::npos && pos + 1 < s.size ()
        && ('-' == s [pos] || '+' == s [pos]) && '*' == s [pos + 1]) {

        const int bound =
            '-' == s [pos] ? era_minus_infinity : era_plus_infinity;

        if (pos + 2 < s.size () && ':' == s [pos + 2]) {
            date [0] = date [1] = date [2] = bound;
            pos += 3;
            return true;
        }

        pos = past_colon (s, start);
        return false;
    }

    std::size_t end = number_at (s, pos, date [0]);

    bool parsed = end != pos && end < s.size () && '/' == s [end];

    if (parsed) {
        pos = end + 1;
        end = number_at (s, pos, date [1]);
        parsed = end != pos && end < s.size () && '/' == s [end];
    }

    if (parsed) {
        pos = end + 1;
        end = number_at (s, pos, date [2]);
        parsed = end != pos && end < s.size () && ':' == s [end];
    }

    if (!parsed) {
        date [0] = date [1] = date [2] = 0;
        pos = past_colon (s, start);
        return false;
    }

    pos = end + 1;

    // year -1 is 1 BC, which is the year 0 of the count without a gap
    date [0] -= 1900;

    if (date [0] < -1900)
        ++date [0];

    date [1] -= 1;

    return true;
}


// glibc's check of a parsed date, with one difference: glibc tests
// February 29 on the month numbered 2, which is March, and on its
// stored year less 1900.  Here the test is on February and on the
// calendar year.
static bool
is_valid_date (const int date [3])
{
    static const int days [12] = {
        31, 29, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31
    };

    if (era_minus_infinity == date [0] || era_plus_infinity == date [0])
        return true;

    if (date [1] < 0 || 12 <= date [1] || date [2] < 0)
        return false;

    if (days [date [1]] < date [2])
        return false;

    return !(1 == date [1] && 29 == date [2] && !is_leap (date [0] + 1900L));
}


// The fields of an era string, direction:offset:start:end:name:format,
// as glibc's time_finish() reads them.  Each fault is reported and
// recovered from as glibc does, by moving past the next colon; every
// field of the record has a value, a date that does not parse is all
// zero, glibc's partial strtol() results are not kept.  index counts
// the eras from 1, in glibc's messages.
static void
parse_era (era_t &era, const where_t &where, int index, diag_t &diag)
{
    const ucs_string_t &s = era.text;

    era.direction = '+';
    era.offset    = 0;
    era.start [0] = era.start [1] = era.start [2] = 0;
    era.end [0]   = era.end [1]   = era.end [2]   = 0;
    era.name.clear ();
    era.format.clear ();

    if (!s.empty () && ('+' == s [0] || '-' == s [0]))
        era.direction = char (s [0]);
    else
        diag.error (e_inval, where, "direction flag in string %d in `era' "
                    "field is not '+' nor '-'", index);

    std::size_t pos = s.empty () ? 0 : 1;

    if (pos < s.size () && ':' == s [pos])
        ++pos;
    else {
        diag.error (e_inval, where, "direction flag in string %d in `era' "
                    "field is not a single character", index);
        pos = past_colon (s, pos);
    }

    int offset = 0;

    const std::size_t end = number_at (s, pos, offset);

    if (end == pos) {
        diag.error (e_inval, where, "invalid number for offset in string %d "
                    "in `era' field", index);
        pos = past_colon (s, pos);
    }
    else if (end == s.size () || ':' != s [end]) {
        era.offset = offset;
        diag.error (e_inval, where, "garbage at end of offset value in "
                    "string %d in `era' field", index);
        pos = past_colon (s, pos);
    }
    else {
        era.offset = offset;
        pos        = end + 1;
    }

    if (!era_date (s, pos, era.start))
        diag.error (e_inval, where, "invalid starting date in string %d in "
                    "`era' field", index);
    else if (!is_valid_date (era.start))
        diag.error (e_inval, where, "starting date is invalid in string %d "
                    "in `era' field", index);

    if (!era_date (s, pos, era.end))
        diag.error (e_inval, where, "invalid stopping date in string %d in "
                    "`era' field", index);
    else if (!is_valid_date (era.end))
        diag.error (e_inval, where, "invalid stopping date in string %d in "
                    "`era' field", index);

    if (pos == std::string::npos || pos == s.size ()) {
        diag.error (e_inval, where, "missing era name in string %d in "
                    "`era' field", index);
        return;
    }

    const std::size_t colon = past_colon (s, pos);

    if (colon == std::string::npos || colon == s.size ()) {
        diag.error (e_inval, where, "missing era format in string %d in "
                    "`era' field", index);
        return;
    }

    era.name.assign (s.begin () + pos, s.begin () + (colon - 1));
    era.format.assign (s.begin () + colon, s.end ());
}


/**************************************************************************/
// the productions

// A production starts at the keyword that leads its statement and reads
// the line to its end.  It assigns the attribute only when the whole
// statement is well-formed.  A production that reads a string takes the
// charset its strings resolve against; that is the one addition to the
// uniform signature.

// "string", an operand
static bool
string (tok_iter &it, tok_iter last, const charset_t &cs, ucs_string_t &attr,
        diag_t &diag)
{
    if (it == last || tk_string != it->kind)
        return diag.error (e_syntax, where_of (it, last), "expected a string");

    return resolve (*it++, cs, attr, diag);
}


static bool
declared_twice (const token_t &lead, diag_t &diag)
{
    return diag.error (e_syntax, lead.where, "field `%s' declared more "
                       "than once", lead.text.c_str ());
}


// keyword "string".  glibc keeps the first of two.
static bool
string_value (tok_iter &it, tok_iter last, const charset_t &cs,
              field_t<ucs_string_t> &attr, diag_t &diag)
{
    const token_t &lead = *it++;

    ucs_string_t value;

    if (!string (it, last, cs, value, diag) || !end_of_line (it, last, diag))
        return false;

    if (attr.set)
        return declared_twice (lead, diag);

    attr.value = value;
    attr.set   = true;

    return true;
}


// keyword "c": a string of one character or none, for the separators
// glibc also stores as a wide character.
static bool
char_value (tok_iter &it, tok_iter last, const charset_t &cs,
            field_t<ucs_string_t> &attr, diag_t &diag)
{
    const token_t &lead = *it;

    field_t<ucs_string_t> value;

    if (!string_value (it, last, cs, value, diag))
        return false;

    if (1 < value.value.size ())
        return diag.error (e_inval, lead.where, "value for field `%s' must "
                           "be a single character", lead.text.c_str ());

    if (attr.set)
        return declared_twice (lead, diag);

    attr = value;

    return true;
}


// keyword n, where n is a number or -1.  glibc keeps the first of two.
static bool
number_value (tok_iter &it, tok_iter last, field_t<int> &attr, diag_t &diag)
{
    const token_t &lead = *it++;

    if (it == last || tk_number != it->kind)
        return diag.error (e_syntax, where_of (it, last),
                           "%s: expected a number", lead.text.c_str ());

    if (INT_MAX < it->value)
        return diag.error (e_inval, it->where, "%s: value out of range",
                           lead.text.c_str ());

    const int value = int (it++->value);

    if (!end_of_line (it, last, diag))
        return false;

    if (attr.set)
        return declared_twice (lead, diag);

    attr.value = value;
    attr.set   = true;

    return true;
}


// keyword n;n;...;-1, as written: 0 and -1 are kept, and a single -1,
// which glibc stores as no grouping at all, is the list {-1}.  A
// semicolon may end the list, as in glibc.  A later statement replaces
// an earlier one.
static bool
grouping (tok_iter &it, tok_iter last, field_t<std::vector<int> > &attr,
          diag_t &diag)
{
    const token_t &lead = *it++;

    std::vector<int> value;

    do {
        if (it == last || tk_number != it->kind)
            return diag.error (e_syntax, where_of (it, last),
                               "%s: expected a number", lead.text.c_str ());

        if (!value.empty () && -1 == value.back ())
            return diag.error (e_syntax, it->where, "`-1' must be last "
                               "entry in `%s' field", lead.text.c_str ());

        if (126 < it->value)
            return diag.error (e_inval, it->where, "values for field `%s' "
                               "must be smaller than 127",
                               lead.text.c_str ());

        value.push_back (int (it++->value));

        if (it == last || tk_semicolon != it->kind)
            break;

    } while (++it != last);

    if (!end_of_line (it, last, diag))
        return false;

    attr.value = value;
    attr.set   = true;

    return true;
}


// keyword "s";"s";... with min to max strings.  A later statement
// replaces an earlier one, as in glibc.
static bool
string_list (tok_iter &it, tok_iter last, const charset_t &cs,
             std::size_t min, std::size_t max,
             field_t<std::vector<ucs_string_t> > &attr, diag_t &diag)
{
    const token_t &lead = *it++;

    std::vector<ucs_string_t> value;

    while (it != last && value.size () < max) {

        ucs_string_t str;

        if (!string (it, last, cs, str, diag))
            return false;

        value.push_back (str);

        if (it == last)
            break;

        if (tk_semicolon != it->kind)
            return end_of_line (it, last, diag);

        if (++it == last)
            return diag.error (e_syntax, (it - 1)->where,
                               "extra trailing semicolon");
    }

    if (it != last)
        return diag.error (e_syntax, it->where, "too many values for "
                           "field `%s'", lead.text.c_str ());

    if (value.size () < min)
        return diag.error (e_syntax, lead.where, "too few values for "
                           "field `%s'", lead.text.c_str ());

    attr.value = value;
    attr.set   = true;

    return true;
}


// era "s";"s";...: one era per string, after those of earlier era
// lines, as glibc appends them.
static bool
era (tok_iter &it, tok_iter last, const charset_t &cs,
     std::vector<era_t> &attr, diag_t &diag)
{
    ++it;

    std::vector<era_t> value;

    for (;;) {

        const where_t where = where_of (it, last);

        era_t record;

        if (!string (it, last, cs, record.text, diag))
            return false;

        parse_era (record, where, int (attr.size () + value.size () + 1),
                   diag);

        value.push_back (record);

        if (it == last)
            break;

        if (tk_semicolon != it->kind)
            return end_of_line (it, last, diag);

        ++it;
    }

    attr.insert (attr.end (), value.begin (), value.end ());

    return true;
}


// copy "name".  The name is a file name: glibc takes its bytes and
// asks for the portable characters, here code points below 0x80.
static bool
copy (tok_iter &it, tok_iter last, const charset_t &cs, std::string &attr,
      diag_t &diag)
{
    const token_t &lead = *it++;

    if (it == last || tk_string != it->kind)
        return diag.error (e_syntax, where_of (it, last),
                           "expecting string argument for `copy'");

    ucs_string_t name;

    if (!string (it, last, cs, name, diag) || !end_of_line (it, last, diag))
        return false;

    std::string value;

    for (std::size_t i = 0; i != name.size (); ++i) {

        if (name [i] <= 0 || 0x7f < name [i])
            return diag.error (e_inval, lead.where, "locale name should "
                               "consist only of portable characters");

        value += char (name [i]);
    }

    if (value.empty ())
        return diag.error (e_inval, lead.where, "empty locale name");

    attr = value;

    return true;
}


bool
end_of_section (tok_iter &it, tok_iter last, const token_t &head,
                diag_t &diag)
{
    ++it;

    if (it == last)
        return diag.error (e_syntax, (it - 1)->where,
                           "%s: incomplete `END' line", head.text.c_str ());

    if (tk_keyword != it->kind || head.value != it->value)
        return diag.error (e_syntax, it->where, "%s: definition does not "
                           "end with `END %s'", head.text.c_str (),
                           head.text.c_str ());

    return end_of_line (++it, last, diag);
}


// A statement its category does not read.  A keyword of another
// category or a word no table knows draws a warning; any other token
// is a syntax error.  Either way the statement is skipped.  glibc
// reports both as syntax errors.
static bool
not_read (const token_t &lead, const char *category, diag_t &diag)
{
    if (tk_keyword == lead.kind || tk_ident == lead.kind)
        return diag.warning (w_syntax, lead.where, "%s: `%s' is not read "
                             "in this category; statement skipped",
                             category, lead.text.c_str ());

    return diag.error (e_syntax, lead.where, "%s: syntax error", category);
}


/**************************************************************************/
// the categories

// One switch per category over the leading keyword, one call per case.
// The keywords plan-localedef.md reads and ignores are matched and
// their statements skipped unread.

static void
statement (tok_iter it, tok_iter last, const charset_t &cs,
           numeric_def_t &def, diag_t &diag)
{
    switch (keyword_of (*it)) {

    case kw_decimal_point:
        char_value (it, last, cs, def.decimal_point, diag);
        break;

    case kw_thousands_sep:
        char_value (it, last, cs, def.thousands_sep, diag);
        break;

    case kw_grouping:
        grouping (it, last, def.grouping, diag);
        break;

    default:
        not_read (*it, "LC_NUMERIC", diag);
    }
}


static void
statement (tok_iter it, tok_iter last, const charset_t &cs,
           monetary_def_t &def, diag_t &diag)
{
    switch (keyword_of (*it)) {

    case kw_int_curr_symbol:
        string_value (it, last, cs, def.int_curr_symbol, diag);
        break;

    case kw_currency_symbol:
        string_value (it, last, cs, def.currency_symbol, diag);
        break;

    case kw_mon_decimal_point:
        char_value (it, last, cs, def.mon_decimal_point, diag);
        break;

    case kw_mon_thousands_sep:
        char_value (it, last, cs, def.mon_thousands_sep, diag);
        break;

    case kw_mon_grouping:
        grouping (it, last, def.mon_grouping, diag);
        break;

    case kw_positive_sign:
        string_value (it, last, cs, def.positive_sign, diag);
        break;

    case kw_negative_sign:
        string_value (it, last, cs, def.negative_sign, diag);
        break;

    case kw_int_frac_digits:
        number_value (it, last, def.int_frac_digits, diag);
        break;

    case kw_frac_digits:
        number_value (it, last, def.frac_digits, diag);
        break;

    case kw_p_cs_precedes:
        number_value (it, last, def.p_cs_precedes, diag);
        break;

    case kw_p_sep_by_space:
        number_value (it, last, def.p_sep_by_space, diag);
        break;

    case kw_n_cs_precedes:
        number_value (it, last, def.n_cs_precedes, diag);
        break;

    case kw_n_sep_by_space:
        number_value (it, last, def.n_sep_by_space, diag);
        break;

    case kw_p_sign_posn:
        number_value (it, last, def.p_sign_posn, diag);
        break;

    case kw_n_sign_posn:
        number_value (it, last, def.n_sign_posn, diag);
        break;

    case kw_int_p_cs_precedes:
        number_value (it, last, def.int_p_cs_precedes, diag);
        break;

    case kw_int_p_sep_by_space:
        number_value (it, last, def.int_p_sep_by_space, diag);
        break;

    case kw_int_n_cs_precedes:
        number_value (it, last, def.int_n_cs_precedes, diag);
        break;

    case kw_int_n_sep_by_space:
        number_value (it, last, def.int_n_sep_by_space, diag);
        break;

    case kw_int_p_sign_posn:
        number_value (it, last, def.int_p_sign_posn, diag);
        break;

    case kw_int_n_sign_posn:
        number_value (it, last, def.int_n_sign_posn, diag);
        break;

    case kw_duo_int_curr_symbol: case kw_duo_currency_symbol:
    case kw_duo_int_frac_digits: case kw_duo_frac_digits:
    case kw_duo_p_cs_precedes: case kw_duo_p_sep_by_space:
    case kw_duo_n_cs_precedes: case kw_duo_n_sep_by_space:
    case kw_duo_int_p_cs_precedes: case kw_duo_int_p_sep_by_space:
    case kw_duo_int_n_cs_precedes: case kw_duo_int_n_sep_by_space:
    case kw_duo_p_sign_posn: case kw_duo_n_sign_posn:
    case kw_duo_int_p_sign_posn: case kw_duo_int_n_sign_posn:
    case kw_uno_valid_from: case kw_uno_valid_to:
    case kw_duo_valid_from: case kw_duo_valid_to:
    case kw_conversion_rate:
        // the second currency: no facet reads it
        break;

    default:
        not_read (*it, "LC_MONETARY", diag);
    }
}


static void
statement (tok_iter it, tok_iter last, const charset_t &cs,
           time_def_t &def, diag_t &diag)
{
    switch (keyword_of (*it)) {

    case kw_abday:
        string_list (it, last, cs, 7, 7, def.abday, diag);
        break;

    case kw_day:
        string_list (it, last, cs, 7, 7, def.day, diag);
        break;

    case kw_abmon:
        string_list (it, last, cs, 12, 12, def.abmon, diag);
        break;

    case kw_mon:
        string_list (it, last, cs, 12, 12, def.mon, diag);
        break;

    case kw_am_pm:
        string_list (it, last, cs, 2, 2, def.am_pm, diag);
        break;

    case kw_d_t_fmt:
        string_value (it, last, cs, def.d_t_fmt, diag);
        break;

    case kw_d_fmt:
        string_value (it, last, cs, def.d_fmt, diag);
        break;

    case kw_t_fmt:
        string_value (it, last, cs, def.t_fmt, diag);
        break;

    case kw_t_fmt_ampm:
        string_value (it, last, cs, def.t_fmt_ampm, diag);
        break;

    case kw_era:
        era (it, last, cs, def.era, diag);
        break;

    case kw_era_d_fmt:
        string_value (it, last, cs, def.era_d_fmt, diag);
        break;

    case kw_era_d_t_fmt:
        string_value (it, last, cs, def.era_d_t_fmt, diag);
        break;

    case kw_era_t_fmt:
        string_value (it, last, cs, def.era_t_fmt, diag);
        break;

    case kw_alt_digits:
        string_list (it, last, cs, 0, 100, def.alt_digits, diag);
        break;

    case kw_alt_mon: case kw_ab_alt_mon: case kw_date_fmt: case kw_week:
    case kw_first_weekday: case kw_first_workday: case kw_cal_direction:
    case kw_timezone: case kw_era_year:
        // no facet reads them
        break;

    default:
        not_read (*it, "LC_TIME", diag);
    }
}


static void
statement (tok_iter it, tok_iter last, const charset_t &cs,
           messages_def_t &def, diag_t &diag)
{
    switch (keyword_of (*it)) {

    case kw_yesexpr:
        string_value (it, last, cs, def.yesexpr, diag);
        break;

    case kw_noexpr:
        string_value (it, last, cs, def.noexpr, diag);
        break;

    case kw_yesstr: case kw_nostr:
        // no facet reads them
        break;

    default:
        not_read (*it, "LC_MESSAGES", diag);
    }
}


// The loop of every category: lines up to END.  A copy statement must
// be the first, and then the only one, as glibc's handle_copy() has
// it; the category is left to the caller to read from the named
// locale.
template <class Def>
static void
section (reader_t &reader, const line_t &head, const cmap_t &cmap, Def &def,
         std::string &copy_name, diag_t &diag)
{
    const token_t     &name  = head.tokens.front ();
    const std::size_t  depth = reader.depth ();

    where_t at_end = name.where;
    bool    first  = true;
    bool    copied = false;
    bool    warned = false;

    copy_name.clear ();

    for (line_t line; reader.next (line, depth); first = false) {

        tok_iter        it   = line.tokens.begin ();
        const tok_iter  last = line.tokens.end ();
        const charset_t cs   = { &cmap, line.escape };

        const keyword_t keyword = keyword_of (*it);

        at_end = it->where;

        if (kw_end == keyword) {
            end_of_section (it, last, name, diag);
            return;
        }

        if (kw_copy == keyword && first) {
            copied = true;
            copy (it, last, cs, copy_name, diag);
            continue;
        }

        if (copied) {
            if (!warned)
                diag.error (e_syntax, it->where, "no other keyword shall be "
                            "specified when `copy' is used");
            warned = true;
            continue;
        }

        if (kw_copy == keyword) {
            diag.error (e_syntax, it->where, "%s: `copy' must be the only "
                        "statement of a category", name.text.c_str ());
            continue;
        }

        statement (it, last, cs, def, diag);
    }

    diag.error (e_syntax, at_end, "%s: premature end of file",
                name.text.c_str ());
}


void
read_numeric (reader_t &reader, const line_t &head, const cmap_t &cmap,
              numeric_def_t &def, std::string &copy, diag_t &diag)
{
    section (reader, head, cmap, def, copy, diag);
}


void
read_monetary (reader_t &reader, const line_t &head, const cmap_t &cmap,
               monetary_def_t &def, std::string &copy, diag_t &diag)
{
    section (reader, head, cmap, def, copy, diag);
}


void
read_time (reader_t &reader, const line_t &head, const cmap_t &cmap,
           time_def_t &def, std::string &copy, diag_t &diag)
{
    section (reader, head, cmap, def, copy, diag);
}


void
read_messages (reader_t &reader, const line_t &head, const cmap_t &cmap,
               messages_def_t &def, std::string &copy, diag_t &diag)
{
    section (reader, head, cmap, def, copy, diag);
}
