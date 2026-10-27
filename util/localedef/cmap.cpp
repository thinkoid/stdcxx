/***************************************************************************
 *
 * cmap.cpp
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

#include "cmap.h"
#include "reader.h"

#include <cstdio>    // for sprintf()
#include <cstdlib>   // for strtoul()
#include <utility>   // for make_pair()


struct char_def_t {
    std::string  from;
    std::string  to;         // empty for a single character
    token_kind_t ellipsis;   // of a range
    std::string  bytes;
    where_t      where;
};

// One line of the WIDTH section.  No facet reads a width, and the
// database has no field for one, so the widths are checked and dropped.
struct width_def_t {
    std::string from;
    std::string to;          // empty for a single character
    long        width;
};


/**************************************************************************/
// names

static bool
is_hex_digits (const std::string &s, std::size_t pos)
{
    for (; pos != s.size (); ++pos) {

        const char c = s [pos];

        const bool hex =
               ('0' <= c && c <= '9')
            || ('a' <= c && c <= 'f') || ('A' <= c && c <= 'F');

        if (!hex)
            return false;
    }

    return true;
}


// The code point a name gives: U or P and 4 or 8 hexadecimal digits,
// below 0x80000000, as glibc's charmap_new_char() reads it.  P names
// the private area; no charmap uses it.  -1 for any other name.
static long
name_ucs (const std::string &name)
{
    const bool form =
           (5 == name.size () || 9 == name.size ())
        && ('U' == name [0] || 'P' == name [0])
        && is_hex_digits (name, 1);

    if (!form)
        return -1;

    const unsigned long ucs = std::strtoul (name.c_str () + 1, 0, 16);

    return ucs < 0x80000000UL ? long (ucs) : -1;
}


// glibc spells every <Uxxxx> name as U and 8 digits before it looks
// one up, so <U0041> and <U00000041> are one name.
static std::string
name_key (const std::string &name)
{
    const long ucs = name_ucs (name);

    if (ucs < 0 || 'U' != name [0])
        return name;

    char key [24];
    std::sprintf (key, "U%08lX", ucs);

    return key;
}


static bool
is_utf8 (const std::string &code_set_name)
{
    static const char utf8 [] = "utf-8";

    if (sizeof utf8 - 1 != code_set_name.size ())
        return false;

    for (std::size_t i = 0; i != code_set_name.size (); ++i) {

        const char c = code_set_name [i];

        if (('A' <= c && c <= 'Z' ? c | 0x20 : c) != utf8 [i])
            return false;
    }

    return true;
}


/**************************************************************************/
// the productions

// <code_set_name> name
static bool
code_set (tok_iter &it, tok_iter last, std::string &attr, diag_t &diag)
{
    if (it == last || (tk_ident != it->kind && tk_string != it->kind))
        return diag.error (e_syntax, where_of (it, last),
                           "syntax error in prolog: bad argument");

    attr = it++->text;

    return true;
}


// <mb_cur_max> n and <mb_cur_min> n
static bool
byte_count (tok_iter &it, tok_iter last, int &attr, diag_t &diag)
{
    if (it == last || tk_number != it->kind)
        return diag.error (e_syntax, where_of (it, last),
                           "syntax error in prolog: bad argument");

    if (it->value < 1)
        return diag.error (e_inval, it->where,
                           "a count of bytes must be 1 or greater");

    attr = int (it++->value);

    return true;
}


// END name
static bool
end_of_section (tok_iter &it, tok_iter last, keyword_t keyword,
                const char *name, diag_t &diag)
{
    if (it == last || tk_keyword != it->kind || keyword != it->value)
        return diag.error (e_syntax, where_of (it, last),
                           "definition does not end with `END %s'", name);

    return end_of_line (++it, last, diag);
}


// the escaped byte form of a character
static bool
encoding (tok_iter &it, tok_iter last, std::string &attr, diag_t &diag)
{
    if (   it == last || tk_charcode != it->kind
        || !decode_charcode (it->text, attr))
        return diag.error (e_syntax, where_of (it, last),
                           "syntax error in CHARMAP definition: "
                           "invalid encoding given");

    ++it;

    return true;
}


