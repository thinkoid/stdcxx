/***************************************************************************
 *
 * lcollate.cpp
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

// The semantics are those of glibc's ld-collate.c, collate_read() and
// the functions it calls, up to collate_finish(): that resolves the
// names to weights and builds the tables, and that is the back end's
// to do.  A statement that fails is reported and skipped whole.
//
// glibc keeps one sequence of entries, a list with a cursor.  An entry
// goes in after the cursor and becomes the cursor; order_start does not
// move it, reorder-after sets it to the entry named, and an entry that
// is placed again is taken out and goes in at the cursor with its new
// weights.  A collating symbol is an entry like a character, so a
// reorder-after block may follow a bare symbol.  The list here is the
// same list, threaded through a vector by index, with a map from name
// to entry; placing an entry costs a lookup and four links.
//
// An ellipsis line stands for the characters between the entries
// before and after it.  glibc makes an entry of each when it reads the
// line after; here the range stays one entry, so that iso14651_t1's
// 20900 Han characters are one line of the model.  A range is split
// where a later statement names one of its members.
//
// LC_COLLATE may go on after its copy, as glibc reads it: the copied
// locale is read where the copy stands, into the same state, and the
// statements after it go on from where the copied locale left the
// cursor and the section.  A define before the copy is seen in the
// locale copied, since glibc keeps one list of names for the run.
//
// Characters are code points, whether or not the charmap has them, as
// in LC_CTYPE.  glibc takes a <Uxxxx> name the charmap lacks for a name
// nothing declares; here it stays the character, and which of them a
// narrow encoding can write is the back end's to decide.

#include "lcollate.h"
#include "scalar.h"   // for copy(), end_of_section(), not_read()

#include <algorithm>   // for find()
#include <cstdlib>     // for strtoul()


/**************************************************************************/
// names

// The name of a code point, as range_member() spells one.
static std::string
ucs_name (long ucs)
{
    name_range_t range = name_range_t ();

    range.ucs = true;

    return range_member (range, (unsigned long)ucs);
}


// The code point of a name of U and 4 or 8 hexadecimal digits, -1 for
// any other name, as the reader takes one.
static long
name_ucs (const std::string &name)
{
    if ((5 != name.size () && 9 != name.size ()) || 'U' != name [0])
        return -1;

    unsigned long ucs = 0;

    for (std::size_t i = 1; i != name.size (); ++i) {

        const char c = name [i];

        if ('0' <= c && c <= '9')
            ucs = ucs * 16 + (c - '0');
        else if ('A' <= (c & ~0x20) && (c & ~0x20) <= 'F')
            ucs = ucs * 16 + ((c & ~0x20) - 'A' + 10);
        else
            return -1;
    }

    // glibc's charmap takes no value from 0x80000000 on
    return ucs < 0x80000000UL ? long (ucs) : -1;
}


/**************************************************************************/
// the sequence

static int
file_index (collate_state_t &state, const char *file)
{
    const std::map<const char*, int>::const_iterator it =
        state.files.find (file);

    if (it != state.files.end ())
        return it->second;

    const int index = int (state.def.files.size ());

    state.def.files.push_back (file);
    state.files.insert (std::make_pair (file, index));

    return index;
}


// A node for entry, placed nowhere yet.
static int
new_node (collate_state_t &state, const coll_entry_t &entry, bool declared)
{
    coll_node_t node;

    node.entry    = entry;
    node.prev     = -1;
    node.next     = -1;
    node.linked   = false;
    node.declared = declared;

    state.nodes.push_back (node);

    return int (state.nodes.size () - 1);
}


// Links node in after the node after, or at the head when after is -1.
// glibc links an entry after a cursor it has lost to nothing, so that
// the entry is in no list; here it goes to the head.
static void
link_after (collate_state_t &state, int node, int after)
{
    coll_node_t &n = state.nodes [node];

    n.prev   = after;
    n.next   = after < 0 ? state.head : state.nodes [after].next;
    n.linked = true;

    if (0 <= n.next)
        state.nodes [n.next].prev = node;

    if (after < 0)
        state.head = node;
    else
        state.nodes [after].next = node;
}


static void
unlink (collate_state_t &state, int node)
{
    coll_node_t &n = state.nodes [node];

    if (0 <= n.prev)
        state.nodes [n.prev].next = n.next;
    else
        state.head = n.next;

    if (0 <= n.next)
        state.nodes [n.next].prev = n.prev;

    n.prev   = -1;
    n.next   = -1;
    n.linked = false;
}


// Places node at the cursor and makes it the cursor, in the current
// section, as glibc's insert_weights() does.
static void
place (collate_state_t &state, int node)
{
    link_after (state, node, state.cursor);

    state.cursor = node;

    state.nodes [node].entry.section = state.section;

    if (0 <= state.section)
        state.section_used [state.section] = true;
}


// The number of member name in a range, false when it is none.
static bool
range_number (const name_range_t &range, const std::string &name,
              unsigned long &n)
{
    if (range.ucs) {

        const long ucs = name_ucs (name);

        if (ucs < 0)
            return false;

        n = (unsigned long)ucs;
    }
    else {

        if (   name.size () != range.prefix.size () + range.width
            || 0 == range.width
            || 0 != name.compare (0, range.prefix.size (), range.prefix))
            return false;

        char *end = 0;

        n = std::strtoul (name.c_str () + range.prefix.size (), &end,
                          range.base);

        if (*end || range_member (range, n) != name)
            return false;
    }

    return range.first <= n && n <= range.last;
}


// The range node whose members include name, -1 when none does.
static int
find_member (const collate_state_t &state, const std::string &name,
             unsigned long &n)
{
    for (std::size_t i = 0; i != state.ranges.size (); ++i) {

        const coll_node_t &node = state.nodes [state.ranges [i]];

        if (node.linked && range_number (node.entry.range, name, n))
            return state.ranges [i];
    }

    return -1;
}


// Takes member n out of the range at node, as an entry of its own in
// its place with the range's weights, and returns it.  What is left of
// the range stays on either side.
static int
split_range (collate_state_t &state, int node, unsigned long n)
{
    const coll_entry_t range = state.nodes [node].entry;

    coll_entry_t member = range;

    member.kind  = ck_char;
    member.name  = range_member (range.range, n);
    member.ucs   = range.range.ucs ? long (n) : -1;
    member.range = name_range_t ();

    const int at = new_node (state, member, false);

    link_after (state, at, node);

    state.placed [member.name] = at;

    if (n < range.range.last) {

        coll_entry_t right = range;

        right.range.first = n + 1;

        const int rest = new_node (state, right, false);

        link_after (state, rest, at);
        state.ranges.push_back (rest);
    }

    if (n == range.range.first)
        unlink (state, node);
    else
        state.nodes [node].entry.range.last = n - 1;

    return at;
}


