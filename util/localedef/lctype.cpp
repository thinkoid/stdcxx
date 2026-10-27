/***************************************************************************
 *
 * lctype.cpp
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

// The semantics are those of glibc's ld-ctype.c, up to its
// ctype_finish(): that supplies the classes and maps the source leaves
// unset, adds to print and graph the members of the classes they
// imply, makes alnum of alpha and digit, and builds the tables, and
// that is the back end's to do.  A statement that fails is reported
// and skipped whole, as in the scalar categories; glibc keeps the
// members it read before the fault.
//
// Unlike the scalar categories, LC_CTYPE may go on after its copy:
// glibc reads the copied locale first, into the state of the locale
// that copies it, and the statements after the copy add to it.  An
// include in a translit_start section names a locale whose entries
// rank below the locale's own; glibc reads it after the category and
// folds it in at the end, here it is read where it is met.  Both give
// one order, since the includes are taken in the order they are read.
//
// Characters are code points, whether or not the charmap has them.
// glibc drops a transliteration that names a character the charmap
// lacks; here it is kept, and which entries a narrow encoding can use
// is the back end's to decide.

#include "lctype.h"
#include "scalar.h"   // for copy(), end_of_section(), not_read()


/**************************************************************************/
// the model

// Adds first to last to a class, merged with the runs it overlaps or
// touches.
static void
add_run (ucs_class_t &cls, long first, long last)
{
    // the first run that ends no earlier than first - 1
    std::size_t lo = 0;
    std::size_t hi = cls.size ();

    while (lo < hi) {

        const std::size_t mid = (lo + hi) / 2;

        if (cls [mid].last + 1 < first)
            lo = mid + 1;
        else
            hi = mid;
    }

    // past the last run that starts no later than last + 1
    std::size_t end = lo;

    while (end < cls.size () && cls [end].first <= last + 1)
        ++end;

    if (lo == end) {
        const ucs_range_t run = { first, last };
        cls.insert (cls.begin () + lo, run);
        return;
    }

    if (first < cls [lo].first)
        cls [lo].first = first;

    cls [lo].last = last < cls [end - 1].last ? cls [end - 1].last : last;

    cls.erase (cls.begin () + lo + 1, cls.begin () + end);
}


// Sets the pair of from in a map, replacing an earlier one.
static void
add_pair (ucs_map_t &map, long from, long to)
{
    std::size_t lo = 0;
    std::size_t hi = map.size ();

    while (lo < hi) {

        const std::size_t mid = (lo + hi) / 2;

        if (map [mid].from < from)
            lo = mid + 1;
        else
            hi = mid;
    }

    if (lo < map.size () && from == map [lo].from) {
        map [lo].to = to;
        return;
    }

    const ucs_pair_t pair = { from, to };
    map.insert (map.begin () + lo, pair);
}


// The class or the map a keyword names, 0 for one the model does not
// hold.
static field_t<ucs_class_t>*
class_of (keyword_t keyword, ctype_def_t &def)
{
    switch (keyword) {
    case kw_upper:  return &def.upper;
    case kw_lower:  return &def.lower;
    case kw_alpha:  return &def.alpha;
    case kw_digit:  return &def.digit;
    case kw_xdigit: return &def.xdigit;
    case kw_space:  return &def.space;
    case kw_print:  return &def.print;
    case kw_graph:  return &def.graph;
    case kw_blank:  return &def.blank;
    case kw_cntrl:  return &def.cntrl;
    case kw_punct:  return &def.punct;
    default:        return 0;
    }
}


static field_t<ucs_map_t>*
map_of (keyword_t keyword, ctype_def_t &def)
{
    switch (keyword) {
    case kw_toupper: return &def.toupper;
    case kw_tolower: return &def.tolower;
    default:         return 0;
    }
}


