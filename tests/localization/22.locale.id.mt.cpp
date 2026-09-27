/************************************************************************
 *
 * 22.locale.id.mt.cpp
 *
 * test exercising the thread safety of the first use of a facet id
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

#include <locale>     // for locale, has_facet

#include <cstring>    // for memcpy()

#include <rw_thread.h>
#include <rw_driver.h>


// maximum number of threads allowed by the command line interface
#define MAX_THREADS      32

// the number of threads racing each round (at most 8 by default,
// fewer on machines with fewer processors)
int opt_nthreads = 1;

// default timeout used by each threaded section of this test
int opt_timeout = 60;

/**************************************************************************/

// a user-defined facet type per round: the id of a type is
// initialized once per process, so each round needs a new one
template <int N>
struct Facet: std::locale::facet
{
    static std::locale::id id;
};

template <int N>
std::locale::id Facet<N>::id;


// installs a facet of the type in a new locale and asks for it back
template <int N>
static bool
install_and_find ()
{
    const std::locale loc (std::locale::classic (), new Facet<N>);

    return std::has_facet<Facet<N> >(loc);
}


// the numeric value of the type's id, its only member
template <int N>
static std::size_t
id_value ()
{
    std::size_t value;
    std::memcpy (&value, &Facet<N>::id, sizeof value);
    return value;
}


typedef bool        InstallFunc ();
typedef std::size_t IdFunc ();

// the number of rounds, one facet type each
#define NROUNDS   500

static InstallFunc* install_funcs [NROUNDS];
static IdFunc*      id_funcs [NROUNDS];

template <int N>
struct FuncTable
{
    static void fill () {
        install_funcs [N] = &install_and_find<N>;
        id_funcs [N]      = &id_value<N>;
        FuncTable<N - 1>::fill ();
    }
};

template <>
struct FuncTable<-1>
{
    static void fill () { }
};

/**************************************************************************/

// the round being raced
static int race_round;

// the number of threads that have started in the current round
static volatile int nstarted;

// the number of installed facets has_facet did not find
static int nmissed;

extern "C" {

static void*
race_func (void*)
{
    // cast nstarted to int& (see STDCXX-792)
    _RWSTD_ATOMIC_PREINCREMENT (_RWSTD_CONST_CAST (int&, nstarted), false);

    // spin until all threads have been created so that they find
    // the id uninitialized at the same time
    while (nstarted < opt_nthreads);

    if (!install_funcs [race_round] ())
        _RWSTD_ATOMIC_PREINCREMENT (nmissed, false);

    return 0;
}

}   // extern "C"

/**************************************************************************/

static int
run_test (int, char**)
{
    if (sizeof (std::locale::id) != sizeof (std::size_t)) {
        rw_warn (false, 0, __LINE__,
                 "sizeof (std::locale::id) == %zu, expected %zu; skipping",
                 sizeof (std::locale::id), sizeof (std::size_t));
        return 0;
    }

    FuncTable<NROUNDS - 1>::fill ();

    rw_info (0, 0, 0,
             "racing the first use of a facet id from %d thread%{?}s%{;}, "
             "%d facet types",
             opt_nthreads, 1 != opt_nthreads, NROUNDS);

    int result = 0;

    for (int i = 0; 0 == result && i != NROUNDS; ++i) {

        race_round = i;
        nstarted   = 0;
        nmissed    = 0;

        result = rw_thread_pool (0, std::size_t (opt_nthreads), 0,
                                 race_func, 0,
                                 std::size_t (opt_timeout));

        rw_error (result == 0, 0, __LINE__,
                  "rw_thread_pool(0, %d, 0, %{#f}, 0) failed",
                  opt_nthreads, race_func);

        rw_assert (0 == nmissed, 0, __LINE__,
                   "has_facet<Facet<%d> >(locale (classic (), new "
                   "Facet<%d>)) == false in %d thread%{?}s%{;} of %d",
                   i, i, nmissed, 1 != nmissed, opt_nthreads);

        // one id per type: the types used in order take consecutive
        // ids, and an id generated and then overwritten by another
        // thread's shows as a gap
        if (0 != i) {
            const std::size_t prev = id_funcs [i - 1] ();
            const std::size_t curr = id_funcs [i] ();

            if (!rw_assert (prev + 1 == curr, 0, __LINE__,
                            "Facet<%d>::id == %zu, expected %zu: %zu ids "
                            "generated for one facet type by %d threads",
                            i, curr, prev + 1, curr - prev, opt_nthreads))
                break;
        }
    }

    return result;
}

/**************************************************************************/

int main (int argc, char *argv[])
{
#ifdef _RWSTD_REENTRANT

    // race the greater of 2 and the number of processors, at most 8:
    // contention comes from threads released together, not from more
    // of them
    opt_nthreads = rw_get_cpus ();
    if (opt_nthreads < 2)
        opt_nthreads = 2;
    else if (8 < opt_nthreads)
        opt_nthreads = 8;

#endif   // _RWSTD_REENTRANT

    return rw_test (argc, argv, __FILE__,
                    "lib.locale.id",
                    "thread safety", run_test,
                    "|-soft-timeout#0 "  // must be non-negative
                    "|-nthreads#0-* ",   // must be in [0, MAX_THREADS]
                    &opt_timeout,
                    int (MAX_THREADS),
                    &opt_nthreads);
}
