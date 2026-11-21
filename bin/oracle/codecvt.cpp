/***************************************************************************
 *
 * codecvt.cpp - the library's codecvt<wchar_t> against the C library's
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
 * A differential oracle for localedef: the conversions of the codecvt
 * database the library's localedef built for the codeset of a locale,
 * read through std::codecvt<wchar_t, char, mbstate_t> with the facets
 * held to the library's own databases, against the C library's
 * mbrtowc and wcrtomb under setlocale for the same name. The C
 * library must have the locale generated. The wide characters of the
 * two sides compare as they are: the database pairs each character
 * with its code point as both its internal and its UCS value, and
 * glibc's wchar_t is the code point in every locale. The checks:
 *
 *   in       the byte sequences of the characters and their prefixes:
 *            each byte, then each byte appended to a sequence the
 *            library finds incomplete, or takes as a character where
 *            the C library finds it incomplete, up to the longest
 *            character that MB_CUR_MAX, max_length or the charmap
 *            allows; in of the sequence against mbrtowc repeated over
 *            it, a -2 being partial and a -1 error, compared by the
 *            result, the wide characters and the bytes consumed up to
 *            the incomplete or invalid character
 *   length   the same sequences: length with a max of 1 against the
 *            bytes of the character mbrtowc completes first, or 0,
 *            and against max_length, which no length may exceed
 *   out      every wchar_t from 0 to 0x10FFFF, the surrogates
 *            included, and four values past it: out of the character
 *            against wcrtomb, a -1 being error, compared by the
 *            result and the bytes
 *   members  max_length against MB_CUR_MAX and the charmap's
 *            <mb_cur_max>; encoding against -1 when mbtowc (0, 0, 0)
 *            finds a state-dependent encoding, 1 when MB_CUR_MAX is 1
 *            and 0 otherwise, and against the charmap's <mb_cur_max>
 *            when <mb_cur_min> equals it and 0 otherwise;
 *            always_noconv against false, since every byname facet
 *            converts
 *
 * Every sequence handed to a C function is counted: mbrtowc takes
 * the sequence and its length, wcrtomb one character. The walk of in
 * stops where the library rejects a sequence, so a character the C
 * library decodes below it is one disagreement, at the sequence, and
 * out sees its code point. The repertoire is thus the union of the
 * two sides': a character of the charmap that both reject goes
 * unseen, one that either converts is seen.
 *
 * The output lists the disagreements of each check, the first few
 * of each (-n), as the locale, the check, the item and the two
 * results, bytes in hexadecimal \xNN and wide characters <UNNNN>;
 * then one line with the number of items and of disagreements, in
 * all and per check. The exit status is 0 when nothing disagrees.
 *
 * Build, against a build directory $B of the library, with the
 * CPPFLAGS of its makefile.in (15D here, a debug build):
 *
 *   gcc -I$TOPDIR/include -I$TOPDIR/include/ansi -I$B/include \
 *       -D_RWSTDDEBUG -nostdinc++ -pthread -g codecvt.cpp \
 *       -o codecvt-oracle -L$B/lib -Wl,-R$B/lib -lstd15D -lsupc++ -lm
 *
 * Run, with the database for de_DE.UTF-8 and the codecvt database
 * UTF-8 under $ROOT (from "localedef -c -f UTF-8 -i de_DE de_DE.UTF-8"
 * run in $ROOT):
 *
 *   RWSTD_LOCALE_ROOT=$ROOT ./codecvt-oracle -c de_DE.utf8 \
 *       -f $TOPDIR/etc/nls/charmaps/UTF-8 de_DE.UTF-8
 *
 * codecvt-all.sh runs it over every locale of etc/nls/SUPPORTED.
 *
 * Options:
 *
 *   -c name     the C library's name for the locale, when not the
 *               library's (de_DE.utf8 for de_DE.UTF-8)
 *   -f charmap  the charmap the database was built from; without it
 *               the members are not compared with the charmap
 *   -n count    the disagreements listed per check (default 8)
 *
 **************************************************************************/

#include <locale>
#include <string>
#include <vector>

#include <clocale>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cwchar>

#include <loc/_localedef.h>


typedef std::codecvt<wchar_t, char, std::mbstate_t> cvt_t;


// the longest sequence the walk extends, whatever the two sides say
static const std::size_t longest_walked = 8;


// a conversion of a sequence by either side: the result, the wide
// characters and the bytes consumed
struct conv_t
{
    std::codecvt_base::result res;
    std::wstring              chars;
    std::size_t               used;

