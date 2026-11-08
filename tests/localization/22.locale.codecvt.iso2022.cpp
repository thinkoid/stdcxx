/***************************************************************************
 *
 * 22.locale.codecvt.iso2022.cpp
 *
 * test exercising lib.locale.codecvt.virtuals for the ISO-2022-JP and
 * ISO-2022-JP-2 converters of codecvt_byname<wchar_t, char, mbstate_t>
 *
 * $Id$
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

#include <locale>    // for codecvt, codecvt_byname, locale, use_facet
#include <cstddef>   // for size_t
#include <cstring>   // for memcmp(), memcpy(), memset(), strcat(), strcpy()
#include <cwchar>    // for mbstate_t

#include <iconv.h>   // for iconv(), iconv_open(), iconv_close()

#include <rw_driver.h>   // for rw_test(), rw_assert(), rw_warn()
#include <rw_file.h>     // for rw_fwrite()
#include <rw_locale.h>   // for rw_localedef(), rw_set_locale_root()

/**************************************************************************/

typedef std::codecvt<wchar_t, char, std::mbstate_t>        Cvt;
typedef std::codecvt_byname<wchar_t, char, std::mbstate_t> CvtByname;
typedef std::codecvt_base                                  Base;

// the size of the conversion buffers
enum { BUFSIZE = 512 };

// the value of the bytes past the destination window of out
const char guard = '\xa5';

// a length larger than the number of characters of any fixture
const std::size_t length_max = 1000;


// a wide fixture, converted in both directions
struct WideFixture {
    int            line;
    const wchar_t* str;
    bool           jp;     // ISO-2022-JP carries it, not only -JP-2
};

static const WideFixture wide_fixtures [] = {
    { __LINE__, L"abc",                                      true  },
    // JIS X 0208
    { __LINE__, L"\x3042\x3044",                             true  },
    { __LINE__, L"a\x3042" L"b\x6f22\x5b57" L"c",            true  },
    { __LINE__, L"\x3042\n\x3044",                           true  },
    // Greek in JIS X 0208
    { __LINE__, L"\x3b1\x3b2",                               true  },
    // ISO-8859-1 through the single shift
    { __LINE__, L"\xb5",                                     false },
    { __LINE__, L"a\xb5" L"b\xbb",                           false },
    // JIS X 0212
    { __LINE__, L"\xe9",                                     false },
    // ISO-8859-7 through the single shift
    { __LINE__, L"\x37a\x20af",                              false },
    // GB 2312
    { __LINE__, L"\x4e1c",                                   false },
    // KSC 5601
    { __LINE__, L"\xac00\xac01",                             false },
    { __LINE__, L"a\xb5" L"b\x37a" L"c\x4e1c\xac00\x3b1\x3042\xe9", false }
};


// an external fixture, converted by in
struct ByteFixture {
    int            line;
    const char*    bytes;
    std::size_t    nbytes;
    const wchar_t* str;
    bool           jp;     // ISO-2022-JP carries it, not only -JP-2
};

#define BYTES(s)   s, sizeof s - 1

static const ByteFixture byte_fixtures [] = {
    { __LINE__, BYTES ("a\x1b$B$\"\x1b(B"), L"a\x3042", true },
    // the 1978 designation
    { __LINE__, BYTES ("a\x1b$@$\"\x1b(B"), L"a\x3042", true },
    // ISO-8859-1, GB 2312, KSC 5601
    { __LINE__, BYTES ("a\x1b.A\x1bNi\x1b$A0!\x1b$(C0!\x1b(B"),
      L"a\xe9\x554a\xac00", false },
    // ISO-8859-7
    { __LINE__, BYTES ("\x1b.F\x1bNa"), L"\x3b1", false }
};

/**************************************************************************/

static std::size_t
wlen (const wchar_t* s)
{
    std::size_t n = 0;
    while (s [n])
        ++n;

    return n;
}


