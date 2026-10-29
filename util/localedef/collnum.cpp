/***************************************************************************
 *
 * collnum.cpp
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

// Two passes over the sequence, as the old compiler made them: the
// first gives every entry its collating values, so that a weight may
// name an entry placed after the line that names it; the second
// builds the rows and resolves the weights.  The old compiler's second
// pass resolved a name to the value its first pass had given, which
// is the value the name keeps, since both passes count the same
// entries in the same order; the one name it could not resolve, a
// member of a range not yet reached, is an error here as there.

#include "collnum.h"
#include "reader.h"   // for range_member()

#include <algorithm>   // for lower_bound(), sort()
#include <climits>     // for UINT_MAX
#include <map>


coll_numbering_t::
coll_numbering_t ()
    : levels (0), longest_weight (1), largest_element (1), codepoint (false),
      undefined_row (-1), next_value (2), missing (0)
{
}


static bool
by_ucs (const std::pair<long, unsigned> &a, const std::pair<long, unsigned> &b)
{
    return a.first < b.first;
}


// The pair of a code point in a vector sorted by code point, or end.
static std::vector<std::pair<long, unsigned> >::const_iterator
find_ucs (const std::vector<std::pair<long, unsigned> > &pairs, long ucs)
{
    const std::pair<long, unsigned> key (ucs, 0);

    const std::vector<std::pair<long, unsigned> >::const_iterator it =
        std::lower_bound (pairs.begin (), pairs.end (), key, by_ucs);

    return it != pairs.end () && it->first == ucs ? it : pairs.end ();
}


long
char_row (const coll_numbering_t &num, long ucs)
{
    const std::vector<std::pair<long, unsigned> >::const_iterator it =
        find_ucs (num.char_rows, ucs);

    return it == num.char_rows.end () ? -1 : long (it->second);
}


/**************************************************************************/
// the direction word

// The two bits of one level, as the runtime's __rw_process_offsets()
// reads them: 0 forward, 1 backward, 2 forward and position, 3
// backward and position.
static unsigned
direction_bits (int rule)
{
    return (rule & dir_backward ? 1U : 0U) | (rule & dir_position ? 2U : 0U);
}


// The word of a section: the bits of each level, the first level in
// the highest pair.
static unsigned
order_word (const std::vector<int> &rules)
{
    unsigned word = 0;

    for (std::size_t i = 0; i != rules.size (); ++i)
        word = word << 2 | direction_bits (rules [i]);

    return word;
}


// The rules an entry's row is written with: its section's, or the
// last section's for an entry placed in none.
static const std::vector<int>&
rules_of (const collate_def_t &def, int section)
{
    static const std::vector<int> none;

    if (def.sections.empty ())
        return none;

    const std::size_t i =
        0 <= section && std::size_t (section) < def.sections.size ()
            ? std::size_t (section) : def.sections.size () - 1;

    return def.sections [i].rules;
}


/**************************************************************************/
// the values

// The collating values by name and by code point.  An entry of the
// sequence is found by its name; a member of a range has no entry and
// is found by the code point its name gives.
struct values_t {
    std::map<std::string, unsigned>          by_name;
    std::vector<std::pair<long, unsigned> >  by_ucs;
};


static bool
value_of (const values_t &values, const cmap_t &cmap, const std::string &name,
          unsigned &value)
{
    const std::map<std::string, unsigned>::const_iterator it =
        values.by_name.find (name);

    if (it != values.by_name.end ()) {
        value = it->second;
        return true;
    }

    cmap_entry_t entry;

    if (!cmap.find_name (name, entry) || entry.ucs < 0)
        return false;

    const std::vector<std::pair<long, unsigned> >::const_iterator found =
        find_ucs (values.by_ucs, entry.ucs);

    if (found == values.by_ucs.end ())
        return false;

    value = found->second;
    return true;
}


// The code point of member n of a range, -1 when the charmap lacks it
// or the name is not a character's.
static long
member_ucs (const name_range_t &range, unsigned long n, const cmap_t &cmap)
{
    cmap_entry_t entry;

    if (range.ucs)
        return cmap.find_ucs (long (n), entry) ? long (n) : -1;

    if (!cmap.find_name (range_member (range, n), entry))
        return -1;

    return entry.ucs;
}