// The node that holds name in the sequence, splitting a range for it,
// or -1 when nothing does.  glibc's find_element() makes an element of
// each name a weight names, placed nowhere until a line places it, and
// reorder-after or a reorder line that finds one links into it; here a
// weight stays a name, and such a name is not in the sequence.
static int
find_placed (collate_state_t &state, const std::string &name)
{
    const std::map<std::string, int>::const_iterator it =
        state.placed.find (name);

    if (it != state.placed.end ())
        return it->second;

    unsigned long n;

    const int range = find_member (state, name, n);

    return range < 0 ? -1 : split_range (state, range, n);
}


static bool
is_symbol (const collate_state_t &state, const std::string &name)
{
    if (state.symbols.count (name))
        return true;

    for (std::size_t i = 0; i != state.symbol_ranges.size (); ++i) {

        unsigned long n;

        if (range_number (state.def.symbols [state.symbol_ranges [i]], name,
                          n))
            return true;
    }

    return false;
}


/**************************************************************************/
// the productions

// A production starts at the token it names and reads no further than
// the end of its statement.  It assigns the attribute only when what
// it reads is well-formed.  The state, for the levels and the escape
// character, is an addition to the uniform signature, as the charset is
// in the other categories.

// A name in an entry, a weight or reorder-after: the name, and the code
// point of a <Uxxxx> name or a character written raw, -1 otherwise.
struct coll_name_t {
    std::string name;
    long        ucs;
};


// <name> or a character written raw.
static bool
element_name (tok_iter &it, tok_iter last, coll_name_t &attr, diag_t &diag)
{
    if (it == last)
        return diag.error (e_syntax, where_of (it, last),
                           "LC_COLLATE: expected a symbolic name");

    const token_t &tok = *it;

    coll_name_t value;

    if (tk_symbol == tok.kind) {
        value.ucs  = tok.value;
        value.name = tok.value < 0 ? tok.text : ucs_name (tok.value);
    }
    else if (tk_raw == tok.kind) {

        // an ill-formed raw character, which the lexer has reported
        if (tok.value < 0)
            return false;

        value.ucs  = tok.value;
        value.name = ucs_name (tok.value);
    }
    else
        return diag.error (e_syntax, tok.where,
                           "LC_COLLATE: expected a symbolic name");

    ++it;

    attr = value;

    return true;
}


// "<a><b>": the names a string weight writes, in order, as glibc's
// insert_weights() splits it.  A <Uxxxx> name is the code point's.
// glibc takes any other character of the string, byte by byte, as a
// name of its own; here a character written in the string reads as
// its <Uxxxx> name, the rule for a character written raw, and an
// escaped character stands for itself.
static bool
weight_string (tok_iter &it, tok_iter /* last */, char escape,
               std::vector<std::string> &attr, diag_t &diag)
{
    const token_t     &tok = *it;
    const std::string &s   = tok.text;

    if (s.empty ())
        return diag.error (e_syntax, tok.where, "LC_COLLATE: empty weight "
                           "string not allowed");

    std::vector<std::string> value;

    for (std::size_t pos = 0; pos != s.size (); ) {

        if ('<' == s [pos]) {

            std::string name;
            std::size_t end = pos + 1;

            for (; end < s.size () && '>' != s [end]; ++end) {

                if (escape == s [end] && end + 1 < s.size ())
                    ++end;

                name += s [end];
            }

            if (end == s.size ())
                return diag.error (e_symend, tok.where, "LC_COLLATE: "
                                   "unterminated symbolic name in string");

            const long ucs = name_ucs (name);

            value.push_back (ucs < 0 ? name : ucs_name (ucs));
            pos = end + 1;
            continue;
        }

        if (escape == s [pos] && pos + 1 < s.size ())
            ++pos;

        long              ucs = (unsigned char)s [pos];
        const std::size_t len = (0x80 & ucs) ? scan_utf8 (s, pos, ucs) : 1;

        if (0 == len)
            return diag.error (e_mbchar, tok.where,
                               "invalid UTF-8 sequence in string");

        value.push_back (ucs_name (ucs));
        pos += len;
    }

    ++it;

    attr = value;

    return true;
}


// One weight: IGNORE, a name, a string of names, or on an ellipsis line
// that line's ellipsis, which stands for each member.
static bool
weight (tok_iter &it, tok_iter last, char escape, token_kind_t ellipsis,
        coll_weight_t &attr, diag_t &diag)
{
    const token_t &tok = *it;

    coll_weight_t value;

    value.kind = cw_names;

    if (kw_ignore == keyword_of (tok)) {
        value.kind = cw_ignore;
        ++it;
    }
    else if (tk_symbol == tok.kind || tk_raw == tok.kind) {

        coll_name_t name;

        if (!element_name (it, last, name, diag))
            return false;

        value.names.push_back (name.name);
    }
    else if (tk_string == tok.kind) {

        if (!weight_string (it, last, escape, value.names, diag))
            return false;
    }
    else if (   tk_semicolon != ellipsis
             && (   tk_ellipsis2 == tok.kind || tk_ellipsis3 == tok.kind
                 || tk_ellipsis4 == tok.kind)) {

        // glibc reports a different ellipsis and takes it all the same
        if (tok.kind != ellipsis)
            return diag.error (e_syntax, tok.where, "LC_COLLATE: weights "
                               "must use the same ellipsis symbol as the "
                               "name");
        value.kind = cw_self;
        ++it;
    }
    else
        return diag.error (e_syntax, tok.where, "LC_COLLATE: syntax error");

    attr = value;

    return true;
}


// weight;weight;...: at most levels, as glibc's insert_weights() reads
// them.  ellipsis is the kind of an ellipsis line's own, tk_semicolon
// on any other line.  A level the line leaves out is the element
// itself, so that an entry has a weight for each level; before the
// first order_start there are none.  A last semicolon may end the list.
static bool
weights (tok_iter &it, tok_iter last, char escape, std::size_t levels,
         token_kind_t ellipsis, std::vector<coll_weight_t> &attr,
         diag_t &diag)
{
    std::vector<coll_weight_t> value;

    while (it != last) {

        if (value.size () == levels)
            return diag.error (e_syntax, it->where,
                               "LC_COLLATE: too many values");

        coll_weight_t level;

        if (!weight (it, last, escape, ellipsis, level, diag))
            return false;

        value.push_back (level);

        if (it == last)
            break;

        if (tk_semicolon != it->kind)
            return diag.error (e_syntax, it->where,
                               "LC_COLLATE: syntax error");
        ++it;
    }

    coll_weight_t self;

    self.kind = cw_self;

    value.resize (levels, self);

    attr = value;

    return true;
}