static bool
same (const wchar_t* a, std::size_t na, const wchar_t* b, std::size_t nb)
{
    return na == nb && 0 == std::memcmp (a, b, na * sizeof *a);
}


// converts with iconv from the encoding `from' to `to'; returns the
// number of bytes stored in dst, or -1 when iconv fails
static long
convert_iconv (const char* to, const char* from,
               const void* src, std::size_t srclen,
               void* dst, std::size_t dstlen)
{
    const iconv_t cd = iconv_open (to, from);
    if (iconv_t (-1) == cd)
        return -1;

    char*       in      = static_cast<char*>(const_cast<void*>(src));
    std::size_t inleft  = srclen;
    char*       out     = static_cast<char*>(dst);
    std::size_t outleft = dstlen;

    std::size_t res = iconv (cd, &in, &inleft, &out, &outleft);

    // return to the initial shift state
    if (std::size_t (-1) != res)
        res = iconv (cd, 0, 0, &out, &outleft);

    iconv_close (cd);

    if (std::size_t (-1) == res || inleft)
        return -1;

    return long (out - static_cast<char*>(dst));
}


// whether iconv reads `bytes' in `enc' back to the wide string `str'
static bool
iconv_reads (const char* enc, const char* bytes, std::size_t nbytes,
             const wchar_t* str, std::size_t nstr)
{
    wchar_t wbuf [BUFSIZE];

    const long n = convert_iconv ("WCHAR_T", enc, bytes, nbytes,
                                  wbuf, sizeof wbuf);

    return 0 <= n && same (wbuf, std::size_t (n) / sizeof *wbuf, str, nstr);
}


// releases the state of an input sequence: unshift deallocates the
// state the converters keep, as the program must before it disposes
// of the state object
static void
release (const Cvt& cvt, std::mbstate_t& state)
{
    char  buf [8];
    char* next = 0;

    cvt.unshift (state, buf, buf + sizeof buf, next);
}


// converts all of [from, from_end) with in, in one call
static Base::result
in_all (const Cvt& cvt, const char* from, const char* from_end,
        wchar_t* wbuf, std::size_t* nw, std::size_t* nb)
{
    std::mbstate_t state = std::mbstate_t ();

    const char* from_next = 0;
    wchar_t*    to_next   = 0;

    const Base::result res =
        cvt.in (state, from, from_end, from_next,
                wbuf, wbuf + BUFSIZE, to_next);

    *nw = std::size_t (to_next - wbuf);
    *nb = std::size_t (from_next - from);

    release (cvt, state);

    return res;
}

/**************************************************************************/

