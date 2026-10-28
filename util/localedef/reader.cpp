/***************************************************************************
 *
 * reader.cpp
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

#include "reader.h"

#include <cerrno>    // for errno
#include <climits>   // for LONG_MAX
#include <cstdlib>   // for strtoul()
#include <cstring>   // for strcmp(), strlen()


/**************************************************************************/
// the keyword table

// glibc keeps two tables, one for charmaps and one for locale files,
// and marks each word: a word marked 1 is a keyword only between the
// brackets (<code_set_name>), a word marked 0 only bare (END).  Here
// the two tables are one, and an entry has a bit for each form in each
// kind of file, so a word is a keyword exactly where glibc's tables
// make it one.  A locale file has no bracketed keyword: <space> is a
// symbol, space a keyword.  As in glibc, a word is a keyword wherever
// it is lexed, not only at the start of a line; a statement decides
// what its operands mean.

enum {
    cm_bracket = 1,   // between brackets in a charmap
    cm_bare    = 2,   // bare in a charmap
    lf_bare    = 4    // bare in a locale file
};

struct keyword_entry_t {
    const char *name;
    keyword_t   keyword;
    int         forms;
};

// sorted by strcmp() for the binary search
static const keyword_entry_t keywords [] = {
    { "CHARMAP",                 kw_charmap,                  cm_bare },
    { "END",                     kw_end,
                                 cm_bare | lf_bare },
    { "IGNORE",                  kw_ignore,                   lf_bare },
    { "LC_ADDRESS",              kw_lc_address,               lf_bare },
    { "LC_COLLATE",              kw_lc_collate,               lf_bare },
    { "LC_CTYPE",                kw_lc_ctype,                 lf_bare },
    { "LC_IDENTIFICATION",       kw_lc_identification,        lf_bare },
    { "LC_MEASUREMENT",          kw_lc_measurement,           lf_bare },
    { "LC_MESSAGES",             kw_lc_messages,              lf_bare },
    { "LC_MONETARY",             kw_lc_monetary,              lf_bare },
    { "LC_NAME",                 kw_lc_name,                  lf_bare },
    { "LC_NUMERIC",              kw_lc_numeric,               lf_bare },
    { "LC_PAPER",                kw_lc_paper,                 lf_bare },
    { "LC_TELEPHONE",            kw_lc_telephone,             lf_bare },
    { "LC_TIME",                 kw_lc_time,                  lf_bare },
    { "UNDEFINED",               kw_undefined,                lf_bare },
    { "WIDTH",                   kw_width_section,            cm_bare },
    { "WIDTH_DEFAULT",           kw_width_default,            cm_bare },
    { "WIDTH_VARIABLE",          kw_width_variable,           cm_bare },
    { "ab_alt_mon",              kw_ab_alt_mon,               lf_bare },
    { "abbreviation",            kw_abbreviation,             lf_bare },
    { "abday",                   kw_abday,                    lf_bare },
    { "abmon",                   kw_abmon,                    lf_bare },
    { "address",                 kw_address,                  lf_bare },
    { "addset",                  kw_addset,                   cm_bracket },
    { "alnum",                   kw_alnum,                    lf_bare },
    { "alpha",                   kw_alpha,                    lf_bare },
    { "alt_digits",              kw_alt_digits,               lf_bare },
    { "alt_mon",                 kw_alt_mon,                  lf_bare },
    { "am_pm",                   kw_am_pm,                    lf_bare },
    { "application",             kw_application,              lf_bare },
    { "audience",                kw_audience,                 lf_bare },
    { "backward",                kw_backward,                 lf_bare },
    { "blank",                   kw_blank,                    lf_bare },
    { "cal_direction",           kw_cal_direction,            lf_bare },
    { "category",                kw_category,                 lf_bare },
    { "charclass",               kw_charclass,                lf_bare },
    { "charconv",                kw_charconv,                 lf_bare },
    { "class",                   kw_class,                    lf_bare },
    { "cntrl",                   kw_cntrl,                    lf_bare },
    { "code_set_name",           kw_code_set_name,            cm_bracket },
    { "codepoint_collation",     kw_codepoint_collation,      lf_bare },
    { "coll_weight_max",         kw_coll_weight_max,          lf_bare },
    { "collating-element",       kw_collating_element,        lf_bare },
    { "collating-symbol",        kw_collating_symbol,         lf_bare },
    { "comment_char",            kw_comment_char,
                                 cm_bracket | lf_bare },
    { "contact",                 kw_contact,                  lf_bare },
    { "conversion_rate",         kw_conversion_rate,          lf_bare },
    { "copy",                    kw_copy,                     lf_bare },
    { "country_ab2",             kw_country_ab2,              lf_bare },
    { "country_ab3",             kw_country_ab3,              lf_bare },
    { "country_car",             kw_country_car,              lf_bare },
    { "country_isbn",            kw_country_isbn,             lf_bare },
    { "country_name",            kw_country_name,             lf_bare },
    { "country_num",             kw_country_num,              lf_bare },
    { "country_post",            kw_country_post,             lf_bare },
    { "currency_symbol",         kw_currency_symbol,          lf_bare },
    { "d_fmt",                   kw_d_fmt,                    lf_bare },
    { "d_t_fmt",                 kw_d_t_fmt,                  lf_bare },
    { "date",                    kw_date,                     lf_bare },
    { "date_fmt",                kw_date_fmt,                 lf_bare },
    { "day",                     kw_day,                      lf_bare },
    { "decimal_point",           kw_decimal_point,            lf_bare },
    { "default_missing",         kw_default_missing,          lf_bare },
    { "define",                  kw_define,                   lf_bare },
    { "digit",                   kw_digit,                    lf_bare },
    { "duo_currency_symbol",     kw_duo_currency_symbol,      lf_bare },
    { "duo_frac_digits",         kw_duo_frac_digits,          lf_bare },
    { "duo_int_curr_symbol",     kw_duo_int_curr_symbol,      lf_bare },
    { "duo_int_frac_digits",     kw_duo_int_frac_digits,      lf_bare },
    { "duo_int_n_cs_precedes",   kw_duo_int_n_cs_precedes,    lf_bare },
    { "duo_int_n_sep_by_space",  kw_duo_int_n_sep_by_space,   lf_bare },
    { "duo_int_n_sign_posn",     kw_duo_int_n_sign_posn,      lf_bare },
    { "duo_int_p_cs_precedes",   kw_duo_int_p_cs_precedes,    lf_bare },
    { "duo_int_p_sep_by_space",  kw_duo_int_p_sep_by_space,   lf_bare },
    { "duo_int_p_sign_posn",     kw_duo_int_p_sign_posn,      lf_bare },
    { "duo_n_cs_precedes",       kw_duo_n_cs_precedes,        lf_bare },
    { "duo_n_sep_by_space",      kw_duo_n_sep_by_space,       lf_bare },
    { "duo_n_sign_posn",         kw_duo_n_sign_posn,          lf_bare },
    { "duo_p_cs_precedes",       kw_duo_p_cs_precedes,        lf_bare },
    { "duo_p_sep_by_space",      kw_duo_p_sep_by_space,       lf_bare },
    { "duo_p_sign_posn",         kw_duo_p_sign_posn,          lf_bare },
    { "duo_valid_from",          kw_duo_valid_from,           lf_bare },
    { "duo_valid_to",            kw_duo_valid_to,             lf_bare },
    { "elifdef",                 kw_elifdef,                  lf_bare },
    { "elifndef",                kw_elifndef,                 lf_bare },
    { "else",                    kw_else,                     lf_bare },
    { "email",                   kw_email,                    lf_bare },
    { "endif",                   kw_endif,                    lf_bare },
    { "era",                     kw_era,                      lf_bare },
    { "era_d_fmt",               kw_era_d_fmt,                lf_bare },
    { "era_d_t_fmt",             kw_era_d_t_fmt,              lf_bare },
    { "era_t_fmt",               kw_era_t_fmt,                lf_bare },
    { "era_year",                kw_era_year,                 lf_bare },
    { "escape_char",             kw_escape_char,
                                 cm_bracket | lf_bare },
    { "escseq",                  kw_escseq,                   cm_bracket },
    { "fax",                     kw_fax,                      lf_bare },
    { "first_weekday",           kw_first_weekday,            lf_bare },
    { "first_workday",           kw_first_workday,            lf_bare },
    { "forward",                 kw_forward,                  lf_bare },
    { "frac_digits",             kw_frac_digits,              lf_bare },
    { "from",                    kw_from,                     lf_bare },
    { "g0esc",                   kw_g0esc,                    cm_bracket },
    { "g1esc",                   kw_g1esc,                    cm_bracket },
    { "g2esc",                   kw_g2esc,                    cm_bracket },
    { "g3esc",                   kw_g3esc,                    cm_bracket },
    { "graph",                   kw_graph,                    lf_bare },
    { "grouping",                kw_grouping,                 lf_bare },
    { "height",                  kw_height,                   lf_bare },
    { "ifdef",                   kw_ifdef,                    lf_bare },
    { "include",                 kw_include,
                                 cm_bracket | lf_bare },
    { "int_curr_symbol",         kw_int_curr_symbol,          lf_bare },
    { "int_frac_digits",         kw_int_frac_digits,          lf_bare },
    { "int_n_cs_precedes",       kw_int_n_cs_precedes,        lf_bare },
    { "int_n_sep_by_space",      kw_int_n_sep_by_space,       lf_bare },
    { "int_n_sign_posn",         kw_int_n_sign_posn,          lf_bare },
    { "int_p_cs_precedes",       kw_int_p_cs_precedes,        lf_bare },
    { "int_p_sep_by_space",      kw_int_p_sep_by_space,       lf_bare },
    { "int_p_sign_posn",         kw_int_p_sign_posn,          lf_bare },
    { "int_prefix",              kw_int_prefix,               lf_bare },
    { "int_select",              kw_int_select,               lf_bare },
    { "lang_ab",                 kw_lang_ab,                  lf_bare },
    { "lang_lib",                kw_lang_lib,                 lf_bare },
    { "lang_name",               kw_lang_name,                lf_bare },
    { "lang_term",               kw_lang_term,                lf_bare },
    { "language",                kw_language,                 lf_bare },
    { "lower",                   kw_lower,                    lf_bare },
    { "map",                     kw_map,                      lf_bare },
    { "mb_cur_max",              kw_mb_cur_max,               cm_bracket },
    { "mb_cur_min",              kw_mb_cur_min,               cm_bracket },
    { "measurement",             kw_measurement,              lf_bare },
    { "mon",                     kw_mon,                      lf_bare },
    { "mon_decimal_point",       kw_mon_decimal_point,        lf_bare },
    { "mon_grouping",            kw_mon_grouping,             lf_bare },
    { "mon_thousands_sep",       kw_mon_thousands_sep,        lf_bare },
    { "n_cs_precedes",           kw_n_cs_precedes,            lf_bare },
    { "n_sep_by_space",          kw_n_sep_by_space,           lf_bare },
    { "n_sign_posn",             kw_n_sign_posn,              lf_bare },
    { "name_fmt",                kw_name_fmt,                 lf_bare },
    { "name_gen",                kw_name_gen,                 lf_bare },
    { "name_miss",               kw_name_miss,                lf_bare },
    { "name_mr",                 kw_name_mr,                  lf_bare },
    { "name_mrs",                kw_name_mrs,                 lf_bare },
    { "name_ms",                 kw_name_ms,                  lf_bare },
    { "negative_sign",           kw_negative_sign,            lf_bare },
    { "noexpr",                  kw_noexpr,                   lf_bare },
    { "nostr",                   kw_nostr,                    lf_bare },
    { "order_end",               kw_order_end,                lf_bare },
    { "order_start",             kw_order_start,              lf_bare },
    { "outdigit",                kw_outdigit,                 lf_bare },
    { "p_cs_precedes",           kw_p_cs_precedes,            lf_bare },
    { "p_sep_by_space",          kw_p_sep_by_space,           lf_bare },
    { "p_sign_posn",             kw_p_sign_posn,              lf_bare },
    { "position",                kw_position,                 lf_bare },
    { "positive_sign",           kw_positive_sign,            lf_bare },
    { "postal_fmt",              kw_postal_fmt,               lf_bare },
    { "print",                   kw_print,                    lf_bare },
    { "punct",                   kw_punct,                    lf_bare },
    { "reorder-after",           kw_reorder_after,            lf_bare },
    { "reorder-end",             kw_reorder_end,              lf_bare },
    { "reorder-sections-after",  kw_reorder_sections_after,   lf_bare },
    { "reorder-sections-end",    kw_reorder_sections_end,     lf_bare },
    { "repertoiremap",           kw_repertoiremap,            lf_bare },
    { "revision",                kw_revision,                 lf_bare },
    { "script",                  kw_script,                   lf_bare },
    { "section-symbol",          kw_section_symbol,           lf_bare },
    { "source",                  kw_source,                   lf_bare },
    { "space",                   kw_space,                    lf_bare },
    { "symbol-equivalence",      kw_symbol_equivalence,       lf_bare },
    { "t_fmt",                   kw_t_fmt,                    lf_bare },
    { "t_fmt_ampm",              kw_t_fmt_ampm,               lf_bare },
    { "tel",                     kw_tel,                      lf_bare },
    { "tel_dom_fmt",             kw_tel_dom_fmt,              lf_bare },
    { "tel_int_fmt",             kw_tel_int_fmt,              lf_bare },
    { "territory",               kw_territory,                lf_bare },
    { "thousands_sep",           kw_thousands_sep,            lf_bare },
    { "timezone",                kw_timezone,                 lf_bare },
    { "title",                   kw_title,                    lf_bare },
    { "tolower",                 kw_tolower,                  lf_bare },
    { "toupper",                 kw_toupper,                  lf_bare },
    { "translit_end",            kw_translit_end,             lf_bare },
    { "translit_ignore",         kw_translit_ignore,          lf_bare },
    { "translit_start",          kw_translit_start,           lf_bare },
    { "undef",                   kw_undef,                    lf_bare },
    { "uno_valid_from",          kw_uno_valid_from,           lf_bare },
    { "uno_valid_to",            kw_uno_valid_to,             lf_bare },
    { "upper",                   kw_upper,                    lf_bare },
    { "week",                    kw_week,                     lf_bare },
    { "width",                   kw_width,                    lf_bare },
    { "xdigit",                  kw_xdigit,                   lf_bare },
    { "yesexpr",                 kw_yesexpr,                  lf_bare },
    { "yesstr",                  kw_yesstr,                   lf_bare }
};