// <name> encoding, or <from>..<to> encoding; the rest of the line is a
// comment.  The leading token is a symbol.
static bool
char_def (tok_iter &it, tok_iter last, char_def_t &attr,
          diag_t &diag)
{
    attr.where    = it->where;
    attr.from     = it++->text;
    attr.ellipsis = tk_ident;
    attr.to.clear ();

    if (it != last && is_ellipsis (it->kind)) {

        attr.ellipsis = it++->kind;

        if (it == last || tk_symbol != it->kind)
            return diag.error (e_syntax, where_of (it, last),
                               "syntax error in CHARMAP definition: no "
                               "symbolic name given for end of range");

        attr.to = it++->text;
    }

    return encoding (it, last, attr.bytes, diag);
}


// <name> width, or <from>...<to> width
static bool
width_def (tok_iter &it, tok_iter last, width_def_t &attr, diag_t &diag)
{
    attr.from = it++->text;
    attr.to.clear ();

    if (it != last && tk_ellipsis3 == it->kind) {

        ++it;

        if (it == last || tk_symbol != it->kind)
            return diag.error (e_syntax, where_of (it, last),
                               "syntax error in WIDTH definition: no "
                               "symbolic name given for end of range");

        attr.to = it++->text;
    }

    if (it == last || tk_number != it->kind)
        return diag.error (e_syntax, where_of (it, last),
                           "value for WIDTH must be an integer");

    attr.width = it++->value;

    return end_of_line (it, last, diag);
}


/**************************************************************************/
// cmap_t

cmap_t::
cmap_t ()
    : mb_cur_max (0), mb_cur_min (0)
{
}


const cmap_entry_t* cmap_t::
find_name (const std::string &name) const
{
    const std::map<std::string, std::size_t>::const_iterator it =
        m_by_name.find (name_key (name));

    return it == m_by_name.end () ? 0 : &m_entries [it->second];
}


const cmap_entry_t* cmap_t::
find_ucs (long ucs) const
{
    const std::map<long, std::size_t>::const_iterator it =
        m_by_ucs.find (ucs);

    return it == m_by_ucs.end () ? 0 : &m_entries [it->second];
}


const cmap_entry_t* cmap_t::
find_bytes (const std::string &bytes) const
{
    const std::map<std::string, std::size_t>::const_iterator it =
        m_by_bytes.find (bytes);

    return it == m_by_bytes.end () ? 0 : &m_entries [it->second];
}


// A name defined twice keeps its first definition, as in glibc, where
// "all later definitions are simply discarded".  glibc still indexes
// the later bytes; here the later line is dropped whole.
void cmap_t::
add (const std::string &name, const std::string &bytes)
{
    const std::string key = name_key (name);

    if (m_by_name.count (key))
        return;

    cmap_entry_t entry;

    entry.name  = name;
    entry.ucs   = name_ucs (name);
    entry.bytes = bytes;

    long ucs;

    if (   entry.ucs < 0 && is_utf8 (code_set_name)
        && scan_utf8 (bytes, 0, ucs) == bytes.size ())
        entry.ucs = ucs;

    const std::size_t index = m_entries.size ();

    m_entries.push_back (entry);

    m_by_name [key] = index;
    m_by_bytes.insert (std::make_pair (bytes, index));

    if (0 <= entry.ucs)
        m_by_ucs.insert (std::make_pair (entry.ucs, index));
}


// glibc's rule for the bytes of a range: the encoding is an integer of
// as many base-256 digits as it has bytes, and each member's is one
// more than the one before, the carry propagating into the bytes
// before the last.
static bool
increment (std::string &bytes)
{
    for (std::size_t i = bytes.size (); i-- != 0; ) {

        const unsigned char byte = bytes [i];

        bytes [i] = char ((byte + 1) & 0xff);

        if (0xff != byte)
            return true;
    }

    return false;
}


// The UTF-8 form of a code point, up to the six bytes of the old
// definition, which the tree's UTF-8 charmap still uses.
static std::string
utf8_of (long ucs)
{
    std::string bytes;

    if (ucs < 0x80) {
        bytes += char (ucs);
        return bytes;
    }

    int  len  = 2;
    long lead = 0xc0;

    for (unsigned long top = 0x800; top <= (unsigned long)ucs && len < 6;
         top <<= 5) {
        ++len;
        lead = (lead >> 1) | 0x80;
    }

    bytes.resize (len);

    for (int i = len - 1; 0 < i; --i, ucs >>= 6)
        bytes [i] = char (0x80 | (ucs & 0x3f));

    bytes [0] = char (lead | ucs);

    return bytes;
}