// in, in one call and across calls, and length, over the bytes of
// `str' in the encoding `enc'
static void
test_in (const Cvt& cvt, const char* enc, int line,
         const char* bytes, std::size_t nbytes,
         const wchar_t* str)
{
    const std::size_t nstr = wlen (str);
    const char* const end  = bytes + nbytes;

    wchar_t wbuf [BUFSIZE];

    std::size_t nw = 0;
    std::size_t nb = 0;

    // one call
    Base::result res = in_all (cvt, bytes, end, wbuf, &nw, &nb);

    rw_assert (Base::ok == res && nbytes == nb && same (wbuf, nw, str, nstr),
               0, line,
               "%s: in (%{#*s}) == ok, consuming %zu, giving %{#*ls}; "
               "got %d, consuming %zu, giving %{#*ls}",
               enc, int (nbytes), bytes, nbytes, int (nstr), str,
               res, nb, int (nw), wbuf);

    // two calls with one carried state, split at every byte
    for (std::size_t k = 0; k <= nbytes; ++k) {
        std::mbstate_t state = std::mbstate_t ();

        const char* next1 = 0;
        wchar_t*    to1   = 0;

        const Base::result res1 =
            cvt.in (state, bytes, bytes + k, next1,
                    wbuf, wbuf + BUFSIZE, to1);

        const char* next2 = 0;
        wchar_t*    to2   = 0;

        const Base::result res2 =
            cvt.in (state, next1, end, next2, to1, wbuf + BUFSIZE, to2);

        release (cvt, state);

        const Base::result res1_expect =
            next1 == bytes + k ? Base::ok : Base::partial;

        rw_assert (   res1_expect == res1 && Base::ok == res2 && end == next2
                   && same (wbuf, std::size_t (to2 - wbuf), str, nstr),
                   0, line,
                   "%s: in (%{#*s}) split at %zu: %d then ok, giving "
                   "%{#*ls}; got %d then %d, consuming %td, giving %{#*ls}",
                   enc, int (nbytes), bytes, k, res1_expect,
                   int (nstr), str, res1, res2, next2 - bytes,
                   int (to2 - wbuf), wbuf);

        // length, the same split, consumes what in consumed
        std::mbstate_t lstate = std::mbstate_t ();

        const int len1 = cvt.length (lstate, bytes, bytes + k, length_max);
        const int len2 = cvt.length (lstate, bytes + len1, end, length_max);

        release (cvt, lstate);

        rw_assert (len1 == next1 - bytes && len1 + len2 == int (nbytes),
                   0, line,
                   "%s: length (%{#*s}) split at %zu == %td then %td; "
                   "got %d then %d",
                   enc, int (nbytes), bytes, k, next1 - bytes,
                   end - next1, len1, len2);
    }

    // one character of destination at a time
    {
        std::mbstate_t state = std::mbstate_t ();

        const char* from = bytes;
        wchar_t*    to   = wbuf;

        Base::result res1 = Base::partial;

        for (std::size_t i = 0; Base::partial == res1 && i <= nbytes; ++i) {
            const char* from_next = 0;
            wchar_t*    to_next   = 0;

            res1 = cvt.in (state, from, end, from_next, to, to + 1, to_next);

            from = from_next;
            to   = to_next;
        }

        release (cvt, state);

        rw_assert (   Base::ok == res1 && end == from
                   && same (wbuf, std::size_t (to - wbuf), str, nstr),
                   0, line,
                   "%s: in (%{#*s}) into one character at a time == ok, "
                   "giving %{#*ls}; got %d, consuming %td, giving %{#*ls}",
                   enc, int (nbytes), bytes, int (nstr), str,
                   res1, from - bytes, int (to - wbuf), wbuf);
    }

    // length (..., 1) over every character is at most max_length ()
    {
        std::mbstate_t state = std::mbstate_t ();

        const int max_length = cvt.max_length ();

        int maxlen = 0;

        for (const char* from = bytes; from != end; ) {
            const int len = cvt.length (state, from, end, 1);

            if (maxlen < len)
                maxlen = len;

            if (len <= 0)
                break;

            from += len;
        }

        release (cvt, state);

        rw_assert (0 < maxlen && maxlen <= max_length, 0, line,
                   "%s: length (%{#*s}, 1) <= max_length () == %d; got %d",
                   enc, int (nbytes), bytes, max_length, maxlen);
    }
}


