/************************************************************************
 *
 * 0.ctype.cpp - test exercising the UserCtype helper class template
 *
 * $Id$
 *
 ************************************************************************
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
 * Copyright 2006 Rogue Wave Software.
 *
 **************************************************************************/

#include <rw_char.h>     // for UserChar
#include <rw_ctype.h>    // for UserCtype
#include <rw_printf.h>   // for rw_printf
#include <rw_driver.h>

#include <stdlib.h>      // for free()
#include <string.h>      // for memset(), size_t, strlen()

/***********************************************************************/

extern const char
cntrl[] = {

#if 'A' == 0x41   // ASCII
    "\x00\x01\x02\x03\x04\x05\x06\x07\x08\x09\x0a\x0b\x0c\x0d\x0e\x0f"
    "\x10\x11\x12\x13\x14\x15\x16\x17\x18\x19\x1a\x1b\x1c\x1d\x1e\x1f\x7f"
#elif 'A' == 0xc1   // EBCDIC
    ""
#endif

};

extern const std::size_t
n_cntrl = sizeof cntrl - 1;

extern const char
digits[] = "0123456789";

extern const std::size_t
n_digits = sizeof digits - 1;

extern const char
graph[] = "!\"#$%&\\()*+,-./0123456789:;<=>?@[]^_`{|}~";

extern const std::size_t
n_graph = sizeof graph - 1;

extern const char
lower[] = "abcdefghijklmnopqrstuvwxyz";

extern const std::size_t
n_lower = sizeof lower - 1;

extern const char
punct[] = "!#$%&\'()*+,-./:;<=>?@[\\]^_`{|}~";

extern const std::size_t
n_punct = sizeof punct - 1;

extern const char
print[] = {
    "!\"#$%&\\()*+,-./0123456789:;<=>?@[]^_`{|}~"
    "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
    "abcdefghijklmnopqrstuvwxyz"
};

extern const std::size_t
n_print = sizeof print - 1;

extern const char
spaces[] = " \t\v\f\r\n";

extern const std::size_t
n_spaces = sizeof spaces - 1;