static keyword_t
find_keyword (const std::string &word, int form)
{
    std::size_t low  = 0;
    std::size_t high = sizeof keywords / sizeof *keywords;

    while (low < high) {

        const std::size_t mid = (low + high) / 2;
        const int         cmp = std::strcmp (word.c_str (),
                                             keywords [mid].name);

        if (0 == cmp)
            return (keywords [mid].forms & form) ? keywords [mid].keyword
                                                 : kw_none;

        if (cmp < 0)
            high = mid;
        else
            low = mid + 1;
    }

    return kw_none;
}


/**************************************************************************/
// character classes

// glibc's isspace() and friends in the C locale: ASCII only, so that a
// byte of a UTF-8 sequence is never a separator.

static bool
is_space (char c)
{
    return    ' '  == c || '\t' == c || '\n' == c
           || '\v' == c || '\f' == c || '\r' == c;
}

static bool
is_digit (char c)
{
    return '0' <= c && c <= '9';
}

static bool
is_xdigit (char c)
{
    return is_digit (c) || ('a' <= c && c <= 'f') || ('A' <= c && c <= 'F');
}

static bool
is_alpha (char c)
{
    return ('a' <= c && c <= 'z') || ('A' <= c && c <= 'Z');
}

static int
digit_value (char c)
{
    if (is_digit (c))
        return c - '0';

    return (c | 0x20) - 'a' + 10;
}


