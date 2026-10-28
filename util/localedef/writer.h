/***************************************************************************
 *
 * writer.h
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

#ifndef RWSTD_UTIL_LOCALEDEF_WRITER_H_INCLUDED
#define RWSTD_UTIL_LOCALEDEF_WRITER_H_INCLUDED

#include "cmap.h"
#include "model.h"

#include <string>
#include <vector>


// The writers of the category images, one per file of a locale
// database, each from the model and the charmap to the bytes of the
// file, in the format include/loc/_localedef.h describes and the old
// compiler wrote.  A keyword the source leaves unset takes the old
// compiler's default here, not in the model: an empty string, CHAR_MAX
// for the small integers of LC_MONETARY, CHAR_MAX alone for a grouping.
// The narrow form of every string goes through the charmap and the
// transliteration table of LC_CTYPE, as pool.h says.

// The two names every image ends with.
struct image_names_t {
    std::string codeset;   // the charmap's <code_set_name>
    std::string charmap;   // the charmap file's name, without directories
};

void write_numeric (const locale_def_t &def, const cmap_t &cmap,
                    const image_names_t &names, std::vector<char> &image);

void write_monetary (const locale_def_t &def, const cmap_t &cmap,
                     const image_names_t &names, std::vector<char> &image);

void write_time (const locale_def_t &def, const cmap_t &cmap,
                 const image_names_t &names, std::vector<char> &image);

void write_messages (const locale_def_t &def, const cmap_t &cmap,
                     const image_names_t &names, std::vector<char> &image);


// The punctuation header the two numeric images open with, and its
// strings, appended to image: decimal point and thousands separator,
// wide then narrow, and the grouping.  Defined with the numeric
// writer, used by the monetary one.
void write_punct (const field_t<ucs_string_t> &decimal_point,
                  const field_t<ucs_string_t> &thousands_sep,
                  const field_t<std::vector<int> > &grouping,
                  const cmap_t &cmap, const translit_t &translit,
                  std::vector<char> &image);

#endif   // RWSTD_UTIL_LOCALEDEF_WRITER_H_INCLUDED