// out in one call and across calls, against in and iconv
static void
test_out (const Cvt& cvt, const char* enc, int line, const wchar_t* str,
          bool use_iconv)
{
    const std::size_t nstr = wlen (str);

    char bytes [BUFSIZE];
    std::size_t nbytes = 0;

    // one call and unshift
    {
        std::mbstate_t state = std::mbstate_t ();

        const wchar_t* from_next = 0;
        char*          to_next   = 0;

        const Base::result res =
            cvt.out (state, str, str + nstr, from_next,
                     bytes, bytes + BUFSIZE, to_next);

        char* to_end = 0;

        const Base::result ures =
            cvt.unshift (state, to_next, bytes + BUFSIZE, to_end);

        nbytes = std::size_t (to_end - bytes);

        rw_assert (   Base::ok == res && str + nstr == from_next
                   && Base::ok == ures,
                   0, line,
                   "%s: out (%{#*ls}) == ok, unshift == ok; got %d, "
                   "consuming %td, then %d",
                   enc, int (nstr), str, res, from_next - str, ures);
    }

    // the facet reads its own output back
    test_in (cvt, enc, line, bytes, nbytes, str);

    if (use_iconv) {
        rw_assert (iconv_reads (enc, bytes, nbytes, str, nstr), 0, line,
                   "%s: iconv reads out (%{#*ls}) == %{#*s} back",
                   enc, int (nstr), str, int (nbytes), bytes);

        // the facet reads the output of iconv
        char ibytes [BUFSIZE];

        const long n = convert_iconv (enc, "WCHAR_T",
                                      str, nstr * sizeof *str,
                                      ibytes, sizeof ibytes);

        rw_assert (0 <= n, 0, line, "%s: iconv converts %{#*ls}",
                   enc, int (nstr), str);

        if (0 <= n)
            test_in (cvt, enc, line, ibytes, std::size_t (n), str);
    }

    // two calls with one carried state, split at every character
    for (std::size_t k = 0; k <= nstr; ++k) {
        std::mbstate_t state = std::mbstate_t ();

        char sbytes [BUFSIZE];

        const wchar_t* next1 = 0;
        char*          to1   = 0;

        const Base::result res1 =
            cvt.out (state, str, str + k, next1,
                     sbytes, sbytes + BUFSIZE, to1);

        const wchar_t* next2 = 0;
        char*          to2   = 0;

        const Base::result res2 =
            cvt.out (state, str + k, str + nstr, next2,
                     to1, sbytes + BUFSIZE, to2);

        char* to3 = 0;

        const Base::result res3 =
            cvt.unshift (state, to2, sbytes + BUFSIZE, to3);

        const std::size_t ns = std::size_t (to3 - sbytes);

        wchar_t     wbuf [BUFSIZE];
        std::size_t nw = 0;
        std::size_t nb = 0;

        const Base::result ires = in_all (cvt, sbytes, to3, wbuf, &nw, &nb);

        rw_assert (   Base::ok == res1 && str + k == next1
                   && Base::ok == res2 && str + nstr == next2
                   && Base::ok == res3
                   && Base::ok == ires && same (wbuf, nw, str, nstr)
                   && (!use_iconv || iconv_reads (enc, sbytes, ns, str, nstr)),
                   0, line,
                   "%s: out (%{#*ls}) split at %zu == ok, ok, unshift ok, "
                   "read back; got %d, %d, %d, giving %{#*s}",
                   enc, int (nstr), str, k, res1, res2, res3,
                   int (ns), sbytes);
    }

    // a destination window of every size that holds the longest character
    const std::size_t max_length = std::size_t (cvt.max_length ());

    for (std::size_t size = max_length; size <= nbytes + 1; ++size) {
        std::mbstate_t state = std::mbstate_t ();

        char wbytes [BUFSIZE];

        const wchar_t* from = str;
        char*          to   = wbytes;

        Base::result res = Base::partial;

        bool overrun = false;
        bool stuck   = false;

        while (Base::partial == res && !overrun && !stuck) {
            char* const to_limit = to + size;

            std::memset (to, guard, std::size_t (wbytes + BUFSIZE - to));

            const wchar_t* from_next = 0;
            char*          to_next   = 0;

            res = cvt.out (state, from, str + nstr, from_next,
                           to, to_limit, to_next);

            for (const char* p = to_limit; p != wbytes + BUFSIZE; ++p)
                if (guard != *p)
                    overrun = true;

            stuck = from_next == from;

            from = from_next;
            to   = to_next;
        }

        char* to_end = 0;

        const Base::result ures =
            cvt.unshift (state, to, wbytes + BUFSIZE, to_end);

        const std::size_t nw = std::size_t (to_end - wbytes);

        wchar_t     wbuf [BUFSIZE];
        std::size_t nwide = 0;
        std::size_t nb    = 0;

        const Base::result ires =
            in_all (cvt, wbytes, to_end, wbuf, &nwide, &nb);

        rw_assert (   Base::ok == res && !overrun && !stuck
                   && Base::ok == ures
                   && Base::ok == ires && same (wbuf, nwide, str, nstr)
                   && (!use_iconv || iconv_reads (enc, wbytes, nw, str, nstr)),
                   0, line,
                   "%s: out (%{#*ls}) into windows of %zu == ok, nothing "
                   "past the window, read back; got %d%{?}, overrun%{;}"
                   "%{?}, stuck%{;}, giving %{#*s}",
                   enc, int (nstr), str, size, res, overrun, stuck,
                   int (nw), wbytes);
    }
}


