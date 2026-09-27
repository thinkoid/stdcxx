/************************************************************************
 *
 * 22.locale.time.put.libc.mt.cpp
 *
 * test exercising the thread safety of the first time_put call on
 * a facet whose data the library builds from the C library
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

// default number of threads (will be adjusted to the number
// of processors/cores later)
int opt_nthreads = 1;

// the number of locales the test constructs, one per round
int opt_nrounds = 200;

// default timeout used by each threaded section of this test
int opt_timeout = 60;

/**************************************************************************/

struct MyIos: std::basic_ios<char>
{
    MyIos () {
        this->init (0);
    }
};


struct MyStreambuf: std::basic_streambuf<char>
{
    void pubsetp (char *pbeg, std::streamsize n) {
        this->setp (pbeg, pbeg + n);
    }
};


// per-thread formatting state, constructed by main before the first
// checkpoint so that the threads allocate nothing of their own
static struct {
    MyIos       io;
    MyStreambuf sb;
    char        buf [64];
} thread_data [MAX_THREADS];


// the facet whose data the threads race to build
static const std::time_put<char>* race_facet;

// the time the threads format
static std::tm race_time;

// the number of threads that have started in the current round
static volatile int nstarted;

extern "C" {

// the replacement operator new of rw_new.h keeps no lock: the only
// allocation the threads make is the facet data's, under the lock
// of the library's __rw_setlocale
static void*
race_func (void *arg)
{
    const long inx = long (arg);

    // cast nstarted to int& (see STDCXX-792)
    _RWSTD_ATOMIC_PREINCREMENT (_RWSTD_CONST_CAST (int&, nstarted), false);

    // spin until all threads have been created so that they find
    // the facet data missing at the same time
    while (nstarted < opt_nthreads);

    MyStreambuf& sb = thread_data [inx].sb;
    sb.pubsetp (thread_data [inx].buf, sizeof thread_data [inx].buf);

    // %A formats a weekday name, which only the facet data has
    race_facet->put (std::ostreambuf_iterator<char>(&sb),
                     thread_data [inx].io, ' ', &race_time, 'A');

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
             "racing the first std::time_put<char>::put() in %#s "
             "from %d thread%{?}s%{;}, %d locale%{?}s%{;}",
             namebuf, opt_nthreads, 1 != opt_nthreads,
             opt_nrounds, 1 != opt_nrounds);

    race_time.tm_wday = 3;

    // a first round in this thread alone, for the allocations
    // the library makes once per process
    {
        const std::locale loc (namebuf);

        race_facet = &std::use_facet<std::time_put<char> >(loc);
        nstarted   = opt_nthreads;
        race_func (0);
    }

    int result = 0;

    for (int i = 0; 0 == result && i != opt_nrounds; ++i) {

        rwt_check_leaks (0, 0);

        {
            // each round a new locale, whose facet has no data yet
            const std::locale loc (namebuf);

            race_facet = &std::use_facet<std::time_put<char> >(loc);
            nstarted   = 0;

            void* args [MAX_THREADS];
            for (int j = 0; j != opt_nthreads; ++j)
                args [j] = (void*)long (j);

            result = rw_thread_pool (0, std::size_t (opt_nthreads), 0,
                                     race_func, args,
                                     std::size_t (opt_timeout));

            rw_error (result == 0, 0, __LINE__,
                      "rw_thread_pool(0, %d, 0, %{#f}, ...) failed",
                      opt_nthreads, race_func);

            race_facet = 0;
        }

        // the facet and its data die with the only locale that holds
        // it: no thread built data that was then overwritten
        std::size_t nbytes;
        const std::size_t nblocks = rwt_check_leaks (&nbytes, 0);

        if (!rw_assert (0 == nblocks, 0, __LINE__,
                        "std::time_put<char>::put() in std::locale (%#s) "
                        "from %d threads: %zu blocks (%zu bytes) outlive "
                        "the locale in round %d",
                        namebuf, opt_nthreads, nblocks, nbytes, i))
            break;
    }

    return result;
}

/**************************************************************************/

int main (int argc, char *argv[])
{
#ifdef _RWSTD_REENTRANT

    // set nthreads to the greater of the number of processors
    // and 2 (for uniprocessor systems) by default
    opt_nthreads = rw_get_cpus ();
    if (opt_nthreads < 2)
        opt_nthreads = 2;

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