// forward,position;backward;...: the directions of each level, as
// glibc's read_directions() reads them.
static bool
directions (tok_iter &it, tok_iter last, std::vector<int> &attr,
            diag_t &diag)
{
    std::vector<int> value (1, 0);

    for (;;) {

        int bit = 0;

        if (it != last)
            switch (keyword_of (*it)) {
            case kw_forward:  bit = dir_forward;  break;
            case kw_backward: bit = dir_backward; break;
            case kw_position: bit = dir_position; break;
            default:          break;
            }

        if (0 == bit)
            return diag.error (e_syntax, where_of (it, last),
                               "LC_COLLATE: syntax error");

        int &rule = value.back ();

        if (rule & bit)
            return diag.error (e_syntax, it->where, "LC_COLLATE: `%s' "
                               "mentioned more than once in definition of "
                               "weight %d", it->text.c_str (),
                               int (value.size ()));

        if (   (dir_forward == bit && (rule & dir_backward))
            || (dir_backward == bit && (rule & dir_forward)))
            return diag.error (e_syntax, it->where, "LC_COLLATE: `forward' "
                               "and `backward' are mutually excluding each "
                               "other");
        rule |= bit;

        if (++it == last)
            break;

        if (tk_semicolon == it->kind)
            value.push_back (0);
        else if (tk_comma != it->kind)
            return diag.error (e_syntax, it->where,
                               "LC_COLLATE: syntax error");
        ++it;
    }

    for (std::size_t i = 0; i != value.size (); ++i)
        if (dir_position == value [i])
            value [i] |= dir_forward;

    attr = value;

    return true;
}


// order_start, at the keyword: the section, if one is named, and the
// directions.  A section named alone has one level, forward.  glibc
// takes any token between the name and the directions; here it is the
// semicolon.
struct coll_head_t {
    bool             named;
    std::string      name;
    bool             one;     // no directions: one level, forward
    std::vector<int> rules;
};

static bool
section_head (tok_iter &it, tok_iter last, coll_head_t &attr, diag_t &diag)
{
    ++it;

    coll_head_t value;

    value.named = it != last && tk_symbol == it->kind && it->value < 0;
    value.one   = false;

    if (value.named) {

        value.name = it++->text;

        if (it == last) {
            value.one = true;
            value.rules.assign (1, int (dir_forward));
            attr = value;
            return true;
        }

        if (tk_semicolon != it->kind)
            return diag.error (e_syntax, it->where, "LC_COLLATE: expected "
                               "`;' after the section name");
        ++it;
    }

    if (!directions (it, last, value.rules, diag))
        return false;

    attr = value;

    return true;
}


// collating-symbol <name>, or a range of them <from>..<to>, with the
// hexadecimal ellipsis, or <from>....<to>, with the decimal one.  A
// single name is a range of one with no digits.
static bool
symbol_decl (tok_iter &it, tok_iter last, name_range_t &attr, diag_t &diag)
{
    ++it;

    if (it == last || tk_symbol != it->kind || 0 <= it->value)
        return diag.error (e_syntax, where_of (it, last),
                           "LC_COLLATE: syntax error");

    name_range_t value;

    if (it + 1 == last) {

        value          = name_range_t ();
        value.ellipsis = tk_ellipsis2;
        value.prefix   = it->text;
        value.base     = 16;
        value.step     = 1;
        ++it;
    }
    else {

        const token_t &ellipsis = *(it + 1);

        if (tk_ellipsis2 != ellipsis.kind && tk_ellipsis4 != ellipsis.kind)
            return diag.error (e_syntax, ellipsis.where,
                               "LC_COLLATE: syntax error");

        if (!name_range (it, last, value, diag))
            return false;

        if (value.ucs)
            return diag.error (e_syntax, ellipsis.where,
                               "LC_COLLATE: syntax error");
    }

    if (!end_of_line (it, last, diag))
        return false;

    attr = value;

    return true;
}


// collating-element <name> from "string": the string as code points.
static bool
element_decl (tok_iter &it, tok_iter last, const charset_t &cs,
              coll_element_t &attr, diag_t &diag)
{
    ++it;

    if (it == last || tk_symbol != it->kind || 0 <= it->value)
        return diag.error (e_syntax, where_of (it, last),
                           "LC_COLLATE: syntax error");

    coll_element_t value;

    value.name = it++->text;

    if (it == last || kw_from != keyword_of (*it))
        return diag.error (e_syntax, where_of (it, last),
                           "LC_COLLATE: expected `from'");
    ++it;

    if (it == last || tk_string != it->kind)
        return diag.error (e_syntax, where_of (it, last),
                           "LC_COLLATE: expected a string");

    if (!resolve (*it++, cs, value.value, diag) || !end_of_line (it, last, diag))
        return false;

    attr = value;

    return true;
}


// A statement of one symbolic name and nothing else, at the keyword:
// script, section-symbol, reorder-after, reorder-sections-after.  A
// <Uxxxx> name is taken where ucs allows it.
static bool
one_name (tok_iter &it, tok_iter last, bool ucs, coll_name_t &attr,
          diag_t &diag)
{
    ++it;

    coll_name_t value;

    if (it == last || (!ucs && tk_symbol == it->kind && 0 <= it->value)
        || (!ucs && tk_raw == it->kind))
        return diag.error (e_syntax, where_of (it, last),
                           "LC_COLLATE: syntax error");

    if (!element_name (it, last, value, diag) || !end_of_line (it, last, diag))
        return false;

    attr = value;

    return true;
}


// define, undef, ifdef, elifdef, elifndef, at the keyword: a word.
static bool
condition_name (tok_iter &it, tok_iter last, std::string &attr, diag_t &diag)
{
    ++it;

    if (it == last || tk_ident != it->kind)
        return diag.error (e_syntax, where_of (it, last),
                           "LC_COLLATE: syntax error");

    const std::string value = it++->text;

    if (!end_of_line (it, last, diag))
        return false;

    attr = value;

    return true;
}


// symbol-equivalence <name> <symbol>: read and ignored, no facet reads
// it.  glibc asks for the symbol to be declared.
static bool
equivalence (tok_iter &it, tok_iter last, diag_t &diag)
{
    ++it;

    for (int i = 0; i != 2; ++i, ++it)
        if (it == last || tk_symbol != it->kind)
            return diag.error (e_syntax, where_of (it, last),
                               "LC_COLLATE: syntax error");

    return end_of_line (it, last, diag);
}


// coll_weight_max number: read and ignored.
static bool
weight_max (tok_iter &it, tok_iter last, diag_t &diag)
{
    ++it;

    if (it == last || tk_number != it->kind)
        return diag.error (e_syntax, where_of (it, last),
                           "LC_COLLATE: syntax error");

    return end_of_line (++it, last, diag);
}


/**************************************************************************/
// the category

// glibc's parsing states of collate_read(), for one file's LC_COLLATE.
enum coll_parse_t {
    cp_start,            // before any section
    cp_order,            // order_start to order_end
    cp_after_order,      // after order_end
    cp_reorder,          // reorder-after to reorder-end
    cp_after_reorder,    // after reorder-end
    cp_sections,         // reorder-sections-after to reorder-sections-end
    cp_after_sections    // after reorder-sections-end
};