// out of a character the encoding lacks
static void
test_out_error (const Cvt& cvt, const char* enc, wchar_t lacking,
                bool use_iconv)
{
    // the character alone: error, nothing converted
    {
        std::mbstate_t state = std::mbstate_t ();

        const wchar_t src [] = { lacking };
        char          bytes [BUFSIZE];

        std::memset (bytes, guard, sizeof bytes);

        const wchar_t* from_next = 0;
        char*          to_next   = 0;

        const Base::result res =
            cvt.out (state, src, src + 1, from_next,
                     bytes, bytes + BUFSIZE, to_next);

        rw_assert (   Base::error == res && src == from_next
                   && bytes == to_next && guard == bytes [0],
                   0, __LINE__,
                   "%s: out (%{#*ls}) == error, converting nothing; "
                   "got %d, consuming %td, writing %td",
                   enc, 1, src, res, from_next - src, to_next - bytes);
    }

    // after a character of JIS X 0208: error at the character, and the
    // state still that of the characters before it
    {
        std::mbstate_t state = std::mbstate_t ();

        const wchar_t src [] = { L'a', 0x3042, lacking };
        const wchar_t rest [] = { L'b' };
        const wchar_t expect [] = { L'a', 0x3042, L'b' };

        char bytes [BUFSIZE];

        const wchar_t* from_next = 0;
        char*          to_next   = 0;

        const Base::result res =
            cvt.out (state, src, src + 3, from_next,
                     bytes, bytes + BUFSIZE, to_next);

        const wchar_t* from_next2 = 0;
        char*          to_next2   = 0;

        const Base::result res2 =
            cvt.out (state, rest, rest + 1, from_next2,
                     to_next, bytes + BUFSIZE, to_next2);

        char* to_end = 0;

        const Base::result ures =
            cvt.unshift (state, to_next2, bytes + BUFSIZE, to_end);

        const std::size_t nbytes = std::size_t (to_end - bytes);

        wchar_t     wbuf [BUFSIZE];
        std::size_t nw = 0;
        std::size_t nb = 0;

        const Base::result ires =
            in_all (cvt, bytes, to_end, wbuf, &nw, &nb);

        rw_assert (   Base::error == res && src + 2 == from_next
                   && Base::ok == res2 && Base::ok == ures
                   && Base::ok == ires && same (wbuf, nw, expect, 3)
                   && (!use_iconv
                       || iconv_reads (enc, bytes, nbytes, expect, 3)),
                   0, __LINE__,
                   "%s: out (%{#*ls}) == error at 2, then out (L\"b\") "
                   "and unshift read back as %{#*ls}; got %d at %td, "
                   "%d, %d, giving %{#*s}",
                   enc, 3, src, 3, expect, res, from_next - src,
                   res2, ures, int (nbytes), bytes);
    }
}


