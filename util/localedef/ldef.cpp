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

// The locale compiler.  It writes the locale database of one locale,
// and it has seven dump forms, each the check of one piece of the
// compiler:
//
//   localedef [-c] [-w] -f CHARMAP -i FILE [-d DIR] [-I DIR] NAME
//                                               the database DIR/NAME,
//                                               the six categories
//                                               and the codecvt
//                                               database
//   localedef --tokens FILE...                  the tokens of each line
//   localedef --charmap FILE                    the entries of a charmap
//   localedef --values -f CHARMAP -i FILE [-I DIR]
//                                               the values of the four
//                                               scalar categories
//   localedef --ctype -f CHARMAP -i FILE [-I DIR]
//                                               the classes, case maps
//                                               and transliteration
//   localedef --collate -f CHARMAP -i FILE [-I DIR]
//                                               the declarations and
//                                               the sequence of
//                                               LC_COLLATE
//   localedef --census -f CHARMAP -i FILE [-I DIR]
//                                               what POSIX's class
//                                               defaults add to
//                                               LC_CTYPE
//   localedef --collate-numbers -f CHARMAP -i FILE [-I DIR]
//                                               the collation numbering:
//                                               values, rows, weights

#include "cmap.h"
#include "collnum.h"
#include "diag.h"
#include "locfile.h"
#include "model.h"
#include "reader.h"
#include "writer.h"

#include <cerrno>    // for errno
#include <climits>   // for UINT_MAX
#include <cstdio>    // for printf(), fwrite(), fopen()
#include <cstdlib>   // for strtol()
#include <cstring>   // for strcmp(), strerror(), strrchr()
#include <ctime>     // for clock()
#include <set>
#include <string>
#include <vector>

