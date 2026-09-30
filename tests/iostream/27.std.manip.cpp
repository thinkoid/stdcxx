/***********************************************************************
 *
 * 27.std.manip.cpp - test exercising 27.6.3 - Standard manipulators
 *
 * $Id$
 *
 ***********************************************************************
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
 * Copyright 1994-2008 Rogue Wave Software.
 * 
 **************************************************************************/

#include <rw/_defs.h>

   // disabled for MSVC to work around its innumerable bugs

   // prevent the inclusion of non-essential public headers; only headers
   // necessary to compile each header #included here are #included
   // will produce errors if any templates defined in the header are
   // instantiated without the names they use having been completely defined
#define _RWSTD_NO_REDUNDANT_DEFINITIONS

   // prevent out-of-line template definitions in .cc files from being
   // explicitly #included during the processing of library headers (faster
   // compilation) assumes that the test doesn't instantiate those templates
   // on types other than those they were explcitly instantiated on (will
   // have no effect if explicit instantiation is disabled or unsupported)
#define _RWSTD_NO_TEMPLATE_DEFINITIONS


#include <climits>
#include <iomanip>
#include <istream>
#include <ostream>
#include <sstream>

#include <rw_driver.h>

/***********************************************************************/

// user-defined Traits type exercises the ability to instantiate
// std::ws and any supporting templates on types other than the
// default std::char_traits<charT>

template <class charT>
struct CharTraits: std::char_traits<charT> { /* empty */ };

template < typename T >
/*static*/ const char* type_name ();

_RWSTD_SPECIALIZED_FUNCTION
/*static*/ const char*
type_name< char > ()
{
    return "char";
}

_RWSTD_SPECIALIZED_FUNCTION
/*static*/ const char*
type_name< std::char_traits<char> > ()
{
    return "std::char_traits<char>";
}

_RWSTD_SPECIALIZED_FUNCTION
/*static*/ const char*
type_name< CharTraits<char> > ()
{
    return "CharTraits<char>";
}

#ifndef _RWSTD_NO_WCHAR_T

_RWSTD_SPECIALIZED_FUNCTION
/*static*/ const char*
type_name< wchar_t > ()
{
    return "wchar_t";
}

_RWSTD_SPECIALIZED_FUNCTION
/*static*/ const char*
type_name< std::char_traits<wchar_t> > ()
{
    return "std::char_traits<wchar_t>";
}

_RWSTD_SPECIALIZED_FUNCTION
/*static*/ const char*
type_name< CharTraits<wchar_t> > ()
{
    return "CharTraits<wchar_t>";
}

#endif   // _RWSTD_NO_WCHAR_T

/***********************************************************************/

enum Manip {
    resetiosflags,
    setiosflags,
    setbase,
    setfill,
    setprecision,
    setw
};


template <class charT, class Traits>
struct StreamBuf: std::basic_streambuf<charT, Traits> { /* empty */ };


// extracts a long from str after std::setbase (base)
template <class charT>
static long
extract (const charT *str, int base)
{
    std::basic_istringstream<charT, std::char_traits<charT> > istrm (str);

    long val = -1;

    istrm >> std::setbase (base) >> val;

    return val;
}


// clears basefield after std::hex or std::oct in one of the three
// ways 27.4.2.2 and 27.6.3 provide; basefield must end up empty,
// giving prefix-dependent input and decimal output
template <class charT>
static int
test_basefield_clear (int lineno, std::ios_base::fmtflags base, int how)
{
    static const char* const hows [] = {
        "unsetf (basefield)",
        "setf (0, basefield)",
        "resetiosflags (basefield)"
    };

    static const charT str [] = { '0', '1', '0', ' ', '1', '9', '\0' };
    static const charT dec_255 [] = { '2', '5', '5', '\0' };

    std::basic_stringstream<charT, std::char_traits<charT> > strm (str);

    strm.setf (base, std::ios_base::basefield);

    if (0 == how)
        strm.unsetf (std::ios_base::basefield);
    else if (1 == how)
        strm.setf (std::ios_base::fmtflags (0), std::ios_base::basefield);
    else
        strm >> std::resetiosflags (std::ios_base::basefield);

    const std::ios_base::fmtflags basefield =
        strm.flags () & std::ios_base::basefield;

    long val_010 = -1;
    long val_19  = -1;

    strm >> val_010 >> val_19;

    std::basic_ostringstream<charT, std::char_traits<charT> > ostrm;

    ostrm.flags (strm.flags ());
    ostrm << 255;

    const bool success =
           0 == basefield && 8 == val_010 && 19 == val_19
        && ostrm.str () == dec_255;

    rw_assert (success, __FILE__, lineno,
               "%{If} followed by %s: basefield %{If}, \"010 19\" "
               "extracted as %ld and %ld, 255 inserted as %{*Ac}; "
               "expected fmtflags(0), 8, 19, \"255\"",
               int (base), hows [how], int (basefield), val_010, val_19,
               int (sizeof (charT)), ostrm.str ().c_str ());

    return !success;
}


