/***************************************************************************
 *
 * 22.locale.cons.cpp - test exercising the std::locale constructors
 *                      [lib.locale.cons]: the exceptions of those that
 *                      take a name or a category, the combinations of
 *                      categories, and the names of the results
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

// 22.1.1.2, p7 and p10 have the constructors from a name throw
// runtime_error for a name that is not valid or is null. p14 gives the
// constructor from two locales and a category no Throws clause; the
// library throws runtime_error there too for a category outside
// locale::all, and the constructor from a name and a category checks
// the category before the name (doc/notes/ref-locale.md 8.3, "An
// invalid category throws"). The message names the bad argument: the
// name, "(null)" for a null one, or the category value in hexadecimal.
//
// The combinations take a locale of the C library, de_DE.UTF-8, and
// run where setlocale knows it. The name of a mixed locale lists six
// categories in the library's order, that of config.h's _RWSTD_CAT_0
// to _RWSTD_CAT_5 on glibc (doc/notes/ref-locale.md 8.2).

#include <clocale>     // for LC_ALL, LC_COLLATE, setlocale()
#include <cstdio>      // for sprintf()
#include <cstring>     // for strchr(), strcmp(), strcpy(), strlen(), ...
#include <locale>      // for locale, has_facet, use_facet, the facets
#include <stdexcept>   // for runtime_error
#include <string>      // for string

#include <rw_driver.h>

/**************************************************************************/

// checks the name of `loc', which `call' constructed, and that a locale
// constructed from that name equals it (22.1.1.3, p5)
static void
test_name (int line, const std::locale &loc, const char *call,
           const char *expect)
{
    const std::string name = loc.name ();

    rw_assert (0 == std::strcmp (expect, name.c_str ()), 0, line,
               "%s.name () == %#s, got %#s", call, expect, name.c_str ());

    if ("*" == name)
        return;

    try {
        rw_assert (std::locale (name.c_str ()) == loc, 0, line,
                   "std::locale (%s.name ().c_str ()) == %s", call, call);
    }
    catch (...) {
        rw_assert (false, 0, line,
                   "std::locale (%#s), the name of %s, threw an exception",
                   name.c_str (), call);
    }
}

/**************************************************************************/

// 22.1.1.2, p2, p3 and p6: the default constructor gives the classic
// locale while locale::global has not been called, the copy equals its
// source, and the constructor from a name gives the locale of that
// name; "C" is the classic one (22.1.1.5, p5)
static void
test_good ()
{
    rw_info (0, 0, __LINE__,
             "std::locale (), std::locale (const locale&), "
             "std::locale (const char*)");

    const std::locale classic = std::locale::classic ();
    const std::locale def;
    const std::locale copy (def);
    const std::locale c ("C");

    test_name (__LINE__, def, "std::locale ()", "C");
    rw_assert (def == classic, 0, __LINE__,
               "std::locale () == std::locale::classic ()");

    test_name (__LINE__, copy, "std::locale (std::locale ())", "C");
    rw_assert (copy == def, 0, __LINE__,
               "std::locale (std::locale ()) == std::locale ()");

    test_name (__LINE__, c, "std::locale (\"C\")", "C");
    rw_assert (c == classic, 0, __LINE__,
               "std::locale (\"C\") == std::locale::classic ()");
}

/**************************************************************************/

// 22.1.1.2, p6 and p8: "" names the locale of the environment; it is
// named (22.1.1, p8), by one name when every category agrees and by
// the library's mixed form otherwise
static void
test_native ()
{
    char native [1024];

    const char* const env = std::setlocale (LC_ALL, "");
    const bool ok = env && std::strlen (env) < sizeof native;
    if (ok)
        std::strcpy (native, env);

    std::setlocale (LC_ALL, "C");

    if (!ok) {
        rw_warn (0, 0, __LINE__,
                 "the C library knows no locale of the environment, "
                 "std::locale (\"\") not tested");
        return;
    }

    rw_info (0, 0, __LINE__, "std::locale (\"\"), the environment's %#s",
             native);

    try {
        const std::locale loc ("");

        if (0 == std::strchr (native, '=')) {
            test_name (__LINE__, loc, "std::locale (\"\")", native);
        }
        else {
            rw_assert ("*" != loc.name (), 0, __LINE__,
                       "std::locale (\"\").name () != \"*\", got %#s",
                       loc.name ().c_str ());
            rw_assert (std::locale (loc.name ().c_str ()) == loc, 0, __LINE__,
                       "std::locale (std::locale (\"\").name ().c_str ()) "
                       "== std::locale (\"\")");
        }
    }
    catch (...) {
        rw_assert (false, 0, __LINE__,
                   "std::locale (\"\") threw an exception where setlocale "
                   "(LC_ALL, \"\") returned %#s", native);
    }
}