// The keyword of a class or a map named after class or map, by a word,
// a string or the keyword itself, as glibc compares the name with the
// names it knows.
static keyword_t
name_keyword (const token_t &name)
{
    static const struct {
        const char *name;
        keyword_t   keyword;
    } names [] = {
        { "upper",   kw_upper },   { "lower",   kw_lower },
        { "alpha",   kw_alpha },   { "digit",   kw_digit },
        { "xdigit",  kw_xdigit },  { "space",   kw_space },
        { "print",   kw_print },   { "graph",   kw_graph },
        { "blank",   kw_blank },   { "cntrl",   kw_cntrl },
        { "punct",   kw_punct },   { "alnum",   kw_alnum },
        { "toupper", kw_toupper }, { "tolower", kw_tolower }
    };

    if (tk_keyword == name.kind)
        return keyword_t (name.value);

    for (std::size_t i = 0; i != sizeof names / sizeof *names; ++i)
        if (name.text == names [i].name)
            return names [i].keyword;

    return kw_none;
}


/**************************************************************************/
// characters

// glibc's three kinds of character operand: tok_ucs4, a <Uxxxx> name or
// a character written raw; tok_bsymbol, any other name; tok_charcode.
enum operand_t {
    op_ucs,
    op_name,
    op_code,
    op_none
};


static operand_t
operand_of (const token_t &tok)
{
    switch (tok.kind) {
    case tk_symbol:   return tok.value < 0 ? op_name : op_ucs;
    case tk_raw:      return op_ucs;
    case tk_charcode: return op_code;
    default:          return op_none;
    }
}


// The code point of a name the charmap has, -1 otherwise.
static long
name_ucs (const charset_t &cs, const std::string &name)
{
    const cmap_entry_t* const entry = cs.cmap->find_name (name);

    return entry ? entry->ucs : -1;
}


// The code point of the character a byte code encodes, -1 when the
// charmap has none.
static long
bytes_ucs (const charset_t &cs, const std::string &bytes)
{
    const cmap_entry_t* const entry = cs.cmap->find_bytes (bytes);

    return entry ? entry->ucs : -1;
}


// One character, as glibc's get_character() reads it.  ucs is -1 for a
// character the charmap does not know, which glibc skips without a
// word.  glibc takes a name other than <Uxxxx> from a repertoire map,
// which none of its locales names, so that such a name gives no code
// point at all; here it is the charmap's, as it is in a string.
static bool
character (tok_iter &it, tok_iter last, const charset_t &cs, long &ucs,
           diag_t &diag)
{
    if (it == last)
        return diag.error (e_syntax, where_of (it, last),
                           "LC_CTYPE: expected a character");

    const token_t &tok = *it;

    std::string bytes;

    switch (operand_of (tok)) {

    case op_ucs:
        // an ill-formed raw character, which the lexer has reported
        if (tok.value < 0)
            return false;

        ucs = tok.value;
        break;

    case op_name:
        ucs = name_ucs (cs, tok.text);
        break;

    case op_code:
        decode_charcode (tok.text, bytes);
        ucs = bytes_ucs (cs, bytes);
        break;

    default:
        return diag.error (e_syntax, tok.where,
                           "LC_CTYPE: expected a character");
    }

    ++it;

    return true;
}


/**************************************************************************/
// the productions

// A production starts at the token it names and reads no further than
// the end of its statement.  It assigns the attribute only when what
// it reads is well-formed.  The charset is the one addition to the
// uniform signature, as in the scalar categories.

