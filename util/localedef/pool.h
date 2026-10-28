/***************************************************************************
 *
 * pool.h
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

#ifndef RWSTD_UTIL_LOCALEDEF_POOL_H_INCLUDED
#define RWSTD_UTIL_LOCALEDEF_POOL_H_INCLUDED

#include "cmap.h"
#include "model.h"

#include <cstddef>   // for size_t
#include <string>
#include <vector>


// The strings of a category image.  The image is a header of offsets
// and the data after it, and every offset counts bytes from the end
// of the header, so the pool counts from zero: a string is added and
// its offset returned, and the order of addition is the order on
// disk.  The old compiler fixed that order for every category: the
// wide strings first, each a NUL-terminated array of wchar_t, which
// keeps them aligned since the header is a whole number of words;
// then the narrow strings, NUL-terminated; then the codeset name and
// the charmap name.  A record whose fields are themselves offsets, an
// era of LC_TIME, is reserved before the strings and filled through
// put() once they are placed.
class pool_t
{
public:

    // A wide string: one wchar_t per code point, then a NUL.
    unsigned add (const ucs_string_t &wide);

    // A narrow string and its NUL.
    unsigned add (const std::string &str);

    // n bytes as they are.
    unsigned add (const void *bytes, std::size_t n);

    // n zero bytes, to be filled through put().
    unsigned reserve (std::size_t n);

    void put (unsigned offset, const void *bytes, std::size_t n);

    // The offset the next addition gets.
    unsigned size () const {
        return unsigned (m_bytes.size ());
    }

    // Appends the pool to an image, after its header.
    void append_to (std::vector<char> &image) const;

private:

    std::vector<char> m_bytes;
};


// Appends n bytes, a header, to an image.
void append (std::vector<char> &image, const void *bytes, std::size_t n);


// The narrow form of a string: the charmap's bytes for each code
// point, or, when the charmap lacks it, the bytes of the first
// alternative of its transliteration entry whose every code point the
// charmap has, as glibc's localedef forms the narrow strings of a
// locale.  An empty alternative drops the character.  A code point
// with neither makes the whole string empty, which is what the old
// compiler wrote for it.
std::string narrow (const ucs_string_t &ucs, const cmap_t &cmap,
                    const translit_t &translit);

// A grouping as the facets read it: one byte per number as written,
// -1 as the byte 0xff and 0 kept, or CHAR_MAX alone when the source
// sets none, the old compiler's default.
std::string grouping_bytes (const field_t<std::vector<int> > &grouping);

#endif   // RWSTD_UTIL_LOCALEDEF_POOL_H_INCLUDED
