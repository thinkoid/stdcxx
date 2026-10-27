/***************************************************************************
 *
 * cmap.h
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

#ifndef RWSTD_UTIL_LOCALEDEF_CMAP_H_INCLUDED
#define RWSTD_UTIL_LOCALEDEF_CMAP_H_INCLUDED

#include "diag.h"
#include "model.h"
#include "reader.h"

#include <cstddef>   // for size_t
#include <map>
#include <string>
#include <vector>


// One character of a charmap.  The code point of a <Uxxxx> name is the
// name's; any other name has one only in a UTF-8 charmap, from its
// bytes.  Elsewhere glibc takes it from a repertoire map, and so will a
// later step if one needs it.
struct cmap_entry_t {
    std::string name;    // the symbolic name, without the brackets
    long        ucs;     // the UCS-4 code point, or -1 when unknown
    std::string bytes;   // the encoding
};


// One line of the CHARMAP section, as read.
struct char_def_t;


// A charmap: one table of entries in the order the file defines them,
// and three indexes over it.
class cmap_t
{
public:

    cmap_t ();

    // Reads the charmap at path.  False if it reported an error; the
    // definitions that were well-formed are kept.  Throws open_error_t
    // when path cannot be opened.
    bool read (const std::string &path, diag_t &diag);

    const std::vector<cmap_entry_t>& entries () const {
        return m_entries;
    }

    // 0 when nothing matches.  <U0041> and <U00000041> are one name.
    const cmap_entry_t* find_name (const std::string &name) const;
    const cmap_entry_t* find_ucs (long ucs) const;
    const cmap_entry_t* find_bytes (const std::string &bytes) const;

    std::string code_set_name;
    int         mb_cur_max;
    int         mb_cur_min;

private:

    bool define (const char_def_t &def, diag_t &diag);
    void add (const std::string &name, const std::string &bytes);

    // The indexes hold positions in m_entries, which only grows.
    std::vector<cmap_entry_t>          m_entries;
    std::map<std::string, std::size_t> m_by_name;
    std::map<long, std::size_t>        m_by_ucs;
    std::map<std::string, std::size_t> m_by_bytes;
};


// What a string resolves against: the charmap, for a symbolic name,
// and the escape character in effect on the string's line.
struct charset_t {
    const cmap_t *cmap;
    char          escape;
};


// The code points of a string token, by glibc's get_string() for a
// locale file.  The escape character takes the next character as
// itself: "//" is one slash under escape_char /.  A string has no
// charcode form, so "\x41" is the three characters x41, as in glibc.
// A <name> of U and 4 or 8 hexadecimal digits is that code point
// whether or not the charmap has the name; any other name is the
// charmap's.  A byte from 0x80 starts a character written raw in
// UTF-8, which reads as its <Uxxxx> name.
bool resolve (const token_t &tok, const charset_t &cs, ucs_string_t &ucs,
              diag_t &diag);

#endif   // RWSTD_UTIL_LOCALEDEF_CMAP_H_INCLUDED