    bool operator== (const conv_t& other) const {
        return res == other.res && chars == other.chars && used == other.used;
    }
};


// the items of one check and its disagreements, of which the first
// few are listed; the caller formats a disagreement only to list it
struct check_t
{
    const char*   locale;
    const char*   name;
    unsigned long listed;
    unsigned long items;
    unsigned long count;

    check_t (const char* loc, const char* what, unsigned long n)
        : locale (loc), name (what), listed (n), items (0), count (0) { }

    // counts the item; true for a disagreement to list
    bool disagrees (bool agree) {
        ++items;

        return !agree && ++count <= listed;
    }

    void list (const std::string& item, const std::string& ours,
               const char* other, const std::string& theirs) const {
        std::printf ("  %s %s %s: library \"%s\" %s \"%s\"\n", locale,
                     name, item.c_str (), ours.c_str (), other,
                     theirs.c_str ());
    }
};


// the library's databases for the locale must exist under the root,
// its LC_CTYPE and the codecvt database of the codeset LC_CTYPE
// names: without one the facet would come from the C library and
// agree with it trivially
static bool
have_databases (const char* name)
{
    const char* const root = std::getenv ("RWSTD_LOCALE_ROOT");

    const std::string dir = std::string (root ? root : ".") + '/';
    const std::string path = dir + name + "/LC_CTYPE";

    std::FILE* f = std::fopen (path.c_str (), "rb");
    if (0 == f) {
        std::fprintf (stderr, "no database %s\n", path.c_str ());
        return false;
    }

    std::vector<char> image;

    char buf [4096];
    for (std::size_t n; 0 != (n = std::fread (buf, 1, sizeof buf, f)); )
        image.insert (image.end (), buf, buf + n);

    std::fclose (f);

    if (image.size () < sizeof (__rw::__rw_ctype_t)) {
        std::fprintf (stderr, "%s is too short\n", path.c_str ());
        return false;
    }

    const __rw::__rw_ctype_t* const pctype =
        reinterpret_cast< const __rw::__rw_ctype_t* >(&image [0]);

    const std::string codecvt_path = dir + pctype->codeset_name ();

    f = std::fopen (codecvt_path.c_str (), "rb");
    if (0 == f) {
        std::fprintf (stderr, "no database %s\n", codecvt_path.c_str ());
        return false;
    }

    std::fclose (f);

    return true;
}


// the <mb_cur_min> and <mb_cur_max> of a charmap, 1 each when it
// does not give them (XBD 6.4)
static bool
read_charmap (const char* path, int& mb_cur_min, int& mb_cur_max)
{
    std::FILE* const f = std::fopen (path, "r");
    if (0 == f) {
        std::fprintf (stderr, "no charmap %s\n", path);
        return false;
    }

    mb_cur_min = 1;
    mb_cur_max = 1;

    char line [256];
    while (std::fgets (line, sizeof line, f)) {
        if (0 == std::strncmp (line, "CHARMAP", 7))
            break;

        std::sscanf (line, "<mb_cur_min> %d", &mb_cur_min);
        std::sscanf (line, "<mb_cur_max> %d", &mb_cur_max);
    }

    std::fclose (f);

    return true;
}


static std::string
show (const char* bytes, std::size_t n)
{
    std::string out;

    for (std::size_t i = 0; i != n; ++i) {
        char buf [8];
        std::sprintf (buf, "\\x%02X", (unsigned char)bytes [i]);
        out += buf;
    }

    return out;
}


static std::string
show (wchar_t wc)
{
    char buf [16];
    std::sprintf (buf, "<U%04lX>", (unsigned long)wc & 0xFFFFFFFFUL);

    return buf;
}


static std::string
show (const conv_t& conv)
{
    static const char* const results [] = {
        "ok", "partial", "error", "noconv"
    };

    const unsigned res = unsigned (conv.res);

    std::string out = res < 4 ? results [res] : "?";

    for (std::size_t i = 0; i != conv.chars.size (); ++i)
        out += ' ' + show (conv.chars [i]);

    if (std::codecvt_base::ok != conv.res && conv.used) {
        char buf [32];
        std::sprintf (buf, " at %lu", (unsigned long)conv.used);
        out += buf;
    }

    return out;
}


static std::string
show (int n)
{
    char buf [16];
    std::sprintf (buf, "%d", n);

    return buf;
}


