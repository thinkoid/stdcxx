/***************************************************************************
 *
 * ctype.cpp - the library's ctype<wchar_t> against the C library's
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
 * A differential oracle for localedef: the classification and the
 * case mappings of a database the library's localedef built, read
 * through std::ctype<wchar_t> with the facets held to the library's
 * own databases, against the C library's iswctype, towupper and
 * towlower under setlocale for the same name. Every code point from
 * 0 to 0x10FFFF but the surrogates is checked; the C library must
 * have the locale generated. The output is one line per class and
 * mapping with the number of code points that disagree, the first
 * few listed, and a total; the exit status is 0 when nothing does.
 * The C library's name for the locale is the library's unless -c
 * gives another (glibc's SUPPORTED names some locales without the
 * codeset the C library adds, fa_IR for fa_IR.UTF-8).
 *
 * Build, against a build directory $B of the library, with the
 * CPPFLAGS of its makefile.in (15D here, a debug build):
 *
 *   gcc -I$TOPDIR/include -I$TOPDIR/include/ansi -I$B/include \
 *       -D_RWSTDDEBUG -nostdinc++ -pthread -g ctype.cpp -o ctype-oracle \
 *       -L$B/lib -Wl,-R$B/lib -lstd15D -lsupc++ -lm
 *
 * Run, with the database for en_US.UTF-8 under $ROOT (from
 * "localedef -c -f UTF-8 -i en_US en_US.UTF-8" run in $ROOT):
 *
 *   RWSTD_LOCALE_ROOT=$ROOT ./ctype-oracle en_US.UTF-8
 *   RWSTD_LOCALE_ROOT=$ROOT ./ctype-oracle -c fa_IR.UTF-8 fa_IR
 *
 **************************************************************************/

#include <locale>
#include <string>

#include <clocale>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cwctype>


struct class_t
{
    const char*           name;
    std::ctype_base::mask mask;
};


static const class_t classes [] = {
    { "upper",  std::ctype_base::upper  },
    { "lower",  std::ctype_base::lower  },
    { "alpha",  std::ctype_base::alpha  },
    { "digit",  std::ctype_base::digit  },
    { "xdigit", std::ctype_base::xdigit },
    { "space",  std::ctype_base::space  },
    { "print",  std::ctype_base::print  },
    { "graph",  std::ctype_base::graph  },
    { "cntrl",  std::ctype_base::cntrl  },
    { "punct",  std::ctype_base::punct  },
    { "alnum",  std::ctype_base::alnum  }
};

static const int nclasses = int (sizeof classes / sizeof *classes);

// the first disagreements a line lists
static const int nlisted = 8;


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


static void
report (const char* what, unsigned long count, const unsigned long* first)
{
    std::printf ("%-8s %8lu", what, count);

    for (unsigned long i = 0; i < count && i < unsigned (nlisted); ++i)
        std::printf (" %04lX", first [i]);

    std::printf ("%s\n", count > unsigned (nlisted) ? " ..." : "");
}


int main (int argc, char* argv [])
{
    const char* libc_name = 0;

    int argi = 1;
    if (argi + 1 < argc && 0 == std::strcmp (argv [argi], "-c")) {
        libc_name = argv [argi + 1];
        argi += 2;
    }

    if (argc <= argi) {
        std::fprintf (stderr, "usage: %s [-c name] <locale-name>\n",
                      argv [0]);
        return 2;
    }

    const char* const name = argv [argi];

    if (0 == libc_name)
        libc_name = name;

    if (0 == std::setlocale (LC_ALL, libc_name)) {
        std::fprintf (stderr, "setlocale (LC_ALL, \"%s\") failed\n",
                      libc_name);
        return 2;
    }

    if (!have_database (name, "LC_CTYPE"))
        return 2;

    const std::locale loc (name);

    const std::ctype<wchar_t>& ct =
        std::use_facet<std::ctype<wchar_t> >(loc);

    std::wctype_t libc_class [nclasses];
    for (int i = 0; i < nclasses; ++i)
        libc_class [i] = std::wctype (classes [i].name);

    unsigned long count [nclasses + 2] = { 0 };
    unsigned long first [nclasses + 2][nlisted];

    for (unsigned long wc = 0; wc <= 0x10FFFFUL; ++wc) {

        if (0xD800UL <= wc && wc <= 0xDFFFUL)
            continue;

        const wchar_t ch = wchar_t (wc);

        for (int i = 0; i < nclasses; ++i) {
            const bool ours = ct.is (classes [i].mask, ch);
            const bool libc = 0 != std::iswctype (ch, libc_class [i]);

            if (ours != libc && count [i]++ < unsigned (nlisted))
                first [i][count [i] - 1] = wc;
        }

        if (   ct.toupper (ch) != wchar_t (std::towupper (ch))
            && count [nclasses]++ < unsigned (nlisted))
            first [nclasses][count [nclasses] - 1] = wc;

        if (   ct.tolower (ch) != wchar_t (std::towlower (ch))
            && count [nclasses + 1]++ < unsigned (nlisted))
            first [nclasses + 1][count [nclasses + 1] - 1] = wc;
    }

    unsigned long total = 0;

    for (int i = 0; i < nclasses; ++i) {
        report (classes [i].name, count [i], first [i]);
        total += count [i];
    }

    report ("toupper", count [nclasses], first [nclasses]);
    report ("tolower", count [nclasses + 1], first [nclasses + 1]);

    total += count [nclasses] + count [nclasses + 1];

    std::printf ("%-8s %8lu\n", "total", total);

    return 0 != total;
}
