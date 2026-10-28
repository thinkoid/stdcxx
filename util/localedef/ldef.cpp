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

// The new locale compiler.  For now it has four forms, each the check
// of one piece of the front end:
//
//   ldef --tokens FILE...                       the tokens of each line
//   ldef --charmap FILE                         the entries of a charmap
//   ldef --values -f CHARMAP -i FILE [-I DIR]   the values of the four
//                                               scalar categories
//   ldef --ctype -f CHARMAP -i FILE [-I DIR]    the classes, case maps
//                                               and transliteration
//   ldef --collate -f CHARMAP -i FILE [-I DIR]  the declarations and
//                                               the sequence of
//                                               LC_COLLATE

#include "cmap.h"
#include "diag.h"
#include "locfile.h"
#include "model.h"
#include "reader.h"

#include <cstdio>    // for printf(), fwrite()
#include <cstring>   // for strcmp(), strerror(), strrchr()
#include <ctime>     // for clock()
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
    else
        diag.error (e_cmdarg, nowhere,
                    "usage: ldef --tokens FILE... | ldef --charmap FILE | "
                    "ldef --values -f CHARMAP -i FILE [-I DIR] | "
                    "ldef --ctype -f CHARMAP -i FILE [-I DIR] | "
                    "ldef --collate -f CHARMAP -i FILE [-I DIR]");

    return diag.exit_status ();
}