static conv_t
lib_in (const cvt_t& cvt, const char* bytes, std::size_t n)
{
    std::mbstate_t state = std::mbstate_t ();

    // a byte is at most one wide character
    wchar_t to [longest_walked + 1];

    const char* from_next = bytes;
    wchar_t*    to_next   = to;

    conv_t conv;
    conv.res = cvt.in (state, bytes, bytes + n, from_next,
                       to, to + n, to_next);

    conv.chars.assign (to, to_next);
    conv.used = std::size_t (from_next - bytes);

    return conv;
}


// in as the C library performs it: mbrtowc character by character,
// a null byte one character
static conv_t
libc_in (const char* bytes, std::size_t n)
{
    std::mbstate_t state = std::mbstate_t ();

    conv_t conv;
    conv.res  = std::codecvt_base::ok;
    conv.used = 0;

    while (conv.used != n) {
        wchar_t wc = L'\0';

        std::size_t len = std::mbrtowc (&wc, bytes + conv.used,
                                        n - conv.used, &state);
        if (std::size_t (-1) == len) {
            conv.res = std::codecvt_base::error;
            break;
        }

        if (std::size_t (-2) == len) {
            conv.res = std::codecvt_base::partial;
            break;
        }

        if (0 == len)
            len = 1;

        conv.chars += wc;
        conv.used  += len;
    }

    return conv;
}


// the bytes of the first character mbrtowc completes, or 0
static int
libc_length (const char* bytes, std::size_t n)
{
    std::mbstate_t state = std::mbstate_t ();

    wchar_t wc;

    const std::size_t len = std::mbrtowc (&wc, bytes, n, &state);
    if (std::size_t (-2) <= len)
        return 0;

    return 0 == len ? 1 : int (len);
}


// in and length over the byte sequences
struct walk_t
{
    const cvt_t& cvt;
    std::size_t  limit;
    int          max_length;
    check_t      in;
    check_t      length;

    walk_t (const char* name, unsigned long listed, const cvt_t& c,
            std::size_t lim)
        : cvt (c), limit (lim), max_length (c.max_length ()),
          in (name, "in", listed), length (name, "length", listed) { }

    // each byte appended to the sequence, and the walk continued from
    // each sequence that either side finds incomplete
    void extend (std::string& seq) {
        seq += '\0';

        for (int byte = 0; byte != 256; ++byte) {
            seq [seq.size () - 1] = char (byte);

            const char* const bytes = seq.data ();
            const std::size_t n     = seq.size ();

            const conv_t ours = lib_in (cvt, bytes, n);
            const conv_t libc = libc_in (bytes, n);

            if (in.disagrees (ours == libc))
                in.list (show (bytes, n), show (ours), "C library",
                         show (libc));

            std::mbstate_t state = std::mbstate_t ();

            const int len  = cvt.length (state, bytes, bytes + n, 1);
            const int clen = libc_length (bytes, n);

            if (length.disagrees (len == clen))
                length.list (show (bytes, n), show (len), "C library",
                             show (clen));

            if (length.disagrees (len <= max_length))
                length.list (show (bytes, n), show (len), "max_length",
                             show (max_length));

            // the C library's partial below the library's error is not
            // followed: glibc's UTF-8 finds "\xF8" and every prefix of
            // an overlong form incomplete, and the walk would not end
            if (   n < limit
                && (   std::codecvt_base::partial == ours.res
                    || (   std::codecvt_base::partial == libc.res
                        && std::codecvt_base::error != ours.res)))
                extend (seq);
        }

        seq.erase (seq.size () - 1);
    }
};


static std::string
show_out (bool ok, const char* bytes, std::size_t n)
{
    if (!ok)
        return "error";

    return n ? "ok " + show (bytes, n) : std::string ("ok");
}


