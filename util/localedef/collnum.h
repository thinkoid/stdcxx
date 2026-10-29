/***************************************************************************
 *
 * collnum.h
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

#ifndef RWSTD_UTIL_LOCALEDEF_COLLNUM_H_INCLUDED
#define RWSTD_UTIL_LOCALEDEF_COLLNUM_H_INCLUDED

#include "cmap.h"
#include "diag.h"
#include "model.h"

#include <cstddef>   // for size_t
#include <string>
#include <utility>   // for pair
#include <vector>


// The collation numbering: the one place where the names of the
// model's sequence become numbers, before the LC_COLLATE writer puts
// them on disk.  The rules are the old compiler's, so that the image
// comes out the same:
//
// - Every entry of the sequence takes a collating value, from 2 in
//   sequence order; 0 is IGNORE and 1 ends a pass in a transform.
//   A range takes one value per member the charmap has, and a member
//   it lacks takes none.
// - A character the charmap has, an element whose every character it
//   has, and UNDEFINED take a row of weights, in the order the
//   sequence gives them.  A symbol, a character the charmap lacks and
//   an element it cannot spell take a value and no row: a weight may
//   name them, and nothing else can.
// - A weight is resolved by name to the value the name has, IGNORE is
//   the weight 0, and a level the line leaves out is the entry's own
//   value.  A row holds the levels one after the other, each as many
//   words as the longest weight, the words past a weight's UINT_MAX;
//   before them a word of the section's direction bits, two per
//   level, the first level in the highest pair.
// - The characters the sequence leaves out share one row.  With
//   UNDEFINED in the sequence it is that entry's row, at its place.
//   Without it the row comes after every other, every level its own
//   value, with the last section's direction word, and the omission
//   is a warning, as POSIX asks: the characters sort last, POSIX's
//   order and the library's rule since the 2005 import, kept over
//   glibc's, which gives them the lowest row.
// - codepoint_collation discards the sequence, as glibc does, and
//   numbers nothing: the image holds no table and the facets compare
//   the characters' values.

// What one entry of the sequence was given.
struct coll_number_t {
    unsigned    value;   // its first collating value, 0 for none
    std::size_t count;   // its values: a range's members the charmap has
    long        row;     // its first row, -1 for none
};


// One row of the weight table; its words are in coll_numbering_t.
struct coll_row_t {
    std::string name;    // the entry's, for the dump
    unsigned    value;
    unsigned    order;   // the direction word
};


struct coll_numbering_t {

    coll_numbering_t ();

    // The levels and the directions of each, the last section's: the
    // header's weight types.
    std::size_t      levels;
    std::vector<int> rules;

    unsigned longest_weight;    // words per level, at least 1
    unsigned largest_element;   // characters of the longest element
                                // with a row, at least 1
    bool     codepoint;         // codepoint_collation, nothing numbered

    std::vector<coll_number_t> numbers;   // one per entry of the sequence
    std::vector<coll_row_t>    rows;
    std::vector<unsigned>      words;     // levels * longest_weight per row

    long        undefined_row;    // the shared row, -1 when codepoint
    unsigned    next_value;       // one past the last value given
    std::size_t missing;          // the characters without a row

    // The row of a code point, in ascending order of code point, and
    // of an element, by its index in the model, -1 for none.
    std::vector<std::pair<long, unsigned> > char_rows;
    std::vector<long>                       element_rows;

    // The words of a row, in the order the weight table holds them.
    const unsigned* row_words (std::size_t row) const {
        return &words [row * levels * longest_weight];
    }
};


// The row of a code point, -1 when it has none.
long char_row (const coll_numbering_t &num, long ucs);

// Numbers the collation of def against cmap.  False if it reported an
// error: a weight that names what the sequence does not order.  The
// characters left out draw a warning.
bool number_collate (const collate_def_t &def, const cmap_t &cmap,
                     coll_numbering_t &num, diag_t &diag);

#endif   // RWSTD_UTIL_LOCALEDEF_COLLNUM_H_INCLUDED