// \x41...\x5a: each byte sequence from the first to the last in turn,
// as glibc's charclass_charcode_ellipsis() steps through them.
static bool
code_range (tok_iter &it, tok_iter last, const charset_t &cs,
            ucs_class_t &attr, diag_t &diag)
{
    const token_t &from     = *it++;
    const token_t &ellipsis = *it++;

    if (it == last || tk_charcode != it->kind)
        return diag.error (e_syntax, where_of (it, last), "LC_CTYPE: "
                           "expected a character code to end the range");

    const token_t &to = *it++;

    if (tk_ellipsis3 != ellipsis.kind)
        return diag.error (e_pair, ellipsis.where, "with character code "
                           "range values one must use the absolute "
                           "ellipsis `...'");

    std::string bytes;
    std::string end;

    decode_charcode (from.text, bytes);
    decode_charcode (to.text, end);

    if (bytes.size () != end.size ())
        return diag.error (e_range, from.where, "start and end character "
                           "sequence of range must have the same length");

    for (std::size_t i = 0; i != bytes.size (); ++i) {

        const unsigned char a = bytes [i];
        const unsigned char b = end [i];

        if (b < a)
            return diag.error (e_range, from.where, "to-value character "
                               "sequence is smaller than from-value "
                               "sequence");
        if (a < b)
            break;
    }

    ucs_class_t value;

    for (;;) {

        const long ucs = bytes_ucs (cs, bytes);

        if (0 <= ucs)
            add_run (value, ucs, ucs);

        if (bytes == end)
            break;

        for (std::size_t i = bytes.size (); i-- != 0; )
            if (0 != (bytes [i] = char ((bytes [i] + 1) & 0xff)))
                break;
    }

    attr = value;

    return true;
}


// first ellipsis last, by glibc's three rules.  Between <Uxxxx> names
// or raw characters the ellipsis is .. or ..(2).., and the range is the
// code points from first to last.  Between two other names it is any
// ellipsis but ..., and the members are the names name_range() gives.
// Between byte codes it is ..., and code_range() steps through them.
// glibc reports a wrong ellipsis and takes the range all the same;
// here the statement fails, as every statement with an error does.
static bool
char_range (tok_iter &it, tok_iter last, const charset_t &cs,
            ucs_class_t &attr, diag_t &diag)
{
    const token_t &from     = *it;
    const token_t &ellipsis = *(it + 1);

    if (it + 2 == last)
        return diag.error (e_syntax, ellipsis.where, "LC_CTYPE: expected a "
                           "character after the ellipsis");

    const token_t   &to   = *(it + 2);
    const operand_t  kind = operand_of (from);

    if (kind != operand_of (to) || op_none == kind)
        return diag.error (e_pair, from.where, "ellipsis range must be "
                           "marked by two operands of same type");

    if (op_code == kind)
        return code_range (it, last, cs, attr, diag);

    name_range_t range;

    if (tk_symbol == from.kind && tk_symbol == to.kind) {

        if (!name_range (it, last, range, diag))
            return false;
    }
    else {

        // a raw character at either end, glibc's tok_ucs4: the lexer
        // has reported an ill-formed one
        if (from.value < 0 || to.value < 0)
            return false;

        if (to.value < from.value)
            return diag.error (e_range, from.where, "to-value of range is "
                               "smaller than from-value");

        range.ellipsis = ellipsis.kind;
        range.ucs      = true;
        range.first    = (unsigned long)from.value;
        range.last     = (unsigned long)to.value;
        range.step     = tk_ellipsis2_2 == ellipsis.kind ? 2 : 1;
        it            += 3;
    }

    ucs_class_t value;

    if (range.ucs) {

        if (tk_ellipsis2 != range.ellipsis && tk_ellipsis2_2 != range.ellipsis)
            return diag.error (e_pair, ellipsis.where, "with UCS range "
                               "values one must use the hexadecimal "
                               "symbolic ellipsis `..'");

        if (1 == range.step)
            add_run (value, long (range.first), long (range.last));
        else
            for (unsigned long n = range.first; n <= range.last; n += 2)
                add_run (value, long (n), long (n));
    }
    else {

        if (tk_ellipsis3 == range.ellipsis)
            return diag.error (e_pair, ellipsis.where, "with symbolic name "
                               "range values the absolute ellipsis `...' "
                               "must not be used");

        for (unsigned long n = range.first; n <= range.last; n += range.step) {

            const long ucs = name_ucs (cs, range_member (range, n));

            if (0 <= ucs)
                add_run (value, ucs, ucs);
        }
    }

    attr = value;

    return true;
}


