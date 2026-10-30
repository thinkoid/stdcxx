/***************************************************************************
 *
 * scalar.cpp - the library's punctuation and names against the C library's
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
 * A differential oracle for localedef: the scalar values of a
 * database the library's localedef built, read through the facets
 * held to the library's own databases, against the C library under
 * setlocale for the same name. The C library must have the locale
 * generated. The items:
 *
 *   numpunct<char>, numpunct<wchar_t>
 *            decimal_point, thousands_sep and grouping against
 *            localeconv's decimal_point, thousands_sep and grouping
 *   moneypunct<char, false>, moneypunct<char, true>,
 *   moneypunct<wchar_t, false>, moneypunct<wchar_t, true>
 *            decimal_point, thousands_sep, grouping, curr_symbol,
 *            positive_sign, negative_sign, frac_digits, pos_format
 *            and neg_format against localeconv's mon_decimal_point,
 *            mon_thousands_sep, mon_grouping, currency_symbol or
 *            int_curr_symbol, positive_sign, negative_sign,
 *            frac_digits or int_frac_digits, and the cs_precedes,
 *            sep_by_space and sign_posn of p_ and n_ or of int_p_
 *            and int_n_
 *   time_put<char>, time_put<wchar_t>
 *            %A and %a for each day of the week, %B and %b for each
 *            month, %p at 09:00 and at 21:00, and %x, %X and %c of
 *            Wednesday 2026-10-28 21:05:09, day 300 of the year, no
 *            daylight saving time, against strftime and wcsftime
 *            with the same format and the same std::tm; the oracle
 *            sets TZ to UTC0, so that the zone %c spells in some
 *            locales does not depend on the host's
 *
 * The wide facets are compared with the C library's bytes converted
 * by mbstowcs under the same locale. A separator is one character:
 * the C library's string of one character is compared with it, an
 * empty string with the null character. Groupings are compared by
 * their meaning in C++03 22.2.3.1.2, p3: a group of -1 and one of
 * CHAR_MAX both end the grouping, and the empty grouping, none at
 * all, agrees with one whose first group is unlimited. The patterns
 * are the C library's three fields mapped by the table the library's
 * moneypunct builds its facets from the C library with, in
 * src/punct.cpp, which covers every value in range, sep_by_space 2
 * included. A sign_posn of 0, parentheses around the quantity and
 * the symbol, is in C++ the sign string "()", whose first character
 * the facet puts at the sign and the rest at the end; under it the
 * expected sign is "()".
 *
 * An item the facet has no way to hold is counted as unrepresentable
 * and not compared: a separator the C library spells in more than one
 * character, narrow or wide; a frac_digits of CHAR_MAX, not
 * available; and a pattern one of whose three fields is CHAR_MAX,
 * not available, or out of range.
 *
 * The output lists each disagreement as the locale, the facet, the
 * item and the two values, bytes and code points outside printable
 * ASCII in hexadecimal, \xNN narrow and <UNNNN> wide; then one line
 * with the number of items, of disagreements and of unrepresentable
 * items. The exit status is 0 when nothing disagrees.
 *
 * Build, against a build directory $B of the library, with the
 * CPPFLAGS of its makefile.in (15D here, a debug build):
 *
 *   gcc -I$TOPDIR/include -I$TOPDIR/include/ansi -I$B/include \
 *       -D_RWSTDDEBUG -nostdinc++ -pthread -g scalar.cpp \
 *       -o scalar-oracle -L$B/lib -Wl,-R$B/lib -lstd15D -lsupc++ -lm
 *
 * Run, with the database for de_DE.UTF-8 under $ROOT (from
 * "localedef -c -f UTF-8 -i de_DE de_DE.UTF-8" run in $ROOT):
 *
 *   RWSTD_LOCALE_ROOT=$ROOT ./scalar-oracle -c de_DE.utf8 de_DE.UTF-8
 *
 * scalar-all.sh runs it over every locale of etc/nls/SUPPORTED.
 *
 * Options:
 *
 *   -c name     the C library's name for the locale, when not the
 *               library's (de_DE.utf8 for de_DE.UTF-8)
 *   -u          list the unrepresentable items too
 *
 **************************************************************************/

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


