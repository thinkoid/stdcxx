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


// One character of a charmap, as a lookup or the iteration gives it.
// The code point of a <Uxxxx> name is the name's; any other name has
// one only in a UTF-8 charmap, from its bytes.  Elsewhere glibc takes
// it from a repertoire map, and so will a later step if one needs it.
struct cmap_entry_t {
    std::string name;    // the symbolic name, without the brackets
    long        ucs;     // the UCS-4 code point, or -1 when unknown
    std::string bytes;   // the encoding
};


// One line of the CHARMAP section, as read.
struct char_def_t;


// One line of the CHARMAP section as kept: a single character, or a
// range of <Uxxxx> names, which every range of glibc's charmaps is.
// A range is one record whatever its length: its members are named
// by their code points, and their bytes follow from the first's.
struct cmap_run_t {
    long        first;    // code point of the first member, -1 for none
    long        last;     // of the last; first for a single character
    int         digits;   // 4 or 8 of a <Uxxxx> name; 0 for another
    std::string name;     // a single's name, when not the <Uxxxx> form
    std::string bytes;    // of the first member; none when they are
                          // the code point's UTF-8 form
};


class cmap_iter_t;


// A charmap: one table of runs in the order the file defines them,
// and the indexes over it.  glibc's UTF-8 charmap, 297334 characters
// in 4185 ranges, costs one record a line.
class cmap_t
{
public:

    cmap_t ();

    // Reads the charmap at path.  False if it reported an error; the
    // definitions that were well-formed are kept.  Throws open_error_t
    // when path cannot be opened.
    bool read (const std::string &path, diag_t &diag);

    // False when nothing matches, and entry is then unchanged.
    // <U0041> and <U00000041> are one name.
    bool find_name (const std::string &name, cmap_entry_t &entry) const;
    bool find_ucs (long ucs, cmap_entry_t &entry) const;
    bool find_bytes (const std::string &bytes, cmap_entry_t &entry) const;

    std::string code_set_name;
    int         mb_cur_max;
    int         mb_cur_min;

private:

    friend class cmap_iter_t;

    bool define (const char_def_t &def, diag_t &diag);
    void add (const std::string &name, const std::string &bytes);
    bool index (const cmap_run_t &run, std::size_t pos);

    std::size_t rank (long ucs) const;
    std::size_t run_of (long ucs) const;
    void        expand (const cmap_run_t &run, long ucs,
                        cmap_entry_t &entry) const;

    bool m_utf8;   // a range member's bytes are its code point's

    // The indexes hold positions in m_runs, which only grows.  A code
    // point has one run, the first that defines it, in a vector in
    // code point order for a binary search; a name that is not
    // <Uxxxx> has its own index.  The index by bytes is built when
    // first wanted, and not at all in a UTF-8 charmap, where the
    // bytes decode to the code point.
    std::vector<cmap_run_t>            m_runs;
    std::vector<std::size_t>           m_by_ucs;
    std::map<std::string, std::size_t> m_by_name;
    mutable std::vector<std::size_t>   m_by_bytes;
};


// The characters of a charmap one at a time, in the order the file
// defines them, a range expanded to its members.
class cmap_iter_t
{
public:

    explicit cmap_iter_t (const cmap_t &cmap)
        : m_cmap (cmap), m_run (0), m_offset (0) { }

    // False past the last character, and entry is then unchanged.
    bool next (cmap_entry_t &entry);

private:

    const cmap_t &m_cmap;
    std::size_t   m_run;      // the run of the next character
    long          m_offset;   // and its distance from the run's first
};


// What a string resolves against: the charmap, for a symbolic name,
// and the escape character in effect on the string's line.
struct charset_t {
    const cmap_t *cmap;
    char          escape;
};


// The UTF-8 form of a code point, up to the six bytes of the old
// definition, which the tree's UTF-8 charmap still uses.
std::string utf8_of (long ucs);


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
