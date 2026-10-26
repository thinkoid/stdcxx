/***************************************************************************
 *
 * ldef.cpp
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

// The new locale compiler.  For now it has two forms, each the check of
// one piece of the front end:
//
//   ldef --tokens FILE...   the tokens of each line
//   ldef --charmap FILE     the entries of a charmap

#include "cmap.h"
#include "diag.h"
#include "reader.h"

#include <cstdio>    // for printf(), fwrite()
#include <cstring>   // for strcmp(), strerror()
#include <set>
#include <string>


static void
report (const open_error_t &error, diag_t &diag)
{
    diag.error (e_openrd, nowhere, "%s could not be opened for reading: %s",
                error.path.c_str (), std::strerror (error.error));
}


// The command line names no kind of file, so the dump takes it from
// where the file sits: in a directory named charmaps, as both trees of
// sources keep them, a file is a charmap.
static file_kind_t
kind_of (const std::string &path)
{
    const std::string::size_type slash = path.rfind ('/');

    if (std::string::npos == slash)
        return locale_file;

    const std::string::size_type start = path.rfind ('/', slash - 1);
    const std::string::size_type first =
        std::string::npos == start ? 0 : start + 1;

    return 0 == path.compare (first, slash - first, "charmaps")
        ? charmap_file : locale_file;
}


// Prints "file:line: kind text" per token.  Then the counts, and the
// words that lead a line without being keywords: a statement the
// keyword table does not know.
static void
dump_tokens (int argc, char *argv [], diag_t &diag)
{
    std::set<std::string> words;

    long files  = 0;
    long lines  = 0;
    long tokens = 0;
    long idents = 0;

    for (int i = 2; i < argc; ++i) {

        reader_t reader (diag);

        try {
            reader.push (argv [i], kind_of (argv [i]));
        }
        catch (const open_error_t &error) {
            report (error, diag);
            continue;
        }

        ++files;

        for (line_t line; reader.next (line); ) {

            ++lines;

            for (tok_iter it = line.tokens.begin ();
                 it != line.tokens.end (); ++it) {

                std::printf ("%s:%d: %s ", line.file, line.number,
                             kind_name (it->kind));
                std::fwrite (it->text.data (), 1, it->text.size (), stdout);
                std::putchar ('\n');

                ++tokens;
            }

            if (tk_ident == line.tokens.front ().kind) {
                ++idents;
                words.insert (line.tokens.front ().text);
            }
        }
    }

    std::printf ("files %ld, lines %ld, tokens %ld\n", files, lines, tokens);
    std::printf ("idents at line start %ld, distinct %lu\n",
                 idents, (unsigned long)words.size ());

    for (std::set<std::string>::const_iterator it = words.begin ();
         it != words.end (); ++it)
        std::printf ("    %s\n", it->c_str ());
}


// Prints "name codepoint bytes" per entry, the code point in
// hexadecimal or - when unknown, the bytes in hexadecimal.
static void
dump_charmap (const char *path, diag_t &diag)
{
    cmap_t cmap;

    try {
        cmap.read (path, diag);
    }
    catch (const open_error_t &error) {
        report (error, diag);
        return;
    }

    const std::vector<cmap_entry_t> &entries = cmap.entries ();

    for (std::size_t i = 0; i != entries.size (); ++i) {

        const cmap_entry_t &entry = entries [i];

        std::printf ("<%s> ", entry.name.c_str ());

        if (entry.ucs < 0)
            std::printf ("- ");
        else
            std::printf ("%04lX ", entry.ucs);

        for (std::size_t j = 0; j != entry.bytes.size (); ++j)
            std::printf ("%02x", (unsigned char)entry.bytes [j]);

        std::putchar ('\n');
    }
}


int
main (int argc, char *argv [])
{
    diag_t diag;

    if (2 < argc && 0 == std::strcmp ("--tokens", argv [1]))
        dump_tokens (argc, argv, diag);
    else if (3 == argc && 0 == std::strcmp ("--charmap", argv [1]))
        dump_charmap (argv [2], diag);
    else
        diag.error (e_cmdarg, nowhere,
                    "usage: ldef --tokens FILE... | ldef --charmap FILE");

    return diag.exit_status ();
}
