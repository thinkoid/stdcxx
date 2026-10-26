/***************************************************************************
 *
 * collate.cpp - the library's collate facets against the C library's
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
 ***************************************************************************
 *
 * A differential oracle for localedef: the collation of a database
 * the library's localedef built, read through std::collate with the
 * facets held to the library's own databases, against the C
 * library's wcscoll and strcoll under setlocale for the same name.
 * The C library must have the locale generated, in a UTF-8 codeset.
 * Three checks, each a count of the pairs whose order disagrees:
 *
 *   words    every pair of a word list: the lines of the files named
 *            on the command line and the month and day names of the
 *            locale (nl_langinfo); collate<wchar_t>::compare against
 *            wcscoll and collate<char>::compare against strcoll
 *   random   pairs of random strings of 1 to 4 characters drawn from
 *            the characters in the code point ranges of -a that the
 *            C library calls printable; both facets as above
 *   single   the same characters, sorted by wcscoll, each against
 *            the next: the order of the
 *            collate<wchar_t>::transform strings against wcscoll;
 *            a disagreement is "inverted" when the two orders are
 *            opposite, "libc ties" when only the library orders the
 *            pair, "ours ties" when only the C library does
 *
 * The first few disagreements of each check are listed as code
 * points; the exit status is 0 when nothing disagrees.
 *
 * Build, against a build directory $B of the library, with the
 * CPPFLAGS of its makefile.in (15D here, a debug build):
 *
 *   gcc -I$TOPDIR/include -I$TOPDIR/include/ansi -I$B/include \
 *       -D_RWSTDDEBUG -nostdinc++ -pthread -g collate.cpp \
 *       -o collate-oracle -L$B/lib -Wl,-R$B/lib -lstd15D -lsupc++ -lm
 *
 * Run, with the database for en_US.UTF-8 under $ROOT (from
 * "localedef -c -f UTF-8 -i en_US en_US.UTF-8" run in $ROOT):
 *
 *   RWSTD_LOCALE_ROOT=$ROOT ./collate-oracle \
 *       -a 0041-005A,0061-007A,00C0-024F,0370-03FF,0400-04FF \
 *       en_US.UTF-8 words.txt
 *
 * Options:
 *
 *   -a ranges   the code points of the random and single checks, a
 *               comma separated list of hexadecimal ranges first-last
 *               (default 0020-007E,00A0-024F)
 *   -n count    the number of random pairs (default 50000)
 *   -c name     the C library's name for the locale, when not the
 *               library's (glibc's SUPPORTED names some locales
 *               without the codeset the C library adds, fa_IR for
 *               fa_IR.UTF-8)
 *   -s seed     the seed of the random strings (default 1)
 *
 **************************************************************************/

#include <algorithm>
#include <fstream>
#include <locale>
#include <string>
#include <vector>

#include <clocale>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cwchar>
#include <cwctype>

#include <langinfo.h>   // for nl_langinfo()


// the first disagreements a check lists
static const int nlisted = 6;


// the library's database for the locale must exist under the root:
// without one the facets would come from the C library and agree
// with it trivially
static bool
have_database (const char* name, const char* category)
{
    const char* const root = std::getenv ("RWSTD_LOCALE_ROOT");

    const std::string path =
        std::string (root ? root : ".") + '/' + name + '/' + category;

    std::FILE* const f = std::fopen (path.c_str (), "rb");
    if (0 == f) {
        std::fprintf (stderr, "no database %s\n", path.c_str ());
        return false;
    }

    std::fclose (f);

    return true;
}


static int
sign (int n)
{
    return n < 0 ? -1 : 0 < n;
}


// UTF-8 to wide by the C library, in the locale set
static std::wstring
widen (const std::string& s)
{
    std::vector<wchar_t> buf (s.size () + 1);

    const std::size_t n = std::mbstowcs (&buf [0], s.c_str (), buf.size ());

    if (std::size_t (-1) == n)
        return std::wstring ();

    return std::wstring (&buf [0], n);
}


// wide to UTF-8 by the C library, in the locale set
static std::string
narrow (const std::wstring& s)
{
    std::vector<char> buf (s.size () * MB_CUR_MAX + 1);

    const std::size_t n = std::wcstombs (&buf [0], s.c_str (), buf.size ());

    if (std::size_t (-1) == n)
        return std::string ();

    return std::string (&buf [0], n);
}