/**************************************************************************/
// helpers shared with the productions

const where_t&
where_of (tok_iter it, tok_iter last)
{
    return it == last ? (it - 1)->where : it->where;
}


bool
end_of_line (tok_iter it, tok_iter last, diag_t &diag)
{
    if (it == last)
        return true;

    return diag.error (e_syntax, it->where, "trailing garbage at end of line");
}


keyword_t
keyword_of (const token_t &tok)
{
    return tk_keyword == tok.kind ? keyword_t (tok.value) : kw_none;
}


bool
is_ellipsis (token_kind_t kind)
{
    return    tk_ellipsis2 == kind || tk_ellipsis3 == kind
           || tk_ellipsis4 == kind || tk_ellipsis2_2 == kind
           || tk_ellipsis4_2 == kind;
}


const char*
ellipsis_text (token_kind_t kind)
{
    switch (kind) {
    case tk_ellipsis2:   return "..";
    case tk_ellipsis3:   return "...";
    case tk_ellipsis4:   return "....";
    case tk_ellipsis2_2: return "..(2)..";
    case tk_ellipsis4_2: return "....(2)....";
    default:             return "";
    }
}


// glibc's ld-ctype.c reports names that differ only in the case of a
// hexadecimal digit, which give the same number, as no range; here,
// as in ld-collate.c, they are a range of one.
bool
name_range (tok_iter &it, tok_iter last, name_range_t &attr, diag_t &diag)
{
    const token_t &from = *it++;

    if (it == last || !is_ellipsis (it->kind))
        return diag.error (e_syntax, where_of (it, last),
                           "expected an ellipsis");

    const token_kind_t ellipsis = it++->kind;

    if (it == last || tk_symbol != it->kind)
        return diag.error (e_syntax, where_of (it, last),
                           "expected a symbolic name to end the range");

    const token_t &to = *it++;

    name_range_t value;

    value.ellipsis = ellipsis;
    value.ucs      = 0 <= from.value && 0 <= to.value;
    value.base     =    tk_ellipsis4 == ellipsis
                     || tk_ellipsis4_2 == ellipsis ? 10 : 16;
    value.step     =    tk_ellipsis2_2 == ellipsis
                     || tk_ellipsis4_2 == ellipsis ? 2 : 1;

    if (value.ucs) {

        if (to.value < from.value)
            return diag.error (e_range, from.where, "to-value <%s> of range "
                               "is smaller than from-value <%s>",
                               to.text.c_str (), from.text.c_str ());

        value.prefix = "U";
        value.width  = 0;
        value.first  = (unsigned long)from.value;
        value.last   = (unsigned long)to.value;

        attr = value;
        return true;
    }

    if (0 <= from.value || 0 <= to.value)
        return diag.error (e_pair, from.where, "ellipsis range must be "
                           "marked by two operands of same type");

    const std::string &a = from.text;
    const std::string &b = to.text;

    if (a.size () != b.size ())
        return diag.error (e_pair, from.where, "`%s' and `%s' are not "
                           "valid names for symbolic range",
                           a.c_str (), b.c_str ());

    std::size_t prefix = 0;

    while (prefix < a.size () && a [prefix] == b [prefix])
        ++prefix;

    value.prefix.assign (a, 0, prefix);
    value.width = int (a.size () - prefix);
    value.first = 0;
    value.last  = 0;

    // two names alike are a range of the one name
    if (prefix < a.size ()) {

        char *end = 0;

        value.first = std::strtoul (a.c_str () + prefix, &end, value.base);

        bool valid = 0 == *end;

        value.last = std::strtoul (b.c_str () + prefix, &end, value.base);

        if (!valid || *end || value.last < value.first)
            return diag.error (e_pair, from.where, "`%s' and `%s' are not "
                               "valid names for symbolic range",
                               a.c_str (), b.c_str ());
    }

    attr = value;

    return true;
}