// the C library's values, copied as soon as the locale is set: the
// structure localeconv returns is overwritten by the next call
struct lconv_t
{
    std::string decimal_point;
    std::string thousands_sep;
    std::string grouping;

    std::string mon_decimal_point;
    std::string mon_thousands_sep;
    std::string mon_grouping;
    std::string positive_sign;
    std::string negative_sign;

    // domestic [0] and international [1], as in __rw_mon_t
    std::string curr_symbol [2];
    char        frac_digits [2];
    char        p_cs_precedes [2];
    char        p_sep_by_space [2];
    char        p_sign_posn [2];
    char        n_cs_precedes [2];
    char        n_sep_by_space [2];
    char        n_sign_posn [2];
};


static void
read_lconv (lconv_t& lc)
{
    const std::lconv* const pc = std::localeconv ();

    lc.decimal_point     = pc->decimal_point;
    lc.thousands_sep     = pc->thousands_sep;
    lc.grouping          = pc->grouping;

    lc.mon_decimal_point = pc->mon_decimal_point;
    lc.mon_thousands_sep = pc->mon_thousands_sep;
    lc.mon_grouping      = pc->mon_grouping;
    lc.positive_sign     = pc->positive_sign;
    lc.negative_sign     = pc->negative_sign;

    lc.curr_symbol [0]    = pc->currency_symbol;
    lc.curr_symbol [1]    = pc->int_curr_symbol;
    lc.frac_digits [0]    = pc->frac_digits;
    lc.frac_digits [1]    = pc->int_frac_digits;
    lc.p_cs_precedes [0]  = pc->p_cs_precedes;
    lc.p_cs_precedes [1]  = pc->int_p_cs_precedes;
    lc.p_sep_by_space [0] = pc->p_sep_by_space;
    lc.p_sep_by_space [1] = pc->int_p_sep_by_space;
    lc.p_sign_posn [0]    = pc->p_sign_posn;
    lc.p_sign_posn [1]    = pc->int_p_sign_posn;
    lc.n_cs_precedes [0]  = pc->n_cs_precedes;
    lc.n_cs_precedes [1]  = pc->int_n_cs_precedes;
    lc.n_sep_by_space [0] = pc->n_sep_by_space;
    lc.n_sep_by_space [1] = pc->int_n_sep_by_space;
    lc.n_sign_posn [0]    = pc->n_sign_posn;
    lc.n_sign_posn [1]    = pc->int_n_sign_posn;
}


// the library's database for the locale must exist under the root:
// without one the facets would come from the C library and agree
// with it trivially
static bool
have_database (const char* name, const char* category)
{
    const char* const root = std::getenv ("RWSTD_LOCALE_ROOT");

    const std::string path =
        std::string (root ? root : ".") + '/' + name + '/' + category;

    std::FILE* const f = std::fopen (path.c_str (), "rb");
    if (0 == f) {
        std::fprintf (stderr, "no database %s\n", path.c_str ());
        return false;
    }

    std::fclose (f);

    return true;
}


static const char*
char_name (char)
{
    return "char";
}


static const char*
char_name (wchar_t)
{
    return "wchar_t";
}


// the C library's bytes in the facet's character type: as they are,
// or converted by the C library in the locale set
static std::string
convert (const std::string& s, char)
{
    return s;
}


static std::wstring
convert (const std::string& s, wchar_t)
{
    std::vector<wchar_t> buf (s.size () + 1);

    const std::size_t n = std::mbstowcs (&buf [0], s.c_str (), buf.size ());

    if (std::size_t (-1) == n)
        return L"<invalid>";

    return std::wstring (&buf [0], n);
}


// the printable ASCII characters but the backslash, the quote and
// the angle bracket as they are, the rest in hexadecimal, so that
// two values agree exactly when their forms do
static std::string
show (const std::string& s)
{
    std::string out;

    for (std::size_t i = 0; i != s.size (); ++i) {
        const unsigned char c = (unsigned char)s [i];

        if (' ' <= c && c <= '~' && '\\' != c && '"' != c && '<' != c)
            out += char (c);
        else {
            char buf [8];
            std::sprintf (buf, "\\x%02X", c);
            out += buf;
        }
    }

    return out;
}