// out of each character against wcrtomb
static void
check_out (check_t& out, const cvt_t& cvt)
{
    // past the last code point, the largest value and two negative ones
    static const unsigned long beyond [] = {
        0x110000UL, 0x7FFFFFFFUL, 0x80000000UL, 0xFFFFFFFFUL
    };

    const unsigned long nchars =
        0x110000UL + sizeof beyond / sizeof *beyond;

    for (unsigned long i = 0; i != nchars; ++i) {
        const wchar_t wc =
            wchar_t (i < 0x110000UL ? i : beyond [i - 0x110000UL]);

        std::mbstate_t state = std::mbstate_t ();

        char to [32];

        const wchar_t* from_next = &wc;
        char*          to_next   = to;

        const bool ours_ok = std::codecvt_base::ok ==
            cvt.out (state, &wc, &wc + 1, from_next, to, to + sizeof to,
                     to_next);

        const std::size_t nours = std::size_t (to_next - to);

        std::mbstate_t libc_state = std::mbstate_t ();

        char buf [32];

        const std::size_t len = std::wcrtomb (buf, wc, &libc_state);
        const bool libc_ok = std::size_t (-1) != len;

        const bool agree =    ours_ok == libc_ok
                           && (!ours_ok || (   nours == len
                                            && 0 == std::memcmp (to, buf,
                                                                 len)));

        if (out.disagrees (agree))
            out.list (show (wc), show_out (ours_ok, to, nours), "C library",
                      show_out (libc_ok, buf, len));
    }
}


// max_length, encoding and always_noconv
static void
check_members (check_t& members, const cvt_t& cvt,
               int mb_cur_min, int mb_cur_max)
{
    const int max_length = cvt.max_length ();
    const int libc_max   = int (MB_CUR_MAX);

    if (members.disagrees (max_length == libc_max))
        members.list ("max_length", show (max_length), "C library",
                      show (libc_max));

    const int encoding = cvt.encoding ();

    const int libc_encoding =
        std::mbtowc (0, 0, 0) ? -1 : 1 == libc_max ? 1 : 0;

    if (members.disagrees (encoding == libc_encoding))
        members.list ("encoding", show (encoding), "C library",
                      show (libc_encoding));

    // against the charmap, when one is given
    if (0 < mb_cur_max) {
        if (members.disagrees (max_length == mb_cur_max))
            members.list ("max_length", show (max_length), "charmap",
                          show (mb_cur_max));

        const int charmap_encoding =
            mb_cur_min == mb_cur_max ? mb_cur_max : 0;

        if (members.disagrees (encoding == charmap_encoding))
            members.list ("encoding", show (encoding), "charmap",
                          show (charmap_encoding));
    }

    const bool noconv = cvt.always_noconv ();

    if (members.disagrees (!noconv))
        members.list ("always_noconv", noconv ? "true" : "false",
                      "expected", "false");
}


int main (int argc, char* argv [])
{
    const char*   libc_name = 0;
    const char*   charmap   = 0;
    unsigned long listed    = 8;

    int argi = 1;
    for (; argi + 1 < argc && '-' == argv [argi][0]; argi += 2) {
        if (0 == std::strcmp (argv [argi], "-c"))
            libc_name = argv [argi + 1];
        else if (0 == std::strcmp (argv [argi], "-f"))
            charmap = argv [argi + 1];
        else if (0 == std::strcmp (argv [argi], "-n"))
            listed = std::strtoul (argv [argi + 1], 0, 10);
        else
            break;
    }

    if (argc != argi + 1) {
        std::fprintf (stderr, "usage: %s [-c name] [-f charmap] [-n count]"
                      " <locale-name>\n", argv [0]);
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

    int mb_cur_min = 0;
    int mb_cur_max = 0;

    if (charmap && !read_charmap (charmap, mb_cur_min, mb_cur_max))
        return 2;

    if (!have_databases (name))
        return 2;

    const std::locale loc (name);

    const cvt_t& cvt = std::use_facet<cvt_t>(loc);

    check_t members (name, "members", listed);
    check_members (members, cvt, mb_cur_min, mb_cur_max);

    // the walk goes as deep as the longest character either side or
    // the charmap allows, to see every proper prefix of it
    std::size_t limit = std::size_t (MB_CUR_MAX);

    if (limit < std::size_t (cvt.max_length ()))
        limit = std::size_t (cvt.max_length ());

    if (limit < std::size_t (mb_cur_max))
        limit = std::size_t (mb_cur_max);

    if (longest_walked < limit)
        limit = longest_walked;

    walk_t walk (name, listed, cvt, limit);

    std::string seq;
    walk.extend (seq);

    check_t out (name, "out", listed);
    check_out (out, cvt);

    const unsigned long items =
        walk.in.items + walk.length.items + out.items + members.items;

    const unsigned long disagreements =
        walk.in.count + walk.length.count + out.count + members.count;

    std::printf ("%s: %lu items, %lu disagreements (in %lu, length %lu,"
                 " out %lu, members %lu)\n", name, items, disagreements,
                 walk.in.count, walk.length.count, out.count, members.count);

    return disagreements ? 1 : 0;
}