std::string
range_member (const name_range_t &range, unsigned long n)
{
    char number [24];

    if (range.ucs) {
        std::sprintf (number, n < 0x10000UL ? "U%04lX" : "U%08lX", n);
        return number;
    }

    if (0 == range.width)
        return range.prefix;

    std::sprintf (number, 16 == range.base ? "%0*lX" : "%0*lu",
                  range.width, n);

    return range.prefix + number;
}


const char*
kind_name (token_kind_t kind)
{
    switch (kind) {
    case tk_keyword:     return "keyword";
    case tk_symbol:      return "symbol";
    case tk_string:      return "string";
    case tk_number:      return "number";
    case tk_charcode:    return "charcode";
    case tk_raw:         return "raw";
    case tk_ellipsis2:   return "ellipsis2";
    case tk_ellipsis3:   return "ellipsis3";
    case tk_ellipsis4:   return "ellipsis4";
    case tk_ellipsis2_2: return "ellipsis2_2";
    case tk_ellipsis4_2: return "ellipsis4_2";
    case tk_semicolon:   return "semicolon";
    case tk_comma:       return "comma";
    case tk_open:        return "open";
    case tk_close:       return "close";
    case tk_ident:       return "ident";
    }

    return "?";
}


// The byte form at s [pos], s [pos] being the escape character, by
// glibc's get_toplvl_escape(): an optional d or x, two digits of the
// base, and for octal and decimal an optional third digit, which glibc
// checks only for being a decimal digit.  Repeated while the escape
// character follows, up to glibc's 16 bytes.  Returns the position
// past the form, or npos when it is ill-formed.  glibc keeps the low
// byte of \d999; here a byte above 255 is ill-formed.
static std::size_t
scan_charcode (const std::string &s, std::size_t pos, std::string &bytes)
{
    const char escape = s [pos];

    bytes.clear ();

    do {
        ++pos;

        int base = 8;

        if (pos < s.size () && 'd' == s [pos]) {
            base = 10;
            ++pos;
        }
        else if (pos < s.size () && 'x' == s [pos]) {
            base = 16;
            ++pos;
        }

        int value = 0;

        for (int i = 0; i != 2; ++i, ++pos) {

            const bool valid =
                pos < s.size ()
                && (16 == base ? is_xdigit (s [pos])
                               : '0' <= s [pos] && s [pos] < '0' + base);

            if (!valid)
                return std::string::npos;

            value = value * base + digit_value (s [pos]);
        }

        if (16 != base && pos < s.size () && is_digit (s [pos]))
            value = value * base + digit_value (s [pos++]);

        if (255 < value)
            return std::string::npos;

        bytes += char (value);

    } while (pos < s.size () && escape == s [pos] && bytes.size () < 16);

    return pos;
}