// unshift after a two-byte set and of an input state, and length at
// the end of the source
static void
test_unshift_length (const Cvt& cvt, const char* enc, bool jp2)
{
    {
        std::mbstate_t state = std::mbstate_t ();

        const wchar_t src [] = { 0x3042 };
        char          bytes [BUFSIZE];

        const wchar_t* from_next = 0;
        char*          to_next   = 0;

        const Base::result res =
            cvt.out (state, src, src + 1, from_next,
                     bytes, bytes + BUFSIZE, to_next);

        char* to_end = 0;

        const Base::result ures =
            cvt.unshift (state, to_next, bytes + BUFSIZE, to_end);

        char* to_end2 = 0;

        const Base::result ures2 =
            cvt.unshift (state, to_end, bytes + BUFSIZE, to_end2);

        static const char out [] = "\x1b$B$\"";
        static const char esc [] = "\x1b(B";

        rw_assert (   Base::ok == res && sizeof out - 1 == to_next - bytes
                   && 0 == std::memcmp (bytes, out, sizeof out - 1)
                   && Base::ok == ures && sizeof esc - 1 == to_end - to_next
                   && 0 == std::memcmp (to_next, esc, sizeof esc - 1)
                   && Base::ok == ures2 && to_end == to_end2,
                   0, __LINE__,
                   "%s: out (L\"\\x3042\") == %{#*s}, unshift == %{#*s}, "
                   "then nothing; got %{#*s}, %d, %{#*s}, %d, %td",
                   enc, int (sizeof out - 1), out, int (sizeof esc - 1), esc,
                   int (to_next - bytes), bytes, ures,
                   int (to_end - to_next), to_next, ures2, to_end2 - to_end);
    }

    // a heap buffer holding exactly an escape sequence, so that a read
    // past its end is one past the allocation
    {
        char* const esc = new char [3];
        std::memcpy (esc, "\x1b(B", 3);

        std::mbstate_t state = std::mbstate_t ();

        const int len = cvt.length (state, esc, esc + 3, length_max);

        release (cvt, state);

        rw_assert (3 == len, 0, __LINE__,
                   "%s: length (\"\\x1b(B\") == 3, got %d", enc, len);

        delete[] esc;
    }

    // unshift deallocates the state an input sequence leaves, in JIS X
    // 0208 or, under -JP-2, with ISO-8859-1 in G2, so more conversions
    // than the converters have states all succeed
    {
        static const char jp_src [] = "\x1b$B$\"";
        static const char jp2_src [] = "\x1b.A\x1bN5";

        const char* const src  = jp2 ? jp2_src : jp_src;
        const std::size_t nsrc = jp2 ? sizeof jp2_src - 1 : sizeof jp_src - 1;

        std::size_t i = 0;

        for (; i != 2048; ++i) {
            std::mbstate_t state = std::mbstate_t ();

            wchar_t     wbuf [BUFSIZE];
            const char* from_next = 0;
            wchar_t*    to_next   = 0;

            const Base::result res =
                cvt.in (state, src, src + nsrc, from_next,
                        wbuf, wbuf + BUFSIZE, to_next);

            release (cvt, state);

            if (Base::ok != res)
                break;
        }

        rw_assert (2048 == i, 0, __LINE__,
                   "%s: in (%{#*s}) and unshift 2048 times == ok; "
                   "failed at %zu",
                   enc, int (nsrc), src, i);
    }
}