// Whether the charmap spells an element: it has every character.  An
// element it does not spell has no bytes to key a trie with, and the
// old compiler took it for a collating symbol.
static bool
spelled (const coll_element_t &element, const cmap_t &cmap)
{
    cmap_entry_t entry;

    if (element.value.empty ())
        return false;

    for (std::size_t i = 0; i != element.value.size (); ++i)
        if (!cmap.find_ucs (element.value [i], entry))
            return false;

    return true;
}


/**************************************************************************/
// the rows

static std::size_t
add_row (coll_numbering_t &num, const std::string &name, unsigned value,
         unsigned order)
{
    const coll_row_t row = { name, value, order };

    num.rows.push_back (row);
    num.words.resize (num.rows.size () * num.levels * num.longest_weight,
                      UINT_MAX);

    return num.rows.size () - 1;
}


// Resolves one weight into the words of its level: IGNORE is 0, the
// entry itself its own value, names their values, and the words past
// the weight stay UINT_MAX.
static bool
resolve_weight (const coll_weight_t &weight, unsigned self,
                const values_t &values, const cmap_t &cmap, unsigned *out,
                const where_t &where, const std::string &of, diag_t &diag)
{
    switch (weight.kind) {

    case cw_ignore:
        out [0] = 0;
        break;

    case cw_self:
        out [0] = self;
        break;

    case cw_names:
        for (std::size_t k = 0; k != weight.names.size (); ++k)
            if (!value_of (values, cmap, weight.names [k], out [k]))
                return diag.error (e_symused, where, "LC_COLLATE: weight "
                                   "<%s> of <%s> is not in the collation "
                                   "sequence", weight.names [k].c_str (),
                                   of.c_str ());
        break;
    }

    return true;
}


// A row for an entry, or for one member of a range: the words of
// each level from the entry's weights, a level the line left out
// being the entry itself.
static bool
fill_row (coll_numbering_t &num, std::size_t row, const coll_entry_t &entry,
          const std::string &name, unsigned self, const values_t &values,
          const cmap_t &cmap, const where_t &where, diag_t &diag)
{
    // no level, no words: a definition with no order_start
    if (0 == num.levels)
        return true;

    unsigned* const words = &num.words [row * num.levels * num.longest_weight];

    const coll_weight_t itself = { cw_self, std::vector<std::string> () };

    for (std::size_t j = 0; j != num.levels; ++j) {

        const coll_weight_t &weight =
            j < entry.weights.size () ? entry.weights [j] : itself;

        if (!resolve_weight (weight, self, values, cmap,
                             words + j * num.longest_weight, where, name,
                             diag))
            return false;
    }

    return true;
}


/**************************************************************************/