bool
decode_charcode (const std::string &spelling, std::string &bytes)
{
    return    !spelling.empty ()
           && scan_charcode (spelling, 0, bytes) == spelling.size ();
}


// glibc's utf8_decode(): RFC 3629 lead bytes and no overlong form, but
// surrogates and values past 0x10FFFF from an F4 lead are accepted.
std::size_t
scan_utf8 (const std::string &s, std::size_t pos, long &ucs)
{
    const unsigned char lead = s [pos];

    std::size_t len;
    long        min;

    if (lead < 0xc2 || 0xf4 < lead)
        return 0;

    if (lead <= 0xdf) {
        len = 2;
        min = 0x80;
        ucs = lead & 0x1f;
    }
    else if (lead <= 0xef) {
        len = 3;
        min = 0x800;
        ucs = lead & 0x0f;
    }
    else {
        len = 4;
        min = 0x10000;
        ucs = lead & 0x07;
    }

    if (s.size () - pos < len)
        return 0;

    for (std::size_t i = 1; i != len; ++i) {

        const unsigned char next = s [pos + i];

        if (0x80 != (next & 0xc0))
            return 0;

        ucs = (ucs << 6) | (next & 0x3f);
    }

    return ucs < min ? 0 : len;
}


/**************************************************************************/
// the lexer

namespace {

// One logical line: the physical lines joined, each continuation (the
// escape character and the newline) removed, the last newline dropped.
struct logical_t {
    std::string              text;
    std::vector<std::size_t> starts;   // offset of each physical line
    int                      first;    // number of the first of them
};


// Splits one logical line into tokens.
struct lexer_t {

    lexer_t (const logical_t &l, const char *f, file_kind_t k, char cc,
             char ec, diag_t &d)
        : line (l), s (l.text), file (f), kind (k), comment (cc),
          escape (ec), diag (d), pos (0) { }

    bool next (token_t &tok);

    where_t where_at (std::size_t offset) const;
    std::size_t end_of_physical (std::size_t offset) const;