#include <sys/stat.h>   // for mkdir(), stat()
#include <unistd.h>     // for symlink(), unlink()


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

    cmap_iter_t it (cmap);

    for (cmap_entry_t entry; it.next (entry); ) {

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


/**************************************************************************/
// the values of the scalar categories

// The dump prints one keyword=value line per keyword of the model, in
// the forms of glibc's locale -k, so that the two compare line by line:
// a string in quotes, an array of strings in one pair of quotes joined
// by semicolons, a list of strings each in its quotes, numbers plain.
// It differs where locale -k prints what the source does not say: an
// unset keyword prints <unset>, where locale -k prints glibc's default;
// a grouping prints as written, 0 included; a list prints all of its
// strings, where locale -k stops at the first empty one.

// The bytes of a string in the charmap's encoding.  A code point the
// charmap does not have prints as its <Uxxxx> name.
static void
print_ucs (const cmap_t &cmap, const ucs_string_t &ucs)
{
    cmap_entry_t entry;

    for (std::size_t i = 0; i != ucs.size (); ++i) {

        if (cmap.find_ucs (ucs [i], entry))
            std::fwrite (entry.bytes.data (), 1, entry.bytes.size (),
                         stdout);
        else
            std::printf ("<U%04lX>", ucs [i]);
    }
}


static bool
print_unset (const char *keyword, bool set)
{
    std::printf ("%s=", keyword);

    if (!set)
        std::puts ("<unset>");

    return set;
}


static void
print_string (const char *keyword, const cmap_t &cmap,
              const field_t<ucs_string_t> &field)
{
    if (!print_unset (keyword, field.set))
        return;

    std::putchar ('"');
    print_ucs (cmap, field.value);
    std::puts ("\"");
}


static void
print_array (const char *keyword, const cmap_t &cmap,
             const field_t<std::vector<ucs_string_t> > &field)
{
    if (!print_unset (keyword, field.set))
        return;

    std::putchar ('"');

    for (std::size_t i = 0; i != field.value.size (); ++i) {

        if (i)
            std::putchar (';');

        print_ucs (cmap, field.value [i]);
    }

    std::puts ("\"");
}


static void
print_list (const cmap_t &cmap, const std::vector<ucs_string_t> &list)
{
    for (std::size_t i = 0; i != list.size (); ++i) {

        std::fputs (i ? ";\"" : "\"", stdout);
        print_ucs (cmap, list [i]);
        std::putchar ('"');
    }

    std::putchar ('\n');
}


static void
print_number (const char *keyword, const field_t<int> &field)
{
    if (print_unset (keyword, field.set))
        std::printf ("%d\n", field.value);
}


static void
print_grouping (const char *keyword, const field_t<std::vector<int> > &field)
{
    if (!print_unset (keyword, field.set))
        return;

    for (std::size_t i = 0; i != field.value.size (); ++i)
        std::printf (i ? ";%d" : "%d", field.value [i]);

    std::putchar ('\n');
}


static void
print_date (const int date [3])
{
    std::printf ("%d/%d/%d", date [0], date [1], date [2]);
}


// After the era strings, one line per record: the fields parsed, the
// dates as stored.  locale -k prints no such line.
static void
print_eras (const cmap_t &cmap, const std::vector<era_t> &eras)
{
    std::vector<ucs_string_t> texts;

    for (std::size_t i = 0; i != eras.size (); ++i)
        texts.push_back (eras [i].text);

    std::printf ("era=");
    print_list (cmap, texts);

    for (std::size_t i = 0; i != eras.size (); ++i) {

        const era_t &era = eras [i];

        std::printf ("era-record=%c:%d:", era.direction, era.offset);
        print_date (era.start);
        std::putchar (':');
        print_date (era.end);
        std::putchar (':');
        print_ucs (cmap, era.name);
        std::putchar (':');
        print_ucs (cmap, era.format);
        std::putchar ('\n');
    }
}


// Reads the charmap and the definition, the locales it copies and
// includes taken from dir, or from the definition's own directory.
// False if a file could not be opened.
static bool
read_model (const char *charmap, const char *path, const char *dir,
            cmap_t &cmap, locale_def_t &def, diag_t &diag)
{
    std::string copy_dir;

    if (dir)
        copy_dir = dir;
    else {
        const char* const slash = std::strrchr (path, '/');
        copy_dir = slash ? std::string (path, slash) : std::string (".");
    }

    try {
        cmap.read (charmap, diag);
        read_locale (path, copy_dir, cmap, def, diag);
    }
    catch (const open_error_t &error) {
        report (error, diag);
        return false;
    }

    return true;
}


static void
dump_values (const char *charmap, const char *path, const char *dir,
             diag_t &diag)
{
    cmap_t       cmap;
    locale_def_t def;

    if (!read_model (charmap, path, dir, cmap, def, diag))
        return;

    const numeric_def_t &num = def.numeric;

    print_string ("decimal_point", cmap, num.decimal_point);
    print_string ("thousands_sep", cmap, num.thousands_sep);
    print_grouping ("grouping", num.grouping);

    const monetary_def_t &mon = def.monetary;

    print_string ("int_curr_symbol", cmap, mon.int_curr_symbol);
    print_string ("currency_symbol", cmap, mon.currency_symbol);
    print_string ("mon_decimal_point", cmap, mon.mon_decimal_point);
    print_string ("mon_thousands_sep", cmap, mon.mon_thousands_sep);
    print_grouping ("mon_grouping", mon.mon_grouping);
    print_string ("positive_sign", cmap, mon.positive_sign);
    print_string ("negative_sign", cmap, mon.negative_sign);
    print_number ("int_frac_digits", mon.int_frac_digits);
    print_number ("frac_digits", mon.frac_digits);
    print_number ("p_cs_precedes", mon.p_cs_precedes);
    print_number ("p_sep_by_space", mon.p_sep_by_space);
    print_number ("n_cs_precedes", mon.n_cs_precedes);
    print_number ("n_sep_by_space", mon.n_sep_by_space);
    print_number ("p_sign_posn", mon.p_sign_posn);
    print_number ("n_sign_posn", mon.n_sign_posn);
    print_number ("int_p_cs_precedes", mon.int_p_cs_precedes);
    print_number ("int_p_sep_by_space", mon.int_p_sep_by_space);
    print_number ("int_n_cs_precedes", mon.int_n_cs_precedes);
    print_number ("int_n_sep_by_space", mon.int_n_sep_by_space);
    print_number ("int_p_sign_posn", mon.int_p_sign_posn);
    print_number ("int_n_sign_posn", mon.int_n_sign_posn);

    const time_def_t &time = def.time;

    print_array ("abday", cmap, time.abday);
    print_array ("day", cmap, time.day);
    print_array ("abmon", cmap, time.abmon);
    print_array ("mon", cmap, time.mon);
    print_array ("am_pm", cmap, time.am_pm);
    print_string ("d_t_fmt", cmap, time.d_t_fmt);
    print_string ("d_fmt", cmap, time.d_fmt);
    print_string ("t_fmt", cmap, time.t_fmt);
    print_string ("t_fmt_ampm", cmap, time.t_fmt_ampm);
    print_eras (cmap, time.era);
    print_string ("era_d_fmt", cmap, time.era_d_fmt);

    if (print_unset ("alt_digits", time.alt_digits.set))
        print_list (cmap, time.alt_digits.value);

    print_string ("era_d_t_fmt", cmap, time.era_d_t_fmt);
    print_string ("era_t_fmt", cmap, time.era_t_fmt);

    const messages_def_t &msg = def.messages;

    print_string ("yesexpr", cmap, msg.yesexpr);
    print_string ("noexpr", cmap, msg.noexpr);
}


/**************************************************************************/
// the classes, maps and transliteration of LC_CTYPE

// The dump prints each class as a line "name N", N its runs, then one
// run per line as first-last in hexadecimal, a single character as a
// run of one; each case map as "name N" and one pair per line, from
// and to in hexadecimal; then "translit N" and one entry per line: the
// source in angle brackets, then the alternatives separated by
// semicolons, each its code points in hexadecimal separated by spaces.
// A class or a map the source leaves unset prints "name <unset>".

static void
print_class (const char *name, const field_t<ucs_class_t> &field)
{
    if (!field.set) {
        std::printf ("%s <unset>\n", name);
        return;
    }

    std::printf ("%s %lu\n", name, (unsigned long)field.value.size ());

    for (std::size_t i = 0; i != field.value.size (); ++i)
        std::printf ("%04lX-%04lX\n", field.value [i].first,
                     field.value [i].last);
}


static void
print_map (const char *name, const field_t<ucs_map_t> &field)
{
    if (!field.set) {
        std::printf ("%s <unset>\n", name);
        return;
    }

    std::printf ("%s %lu\n", name, (unsigned long)field.value.size ());

    for (std::size_t i = 0; i != field.value.size (); ++i)
        std::printf ("%04lX %04lX\n", field.value [i].from,
                     field.value [i].to);
}


static void
print_points (const ucs_string_t &ucs)
{
    for (std::size_t i = 0; i != ucs.size (); ++i)
        std::printf (i ? " %04lX" : "%04lX", ucs [i]);
}


static void
dump_ctype (const char *charmap, const char *path, const char *dir,
            diag_t &diag)
{
    cmap_t       cmap;
    locale_def_t def;

    if (!read_model (charmap, path, dir, cmap, def, diag))
        return;

    const ctype_def_t &ctype = def.ctype;

    print_class ("upper", ctype.upper);
    print_class ("lower", ctype.lower);
    print_class ("alpha", ctype.alpha);
    print_class ("digit", ctype.digit);
    print_class ("xdigit", ctype.xdigit);
    print_class ("space", ctype.space);
    print_class ("print", ctype.print);
    print_class ("graph", ctype.graph);
    print_class ("blank", ctype.blank);
    print_class ("cntrl", ctype.cntrl);
    print_class ("punct", ctype.punct);
    print_map ("toupper", ctype.toupper);
    print_map ("tolower", ctype.tolower);

    std::printf ("translit %lu\n", (unsigned long)ctype.translit.size ());

    for (translit_t::const_iterator it = ctype.translit.begin ();
         it != ctype.translit.end (); ++it) {

        std::putchar ('<');
        print_points (it->first);
        std::putchar ('>');

        for (std::size_t i = 0; i != it->second.size (); ++i) {
            std::putchar (i ? ';' : ' ');
            print_points (it->second [i]);
        }

        std::putchar ('\n');
    }
}


// The census prints one line per class, "name N", N the characters
// POSIX's automatic inclusions added to the class beyond the source's
// statements, which is the measure plan-localedef-backend.md 3.2 asks
// for of where the standard's defaults part from the old compiler's.
static void
dump_census (const char *charmap, const char *path, const char *dir,
             diag_t &diag)
{
    cmap_t       cmap;
    locale_def_t def;

    if (!read_model (charmap, path, dir, cmap, def, diag))
        return;

    const image_names_t names = { std::string (), std::string () };

    std::vector<char> image;
    ctype_census_t    census;

    write_ctype (def, cmap, names, image, census);

    for (std::size_t i = 0; i != ctype_class_count; ++i)
        std::printf ("%s %u\n", ctype_class_name (i), census.added [i]);
}


/**************************************************************************/
// the sequence of LC_COLLATE

// The dump prints the declarations, then the sequence.  First
// "codepoint_collation" when the source says it; then "symbols N" and
// one declaration per line, a range as <first>..<last>; "elements N"
// and per line the name and the string it stands for, as <Uxxxx>
// names; "scripts N" and one per line; "sections N" and per line the
// name, <> for the unnamed section, and the directions of each level
// as order_start writes them.  Then "sequence N" and one entry per
// line, as the source writes it: the file and the line that placed it,
// the element, and the weights of the levels separated by semicolons.
// A name is in angle brackets, a weight of several names in quotes, a
// range as <first>..<last> with its members' first and last included,
// a weight that is the element itself as the element, or for a range
// as its ellipsis.  A line "% section <name>" stands before an entry
// placed in another section than the one before it.  The entry count
// and the time taken go to stderr.

static const char*
base_name (const std::string &path)
{
    const std::string::size_type slash = path.rfind ('/');

    return path.c_str () + (std::string::npos == slash ? 0 : slash + 1);
}


static void
print_range (const name_range_t &range)
{
    if (0 == range.width && !range.ucs) {
        std::printf ("<%s>", range.prefix.c_str ());
        return;
    }

    std::printf ("<%s>%s<%s>", range_member (range, range.first).c_str (),
                 ellipsis_text (range.ellipsis),
                 range_member (range, range.last).c_str ());
}


static void
print_rules (const std::vector<int> &rules)
{
    for (std::size_t i = 0; i != rules.size (); ++i) {

        const char *sep = i ? ";" : "";

        if (rules [i] & dir_forward) {
            std::printf ("%sforward", sep);
            sep = ",";
        }

        if (rules [i] & dir_backward) {
            std::printf ("%sbackward", sep);
            sep = ",";
        }

        if (rules [i] & dir_position)
            std::printf ("%sposition", sep);
    }
}


static void
print_element (const coll_entry_t &entry)
{
    switch (entry.kind) {

    case ck_undefined:
        std::fputs ("UNDEFINED", stdout);
        break;

    case ck_range:
        print_range (entry.range);
        break;

    default:
        std::printf ("<%s>", entry.name.c_str ());
    }
}


static void
print_weight (const coll_entry_t &entry, const coll_weight_t &weight)
{
    switch (weight.kind) {

    case cw_ignore:
        std::fputs ("IGNORE", stdout);
        break;

    case cw_self:
        if (ck_range == entry.kind)
            std::fputs (ellipsis_text (entry.range.ellipsis), stdout);
        else
            print_element (entry);
        break;

    case cw_names:
        if (1 < weight.names.size ())
            std::putchar ('"');

        for (std::size_t i = 0; i != weight.names.size (); ++i)
            std::printf ("<%s>", weight.names [i].c_str ());

        if (1 < weight.names.size ())
            std::putchar ('"');
        break;
    }
}


static void
dump_collate (const char *charmap, const char *path, const char *dir,
              diag_t &diag)
{
    const std::clock_t start = std::clock ();

    cmap_t       cmap;
    locale_def_t def;

    if (!read_model (charmap, path, dir, cmap, def, diag))
        return;

    const std::clock_t read = std::clock ();

    const collate_def_t &coll = def.collate;

    if (coll.codepoint_collation)
        std::puts ("codepoint_collation");

    std::printf ("symbols %lu\n", (unsigned long)coll.symbols.size ());

    for (std::size_t i = 0; i != coll.symbols.size (); ++i) {
        print_range (coll.symbols [i]);
        std::putchar ('\n');
    }

    std::printf ("elements %lu\n", (unsigned long)coll.elements.size ());

    for (std::size_t i = 0; i != coll.elements.size (); ++i) {

        std::printf ("<%s> \"", coll.elements [i].name.c_str ());

        const ucs_string_t &value = coll.elements [i].value;

        for (std::size_t j = 0; j != value.size (); ++j)
            std::printf (value [j] < 0x10000L ? "<U%04lX>" : "<U%08lX>",
                         value [j]);

        std::puts ("\"");
    }

    std::printf ("scripts %lu\n", (unsigned long)coll.scripts.size ());

    for (std::size_t i = 0; i != coll.scripts.size (); ++i)
        std::printf ("<%s>\n", coll.scripts [i].c_str ());

    std::printf ("sections %lu\n", (unsigned long)coll.sections.size ());

    for (std::size_t i = 0; i != coll.sections.size (); ++i) {
        std::printf ("<%s> ", coll.sections [i].name.c_str ());
        print_rules (coll.sections [i].rules);
        std::putchar ('\n');
    }

    std::printf ("sequence %lu\n", (unsigned long)coll.sequence.size ());

    int section = -1;

    for (std::size_t i = 0; i != coll.sequence.size (); ++i) {

        const coll_entry_t &entry = coll.sequence [i];

        if (entry.section != section && 0 <= entry.section)
            std::printf ("%% section <%s>\n",
                         coll.sections [entry.section].name.c_str ());

        section = entry.section;

        std::printf ("%s:%d: ", base_name (coll.files [entry.file]),
                     entry.line);

        print_element (entry);

        for (std::size_t j = 0; j != entry.weights.size (); ++j) {
            std::putchar (j ? ';' : ' ');
            print_weight (entry, entry.weights [j]);
        }

        std::putchar ('\n');
    }

    std::fflush (stdout);

    const std::clock_t end = std::clock ();

    std::fprintf (stderr, "entries %lu, clock %.2f s: read %.2f s "
                  "(charmap included), dump %.2f s\n",
                  (unsigned long)coll.sequence.size (),
                  double (end - start) / CLOCKS_PER_SEC,
                  double (read - start) / CLOCKS_PER_SEC,
                  double (end - read) / CLOCKS_PER_SEC);
}


/**************************************************************************/
// the collation numbering

// The dump prints what the numbering gives the sequence, so that its
// counts can be read against the old compiler's image: first the
// levels and their directions, the longest weight, the largest
// element, the values given, the rows, the shared row and the
// characters left out; then one line per entry of the sequence in the
// form of the --collate dump, followed by "% value" and, for an entry
// with a row, "row R" and the words of each level, the levels
// separated by semicolons and the words of one by commas, an unused
// word as -.  A range prints its first and last value and row and the
// words of its first member; an entry with no value prints "% -".
// Before the sequence, "elements N" and per line an element's name,
// its string as <Uxxxx> names and its row, or - when the charmap does
// not spell it.  Under codepoint_collation nothing is numbered, and
// the dump is that one word.

static void
print_words (const coll_numbering_t &num, std::size_t row)
{
    const unsigned* const words = num.row_words (row);

    for (std::size_t j = 0; j != num.levels; ++j) {

        std::putchar (j ? ';' : ' ');

        for (unsigned k = 0; k != num.longest_weight; ++k) {

            const unsigned word = words [j * num.longest_weight + k];

            if (k)
                std::putchar (',');

            if (UINT_MAX == word)
                std::putchar ('-');
            else
                std::printf ("%u", word);
        }
    }
}


static void
dump_numbers (const char *charmap, const char *path, const char *dir,
              diag_t &diag)
{
    cmap_t       cmap;
    locale_def_t def;

    if (!read_model (charmap, path, dir, cmap, def, diag))
        return;

    const collate_def_t &coll = def.collate;

    coll_numbering_t num;

    number_collate (coll, cmap, num, diag);

    if (num.codepoint) {
        std::puts ("codepoint_collation");
        return;
    }

    std::printf ("levels %lu ", (unsigned long)num.levels);
    print_rules (num.rules);
    std::printf ("\nlongest weight %u, largest element %u\n",
                 num.longest_weight, num.largest_element);
    std::printf ("values %u, rows %lu, undefined row %ld, missing %lu\n",
                 num.next_value - 2, (unsigned long)num.rows.size (),
                 num.undefined_row, (unsigned long)num.missing);

    std::printf ("elements %lu\n", (unsigned long)coll.elements.size ());

    for (std::size_t i = 0; i != coll.elements.size (); ++i) {

        std::printf ("<%s> \"", coll.elements [i].name.c_str ());

        const ucs_string_t &value = coll.elements [i].value;

        for (std::size_t j = 0; j != value.size (); ++j)
            std::printf (value [j] < 0x10000L ? "<U%04lX>" : "<U%08lX>",
                         value [j]);

        if (num.element_rows [i] < 0)
            std::puts ("\" row -");
        else
            std::printf ("\" row %ld\n", num.element_rows [i]);
    }

    bool undefined = false;

    for (std::size_t i = 0; i != coll.sequence.size (); ++i) {

        const coll_entry_t  &entry  = coll.sequence [i];
        const coll_number_t &number = num.numbers [i];

        print_element (entry);

        for (std::size_t j = 0; j != entry.weights.size (); ++j) {
            std::putchar (j ? ';' : ' ');
            print_weight (entry, entry.weights [j]);
        }

        std::fputs (" % ", stdout);

        if (0 == number.count)
            std::fputs ("-", stdout);
        else if (ck_range == entry.kind)
            std::printf ("%u..%u", number.value,
                         number.value + unsigned (number.count) - 1);
        else
            std::printf ("%u", number.value);

        if (0 <= number.row) {

            if (ck_range == entry.kind)
                std::printf (" rows %ld..%ld", number.row,
                             number.row + long (number.count) - 1);
            else
                std::printf (" row %ld", number.row);

            print_words (num, std::size_t (number.row));
        }

        std::putchar ('\n');

        if (ck_undefined == entry.kind)
            undefined = true;
    }

    // the shared row the numbering added after the sequence
    if (!undefined && 0 <= num.undefined_row) {

        const std::size_t row = std::size_t (num.undefined_row);

        std::printf ("UNDEFINED %% %u row %lu", num.rows [row].value,
                     (unsigned long)row);
        print_words (num, row);
        std::putchar ('\n');
    }
}


/**************************************************************************/
// the locale database

// Creates dir and the directories above it that are not there.  False
// with errno set when one cannot be created.
static bool
make_dirs (const std::string &dir)
{
    std::string::size_type slash = dir.find ('/', 1);

    for (;;) {

        const std::string prefix = dir.substr (0, slash);

        if (0 != mkdir (prefix.c_str (), 0755) && EEXIST != errno)
            return false;

        if (std::string::npos == slash)
            return true;

        slash = dir.find ('/', slash + 1);
    }
}


// Writes one image as the file name in dir.
static bool
write_file (const std::string &dir, const char *name,
            const std::vector<char> &image, diag_t &diag)
{
    const std::string path = dir + '/' + name;

    std::FILE* const file = std::fopen (path.c_str (), "wb");

    if (!file)
        return diag.error (e_openwr, nowhere, "%s cannot be opened for "
                           "writing: %s", path.c_str (),
                           std::strerror (errno));

    const bool written =
           image.empty ()
        || 1 == std::fwrite (&image [0], image.size (), 1, file);

    if (0 != std::fclose (file) || !written)
        return diag.error (e_openwr, nowhere, "%s could not be written: %s",
                           path.c_str (), std::strerror (errno));

    return true;
}


// The directory a locale's directory is in, where the files shared
// between locales go.
static std::string
parent_of (const std::string &dir)
{
    const std::string::size_type slash = dir.rfind ('/');

    return std::string::npos == slash ? std::string (".")
                                      : dir.substr (0, slash);
}


// Writes the LC_CTYPE of a locale whose section is a copy alone: the
// image goes to the shared file NAME.ctype.CHARMAP beside the locale
// directory, the first locale to need it writing it, as in the old
// compiler, and the locale's LC_CTYPE is a symbolic link to it with a
// relative target, so that the database moves as one tree.  A file an
// earlier build left where the link goes is removed first.
static void
share_ctype (const std::string &dir, const std::string &shared,
             const std::vector<char> &image, diag_t &diag)
{
    const std::string parent = parent_of (dir);

    struct stat st;

    if (   0 != stat ((parent + '/' + shared).c_str (), &st)
        && !write_file (parent, shared.c_str (), image, diag))
        return;

    const std::string link   = dir + "/LC_CTYPE";
    const std::string target = "../" + shared;

    if (0 != unlink (link.c_str ()) && ENOENT != errno) {
        diag.error (e_openwr, nowhere, "%s cannot be removed: %s",
                    link.c_str (), std::strerror (errno));
        return;
    }

    if (0 != symlink (target.c_str (), link.c_str ()))
        diag.error (e_openwr, nowhere, "%s cannot be linked to %s: %s",
                    link.c_str (), target.c_str (), std::strerror (errno));
}


// Writes the codecvt database of the codeset beside the locale
// directory under the codeset's name, unless a file of that name is
// there already: the database is a function of the charmap alone, so
// the first locale built with it writes it and the rest skip it, as
// in the old compiler.
static void
share_codecvt (const std::string &dir, const cmap_t &cmap,
               const image_names_t &names, diag_t &diag)
{
    const std::string parent = parent_of (dir);

    struct stat st;

    if (0 == stat ((parent + '/' + names.codeset).c_str (), &st))
        return;

    std::vector<char> image;

    write_codecvt (cmap, names, image);
    write_file (parent, names.codeset.c_str (), image, diag);
}


// The options for one locale: -c writes the database
// although warnings were issued, -w mutes them, -d names the directory
// the locale's own goes under, the current one otherwise, and -I the
// directory the locales a copy names are read from.  --no_position
// writes a forward,position level as forward, in the header and in
// every row's word, as the old driver did; the collate test builds
// its locales with it.
struct write_opts_t {
    const char *charmap;
    const char *input;
    const char *dir;
    const char *outdir;
    const char *name;
    bool        force;
    bool        quiet;
    bool        no_position;
};


// [-c] [-w] [-w###] [--no_position] -f CHARMAP -i FILE [-d DIR] [-I DIR]
// NAME, in any order; -w### mutes the one warning in diag as it is read
static bool
write_args (int argc, char *argv [], write_opts_t &opts, diag_t &diag)
{
    for (int i = 1; i < argc; ++i) {

        const char* const arg = argv [i];

        if ('-' != arg [0]) {
            if (opts.name)
                return false;

            opts.name = arg;
            continue;
        }

        if (0 == std::strcmp ("-c", arg))
            opts.force = true;
        else if (0 == std::strcmp ("-w", arg))
            opts.quiet = true;
        else if ('w' == arg [1] && arg [2]) {
            char *end = 0;
            const long id = std::strtol (arg + 2, &end, 10);

            if (*end)
                return false;

            diag.mute_warning (int (id));
        }
        else if (0 == std::strcmp ("--no_position", arg))
            opts.no_position = true;
        else if (i + 1 == argc)
            return false;
        else if (0 == std::strcmp ("-f", arg))
            opts.charmap = argv [++i];
        else if (0 == std::strcmp ("-i", arg))
            opts.input = argv [++i];
        else if (0 == std::strcmp ("-d", arg))
            opts.outdir = argv [++i];
        else if (0 == std::strcmp ("-I", arg))
            opts.dir = argv [++i];
        else
            return false;
    }

    return opts.charmap && opts.input && opts.name;
}


// Reads the definition and writes its database: the six categories,
// each as its file under DIR/NAME, and the codecvt database of the
// codeset beside it.  An error withholds the output, as do warnings
// without -c, which is what the old driver did.
static void
write_locale (const write_opts_t &opts, diag_t &diag)
{
    cmap_t       cmap;
    locale_def_t def;

    if (!read_model (opts.charmap, opts.input, opts.dir, cmap, def, diag))
        return;

    if (opts.no_position)
        for (std::size_t i = 0; i != def.collate.sections.size (); ++i) {

            std::vector<int> &rules = def.collate.sections [i].rules;

            for (std::size_t j = 0; j != rules.size (); ++j)
                rules [j] &= ~dir_position;
        }

    // the collation is numbered before the checks below, since the
    // characters a sequence leaves out are a warning that counts with
    // the reader's; a definition without the category writes no file,
    // as in the old compiler
    const collate_def_t &coll = def.collate;

    const bool has_collate =
           coll.codepoint_collation || !coll.sections.empty ()
        || !coll.sequence.empty () || !coll.symbols.empty ()
        || !coll.elements.empty ();

    coll_numbering_t numbering;

    if (has_collate)
        number_collate (coll, cmap, numbering, diag);

    if (diag.errors ())
        return;

    if (diag.warnings () && !opts.force) {
        std::fputs ("Warnings occurred - No output produced\n", stderr);
        return;
    }

    // the charmap file's name stands for the code set when the charmap
    // names none, as in the old compiler
    image_names_t names;

    names.charmap = base_name (opts.charmap);
    names.codeset =
        cmap.code_set_name.empty () ? names.charmap : cmap.code_set_name;

    const std::string dir =
        opts.outdir ? std::string (opts.outdir) + '/' + opts.name
                    : std::string (opts.name);

    if (!make_dirs (dir)) {
        diag.error (e_openwr, nowhere, "%s cannot be created: %s",
                    dir.c_str (), std::strerror (errno));
        return;
    }

    std::vector<char> image;
    ctype_census_t    census;

    write_ctype (def, cmap, names, image, census);

    if (def.ctype.copied.empty ())
        write_file (dir, "LC_CTYPE", image, diag);
    else
        share_ctype (dir, def.ctype.copied + ".ctype." + names.charmap,
                     image, diag);

    share_codecvt (dir, cmap, names, diag);

    write_numeric (def, cmap, names, image);
    write_file (dir, "LC_NUMERIC", image, diag);

    write_monetary (def, cmap, names, image);
    write_file (dir, "LC_MONETARY", image, diag);

    write_time (def, cmap, names, image);
    write_file (dir, "LC_TIME", image, diag);

    write_messages (def, cmap, names, image);
    write_file (dir, "LC_MESSAGES", image, diag);

    if (has_collate) {
        write_collate (numbering, coll, cmap, names, image);
        write_file (dir, "LC_COLLATE", image, diag);
    }
}


/**************************************************************************/
// the command line

// --help and -?, whose first line the locale utilities' sanity test
// looks for
static void
print_help ()
{
    static const char msg[] =
        "NAME\n"
        "\tlocaledef - generate a locale environment\n"
        "\n"
        "SYNOPSIS\n"
        "\tlocaledef [-c][-w[###]][--no_position] -f charmap_file\n"
        "\t\t-i locale_definition [-d output_dir][-I source_dir]\n"
        "\t\tlocale_name\n"
        "\n"
        "\tlocaledef --tokens file...\n"
        "\n"
        "\tlocaledef --charmap charmap_file\n"
        "\n"
        "\tlocaledef --values | --ctype | --collate | --census\n"
        "\t\t| --collate-numbers -f charmap_file -i locale_definition\n"
        "\t\t[-I source_dir]\n"
        "\n"
        "DESCRIPTION\n"
        "\tThe localedef utility sets up the language environment for the\n"
        "\tnamed locale. localedef reads a locale definition file and\n"
        "\tcreates a locale database with the name specified on the\n"
        "\tcommand line, under output_dir when one is given. The codecvt\n"
        "\tdatabase of the character set is written beside the locale,\n"
        "\tonce per character set. The forms with a long option print\n"
        "\twhat the utility read instead of writing a database.\n"
        "\n"
        "OPTIONS\n"
        "\tThe localedef utility recognizes the following options:\n"
        "\n"
        "\t-c\tCreate output irrespective of warnings.\n"
        "\n"
        "\t-f charmap_file\n"
        "\t\tUse charmap_file for the symbolic names of the definition.\n"
        "\n"
        "\t-i locale_definition\n"
        "\t\tUse locale_definition as input.\n"
        "\n"
        "\t-d output_dir\n"
        "\t\tSpecify the directory where the generated locale database\n"
        "\t\tfiles should be placed.\n"
        "\n"
        "\t-I source_dir\n"
        "\t\tSpecify the directory where the definitions a copy\n"
        "\t\tstatement names are located.\n"
        "\n"
        "\t-w\tDisable all warning messages.\n"
        "\n"
        "\t-w###\n"
        "\t\tDisable warning number ###.\n"
        "\n"
        "\t--no_position\n"
        "\t\tWrite a forward,position collation level as forward.\n"
        "\n"
        "\t--tokens\tPrint the tokens of the files.\n"
        "\n"
        "\t--charmap\tPrint the characters of the character map.\n"
        "\n"
        "\t--values\tPrint the scalar categories of the definition.\n"
        "\n"
        "\t--ctype\tPrint the LC_CTYPE classes and maps.\n"
        "\n"
        "\t--collate\tPrint the LC_COLLATE model.\n"
        "\n"
        "\t--census\tPrint what the POSIX class defaults add to LC_CTYPE.\n"
        "\n"
        "\t--collate-numbers\n"
        "\t\tPrint the collating values and rows of the definition.\n"
        "\n"
        "\t-?\n"
        "\t--help\tPrint out this message.\n";

    std::fputs (msg, stdout);
}


// OPTION -f CHARMAP -i FILE [-I DIR], the options in any order
static bool
model_args (int argc, char *argv [], const char *option,
            const char *&charmap, const char *&input, const char *&dir)
{
    if (argc < 3 || 0 != std::strcmp (option, argv [1]) || argc % 2)
        return false;

    for (int i = 2; i < argc; i += 2) {

        if (0 == std::strcmp ("-f", argv [i]))
            charmap = argv [i + 1];
        else if (0 == std::strcmp ("-i", argv [i]))
            input = argv [i + 1];
        else if (0 == std::strcmp ("-I", argv [i]))
            dir = argv [i + 1];
        else
            return false;
    }

    return charmap && input;
}


int
main (int argc, char *argv [])
{
    diag_t diag;

    const char *charmap = 0;
    const char *input   = 0;
    const char *dir     = 0;

    write_opts_t opts = { 0, 0, 0, 0, 0, false, false, false };

    for (int i = 1; i < argc; ++i)
        if (   0 == std::strcmp ("--help", argv [i])
            || 0 == std::strcmp ("-?", argv [i])) {
            print_help ();
            return 0;
        }

    if (2 < argc && 0 == std::strcmp ("--tokens", argv [1]))
        dump_tokens (argc, argv, diag);
    else if (3 == argc && 0 == std::strcmp ("--charmap", argv [1]))
        dump_charmap (argv [2], diag);
    else if (model_args (argc, argv, "--values", charmap, input, dir))
        dump_values (charmap, input, dir, diag);
    else if (model_args (argc, argv, "--ctype", charmap, input, dir))
        dump_ctype (charmap, input, dir, diag);
    else if (model_args (argc, argv, "--collate", charmap, input, dir))
        dump_collate (charmap, input, dir, diag);
    else if (model_args (argc, argv, "--census", charmap, input, dir))
        dump_census (charmap, input, dir, diag);
    else if (model_args (argc, argv, "--collate-numbers", charmap, input,
                         dir))
        dump_numbers (charmap, input, dir, diag);
    else if (write_args (argc, argv, opts, diag)) {
        if (opts.quiet)
            diag.mute_warnings ();

        write_locale (opts, diag);
    }
    else
        diag.error (e_cmdarg, nowhere,
                    "usage: localedef [-c] [-w] [-w###] [--no_position] "
                    "-f CHARMAP -i FILE [-d DIR] [-I DIR] NAME | "
                    "localedef --tokens FILE... | "
                    "localedef --charmap FILE | "
                    "localedef --values -f CHARMAP -i FILE [-I DIR] | "
                    "localedef --ctype -f CHARMAP -i FILE [-I DIR] | "
                    "localedef --collate -f CHARMAP -i FILE [-I DIR] | "
                    "localedef --census -f CHARMAP -i FILE [-I DIR] | "
                    "localedef --collate-numbers -f CHARMAP -i FILE [-I DIR]");

    return diag.exit_status ();
}
