/***************************************************************************
 *
 * trie.cpp
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

#include "trie.h"

#include <climits>   // for UINT_MAX


bool
bytes_before (const std::string &a, const std::string &b)
{
    const std::size_t n = a.size () < b.size () ? a.size () : b.size ();

    // char_traits<char> of C++03 compares as char, which is signed
    // here; the pages index by the unsigned value
    for (std::size_t i = 0; i != n; ++i)
        if (a [i] != b [i])
            return (unsigned char)a [i] < (unsigned char)b [i];

    return a.size () < b.size ();
}


// The page of the keys in [lo, hi), which share their first depth
// bytes; leaf is the page's own, the leaf of the key spelled by
// exactly those bytes.  The keys being sorted, the ones through one byte are
// adjacent and a key that ends at that byte is the first of them.
// The page is numbered when it is made and filled as its bytes are
// walked, each subtrie made before the next byte, which is the
// depth-first numbering; the vector grows meanwhile, so the page is
// reached by its number, never held by reference.
static void
build_page (const std::vector<trie_key_t> &keys, std::size_t lo,
            std::size_t hi, std::size_t depth, unsigned leaf,
            const trie_rules_t &rules, std::vector<trie_page_t> &pages)
{
    const std::size_t number = pages.size ();

    trie_page_t page;

    for (unsigned i = 0; i != 256; ++i)
        page.word [i] = UINT_MAX;

    page.leaf  = leaf;
    page.first = -1;
    page.last  = -1;

    pages.push_back (page);

    for (std::size_t i = lo; i != hi; ) {

        const unsigned char byte = keys [i].bytes [depth];

        std::size_t j = i;

        while (j != hi && (unsigned char)keys [j].bytes [depth] == byte)
            ++j;

        // the keys that end at this byte come first, then the ones
        // that continue past it
        std::size_t from = i;

        while (from != j && keys [from].bytes.size () == depth + 1)
            ++from;

        const bool ends = from != i;

        unsigned word = UINT_MAX;

        if (ends && (rules.leaf_ends_walk || from == j))
            word = keys [i].leaf;
        else {

            bool valued = rules.page_for_blank_prefix;

            for (std::size_t k = from; k != j && !valued; ++k)
                valued = UINT_MAX != keys [k].leaf;

            // the key that ends here, if any, goes on as the leaf of
            // the page of the longer keys, or takes the word when they
            // get none
            const unsigned ending = ends ? keys [i].leaf : UINT_MAX;

            if (valued) {
                word = unsigned (pages.size ()) | trie_page_bit;
                build_page (keys, from, j, depth + 1, ending, rules, pages);
            }
            else
                word = ending;
        }

        if (UINT_MAX != word) {

            trie_page_t &filled = pages [number];

            filled.word [byte] = word;

            if (filled.first < 0)
                filled.first = byte;

            filled.last = byte;
        }

        i = j;
    }
}


void
build_trie (const std::vector<trie_key_t> &keys, const trie_rules_t &rules,
            std::vector<trie_page_t> &pages)
{
    // an empty key has no byte to index with and is skipped as the
    // old generators skipped it, never finding it among the bytes
    std::size_t lo = 0;

    while (lo != keys.size () && keys [lo].bytes.empty ())
        ++lo;

    build_page (keys, lo, keys.size (), 0, UINT_MAX, rules, pages);
}