static std::string
show (const std::wstring& s)
{
    std::string out;

    for (std::size_t i = 0; i != s.size (); ++i) {
        const unsigned long c = (unsigned long)s [i];

        if (' ' <= c && c <= '~' && '\\' != c && '"' != c && '<' != c)
            out += char (c);
        else {
            char buf [16];
            std::sprintf (buf, "<U%04lX>", c);
            out += buf;
        }
    }

    return out;
}


struct tally_t
{
    const char*   locale;
    bool          list_unrepresentable;
    unsigned long items;
    unsigned long disagreements;
    unsigned long unrepresentable;

    tally_t (const char* name, bool list)
        : locale (name), list_unrepresentable (list),
          items (0), disagreements (0), unrepresentable (0) { }

    void check (bool agree, const std::string& facet, const std::string& item,
                const std::string& ours, const std::string& libc) {
        ++items;

        if (agree)
            return;

        ++disagreements;
        std::printf ("  %s %s %s: library \"%s\" C library \"%s\"\n",
                     locale, facet.c_str (), item.c_str (),
                     ours.c_str (), libc.c_str ());
    }

    void skip (const std::string& facet, const std::string& item,
               const std::string& libc) {
        ++items;
        ++unrepresentable;

        if (list_unrepresentable)
            std::printf ("  %s %s %s: unrepresentable, C library \"%s\"\n",
                         locale, facet.c_str (), item.c_str (),
                         libc.c_str ());
    }
};


// a separator: the C library's string of one character, or none
template <class charT>
static void
check_char (tally_t& tally, const std::string& facet, const char* item,
            charT ours, const std::basic_string<charT>& libc)
{
    if (1 < libc.size ()) {
        tally.skip (facet, item, show (libc));
        return;
    }

    const charT expected = libc.empty () ? charT () : libc [0];

    tally.check (ours == expected, facet, item,
                 show (std::basic_string<charT>(1, ours)), show (libc));
}


// a grouping in its C++ meaning: the sizes of the groups up to the
// first that is unlimited (a value <= 0 or CHAR_MAX), marked by a
// trailing 0, or else up to the end, the last size repeated, a run
// of equal sizes at the end folded into one; no groups at all is an
// unlimited first group
static std::vector<int>
meaning_of_grouping (const std::string& grouping)
{
    std::vector<int> sizes;

    for (std::size_t i = 0; i != grouping.size (); ++i) {
        const int n = (signed char)grouping [i];

        if (n <= 0 || CHAR_MAX == n) {
            sizes.push_back (0);
            return sizes;
        }

        sizes.push_back (n);
    }

    if (sizes.empty ())
        sizes.push_back (0);

    while (1 < sizes.size () && sizes [sizes.size () - 1]
                             == sizes [sizes.size () - 2])
        sizes.pop_back ();

    return sizes;
}


static void
check_grouping (tally_t& tally, const std::string& facet, const char* item,
                const std::string& ours, const std::string& libc)
{
    tally.check (meaning_of_grouping (ours) == meaning_of_grouping (libc),
                 facet, item, show (ours), show (libc));
}


template <class charT>
static void
check_numpunct (tally_t& tally, const std::locale& loc, const lconv_t& lc)
{
    const std::numpunct<charT>& np =
        std::use_facet<std::numpunct<charT> >(loc);

    const std::string facet =
        std::string ("numpunct<") + char_name (charT ()) + '>';

    check_char (tally, facet, "decimal_point", np.decimal_point (),
                convert (lc.decimal_point, charT ()));
    check_char (tally, facet, "thousands_sep", np.thousands_sep (),
                convert (lc.thousands_sep, charT ()));

    check_grouping (tally, facet, "grouping", np.grouping (), lc.grouping);
}


enum {
    // for syntactic convenience
    none   = std::money_base::none,
    space  = std::money_base::space,
    symbol = std::money_base::symbol,
    sign   = std::money_base::sign,
    value  = std::money_base::value
};