    bool lex (token_t &tok);
    bool ellipsis (token_t &tok);
    bool charcode (token_t &tok);
    void symbol (token_t &tok);
    void string (token_t &tok);
    void number (token_t &tok);
    void raw (token_t &tok);
    void ident (token_t &tok);

    const logical_t   &line;
    const std::string &s;
    const char        *file;
    const file_kind_t  kind;
    const char         comment;
    const char         escape;
    diag_t            &diag;

    std::size_t pos;
};


where_t lexer_t::
where_at (std::size_t offset) const
{
    std::size_t i = line.starts.size () - 1;

    while (offset < line.starts [i])
        --i;

    const where_t where = {
        file, line.first + int (i), int (offset - line.starts [i]) + 1
    };

    return where;
}


std::size_t lexer_t::
end_of_physical (std::size_t offset) const
{
    for (std::size_t i = 0; i != line.starts.size (); ++i)
        if (offset < line.starts [i])
            return line.starts [i];

    return s.size ();
}


bool lexer_t::
next (token_t &tok)
{
    for (;;) {

        while (pos < s.size () && is_space (s [pos]))
            ++pos;

        if (pos == s.size ())
            return false;

        // The comment character counts only where a token would start.
        // A comment ends with its physical line, and the escape
        // character at its end still continues the statement on the
        // next line: glibc's lr_next() removes the escape and the
        // newline before its lexer sees the line, and lr_token() goes
        // on with the statement where a comment ends short of a
        // newline.  glibc's sources write it that way (uk_UA lists
        // its day names one per continued line, each with a comment
        // before the escape; zh_CN comments out a range inside a
        // continued class), so a comment whose text ends in the escape
        // character is a continuation here too, as it is in glibc;
        // the tree's iso14651_t1 had one, which glibc rejects as this
        // reader did, and it was fixed in the source.
        if (comment == s [pos]) {
            pos = end_of_physical (pos);
            continue;
        }

        if (lex (tok))
            return true;
    }
}


bool lexer_t::
lex (token_t &tok)
{
    tok.text.clear ();
    tok.value = 0;
    tok.where = where_at (pos);

    const char c = s [pos];

    if (escape == c) {

        const bool kept = charcode (tok);

        // glibc's charmap parser ignores what follows the encoding of a
        // character, unlexed: the text is a description, and it may
        // hold a < with no >.  No other line of a charmap has a code.
        if (charmap_file == kind)
            pos = s.size ();

        return kept;
    }

    if ('.' == c && ellipsis (tok))
        return true;

    switch (c) {
    case '<':
        symbol (tok);
        return true;

    case '"':
        string (tok);
        return true;

    case ';': tok.kind = tk_semicolon; break;
    case ',': tok.kind = tk_comma;     break;
    case '(': tok.kind = tk_open;      break;
    case ')': tok.kind = tk_close;     break;

    case '-':
        // glibc's tok_minus1, only the two characters: -12 is -1 and 2.
        // glibc lexes any other word that starts with - with the byte
        // before it as its first; here the word starts with the -.
        if (pos + 1 < s.size () && '1' == s [pos + 1]) {
            tok.kind  = tk_number;
            tok.text  = "-1";
            tok.value = -1;
            pos      += 2;
            return true;
        }
        ident (tok);
        return true;

    default:
        if (is_digit (c))
            number (tok);
        else if (0x80 & c)
            raw (tok);
        else
            ident (tok);
        return true;
    }

    tok.text = c;
    ++pos;

    return true;
}


// glibc tests the longest form first: .... is one token, not two.
bool lexer_t::
ellipsis (token_t &tok)
{
    static const struct {
        const char  *spelling;
        token_kind_t kind;
    } forms [] = {
        { "....(2)....", tk_ellipsis4_2 },
        { "....",        tk_ellipsis4 },
        { "...",         tk_ellipsis3 },
        { "..(2)..",     tk_ellipsis2_2 },
        { "..",          tk_ellipsis2 }
    };

    for (std::size_t i = 0; i != sizeof forms / sizeof *forms; ++i) {

        const std::size_t len = std::strlen (forms [i].spelling);

        if (0 == s.compare (pos, len, forms [i].spelling)) {
            tok.kind  = forms [i].kind;
            tok.text  = forms [i].spelling;
            pos      += len;
            return true;
        }
    }

    // a lone dot starts a word
    return false;
}


// False when the form is ill-formed and the word is dropped.
bool lexer_t::
charcode (token_t &tok)
{
    const std::size_t start = pos;

    std::string       bytes;
    const std::size_t end = scan_charcode (s, pos, bytes);

    if (std::string::npos == end) {

        // glibc's tok_error: the word up to the next blank is dropped
        while (pos < s.size () && !is_space (s [pos]))
            ++pos;

        return diag.error (e_syntax, tok.where, "invalid character code %s",
                           s.substr (start, pos - start).c_str ());
    }

    pos = end;

    tok.kind  = tk_charcode;
    tok.text  = s.substr (start, pos - start);
    tok.value = long (bytes.size ());

    if (pos < s.size () && !is_space (s [pos]))
        diag.error (e_syntax, where_at (pos),
                    "garbage at end of character code specification");

    return true;
}


// The name runs to the closing bracket; the escape character makes the
// next character part of the name, a > among them.  A name of U and 4
// or 8 hexadecimal digits carries its code point, as glibc's tok_ucs4
// does, and is never a keyword.
void lexer_t::
symbol (token_t &tok)
{
    tok.kind  = tk_symbol;
    tok.value = -1;

    for (++pos; ; ) {

        if (pos == s.size ()) {
            diag.error (e_symend, tok.where, "unterminated symbolic name");
            return;
        }

        char c = s [pos++];

        if (escape == c) {
            if (pos == s.size ()) {
                diag.error (e_symend, tok.where,
                            "unterminated symbolic name");
                return;
            }
            c = s [pos++];
        }
        else if ('>' == c)
            break;

        tok.text += c;
    }

    const std::string &name = tok.text;

    if ((5 == name.size () || 9 == name.size ()) && 'U' == name [0]) {

        unsigned long ucs = 0;
        std::size_t   i   = 1;

        for (; i != name.size () && is_xdigit (name [i]); ++i)
            ucs = ucs * 16 + digit_value (name [i]);

        // glibc's charmap takes no value from 0x80000000 on
        if (i == name.size () && ucs < 0x80000000UL) {
            tok.value = long (ucs);
            return;
        }
    }

    const keyword_t keyword =
        charmap_file == kind ? find_keyword (name, cm_bracket) : kw_none;

    if (kw_none != keyword) {
        tok.kind  = tk_keyword;
        tok.value = keyword;
    }
}


// The extent is glibc's for a locale file, where strings are
// translated: the escape character takes the next character, and a <
// opens a name that runs to its >, a quote among it.  The text is kept
// raw for a later step to resolve against the charmap, and its bytes
// are that step's to check: glibc's lexer rejects a byte that is not
// UTF-8, and the strings that have one sit in categories the compiler
// skips.  glibc reads a charmap's strings untranslated, where an
// escaped quote ends the string; no charmap writes one.
void lexer_t::
string (token_t &tok)
{
    tok.kind = tk_string;

    for (++pos; ; ) {

        if (pos == s.size ()) {
            diag.error (e_syntax, tok.where, "unterminated string");
            return;
        }

        const char c = s [pos];

        if ('"' == c) {
            ++pos;
            return;
        }

        if (escape == c) {

            if (pos + 1 == s.size ()) {
                ++pos;
                continue;
            }

            tok.text.append (s, pos, 2);
            pos += 2;
            continue;
        }

        if ('<' == c) {

            std::size_t end = pos + 1;

            while (end < s.size () && '>' != s [end])
                end += escape == s [end] ? 2 : 1;

            if (s.size () <= end) {
                tok.text.append (s, pos, std::string::npos);
                pos = s.size ();
                continue;
            }

            tok.text.append (s, pos, end + 1 - pos);
            pos = end + 1;
            continue;
        }

        tok.text += c;
        ++pos;
    }
}


// Decimal only: glibc reads no 0x or leading-0 form, so 0x41 is the
// number 0 and the word x41.
void lexer_t::
number (token_t &tok)
{
    tok.kind = tk_number;

    while (pos < s.size () && is_digit (s [pos])) {

        const int digit = s [pos] - '0';

        if (tok.value <= (LONG_MAX - digit) / 10)
            tok.value = tok.value * 10 + digit;
        else
            tok.value = LONG_MAX;

        tok.text += s [pos++];
    }

    if (pos < s.size () && is_alpha (s [pos]))
        diag.error (e_ifmt, where_at (pos), "garbage at end of number");
}


// One character written raw in UTF-8, as glibc's tok_ucs4.  An
// ill-formed sequence is the lead byte and the continuation bytes after
// it, reported once.
void lexer_t::
raw (token_t &tok)
{
    tok.kind = tk_raw;

    long              ucs;
    const std::size_t len = scan_utf8 (s, pos, ucs);

    if (len) {
        tok.text.assign (s, pos, len);
        tok.value = ucs;
        pos      += len;
        return;
    }

    std::size_t end = pos + 1;

    while (end < s.size () && end - pos < 4 && 0x80 == (s [end] & 0xc0))
        ++end;

    tok.text.assign (s, pos, end - pos);
    tok.value = -1;
    pos       = end;

    diag.error (e_mbchar, tok.where, "invalid UTF-8 sequence");
}


// glibc's get_ident(): a word runs to a blank, a quote, a semicolon, a
// < or a comma.  A parenthesis, a dot and the comment character do not
// end it.  The escape character takes the next character into the word.
void lexer_t::
ident (token_t &tok)
{
    tok.kind = tk_ident;

    while (pos < s.size ()) {

        char c = s [pos];

        if (is_space (c) || '"' == c || ';' == c || '<' == c || ',' == c)
            break;

        ++pos;

        if (escape == c) {
            if (pos == s.size ()) {
                diag.error (e_syntax, where_at (pos - 1),
                            "invalid escape sequence");
                break;
            }
            c = s [pos++];
        }

        tok.text += c;
    }

    const keyword_t keyword =
        find_keyword (tok.text, charmap_file == kind ? cm_bare : lf_bare);

    if (kw_none != keyword) {
        tok.kind  = tk_keyword;
        tok.value = keyword;
    }
}

}   // namespace