// glibc's else_action: where a conditional stands.
enum coll_branch_t {
    cb_none,     // outside one
    cb_taken,    // in the branch taken: else and elif skip to endif
    cb_else      // in the else branch
};


// The reading of one LC_COLLATE section: the state the locale and the
// locales it copies share, and the state of this file's section.
struct coll_reading_t {

    coll_reading_t (reader_t &r, const cmap_t &c, collate_state_t &s,
                    collate_locales_t &l, diag_t &d)
        : reader (r), cmap (c), state (s), locales (l), diag (d),
          depth (r.depth ()), parse (cp_start), branch (cb_none),
          first (true), copied (false), ellipsis (tk_semicolon) { }

    reader_t          &reader;
    const cmap_t      &cmap;
    collate_state_t   &state;
    collate_locales_t &locales;
    diag_t            &diag;

    std::size_t   depth;
    coll_parse_t  parse;

    // glibc keeps where a conditional stands in the state the copies
    // share, so that one a copied file leaves open goes on in the file
    // that copies it; here a conditional ends with its file.
    coll_branch_t branch;
    bool          first;    // only defines read so far
    bool          copied;   // a copy came first, glibc's copy_locale

    // An ellipsis line waiting for the entry after it: its kind, or
    // tk_semicolon for none, its weights and its position.
    token_kind_t               ellipsis;
    std::vector<coll_weight_t> ellipsis_weights;
    where_t                    ellipsis_where;
};


static bool
syntax_error (const token_t &tok, diag_t &diag)
{
    return diag.error (e_syntax, tok.where, "LC_COLLATE: syntax error");
}


// An entry made from a name, with its kind as glibc's insert_value()
// finds it: a character, a collating element, a declared symbol, or a
// symbol nothing declares.
static coll_entry_t
make_entry (const coll_reading_t &r, const coll_name_t &name,
            const std::vector<coll_weight_t> &weights, const where_t &where,
            bool &declared)
{
    coll_entry_t entry = coll_entry_t ();

    entry.name    = name.name;
    entry.ucs     = name.ucs;
    entry.weights = weights;
    entry.section = r.state.section;
    entry.file    = file_index (r.state, where.file);
    entry.line    = where.line;
    declared      = false;

    const cmap_entry_t *cm = 0;

    if (0 <= name.ucs)
        entry.kind = ck_char;
    else if (0 != (cm = r.cmap.find_name (name.name))) {
        entry.kind = ck_char;
        entry.ucs  = cm->ucs;
    }
    else if (r.state.elements.count (name.name))
        entry.kind = ck_element;
    else {
        entry.kind = ck_symbol;
        declared   = is_symbol (r.state, name.name);
    }

    return entry;
}


static bool
already_defined (const coll_reading_t &r, const std::string &name, int node,
                 const where_t &where)
{
    const coll_entry_t &entry = r.state.nodes [node].entry;

    return r.diag.error (e_syntax, where, "LC_COLLATE: order for `%s' "
                         "already defined at %s:%d", name.c_str (),
                         r.state.def.files [entry.file].c_str (),
                         entry.line);
}


// glibc's insert_value(): a new entry at the cursor.  An entry already
// in the sequence is an error.
static int
insert_value (coll_reading_t &r, const coll_name_t &name,
              const std::vector<coll_weight_t> &weights, const where_t &where)
{
    const int old = find_placed (r.state, name.name);

    if (0 <= old) {
        already_defined (r, name.name, old, where);
        return -1;
    }

    bool declared;

    const coll_entry_t entry = make_entry (r, name, weights, where, declared);
    const int          node  = new_node (r.state, entry, declared);

    place (r.state, node);

    r.state.placed [name.name] = node;

    return node;
}


// glibc's move_entry: an entry in the sequence goes in at the cursor
// with new weights, in the current section.  glibc corrupts its list
// when the entry is the cursor; here it stays where it is.
static void
move_entry (coll_reading_t &r, int node,
            const std::vector<coll_weight_t> &weights, const where_t &where)
{
    coll_entry_t &entry = r.state.nodes [node].entry;

    entry.weights = weights;
    entry.file    = file_index (r.state, where.file);
    entry.line    = where.line;

    if (node == r.state.cursor) {
        entry.section = r.state.section;
        return;
    }

    unlink (r.state, node);
    place (r.state, node);
}


// glibc's handle_ellipsis(), at the entry after an ellipsis line: the
// entry goes in after the one before the line, and the range of the
// line between the two.  Its members are the names from the one before
// to the one after, both left out, by the rule of name_range(): code
// points between <Uxxxx> names, which take the hexadecimal ellipsis
// alone.  A member already in the sequence is an error and is taken out
// of the range.  glibc skips a member the charmap does not have; here
// the range keeps it, as a character stays a code point, and the back
// end skips it.  glibc also reads the absolute ellipsis, ..., whose
// members are the characters of the charmap whose bytes lie between;
// no source uses it in LC_COLLATE, the charmap's order is not the
// model's, and here it is an error.
static void
handle_ellipsis (coll_reading_t &r, const coll_name_t &name,
                 const std::vector<coll_weight_t> &weights,
                 const where_t &where)
{
    const token_kind_t kind  = r.ellipsis;
    const where_t      at    = r.ellipsis_where;
    const int          start = r.state.cursor;

    r.ellipsis = tk_semicolon;

    if (start < 0) {
        r.diag.error (e_syntax, at, "LC_COLLATE: symbolic range ellipsis "
                      "must not directly follow `order_start'");
        return;
    }

    if (insert_value (r, name, weights, where) < 0)
        return;

    if (tk_ellipsis3 == kind) {
        r.diag.error (e_syntax, at, "LC_COLLATE: the absolute ellipsis `...' "
                      "is not read in an order section");
        return;
    }

    const coll_entry_t &before = r.state.nodes [start].entry;

    std::vector<token_t> ends (3);

    ends [0].kind  = tk_symbol;
    ends [0].text  = before.name;
    ends [0].value = name_ucs (before.name);
    ends [0].where = at;
    ends [1].kind  = kind;
    ends [1].text  = ellipsis_text (kind);
    ends [1].value = 0;
    ends [1].where = at;
    ends [2].kind  = tk_symbol;
    ends [2].text  = name.name;
    ends [2].value = name_ucs (name.name);
    ends [2].where = at;

    if (before.name.empty ()) {
        r.diag.error (e_pair, at, "LC_COLLATE: an ellipsis must follow a "
                      "symbolic name");
        return;
    }

    tok_iter     it = ends.begin ();
    name_range_t range;

    if (!name_range (it, ends.end (), range, r.diag))
        return;

    if (range.ucs && tk_ellipsis2 != kind) {
        r.diag.error (e_pair, at, "LC_COLLATE: with UCS range values one "
                      "must use the hexadecimal symbolic ellipsis `..'");
        return;
    }

    // two names alike, or next to each other: no member
    if (0 == range.width && !range.ucs)
        return;

    if (range.last - range.first < 2)
        return;

    ++range.first;
    --range.last;

    std::vector<unsigned long> taken;

    for (unsigned long n = range.first; n <= range.last; ++n) {

        const std::string member = range_member (range, n);

        unsigned long other;

        int old = -1;

        const std::map<std::string, int>::const_iterator p =
            r.state.placed.find (member);

        if (p != r.state.placed.end ())
            old = p->second;
        else
            old = find_member (r.state, member, other);

        if (0 <= old) {
            already_defined (r, member, old, at);
            taken.push_back (n);
        }
    }

    coll_entry_t entry = coll_entry_t ();

    entry.kind    = ck_range;
    entry.ucs     = -1;
    entry.range   = range;
    entry.weights = r.ellipsis_weights;
    entry.section = r.state.section;
    entry.file    = file_index (r.state, at.file);
    entry.line    = at.line;

    const int node = new_node (r.state, entry, false);

    const int end = r.state.cursor;

    r.state.cursor = start;
    place (r.state, node);
    r.state.cursor = end;

    // the pieces of this range are the ranges from here on: a member
    // taken out is split out of the piece that holds it and dropped
    const std::size_t pieces = r.state.ranges.size ();

    r.state.ranges.push_back (node);

    for (std::size_t i = 0; i != taken.size (); ++i) {

        const std::string member = range_member (range, taken [i]);

        for (std::size_t j = pieces; j != r.state.ranges.size (); ++j) {

            const coll_node_t &piece = r.state.nodes [r.state.ranges [j]];

            unsigned long n;

            if (piece.linked && range_number (piece.entry.range, member, n)) {

                // the name stays with the entry that holds it already
                const std::map<std::string, int>::const_iterator p =
                    r.state.placed.find (member);
                const int kept = p == r.state.placed.end () ? -1 : p->second;

                unlink (r.state, split_range (r.state, r.state.ranges [j], n));

                if (kept < 0)
                    r.state.placed.erase (member);
                else
                    r.state.placed [member] = kept;
                break;
            }
        }
    }
}