/**************************************************************************/

// the constructors that take a name or a category, the second and the
// third from the classic locale
enum { name_ctor, name_cat_ctor, locale_cat_ctor };

static const char* const ctor_sigs [] = {
    "const char*",
    "const locale&, const char*, category",
    "const locale&, const locale&, category"
};


static void
construct (int ctor, const char *locname, std::locale::category cat)
{
    const std::locale classic = std::locale::classic ();

    if (name_ctor == ctor) {
        const std::locale loc (locname);
    }
    else if (name_cat_ctor == ctor) {
        const std::locale loc (classic, locname, cat);
    }
    else {
        const std::locale loc (classic, classic, cat);
    }
}


// checks that the constructor `ctor' throws runtime_error for the name
// `locname' and the category `cat', and that what () holds `expect'
static void
test_throws (int line, int ctor, const char *locname,
             std::locale::category cat, const char *expect)
{
    const char *caught = "nothing";

    try {
        construct (ctor, locname, cat);
    }
    catch (const std::runtime_error &e) {
        rw_assert (0 != std::strstr (e.what (), expect), 0, line,
                   "std::locale (%s) with %#s and %#x threw "
                   "std::runtime_error, what () == %#s holds %#s",
                   ctor_sigs [ctor], locname, cat, e.what (), expect);
        return;
    }
    catch (...) {
        caught = "another exception";
    }

    rw_assert (false, 0, line,
               "std::locale (%s) with %#s and %#x threw "
               "std::runtime_error, caught %s",
               ctor_sigs [ctor], locname, cat, caught);
}


// 22.1.1.2, p7 and p10: a bad or null name throws runtime_error, and so
// does a category outside locale::all, an extension (p14)
static void
test_bad ()
{
    rw_info (0, 0, __LINE__,
             "std::runtime_error for a bad name, a null name and "
             "a bad category");

    const char* const bad     = "no-such-locale";
    const char* const badmsg  = "bad locale name: \"no-such-locale\"";
    const char* const nullmsg = "bad locale name: \"(null)\"";

    const std::locale::category badcat = ~std::locale::all;

    char catmsg [64];
    std::sprintf (catmsg, "bad category value: %#x", unsigned (badcat));

    const std::locale::category collate = std::locale::collate;

    test_throws (__LINE__, name_ctor, bad, collate, badmsg);
    test_throws (__LINE__, name_ctor, 0, collate, nullmsg);

    test_throws (__LINE__, name_cat_ctor, bad, collate, badmsg);
    test_throws (__LINE__, name_cat_ctor, 0, collate, nullmsg);

    // the name is good and the category is named, not the name
    test_throws (__LINE__, name_cat_ctor, "C", badcat, catmsg);

    test_throws (__LINE__, locale_cat_ctor, 0, badcat, catmsg);
}

/**************************************************************************/

// checks that the facet Facet of `loc' comes from the named locale,
// the byname facet, when `named', and otherwise from the classic one
template <class Facet, class Byname>
static void
test_facet (int line, const std::locale &loc, const char *call,
            bool named, const char *fname)
{
    const std::locale classic = std::locale::classic ();

    const bool is_classic =
        &std::use_facet<Facet>(loc) == &std::use_facet<Facet>(classic);
    const bool is_byname  = std::has_facet<Byname>(loc);

    rw_assert (named ? is_byname && !is_classic : is_classic, 0, line,
               "std::use_facet<%s>(%s) comes from the %s locale: "
               "the classic facet %d, a byname facet %d",
               fname, call, named ? "named" : "classic",
               is_classic, is_byname);
}


