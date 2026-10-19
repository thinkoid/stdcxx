/************************************************************************
 *
 * 22.locale.time.put.libc.mt.cpp
 *
 * test exercising the thread safety of the first time_put::put() call,
 * which builds the facet's data from the C library, in char and in
 * wchar_t
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

#include <ctime>      // for tm
#include <ios>        // for basic_ios
#include <iterator>   // for ostreambuf_iterator
#include <locale>     // for locale, time_put, use_facet
#include <streambuf>  // for basic_streambuf

#include <cstring>    // for strcmp(), strcpy(), strlen()

#include <rw_locale.h>    // for rw_locales ()
#include <rw_new.h>       // for rwt_check_leaks ()
#include <rw_thread.h>
#include <rw_driver.h>


// maximum number of threads allowed by the command line interface
#define MAX_THREADS      32

// the number of threads (4 in a thread-safe build, set in main ()
// whatever the number of processors)
int opt_nthreads = 1;

// the number of locales the test constructs, one per round
int opt_nrounds = 200;

// default timeout used by each threaded section of this test
int opt_timeout = 60;

/**************************************************************************/

template <class charT>
struct MyIos: std::basic_ios<charT>
{
    MyIos () {
        this->init (0);
    }
};


template <class charT>
struct MyStreambuf: std::basic_streambuf<charT>
{
    void pubsetp (charT *pbeg, std::streamsize n) {
        this->setp (pbeg, pbeg + n);
    }
};


// per-thread formatting state, both character types, constructed by
// main before the first checkpoint so that the threads allocate
// nothing of their own
static struct {
    MyIos<char>          io;
    MyStreambuf<char>    sb;
    char                 buf [64];
    MyIos<wchar_t>       wio;
    MyStreambuf<wchar_t> wsb;
    wchar_t              wbuf [64];
} thread_data [MAX_THREADS];


// the facets whose data the threads race to build, one per round
static const std::time_put<char>*    race_facet;
static const std::time_put<wchar_t>* race_wfacet;

// whether the round races the wide facet
static bool race_wide;

// the time the threads format
static std::tm race_time;

// the number of threads that have started in the current round
static int nstarted;

extern "C" {

// the replacement operator new of rw_new.h keeps no lock: the only
// allocation the threads make is the facet data's, under the lock
// of the library's __rw_setlocale
static void*
race_func (void *arg)
{
    const long inx = long (arg);

    // start with the other threads so that they find
    // the facet data missing at the same time
    rw_thread_gate (&nstarted, opt_nthreads);

    // %A formats a weekday name, which only the facet data has
    if (race_wide) {
        MyStreambuf<wchar_t>& sb = thread_data [inx].wsb;
        sb.pubsetp (thread_data [inx].wbuf,
                    sizeof thread_data [inx].wbuf / sizeof (wchar_t));

        race_wfacet->put (std::ostreambuf_iterator<wchar_t>(&sb),
                          thread_data [inx].wio, L' ', &race_time, 'A');
    }
    else {
        MyStreambuf<char>& sb = thread_data [inx].sb;
        sb.pubsetp (thread_data [inx].buf, sizeof thread_data [inx].buf);

        race_facet->put (std::ostreambuf_iterator<char>(&sb),
                         thread_data [inx].io, ' ', &race_time, 'A');
    }

    return 0;
}

}   // extern "C"

/**************************************************************************/

// returns the name of an installed locale other than C and POSIX,
// which the library's database does not know when no locale root
// is set, or 0 when there is none
static const char*
find_libc_locale ()
{
    const char* const locale_list =
        rw_opt_locales ? rw_opt_locales : rw_locales (_RWSTD_LC_ALL);

    for (const char* name = locale_list;
         *name;
         name += std::strlen (name) + 1) {

        if (std::strcmp (name, "C") && std::strcmp (name, "POSIX"))
            return name;
    }

    return 0;
}


static int
run_test (int, char**)
{
    const char* const locname = find_libc_locale ();

    if (0 == locname) {
        rw_warn (false, 0, __LINE__,
                 "no installed locale other than C and POSIX; skipping");
        return 0;
    }

    char namebuf [256];
    std::strcpy (namebuf, locname);

    rw_info (0, 0, 0,
             "racing the first std::time_put<char>::put() and "
             "std::time_put<wchar_t>::put(), in turn, in %#s "
             "from %d thread%{?}s%{;}, %d locale%{?}s%{;}",
             namebuf, opt_nthreads, 1 != opt_nthreads,
             opt_nrounds, 1 != opt_nrounds);

    race_time.tm_wday = 3;

    // a first round in this thread alone, both facets, for the
    // allocations the library makes once per process
    {
        const std::locale loc (namebuf);

        race_facet  = &std::use_facet<std::time_put<char> >(loc);
        race_wfacet = &std::use_facet<std::time_put<wchar_t> >(loc);
        nstarted    = opt_nthreads;

        race_wide = false;
        race_func (0);
        race_wide = true;
        race_func (0);
    }

    int result = 0;

    for (int i = 0; 0 == result && i != opt_nrounds; ++i) {

        rwt_check_leaks (0, 0);

        {
            // each round a new locale, whose facets have no data
            // yet, and the other character type
            const std::locale loc (namebuf);

            race_facet  = &std::use_facet<std::time_put<char> >(loc);
            race_wfacet = &std::use_facet<std::time_put<wchar_t> >(loc);
            race_wide   = 0 != i % 2;
            nstarted    = 0;

            void* args [MAX_THREADS];
            for (int j = 0; j != opt_nthreads; ++j)
                args [j] = (void*)long (j);

            result = rw_thread_pool (0, std::size_t (opt_nthreads), 0,
                                     race_func, args,
                                     std::size_t (opt_timeout));

            rw_error (result == 0, 0, __LINE__,
                      "rw_thread_pool(0, %d, 0, %{#f}, ...) failed",
                      opt_nthreads, race_func);

            race_facet  = 0;
            race_wfacet = 0;
        }

        // the facet and its data die with the only locale that holds
        // it: no thread built data that was then overwritten
        std::size_t nbytes;
        const std::size_t nblocks = rwt_check_leaks (&nbytes, 0);

        if (!rw_assert (0 == nblocks, 0, __LINE__,
                        "std::time_put<%s>::put() in std::locale (%#s) "
                        "from %d threads: %zu blocks (%zu bytes) outlive "
                        "the locale in round %d",
                        0 != i % 2 ? "wchar_t" : "char", namebuf,
                        opt_nthreads, nblocks, nbytes, i))
            break;
    }

    return result;
}

/**************************************************************************/

int main (int argc, char *argv[])
{
#ifdef _RWSTD_REENTRANT

    // four threads whatever the number of processors, the size
    // chosen for this program under ThreadSanitizer
    // (doc/notes/analysis-locale-mt-sizing.md)
    opt_nthreads = 4;

#endif   // _RWSTD_REENTRANT

    return rw_test (argc, argv, __FILE__,
                    "lib.locale.time.put",
                    "thread safety", run_test,
                    "|-soft-timeout#0 "  // must be non-negative
                    "|-nrounds#0 "       // must be non-negative
                    "|-nthreads#0-* "    // must be in [0, MAX_THREADS]
                    "|-locales=",        // must be provided
                    &opt_timeout,
                    &opt_nrounds,
                    int (MAX_THREADS),
                    &opt_nthreads,
                    &rw_opt_setlocales);
}
