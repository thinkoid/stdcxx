/***************************************************************************
 *
 * wnumeric.cpp
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

// LC_NUMERIC: a punctuation header and its strings, then the numeric
// header and its own, as __rw_punct_t and __rw_num_t lay them out.
// The numeric header's offsets count from its own end, not from the
// punctuation header's, and it sits on a word boundary after the
// punctuation strings.

#include "pool.h"
#include "writer.h"

#include <loc/_localedef.h>

#include <cstring>   // for memset()


void
write_punct (const field_t<ucs_string_t> &decimal_point,
             const field_t<ucs_string_t> &thousands_sep,
             const field_t<std::vector<int> > &grouping,
             const cmap_t &cmap, const translit_t &translit,
             std::vector<char> &image)
{
    __rw::__rw_punct_t punct;
    std::memset (&punct, 0, sizeof punct);

    pool_t pool;

    // an unset separator is the empty string, which an unset field
    // already holds
    punct.decimal_point_off [1] = pool.add (decimal_point.value);
    punct.thousands_sep_off [1] = pool.add (thousands_sep.value);
    punct.decimal_point_off [0] =
        pool.add (narrow (decimal_point.value, cmap, translit));
    punct.thousands_sep_off [0] =
        pool.add (narrow (thousands_sep.value, cmap, translit));
    punct.grouping_off          = pool.add (grouping_bytes (grouping));

    // the header that follows is read in place, so it starts on a
    // word boundary
    const unsigned align = sizeof punct.punct_ext_off;

    pool.reserve ((align - pool.size () % align) % align);
    punct.punct_ext_off = pool.size ();

    append (image, &punct, sizeof punct);
    pool.append_to (image);
}


void
write_numeric (const locale_def_t &def, const cmap_t &cmap,
               const image_names_t &names, std::vector<char> &image)
{
    const numeric_def_t &num      = def.numeric;
    const translit_t    &translit = def.ctype.translit;

    image.clear ();

    write_punct (num.decimal_point, num.thousands_sep, num.grouping,
                 cmap, translit, image);

    __rw::__rw_num_t hdr;
    std::memset (&hdr, 0, sizeof hdr);

    pool_t pool;

    // no source sets the names of true and false; the old compiler
    // wrote them empty, the wide ones first
    const ucs_string_t none;

    hdr.truename_off [1]  = pool.add (none);
    hdr.falsename_off [1] = pool.add (none);
    hdr.truename_off [0]  = pool.add (std::string ());
    hdr.falsename_off [0] = pool.add (std::string ());
    hdr.codeset_off       = pool.add (names.codeset);
    hdr.charmap_off       = pool.add (names.charmap);

    append (image, &hdr, sizeof hdr);
    pool.append_to (image);
}