// A line led by a name or a character, in the section it stands in:
// one switch per kind of section.
static void
entry_line (tok_iter it, tok_iter last, char escape, coll_reading_t &r)
{
    const token_t &lead = *it;

    coll_name_t name;

    if (!element_name (it, last, name, r.diag))
        return;

    // glibc reads no <Uxxxx> name nor a character outside a section
    if (0 <= name.ucs && (cp_start == r.parse || cp_sections == r.parse)) {
        syntax_error (lead, r.diag);
        return;
    }

    if (   cp_start != r.parse && cp_order != r.parse && cp_reorder != r.parse
        && cp_sections != r.parse) {
        syntax_error (lead, r.diag);
        return;
    }

    collate_state_t &state = r.state;

    // glibc skips the block when it could not find where it goes
    if (cp_reorder == r.parse && state.cursor < 0)
        return;

    if (cp_sections == r.parse) {

        // reorder-sections-after: a section and its directions.  glibc
        // moves the section in its list of sections, an order that
        // numbers the rule sets of its output and nothing else; here
        // the sections stay in the order they were opened.
        std::size_t i = 0;

        while (i != state.def.sections.size ()
               && state.def.sections [i].name != name.name)
            ++i;

        if (i == state.def.sections.size ()) {
            r.diag.error (e_syntax, lead.where, "LC_COLLATE: section `%s' "
                          "not known", name.name.c_str ());
            return;
        }

        std::vector<int> rules;

        if (it != last) {

            if (!directions (it, last, rules, r.diag))
                return;

            if (rules.size () != state.levels) {
                r.diag.error (e_syntax, lead.where, "LC_COLLATE: invalid "
                              "number of sorting rules");
                return;
            }

            state.def.sections [i].rules = rules;
        }

        state.section = int (i);
        return;
    }

    std::vector<coll_weight_t> value;

    if (!weights (it, last, escape, state.levels, tk_semicolon, value,
                  r.diag))
        return;

    switch (r.parse) {

    case cp_start: {
        // a collating symbol before any section, or after a copy: it
        // takes its place at the cursor.  A name nothing declares that
        // is in the sequence moves there; glibc reads no other name.
        const std::map<std::string, int>::const_iterator p =
            state.placed.find (name.name);

        if (p != state.placed.end ()) {

            const coll_node_t &node = state.nodes [p->second];

            if (ck_symbol == node.entry.kind && !node.declared) {
                move_entry (r, p->second, value, lead.where);
                return;
            }

            if (ck_symbol != node.entry.kind) {
                syntax_error (lead, r.diag);
                return;
            }
        }
        else if (!is_symbol (state, name.name)) {
            syntax_error (lead, r.diag);
            return;
        }

        if (tk_semicolon != r.ellipsis)
            handle_ellipsis (r, name, value, lead.where);
        else
            insert_value (r, name, value, lead.where);
        break;
    }

    case cp_order:
        if (tk_semicolon != r.ellipsis)
            handle_ellipsis (r, name, value, lead.where);
        else
            insert_value (r, name, value, lead.where);
        break;

    case cp_reorder: {
        // glibc leaves an ellipsis in a reorder-after block where it
        // stands, its members never made; here it is a range, as in a
        // section
        if (tk_semicolon != r.ellipsis) {
            handle_ellipsis (r, name, value, lead.where);
            return;
        }

        const int node = find_placed (state, name.name);

        if (0 <= node)
            move_entry (r, node, value, lead.where);
        else
            insert_value (r, name, value, lead.where);
        break;
    }

    default:
        syntax_error (lead, r.diag);
    }
}


// UNDEFINED weights: the entry of every character the sequence does not
// name, once, in a section.
static void
undefined_line (tok_iter it, tok_iter last, char escape, coll_reading_t &r)
{
    const token_t &lead = *it++;

    if (cp_order != r.parse) {
        syntax_error (lead, r.diag);
        return;
    }

    collate_state_t &state = r.state;

    if (tk_semicolon != r.ellipsis) {
        r.diag.error (e_syntax, lead.where, "LC_COLLATE: cannot have "
                      "`UNDEFINED' as end of ellipsis range");
        r.ellipsis = tk_semicolon;
    }

    if (0 <= state.undefined && state.nodes [state.undefined].linked) {
        already_defined (r, "UNDEFINED", state.undefined, lead.where);
        return;
    }

    std::vector<coll_weight_t> value;

    if (!weights (it, last, escape, state.levels, tk_semicolon, value,
                  r.diag))
        return;

    coll_entry_t entry = coll_entry_t ();

    entry.kind    = ck_undefined;
    entry.ucs     = -1;
    entry.weights = value;
    entry.file    = file_index (state, lead.where.file);
    entry.line    = lead.where.line;

    state.undefined = new_node (state, entry, false);

    place (state, state.undefined);
}