// operand;operand;...: each a character or a range of them.  The list
// may be empty and may end in a semicolon, as in glibc.
static bool
members (tok_iter &it, tok_iter last, const charset_t &cs,
         ucs_class_t &attr, diag_t &diag)
{
    ucs_class_t value;

    while (it != last) {

        if (it + 1 != last && is_ellipsis ((it + 1)->kind)) {

            ucs_class_t range;

            if (!char_range (it, last, cs, range, diag))
                return false;

            for (std::size_t i = 0; i != range.size (); ++i)
                add_run (value, range [i].first, range [i].last);
        }
        else {

            long ucs;

            if (!character (it, last, cs, ucs, diag))
                return false;

            if (0 <= ucs)
                add_run (value, ucs, ucs);
        }

        if (it == last)
            break;

        if (tk_semicolon != it->kind)
            return diag.error (e_syntax, it->where, "LC_CTYPE: expected "
                               "`;' between the members of a class");
        ++it;
    }

    attr = value;

    return true;
}


// The members of a class statement add to those of the statements
// before it, the copied locale's among them, as in glibc.
static void
add_class (field_t<ucs_class_t> &attr, const ucs_class_t &value)
{
    for (std::size_t i = 0; i != value.size (); ++i)
        add_run (attr.value, value [i].first, value [i].last);

    attr.set = true;
}


// keyword operand;operand;...
static bool
class_value (tok_iter &it, tok_iter last, const charset_t &cs,
             field_t<ucs_class_t> &attr, diag_t &diag)
{
    ++it;

    ucs_class_t value;

    if (!members (it, last, cs, value, diag))
        return false;

    add_class (attr, value);

    return true;
}


// (from,to);(from,to)...: the pairs of a case map named name.  The
// semicolons are optional, as in glibc, and so is a last one.  A pair
// with a character the charmap does not know is skipped.  glibc reads
// each map once, so a map the copied locale defines may not be defined
// again.
static bool
map_pairs (tok_iter &it, tok_iter last, const charset_t &cs,
           const token_t &name, field_t<ucs_map_t> &attr, diag_t &diag)
{
    if (attr.set)
        return diag.error (e_syntax, name.where, "duplicated definition "
                           "for mapping `%s'", name.text.c_str ());

    ucs_map_t value;

    while (it != last) {

        if (tk_open != it->kind)
            return diag.error (e_syntax, it->where,
                               "LC_CTYPE: expected `(' to open a pair");
        ++it;

        long from;
        long to;

        if (!character (it, last, cs, from, diag))
            return false;

        if (it == last || tk_comma != it->kind)
            return diag.error (e_syntax, where_of (it, last),
                               "LC_CTYPE: expected `,' in a pair");
        ++it;

        if (!character (it, last, cs, to, diag))
            return false;

        if (it == last || tk_close != it->kind)
            return diag.error (e_syntax, where_of (it, last),
                               "LC_CTYPE: expected `)' to close a pair");
        ++it;

        if (0 <= from && 0 <= to)
            add_pair (value, from, to);

        if (it != last && tk_semicolon == it->kind)
            ++it;
    }

    attr.value = value;
    attr.set   = true;

    return true;
}


// keyword (from,to);...
static bool
map_value (tok_iter &it, tok_iter last, const charset_t &cs,
           field_t<ucs_map_t> &attr, diag_t &diag)
{
    const token_t &lead = *it++;

    return map_pairs (it, last, cs, lead, attr, diag);
}


// class name; or map name;, at the keyword: the name a word, a string
// or a keyword.
static bool
name_head (tok_iter &it, tok_iter last, token_t &attr, diag_t &diag)
{
    const token_t &lead = *it++;

    if (   it == last
        || (tk_ident != it->kind && tk_string != it->kind
            && tk_keyword != it->kind))
        return diag.error (e_syntax, where_of (it, last), "LC_CTYPE: `%s' "
                           "takes a name", lead.text.c_str ());

    const token_t &name = *it++;

    if (it == last || tk_semicolon != it->kind)
        return diag.error (e_syntax, where_of (it, last), "LC_CTYPE: "
                           "expected `;' after the name");
    ++it;

    attr = name;

    return true;
}