// A range names its members by a prefix and a number, hexadecimal for
// .. and ..(2).., decimal for ... and ...., as glibc's
// charmap_new_char() reads them.  glibc reads ..(2).. as decimal, by a
// test that compares the ellipsis before the step is taken out of it;
// no charmap writes one.  A member's code point comes from its own
// name, never from its bytes.
//
// In a UTF-8 charmap a member's bytes are the UTF-8 form of that code
// point.  glibc's increment knows nothing of UTF-8: past a multiple of
// 64 it makes bytes that are not UTF-8, and glibc's own UTF-8 charmap
// cuts 349 runs across one, 13199 members past it.  That is where the
// old compiler, which took the code point from those bytes, lost 135
// code points.  Every other charmap keeps glibc's increment.
bool cmap_t::
define (const char_def_t &def, diag_t &diag)
{
    if (int (def.bytes.size ()) < mb_cur_min)
        return diag.error (e_mbchar, def.where,
                           "too few bytes in character encoding");

    if (mb_cur_max < int (def.bytes.size ()))
        return diag.error (e_mbchar, def.where,
                           "too many bytes in character encoding");

    if (def.to.empty ()) {
        add (def.from, def.bytes);
        return true;
    }

    const bool hex  = tk_ellipsis2 == def.ellipsis
                   || tk_ellipsis2_2 == def.ellipsis;
    const int  step = tk_ellipsis2_2 == def.ellipsis
                   || tk_ellipsis4_2 == def.ellipsis ? 2 : 1;

    const std::string &from = def.from;
    const std::string &to   = def.to;

    std::size_t prefix = from.size ();

    for (bool lower = false; prefix; --prefix) {

        const char c = from [prefix - 1];

        if ('0' <= c && c <= '9')
            continue;

        if (hex && 'A' <= c && c <= 'F')
            continue;

        if (hex && 'a' <= c && c <= 'f') {
            if (!lower)
                diag.error (e_pair, def.where, "hexadecimal range format "
                            "should use only capital characters");
            lower = true;
            continue;
        }

        break;
    }

    const std::size_t digits = from.size () - prefix;

    if (   from.size () != to.size () || 0 == digits || 8 < digits
        || 0 != from.compare (0, prefix, to, 0, prefix))
        return diag.error (e_pair, def.where,
                           "invalid names for character range");

    char *end = 0;

    const unsigned long first =
        std::strtoul (from.c_str () + prefix, &end, hex ? 16 : 10);

    const unsigned long last =
        std::strtoul (to.c_str () + prefix, &end, hex ? 16 : 10);

    if (*end)
        return diag.error (e_pair, def.where,
                           "<%s> and <%s> are invalid names for range",
                           from.c_str (), to.c_str ());

    if (last < first)
        return diag.error (e_range, def.where, "upper limit in range "
                           "is smaller than lower limit");

    // glibc increments once more after the last member, and reports a
    // range that ends at the top of its bytes as not representable;
    // here the increment is only between members.
    const bool  utf8  = is_utf8 (code_set_name);
    std::string bytes = def.bytes;

    for (unsigned long n = first; n <= last; n += step) {

        char number [24];
        std::sprintf (number, hex ? "%0*lX" : "%0*lu", int (digits), n);

        const std::string name = from.substr (0, prefix) + number;
        const long        ucs  = name_ucs (name);

        if (utf8 && 0 <= ucs) {
            add (name, utf8_of (ucs));
            continue;
        }

        add (name, bytes);

        if (n + step <= last && !increment (bytes))
            return diag.error (e_range, def.where,
                               "resulting bytes for range "
                               "not representable");
    }

    return true;
}