// the patterns of src/punct.cpp (__rw_get_moneypunct), indexed by
// cs_precedes [0..1], sep_by_space [0..2] and sign_posn [0..4]
static const std::money_base::pattern patterns [] = {

    // +-------- cs_precedes
    // |+----- sep_by_space
    // ||+-- sign_posn
    // |||
    // VVV  ....        -     1      $       .           // pattern
    /* 000: -1$. */ { { sign, value, symbol, none } },   // "\3\4\2\0"
    /* 001: -1$. */ { { sign, value, symbol, none } },   // "\3\4\2\0"
    /* 002: 1$-. */ { { value, symbol, sign, none } },   // "\4\2\3\0"
    /* 003: 1-$. */ { { value, sign, symbol, none } },   // "\4\3\2\0"
    /* 004: 1$-. */ { { value, symbol, sign, none } },   // "\4\2\3\0"

    /* 010: -1 $ */ { { sign, value, space, symbol } },  // "\3\4\1\2"
    /* 011: -1 $ */ { { sign, value, space, symbol } },  // "\3\4\1\2"
    /* 012: 1 $- */ { { value, space, symbol, sign } },  // "\4\1\2\3"
    /* 013: 1 -$ */ { { value, space, sign, symbol } },  // "\4\1\3\2"
    /* 014: 1 $- */ { { value, space, symbol, sign } },  // "\4\1\2\3"

    /* 020: - 1$ */ { { sign, space, value, symbol } },  // "\3\1\4\2"
    /* 021: - 1$ */ { { sign, space, value, symbol } },  // "\3\1\4\2"
    /* 022: 1$ - */ { { value, symbol, space, sign } },  // "\4\2\1\3"
    /* 023: 1- $ */ { { value, sign, space, symbol } },  // "\4\3\1\2"
    /* 024: 1$ - */ { { value, symbol, space, sign } },  // "\4\2\1\3"

    /* 100: -$1. */ { { sign, symbol, value, none } },   // "\3\2\4\0"
    /* 101: -$1. */ { { sign, symbol, value, none } },   // "\3\2\4\0"
    /* 102: $1-. */ { { symbol, value, sign, none } },   // "\2\4\3\0"
    /* 103: -$1. */ { { sign, symbol, value, none } },   // "\3\2\4\0"
    /* 104: $-1. */ { { symbol, sign, value, none } },   // "\2\3\4\0"

    /* 110: -$ 1 */ { { sign, symbol, space, value } },  // "\3\2\1\4"
    /* 111: -$ 1 */ { { sign, symbol, space, value } },  // "\3\2\1\4"
    /* 112: $ 1- */ { { symbol, space, value, sign } },  // "\2\1\4\3"
    /* 113: -$ 1 */ { { sign, symbol, space, value } },  // "\3\2\1\4"
    /* 114: $- 1 */ { { symbol, sign, space, value } },  // "\2\3\1\4"

    /* 120: - $1 */ { { sign, space, symbol, value } },  // "\3\1\2\4"
    /* 121: - $1 */ { { sign, space, symbol, value } },  // "\3\1\2\4"
    /* 122: $1 - */ { { symbol, value, space, sign } },  // "\2\4\1\3"
    /* 123: - $1 */ { { sign, space, symbol, value } },  // "\3\1\2\4"
    /* 124: $ -1 */ { { symbol, space, sign, value } }   // "\2\1\3\4"
};


static std::string
show (const std::money_base::pattern& pat)
{
    static const char* const parts [] = {
        "none", "space", "symbol", "sign", "value"
    };

    std::string out;

    for (int i = 0; i != 4; ++i) {
        const unsigned part = (unsigned char)pat.field [i];

        if (i)
            out += ' ';

        if (part < sizeof parts / sizeof *parts)
            out += parts [part];
        else {
            char buf [8];
            std::sprintf (buf, "%u", part);
            out += buf;
        }
    }

    return out;
}


static std::string
show_fields (char cs_precedes, char sep_by_space, char sign_posn)
{
    char buf [64];
    std::sprintf (buf, "cs_precedes %d sep_by_space %d sign_posn %d",
                  (unsigned char)cs_precedes, (unsigned char)sep_by_space,
                  (unsigned char)sign_posn);

    return buf;
}