// class name; operand;...  A class the model holds reads into it.  Any
// other name is declared, and its statement skipped unread; alnum is
// alpha and digit.
static bool
class_named (tok_iter &it, tok_iter last, const charset_t &cs,
             ctype_state_t &state, diag_t &diag)
{
    token_t name;

    if (!name_head (it, last, name, diag))
        return false;

    const keyword_t keyword = name_keyword (name);

    field_t<ucs_class_t>* const attr = class_of (keyword, *state.def);

    if (0 == attr) {
        if (kw_none == keyword)
            state.names.insert (name.text);
        return true;
    }

    ucs_class_t value;

    if (!members (it, last, cs, value, diag))
        return false;

    add_class (*attr, value);

    return true;
}


// map name; (from,to);...  toupper and tolower read into the model; any
// other name is declared, and its statement skipped unread.
static bool
map_named (tok_iter &it, tok_iter last, const charset_t &cs,
           ctype_state_t &state, diag_t &diag)
{
    token_t name;

    if (!name_head (it, last, name, diag))
        return false;

    const keyword_t keyword = name_keyword (name);

    field_t<ucs_map_t>* const attr = map_of (keyword, *state.def);

    if (0 == attr) {
        if (kw_none == keyword)
            state.names.insert (name.text);
        return true;
    }

    return map_pairs (it, last, cs, name, *attr, diag);
}


// charclass name;name;... and charconv name;name;...: the names of
// classes and maps no facet reads.  They are declared so that their
// statements are skipped without a word.
static bool
declare (tok_iter &it, tok_iter last, std::set<std::string> &attr,
         diag_t &diag)
{
    const token_t &lead = *it++;

    std::vector<std::string> value;

    while (it != last && (tk_ident == it->kind || tk_string == it->kind)) {

        value.push_back (it++->text);

        if (it == last || tk_semicolon != it->kind)
            break;

        ++it;
    }

    if (it != last)
        return diag.error (e_syntax, it->where, "LC_CTYPE: syntax error in "
                           "`%s'", lead.text.c_str ());

    attr.insert (value.begin (), value.end ());

    return true;
}


// One operand of a transliteration, as glibc's read_widestring() takes
// it: a character as character() reads it, a string, or
// default_missing, which stands for the empty string.  usable is false
// for an operand glibc cannot use: a character the charmap does not
// know, or an empty string.
static bool
translit_operand (tok_iter &it, tok_iter last, const charset_t &cs,
                  ucs_string_t &attr, bool &usable, diag_t &diag)
{
    attr.clear ();
    usable = true;

    if (it != last && tk_string == it->kind) {

        if (!resolve (*it++, cs, attr, diag))
            return false;

        usable = !attr.empty ();
        return true;
    }

    if (it != last && kw_default_missing == keyword_of (*it)) {
        ++it;
        return true;
    }

    long ucs;

    if (!character (it, last, cs, ucs, diag))
        return false;

    if (ucs < 0)
        usable = false;
    else
        attr.push_back (ucs);

    return true;
}