template <class charT, class Traits>
static int
test (int lineno, Manip m, int iarg, charT carg)
{
    static/*?*/ const char* const cname = type_name< charT > ();
    static/*?*/ const char* const tname = type_name< Traits > ();

    typedef std::ios_base::fmtflags            Fmtflags;
    typedef std::basic_istream<charT, Traits>  Istream;
    typedef std::basic_ostream<charT, Traits>  Ostream;
    typedef std::basic_iostream<charT, Traits> IOstream;

    StreamBuf<charT, Traits> sb;

    // stream objects used to apply manipulators to
    Istream  in1 (&sb);
    Ostream  out1 (&sb);
    IOstream inout1 (&sb);

    // stream objects used to call member functions on
    Istream  in2 (&sb);
    Ostream  out2 (&sb);
    IOstream inout2 (&sb);

    int nfailed = 0;

    switch (m) {

    case resetiosflags: {   // exercise 27.6.3, p3

        Istream &in3 = in1 >> std::resetiosflags (Fmtflags (iarg));
        in2.setf (Fmtflags (0), Fmtflags (iarg));

        if (in2.flags () != in3.flags ()) {
            ++nfailed;
            rw_assert (0, __FILE__, lineno,
                       "in >> std::resetiosflags(%{If}) with basic_istream "
                       "<%s, %s >", iarg, cname, tname);
        }

        Ostream &out3 = out1 << std::resetiosflags (Fmtflags (iarg));
        out2.setf (Fmtflags (0), Fmtflags (iarg));

        if (out2.flags () != out3.flags ()) {
            ++nfailed;
            rw_assert (0, __FILE__, lineno,
                       "out << std::resetiosflags(%{If}) with basic_ostream "
                       "<%s, %s >", iarg, cname, tname);
        }

        Ostream &inout3 = inout1 << std::resetiosflags (Fmtflags (iarg));
        inout2.setf (Fmtflags (0), Fmtflags (iarg));

        if (inout2.flags () != inout3.flags ()) {
            ++nfailed;
            rw_assert (0, __FILE__, lineno,
                       "io << std::resetiosflags(%{If}) with basic_iostream "
                       "<%s, %s >", iarg, cname, tname);
        }

        break;
    }
        
    case setiosflags: {   // exercise 27.6.3, p3

        Istream &in3 = in1 >> std::setiosflags (Fmtflags (iarg));
        in2.setf (Fmtflags (iarg));

        if (in2.flags () != in3.flags ()) {
            ++nfailed;
            rw_assert (0, __FILE__, lineno,
                       "std::setiosflags (%s) with basic_istream "
                       "<%s, %s >", iarg, cname, tname);
        }

        Ostream &out3 = out1 << std::setiosflags (Fmtflags (iarg));
        out2.setf (Fmtflags (iarg));

        if (out2.flags () != out3.flags ()) {
            ++nfailed;
            rw_assert (0, __FILE__, lineno,
                       "std::setiosflags (%s) with basic_ostream "
                       "<%s, %s >", iarg, cname, tname);
        }

        Ostream &inout3 = inout1 << std::setiosflags (Fmtflags (iarg));
        inout2.setf (Fmtflags (iarg));

        if (inout2.flags () != inout3.flags ()) {
            ++nfailed;
            rw_assert (0, __FILE__, lineno,
                       "std::setiosflags (%s) with basic_iostream "
                       "<%s, %s >", iarg, cname, tname);
        }

        break;
    }

    case setbase: {   // exercise 27.6.3, p5

        // the standard's f() sets basefield to oct, dec or hex for 8,
        // 10 and 16 and clears it for every other value: decimal output
        // and prefix-dependent input
        const Fmtflags basefield =
              8 == iarg ? std::ios_base::oct
           : 10 == iarg ? std::ios_base::dec
           : 16 == iarg ? std::ios_base::hex
           : Fmtflags (0);

        out1 << std::setbase (iarg);

        if ((out1.flags () & std::ios_base::basefield) != basefield) {
            ++nfailed;
            rw_assert (0, __FILE__, lineno,
                       "std::setbase (%d) set basefield to %{If}, "
                       "expected %{If}", iarg,
                       int (out1.flags () & std::ios_base::basefield),
                       int (basefield));
        }

        // 1234 inserted in the base basefield selects
        static const charT oct_1234 [] = { '2', '3', '2', '2', '\0' };
        static const charT dec_1234 [] = { '1', '2', '3', '4', '\0' };
        static const charT hex_1234 [] = { '4', 'd', '2', '\0' };

        const charT* const ins =
              8 == iarg ? oct_1234
           : 16 == iarg ? hex_1234
           : dec_1234;

        std::basic_ostringstream<charT, std::char_traits<charT> > ostrm;

        ostrm << std::setbase (iarg) << 1234;

        if (ostrm.str () != ins) {
            ++nfailed;
            rw_assert (0, __FILE__, lineno,
                       "std::setbase (%d) inserted 1234 as %{*Ac}; "
                       "expected %{*Ac}", iarg,
                       int (sizeof (charT)), ostrm.str ().c_str (),
                       int (sizeof (charT)), ins);
        }

        // "010" and "19" extracted in that base: 8 and 1 in octal, 10
        // and 19 in decimal, 16 and 25 in hex, 8 and 19 prefix-dependent
        static const charT str_010 [] = { '0', '1', '0', '\0' };
        static const charT str_19 []  = { '1', '9', '\0' };

        const long ext_010 = 10 == iarg ? 10 : 16 == iarg ? 16 : 8;
        const long ext_19  =  8 == iarg ? 1 : 16 == iarg ? 25 : 19;

        const long got_010 = extract (str_010, iarg);
        const long got_19  = extract (str_19, iarg);

        if (got_010 != ext_010 || got_19 != ext_19) {
            ++nfailed;
            rw_assert (0, __FILE__, lineno,
                       "std::setbase (%d) extracted %{*Ac} as %ld and "
                       "%{*Ac} as %ld; expected %ld and %ld", iarg,
                       int (sizeof (charT)), str_010, got_010,
                       int (sizeof (charT)), str_19, got_19,
                       ext_010, ext_19);
        }

        break;
    }

    case setfill: {   // exercise 27.6.3, p6
        Istream &in3 = in1 >> std::setfill (carg);
        in2.fill (carg);

        if (in2.fill () != in3.fill ()) {
            ++nfailed;
            rw_assert (0, __FILE__, lineno,
                       "in >> std::setfill('%c') with basic_istream"
                       "<%s, %s >", carg, cname, tname);
        }

        Ostream &out3 = out1 << std::setfill (carg);
        out2.fill (carg);

        if (out2.fill () != out3.fill ()) {
            ++nfailed;
            rw_assert (0, __FILE__, lineno,
                       "out << std::setfill('%c') with basic_ostream"
                       "<%s, %s >", carg, cname, tname);
        }

        Ostream &inout3 = inout1 << std::setfill (carg);
        inout2.fill (carg);

        if (inout2.fill () != inout3.fill ()) {
            ++nfailed;
            rw_assert (0, __FILE__, lineno,
                       "io << std::setfill('%c') with basic_iostream"
                       "<%s, %s >", carg, cname, tname);
        }

        break;
    }

    case setprecision: {   // exercise 27.6.3, p7

        Istream &in3 = in1 >> std::setprecision (iarg);
        in2.precision (std::streamsize (iarg));

        if (in2.precision () != in3.precision ()) {
            ++nfailed;
            rw_assert (0, __FILE__, lineno,
                       "std::setprecision (%d) with basic_istream "
                       "<%s, %s >", iarg, cname, tname);
        }

        Ostream &out3 = out1 << std::setprecision (iarg);
        out2.precision (std::streamsize (iarg));

        if (out2.precision () != out3.precision ()) {
            ++nfailed;
            rw_assert (0, __FILE__, lineno,
                       "std::setprecision (%d) with basic_ostream "
                       "<%s, %s >", iarg, cname, tname);
        }

        Ostream &inout3 = inout1 << std::setprecision (iarg);
        inout2.precision (std::streamsize (iarg));

        if (inout2.precision () != inout3.precision ()) {
            ++nfailed;
            rw_assert (0, __FILE__, lineno,
                       "std::setprecision (%d) with basic_iostream "
                       "<%s, %s >", iarg, cname, tname);
        }

        break;
    }

    case setw: {   // exercise 27.6.3, p8

        Istream &in3 = in1 >> std::setw (iarg);
        in2.width (std::streamsize (iarg));

        if (in2.width () != in3.width ()) {
            ++nfailed;
            rw_assert (0, __FILE__, lineno,
                       "std::setw (%d) with basic_istream<%s, %s >",
                       iarg, cname, tname);
        }

        Ostream &out3 = out1 << std::setw (iarg);
        out2.width (std::streamsize (iarg));

        if (out2.width () != out3.width ()) {
            ++nfailed;
            rw_assert (0, __FILE__, lineno,
                       "std::setw (%d) with basic_ostream<%s, %s >",
                       iarg, cname, tname);
        }

        Ostream &inout3 = inout1 << std::setw (iarg);
        inout2.width (std::streamsize (iarg));

        if (inout2.width () != inout3.width ()) {
            ++nfailed;
            rw_assert (0, __FILE__, lineno,
                       "std::setw (%d) with basic_iostream<%s, %s >",
                       iarg, cname, tname);
        }

        break;
    }

    }

    return nfailed;
}

