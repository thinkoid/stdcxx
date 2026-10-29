/***************************************************************************
 *
 * wmonetary.cpp
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

// LC_MONETARY: a punctuation header and its strings, then the
// monetary header, __rw_mon_t, with the nine small integers and the
// four format arrays inline and its strings after it.

#include "pool.h"
#include "writer.h"

#include <loc/_localedef.h>

#include <climits>   // for CHAR_MAX
#include <cstring>   // for memset()


// The parts of a money format, the values of std::money_base::part
// (C++03 22.2.6.3), which the facets read from the arrays.
enum {
    part_none   = 0,
    part_space  = 1,
    part_symbol = 2,
    part_sign   = 3,
    part_value  = 4
};


// One of the small integers: the value as written, or CHAR_MAX for -1
// and for a keyword the source leaves unset, as localeconv() reports
// a value that is not available.
static char
small_int (const field_t<int> &field)
{
    return field.set && 0 <= field.value ? char (field.value) : CHAR_MAX;
}


// One of the six international small integers: an unset keyword takes
// its domestic counterpart's value, CHAR_MAX when that is unset too,
// as glibc's monetary_finish() gives it; a -1 as written stays CHAR_MAX.
static char
small_int (const field_t<int> &field, const field_t<int> &domestic)
{
    return field.set ? small_int (field) : small_int (domestic);
}


// The format array of the old compiler's create_format(): the order
// of sign, symbol and value from sign_posn and cs_precedes, then the
// place of the space from sep_by_space.  A sign_posn of CHAR_MAX, no
// value, makes the four parts CHAR_MAX; a cs_precedes or sep_by_space
// of CHAR_MAX reads as 1 and as 2.
static void
format_of (char format [4], char sign_posn, char cs_precedes,
           char sep_by_space)
{
    format [0] = format [1] = format [2] = format [3] = part_none;

    if (CHAR_MAX == sign_posn) {
        format [0] = format [1] = format [2] = format [3] = CHAR_MAX;
        return;
    }

    // sign_posn 0, parentheses, and 1 put the sign first: the sign
    // string is "()" for 0, and the facet puts its second character
    // last
    switch (sign_posn) {

    case 0:
    case 1:
        format [0] = part_sign;
        format [1] = 0 == cs_precedes ? part_value : part_symbol;
        format [2] = 0 == cs_precedes ? part_symbol : part_value;
        break;

    case 2:
        format [0] = 0 == cs_precedes ? part_value : part_symbol;
        format [1] = 0 == cs_precedes ? part_symbol : part_value;
        format [2] = part_sign;
        break;

    case 3:
        if (0 == cs_precedes) {
            format [0] = part_value;
            format [1] = part_sign;
            format [2] = part_symbol;
        }
        else {
            format [0] = part_sign;
            format [1] = part_symbol;
            format [2] = part_value;
        }
        break;

    case 4:
        if (0 == cs_precedes) {
            format [0] = part_value;
            format [1] = part_symbol;
            format [2] = part_sign;
        }
        else {
            format [0] = part_symbol;
            format [1] = part_sign;
            format [2] = part_value;
        }
        break;

    default:
        break;
    }

    if (0 == sep_by_space) {
        format [3] = part_none;
        return;
    }

    // 1 separates the symbol from the value, 2 the sign from the
    // value: the old compiler's chain of cases, which puts the space
    // second or third by what comes first
    const bool value_or_symbol_first =
           part_value == format [0]
        || (part_value != format [2] && part_symbol == format [0]);

    const bool space_second =
        1 == sep_by_space ? value_or_symbol_first : !value_or_symbol_first;

    format [3] = format [2];

    if (space_second) {
        format [2] = format [1];
        format [1] = part_space;
    }
    else
        format [2] = part_space;
}


void
write_monetary (const locale_def_t &def, const cmap_t &cmap,
                const image_names_t &names, std::vector<char> &image)
{
    const monetary_def_t &mon      = def.monetary;
    const translit_t     &translit = def.ctype.translit;

    image.clear ();

    write_punct (mon.mon_decimal_point, mon.mon_thousands_sep,
                 mon.mon_grouping, cmap, translit, image);

    __rw::__rw_mon_t hdr;
    std::memset (&hdr, 0, sizeof hdr);

    hdr.frac_digits [0]    = small_int (mon.frac_digits);
    hdr.frac_digits [1]    = small_int (mon.int_frac_digits);
    hdr.p_cs_precedes [0]  = small_int (mon.p_cs_precedes);
    hdr.p_cs_precedes [1]  = small_int (mon.int_p_cs_precedes,
                                        mon.p_cs_precedes);
    hdr.p_sep_by_space [0] = small_int (mon.p_sep_by_space);
    hdr.p_sep_by_space [1] = small_int (mon.int_p_sep_by_space,
                                        mon.p_sep_by_space);
    hdr.n_cs_precedes [0]  = small_int (mon.n_cs_precedes);
    hdr.n_cs_precedes [1]  = small_int (mon.int_n_cs_precedes,
                                        mon.n_cs_precedes);
    hdr.n_sep_by_space [0] = small_int (mon.n_sep_by_space);
    hdr.n_sep_by_space [1] = small_int (mon.int_n_sep_by_space,
                                        mon.n_sep_by_space);
    hdr.p_sign_posn [0]    = small_int (mon.p_sign_posn);
    hdr.p_sign_posn [1]    = small_int (mon.int_p_sign_posn,
                                        mon.p_sign_posn);
    hdr.n_sign_posn [0]    = small_int (mon.n_sign_posn);
    hdr.n_sign_posn [1]    = small_int (mon.int_n_sign_posn,
                                        mon.n_sign_posn);

    for (int intl = 0; intl != 2; ++intl) {
        format_of (hdr.pos_format [intl], hdr.p_sign_posn [intl],
                   hdr.p_cs_precedes [intl], hdr.p_sep_by_space [intl]);
        format_of (hdr.neg_format [intl], hdr.n_sign_posn [intl],
                   hdr.n_cs_precedes [intl], hdr.n_sep_by_space [intl]);
    }

    // A sign_posn of 0 puts parentheses around the quantity and the
    // symbol, and the facets spell that as the sign string "()", the
    // first character before and the rest after.  The old compiler
    // substituted it into the narrow sign, for the local or the
    // international format, after it had sized the strings, so every
    // offset past the sign fell two bytes short of its string, and it
    // left the wide sign as written, so the wide facet printed a minus
    // where the narrow one printed parentheses.  Here the substituted
    // string is the one sized, and both forms get it.
    ucs_string_t wide_positive = mon.positive_sign.value;
    ucs_string_t wide_negative = mon.negative_sign.value;
    std::string  positive = narrow (wide_positive, cmap, translit);
    std::string  negative = narrow (wide_negative, cmap, translit);

    ucs_string_t parens;
    parens.push_back ('(');
    parens.push_back (')');

    if (0 == hdr.p_sign_posn [0] || 0 == hdr.p_sign_posn [1]) {
        wide_positive = parens;
        positive      = "()";
    }

    if (0 == hdr.n_sign_posn [0] || 0 == hdr.n_sign_posn [1]) {
        wide_negative = parens;
        negative      = "()";
    }

    pool_t pool;

    hdr.curr_symbol_off [1][1] = pool.add (mon.int_curr_symbol.value);
    hdr.curr_symbol_off [0][1] = pool.add (mon.currency_symbol.value);
    hdr.positive_sign_off [1]  = pool.add (wide_positive);
    hdr.negative_sign_off [1]  = pool.add (wide_negative);
    hdr.curr_symbol_off [1][0] =
        pool.add (narrow (mon.int_curr_symbol.value, cmap, translit));
    hdr.curr_symbol_off [0][0] =
        pool.add (narrow (mon.currency_symbol.value, cmap, translit));
    hdr.positive_sign_off [0]  = pool.add (positive);
    hdr.negative_sign_off [0]  = pool.add (negative);
    hdr.codeset_off            = pool.add (names.codeset);
    hdr.charmap_off            = pool.add (names.charmap);

    append (image, &hdr, sizeof hdr);
    pool.append_to (image);
}