// source alternative;alternative;...  The source is one operand, so
// that <U0417><U0413> "ZGH" is the source <U0417> and the alternative
// <U0413>ZGH, as glibc reads it.  An alternative is the operands up to
// the next semicolon, joined, and there is at least one.  As in glibc,
// an alternative with an operand glibc cannot use is dropped, an empty
// one between two semicolons or after the last is the empty string,
// and the entry stays with no alternative left.  glibc keeps the
// operands of a dropped alternative before the one it cannot use, and
// they start the next alternative; here the dropped one goes whole.
// A source glibc cannot use drops the entry: the attribute's source is
// then empty.  glibc goes on to read the rest of that line as another
// entry; here the line is the entry.
static bool
translit_entry (tok_iter &it, tok_iter last, const charset_t &cs,
                translit_entry_t &attr, diag_t &diag)
{
    translit_entry_t value;

    bool usable;

    if (!translit_operand (it, last, cs, value.from, usable, diag))
        return false;

    if (!usable)
        value.from.clear ();

    if (it == last || tk_semicolon == it->kind)
        return diag.error (e_syntax, where_of (it, last), "LC_CTYPE: "
                           "expected a transliteration");

    ucs_string_t alternative;

    bool keep = true;

    for (;;) {

        if (it == last || tk_semicolon == it->kind) {

            if (keep)
                value.to.push_back (alternative);

            if (it == last)
                break;

            ++it;

            alternative.clear ();
            keep = true;
            continue;
        }

        ucs_string_t operand;

        if (!translit_operand (it, last, cs, operand, usable, diag))
            return false;

        keep = keep && usable;
        alternative.insert (alternative.end (), operand.begin (),
                            operand.end ());
    }

    attr = value;

    return true;
}


// include "name";"repertoire", at the keyword: the name of a locale in
// the portable characters, a string or a word.  The repertoire map is
// not read: the code points come from the <Uxxxx> names.
static bool
include_name (tok_iter &it, tok_iter last, const charset_t &cs,
              std::string &attr, diag_t &diag)
{
    const token_t &lead = *it++;

    std::string name;

    if (it != last && tk_ident == it->kind)
        name = it++->text;
    else if (it != last && tk_string == it->kind) {

        ucs_string_t ucs;

        if (!resolve (*it++, cs, ucs, diag))
            return false;

        for (std::size_t i = 0; i != ucs.size (); ++i) {

            if (ucs [i] <= 0 || 0x7f < ucs [i])
                return diag.error (e_inval, lead.where, "locale name "
                                   "should consist only of portable "
                                   "characters");

            name += char (ucs [i]);
        }
    }
    else
        return diag.error (e_syntax, where_of (it, last), "LC_CTYPE: "
                           "`include' takes a locale name");

    if (it == last || tk_semicolon != it->kind)
        return diag.error (e_syntax, where_of (it, last), "LC_CTYPE: "
                           "expected `;' after the locale name");
    ++it;

    if (it == last || (tk_string != it->kind && tk_ident != it->kind))
        return diag.error (e_syntax, where_of (it, last), "LC_CTYPE: "
                           "`include' takes a repertoire name");
    ++it;

    if (!end_of_line (it, last, diag))
        return false;

    if (name.empty ())
        return diag.error (e_inval, lead.where, "empty locale name");

    attr = name;

    return true;
}


/**************************************************************************/
// transliteration

// The entries of a state in the order of glibc's translit_flatten():
// those its includes bring, then its own from the last read to the
// first.  glibc sorts them by source and keeps the last for each, so
// that the locale's own entries win over the included ones, the first
// of two own entries wins, and of two includes the later.
static void
flatten (const ctype_state_t &state, std::vector<translit_entry_t> &list)
{
    list.insert (list.end (), state.included.begin (), state.included.end ());
    list.insert (list.end (), state.own.rbegin (), state.own.rend ());
}


// include: the locale's entries, read for its transliteration alone,
// rank below those of the locale that includes it.  glibc takes each
// locale once, at its first include in its depth-first walk; reading
// the includes in the order met is that walk.
static void
include (const std::string &name, const token_t &lead, ctype_state_t &state,
         ctype_locales_t &locales)
{
    if (!state.visited.insert (name).second)
        return;

    ctype_state_t other (0, state.visited);

    locales.read (name, lead, other);

    flatten (other, state.included);
}


void
finish_translit (const ctype_state_t &state, translit_t &table)
{
    std::vector<translit_entry_t> list;

    flatten (state, list);

    for (std::size_t i = 0; i != list.size (); ++i)
        table [list [i].from] = list [i].to;
}


/**************************************************************************/
// the category

