/***************************************************************************
 *
 * wctype.cpp
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


// LC_CTYPE: the __rw_ctype_t header, which holds the narrow tables,
// then three wide arrays in ascending order of character, the
// (lower, upper) pairs of toupper, the (upper, lower) pairs of
// tolower and the (character, mask) elements, then the two names.  A
// wide element is written for every character the charmap has that a
// class holds, a narrow entry for each of those the charmap encodes
// in one byte, and a case pair when the charmap has both sides; a
// character the source names and the charmap lacks is left out, as
// the old compiler left it out of a symbolic range.
//
// The classes written are the source's with POSIX's automatic
// inclusions, XBD 7.3.1, applied here rather than in the model: the
// portable letters, digits and hexadecimal digits into upper, lower,
// digit and xdigit, the six portable white-space characters and the
// members of blank into space, upper and lower into alpha, those and
// alpha, digit, xdigit and punct into graph, and graph with the one
// <space> into print.  The old compiler's defaults were a part of
// that list; the census counts what the whole of it adds.  alnum has
// no bit: the facets read it as alpha and digit.  blank has no bit
// in ctype_base before C++11 and takes the free bit 0x400, which no
// facet's mask names, so it stays unread until one does.

#include "pool.h"
#include "writer.h"

#include <loc/_localedef.h>

#include <algorithm>   // for lower_bound(), sort(), unique()
#include <cstring>     // for memset()
#include <locale>      // for ctype_base
#include <utility>     // for pair


typedef std::ctype_base ctype_base;

enum { mask_blank = 0x400 };


// The eleven classes, their bits and their fields of the model, in
// the order of ctype_census_t.
struct class_bit_t {
    const char                        *name;
    unsigned                           bit;
    field_t<ucs_class_t> ctype_def_t::*member;
};

static const class_bit_t class_bits [ctype_class_count] = {
    { "upper",  ctype_base::upper,  &ctype_def_t::upper  },
    { "lower",  ctype_base::lower,  &ctype_def_t::lower  },
    { "alpha",  ctype_base::alpha,  &ctype_def_t::alpha  },
    { "digit",  ctype_base::digit,  &ctype_def_t::digit  },
    { "xdigit", ctype_base::xdigit, &ctype_def_t::xdigit },
    { "space",  ctype_base::space,  &ctype_def_t::space  },
    { "print",  ctype_base::print,  &ctype_def_t::print  },
    { "graph",  ctype_base::graph,  &ctype_def_t::graph  },
    { "blank",  mask_blank,         &ctype_def_t::blank  },
    { "cntrl",  ctype_base::cntrl,  &ctype_def_t::cntrl  },
    { "punct",  ctype_base::punct,  &ctype_def_t::punct  }
};


const char*
ctype_class_name (std::size_t index)
{
    return class_bits [index].name;
}


// One character of the wide table while it is built.
struct elem_t {
    long     ch;
    unsigned mask;
};

static bool
by_char (const elem_t &a, const elem_t &b)
{
    return a.ch < b.ch;
}

static bool
same_char (const elem_t &a, const elem_t &b)
{
    return a.ch == b.ch;
}


// Whether a class holds the code point.  The runs are in ascending
// order and disjoint, as the reader keeps them.
static bool
holds (const ucs_class_t &cls, long ucs)
{
    // the first run that ends at or after ucs
    std::size_t lo = 0;
    std::size_t hi = cls.size ();

    while (lo < hi) {

        const std::size_t mid = (lo + hi) / 2;

        if (cls [mid].last < ucs)
            lo = mid + 1;
        else
            hi = mid;
    }

    return lo < cls.size () && cls [lo].first <= ucs;
}


// The bits POSIX gives a character of the portable character set in
// every locale, whatever the source says: the letters, digits and
// hexadecimal digits their classes, the six white-space characters
// space, <space> and <tab> blank, and the one <space> print.  The
// portable characters are named by their code points, which the
// character literals are on every target the compiler is built for.
static unsigned
portable_bits (long ucs)
{
    unsigned bits = 0;

    if ('A' <= ucs && ucs <= 'Z')
        bits |= ctype_base::upper;

    if ('a' <= ucs && ucs <= 'z')
        bits |= ctype_base::lower;

    if ('0' <= ucs && ucs <= '9')
        bits |= ctype_base::digit | ctype_base::xdigit;

    if (('A' <= ucs && ucs <= 'F') || ('a' <= ucs && ucs <= 'f'))
        bits |= ctype_base::xdigit;

    switch (ucs) {
    case ' ':
        bits |= ctype_base::space | ctype_base::print;
        bits |= mask_blank;
        break;
    case '\t':
        bits |= ctype_base::space;
        bits |= mask_blank;
        break;
    case '\f': case '\n': case '\r': case '\v':
        bits |= ctype_base::space;
        break;
    }

    return bits;
}


// The classes of every character the charmap has, in ascending order
// of code point, one element per code point: those a class holds,
// and those the portable rule gives a bit whether or not the source
// names them.  Alongside, the single-byte characters and their bytes,
// for the narrow table; a code point the charmap encodes twice has
// both bytes.
static void
collect (const ctype_def_t &ctype, const cmap_t &cmap,
         std::vector<elem_t> &elems,
         std::vector<std::pair<long, unsigned char> > &narrow)
{
    cmap_iter_t  iter (cmap);
    cmap_entry_t entry;

    while (iter.next (entry)) {

        // a name without a code point can be in no class
        if (entry.ucs < 0)
            continue;

        elem_t elem = { entry.ucs, 0 };

        for (std::size_t i = 0; i != ctype_class_count; ++i)
            if (holds ((ctype.*class_bits [i].member).value, entry.ucs))
                elem.mask |= class_bits [i].bit;

        if (0 == elem.mask && 0 == portable_bits (entry.ucs))
            continue;

        elems.push_back (elem);

        if (1 == entry.bytes.size ())
            narrow.push_back (
                std::make_pair (entry.ucs, (unsigned char)entry.bytes [0]));
    }

    std::sort (elems.begin (), elems.end (), by_char);
    elems.erase (std::unique (elems.begin (), elems.end (), same_char),
                 elems.end ());
}


// POSIX's automatic inclusions on one character's mask, each class
// after the ones it takes from, and the census of the bits they add.
static void
include_defaults (long ucs, unsigned &mask, ctype_census_t &census)
{
    const unsigned before = mask;

    mask |= portable_bits (ucs);

    if (mask & mask_blank)
        mask |= ctype_base::space;

    if (mask & (ctype_base::upper | ctype_base::lower))
        mask |= ctype_base::alpha;

    if (mask & (  ctype_base::upper | ctype_base::lower | ctype_base::alpha
                | ctype_base::digit | ctype_base::xdigit | ctype_base::punct))
        mask |= ctype_base::graph;

    // graph holds its classes by now and <space> came with the
    // portable bits, so print takes graph alone
    if (mask & ctype_base::graph)
        mask |= ctype_base::print;

    for (std::size_t i = 0; i != ctype_class_count; ++i)
        if ((mask & ~before) & class_bits [i].bit)
            ++census.added [i];
}


// The mask of a code point in the wide table, 0 for one it lacks.
static unsigned
mask_of (const std::vector<elem_t> &elems, long ucs)
{
    const elem_t key = { ucs, 0 };

    const std::vector<elem_t>::const_iterator it =
        std::lower_bound (elems.begin (), elems.end (), key, by_char);

    return it != elems.end () && it->ch == ucs ? it->mask : 0;
}


// The narrow form of a case map: the pairs whose two sides the
// charmap encodes in one byte each, a later pair for a byte over an
// earlier one, and every byte without one mapped to itself, which is
// how the old compiler filled the entries left at zero.
static void
narrow_map (const ucs_map_t &map, const cmap_t &cmap,
            _RWSTD_UINT8_T table [256])
{
    cmap_entry_t from;
    cmap_entry_t to;

    for (std::size_t i = 0; i != map.size (); ++i)
        if (   cmap.find_ucs (map [i].from, from) && 1 == from.bytes.size ()
            && cmap.find_ucs (map [i].to, to) && 1 == to.bytes.size ())
            table [(unsigned char)from.bytes [0]] = to.bytes [0];

    for (unsigned i = 0; i != 256; ++i)
        if (0 == table [i])
            table [i] = (unsigned char)i;
}


// The wide form of a case map: the pairs whose two sides the charmap
// has, in the model's order, which is ascending by the character
// mapped from.  An __rw_upper_elm and an __rw_lower_elm are both the
// pair (from, to) of their map.
static void
wide_map (const ucs_map_t &map, const cmap_t &cmap, pool_t &pool)
{
    cmap_entry_t from;
    cmap_entry_t to;

    for (std::size_t i = 0; i != map.size (); ++i) {

        if (   !cmap.find_ucs (map [i].from, from)
            || !cmap.find_ucs (map [i].to, to))
            continue;

        const wchar_t pair [2] = {
            wchar_t (map [i].from), wchar_t (map [i].to)
        };

        pool.add (pair, sizeof pair);
    }
}


void
write_ctype (const locale_def_t &def, const cmap_t &cmap,
             const image_names_t &names, std::vector<char> &image,
             ctype_census_t &census)
{
    const ctype_def_t &ctype = def.ctype;

    std::memset (&census, 0, sizeof census);

    std::vector<elem_t>                           elems;
    std::vector<std::pair<long, unsigned char> >  narrow;

    collect (ctype, cmap, elems, narrow);

    for (std::size_t i = 0; i != elems.size (); ++i)
        include_defaults (elems [i].ch, elems [i].mask, census);

    __rw::__rw_ctype_t hdr;
    std::memset (&hdr, 0, sizeof hdr);

    // a byte the charmap gives two code points holds the bits of both
    for (std::size_t i = 0; i != narrow.size (); ++i)
        hdr.mask_tab [narrow [i].second] |= mask_of (elems, narrow [i].first);

    narrow_map (ctype.toupper.value, cmap, hdr.toupper_tab);
    narrow_map (ctype.tolower.value, cmap, hdr.tolower_tab);

    hdr.mb_cur_max = (unsigned char)cmap.mb_cur_max;

    pool_t pool;

    hdr.wtoupper_off = pool.size ();
    wide_map (ctype.toupper.value, cmap, pool);

    hdr.wtolower_off = pool.size ();
    wide_map (ctype.tolower.value, cmap, pool);

    hdr.wmask_off = pool.size ();
    hdr.wmask_s   = unsigned (elems.size ());

    for (std::size_t i = 0; i != elems.size (); ++i) {

        const __rw::__rw_mask_elm elm = {
            wchar_t (elems [i].ch), elems [i].mask
        };

        pool.add (&elm, sizeof elm);
    }

    hdr.codeset_off = pool.add (names.codeset);
    hdr.charmap_off = pool.add (names.charmap);

    image.clear ();

    append (image, &hdr, sizeof hdr);
    pool.append_to (image);
}