static void
check_pattern (tally_t& tally, const std::string& facet, const char* item,
               const std::money_base::pattern& ours,
               char cs_precedes, char sep_by_space, char sign_posn)
{
    const unsigned cs   = (unsigned char)cs_precedes;
    const unsigned sep  = (unsigned char)sep_by_space;
    const unsigned posn = (unsigned char)sign_posn;

    if (1 < cs || 2 < sep || 4 < posn) {
        tally.skip (facet, item,
                    show_fields (cs_precedes, sep_by_space, sign_posn));
        return;
    }

    const std::money_base::pattern& expected =
        patterns [cs * 3 * 5 + sep * 5 + posn];

    tally.check (0 == std::memcmp (ours.field, expected.field, 4),
                 facet, item, show (ours),
                 show (expected) + " from "
                 + show_fields (cs_precedes, sep_by_space, sign_posn));
}


// a sign: "()" under a sign_posn of 0, else the C library's
template <class charT>
static void
check_sign (tally_t& tally, const std::string& facet, const char* item,
            const std::basic_string<charT>& ours, const std::string& libc,
            char sign_posn)
{
    const std::string expected = 0 == sign_posn ? std::string ("()") : libc;

    const std::basic_string<charT> wexpected = convert (expected, charT ());

    tally.check (ours == wexpected, facet, item, show (ours),
                 show (wexpected));
}