// One statement outside translit_start, in a state that holds classes
// and maps.  The keywords plan-localedef.md reads and ignores are
// matched and their statements skipped unread.
static void
statement (tok_iter it, tok_iter last, const charset_t &cs,
           ctype_state_t &state, diag_t &diag)
{
    const keyword_t keyword = keyword_of (*it);

    switch (keyword) {

    case kw_upper: case kw_lower: case kw_alpha: case kw_digit:
    case kw_xdigit: case kw_space: case kw_print: case kw_graph:
    case kw_blank: case kw_cntrl: case kw_punct:
        class_value (it, last, cs, *class_of (keyword, *state.def), diag);
        break;

    case kw_class:
        class_named (it, last, cs, state, diag);
        break;

    case kw_toupper: case kw_tolower:
        map_value (it, last, cs, *map_of (keyword, *state.def), diag);
        break;

    case kw_map:
        map_named (it, last, cs, state, diag);
        break;

    case kw_charclass: case kw_charconv:
        declare (it, last, state.names, diag);
        break;

    case kw_alnum: case kw_outdigit:
        // no facet reads them; alnum is alpha and digit
        break;

    default:
        // a class or a map declared is skipped unread
        if (tk_ident != it->kind || 0 == state.names.count (it->text))
            not_read (*it, "LC_CTYPE", diag);
    }
}


// One line of a translit_start section.  False at translit_end.
static bool
translit_statement (tok_iter it, tok_iter last, const charset_t &cs,
                    ctype_state_t &state, ctype_locales_t &locales,
                    diag_t &diag)
{
    const token_t &lead = *it;

    switch (keyword_of (lead)) {

    case kw_translit_end:
        end_of_line (++it, last, diag);
        return false;

    case kw_include: {
        std::string name;

        if (include_name (it, last, cs, name, diag))
            include (name, lead, state, locales);
        break;
    }

    case kw_default_missing: case kw_translit_ignore:
        // read and ignored: the runtime reads no transliteration
        break;

    default: {
        translit_entry_t entry;

        if (translit_entry (it, last, cs, entry, diag) && !entry.from.empty ())
            state.own.push_back (entry);
    }
    }

    return true;
}


void
read_ctype (reader_t &reader, const line_t &head, const cmap_t &cmap,
            ctype_state_t &state, ctype_locales_t &locales, diag_t &diag)
{
    const token_t     &name  = head.tokens.front ();
    const std::size_t  depth = reader.depth ();

    where_t at_end   = name.where;
    where_t at_start = name.where;
    bool    first    = true;
    bool    translit = false;

    for (line_t line; reader.next (line, depth); first = false) {

        tok_iter        it   = line.tokens.begin ();
        const tok_iter  last = line.tokens.end ();
        const charset_t cs   = { &cmap, line.escape };

        const token_t   &lead    = *it;
        const keyword_t  keyword = keyword_of (lead);

        at_end = lead.where;

        // glibc reads END inside a translit_start section as an entry;
        // here it ends the section and the category
        if (kw_end == keyword) {

            if (translit)
                diag.error (e_syntax, at_start, "LC_CTYPE: `translit_start' "
                            "section does not end with `translit_end'");

            end_of_section (it, last, name, diag);
            return;
        }

        if (translit) {
            translit = translit_statement (it, last, cs, state, locales,
                                           diag);
            continue;
        }

        if (kw_translit_start == keyword) {
            end_of_line (++it, last, diag);
            translit = true;
            at_start = lead.where;
            continue;
        }

        // glibc skips the rest of the category when the copied locale
        // cannot be read; here the statements after the copy are read
        if (kw_copy == keyword) {

            std::string copied;

            if (!first)
                diag.error (e_syntax, lead.where, "LC_CTYPE: `copy' must be "
                            "the first statement of the category");
            else if (copy (it, last, cs, copied, diag))
                locales.read (copied, lead, state);

            continue;
        }

        // a locale read for its transliteration skips the rest unread
        if (state.def)
            statement (it, last, cs, state, diag);
    }

    diag.error (e_syntax, at_end, "%s: premature end of file",
                name.text.c_str ());
}