static void
print_string (const std::wstring& s)
{
    for (std::size_t i = 0; i < s.size (); ++i)
        std::printf ("%s%04lX", i ? "." : "", (unsigned long)s [i]);
}


struct tally_t
{
    const char*   name;
    unsigned long pairs;
    unsigned long count;

    tally_t (const char* what): name (what), pairs (0), count (0) { }

    void check (int ours, int libc,
                const std::wstring& a, const std::wstring& b) {
        ++pairs;

        if (sign (ours) == sign (libc))
            return;

        if (count++ < unsigned (nlisted)) {
            std::printf ("  %s: ", name);
            print_string (a);
            std::printf (" ");
            print_string (b);
            std::printf (" ours %d libc %d\n", sign (ours), sign (libc));
        }
    }

    void report () const {
        std::printf ("%-14s %8lu of %8lu\n", name, count, pairs);
    }
};


struct facets_t
{
    const std::collate<char>&    nco;
    const std::collate<wchar_t>& wco;

    facets_t (const std::collate<char>& n, const std::collate<wchar_t>& w)
        : nco (n), wco (w) { }

    void compare (tally_t& wide, tally_t& nrrw,
                  const std::wstring& a, const std::wstring& b) const {
        wide.check (wco.compare (a.data (), a.data () + a.size (),
                                 b.data (), b.data () + b.size ()),
                    std::wcscoll (a.c_str (), b.c_str ()), a, b);

        const std::string na = narrow (a);
        const std::string nb = narrow (b);

        nrrw.check (nco.compare (na.data (), na.data () + na.size (),
                                 nb.data (), nb.data () + nb.size ()),
                    std::strcoll (na.c_str (), nb.c_str ()), a, b);
    }
};


struct by_wcscoll
{
    bool operator() (const std::wstring& a, const std::wstring& b) const {
        const int cmp = std::wcscoll (a.c_str (), b.c_str ());

        // ties broken by code point, for a stable listing
        return cmp < 0 || (0 == cmp && a < b);
    }
};


static bool
parse_ranges (const char* spec, std::vector<wchar_t>& chars)
{
    for (const char* s = spec; *s; ) {
        char* end = 0;

        const unsigned long first = std::strtoul (s, &end, 16);
        if ('-' != *end)
            return false;

        const unsigned long last = std::strtoul (end + 1, &end, 16);
        if (last < first || (',' != *end && '\0' != *end))
            return false;

        for (unsigned long wc = first; wc <= last; ++wc)
            if (wc < 0xD800UL || 0xDFFFUL < wc)
                chars.push_back (wchar_t (wc));

        s = ',' == *end ? end + 1 : end;
    }

    return !chars.empty ();
}


