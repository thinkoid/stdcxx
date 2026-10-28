/***************************************************************************
 *
 * pool.cpp
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

#include "pool.h"

#include <climits>   // for CHAR_MAX
#include <cstring>   // for memcpy()


unsigned pool_t::
add (const ucs_string_t &wide)
{
    const unsigned offset = size ();

    // the file holds the platform's wchar_t, which the facets read in
    // place
    for (std::size_t i = 0; i != wide.size (); ++i) {
        const wchar_t wc = wchar_t (wide [i]);
        add (&wc, sizeof wc);
    }

    const wchar_t nul = 0;
    add (&nul, sizeof nul);

    return offset;
}


unsigned pool_t::
add (const std::string &str)
{
    const unsigned offset = add (str.data (), str.size ());

    const char nul = 0;
    add (&nul, 1);

    return offset;
}


unsigned pool_t::
add (const void *bytes, std::size_t n)
{
    const unsigned     offset = size ();
    const char* const  first  = static_cast<const char*> (bytes);

    m_bytes.insert (m_bytes.end (), first, first + n);

    return offset;
}


unsigned pool_t::
reserve (std::size_t n)
{
    const unsigned offset = size ();

    m_bytes.insert (m_bytes.end (), n, char (0));

    return offset;
}


void pool_t::
put (unsigned offset, const void *bytes, std::size_t n)
{
    std::memcpy (&m_bytes [offset], bytes, n);
}


void pool_t::
append_to (std::vector<char> &image) const
{
    image.insert (image.end (), m_bytes.begin (), m_bytes.end ());
}


void
append (std::vector<char> &image, const void *bytes, std::size_t n)
{
    const char* const first = static_cast<const char*> (bytes);

    image.insert (image.end (), first, first + n);
}


/**************************************************************************/
// the narrow strings

// The bytes of one alternative, when the charmap has every code point
// of it; false otherwise.
static bool
narrow_alternative (const ucs_string_t &alt, const cmap_t &cmap,
                    std::string &bytes)
{
    cmap_entry_t entry;

    bytes.clear ();

    for (std::size_t i = 0; i != alt.size (); ++i) {

        if (!cmap.find_ucs (alt [i], entry))
            return false;

        bytes += entry.bytes;
    }

    return true;
}


// The bytes of the first alternative the charmap can write, as
// find_translit() in glibc's ld-ctype.c picks it; false when the
// table has no entry for the code point or no alternative fits.  The
// key is the one code point: glibc never looks a longer key up here.
static bool
transliterate (long ucs, const cmap_t &cmap, const translit_t &translit,
               std::string &bytes)
{
    const translit_t::const_iterator entry =
        translit.find (ucs_string_t (1, ucs));

    if (entry == translit.end ())
        return false;

    const std::vector<ucs_string_t> &alternatives = entry->second;

    for (std::size_t i = 0; i != alternatives.size (); ++i)
        if (narrow_alternative (alternatives [i], cmap, bytes))
            return true;

    return false;
}


std::string
narrow (const ucs_string_t &ucs, const cmap_t &cmap,
        const translit_t &translit)
{
    std::string  result;
    std::string  bytes;
    cmap_entry_t entry;

    for (std::size_t i = 0; i != ucs.size (); ++i) {

        if (cmap.find_ucs (ucs [i], entry)) {
            result += entry.bytes;
            continue;
        }

        if (!transliterate (ucs [i], cmap, translit, bytes))
            return std::string ();

        result += bytes;
    }

    return result;
}


std::string
grouping_bytes (const field_t<std::vector<int> > &grouping)
{
    if (!grouping.set)
        return std::string (1, char (CHAR_MAX));

    std::string bytes;

    for (std::size_t i = 0; i != grouping.value.size (); ++i)
        bytes += char (grouping.value [i]);

    return bytes;
}