/**************************************************************************/
// physical lines

// One physical line without its newline; false at the end of the file.
static bool
read_physical (std::FILE *stream, std::string &text, bool &newline)
{
    text.clear ();
    newline = false;

    for (int c; EOF != (c = std::getc (stream)); ) {

        if ('\n' == c) {
            newline = true;
            return true;
        }

        text += char (c);
    }

    return !text.empty ();
}


// A physical line that ends in the escape character and a newline goes
// on in the next one; glibc's lr_next() removes both and joins the two
// with nothing between.  The escape character is the one in effect when
// the line is read.
static bool
read_logical (std::FILE *stream, char escape, int &lineno, logical_t &line)
{
    line.text.clear ();
    line.starts.clear ();
    line.first = lineno + 1;

    std::string physical;
    bool        newline;

    while (read_physical (stream, physical, newline)) {

        ++lineno;

        line.starts.push_back (line.text.size ());

        const bool continued =
               newline && !physical.empty ()
            && escape == physical [physical.size () - 1];

        if (!continued) {
            line.text += physical;
            return true;
        }

        line.text.append (physical, 0, physical.size () - 1);
    }

    return !line.starts.empty ();
}


/**************************************************************************/
// reader_t

reader_t::
reader_t (diag_t &diag)
    : m_diag (diag)
{
}