/***********************************************************************/

// for convenience
#define Boolalpha   std::ios_base::boolalpha
#define Dec         std::ios_base::dec
#define Fixed       std::ios_base::fixed
#define Hex         std::ios_base::hex
#define Internal    std::ios_base::internal
#define Left        std::ios_base::left
#define Oct         std::ios_base::oct
#define Right       std::ios_base::right
#define Scientific  std::ios_base::scientific
#define Showbase    std::ios_base::showbase
#define Showpoint   std::ios_base::showpoint
#define Showpos     std::ios_base::showpos
#define Skipws      std::ios_base::skipws
#define Unitbuf     std::ios_base::unitbuf
#define Uppercase   std::ios_base::uppercase
#define Bin         std::ios_base::bin
#define Adjustfield std::ios_base::adjustfield
#define Basefield   std::ios_base::basefield
#define Floatfield  std::ios_base::floatfield
#define Nolock      std::ios_base::nolock
#define Nolockbuf   std::ios_base::nolockbuf


template <class charT, class Traits>
static void do_test ()
{
#define TEST(M, V, C) \
    ++ntried; nfailed += test< charT, Traits > (__LINE__, M, V, C)

    int ntried  = 0;
    int nfailed = 0;

    /////////////////////////////////////////////////////////////////////
    // exercise std::resetiosflags

    const char* const cname = type_name< charT > ();
    const char* const tname = type_name< Traits > ();

    rw_info (0, __FILE__, __LINE__,
             "27.6.3, p3 - std::resetiosflags(ios_base::"
             "fmtflags) with basic_{i,o,io}stream<%s, %s >",
             cname, tname);

    TEST (resetiosflags, Boolalpha, charT ());
    TEST (resetiosflags, Dec, charT ());
    TEST (resetiosflags, Fixed, charT ());
    TEST (resetiosflags, Hex, charT ());
    TEST (resetiosflags, Internal, charT ());
    TEST (resetiosflags, Left, charT ());
    TEST (resetiosflags, Oct, charT ());
    TEST (resetiosflags, Right, charT ());
    TEST (resetiosflags, Scientific, charT ());
    TEST (resetiosflags, Showbase, charT ());
    TEST (resetiosflags, Showpoint, charT ());
    TEST (resetiosflags, Showpos, charT ());
    TEST (resetiosflags, Skipws, charT ());
    TEST (resetiosflags, Unitbuf, charT ());
    TEST (resetiosflags, Uppercase, charT ());

#ifndef _RWSTD_NO_EXT_BIN_IO

    TEST (resetiosflags, Bin, charT ());

#endif   // _RWSTD_NO_EXT_BIN_IO

    TEST (resetiosflags, Adjustfield, charT ());
    TEST (resetiosflags, Basefield, charT ());
    TEST (resetiosflags, Floatfield, charT ());

#ifndef _RWSTD_NO_EXT_REENTRANT_IO

    TEST (resetiosflags, Nolock, charT ());
    TEST (resetiosflags, Nolockbuf, charT ());

#endif   // _RWSTD_NO_EXT_REENTRANT_IO


    TEST (resetiosflags, Oct | Dec,       charT ());
    TEST (resetiosflags, Oct | Hex,       charT ());
    TEST (resetiosflags, Dec | Hex,       charT ());
    TEST (resetiosflags, Oct | Dec | Hex, charT ());
    
    TEST (resetiosflags, Left | Internal,         charT ());
    TEST (resetiosflags, Left | Right,            charT ());
    TEST (resetiosflags, Right | Internal,        charT ());
    TEST (resetiosflags, Left | Right | Internal, charT ());

    TEST (resetiosflags, Showbase | Showpoint,           charT ());
    TEST (resetiosflags, Showbase | Showpos,             charT ());
    TEST (resetiosflags, Showbase | Showpoint | Showpos, charT ());

    if (!nfailed)
        rw_assert (true, __FILE__, __LINE__,
                   "std::resetiosflags failed %d out "
                   "of %d assertions", nfailed, ntried);

    /////////////////////////////////////////////////////////////////////
    // exercise std::setiosflags

    rw_info (0, __FILE__, __LINE__,
             "27.6.3, p4 - std::setiosflags(ios_base::"
             "fmtflags) with basic_{i,o,io}stream<%s, %s >",
             cname, tname);

    ntried = nfailed = 0;

    TEST (setiosflags, Boolalpha, charT ());
    TEST (setiosflags, Dec, charT ());
    TEST (setiosflags, Fixed, charT ());
    TEST (setiosflags, Hex, charT ());
    TEST (setiosflags, Internal, charT ());
    TEST (setiosflags, Left, charT ());
    TEST (setiosflags, Oct, charT ());
    TEST (setiosflags, Right, charT ());
    TEST (setiosflags, Scientific, charT ());
    TEST (setiosflags, Showbase, charT ());
    TEST (setiosflags, Showpoint, charT ());
    TEST (setiosflags, Showpos, charT ());
    TEST (setiosflags, Skipws, charT ());
    TEST (setiosflags, Unitbuf, charT ());
    TEST (setiosflags, Uppercase, charT ());

#ifndef _RWSTD_NO_EXT_BIN_IO

    TEST (setiosflags, Bin, charT ());

#endif   // _RWSTD_NO_EXT_BIN_IO

    TEST (setiosflags, Adjustfield, charT ());
    TEST (setiosflags, Basefield, charT ());
    TEST (setiosflags, Floatfield, charT ());
    TEST (setiosflags, Nolock, charT ());
    TEST (setiosflags, Nolockbuf, charT ());

    if (!nfailed)
        rw_assert (true, __FILE__, __LINE__,
                   "std::setiosflags failed %d out "
                   "of %d assertions", nfailed, ntried);

    /////////////////////////////////////////////////////////////////////
    // exercise std::setbase

    rw_info (0, __FILE__, __LINE__,
             "27.6.3, p5 - std::setbase(int) with "
             "basic_{i,o,io}stream<%s, %s >", cname, tname);

    ntried = nfailed = 0;

    TEST (setbase,  8, charT ());   // oct
    TEST (setbase, 10, charT ());   // dec
    TEST (setbase, 16, charT ());   // hex

    // every other value clears basefield
    TEST (setbase,  0, charT ());
    TEST (setbase,  1, charT ());
    TEST (setbase,  2, charT ());
    TEST (setbase,  3, charT ());
    TEST (setbase,  7, charT ());
    TEST (setbase,  9, charT ());
    TEST (setbase, 11, charT ());
    TEST (setbase, 17, charT ());
    TEST (setbase, 36, charT ());
    TEST (setbase, 37, charT ());
    TEST (setbase, -1, charT ());
    TEST (setbase, -2, charT ());

    if (!nfailed)
        rw_assert (true, __FILE__, __LINE__,
                   "std::setbase failed %d out "
                   "of %d assertions", nfailed, ntried);

    /////////////////////////////////////////////////////////////////////
    // exercise clearing basefield after std::hex and std::oct

    rw_info (0, __FILE__, __LINE__,
             "27.4.2.2, p6 and p8, 27.6.3, p3 - clearing basefield with "
             "basic_stringstream<%s, %s >", cname, tname);

    for (int how = 0; how != 3; ++how) {
        test_basefield_clear<charT>(__LINE__, std::ios_base::hex, how);
        test_basefield_clear<charT>(__LINE__, std::ios_base::oct, how);
    }

    /////////////////////////////////////////////////////////////////////
    // exercise std::setfill

    rw_info (0, __FILE__, __LINE__,
             "27.6.3, p6 - std::setfill(%s) with "
             "basic_{i,o,io}stream<%s, %s >", cname, cname, tname);

    ntried = nfailed = 0;

    TEST (setfill, 0, charT ('\0'));
    TEST (setfill, 0, charT ('\a'));
    TEST (setfill, 0, charT ('\n'));
    TEST (setfill, 0, charT ('\r'));
    TEST (setfill, 0, charT ('\t'));
    TEST (setfill, 0, charT ('\v'));
    TEST (setfill, 0, charT (' '));
    TEST (setfill, 0, charT ('*'));
    TEST (setfill, 0, charT ('#'));

    if (!nfailed)
        rw_assert (true, __FILE__, __LINE__,
                   "std::setfill failed %d out "
                   "of %d assertions", nfailed, ntried);
    
    /////////////////////////////////////////////////////////////////////
    // exercise std::setprecision

    rw_info (0, __FILE__, __LINE__,
             "27.6.3, p7 - std::setprecision(int) with "
             "basic_{i,o,io}stream<%s, %s >", cname, tname);

    ntried = nfailed = 0;

    TEST (setprecision, 0,         charT ());
    TEST (setprecision, 1,         charT ());
    TEST (setprecision, 2,         charT ());
    TEST (setprecision, SHRT_MAX,  charT ());
    TEST (setprecision, INT_MAX,   charT ());
    TEST (setprecision, -1,        charT ());
    TEST (setprecision, -2,        charT ());
    TEST (setprecision, SHRT_MIN,  charT ());
    TEST (setprecision, INT_MIN,   charT ());

    if (!nfailed)
        rw_assert (true, __FILE__, __LINE__,
                   "std::setprecision failed %d out "
                   "of %d assertions", nfailed, ntried);

    /////////////////////////////////////////////////////////////////////
    // exercise std::setw

    rw_info (0, __FILE__, __LINE__,
             "27.6.3, p8 - std::setw(int) with "
             "basic_{i,o,io}stream<%s, %s >", cname, tname);

    ntried = nfailed = 0;

    TEST (setw, 0,         charT ());
    TEST (setw, 1,         charT ());
    TEST (setw, 2,         charT ());
    TEST (setw, SHRT_MAX,  charT ());
    TEST (setw, INT_MAX,   charT ());
    TEST (setw, -1,        charT ());
    TEST (setw, -2,        charT ());
    TEST (setw, SHRT_MIN,  charT ());
    TEST (setw, INT_MIN,   charT ());

    if (!nfailed)
        rw_assert (true, __FILE__, __LINE__,
                   "std::setw failed %d out "
                   "of %d assertions", nfailed, ntried);
}

/***********************************************************************/

static int
run_test (int /*argc*/, char* /*argv*/ [])
{
    do_test<char, std::char_traits<char> > ();
    do_test<char, CharTraits<char> > ();

#ifndef _RWSTD_NO_WCHAR_T

    do_test<wchar_t, std::char_traits<wchar_t> > ();
    do_test<wchar_t, CharTraits<wchar_t> > ();

#endif   // _RWSTD_NO_WCHAR_T

    return 0;
}


/*extern*/ int
main (int argc, char* argv [])
{
    return rw_test (argc, argv, __FILE__,
                    "[lib.std.manip]",
                    "27.6.3 Standard manipulators",
                    run_test, "", 0);
}

