/***************************************************************************
 *
 * wcollate.cpp
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

// LC_COLLATE: the __rw_collate_t header, then the tries of the
// collating elements, narrow and wide, the weight table, the tries of
// the characters, narrow and wide, the four tables of page offsets,
// the first byte of each character page, the first and the last byte
// of each element page, and the two names, as the old compiler laid
// them out.  The numbers are the numbering's, collnum.h; this file
// turns them into bytes.
//
// The narrow tries are keyed by the charmap's bytes, the wide ones by
// the UTF-8 form of the wchar_t, which is the code point on every
// target: the runtime spells a wide character that way before it
// walks a table.  A leaf is the index of a row.  A character without
// a row, one the sequence leaves out, is a key with no leaf, so that
// the pages it alone would need are not made; the runtime finds no
// leaf for it and takes the shared row.  A page of a character trie
// is written from its first byte with a word to byte 255, a page of
// an element trie from its first to its last, and the runtime indexes
// a page by the byte less the first; the offset tables give each
// page's position in words from the first page of its trie.
//
// A key that is a prefix of another, one element of two letters at
// the head of one of three, keeps its leaf, and the longer key is
// never reached: the runtime returns at the first leaf it meets.  The
// old generator meant the same and mislaid the longer key's page, so
// its tables differ there.

#include "collnum.h"
#include "pool.h"
#include "trie.h"
#include "writer.h"

#include <loc/_localedef.h>

#include <algorithm>   // for stable_sort()
#include <climits>     // for UINT_MAX
#include <cstring>     // for memset()


static bool
by_key (const trie_key_t &a, const trie_key_t &b)
{
    return bytes_before (a.bytes, b.bytes);
}


// The keys of the element tries: the elements with a row, in the
// order declared, which of two spelled alike keeps the first.
static void
element_keys (const coll_numbering_t &num, const collate_def_t &def,
              const cmap_t &cmap, std::vector<trie_key_t> &narrow,
              std::vector<trie_key_t> &wide)
{
    for (std::size_t i = 0; i != def.elements.size (); ++i) {

        if (num.element_rows [i] < 0)
            continue;

        const ucs_string_t &value = def.elements [i].value;

        trie_key_t n;
        trie_key_t w;

        n.leaf = w.leaf = unsigned (num.element_rows [i]);

        for (std::size_t j = 0; j != value.size (); ++j) {

            cmap_entry_t entry;

            // the numbering gave a row to no element the charmap
            // does not spell
            cmap.find_ucs (value [j], entry);

            n.bytes += entry.bytes;
            w.bytes += utf8_of (value [j]);
        }

        narrow.push_back (n);
        wide.push_back (w);
    }

    std::stable_sort (narrow.begin (), narrow.end (), by_key);
    std::stable_sort (wide.begin (), wide.end (), by_key);
}


// The keys of the character tries: every character of the charmap,
// in the order defined, so that of two definitions of one byte string
// the first is the one keyed, as the old compiler's maps kept it.
static void
character_keys (const coll_numbering_t &num, const cmap_t &cmap,
                std::vector<trie_key_t> &narrow, std::vector<trie_key_t> &wide)
{
    cmap_iter_t  iter (cmap);
    cmap_entry_t entry;

    while (iter.next (entry)) {

        if (entry.ucs < 0)
            continue;

        const long row = char_row (num, entry.ucs);

        trie_key_t n;
        trie_key_t w;

        n.bytes = entry.bytes;
        w.bytes = utf8_of (entry.ucs);
        n.leaf  = w.leaf = row < 0 ? UINT_MAX : unsigned (row);

        narrow.push_back (n);
        wide.push_back (w);
    }

    std::stable_sort (narrow.begin (), narrow.end (), by_key);
    std::stable_sort (wide.begin (), wide.end (), by_key);
}


// The first and the last byte a page is written from and to: a
// character page runs to 255; a page with no word, the root of a trie
// no key has a leaf in, is written whole from byte 0, so that every
// lookup in it finds no leaf.
static void
page_span (const trie_page_t &page, bool to_end, unsigned &lo, unsigned &hi)
{
    lo = page.first < 0 ? 0U : unsigned (page.first);
    hi = to_end || page.last < 0 ? 255U : unsigned (page.last);
}


static unsigned
pages_words (const std::vector<trie_page_t> &pages, bool to_end)
{
    unsigned words = 0;

    for (std::size_t i = 0; i != pages.size (); ++i) {

        unsigned lo;
        unsigned hi;

        page_span (pages [i], to_end, lo, hi);

        words += hi - lo + 1;
    }

    return words;
}


static void
add_pages (const std::vector<trie_page_t> &pages, bool to_end, pool_t &pool)
{
    for (std::size_t i = 0; i != pages.size (); ++i) {

        unsigned lo;
        unsigned hi;

        page_span (pages [i], to_end, lo, hi);

        pool.add (&pages [i].word [lo], (hi - lo + 1) * sizeof (unsigned));
    }
}


// The position of each page in words from the first page.
static void
add_offsets (const std::vector<trie_page_t> &pages, bool to_end, pool_t &pool)
{
    unsigned offset = 0;

    for (std::size_t i = 0; i != pages.size (); ++i) {

        unsigned lo;
        unsigned hi;

        page_span (pages [i], to_end, lo, hi);

        pool.add (&offset, sizeof offset);

        offset += hi - lo + 1;
    }
}


static void
add_bounds (const std::vector<trie_page_t> &pages, bool to_end, bool last,
            pool_t &pool)
{
    for (std::size_t i = 0; i != pages.size (); ++i) {

        unsigned lo;
        unsigned hi;

        page_span (pages [i], to_end, lo, hi);

        const unsigned char byte = (unsigned char)(last ? hi : lo);

        pool.add (&byte, 1);
    }
}


void
write_collate (const coll_numbering_t &num, const collate_def_t &def,
               const cmap_t &cmap, const image_names_t &names,
               std::vector<char> &image)
{
    // a leaf ends a walk, and a prefix whose keys all lack a leaf
    // gets no page
    const trie_rules_t rules = { true, false };

    std::vector<trie_key_t>  narrow;
    std::vector<trie_key_t>  wide;
    std::vector<trie_page_t> n_ce;
    std::vector<trie_page_t> w_ce;
    std::vector<trie_page_t> n_char;
    std::vector<trie_page_t> w_char;

    // no element, no page: the runtime walks the element tries only
    // when the header says an element is longer than one character
    element_keys (num, def, cmap, narrow, wide);

    if (!narrow.empty ())
        build_trie (narrow, rules, n_ce);

    if (!wide.empty ())
        build_trie (wide, rules, w_ce);

    narrow.clear ();
    wide.clear ();

    character_keys (num, cmap, narrow, wide);

    build_trie (narrow, rules, n_char);
    build_trie (wide, rules, w_char);

    const unsigned word = sizeof (unsigned);

    __rw::__rw_collate_t hdr;
    std::memset (&hdr, 0, sizeof hdr);

    hdr.elm_size = (unsigned (num.levels) * num.longest_weight + 1) * word;
    hdr.num_elms = unsigned (num.rows.size ());

    hdr.n_ce_tab_off   = 0;
    hdr.w_ce_tab_off   = hdr.n_ce_tab_off + pages_words (n_ce, false) * word;
    hdr.weight_tab_off = hdr.w_ce_tab_off + pages_words (w_ce, false) * word;
    hdr.n_char_tab_off = hdr.weight_tab_off + hdr.num_elms * hdr.elm_size;
    hdr.w_char_tab_off =
        hdr.n_char_tab_off + pages_words (n_char, true) * word;
    hdr.n_char_off_tab_off =
        hdr.w_char_tab_off + pages_words (w_char, true) * word;
    hdr.w_char_off_tab_off =
        hdr.n_char_off_tab_off + unsigned (n_char.size ()) * word;
    hdr.n_ce_off_tab_off =
        hdr.w_char_off_tab_off + unsigned (w_char.size ()) * word;
    hdr.w_ce_off_tab_off =
        hdr.n_ce_off_tab_off + unsigned (n_ce.size ()) * word;
    hdr.n_char_first_char_off =
        hdr.w_ce_off_tab_off + unsigned (w_ce.size ()) * word;
    hdr.w_char_first_char_off =
        hdr.n_char_first_char_off + unsigned (n_char.size ());
    hdr.n_ce_first_char_off =
        hdr.w_char_first_char_off + unsigned (w_char.size ());
    hdr.w_ce_first_char_off =
        hdr.n_ce_first_char_off + unsigned (n_ce.size ());
    hdr.n_ce_last_char_off =
        hdr.w_ce_first_char_off + unsigned (w_ce.size ());
    hdr.w_ce_last_char_off =
        hdr.n_ce_last_char_off + unsigned (n_ce.size ());
    hdr.codeset_off = hdr.w_ce_last_char_off + unsigned (w_ce.size ());
    hdr.charmap_off = hdr.codeset_off + unsigned (names.codeset.size ()) + 1;

    // a shared row for the characters left out, unless the order is
    // the code points'
    hdr.undefined_optimization = num.undefined_row < 0 ? 0 : 1;
    hdr.undefined_weight_idx   =
        num.undefined_row < 0 ? 0 : unsigned (num.undefined_row);

    hdr.longest_weight = (unsigned char)num.longest_weight;
    hdr.num_weights    = (unsigned char)num.levels;
    hdr.largest_ce     = (unsigned char)num.largest_element;

    for (std::size_t i = 0; i != num.rules.size () && i != 256; ++i)
        hdr.weight_type [i] =
            (unsigned char)((num.rules [i] & dir_backward ? 1 : 0)
                            | (num.rules [i] & dir_position ? 2 : 0));

    pool_t pool;

    add_pages (n_ce, false, pool);
    add_pages (w_ce, false, pool);

    for (std::size_t i = 0; i != num.rows.size (); ++i) {

        pool.add (&num.rows [i].order, word);

        if (hdr.elm_size > word)
            pool.add (num.row_words (i), hdr.elm_size - word);
    }

    add_pages (n_char, true, pool);
    add_pages (w_char, true, pool);

    add_offsets (n_char, true, pool);
    add_offsets (w_char, true, pool);
    add_offsets (n_ce, false, pool);
    add_offsets (w_ce, false, pool);

    add_bounds (n_char, true, false, pool);
    add_bounds (w_char, true, false, pool);
    add_bounds (n_ce, false, false, pool);
    add_bounds (w_ce, false, false, pool);
    add_bounds (n_ce, false, true, pool);
    add_bounds (w_ce, false, true, pool);

    pool.add (names.codeset);
    pool.add (names.charmap);

    image.clear ();

    append (image, &hdr, sizeof hdr);
    pool.append_to (image);
}