extern const char
upper[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZ";

extern const std::size_t
n_upper = sizeof upper - 1;

extern const char
xdigits[] = "0123456789ABCDEFabcdef";

extern const std::size_t
n_xdigits = sizeof xdigits - 1;

/***********************************************************************/

template <class charT>
void
test_id (charT*, const char *cname)
{
    rw_info (0, 0, __LINE__, "UserCtype<%s>::id", cname);

    std::locale::id* const pid_b = &std::ctype<charT>::id;
    std::locale::id* const pid_d = &UserCtype<charT>::id;

    rw_assert (pid_b == pid_d, 0, __LINE__,
               "&ctype<%s>::id == &UserCtype<%1$s>::id "
               "(%#p == %2$#p, got %#p)",
               cname, pid_b, pid_d);
}

/***********************************************************************/

// a table of custom characters and their classes, for the facet built
// from it; the characters past UCHAR_MAX are for the wide types
static const int custom_chars[] = {
    '0', '1', '2', '3', 'a', 'b', 'c',
    0x100, 0x7fff, 0x8000, 0xffff,
#if 2 < _RWSTD_INT_SIZE
    0x10000, 0x7fffffff, int (0x80000000), int (0xffffffff),
#else
    0x1ff, 0x700, 0x8fff, 0xf000,
#endif
    -1   // end of chars
};

static const int custom_masks[] = {
    /* '0'        */ std::ctype_base::alpha,
    /* '1'        */ std::ctype_base::cntrl,
    /* '2'        */ std::ctype_base::digit,
    /* '3'        */ std::ctype_base::lower,
    /* 'a'        */ std::ctype_base::print,
    /* 'b'        */ std::ctype_base::punct,
    /* 'c'        */ std::ctype_base::space,
    /* 0x100      */ std::ctype_base::upper,
    /* 0x7fff     */ std::ctype_base::xdigit,
    /* 0x8000     */ std::ctype_base::alpha | std::ctype_base::cntrl,
    /* 0xffff     */ std::ctype_base::cntrl | std::ctype_base::digit,
    /* 0x10000    */ std::ctype_base::digit | std::ctype_base::lower,
    /* 0x7fffffff */ std::ctype_base::lower | std::ctype_base::print,
    /* 0x80000000 */ std::ctype_base::print | std::ctype_base::punct,
    /* 0xffffffff */ std::ctype_base::punct | std::ctype_base::space,
    -1            // end of masks
};

static const int mask_all = 
      std::ctype_base::alpha
    | std::ctype_base::cntrl
    | std::ctype_base::digit
    | std::ctype_base::lower
    | std::ctype_base::print
    | std::ctype_base::punct
    | std::ctype_base::space
    | std::ctype_base::upper
    | std::ctype_base::xdigit;

// the two characters compare equal
static bool same (char a, char b) { return a == b; }
static bool same (wchar_t a, wchar_t b) { return a == b; }
static bool same (UserChar a, UserChar b) { return a.c == b.c; }

// the character's value
static int value (char c) { return (unsigned char)c; }
static int value (wchar_t c) { return int (c); }
static int value (UserChar c) { return c.c; }

/***********************************************************************/

// the largest character value each type represents; UserChar
// carries its character in an unsigned char
static int char_max (char*)     { return _RWSTD_UCHAR_MAX; }
static int char_max (UserChar*) { return _RWSTD_UCHAR_MAX; }

static int char_max (wchar_t*)
{
#if _RWSTD_WCHAR_MAX < _RWSTD_INT_MAX
    return int (_RWSTD_WCHAR_MAX);
#else
    return _RWSTD_INT_MAX;
#endif
}

/***********************************************************************/

template <class charT>
void
test_is (charT*, const char *cname,
         std::ctype_base::mask m,
         const char *chars, std::size_t n_chars)
{
    UserCtype<charT> ctp;

    std::size_t n_calls  = ctp.n_calls_ [ctp.mf_is];
    std::size_t n_throws = ctp.n_throws_ [ctp.mf_is];

    std::size_t expect_calls  = 0;
    std::size_t expect_throws = 0;

    for (std::size_t i = 0; i != n_chars; ++i) {
        const charT ch = make_char (chars [i], (charT*)0);

        int result = -1;

#ifndef _RWSTD_NO_EXCEPTIONS

        // let the first call call succeed and trigger an exception
        // on the second call to UserCtype::is()
        ctp.throw_at_calls_ [ctp.mf_is] =
            ctp.n_calls_ [ctp.mf_is] + 2;

        for (int j = 0; j != 2; ++j) {

            if (1 < sizeof (charT)) {
                // can only count UserCtype<charT>::is() calls when
                // charT != char since there is no virtual do_is()
                // in the char explicit specialization
                ++expect_calls;
            }
            else if (j) {
                // cannot induce an exception out of UserCtype<char>::is()
                // since there is no virtual do_is() in the specialization
                break;
            }

            if (j)
                ++expect_throws;

            int threw = 0;

            try {
                result = ctp.is (m, ch);
            }
            catch (...) {
                threw = 1;
            }

            rw_assert (j == threw, 0, __LINE__,
                       "UserCtype<%s>::is(%{LC}, %{#c}) "
                       "%{?}unexpectedly threw%{:}failed to throw%{;}",
                       cname, m, ch, j == threw);
        }

#else   // if defined (_RWSTD_NO_EXCEPTIONS)

        ++expect_calls;
        result = ctp.is (m, ch);

#endif   // _RWSTD_NO_EXCEPTIONS

        rw_assert (result, 0, __LINE__,
                   "UserCtype<%s>::is(%{LC}, %{#c}) == true",
                   cname, m, chars [i]);
    }

    n_calls = ctp.n_calls_ [ctp.mf_is] - n_calls;

    rw_assert (expect_calls == n_calls, 0, __LINE__,
               "expected %zu calls to UserCtype<%s>::is(%{LC}, char_type), "
               "got %zu", expect_calls, cname, m, n_calls);

    n_throws = ctp.n_throws_ [ctp.mf_is] - n_throws;

    rw_assert (expect_throws == n_throws, 0, __LINE__,
               "expected %zu exceptions thrown by UserCtype<%s>::is(%{LC}, "
               "char_type), got %zu", expect_throws, cname, m, n_throws);
}

template <class charT>
void
test_is (charT*, const char *cname)
{
    rw_info (0, 0, __LINE__, "UserCtype<%s>::is"
             "(mask, char_type) const", cname);

    //////////////////////////////////////////////////////////////////
    // exercise default behavior

    test_is ((charT*)0, cname, std::ctype_base::alnum,  lower,   n_lower);
    test_is ((charT*)0, cname, std::ctype_base::alnum,  upper,   n_upper);
    test_is ((charT*)0, cname, std::ctype_base::alnum,  digits,  n_digits);
    test_is ((charT*)0, cname, std::ctype_base::alnum,  xdigits, n_xdigits);
    test_is ((charT*)0, cname, std::ctype_base::alpha,  lower,   n_lower);
    test_is ((charT*)0, cname, std::ctype_base::alpha,  upper,   n_upper);
    test_is ((charT*)0, cname, std::ctype_base::cntrl,  cntrl,   n_cntrl);
    test_is ((charT*)0, cname, std::ctype_base::digit,  digits,  n_digits);
    test_is ((charT*)0, cname, std::ctype_base::graph,  graph,   n_graph);
    test_is ((charT*)0, cname, std::ctype_base::lower,  lower,   n_lower);
    test_is ((charT*)0, cname, std::ctype_base::print,  print,   n_print);
    test_is ((charT*)0, cname, std::ctype_base::punct,  punct,   n_punct);
    test_is ((charT*)0, cname, std::ctype_base::space,  spaces,  n_spaces);
    test_is ((charT*)0, cname, std::ctype_base::xdigit, xdigits, n_xdigits);

    //////////////////////////////////////////////////////////////////
    // exercise custom behavior


    const UserCtype<charT> cust (custom_chars, custom_masks);

    for (std::size_t i = 0; 0 <= custom_chars [i]; ++i) {

        // the rest of the table is beyond the character type
        if (char_max ((charT*)0) < custom_chars [i])
            break;

        const charT ch = make_char (custom_chars [i], (charT*)0);

        const std::ctype_base::mask m_is =
            std::ctype_base::mask (custom_masks [i]);

        rw_assert (cust.is (m_is, ch),
                   0, __LINE__,
                   "UserCtype<%s>::is(%{LC}, %{#lc}) == true",
                   cname, m_is, custom_chars [i]);

        const std::ctype_base::mask m_not =
            std::ctype_base::mask (~m_is & mask_all);

        rw_assert (!cust.is (m_not, ch), 0, __LINE__,
                   "UserCtype<%s>::is(%{LC}, %{#lc}) == false",
                   cname, m_not, custom_chars [i]);
    }
}

/***********************************************************************/

// the default table on the scan members: the first character of the
// buffer the facet classifies as the mask (scan_is), or not (scan_not),
// is() having been exercised above; the custom table the same over
// the characters the type represents
template <class charT>
void
test_scan (charT*, const char *cname, bool is)
{
    const char* const fname = is ? "scan_is" : "scan_not";

    rw_info (0, 0, __LINE__, "UserCtype<%s>::%s"
             "(mask, const char_type*, const char_type*) const", cname, fname);

    static const std::ctype_base::mask classes[] = {
        std::ctype_base::alpha, std::ctype_base::cntrl, std::ctype_base::digit,
        std::ctype_base::lower, std::ctype_base::print, std::ctype_base::punct,
        std::ctype_base::space, std::ctype_base::upper, std::ctype_base::xdigit,
        std::ctype_base::alnum, std::ctype_base::graph
    };

    const std::size_t nclasses = sizeof classes / sizeof *classes;

    {
        // a buffer over every narrow character
        UserCtype<charT> ctp;
        charT buf [256];
        for (int c = 0; c != 256; ++c)
            buf [c] = make_char (c, (charT*)0);

        std::size_t calls = 0;

        for (std::size_t m = 0; m != nclasses; ++m) {
            const charT* const res = is
                ? ctp.scan_is (classes [m], buf, buf + 256)
                : ctp.scan_not (classes [m], buf, buf + 256);
            ++calls;

            const charT *expect = buf;
            for ( ; expect != buf + 256; ++expect)
                if (ctp.is (classes [m], *expect) == is)
                    break;

            rw_assert (res == expect, 0, __LINE__,
                       "UserCtype<%s>::%s(%{LC}, buf, buf + 256) == buf + %d, "
                       "got buf + %d", cname, fname, classes [m],
                       int (expect - buf), int (res - buf));
        }

        if (1 < sizeof (charT)) {
            const std::size_t n =
                ctp.n_calls_ [is ? ctp.mf_scan_is : ctp.mf_scan_not];
            rw_assert (calls == n, 0, __LINE__,
                       "expected %zu calls to UserCtype<%s>::%s, got %zu",
                       calls, cname, fname, n);
        }
    }

    {
        // the custom characters the type represents, in table order
        const UserCtype<charT> cust (custom_chars, custom_masks);
        charT buf [32];
        std::size_t n = 0;
        for (std::size_t i = 0; 0 <= custom_chars [i]; ++i)
            if (custom_chars [i] <= char_max ((charT*)0))
                buf [n++] = make_char (custom_chars [i], (charT*)0);

        for (std::size_t m = 0; m != nclasses; ++m) {
            const charT* const res = is
                ? cust.scan_is (classes [m], buf, buf + n)
                : cust.scan_not (classes [m], buf, buf + n);

            const charT *expect = buf;
            for ( ; expect != buf + n; ++expect)
                if (cust.is (classes [m], *expect) == is)
                    break;

            rw_assert (res == expect, 0, __LINE__,
                       "UserCtype<%s>::%s(%{LC}, custom, custom + %zu) == "
                       "custom + %d, got custom + %d", cname, fname,
                       classes [m], n, int (expect - buf), int (res - buf));
        }
    }
}

template <class charT>
void
test_scan_is (charT*, const char *cname)
{
    test_scan ((charT*)0, cname, true);
}

template <class charT>
void
test_scan_not (charT*, const char *cname)
{
    test_scan ((charT*)0, cname, false);
}

/***********************************************************************/

// the default table on the case members maps the 26 letters and
// nothing else; a custom upper_/lower_ pair maps its entries both
// ways and nothing else, in the single and the range form
template <class charT>
void
test_case (charT*, const char *cname, bool to_upper)
{
    const char* const fname = to_upper ? "toupper" : "tolower";

    rw_info (0, 0, __LINE__, "UserCtype<%s>::%s(char_type) const",
             cname, fname);
    rw_info (0, 0, __LINE__, "UserCtype<%s>::%s"
             "(char_type*, const char_type*) const", cname, fname);

    {
        UserCtype<charT> ctp;
        const std::size_t calls = ctp.n_calls_ [to_upper ? ctp.mf_toupper
                                                          : ctp.mf_tolower];
        charT buf [256];

        for (int c = 0; c != 256; ++c) {
            buf [c] = make_char (c, (charT*)0);

            const char* const from = to_upper ? lower : upper;
            const char* const to   = to_upper ? upper : lower;
            const char* const pos  = c ? strchr (from, c) : 0;
            const int expect = pos ? to [pos - from] : c;

            const charT ch = make_char (c, (charT*)0);
            const charT res = to_upper ? ctp.toupper (ch) : ctp.tolower (ch);

            rw_assert (same (res, make_char (expect, (charT*)0)), 0, __LINE__,
                       "UserCtype<%s>::%s(%{#lc}) == %{#lc}, got %{#lc}",
                       cname, fname, c, expect, value (res));
        }

        rw_assert (256 + calls == ctp.n_calls_ [to_upper ? ctp.mf_toupper
                                                          : ctp.mf_tolower],
                   0, __LINE__, "expected 256 calls to UserCtype<%s>::%s",
                   cname, fname);

        const charT* const end = to_upper ? ctp.toupper (buf, buf + 256)
                                          : ctp.tolower (buf, buf + 256);
        rw_assert (end == buf + 256, 0, __LINE__,
                   "UserCtype<%s>::%s(buf, buf + 256) returned buf + %d",
                   cname, fname, int (end - buf));

        bool correct = true;
        for (int c = 0; c != 256; ++c) {
            const charT ch = make_char (c, (charT*)0);
            const charT one = to_upper ? ctp.toupper (ch) : ctp.tolower (ch);
            if (!same (buf [c], one))
                correct = false;
        }
        rw_assert (correct, 0, __LINE__,
                   "UserCtype<%s>::%s(buf, buf + 256) differs from the "
                   "single form", cname, fname);
    }

    {
        // the pair: lower 'a' <-> upper '1', 'b' <-> '2', and one wide
        // entry for the types that hold it
        static const int lower_tab[] = { 'a', 'b', 0x100, -1 };
        static const int upper_tab[] = { '1', '2', 0x7fff, -1 };
        const std::size_t ntab = 0x7fff <= char_max ((charT*)0) ? 3 : 2;

        UserCtype<charT> cust;
        cust.lower_ = lower_tab;
        cust.upper_ = upper_tab;

        const int* const from = to_upper ? lower_tab : upper_tab;
        const int* const to   = to_upper ? upper_tab : lower_tab;

        for (std::size_t i = 0; i != ntab; ++i) {
            const charT ch  = make_char (from [i], (charT*)0);
            const charT res = to_upper ? cust.toupper (ch) : cust.tolower (ch);
            rw_assert (same (res, make_char (to [i], (charT*)0)), 0, __LINE__,
                       "UserCtype<%s>::%s(%{#lc}) == %{#lc} from the table, "
                       "got %{#lc}", cname, fname, from [i], to [i], value (res));

            // the counterpart maps back, not forward
            const charT back = make_char (to [i], (charT*)0);
            const charT same_back = to_upper ? cust.toupper (back)
                                             : cust.tolower (back);
            rw_assert (same (same_back, back), 0, __LINE__,
                       "UserCtype<%s>::%s(%{#lc}) == %{#lc} outside the table, "
                       "got %{#lc}", cname, fname, to [i], to [i],
                       value (same_back));
        }

        // a letter the pair does not name stays as it is
        const charT z = make_char ('z', (charT*)0);
        rw_assert (same (to_upper ? cust.toupper (z) : cust.tolower (z), z),
                   0, __LINE__, "UserCtype<%s>::%s('z') == 'z' under a pair "
                   "without it", cname, fname);

        charT buf [4];
        for (std::size_t i = 0; i != ntab; ++i)
            buf [i] = make_char (from [i], (charT*)0);
        buf [ntab] = z;

        const charT* const end = to_upper ? cust.toupper (buf, buf + ntab + 1)
                                          : cust.tolower (buf, buf + ntab + 1);
        bool correct = end == buf + ntab + 1 && same (buf [ntab], z);
        for (std::size_t i = 0; i != ntab; ++i)
            if (!same (buf [i], make_char (to [i], (charT*)0)))
                correct = false;
        rw_assert (correct, 0, __LINE__,
                   "UserCtype<%s>::%s(buf, buf + %zu) under the pair",
                   cname, fname, ntab + 1);
    }
}

template <class charT>
void
test_toupper (charT*, const char *cname)
{
    test_case ((charT*)0, cname, true);
}

template <class charT>
void
test_tolower (charT*, const char *cname)
{
    test_case ((charT*)0, cname, false);
}

/***********************************************************************/

// the default table widens and narrows every narrow character to
// itself; a custom narrow_/wide_ pair maps its entries, leaves an
// entry without a counterpart invalid, and takes the facet's
// exception for it when the facet is set to throw
template <class charT>
void
test_widen (charT*, const char *cname)
{
    rw_info (0, 0, __LINE__, "UserCtype<%s>::widen(char) const", cname);
    rw_info (0, 0, __LINE__, "UserCtype<%s>::widen"
             "(const char*, const char*, char_type*) const", cname);

    {
        UserCtype<charT> ctp;
        char  nbuf [256];
        charT wbuf [256];

        bool correct = true;
        for (int c = 0; c != 256; ++c) {
            nbuf [c] = char (c);
            if (!same (ctp.widen (char (c)), make_char (c, (charT*)0)))
                correct = false;
        }
        rw_assert (correct, 0, __LINE__,
                   "UserCtype<%s>::widen(char) is the identity", cname);
        rw_assert (256 == ctp.n_calls_ [ctp.mf_widen], 0, __LINE__,
                   "expected 256 calls to UserCtype<%s>::widen(char), got %zu",
                   cname, ctp.n_calls_ [ctp.mf_widen]);

        const char* const end = ctp.widen (nbuf, nbuf + 256, wbuf);
        correct = end == nbuf + 256;
        for (int c = 0; c != 256; ++c)
            if (!same (wbuf [c], make_char (c, (charT*)0)))
                correct = false;
        rw_assert (correct, 0, __LINE__,
                   "UserCtype<%s>::widen(const char*, const char*, char_type*) "
                   "is the identity", cname);
    }

    {
        // 'x' widens to 'y'; 'q' has no wide counterpart
        static const int narrow_tab[] = { 'x', 'q', -1 };
        static const int wide_tab[]   = { 'y', -1, -1 };

        UserCtype<charT> cust;
        cust.narrow_ = narrow_tab;
        cust.wide_   = wide_tab;

        rw_assert (same (cust.widen ('x'), make_char ('y', (charT*)0)),
                   0, __LINE__, "UserCtype<%s>::widen('x') == 'y' from the "
                   "table", cname);
        rw_assert (same (cust.widen ('z'), make_char ('z', (charT*)0)),
                   0, __LINE__, "UserCtype<%s>::widen('z') == 'z' outside the "
                   "table", cname);
        rw_assert (same (cust.widen ('q'), make_char ('q', (charT*)0)),
                   0, __LINE__, "UserCtype<%s>::widen('q') == 'q' for an "
                   "invalid entry when not throwing", cname);

        const char src[] = "xzq";
        charT dst [3];
        cust.widen (src, src + 3, dst);
        rw_assert (   same (dst [0], make_char ('y', (charT*)0))
                   && same (dst [1], make_char ('z', (charT*)0))
                   && same (dst [2], make_char ('q', (charT*)0)), 0, __LINE__,
                   "UserCtype<%s>::widen(\"xzq\", ..., dst) == \"yzq\"", cname);

#ifndef _RWSTD_NO_EXCEPTIONS

        // a fresh facet: the library's ctype<char> and ctype<wchar_t>
        // answer widen (char) from a table filled by the first call
        // and would not reach do_widen again
        UserCtype<charT> strict;
        strict.narrow_ = narrow_tab;
        strict.wide_   = wide_tab;
        strict.throw_on_invalid_ = true;

        bool caught = false;
        try {
            strict.widen ('q');
        }
        catch (const UserCtypeBase::Exception&) {
            caught = true;
        }
        rw_assert (caught && 1 == strict.n_throws_ [strict.mf_widen],
                   0, __LINE__, "UserCtype<%s>::widen('q') throws the facet's "
                   "exception for an invalid entry", cname);

        caught = false;
        try {
            strict.widen (src, src + 3, dst);
        }
        catch (const UserCtypeBase::Exception&) {
            caught = true;
        }
        rw_assert (caught, 0, __LINE__, "UserCtype<%s>::widen(\"xzq\", ...) "
                   "throws the facet's exception for an invalid entry", cname);

#endif   // _RWSTD_NO_EXCEPTIONS

    }
}

template <class charT>
void
test_narrow (charT*, const char *cname)
{
    rw_info (0, 0, __LINE__, "UserCtype<%s>::narrow(char_type, char) const",
             cname);
    rw_info (0, 0, __LINE__, "UserCtype<%s>::narrow"
             "(const char_type*, const char_type*, char, char*) const", cname);

    {
        UserCtype<charT> ctp;
        charT wbuf [256];
        char  nbuf [256];

        bool correct = true;
        for (int c = 0; c != 256; ++c) {
            wbuf [c] = make_char (c, (charT*)0);
            if (ctp.narrow (wbuf [c], '\0') != char (c))
                correct = false;
        }
        rw_assert (correct, 0, __LINE__,
                   "UserCtype<%s>::narrow(char_type, char) is the identity",
                   cname);
        rw_assert (256 == ctp.n_calls_ [ctp.mf_narrow], 0, __LINE__,
                   "expected 256 calls to UserCtype<%s>::narrow(char_type, "
                   "char), got %zu", cname, ctp.n_calls_ [ctp.mf_narrow]);

        const charT* const end = ctp.narrow (wbuf, wbuf + 256, '\0', nbuf);
        correct = end == wbuf + 256;
        for (int c = 0; c != 256; ++c)
            if (nbuf [c] != char (c))
                correct = false;
        rw_assert (correct, 0, __LINE__,
                   "UserCtype<%s>::narrow(const char_type*, const char_type*, "
                   "char, char*) is the identity", cname);
    }

    {
        // 'y' narrows to 'x'; 'w' has no narrow counterpart
        static const int narrow_tab[] = { 'x', -1, -1 };
        static const int wide_tab[]   = { 'y', 'w', -1 };

        UserCtype<charT> cust;
        cust.narrow_ = narrow_tab;
        cust.wide_   = wide_tab;

        const charT y = make_char ('y', (charT*)0);
        const charT z = make_char ('z', (charT*)0);
        const charT w = make_char ('w', (charT*)0);

        rw_assert ('x' == cust.narrow (y, '?'), 0, __LINE__,
                   "UserCtype<%s>::narrow('y', '?') == 'x' from the table",
                   cname);
        rw_assert ('?' == cust.narrow (z, '?'), 0, __LINE__,
                   "UserCtype<%s>::narrow('z', '?') == '?' outside the table",
                   cname);
        rw_assert ('?' == cust.narrow (w, '?'), 0, __LINE__,
                   "UserCtype<%s>::narrow('w', '?') == '?' for an invalid "
                   "entry when not throwing", cname);

        const charT src[] = { y, z, w };
        char dst [3];
        cust.narrow (src, src + 3, '?', dst);
        rw_assert ('x' == dst [0] && '?' == dst [1] && '?' == dst [2],
                   0, __LINE__, "UserCtype<%s>::narrow(\"yzw\", ..., '?', dst) "
                   "== \"x??\"", cname);

#ifndef _RWSTD_NO_EXCEPTIONS

        // a fresh facet, as for widen
        UserCtype<charT> strict;
        strict.narrow_ = narrow_tab;
        strict.wide_   = wide_tab;
        strict.throw_on_invalid_ = true;

        bool caught = false;
        try {
            strict.narrow (w, '?');
        }
        catch (const UserCtypeBase::Exception&) {
            caught = true;
        }
        rw_assert (caught && 1 == strict.n_throws_ [strict.mf_narrow],
                   0, __LINE__, "UserCtype<%s>::narrow('w', '?') throws the "
                   "facet's exception for an invalid entry", cname);

        caught = false;
        try {
            strict.narrow (src, src + 3, '?', dst);
        }
        catch (const UserCtypeBase::Exception&) {
            caught = true;
        }
        rw_assert (caught, 0, __LINE__, "UserCtype<%s>::narrow(\"yzw\", ...) "
                   "throws the facet's exception for an invalid entry", cname);

#endif   // _RWSTD_NO_EXCEPTIONS

    }
}

/***********************************************************************/

/* extern */ int opt_id;

/* extern */ int opt_is;
/* extern */ int opt_scan_is;
/* extern */ int opt_scan_not;

/* extern */ int opt_toupper;
/* extern */ int opt_tolower;

/* extern */ int opt_widen;
/* extern */ int opt_narrow;


template <class charT>
void
test (charT*, const char *cname)
{
#define TEST(func)                                      \
    if (0 <= opt_ ## func)                              \
        test_ ## func ((charT*)0, cname);               \
    else                                                \
        rw_note (0, 0, __LINE__,                        \
                 "UserCtype<%s>::%s() tests disabled",  \
                 cname, # func)

    TEST (id);

    TEST (is);
    TEST (scan_is);
    TEST (scan_not);

    TEST (toupper);
    TEST (tolower);

    TEST (widen);
    TEST (narrow);
}

/***********************************************************************/

static int
run_test (int, char**)
{
    test ((char*)0, "char");

    test ((wchar_t*)0, "wchar_t");

    test ((UserChar*)0, "UserChar");

    return 0;
}

/***********************************************************************/

int main (int argc, char *argv[])
{
    return rw_test (argc, argv, __FILE__,
                    "",
                    0,
                    run_test,
                    "|-id~ "
                    "|-is~ "
                    "|-scan_is~ "
                    "|-scan_not~ "
                    "|-toupper~ "
                    "|-tolower~ "
                    "|-widen~ "
                    "|-narrow~ ",
                    &opt_id,
                    &opt_is,
                    &opt_scan_is,
                    &opt_scan_not,
                    &opt_toupper,
                    &opt_tolower,
                    &opt_widen,
                    &opt_narrow,
                    (void*)0 /* sentinel */);
}