int main (int argc, char* argv [])
{
    const char*   ranges = "0020-007E,00A0-024F";
    unsigned long npairs = 50000;
    unsigned      seed   = 1;
    const char*   libc_name = 0;

    int argi = 1;
    for (; argi + 1 < argc && '-' == argv [argi][0]; argi += 2) {
        if (0 == std::strcmp (argv [argi], "-a"))
            ranges = argv [argi + 1];
        else if (0 == std::strcmp (argv [argi], "-n"))
            npairs = std::strtoul (argv [argi + 1], 0, 10);
        else if (0 == std::strcmp (argv [argi], "-c"))
            libc_name = argv [argi + 1];
        else if (0 == std::strcmp (argv [argi], "-s"))
            seed = unsigned (std::strtoul (argv [argi + 1], 0, 10));
        else
            break;
    }

    if (argc <= argi) {
        std::fprintf (stderr, "usage: %s [-a ranges] [-c name] [-n count]"
                      " [-s seed] <locale-name> [word-file...]\n",
                      argv [0]);
        return 2;
    }

    const char* const name = argv [argi++];

    if (0 == libc_name)
        libc_name = name;

    if (0 == std::setlocale (LC_ALL, libc_name)) {
        std::fprintf (stderr, "setlocale (LC_ALL, \"%s\") failed\n",
                      libc_name);
        return 2;
    }

    if (!have_database (name, "LC_COLLATE"))
        return 2;

    std::vector<wchar_t> ranged;
    if (!parse_ranges (ranges, ranged)) {
        std::fprintf (stderr, "invalid ranges: %s\n", ranges);
        return 2;
    }

    // the printable characters: an unassigned code point has no
    // place in either table
    std::vector<wchar_t> alphabet;
    for (std::size_t i = 0; i < ranged.size (); ++i)
        if (std::iswprint (ranged [i]))
            alphabet.push_back (ranged [i]);

    if (alphabet.empty ()) {
        std::fprintf (stderr, "no printable characters in %s\n", ranges);
        return 2;
    }

    const std::locale loc (name);

    const facets_t co (std::use_facet<std::collate<char> >(loc),
                       std::use_facet<std::collate<wchar_t> >(loc));

    // the library's databases only, never the C library behind them
    co.wco._C_opts |=  co.wco._C_use_libstd;
    co.wco._C_opts &= ~co.wco._C_use_libc;

    // the word list: the files, then the names of the locale
    std::vector<std::wstring> words;

    for (; argi < argc; ++argi) {
        std::ifstream in (argv [argi]);
        for (std::string line; std::getline (in, line); )
            if (!line.empty ())
                words.push_back (widen (line));
    }

    static const nl_item names [] = {
        DAY_1, DAY_2, DAY_3, DAY_4, DAY_5, DAY_6, DAY_7,
        ABDAY_1, ABDAY_2, ABDAY_3, ABDAY_4, ABDAY_5, ABDAY_6, ABDAY_7,
        MON_1, MON_2, MON_3, MON_4, MON_5, MON_6,
        MON_7, MON_8, MON_9, MON_10, MON_11, MON_12,
        ABMON_1, ABMON_2, ABMON_3, ABMON_4, ABMON_5, ABMON_6,
        ABMON_7, ABMON_8, ABMON_9, ABMON_10, ABMON_11, ABMON_12
    };

    for (std::size_t i = 0; i < sizeof names / sizeof *names; ++i)
        words.push_back (widen (nl_langinfo (names [i])));

    tally_t words_w ("words wide");
    tally_t words_n ("words narrow");

    for (std::size_t i = 0; i < words.size (); ++i)
        for (std::size_t j = i + 1; j < words.size (); ++j)
            co.compare (words_w, words_n, words [i], words [j]);

    // random strings of the alphabet
    tally_t random_w ("random wide");
    tally_t random_n ("random narrow");

    std::srand (seed);

    for (unsigned long n = 0; n < npairs; ++n) {
        std::wstring s [2];

        for (int k = 0; k < 2; ++k)
            for (int len = 1 + std::rand () % 4; len; --len)
                s [k] += alphabet [std::rand () % alphabet.size ()];

        co.compare (random_w, random_n, s [0], s [1]);
    }

    // the characters, each against the next in wcscoll order
    std::vector<std::wstring> singles;

    for (std::size_t i = 0; i < alphabet.size (); ++i)
        singles.push_back (std::wstring (1, alphabet [i]));

    std::sort (singles.begin (), singles.end (), by_wcscoll ());

    unsigned long inverted   = 0;
    unsigned long libc_ties  = 0;
    unsigned long ours_ties  = 0;
    unsigned long npairs_1   = 0;
    int           nprinted   = 0;

    std::wstring prev_xfrm;

    for (std::size_t i = 0; i < singles.size (); ++i) {
        const std::wstring& s = singles [i];

        const std::wstring xfrm =
            co.wco.transform (s.data (), s.data () + s.size ());

        if (i) {
            const int libc = sign (std::wcscoll (singles [i - 1].c_str (),
                                                 s.c_str ()));
            const int ours = sign (prev_xfrm.compare (xfrm));

            ++npairs_1;

            if (ours != libc) {
                if (0 == libc)
                    ++libc_ties;
                else if (0 == ours)
                    ++ours_ties;
                else
                    ++inverted;

                if (nprinted++ < nlisted) {
                    std::printf ("  single: ");
                    print_string (singles [i - 1]);
                    std::printf (" ");
                    print_string (s);
                    std::printf (" ours %d libc %d\n", ours, libc);
                }
            }
        }

        prev_xfrm = xfrm;
    }

    words_w.report ();
    words_n.report ();
    random_w.report ();
    random_n.report ();

    std::printf ("%-14s %8lu of %8lu (inverted %lu, libc ties %lu,"
                 " ours ties %lu)\n", "single", inverted + libc_ties
                 + ours_ties, npairs_1, inverted, libc_ties, ours_ties);

    return    words_w.count || words_n.count || random_w.count
           || random_n.count || inverted || libc_ties || ours_ties;
}