reader_t::
~reader_t ()
{
    while (!m_files.empty ())
        pop ();
}


void reader_t::
push (const std::string &path, file_kind_t kind)
{
    std::FILE* const stream = std::fopen (path.c_str (), "rb");

    if (0 == stream) {
        open_error_t error;
        error.path  = path;
        error.error = errno;
        throw error;
    }

    const char* const name = m_names.insert (path).first->c_str ();
    const file_t      file = { stream, name, kind, 0, '#', '\\' };

    m_files.push_back (file);
}


void reader_t::
pop ()
{
    std::fclose (m_files.back ().stream);
    m_files.pop_back ();
}


// comment_char and escape_char, bare in a locale file and bracketed in
// a charmap.  The operand is the first character past the blanks after
// the keyword, taken raw, never lexed.  glibc lexes it, so where the
// operand is the comment character already in effect, comment_char #
// under the default #, the operand is read as a comment and the line
// fails.  glibc's own files never write that line, so its rule costs
// it nothing; the tree's iso14651_t1, which 100 of the definitions
// under etc/nls copy, and its CP437 do write it.  A line whose job is
// to name the comment character cannot be commented out by it.  glibc
// accepts the line only in a charmap's prolog or at the top of a
// locale file; here it is applied wherever it is read.
static bool
directive_operand (const logical_t &line, std::size_t pos, char comment,
                   const token_t &lead, diag_t &diag, char &operand)
{
    const std::string &s = line.text;

    while (pos < s.size () && is_space (s [pos]))
        ++pos;

    if (pos == s.size ())
        return diag.error (e_syntax, lead.where, "%s takes a character",
                           lead.text.c_str ());

    operand = s [pos++];

    while (pos < s.size () && is_space (s [pos]))
        ++pos;

    // a comment may follow, by the old character or the new one
    if (pos < s.size () && comment != s [pos] && operand != s [pos])
        diag.error (e_syntax, lead.where, "trailing garbage at end of line");

    return true;
}


bool reader_t::
next (line_t &line, std::size_t depth)
{
    logical_t text;

    while (!m_files.empty () && depth <= m_files.size ()) {

        file_t &file = m_files.back ();

        if (!read_logical (file.stream, file.escape, file.line, text)) {
            pop ();
            continue;
        }

        lexer_t lexer (text, file.name, file.kind, file.comment, file.escape,
                       m_diag);

        token_t tok;

        if (!lexer.next (tok))
            continue;

        const bool sets_char =
               tk_keyword == tok.kind
            && (kw_comment_char == tok.value || kw_escape_char == tok.value);

        if (sets_char) {

            char operand;

            if (directive_operand (text, lexer.pos, file.comment, tok,
                                   m_diag, operand)) {
                if (kw_comment_char == tok.value)
                    file.comment = operand;
                else
                    file.escape = operand;
            }

            continue;
        }

        line.file   = file.name;
        line.number = text.first;
        line.escape = file.escape;
        line.tokens.clear ();

        do
            line.tokens.push_back (tok);
        while (lexer.next (tok));

        return true;
    }

    return false;
}