// checks the name of `loc' and which locale its collate, ctype and
// numpunct facets come from
static void
test_combined (int line, const std::locale &loc, const char *call,
               const char *expect, bool collate, bool ctype)
{
    test_name (line, loc, call, expect);

    test_facet<std::collate<char>, std::collate_byname<char> >(
        line, loc, call, collate, "std::collate<char>");
    test_facet<std::ctype<char>, std::ctype_byname<char> >(
        line, loc, call, ctype, "std::ctype<char>");
    test_facet<std::numpunct<char>, std::numpunct_byname<char> >(
        line, loc, call, false, "std::numpunct<char>");
}


// 22.1.1.2, p9-11 and p14-15: the facets of the category come from the
// second argument, the rest from the first, and the result is named
// when both are; 22.1.1.1.1, p1: collate & ctype, the intersection of
// disjoint categories, selects nothing; p2: the category may be an
// LC_ constant of <clocale>
static void
test_categories ()
{
    const char* const dename = "de_DE.UTF-8";

    const bool known = 0 != std::setlocale (LC_ALL, dename);

    std::setlocale (LC_ALL, "C");

    if (!known) {
        rw_warn (0, 0, __LINE__,
                 "the C library knows no locale %#s, the combinations "
                 "not tested", dename);
        return;
    }

    rw_info (0, 0, __LINE__,
             "std::locale (std::locale::classic (), %#s, category) and "
             "std::locale (std::locale::classic (), std::locale (%#s), "
             "category)", dename, dename);

    const char* const both =
        "LC_CTYPE=de_DE.UTF-8;LC_NUMERIC=C;LC_TIME=C;"
        "LC_COLLATE=de_DE.UTF-8;LC_MONETARY=C;LC_MESSAGES=C";
    const char* const coll =
        "LC_CTYPE=C;LC_NUMERIC=C;LC_TIME=C;"
        "LC_COLLATE=de_DE.UTF-8;LC_MONETARY=C;LC_MESSAGES=C";

    const std::locale classic = std::locale::classic ();
    const std::locale de (dename);
    const std::locale copy (de);

    test_name (__LINE__, de, "std::locale (\"de_DE.UTF-8\")", dename);
    test_name (__LINE__, copy, "std::locale (de)", dename);
    rw_assert (copy == de, 0, __LINE__, "std::locale (de) == de");

    const std::locale::category collate = std::locale::collate;
    const std::locale::category ctype   = std::locale::ctype;

    const std::locale or_name (classic, dename, collate | ctype);
    const std::locale or_loc (classic, de, collate | ctype);

    test_combined (__LINE__, or_name,
                   "std::locale (classic, \"de_DE.UTF-8\", collate | ctype)",
                   both, true, true);
    test_combined (__LINE__, or_loc,
                   "std::locale (classic, de, collate | ctype)",
                   both, true, true);

    const std::locale and_name (classic, dename, collate & ctype);
    const std::locale and_loc (classic, de, collate & ctype);

    test_combined (__LINE__, and_name,
                   "std::locale (classic, \"de_DE.UTF-8\", collate & ctype)",
                   "C", false, false);
    test_combined (__LINE__, and_loc,
                   "std::locale (classic, de, collate & ctype)",
                   "C", false, false);

    const std::locale lc_name (classic, dename, LC_COLLATE);
    const std::locale lc_loc (classic, de, LC_COLLATE);

    test_combined (__LINE__, lc_name,
                   "std::locale (classic, \"de_DE.UTF-8\", LC_COLLATE)",
                   coll, true, false);
    test_combined (__LINE__, lc_loc,
                   "std::locale (classic, de, LC_COLLATE)",
                   coll, true, false);
}

/**************************************************************************/

static int
run_test (int, char**)
{
    test_good ();
    test_native ();
    test_bad ();
    test_categories ();

    return 0;
}

/**************************************************************************/

int main (int argc, char *argv[])
{
    return rw_test (argc, argv, __FILE__,
                    "lib.locale.cons",
                    "the constructors",
                    run_test,
                    0 /* no command line options */);
}
