/***************************************************************************
 *
 * locfile.cpp
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

// The top level of a definition file: the keyword table of
// plan-localedef.md, chapter 6, as it applies to the statements that
// lead a section.  The categories the compiler reads go to their
// readers; the six categories no facet reads are skipped whole.  The
// comment_char and escape_char lines are the reader's.
//
// A scalar category's copy is read after the category's END, from the
// file pushed on the reader.  LC_CTYPE and LC_COLLATE read their copy
// where it is met, since statements may follow it, and so LC_CTYPE
// reads the locales its translit_start sections include: through the
// two read() of files_t, which read the named file at the depth above
// for the one category.  The two readers keep a callback each, so that
// neither header knows the other's state.

#include "lcollate.h"
#include "lctype.h"
#include "locfile.h"
#include "reader.h"
#include "scalar.h"

#include <set>
#include <vector>


// The categories a file is read for.  The definition named on the
// command line is read for all of them; a file read for a copy or an
// include is read for the one category, and its other sections are
// skipped.
enum {
    numeric_bit  = 1,
    monetary_bit = 2,
    time_bit     = 4,
    messages_bit = 8,
    ctype_bit    = 16,
    collate_bit  = 32,
    all_bits     = 63
};


// glibc sets no limit, and a locale that copies itself makes it loop.
static const std::size_t copy_depth_max = 16;


// From the line after head up to an END line, unread, or to the end of
// head's file.
static void
skip_section (reader_t &reader, const line_t &head, diag_t &diag)
{
    const token_t     &name  = head.tokens.front ();
    const std::size_t  depth = reader.depth ();

    where_t at_end = name.where;

    for (line_t line; reader.next (line, depth); ) {

        tok_iter       it   = line.tokens.begin ();
        const tok_iter last = line.tokens.end ();

        at_end = it->where;

        if (kw_end == keyword_of (*it)) {
            end_of_section (it, last, name, diag);
            return;
        }
    }

    diag.error (e_syntax, at_end, "%s: premature end of file",
                name.text.c_str ());
}


// Opens the locale a category copies, if it copies one, to be read
// next for that category alone.
static void
push_copy (reader_t &reader, const std::string &dir, const std::string &name,
           const token_t &head, int bit, std::vector<int> &wanted,
           diag_t &diag)
{
    if (name.empty ())
        return;

    if (copy_depth_max <= reader.depth ()) {
        diag.error (e_syntax, head.where, "%s: copy nested more than %d "
                    "deep", head.text.c_str (), int (copy_depth_max));
        return;
    }

    const std::string path = dir.empty () ? name : dir + '/' + name;

    try {
        reader.push (path, locale_file);
    }
    catch (const open_error_t&) {
        diag.error (e_openrd, head.where, "cannot open locale definition "
                    "file `%s'", path.c_str ());
        return;
    }

    wanted.push_back (bit);
}


// The files of one definition: the reader with its stack, the
// directory the locales copy and include name, and the categories
// wanted from the file at each depth, from 1.
struct files_t: ctype_locales_t, collate_locales_t {

    files_t (const std::string &d, const cmap_t &c, locale_def_t &l,
             diag_t &g)
        : reader (g), dir (d), cmap (c), def (l), diag (g) { }

    // The sections of the files at depth and above, LC_CTYPE into
    // ctype and LC_COLLATE into collate, each 0 when not wanted.
    void read_sections (std::size_t depth, ctype_state_t *ctype,
                        collate_state_t *collate);

    // Opens the locale a copy or an include names, at the depth above
    // the statement's, to be read for one category.
    bool open (const std::string &name, const token_t &at,
               const char *category, int bit);

    virtual void read (const std::string &name, const token_t &at,
                       ctype_state_t &state);

    virtual void read (const std::string &name, const token_t &at,
                       collate_state_t &state);

    reader_t           reader;
    const std::string &dir;
    const cmap_t      &cmap;
    locale_def_t      &def;
    diag_t            &diag;
    std::vector<int>   wanted;
};


void files_t::
read_sections (std::size_t depth, ctype_state_t *ctype,
               collate_state_t *collate)
{
    for (line_t line; reader.next (line, depth); ) {

        wanted.resize (reader.depth ());

        tok_iter       it   = line.tokens.begin ();
        const tok_iter last = line.tokens.end ();

        const token_t &head = *it;

        std::string copy;

        switch (keyword_of (head)) {

        case kw_lc_ctype:
            if (0 == (wanted.back () & ctype_bit))
                break;

            end_of_line (++it, last, diag);
            read_ctype (reader, line, cmap, *ctype, *this, diag);
            continue;

        case kw_lc_numeric:
            if (0 == (wanted.back () & numeric_bit))
                break;

            end_of_line (++it, last, diag);
            read_numeric (reader, line, cmap, def.numeric, copy, diag);
            push_copy (reader, dir, copy, head, numeric_bit, wanted, diag);
            continue;

        case kw_lc_monetary:
            if (0 == (wanted.back () & monetary_bit))
                break;

            end_of_line (++it, last, diag);
            read_monetary (reader, line, cmap, def.monetary, copy, diag);
            push_copy (reader, dir, copy, head, monetary_bit, wanted, diag);
            continue;

        case kw_lc_time:
            if (0 == (wanted.back () & time_bit))
                break;

            end_of_line (++it, last, diag);
            read_time (reader, line, cmap, def.time, copy, diag);
            push_copy (reader, dir, copy, head, time_bit, wanted, diag);
            continue;

        case kw_lc_messages:
            if (0 == (wanted.back () & messages_bit))
                break;

            end_of_line (++it, last, diag);
            read_messages (reader, line, cmap, def.messages, copy, diag);
            push_copy (reader, dir, copy, head, messages_bit, wanted, diag);
            continue;

        case kw_lc_collate:
            if (0 == (wanted.back () & collate_bit))
                break;

            end_of_line (++it, last, diag);
            read_collate (reader, line, cmap, *collate, *this, diag);
            continue;

        case kw_lc_address: case kw_lc_identification:
        case kw_lc_measurement: case kw_lc_name: case kw_lc_paper:
        case kw_lc_telephone:
            // no facet reads them
            break;

        case kw_repertoiremap:
            // the code points come from the <Uxxxx> names
            continue;

        default:
            if (tk_keyword == head.kind || tk_ident == head.kind)
                diag.warning (w_syntax, head.where, "`%s' is not read "
                              "outside a category; statement skipped",
                              head.text.c_str ());
            else
                diag.error (e_syntax, head.where, "syntax error: not "
                            "inside a locale definition section");
            continue;
        }

        skip_section (reader, line, diag);
    }
}


bool files_t::
open (const std::string &name, const token_t &at, const char *category,
      int bit)
{
    if (copy_depth_max <= reader.depth ()) {
        diag.error (e_syntax, at.where, "%s: copy and include nested more "
                    "than %d deep", category, int (copy_depth_max));
        return false;
    }

    const std::string path = dir.empty () ? name : dir + '/' + name;

    try {
        reader.push (path, locale_file);
    }
    catch (const open_error_t&) {
        diag.error (e_openrd, at.where, "cannot open locale definition "
                    "file `%s'", path.c_str ());
        return false;
    }

    wanted.resize (reader.depth () - 1);
    wanted.push_back (bit);

    return true;
}


// The LC_CTYPE of a locale a copy or an include names, read now.
void files_t::
read (const std::string &name, const token_t &at, ctype_state_t &state)
{
    if (open (name, at, "LC_CTYPE", ctype_bit))
        read_sections (reader.depth (), &state, 0);
}


// The LC_COLLATE of a locale a copy names, read now.
void files_t::
read (const std::string &name, const token_t &at, collate_state_t &state)
{
    if (open (name, at, "LC_COLLATE", collate_bit))
        read_sections (reader.depth (), 0, &state);
}


bool
read_locale (const std::string &path, const std::string &dir,
             const cmap_t &cmap, locale_def_t &def, diag_t &diag)
{
    const int errors = diag.errors ();

    files_t files (dir, cmap, def, diag);

    files.reader.push (path, locale_file);
    files.wanted.assign (1, int (all_bits));

    std::set<std::string> visited;
    ctype_state_t         ctype (&def.ctype, visited);
    collate_state_t       collate (def.collate);

    files.read_sections (1, &ctype, &collate);

    finish_translit (ctype, def.ctype.translit);
    finish_collate (collate);

    return errors == diag.errors ();
}