// A line led by an ellipsis: the weights of the members of a range,
// whose ends are the entries before and after the line.
static void
ellipsis_line (tok_iter it, tok_iter last, char escape, coll_reading_t &r)
{
    const token_t &lead = *it++;

    if (   tk_semicolon != r.ellipsis
        || (cp_start != r.parse && cp_order != r.parse
            && cp_reorder != r.parse)
        || (tk_ellipsis2 != lead.kind && tk_ellipsis3 != lead.kind
            && tk_ellipsis4 != lead.kind)) {
        syntax_error (lead, r.diag);
        return;
    }

    std::vector<coll_weight_t> value;

    if (!weights (it, last, escape, r.state.levels, lead.kind, value, r.diag))
        return;

    r.ellipsis         = lead.kind;
    r.ellipsis_weights = value;
    r.ellipsis_where   = lead.where;
}


// order_start: opens a section named by a script, or the unnamed one,
// as glibc does.  The first sets the number of levels of the run, and
// every other must have as many.
static void
order_start (tok_iter it, tok_iter last, coll_reading_t &r)
{
    const token_t &lead = *it;

    if (   cp_start != r.parse && cp_order != r.parse
        && cp_after_order != r.parse) {
        syntax_error (lead, r.diag);
        return;
    }

    coll_head_t head;

    if (!section_head (it, last, head, r.diag))
        return;

    collate_state_t &state = r.state;

    r.parse = cp_order;

    if (0 == state.levels)
        state.levels = head.rules.size ();
    else if (head.one && 1 < state.levels) {
        r.diag.error (e_syntax, lead.where, "LC_COLLATE: invalid number of "
                      "sorting rules");
        head.rules.assign (state.levels, int (dir_forward));
    }
    else if (head.rules.size () > state.levels) {
        r.diag.error (e_syntax, lead.where, "LC_COLLATE: too many rules; "
                      "first entry only had %d", int (state.levels));
        head.rules.resize (state.levels);
    }
    else if (head.rules.size () < state.levels) {
        r.diag.error (e_syntax, lead.where, "LC_COLLATE: not enough sorting "
                      "rules");
        head.rules.resize (state.levels, int (dir_forward));
    }

    std::vector<std::string> &scripts = state.def.scripts;

    if (   head.named
        && std::find (scripts.begin (), scripts.end (), head.name)
           == scripts.end ())
        r.diag.error (e_syntax, lead.where, "LC_COLLATE: unknown section "
                      "name `%s'", head.name.c_str ());

    std::vector<coll_section_t> &sections = state.def.sections;

    std::size_t i = 0;

    while (i != sections.size () && sections [i].name != head.name)
        ++i;

    if (i == sections.size ()) {

        coll_section_t section;

        section.name = head.name;
        sections.push_back (section);
        state.section_used.push_back (false);
    }
    else if (state.section_used [i]) {
        if (head.named)
            r.diag.error (e_syntax, lead.where, "LC_COLLATE: multiple order "
                          "definitions for section `%s'", head.name.c_str ());
        else
            r.diag.error (e_syntax, lead.where, "LC_COLLATE: multiple order "
                          "definitions for unnamed section");
    }

    sections [i].rules = head.rules;
    state.section      = int (i);
}


// The end of an order section, or of the category, with an ellipsis
// line still waiting: glibc's handle_ellipsis() with no entry after.
static void
dangling_ellipsis (coll_reading_t &r)
{
    if (tk_semicolon == r.ellipsis)
        return;

    r.diag.error (e_syntax, r.ellipsis_where, "LC_COLLATE: symbolic range "
                  "ellipsis must not be directly followed by `order_end'");

    r.ellipsis = tk_semicolon;
}


// reorder-after <name>: the cursor goes to the entry named.  glibc
// takes a name the sequence does not hold for an error, and skips the
// entries of the block.
static void
reorder_after (tok_iter it, tok_iter last, coll_reading_t &r)
{
    const token_t &lead = *it;

    if (cp_order == r.parse) {
        r.diag.error (e_syntax, lead.where, "LC_COLLATE: missing "
                      "`order_end' keyword");
        dangling_ellipsis (r);
        r.parse = cp_after_order;
    }
    else if (   (cp_start == r.parse && !r.copied)
             || (cp_start != r.parse && cp_after_order != r.parse
                 && cp_reorder != r.parse)) {
        syntax_error (lead, r.diag);
        return;
    }

    r.parse = cp_reorder;

    coll_name_t name;

    if (!one_name (it, last, true, name, r.diag)) {
        r.state.cursor = -1;
        return;
    }

    const int node = find_placed (r.state, name.name);

    if (0 <= node) {
        r.state.cursor = node;
        return;
    }

    r.state.cursor = -1;

    if (is_symbol (r.state, name.name))
        r.diag.error (e_syntax, lead.where, "LC_COLLATE: order for collating "
                      "symbol %s not yet defined", name.name.c_str ());
    else if (r.state.elements.count (name.name))
        r.diag.error (e_syntax, lead.where, "LC_COLLATE: order for collating "
                      "element %s not yet defined", name.name.c_str ());
    else
        r.diag.error (e_syntax, lead.where, "LC_COLLATE: cannot reorder "
                      "after %s: symbol not known", name.name.c_str ());
}


// reorder-sections-after <section>: the section the lines after it
// name and redirect.
static void
reorder_sections_after (tok_iter it, tok_iter last, coll_reading_t &r)
{
    const token_t &lead = *it;

    if (cp_order == r.parse) {
        r.diag.error (e_syntax, lead.where, "LC_COLLATE: missing "
                      "`order_end' keyword");
        dangling_ellipsis (r);
    }
    else if (cp_reorder == r.parse)
        r.diag.error (e_syntax, lead.where, "LC_COLLATE: missing "
                      "`reorder-end' keyword");
    else if (cp_after_order != r.parse && cp_after_reorder != r.parse) {
        syntax_error (lead, r.diag);
        return;
    }

    r.parse = cp_sections;

    coll_name_t name;

    if (!one_name (it, last, false, name, r.diag))
        return;

    const std::vector<coll_section_t> &sections = r.state.def.sections;

    for (std::size_t i = 0; i != sections.size (); ++i)
        if (sections [i].name == name.name) {
            r.state.section = int (i);
            return;
        }

    r.diag.error (e_syntax, lead.where, "LC_COLLATE: section `%s' not "
                  "known", name.name.c_str ());
}