bool cmap_t::
read (const std::string &path, diag_t &diag)
{
    enum section_t {
        prolog, charmap, after, width, width_variable
    };

    const int errors = diag.errors ();

    reader_t reader (diag);
    reader.push (path, charmap_file);

    section_t section = prolog;
    where_t   at_end  = { 0, 0, 0 };

    for (line_t line; reader.next (line); ) {

        tok_iter       it   = line.tokens.begin ();
        const tok_iter last = line.tokens.end ();

        const token_t &lead    = *it;
        const bool     keyword = tk_keyword == lead.kind;

        at_end = lead.where;

        switch (section) {

        case prolog:

            ++it;

            switch (keyword ? lead.value : long (kw_none)) {

            case kw_code_set_name:
                code_set (it, last, code_set_name, diag)
                    && end_of_line (it, last, diag);
                break;

            case kw_mb_cur_max:
                byte_count (it, last, mb_cur_max, diag)
                    && end_of_line (it, last, diag);
                break;

            case kw_mb_cur_min:
                byte_count (it, last, mb_cur_min, diag)
                    && end_of_line (it, last, diag);
                break;

            case kw_g0esc: case kw_g1esc: case kw_g2esc: case kw_g3esc:
            case kw_escseq:
                // glibc ignores the ISO 2022 declarations
                break;

            case kw_include:
                diag.error (e_syntax, lead.where, "character sets with "
                            "locking states are not supported");
                break;

            case kw_charmap:
                if (0 == mb_cur_max)
                    mb_cur_max = 1;

                if (0 == mb_cur_min)
                    mb_cur_min = mb_cur_max;

                if (mb_cur_max < mb_cur_min) {
                    diag.error (e_inval, lead.where, "<mb_cur_max> must "
                                "be greater than <mb_cur_min>");
                    mb_cur_min = mb_cur_max;
                }

                end_of_line (it, last, diag);
                section = charmap;
                break;

            default:
                diag.error (e_syntax, lead.where,
                            "syntax error in prolog: invalid definition");
            }
            break;

        case charmap:

            if (keyword && kw_end == lead.value) {
                end_of_section (++it, last, kw_charmap, "CHARMAP", diag);
                section = after;
            }
            else if (tk_symbol == lead.kind) {
                char_def_t def;
                char_def (it, last, def, diag) && define (def, diag);
            }
            else
                diag.error (e_syntax, lead.where, "syntax error in "
                            "CHARMAP definition: no symbolic name given");
            break;

        case after:

            ++it;

            switch (keyword ? lead.value : long (kw_none)) {

            case kw_width_default:
                if (it == last || tk_number != it->kind)
                    diag.error (e_syntax, where_of (it, last),
                                "value for WIDTH_DEFAULT must be an integer");
                else
                    end_of_line (++it, last, diag);
                break;

            case kw_width_section:
                end_of_line (it, last, diag);
                section = width;
                break;

            case kw_width_variable:
                end_of_line (it, last, diag);
                section = width_variable;
                break;

            default:
                diag.error (e_syntax, lead.where, "only WIDTH definitions "
                            "are allowed to follow the CHARMAP definition");
            }
            break;

        case width:

            if (keyword && kw_end == lead.value) {
                end_of_section (++it, last, kw_width_section, "WIDTH", diag);
                section = after;
            }
            else if (tk_symbol == lead.kind) {

                width_def_t def;

                if (!width_def (it, last, def, diag))
                    break;

                const cmap_entry_t* const from = find_name (def.from);
                const cmap_entry_t* const to   =
                    def.to.empty () ? from : find_name (def.to);

                if (0 == from || 0 == to)
                    diag.error (e_inval, lead.where, "unknown character `%s'",
                                (from ? def.to : def.from).c_str ());
                else if (from->bytes.size () != to->bytes.size ())
                    diag.error (e_range, lead.where, "number of bytes for "
                                "byte sequence of beginning and end of "
                                "range not the same");
            }
            else
                diag.error (e_syntax, lead.where, "syntax error in WIDTH "
                            "definition: no symbolic name given");
            break;

        case width_variable:

            // glibc reads the names of WIDTH_VARIABLE and enters nothing
            if (keyword && kw_end == lead.value) {
                end_of_section (++it, last, kw_width_variable,
                                "WIDTH_VARIABLE", diag);
                section = after;
            }
            else if (tk_symbol != lead.kind)
                diag.error (e_syntax, lead.where, "syntax error in "
                            "WIDTH_VARIABLE definition: "
                            "no symbolic name given");
            break;
        }
    }

    if (after != section)
        diag.error (e_syntax, at_end, "premature end of file");

    return errors == diag.errors ();
}


/**************************************************************************/
// strings and names against the charmap

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


bool
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
