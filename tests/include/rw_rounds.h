/************************************************************************
 *
 * rw_rounds.h - declarations of the first-use rounds driver
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
 **************************************************************************/

#ifndef RW_ROUNDS_H_INCLUDED
#define RW_ROUNDS_H_INCLUDED

#include <rw_testdefs.h>

#include <locale>   // for locale


// The driver of the locale MT tests that provoke contention instead
// of waiting for it (the TODO entry of that name). A small fixed pool
// of threads uses the facets of a fresh locale, round after round, so
// that every round finds the facets' lazy state empty. The locales are
// built from the tree's sources under names the C library does not
// know: a facet whose data is missing cannot hide behind the C
// library's answer. 22.locale.numpunct.mt is the model.

enum {
    RW_ROUND_MAX_THREADS = 32,   // the upper bound of --nthreads
    RW_ROUND_MAX_LOCALES =  8    // the most locale sources a test names
};

// --nthreads, --nrounds and --soft-timeout; rw_round_test() parses them
_TEST_EXPORT extern int rw_round_nthreads;   // 4 threads, whatever the processor count
_TEST_EXPORT extern int rw_round_nrounds;    // 64 fresh locales
_TEST_EXPORT extern int rw_round_timeout;    // 5 seconds per round


// called by the main thread with each round's locale before the threads
// start: fills the locale's facet slots, so that the threads race only
// the facets (the race to fill a slot is 22.locale.use_facet.mt's), and
// resets what the test keeps per round
typedef void rw_round_prepare_t (const std::locale&);

// the work of one thread: uses the facets of the locale and records
// what it saw in the block it is given, a block of the test's own; the
// main thread calls it once per locale into the expected block, from a
// locale that dies before the rounds start, and every thread of a
// round calls it into the block of the thread
typedef void rw_round_work_t (const std::locale&, void*);

// called by the main thread after the join, once per thread: compares
// the thread's block with the expected one through rw_assert() and
// returns the success of its assertions; the driver's diagnostics are
// not thread-safe, which is why the threads record and the main thread
// asserts
typedef bool rw_round_check_t (const char* /* locname */, int /* round */,
                               int /* thread */, const void* /* expected */,
                               const void* /* got */);


// in a gated round, holds the calling thread until every thread of the
// pool has arrived at the same gate; returns at once otherwise. A gate
// before each member call keeps the threads from drifting apart on the
// first call, which maps the facet's data under the facet's mutex and
// releases them one by one. A gate hides data published too early,
// which is what the ungated rounds are for.
_TEST_EXPORT void
rw_round_gate ();


// builds each locale of `sources' ({ name, charmap }) with rw_localedef()
// and takes its expected block by `work' from a locale that dies before
// the rounds; skips the test when a locale fails to build or when the C
// library knows its name. Then rw_round_nrounds rounds, each on a fresh
// locale of the next source: `prepare', rw_round_nthreads threads through
// `work' into their blocks, `check' per thread. The blocks are `size'
// bytes each, one per source at `expected' and one per thread at `slots'.
// With `alternate' the even rounds are gated, the odd ones are not;
// without it no round is. The first round that fails ends the test:
// later rounds would only repeat it. `what' names the facet in the
// diagnostics. Returns 0, or non-zero when a round's pool failed or
// outlasted the soft timeout. Does nothing in a non-thread-safe build.
_TEST_EXPORT int
rw_first_use_rounds (const char* const    /* sources */ [][2],
                     _RWSTD_SIZE_T        /* nsources */,
                     void*                /* expected */,
                     void*                /* slots */,
                     _RWSTD_SIZE_T        /* size */,
                     rw_round_prepare_t*  /* prepare */,
                     rw_round_work_t*     /* work */,
                     rw_round_check_t*    /* check */,
                     const char*          /* what */,
                     bool                 /* alternate */);


// rw_test() with the driver's command line options
_TEST_EXPORT int
rw_round_test (int           /* argc */,
               char*         /* argv */[],
               const char*   /* filename */,
               const char*   /* clause */,
               const char*   /* comment */,
               int         (*/* testfun */)(int, char**));

#endif   // RW_ROUNDS_H_INCLUDED
