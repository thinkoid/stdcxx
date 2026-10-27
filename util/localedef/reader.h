/***************************************************************************
 *
 * reader.h
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

#ifndef RWSTD_UTIL_LOCALEDEF_READER_H_INCLUDED
#define RWSTD_UTIL_LOCALEDEF_READER_H_INCLUDED

#include "diag.h"

#include <cstddef>   // for size_t
#include <cstdio>    // for FILE
#include <set>
#include <string>
#include <vector>


// The lexical rules are those of glibc's locale/programs/linereader.c,
// since the compiler reads the sources glibc writes.  The kinds are the
// tokens of its locfile-token.h, less the ones that are not lexical.
enum token_kind_t {
    tk_keyword,       // a word of the keyword table; value is its keyword_t
    tk_symbol,        // <name>: text is the name, escapes resolved; value
                      // is the code point of a <Uxxxx> name, else -1
    tk_string,        // "...": text is the raw text between the quotes
    tk_number,        // decimal digits, or -1; value is the number
    tk_charcode,      // \x41, \d065, \101, or several in a row: text is
                      // the spelling, value the number of bytes
    tk_raw,           // a character written raw in UTF-8: text is its
                      // bytes, value its code point, or -1 if ill-formed
    tk_ellipsis2,     // ..
    tk_ellipsis3,     // ...
    tk_ellipsis4,     // ....
    tk_ellipsis2_2,   // ..(2)..
    tk_ellipsis4_2,   // ....(2)....
    tk_semicolon,
    tk_comma,
    tk_open,          // (
    tk_close,         // )
    tk_ident          // a bare word not in the keyword table
};


// Every keyword of glibc's locfile-kw.gperf and charmap-kw.gperf, in
// the groups of its locfile-token.h.  Which of them the compiler reads,
// ignores or skips whole is the plan's chapter 6, not the table's.
enum keyword_t {
    kw_none,

    // charmap files
    kw_code_set_name, kw_mb_cur_max, kw_mb_cur_min,
    kw_escape_char, kw_comment_char,
    kw_g0esc, kw_g1esc, kw_g2esc, kw_g3esc, kw_escseq, kw_addset,
    kw_include, kw_charmap, kw_end,
    kw_width_section,   // WIDTH, of a charmap; width is LC_PAPER's
    kw_width_variable, kw_width_default,

    // locale files
    kw_repertoiremap, kw_copy,

    kw_lc_ctype,
    kw_upper, kw_lower, kw_alpha, kw_digit, kw_xdigit, kw_space,
    kw_print, kw_graph, kw_blank, kw_cntrl, kw_punct, kw_alnum,
    kw_outdigit, kw_charclass, kw_class, kw_charconv,
    kw_toupper, kw_tolower, kw_map,
    kw_translit_start, kw_translit_end, kw_translit_ignore,
    kw_default_missing,

    kw_lc_collate,
    kw_codepoint_collation, kw_coll_weight_max, kw_section_symbol,
    kw_collating_element, kw_collating_symbol, kw_symbol_equivalence,
    kw_script, kw_order_start, kw_order_end, kw_from,
    kw_forward, kw_backward, kw_position, kw_undefined, kw_ignore,
    kw_reorder_after, kw_reorder_end,
    kw_reorder_sections_after, kw_reorder_sections_end,
    kw_define, kw_undef, kw_ifdef, kw_else, kw_elifdef, kw_elifndef,
    kw_endif,

    kw_lc_monetary,
    kw_int_curr_symbol, kw_currency_symbol,
    kw_mon_decimal_point, kw_mon_thousands_sep, kw_mon_grouping,
    kw_positive_sign, kw_negative_sign,
    kw_int_frac_digits, kw_frac_digits,
    kw_p_cs_precedes, kw_p_sep_by_space,
    kw_n_cs_precedes, kw_n_sep_by_space,
    kw_p_sign_posn, kw_n_sign_posn,
    kw_int_p_cs_precedes, kw_int_p_sep_by_space,
    kw_int_n_cs_precedes, kw_int_n_sep_by_space,
    kw_int_p_sign_posn, kw_int_n_sign_posn,
    kw_duo_int_curr_symbol, kw_duo_currency_symbol,
    kw_duo_int_frac_digits, kw_duo_frac_digits,
    kw_duo_p_cs_precedes, kw_duo_p_sep_by_space,
    kw_duo_n_cs_precedes, kw_duo_n_sep_by_space,
    kw_duo_int_p_cs_precedes, kw_duo_int_p_sep_by_space,
    kw_duo_int_n_cs_precedes, kw_duo_int_n_sep_by_space,
    kw_duo_p_sign_posn, kw_duo_n_sign_posn,
    kw_duo_int_p_sign_posn, kw_duo_int_n_sign_posn,
    kw_uno_valid_from, kw_uno_valid_to,
    kw_duo_valid_from, kw_duo_valid_to,
    kw_conversion_rate,

    kw_lc_numeric,
    kw_decimal_point, kw_thousands_sep, kw_grouping,

    kw_lc_time,
    kw_abday, kw_day, kw_abmon, kw_mon,
    kw_d_t_fmt, kw_d_fmt, kw_t_fmt, kw_am_pm, kw_t_fmt_ampm,
    kw_era, kw_era_year, kw_era_d_fmt, kw_era_d_t_fmt, kw_era_t_fmt,
    kw_alt_digits, kw_week, kw_first_weekday, kw_first_workday,
    kw_cal_direction, kw_timezone, kw_date_fmt, kw_alt_mon, kw_ab_alt_mon,

    kw_lc_messages,
    kw_yesexpr, kw_noexpr, kw_yesstr, kw_nostr,

    kw_lc_paper,
    kw_height, kw_width,

    kw_lc_name,
    kw_name_fmt, kw_name_gen, kw_name_mr, kw_name_mrs, kw_name_miss,
    kw_name_ms,

    kw_lc_address,
    kw_postal_fmt, kw_country_name, kw_country_post,
    kw_country_ab2, kw_country_ab3, kw_country_num, kw_country_car,
    kw_country_isbn, kw_lang_name, kw_lang_ab, kw_lang_term, kw_lang_lib,

    kw_lc_telephone,
    kw_tel_int_fmt, kw_tel_dom_fmt, kw_int_select, kw_int_prefix,

    kw_lc_measurement,
    kw_measurement,

    kw_lc_identification,
    kw_title, kw_source, kw_address, kw_contact, kw_email, kw_tel,
    kw_fax, kw_language, kw_territory, kw_audience, kw_application,
    kw_abbreviation, kw_revision, kw_date, kw_category
};


struct token_t {
    token_kind_t kind;
    std::string  text;
    long         value;
    where_t      where;   // the file is a name the reader owns
};

typedef std::vector<token_t>::const_iterator tok_iter;


// One logical line: physical lines joined by continuation.  The end of
// the line is the end of the vector; there is no end-of-line token.
struct line_t {
    const char          *file;     // a name the reader owns
    int                  number;   // the physical line it starts on
    char                 escape;   // the escape character in effect
    std::vector<token_t> tokens;
};


// The position to report at when a production runs out of tokens: the
// last one's.  Every production has consumed a token by then, so it - 1
// is the line's.
const where_t& where_of (tok_iter it, tok_iter last);

// True at the end of the line; otherwise reports trailing garbage.
bool end_of_line (tok_iter it, tok_iter last, diag_t &diag);

// The keyword of a token, kw_none when it is not one.
keyword_t keyword_of (const token_t &tok);


// The name and spelling of a kind, for dumps and messages.
const char* kind_name (token_kind_t kind);

// The bytes of a tk_charcode spelling, whose first character is the
// escape character it was written with.  False if it is ill-formed.
bool decode_charcode (const std::string &spelling, std::string &bytes);

// The length of the UTF-8 sequence at s [pos], and its code point in
// ucs, under glibc's rule; 0 when the sequence is ill-formed.
std::size_t scan_utf8 (const std::string &s, std::size_t pos, long &ucs);


// glibc reads the two kinds of file with two keyword tables, and stops
// lexing a charmap line at the encoding of its character.
enum file_kind_t {
    locale_file,
    charmap_file
};


// A stack of files read one logical line at a time.  The caller nests
// a file by pushing it; the reader pops a file when it ends.  It does
// not interpret copy or include.
class reader_t
{
public:

    explicit reader_t (diag_t &diag);

    ~reader_t ();

    // Opens path and reads it before the rest of the file below it.
    // Throws open_error_t when path cannot be opened.
    void push (const std::string &path, file_kind_t kind);

    // Closes the file on top, abandoning the rest of it.  The stack
    // must not be empty.
    void pop ();

    bool empty () const {
        return m_files.empty ();
    }

    // The number of files open.  After next() it is the depth of the
    // file the line came from, since next() pops the files that ended.
    std::size_t depth () const {
        return m_files.size ();
    }

    // The next line that has a token, from the file at depth or one
    // above it.  Blank and comment lines are skipped, and so are
    // comment_char and escape_char lines, which the reader applies,
    // since they change its own lexing.  False when those files have
    // ended; the files below stay open.  A section reads with the depth
    // of its own file, so that it stops where the file does.
    bool next (line_t &line, std::size_t depth = 1);

private:

    reader_t (const reader_t&);         // not defined
    void operator= (const reader_t&);   // not defined

    struct file_t {
        std::FILE  *stream;
        const char *name;
        file_kind_t kind;
        int         line;      // physical lines read so far
        char        comment;
        char        escape;
    };

    // Every stream on the stack is open and owned here; the destructor
    // closes them.
    std::vector<file_t> m_files;

    // Every name ever pushed.  Tokens point into it, and a set keeps
    // its strings in place, so a name outlives the pop of its file.
    std::set<std::string> m_names;

    diag_t &m_diag;
};

#endif   // RWSTD_UTIL_LOCALEDEF_READER_H_INCLUDED
