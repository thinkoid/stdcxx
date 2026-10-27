/***************************************************************************
 *
 * lctype.h
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

#ifndef RWSTD_UTIL_LOCALEDEF_LCTYPE_H_INCLUDED
#define RWSTD_UTIL_LOCALEDEF_LCTYPE_H_INCLUDED

#include "cmap.h"
#include "diag.h"
#include "model.h"
#include "reader.h"

#include <set>
#include <string>
#include <vector>


// One line of a translit_start section: a source and its alternatives.
struct translit_entry_t {
    ucs_string_t              from;
    std::vector<ucs_string_t> to;
};


// An LC_CTYPE being read.  glibc keeps one state for a locale and the
// locales it copies, so a copy reads into the state of the locale that
// copies it, and an include into a state of its own.
struct ctype_state_t {

    ctype_state_t (ctype_def_t *d, std::set<std::string> &v)
        : def (d), visited (v) { }

    // The classes and maps, or 0 when the locale is read for its
    // transliteration alone, as an include is.
    ctype_def_t *def;

    // The entries of the translit_start sections, in the order read.
    std::vector<translit_entry_t> own;

    // The entries the includes bring, in glibc's order: the last of
    // them for a source wins.
    std::vector<translit_entry_t> included;

    // The names charclass, charconv, class and map declare, whose
    // statements are skipped unread.
    std::set<std::string> names;

    // The locales included so far in the run: glibc takes the entries
    // of each once.
    std::set<std::string> &visited;
};


// The reading of the LC_CTYPE of another locale, for copy and include.
// It is the caller's, since the caller knows where locales are and how
// a file is read for one category.
class ctype_locales_t
{
public:

    // Reads the LC_CTYPE of the locale name into state.  at is the
    // statement's leading token, for a diagnostic.
    virtual void read (const std::string &name, const token_t &at,
                       ctype_state_t &state) = 0;

protected:

    ~ctype_locales_t () { }
};


// Reads the lines of LC_CTYPE after head, the line of the category
// keyword, up to and including the END line, or to the end of head's
// file.  A copy, which must come first, and an include are read when
// they are met, through locales.
void read_ctype (reader_t &reader, const line_t &head, const cmap_t &cmap,
                 ctype_state_t &state, ctype_locales_t &locales,
                 diag_t &diag);

// The transliteration table of a state read whole, by glibc's order of
// precedence.
void finish_translit (const ctype_state_t &state, translit_t &table);

#endif   // RWSTD_UTIL_LOCALEDEF_LCTYPE_H_INCLUDED
