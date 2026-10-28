/***************************************************************************
 *
 * wcodecvt.cpp
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

// The codecvt database of a codeset, from the charmap alone: the
// __rw_codecvt_t header, then the tries, the (internal, UCS) pairs,
// the two names and the multibyte strings, as the old compiler laid
// them out.  The characters are ordered by their bytes, and that
// order is the ordinal of a character, the order of its pair and of
// its string.  Two tries: n_to_w, keyed by the bytes, whose leaf is
// the ordinal, and w_to_n, keyed by the UTF-8 form of the wchar_t,
// whose leaf is the offset of the string.  A code point the charmap
// defines twice decodes from both byte strings and encodes to the
// first definition's, as in glibc; the old compiler encoded to the
// lower of the two byte strings.  The third, utf8_to_ext,
// keyed by the UTF-8 form of the UCS value, is w_to_n under another
// name: wchar_t is UCS-4 on every target the compiler is built for,
// so the header's utf8_to_ext_tab_off is set to w_to_n_tab_off and
// no third trie is written, and the pair holds the code point twice.
// The transliteration trie is one root page with no entries and no
// data after the strings: no reader uses it, and the old compiler's
// depended on which locale of the codeset was built first.

#include "cmap.h"
#include "pool.h"
#include "trie.h"
#include "writer.h"

#include <loc/_localedef.h>

#include <algorithm>   // for sort(), stable_sort()
#include <climits>     // for UINT_MAX
#include <cstring>     // for memset()


enum { page_bytes = 256 * sizeof (unsigned) };


// One character of the database while it is built.
struct mb_char_t {
    std::string bytes;
    long        ucs;
    bool        encodes;   // the code point's first definition
};


static bool
by_bytes (const mb_char_t &a, const mb_char_t &b)
{
    return bytes_before (a.bytes, b.bytes);
}


static bool
by_key (const trie_key_t &a, const trie_key_t &b)
{
    return bytes_before (a.bytes, b.bytes);
}


// The characters the database converts, in the order of their bytes,
// one per byte string.  Bytes defined twice keep their first
// definition, as a name does in the charmap; a code point defined
// twice is two characters, of which the first definition, the one
// find_ucs() gives, is the one its wide value encodes to.  A name
// without a code point has no wide value to pair with and is left
// out: its bytes then fail to convert, which is the truth of it.  No
// charmap of either tree of sources has one.
static void
collect (const cmap_t &cmap, std::vector<mb_char_t> &chars)
{
    cmap_iter_t  iter (cmap);
    cmap_entry_t entry;
    cmap_entry_t first;

    while (iter.next (entry)) {

        if (entry.ucs < 0)
            continue;

        const mb_char_t ch = {
            entry.bytes, entry.ucs,
            cmap.find_ucs (entry.ucs, first) && first.bytes == entry.bytes
        };

        chars.push_back (ch);
    }

    std::stable_sort (chars.begin (), chars.end (), by_bytes);

    std::size_t kept = 0;

    for (std::size_t i = 0; i != chars.size (); ++i)
        if (0 == kept || chars [kept - 1].bytes != chars [i].bytes)
            chars [kept++] = chars [i];

    chars.resize (kept);
}


// Appends the pages, each 256 words, with base added to every leaf:
// the leaves of w_to_n are offsets into the strings, which are placed
// once the pages are counted.
static void
add_pages (const std::vector<trie_page_t> &pages, unsigned base,
           pool_t &pool)
{
    for (std::size_t i = 0; i != pages.size (); ++i) {

        unsigned words [256];

        for (unsigned j = 0; j != 256; ++j) {

            const unsigned word = pages [i].word [j];

            words [j] = UINT_MAX == word || (word & trie_page_bit)
                      ? word : word + base;
        }

        pool.add (words, sizeof words);
    }
}


void
write_codecvt (const cmap_t &cmap, const image_names_t &names,
               std::vector<char> &image)
{
    std::vector<mb_char_t> chars;

    collect (cmap, chars);

    // a complete character ends the walk of either trie, and every
    // key has a value
    const trie_rules_t rules = { true, false };

    std::vector<trie_key_t>  keys (chars.size ());
    std::vector<trie_page_t> n_to_w;
    std::vector<trie_page_t> w_to_n;

    for (std::size_t i = 0; i != chars.size (); ++i) {
        keys [i].bytes = chars [i].bytes;
        keys [i].leaf  = unsigned (i);
    }

    build_trie (keys, rules, n_to_w);

    // the strings follow one another in the order of the characters;
    // a code point defined twice is keyed once, to the string of its
    // first definition
    keys.clear ();

    unsigned offset = 0;

    for (std::size_t i = 0; i != chars.size (); ++i) {

        if (chars [i].encodes) {
            const trie_key_t key = { utf8_of (chars [i].ucs), offset };
            keys.push_back (key);
        }

        offset += unsigned (chars [i].bytes.size ()) + 1;
    }

    std::sort (keys.begin (), keys.end (), by_key);

    build_trie (keys, rules, w_to_n);

    __rw::__rw_codecvt_t hdr;
    std::memset (&hdr, 0, sizeof hdr);

    hdr.n_to_w_tab_off      = 0;
    hdr.w_to_n_tab_off      = unsigned (n_to_w.size () * page_bytes);
    hdr.utf8_to_ext_tab_off = hdr.w_to_n_tab_off;
    hdr.xliteration_off     =
        hdr.w_to_n_tab_off + unsigned (w_to_n.size () * page_bytes);
    hdr.wchar_off           = hdr.xliteration_off + page_bytes;
    hdr.codeset_off         =
        hdr.wchar_off + unsigned (chars.size () * 2 * sizeof (wchar_t));
    hdr.charmap_off         =
        hdr.codeset_off + unsigned (names.codeset.size ()) + 1;
    hdr.mb_cur_max          = (unsigned char)cmap.mb_cur_max;

    const unsigned strings_off =
        hdr.charmap_off + unsigned (names.charmap.size ()) + 1;

    pool_t pool;

    add_pages (n_to_w, 0, pool);
    add_pages (w_to_n, strings_off, pool);

    // the transliteration root: no character leads anywhere
    trie_page_t root;

    for (unsigned i = 0; i != 256; ++i)
        root.word [i] = UINT_MAX;

    root.first = -1;
    root.last  = -1;

    add_pages (std::vector<trie_page_t> (1, root), 0, pool);

    for (std::size_t i = 0; i != chars.size (); ++i) {

        const wchar_t pair [2] = {
            wchar_t (chars [i].ucs), wchar_t (chars [i].ucs)
        };

        pool.add (pair, sizeof pair);
    }

    pool.add (names.codeset);
    pool.add (names.charmap);

    for (std::size_t i = 0; i != chars.size (); ++i)
        pool.add (chars [i].bytes);

    image.clear ();

    append (image, &hdr, sizeof hdr);
    pool.append_to (image);
}
