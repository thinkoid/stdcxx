/***************************************************************************
 *
 * trie.h
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

#ifndef RWSTD_UTIL_LOCALEDEF_TRIE_H_INCLUDED
#define RWSTD_UTIL_LOCALEDEF_TRIE_H_INCLUDED

#include <cstddef>   // for size_t
#include <string>
#include <vector>


// A trie as the codecvt and collate databases keep one: pages of 256
// words, one page per byte of a key prefix, numbered depth-first with
// page 0 the root, a page's subtrie numbered before its next sibling.
// The word at a byte is UINT_MAX when no key goes through it, the
// number of the next page with the high bit set when keys continue,
// and the key's leaf when a key ends there; one word, never both.
// The reader walks one page per byte of input, __rw_mbtowco() in
// _localedef.h, and locates a page as 256 words past the root times
// its number.

enum { trie_page_bit = 0x80000000 };

struct trie_key_t {
    std::string bytes;
    unsigned    leaf;     // the word at the key's last byte
};

struct trie_page_t {
    unsigned word [256];
    unsigned leaf;        // the leaf of the key that ends on arriving
                          // here, UINT_MAX when none
    int      first;       // the lowest byte with a word, -1 when none
    int      last;        // the highest, -1 when none
};


// How the keys that overlap are read; the two tries of a database
// differ in nothing else.
struct trie_rules_t {

    // A key that is a proper prefix of another: true, its leaf fills
    // the word and the longer keys are unreachable, as in the codecvt
    // and the character tries, where a complete character ends the
    // walk; false, the word continues to a page and the shorter key's
    // leaf is that page's leaf, as in the element tries.  A page is
    // reached by one path only, so no two keys share a page's leaf;
    // the root's is UINT_MAX, the empty key being skipped.  When the
    // longer keys get no page, every one without a value under
    // page_for_blank_prefix false, the shorter key's leaf fills the
    // word under either rule.
    bool leaf_ends_walk;

    // A prefix whose every key has the leaf UINT_MAX, a character that
    // is known but has no value: true, it gets a page like any other;
    // false, only the prefixes of keys with a value get pages.
    bool page_for_blank_prefix;
};


// The order the pages take: by byte as an unsigned value, a key before
// the keys it is a prefix of.
bool bytes_before (const std::string &a, const std::string &b);


// Appends the pages of keys in number order.  The keys are in the
// order of bytes_before(); of equal keys the first is the one kept.
void build_trie (const std::vector<trie_key_t> &keys,
                 const trie_rules_t &rules, std::vector<trie_page_t> &pages);

#endif   // RWSTD_UTIL_LOCALEDEF_TRIE_H_INCLUDED