static void
test_encoding (const char* enc, bool jp2, bool use_iconv)
{
    const std::locale loc (std::locale::classic (), new CvtByname (enc));
    const Cvt& cvt = std::use_facet<Cvt>(loc);

    rw_info (0, 0, 0, "codecvt_byname<wchar_t, char, mbstate_t> (%#s)", enc);

    // 22.2.1.5.2, p7: a state-dependent encoding
    rw_assert (-1 == cvt.encoding (), 0, __LINE__,
               "%s: encoding () == -1, got %d", enc, cvt.encoding ());

    rw_assert (!cvt.always_noconv (), 0, __LINE__,
               "%s: always_noconv () == false", enc);

    // a designation and the longest character after it: ESC $ B and
    // two bytes, and under -JP-2 ESC $ ( C and two bytes, or ESC . A,
    // ESC N and one byte
    const int max_length = jp2 ? 6 : 5;

    rw_assert (max_length == cvt.max_length (), 0, __LINE__,
               "%s: max_length () == %d, got %d",
               enc, max_length, cvt.max_length ());

    for (std::size_t i = 0;
         i != sizeof byte_fixtures / sizeof *byte_fixtures; ++i) {
        const ByteFixture& fix = byte_fixtures [i];

        if (jp2 || fix.jp)
            test_in (cvt, enc, fix.line, fix.bytes, fix.nbytes, fix.str);
    }

    for (std::size_t i = 0;
         i != sizeof wide_fixtures / sizeof *wide_fixtures; ++i) {
        const WideFixture& fix = wide_fixtures [i];

        if (jp2 || fix.jp)
            test_out (cvt, enc, fix.line, fix.str, use_iconv);
    }

    // ISO-2022-JP carries no Latin-1 set, and no set carries Thai
    test_out_error (cvt, enc, jp2 ? 0x0e01 : 0xe9, use_iconv);

    test_unshift_length (cvt, enc, jp2);
}

/**************************************************************************/

static int
run_test (int, char*[])
{
    // the codecvt databases of the encodings ISO-2022-JP and -JP-2
    // draw on, ISO-2022-JP the first two, -JP-2 all six; localedef
    // writes one for each character map it processes, and an empty
    // locale definition makes the quickest locale to process
    static const char* const charmaps [] = {
        "ANSI_X3.4-1968", "EUC-JP",
        "ISO-8859-1", "ISO-8859-7", "GB2312", "EUC-KR"
    };

    const std::size_t ncharmaps = sizeof charmaps / sizeof *charmaps;

    const char* const locale_root = rw_set_locale_root ();

    if (!rw_warn (0 != locale_root, 0, __LINE__,
                  "rw_set_locale_root () failed, test skipped"))
        return 0;

    char src [1024];
    std::strcpy (src, locale_root);
    std::strcat (src, "/empty.src");

    if (!rw_warn (std::size_t (-1) != rw_fwrite (src, ""), 0, __LINE__,
                  "rw_fwrite (%#s) failed, test skipped", src))
        return 0;

    std::size_t built = 0;

    for (; built != ncharmaps; ++built) {
        char locname [64];
        std::strcpy (locname, "empty.");
        std::strcat (locname, charmaps [built]);

        if (!rw_warn (0 != rw_localedef ("-w", src, charmaps [built], locname),
                      0, __LINE__,
                      "rw_localedef (\"-w\", %#s, %#s, %#s) failed",
                      src, charmaps [built], locname))
            break;
    }

    // iconv knows both encodings in the C library this test runs on
    const bool use_iconv =
        rw_warn (   iconv_reads ("ISO-2022-JP", "a", 1, L"a", 1)
                 && iconv_reads ("ISO-2022-JP-2", "a", 1, L"a", 1),
                 0, __LINE__,
                 "iconv lacks ISO-2022-JP or ISO-2022-JP-2, its checks "
                 "skipped");

    // ISO-2022-JP needs the first two databases, -JP-2 all six
    if (rw_warn (2 <= built, 0, __LINE__,
                 "the ANSI_X3.4-1968 and EUC-JP databases are missing, "
                 "ISO-2022-JP skipped"))
        test_encoding ("ISO-2022-JP", false, use_iconv);

    if (rw_warn (ncharmaps == built, 0, __LINE__,
                 "a codecvt database is missing, ISO-2022-JP-2 skipped"))
        test_encoding ("ISO-2022-JP-2", true, use_iconv);

    return 0;
}

/**************************************************************************/

int main (int argc, char *argv[])
{
    return rw_test (argc, argv, __FILE__,
                    "lib.locale.codecvt.virtuals",
                    "the ISO-2022-JP and ISO-2022-JP-2 converters",
                    run_test, "", 0);
}