bool
number_collate (const collate_def_t &def, const cmap_t &cmap,
                coll_numbering_t &num, diag_t &diag)
{
    num = coll_numbering_t ();

    num.element_rows.assign (def.elements.size (), -1L);

    // glibc discards the sequence under codepoint_collation and
    // compares the characters' values; the facets do the same, so
    // the keyword takes no value, no row and no level
    if (def.codepoint_collation) {
        num.codepoint = true;
        return true;
    }

    num.levels = rules_of (def, -1).size ();
    num.rules  = rules_of (def, -1);

    std::map<std::string, std::size_t> element_index;

    for (std::size_t i = 0; i != def.elements.size (); ++i)
        element_index.insert (std::make_pair (def.elements [i].name, i));

    const std::vector<coll_entry_t> &sequence = def.sequence;

    // the first pass: the values, and which entries take a row
    values_t          values;
    std::vector<bool> has_row (sequence.size (), false);

    num.numbers.resize (sequence.size ());

    bool undefined = false;

    for (std::size_t i = 0; i != sequence.size (); ++i) {

        const coll_entry_t &entry  = sequence [i];
        coll_number_t      &number = num.numbers [i];

        number.value = 0;
        number.count = 0;
        number.row   = -1;

        cmap_entry_t cm;

        switch (entry.kind) {

        case ck_range:
            number.value = num.next_value;

            for (unsigned long n = entry.range.first;
                 n <= entry.range.last; ++n) {

                const long ucs = member_ucs (entry.range, n, cmap);

                if (ucs < 0)
                    continue;

                values.by_ucs.push_back (
                    std::make_pair (ucs, num.next_value++));
                ++number.count;
            }

            has_row [i] = true;
            break;

        case ck_undefined:
            number.value = num.next_value++;
            number.count = 1;
            has_row [i]  = true;
            undefined    = true;
            break;

        case ck_char:
        case ck_symbol:
        case ck_element:
            number.value = num.next_value++;
            number.count = 1;

            values.by_name [entry.name] = number.value;

            if (ck_char == entry.kind && 0 <= entry.ucs) {
                values.by_ucs.push_back (
                    std::make_pair (entry.ucs, number.value));
                has_row [i] = cmap.find_ucs (entry.ucs, cm);
            }

            if (ck_element == entry.kind) {

                const std::map<std::string, std::size_t>::const_iterator
                    it = element_index.find (entry.name);

                has_row [i] =    it != element_index.end ()
                              && spelled (def.elements [it->second], cmap);

                if (has_row [i]) {

                    const std::size_t n =
                        def.elements [it->second].value.size ();

                    if (num.largest_element < n)
                        num.largest_element = unsigned (n);
                }
            }
            break;
        }

        // the longest weight counts the lines that take a row, as the
        // old compiler's resolution did: a symbol's weights were never
        // read
        if (has_row [i])
            for (std::size_t j = 0; j != entry.weights.size (); ++j)
                if (   cw_names == entry.weights [j].kind
                    && num.longest_weight < entry.weights [j].names.size ())
                    num.longest_weight =
                        unsigned (entry.weights [j].names.size ());
    }

    std::sort (values.by_ucs.begin (), values.by_ucs.end (), by_ucs);

    // the second pass: the rows
    for (std::size_t i = 0; i != sequence.size (); ++i) {

        if (!has_row [i])
            continue;

        const coll_entry_t &entry  = sequence [i];
        coll_number_t      &number = num.numbers [i];

        const where_t  where = { def.files [entry.file].c_str (), entry.line,
                                 0 };
        const unsigned order = order_word (rules_of (def, entry.section));

        if (ck_range == entry.kind) {

            unsigned value = number.value;

            for (unsigned long n = entry.range.first;
                 n <= entry.range.last; ++n) {

                const long ucs = member_ucs (entry.range, n, cmap);

                if (ucs < 0)
                    continue;

                const std::string name = range_member (entry.range, n);

                const std::size_t row = add_row (num, name, value, order);

                if (number.row < 0)
                    number.row = long (row);

                num.char_rows.push_back (std::make_pair (ucs, unsigned (row)));

                if (!fill_row (num, row, entry, name, value++, values, cmap,
                               where, diag))
                    return false;
            }

            continue;
        }

        const std::string &name =
            ck_undefined == entry.kind ? std::string ("UNDEFINED")
                                       : entry.name;

        const std::size_t row = add_row (num, name, number.value, order);

        number.row = long (row);

        if (ck_undefined == entry.kind)
            num.undefined_row = long (row);
        else if (ck_element == entry.kind)
            num.element_rows [element_index [entry.name]] = long (row);
        else
            num.char_rows.push_back (
                std::make_pair (entry.ucs, unsigned (row)));

        if (!fill_row (num, row, entry, name, number.value, values, cmap,
                       where, diag))
            return false;
    }

    std::sort (num.char_rows.begin (), num.char_rows.end (), by_ucs);

    // the characters the sequence leaves out, each code point once
    cmap_iter_t  iter (cmap);
    cmap_entry_t entry;
    cmap_entry_t first;

    while (iter.next (entry))
        if (   0 <= entry.ucs
            && cmap.find_ucs (entry.ucs, first) && first.bytes == entry.bytes
            && char_row (num, entry.ucs) < 0)
            ++num.missing;

    if (undefined)
        return true;

    if (num.missing)
        diag.warning (w_missing, nowhere, "some characters in the codeset "
                      "were not explicitly given a collation value");

    // the shared row after every other, every level its own value
    const unsigned value = num.next_value++;

    const std::size_t row =
        add_row (num, "UNDEFINED", value, order_word (rules_of (def, -1)));

    num.undefined_row = long (row);

    for (std::size_t j = 0; j != num.levels; ++j)
        num.words [row * num.levels * num.longest_weight
                   + j * num.longest_weight] = value;

    return true;
}
