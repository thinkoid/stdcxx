/***************************************************************************
 *
 * lcollate.h
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

#ifndef RWSTD_UTIL_LOCALEDEF_LCOLLATE_H_INCLUDED
#define RWSTD_UTIL_LOCALEDEF_LCOLLATE_H_INCLUDED

#include "cmap.h"
#include "diag.h"
#include "model.h"
#include "reader.h"

#include <map>
#include <set>
#include <string>
#include <vector>


// One entry of the sequence being read and its links.  The sequence is
// a list threaded through a vector by index, as glibc threads its
// elements: an entry goes in after the cursor at once, and the index
// of an entry stays valid while others go in around it.
struct coll_node_t {
    coll_entry_t entry;
    int          prev;       // -1 at the head
    int          next;       // -1 at the tail
    bool         linked;     // false once taken out for good
    bool         declared;   // a collating-symbol statement named it
};


// An LC_COLLATE being read.  glibc keeps one state for a locale and the
// locales it copies, so a copy reads into the state of the locale that
// copies it, where the copy stands, and so do the statements after it.
struct collate_state_t {

    explicit collate_state_t (collate_def_t &d)
        : def (d), head (-1), cursor (-1), undefined (-1), levels (0),
          section (-1) { }

    collate_def_t &def;

    std::vector<coll_node_t> nodes;
    int                      head;
    int                      cursor;    // glibc's: where the next entry
                                        // goes, after it; -1 when unset
    int                      undefined; // the node of UNDEFINED, or -1

    // The entries in the sequence by name, and the nodes that hold a
    // range, whose members have no node of their own.
    std::map<std::string, int> placed;
    std::vector<int>           ranges;

    // The number of levels, glibc's nrules: set by the first
    // order_start of the run, 0 until then.
    std::size_t levels;

    // The section entries are placed in, -1 before the first, and for
    // each section whether one was: glibc reopens no section that has.
    int               section;
    std::vector<bool> section_used;

    // The declared names: single symbols, the declarations that are
    // ranges, by their index in def.symbols, and the elements, by their
    // index in def.elements.
    std::set<std::string>              symbols;
    std::vector<std::size_t>           symbol_ranges;
    std::map<std::string, std::size_t> elements;

    // The names define sets, glibc's one list for the run: a define
    // before a copy is seen in the locale copied.
    std::set<std::string> defined;

    // The locales copied so far.  glibc reads a locale once in a run,
    // so that a second copy of one reads nothing: om_ET copies am_ET
    // and om_KE, and both copy iso14651_t1.
    std::set<std::string> copied;

    // The files entries come from, by the reader's name, to def.files.
    std::map<const char*, int> files;
};


// The reading of the LC_COLLATE of another locale, for copy.  It is the
// caller's, since the caller knows where locales are and how a file is
// read for one category.
class collate_locales_t
{
public:

    // Reads the LC_COLLATE of the locale name into state.  at is the
    // statement's leading token, for a diagnostic.
    virtual void read (const std::string &name, const token_t &at,
                       collate_state_t &state) = 0;

protected:

    ~collate_locales_t () { }
};


// Reads the lines of LC_COLLATE after head, the line of the category
// keyword, up to and including the END line, or to the end of head's
// file.  A copy is read where it is met, through locales.
void read_collate (reader_t &reader, const line_t &head, const cmap_t &cmap,
                   collate_state_t &state, collate_locales_t &locales,
                   diag_t &diag);

// The sequence of a state read whole, into its definition, in order.
void finish_collate (collate_state_t &state);

#endif   // RWSTD_UTIL_LOCALEDEF_LCOLLATE_H_INCLUDED
