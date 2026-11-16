/***************************************************************************
 *
 * 22.locale.category.cpp - test exercising the byname facets of
 *                          [lib.locale.category] a named locale holds:
 *                          one type_info object for each type
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

// 22.1.1.1.1, p4 (Table 52) has the library provide the byname facets,
// and a locale constructed by name holds them: the library creates
// each one, and the program names its type. A locale named other than
// "C" holds the byname form in each of the 18 slots that have one,
// whether the library reads its own database or the C library's. The
// eight slots of num_get, num_put, money_get and money_put, which
// have no byname form, hold the base facet the library makes for any
// name (__rw_locale::_C_get_slot_facet in src/locale_body.cpp), and
// the test asks the same of its type_info.
//
// 18.5.1 lets two type_info objects describe one type: operator==
// compares the types they describe, not the objects. The test asks for
// one object anyway. A shared library with its export marks in place
// gives one: the library exports the type_info and the vtable of each
// byname facet, and the program binds to them. A missing mark gives
// two: the library keeps a hidden copy and the program has its own.
// The language support library compares type_info objects by name, so
// operator==, dynamic_cast and the handlers of a try block still
// succeed then, and only the addresses show the second object. In an
// archive the library and the program are one link unit, with one
// object for each type whatever the marks.

#include <locale>      // for locale, use_facet, the facets
#include <stdexcept>   // for runtime_error
#include <typeinfo>    // for type_info

#include <cstring>     // for strcpy()
#include <cwchar>      // for mbstate_t

#include <rw_locale.h> // for rw_localedef(), rw_set_locale_root()
#include <rw_driver.h>

/**************************************************************************/

// takes the facet through use_facet<Facet> and compares the address of
// the type_info of its dynamic type with that of the program's Type
template <class Facet, class Type>
static void
test_facet (int line, const std::locale &loc, const char *locname,
            const char *fname, const char *tname)
{
    const Facet &facet = std::use_facet<Facet>(loc);

    const std::type_info &lib = typeid (facet);
    const std::type_info &pgm = typeid (Type);

    rw_assert (&lib == &pgm, 0, line,
               "&typeid (std::use_facet<%s>(std::locale (%#s))) == "
               "&typeid (%s): the library's type_info is at %p, "
               "the program's at %p",
               fname, locname, tname, &lib, &pgm);
}

#define TEST(Facet, Type)                                               \
    test_facet< Facet, Type >(__LINE__, loc, locname, #Facet, #Type)


typedef std::codecvt<char, char, std::mbstate_t>           Codecvt;
typedef std::codecvt<wchar_t, char, std::mbstate_t>        WCodecvt;
typedef std::codecvt_byname<char, char, std::mbstate_t>    CodecvtByname;
typedef std::codecvt_byname<wchar_t, char, std::mbstate_t> WCodecvtByname;

typedef std::moneypunct<char, false>            Moneypunct;
typedef std::moneypunct<char, true>             MoneypunctIntl;
typedef std::moneypunct<wchar_t, false>         WMoneypunct;
typedef std::moneypunct<wchar_t, true>          WMoneypunctIntl;
typedef std::moneypunct_byname<char, false>     MoneypunctByname;
typedef std::moneypunct_byname<char, true>      MoneypunctIntlByname;
typedef std::moneypunct_byname<wchar_t, false>  WMoneypunctByname;
typedef std::moneypunct_byname<wchar_t, true>   WMoneypunctIntlByname;


static void
test_facets (const char *locname)
{
    rw_info (0, 0, __LINE__,
             "the type_info of each standard facet of std::locale (%#s)",
             locname);

    const std::locale loc (locname);

    TEST (std::collate<char>, std::collate_byname<char>);
    TEST (std::collate<wchar_t>, std::collate_byname<wchar_t>);
    TEST (std::ctype<char>, std::ctype_byname<char>);
    TEST (std::ctype<wchar_t>, std::ctype_byname<wchar_t>);
    TEST (Codecvt, CodecvtByname);
    TEST (WCodecvt, WCodecvtByname);

    TEST (Moneypunct, MoneypunctByname);
    TEST (MoneypunctIntl, MoneypunctIntlByname);
    TEST (WMoneypunct, WMoneypunctByname);
    TEST (WMoneypunctIntl, WMoneypunctIntlByname);
    TEST (std::numpunct<char>, std::numpunct_byname<char>);
    TEST (std::numpunct<wchar_t>, std::numpunct_byname<wchar_t>);

    TEST (std::time_get<char>, std::time_get_byname<char>);
    TEST (std::time_get<wchar_t>, std::time_get_byname<wchar_t>);
    TEST (std::time_put<char>, std::time_put_byname<char>);
    TEST (std::time_put<wchar_t>, std::time_put_byname<wchar_t>);
    TEST (std::messages<char>, std::messages_byname<char>);
    TEST (std::messages<wchar_t>, std::messages_byname<wchar_t>);

    // the slots without a byname form hold the base facet
    TEST (std::money_get<char>, std::money_get<char>);
    TEST (std::money_get<wchar_t>, std::money_get<wchar_t>);
    TEST (std::money_put<char>, std::money_put<char>);
    TEST (std::money_put<wchar_t>, std::money_put<wchar_t>);
    TEST (std::num_get<char>, std::num_get<char>);
    TEST (std::num_get<wchar_t>, std::num_get<wchar_t>);
    TEST (std::num_put<char>, std::num_put<char>);
    TEST (std::num_put<wchar_t>, std::num_put<wchar_t>);
}

/**************************************************************************/

// 22.1.1.2, p7: the library throws runtime_error for a name that is
// not a locale, and the program catches it by its type
static void
test_exception ()
{
    const char* const locname = "no-such-locale";

    rw_info (0, 0, __LINE__,
             "std::locale (%#s) throws std::runtime_error", locname);

    const char           *caught = "nothing";
    const std::type_info *lib    = 0;

    try {
        const std::locale loc (locname);
    }
    catch (const std::runtime_error &e) {
        caught = "runtime_error";
        lib    = &typeid (e);
    }
    catch (...) {
        caught = "another exception";
    }

    rw_assert (0 != lib, 0, __LINE__,
               "std::locale (%#s) threw std::runtime_error, caught %s",
               locname, caught);

    if (0 == lib)
        return;

    const std::type_info* const pgm = &typeid (std::runtime_error);

    rw_assert (lib == pgm, 0, __LINE__,
               "&typeid (e) == &typeid (std::runtime_error) for the "
               "exception std::locale (%#s) threw: the library's "
               "type_info is at %p, the program's at %p",
               locname, lib, pgm);
}

/**************************************************************************/

static int
run_test (int, char**)
{
    // a locale database of the tree's own, built in a directory of the
    // test's, so that the test does not depend on the host's locales
    rw_set_locale_root ();

    const char* const locname = rw_localedef ("-w", "de_DE", "ISO-8859-1", 0);

    if (0 == locname) {
        rw_warn (false, 0, __LINE__,
                 "failed to build locale de_DE.ISO-8859-1; "
                 "the facets not tested");
    }
    else {
        char namebuf [64];
        std::strcpy (namebuf, locname);

        test_facets (namebuf);
    }

    test_exception ();

    return 0;
}

/**************************************************************************/

int main (int argc, char *argv[])
{
    return rw_test (argc, argv, __FILE__,
                    "lib.locale.category",
                    "the type_info of the byname facets",
                    run_test,
                    0 /* no command line options */);
}