// The end of a block: order_end, reorder-end, reorder-sections-end.
static void
block_end (tok_iter it, tok_iter last, coll_parse_t from, coll_parse_t to,
           coll_reading_t &r)
{
    const token_t &lead = *it++;

    if (from != r.parse) {
        syntax_error (lead, r.diag);
        return;
    }

    if (cp_order == from)
        dangling_ellipsis (r);

    r.parse = to;

    end_of_line (it, last, r.diag);
}


// collating-symbol: declared once, as glibc's check_duplicate() asks
// of a name and of each member of a range.
static void
declare_symbol (tok_iter it, tok_iter last, coll_reading_t &r)
{
    const token_t &lead = *it;

    if (cp_start != r.parse && cp_after_order != r.parse) {
        syntax_error (lead, r.diag);
        return;
    }

    name_range_t range;

    if (!symbol_decl (it, last, range, r.diag))
        return;

    collate_state_t &state = r.state;

    const unsigned long count = range.width ? range.last - range.first + 1
                                            : 1;

    for (unsigned long i = 0; i != count; ++i) {

        const std::string name = range_member (range, range.first + i);

        const char *what = 0;

        if (r.cmap.find_name (name))
            what = "in charmap";
        else if (is_symbol (state, name))
            what = "as collating symbol";
        else if (state.elements.count (name))
            what = "as collating element";

        if (what) {
            r.diag.error (e_syntax, lead.where, "`%s' already defined %s",
                          name.c_str (), what);
            return;
        }
    }

    if (range.width)
        state.symbol_ranges.push_back (state.def.symbols.size ());
    else
        state.symbols.insert (range.prefix);

    state.def.symbols.push_back (range);
}


static void
declare_element (tok_iter it, tok_iter last, const charset_t &cs,
                 coll_reading_t &r)
{
    const token_t &lead = *it;

    if (cp_start != r.parse && cp_after_order != r.parse) {
        syntax_error (lead, r.diag);
        return;
    }

    coll_element_t element;

    if (!element_decl (it, last, cs, element, r.diag))
        return;

    collate_state_t &state = r.state;

    const char *what = 0;

    if (r.cmap.find_name (element.name))
        what = "in charmap";
    else if (is_symbol (state, element.name))
        what = "as collating symbol";
    else if (state.elements.count (element.name))
        what = "as collating element";

    if (what) {
        r.diag.error (e_syntax, lead.where, "`%s' already defined %s",
                      element.name.c_str (), what);
        return;
    }

    state.elements [element.name] = state.def.elements.size ();
    state.def.elements.push_back (element);
}


static void
declare_script (tok_iter it, tok_iter last, coll_reading_t &r)
{
    const token_t &lead = *it;

    coll_name_t name;

    if (!one_name (it, last, false, name, r.diag))
        return;

    std::vector<std::string> &scripts = r.state.def.scripts;

    if (std::find (scripts.begin (), scripts.end (), name.name)
        != scripts.end ()) {
        r.diag.error (e_syntax, lead.where, "duplicate definition of script "
                      "`%s'", name.name.c_str ());
        return;
    }

    scripts.push_back (name.name);
}


// One statement led by a keyword other than END, copy and the
// conditionals.  The keywords plan-localedef.md reads and ignores are
// matched and their statements skipped unread.
static void
statement (tok_iter it, tok_iter last, const charset_t &cs,
           coll_reading_t &r)
{
    const token_t &lead = *it;

    switch (keyword_of (lead)) {

    case kw_codepoint_collation:
        if (end_of_line (++it, last, r.diag))
            r.state.def.codepoint_collation = true;
        break;

    case kw_collating_symbol:
        declare_symbol (it, last, r);
        break;

    case kw_collating_element:
        declare_element (it, last, cs, r);
        break;

    case kw_script:
        declare_script (it, last, r);
        break;

    case kw_order_start:
        order_start (it, last, r);
        break;

    case kw_order_end:
        block_end (it, last, cp_order, cp_after_order, r);
        break;

    case kw_reorder_after:
        reorder_after (it, last, r);
        break;

    case kw_reorder_end:
        block_end (it, last, cp_reorder, cp_after_reorder, r);
        break;

    case kw_reorder_sections_after:
        reorder_sections_after (it, last, r);
        break;

    case kw_reorder_sections_end:
        block_end (it, last, cp_sections, cp_after_sections, r);
        break;

    case kw_undefined:
        undefined_line (it, last, cs.escape, r);
        break;

    case kw_symbol_equivalence:
        if (cp_start != r.parse)
            syntax_error (lead, r.diag);
        else
            equivalence (it, last, r.diag);
        break;

    case kw_coll_weight_max:
        if (cp_start != r.parse)
            syntax_error (lead, r.diag);
        else
            weight_max (it, last, r.diag);
        break;

    case kw_section_symbol: {
        coll_name_t name;

        if (cp_start != r.parse)
            syntax_error (lead, r.diag);
        else
            one_name (it, last, false, name, r.diag);
        break;
    }

    default:
        not_read (lead, "LC_COLLATE", r.diag);
    }
}


/**************************************************************************/
// the conditionals

// Skips lines to the endif of the conditional, or, unless to_endif, to
// its else, elifdef or elifndef, as glibc's skip_to() does, and leaves
// the line it stopped at in line.  An END line stops it too.  A nested
// ifdef is an error and is skipped to its endif.  kw_none when the
// file ends first.
static keyword_t
skip_to (coll_reading_t &r, bool to_endif, line_t &line)
{
    while (r.reader.next (line, r.depth)) {

        const token_t   &lead    = line.tokens.front ();
        const keyword_t  keyword = keyword_of (lead);

        switch (keyword) {

        case kw_end:
            return keyword;

        case kw_ifdef: {
            r.diag.error (e_syntax, lead.where, "LC_COLLATE: nested "
                          "conditionals not supported");

            const keyword_t stop = skip_to (r, true, line);

            if (kw_none == stop || kw_end == stop)
                return stop;
            break;
        }

        case kw_endif:
            end_of_line (line.tokens.begin () + 1, line.tokens.end (),
                         r.diag);
            return keyword;

        case kw_else:
            if (!to_endif) {
                end_of_line (line.tokens.begin () + 1, line.tokens.end (),
                             r.diag);
                return keyword;
            }

            r.diag.error (e_syntax, lead.where, "LC_COLLATE: more than one "
                          "'else'");
            break;

        case kw_elifdef: case kw_elifndef:
            if (!to_endif)
                return keyword;
            break;

        default:
            break;
        }
    }

    return kw_none;
}


