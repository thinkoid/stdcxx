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
// readers; LC_CTYPE and LC_COLLATE are skipped until their readers
// come; the six categories no facet reads are skipped whole.  The
// comment_char and escape_char lines are the reader's.
//
// include is not read here.  glibc reads it in LC_CTYPE alone, inside
// translit_start, and none of the four scalar categories of the 502
// definitions of glibc's SUPPORTED has one.

#include "locfile.h"
#include "reader.h"
#include "scalar.h"

#include <vector>


// The categories a file is read for.  The definition named on the
// command line is read for all of them; a file read for a copy is read
// for the one category copied, and its other sections are skipped.
enum {
    numeric_bit  = 1,
    monetary_bit = 2,
    time_bit     = 4,
    messages_bit = 8,
    all_bits     = 15
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


bool
read_locale (const std::string &path, const std::string &dir,
             const cmap_t &cmap, locale_def_t &def, diag_t &diag)
{
    const int errors = diag.errors ();

    reader_t reader (diag);
    reader.push (path, locale_file);

    // the categories wanted from the file at each depth, from 1
    std::vector<int> wanted (1, int (all_bits));

    for (line_t line; reader.next (line); ) {

        wanted.resize (reader.depth ());

        tok_iter       it   = line.tokens.begin ();
        const tok_iter last = line.tokens.end ();

        const token_t &head = *it;

        std::string copy;

        switch (keyword_of (head)) {

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

        case kw_lc_ctype:
        case kw_lc_collate:
            // steps 4 and 5 of the plan
            break;

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

    return errors == diag.errors ();
}
