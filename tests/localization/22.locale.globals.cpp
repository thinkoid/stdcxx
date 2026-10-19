/************************************************************************
 *
 * 22.locale.globals.cpp
 *
 * test exercising lib.locale.global.templates: has_facet and use_facet
 * for every standard facet, in named locales and in the classic one
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

#include <locale>     // for locale, has_facet, use_facet, the facets
#include <typeinfo>   // for bad_cast
#include <cwchar>     // for mbstate_t
#include <cstring>    // for strcat(), strcpy(), strlen()

#include <rw_locale.h>   // for rw_locales()
#include <rw_driver.h>   // for rw_test()


// disable the checks that use_facet throws
int opt_no_exceptions;

/**************************************************************************/

// a standard facet without a _byname form: every locale has it
template <class Facet>
static void
test_base (const std::locale &loc, const char *locname,
           const char *fname, const char *targs)
{
    rw_assert (std::has_facet<Facet>(loc), 0, __LINE__,
               "std::has_facet<std::%s<%s> >(std::locale (%#s))",
               fname, targs, locname);

    const Facet* const pf = &std::use_facet<Facet>(loc);

    rw_assert (0 != pf, 0, __LINE__,
               "std::use_facet<std::%s<%s> >(std::locale (%#s))",
               fname, targs, locname);
}


// a standard facet with a _byname form: a named locale has the _byname
// facet, and it is the locale's base facet; the classic locale has it
// not, and use_facet throws bad_cast
template <class Base, class Byname>
static void
test_byname (const std::locale &loc, const char *locname, bool byname,
             const char *fname, const char *targs)
{
    test_base<Base>(loc, locname, fname, targs);

    rw_assert (byname == std::has_facet<Byname>(loc), 0, __LINE__,
               "std::has_facet<std::%s_byname<%s> >(std::locale (%#s)) "
               "== %d", fname, targs, locname, int (byname));

    if (byname) {
        const Base* const pb = &std::use_facet<Base>(loc);
        const Base* const pd = &std::use_facet<Byname>(loc);

        rw_assert (pb == pd, 0, __LINE__,
                   "std::use_facet<std::%s_byname<%s> >(std::locale (%#s)) "
                   "is the locale's std::%s<%s>",
                   fname, targs, locname, fname, targs);
    }
    else if (0 == opt_no_exceptions) {

        const char* threw = "nothing";

        try {
            std::use_facet<Byname>(loc);
        }
        catch (const std::bad_cast&) {
            threw = "std::bad_cast";
        }
        catch (...) {
            threw = "an unknown exception";
        }

        rw_assert (0 == std::strcmp ("std::bad_cast", threw), 0, __LINE__,
                   "std::use_facet<std::%s_byname<%s> >(std::locale (%#s)) "
                   "threw %s, expected std::bad_cast",
                   fname, targs, locname, threw);
    }
}


// composes the template arguments for a message: cname and the rest
static const char*
targs (char *buf, const char *cname, const char *rest)
{
    std::strcpy (buf, cname);
    std::strcat (buf, rest);
    return buf;
}


template <class charT>
static void
test_locale (const std::locale &loc, const char *locname, bool byname,
             const char *cname)
{
    typedef std::codecvt<charT, char, std::mbstate_t>        Codecvt;
    typedef std::codecvt_byname<charT, char, std::mbstate_t> CodecvtByname;
    typedef std::collate<charT>                              Collate;
    typedef std::collate_byname<charT>                       CollateByname;
    typedef std::ctype<charT>                                Ctype;
    typedef std::ctype_byname<charT>                         CtypeByname;
    typedef std::messages<charT>                             Messages;
    typedef std::messages_byname<charT>                      MessagesByname;
    typedef std::moneypunct<charT, false>                    Moneypunct0;
    typedef std::moneypunct_byname<charT, false>             Moneypunct0Byname;
    typedef std::moneypunct<charT, true>                     Moneypunct1;
    typedef std::moneypunct_byname<charT, true>              Moneypunct1Byname;
    typedef std::numpunct<charT>                             Numpunct;
    typedef std::numpunct_byname<charT>                      NumpunctByname;
    typedef std::time_get<charT>                             TimeGet;
    typedef std::time_get_byname<charT>                      TimeGetByname;
    typedef std::time_put<charT>                             TimePut;
    typedef std::time_put_byname<charT>                      TimePutByname;
    typedef std::money_get<charT>                            MoneyGet;
    typedef std::money_put<charT>                            MoneyPut;
    typedef std::num_get<charT>                              NumGet;
    typedef std::num_put<charT>                              NumPut;

    char buf [64];

    test_byname<Codecvt, CodecvtByname>
        (loc, locname, byname, "codecvt",
         targs (buf, cname, ", char, std::mbstate_t"));
    test_byname<Collate, CollateByname>
        (loc, locname, byname, "collate", cname);
    test_byname<Ctype, CtypeByname>
        (loc, locname, byname, "ctype", cname);
    test_byname<Messages, MessagesByname>
        (loc, locname, byname, "messages", cname);
    test_byname<Moneypunct0, Moneypunct0Byname>
        (loc, locname, byname, "moneypunct", targs (buf, cname, ", false"));
    test_byname<Moneypunct1, Moneypunct1Byname>
        (loc, locname, byname, "moneypunct", targs (buf, cname, ", true"));
    test_byname<Numpunct, NumpunctByname>
        (loc, locname, byname, "numpunct", cname);
    test_byname<TimeGet, TimeGetByname>
        (loc, locname, byname, "time_get", cname);
    test_byname<TimePut, TimePutByname>
        (loc, locname, byname, "time_put", cname);

    test_base<MoneyGet>(loc, locname, "money_get", cname);
    test_base<MoneyPut>(loc, locname, "money_put", cname);
    test_base<NumGet>(loc, locname, "num_get", cname);
    test_base<NumPut>(loc, locname, "num_put", cname);
}

/**************************************************************************/

static int
run_test (int, char**)
{
    const std::locale classic (std::locale::classic ());

    rw_info (0, 0, 0, "the classic locale");

    test_locale<char>(classic, "C", false, "char");
    test_locale<wchar_t>(classic, "C", false, "wchar_t");

    // the installed locales, or those named on the command line
    const char* const locale_list =
        rw_opt_locales ? rw_opt_locales : rw_locales (_RWSTD_LC_ALL);

    std::size_t nlocales = 0;

    for (const char *name = locale_list;
         *name;
         name += std::strlen (name) + 1) {

        try {
            const std::locale loc (name);

            // "C" and "POSIX" construct the classic locale
            test_locale<char>(loc, name, loc != classic, "char");
            test_locale<wchar_t>(loc, name, loc != classic, "wchar_t");

            ++nlocales;
        }
        catch (...) {
            rw_warn (0, 0, __LINE__,
                     "failed to construct std::locale (%#s)", name);
        }
    }

    rw_info (0, 0, 0, "%zu named locale%{?}s%{;}", nlocales, 1 != nlocales);

    return 0;
}

/**************************************************************************/

int main (int argc, char *argv[])
{
    return rw_test (argc, argv, __FILE__,
                    "lib.locale.global.templates",
                    "has_facet and use_facet",   // comment
                    run_test,
                    "|-locales= "        // must be provided
                    "|-no-exceptions# ",
                    &rw_opt_setlocales,
                    &opt_no_exceptions);
}