template <class charT, bool Intl>
static void
check_moneypunct (tally_t& tally, const std::locale& loc, const lconv_t& lc)
{
    const std::moneypunct<charT, Intl>& mp =
        std::use_facet<std::moneypunct<charT, Intl> >(loc);

    const std::string facet = std::string ("moneypunct<")
        + char_name (charT ()) + (Intl ? ", true>" : ", false>");

    check_char (tally, facet, "decimal_point", mp.decimal_point (),
                convert (lc.mon_decimal_point, charT ()));
    check_char (tally, facet, "thousands_sep", mp.thousands_sep (),
                convert (lc.mon_thousands_sep, charT ()));

    check_grouping (tally, facet, "grouping", mp.grouping (),
                    lc.mon_grouping);

    const std::basic_string<charT> cs =
        convert (lc.curr_symbol [Intl], charT ());

    tally.check (mp.curr_symbol () == cs, facet, "curr_symbol",
                 show (mp.curr_symbol ()), show (cs));

    check_sign (tally, facet, "positive_sign", mp.positive_sign (),
                lc.positive_sign, lc.p_sign_posn [Intl]);
    check_sign (tally, facet, "negative_sign", mp.negative_sign (),
                lc.negative_sign, lc.n_sign_posn [Intl]);

    char ours [16];
    char libc [16];
    std::sprintf (ours, "%d", mp.frac_digits ());
    std::sprintf (libc, "%d", (unsigned char)lc.frac_digits [Intl]);

    if (CHAR_MAX == lc.frac_digits [Intl])
        tally.skip (facet, "frac_digits", libc);
    else
        tally.check (mp.frac_digits () == lc.frac_digits [Intl], facet,
                     "frac_digits", ours, libc);

    check_pattern (tally, facet, "pos_format", mp.pos_format (),
                   lc.p_cs_precedes [Intl], lc.p_sep_by_space [Intl],
                   lc.p_sign_posn [Intl]);
    check_pattern (tally, facet, "neg_format", mp.neg_format (),
                   lc.n_cs_precedes [Intl], lc.n_sep_by_space [Intl],
                   lc.n_sign_posn [Intl]);
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
libc_put (const std::tm& t, const char* fmt, char)
{
    char buf [1024];
    const std::size_t n = std::strftime (buf, sizeof buf, fmt, &t);

    return std::string (buf, n);
}


static std::wstring
libc_put (const std::tm& t, const char* fmt, wchar_t)
{
    wchar_t wfmt [8];
    std::mbstowcs (wfmt, fmt, sizeof wfmt / sizeof *wfmt);

    wchar_t buf [1024];
    const std::size_t n =
        std::wcsftime (buf, sizeof buf / sizeof *buf, wfmt, &t);

    return std::wstring (buf, n);
}


template <class charT>
static void
check_put (tally_t& tally, const std::locale& loc, const std::string& facet,
           const std::tm& t, const char* fmt, const char* what)
{
    const std::basic_string<charT> ours = put<charT> (loc, t, fmt);
    const std::basic_string<charT> libc = libc_put (t, fmt, charT ());

    tally.check (ours == libc, facet, std::string (fmt) + what,
                 show (ours), show (libc));
}


template <class charT>
static void
check_time_put (tally_t& tally, const std::locale& loc)
{
    const std::string facet =
        std::string ("time_put<") + char_name (charT ()) + '>';

    // Wednesday 2026-10-28 21:05:09
    std::tm t = std::tm ();
    t.tm_year = 2026 - 1900;
    t.tm_mon  = 9;
    t.tm_mday = 28;
    t.tm_hour = 21;
    t.tm_min  = 5;
    t.tm_sec  = 9;
    t.tm_wday = 3;
    t.tm_yday = 300;

    for (int wday = 0; wday != 7; ++wday) {
        std::tm d = t;
        d.tm_wday = wday;

        char what [16];
        std::sprintf (what, " (wday %d)", wday);

        check_put<charT> (tally, loc, facet, d, "%A", what);
        check_put<charT> (tally, loc, facet, d, "%a", what);
    }

    for (int mon = 0; mon != 12; ++mon) {
        std::tm d = t;
        d.tm_mon = mon;

        char what [16];
        std::sprintf (what, " (mon %d)", mon);

        check_put<charT> (tally, loc, facet, d, "%B", what);
        check_put<charT> (tally, loc, facet, d, "%b", what);
    }

    std::tm am = t;
    am.tm_hour = 9;

    check_put<charT> (tally, loc, facet, am, "%p", " (hour 9)");
    check_put<charT> (tally, loc, facet, t, "%p", " (hour 21)");

    check_put<charT> (tally, loc, facet, t, "%x", "");
    check_put<charT> (tally, loc, facet, t, "%X", "");
    check_put<charT> (tally, loc, facet, t, "%c", "");
}


int main (int argc, char* argv [])
{
    const char* libc_name = 0;
    bool        list      = false;

    int argi = 1;
    for (; argi < argc && '-' == argv [argi][0]; ++argi) {
        if (0 == std::strcmp (argv [argi], "-u"))
            list = true;
        else if (argi + 1 < argc && 0 == std::strcmp (argv [argi], "-c"))
            libc_name = argv [++argi];
        else
            break;
    }

    if (argc != argi + 1) {
        std::fprintf (stderr, "usage: %s [-c name] [-u] <locale-name>\n",
                      argv [0]);
        return 2;
    }

    const char* const name = argv [argi];

    if (0 == libc_name)
        libc_name = name;

    // %c names the time zone in some locales, the library's from TZ
    // alone and the C library's from the host's when TZ is unset
    static char tz [] = "TZ=UTC0";
    putenv (tz);
    tzset ();

    if (0 == std::setlocale (LC_ALL, libc_name)) {
        std::fprintf (stderr, "setlocale (LC_ALL, \"%s\") failed\n",
                      libc_name);
        return 2;
    }

    lconv_t lc;
    read_lconv (lc);

    if (   !have_database (name, "LC_NUMERIC")
        || !have_database (name, "LC_MONETARY")
        || !have_database (name, "LC_TIME"))
        return 2;

    const std::locale loc (name);

    tally_t tally (name, list);

    check_numpunct<char> (tally, loc, lc);
    check_numpunct<wchar_t> (tally, loc, lc);

    check_moneypunct<char, false> (tally, loc, lc);
    check_moneypunct<char, true> (tally, loc, lc);
    check_moneypunct<wchar_t, false> (tally, loc, lc);
    check_moneypunct<wchar_t, true> (tally, loc, lc);

    check_time_put<char> (tally, loc);
    check_time_put<wchar_t> (tally, loc);

    std::printf ("%s: %lu items, %lu disagreements, %lu unrepresentable\n",
                 name, tally.items, tally.disagreements,
                 tally.unrepresentable);

    return tally.disagreements ? 1 : 0;
}