// The lines of a conditional, as glibc reads them: ifdef takes its
// branch when the name is defined, and otherwise skips to the else or
// to an elifdef or elifndef, which is then the ifdef or the ifndef of
// its own name.  An else or an elif met in the branch taken skips to
// the endif.  glibc has no ifndef keyword; it is a word, and its line
// an error.  False when a skip met the end of the file; an END line it
// met is left in line, and line_end set.
static bool
conditional (coll_reading_t &r, line_t &line, bool &line_end)
{
    // true when line is an elif a skip stopped at
    bool resumed = false;

    for (;;) {

        tok_iter       it   = line.tokens.begin ();
        const tok_iter last = line.tokens.end ();

        const token_t   &lead    = *it;
        const keyword_t  keyword = keyword_of (lead);

        keyword_t stop  = kw_none;
        bool      close = true;   // the skip ends this conditional

        switch (keyword) {

        case kw_define: case kw_undef: {
            std::string name;

            if (condition_name (it, last, name, r.diag)) {
                if (kw_define == keyword)
                    r.state.defined.insert (name);
                else
                    r.state.defined.erase (name);
            }
            return true;
        }

        case kw_ifdef: case kw_elifdef: case kw_elifndef: {

            // glibc reads no conditional inside another: the inner one
            // is skipped to its endif, and the outer goes on
            if (kw_ifdef == keyword && cb_none != r.branch) {
                r.diag.error (e_syntax, lead.where, "LC_COLLATE: nested "
                              "conditionals not supported");
                stop  = skip_to (r, true, line);
                close = false;
                break;
            }

            if (kw_ifdef != keyword && !resumed) {

                if (cb_taken != r.branch) {
                    r.diag.error (e_syntax, lead.where, "LC_COLLATE: '%s' "
                                  "without matching 'ifdef' or 'ifndef'",
                                  lead.text.c_str ());
                    return true;
                }

                stop = skip_to (r, true, line);
                break;
            }

            std::string name;

            if (!condition_name (it, last, name, r.diag))
                return true;

            const bool defined = 0 != r.state.defined.count (name);

            if (defined == (kw_elifndef != keyword)) {
                r.branch = cb_taken;
                return true;
            }

            stop = skip_to (r, false, line);

            if (kw_else == stop) {
                r.branch = cb_else;
                return true;
            }

            if (kw_elifdef == stop || kw_elifndef == stop) {
                resumed = true;
                continue;
            }

            break;
        }

        case kw_else:
            end_of_line (++it, last, r.diag);

            if (cb_taken != r.branch) {
                r.diag.error (e_syntax, lead.where, "LC_COLLATE: 'else' "
                              "without matching 'ifdef' or 'ifndef'");
                return true;
            }

            stop = skip_to (r, true, line);
            break;

        case kw_endif:
            end_of_line (++it, last, r.diag);

            if (cb_none == r.branch)
                r.diag.error (e_syntax, lead.where, "LC_COLLATE: 'endif' "
                              "without matching 'ifdef' or 'ifndef'");

            r.branch = cb_none;
            return true;

        default:
            return true;
        }

        if (kw_none == stop)
            return false;

        if (close)
            r.branch = cb_none;

        line_end = kw_end == stop;

        return true;
    }
}


static bool
is_conditional (keyword_t keyword)
{
    return    kw_define == keyword || kw_undef == keyword
           || kw_ifdef == keyword || kw_elifdef == keyword
           || kw_elifndef == keyword || kw_else == keyword
           || kw_endif == keyword;
}


// The END line, and what glibc asks of the section at its end.
static void
section_end (coll_reading_t &r, const line_t &line, const token_t &name)
{
    tok_iter       it   = line.tokens.begin ();
    const tok_iter last = line.tokens.end ();

    const where_t &where = it->where;

    if (   cp_start == r.parse && !r.copied
        && !r.state.def.codepoint_collation)
        r.diag.error (e_syntax, where, "LC_COLLATE: empty category "
                      "description not allowed");
    else if (cp_order == r.parse) {
        r.diag.error (e_syntax, where, "LC_COLLATE: missing `order_end' "
                      "keyword");
        dangling_ellipsis (r);
    }
    else if (cp_reorder == r.parse)
        r.diag.error (e_syntax, where, "LC_COLLATE: missing `reorder-end' "
                      "keyword");
    else if (cp_sections == r.parse)
        r.diag.error (e_syntax, where, "LC_COLLATE: missing "
                      "`reorder-sections-end' keyword");

    end_of_section (it, last, name, r.diag);
}


void
read_collate (reader_t &reader, const line_t &head, const cmap_t &cmap,
              collate_state_t &state, collate_locales_t &locales,
              diag_t &diag)
{
    const token_t &name = head.tokens.front ();

    coll_reading_t r (reader, cmap, state, locales, diag);

    where_t at_end = name.where;

    for (line_t line; reader.next (line, r.depth); ) {

        at_end = line.tokens.front ().where;

        bool line_end = false;

        const keyword_t lead_keyword = keyword_of (line.tokens.front ());

        // glibc reads the defines before a copy as part of it
        const bool first = r.first;

        if (kw_define != lead_keyword)
            r.first = false;

        if (is_conditional (lead_keyword)) {

            if (!conditional (r, line, line_end))
                break;

            if (!line_end)
                continue;
        }

        tok_iter        it   = line.tokens.begin ();
        const tok_iter  last = line.tokens.end ();
        const charset_t cs   = { &cmap, line.escape };

        const token_t   &lead    = *it;
        const keyword_t  keyword = keyword_of (lead);

        at_end = lead.where;

        if (kw_end == keyword) {
            section_end (r, line, name);
            return;
        }

        // glibc skips the rest of the category when the copied locale
        // cannot be read; here the statements after the copy are read.
        // A copy after the first statement is read in place as well,
        // and a copy of a locale read before reads nothing.
        if (kw_copy == keyword) {

            std::string copied;

            if (copy (it, last, cs, copied, diag)) {

                r.copied = r.copied || first;

                if (state.copied.insert (copied).second)
                    locales.read (copied, lead, state);
            }

            continue;
        }

        switch (lead.kind) {

        case tk_symbol: case tk_raw:
            entry_line (it, last, line.escape, r);
            break;

        case tk_ellipsis2: case tk_ellipsis3: case tk_ellipsis4:
        case tk_ellipsis2_2: case tk_ellipsis4_2:
            ellipsis_line (it, last, line.escape, r);
            break;

        default:
            statement (it, last, cs, r);
        }
    }

    diag.error (e_syntax, at_end, "%s: premature end of file",
                name.text.c_str ());
}


void
finish_collate (collate_state_t &state)
{
    std::vector<coll_entry_t> &sequence = state.def.sequence;

    sequence.clear ();
    sequence.reserve (state.nodes.size ());

    for (int node = state.head; 0 <= node; node = state.nodes [node].next) {

        const coll_entry_t &entry = state.nodes [node].entry;

        // an empty range is one a split used up
        if (ck_range == entry.kind && entry.range.last < entry.range.first)
            continue;

        sequence.push_back (entry);
    }

    state.nodes.clear ();
    state.placed.clear ();
    state.ranges.clear ();
    state.head      = -1;
    state.cursor    = -1;
    state.undefined = -1;
}
