/************************************************************************
 *
 * rand.cpp - definitions of testsuite random number generators
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
 * Copyright 2005-2006 Rogue Wave Software.
 * 
 **************************************************************************/

#include <time.h>   // for clock()

#include <rw_rand.h>


enum { TABLE_SIZE = 55 };

static _RWSTD_UINT32_T table32 [TABLE_SIZE];
static _RWSTD_SIZE_T   inx32 [2];


// seeds the 32-bit random number generator
void
rw_seed32 (_RWSTD_UINT32_T seed)
{
    if (0xffffffffUL == seed)
        seed = unsigned (clock ());

    table32 [TABLE_SIZE - 1] = seed;

    _RWSTD_UINT32_T k = 1;

    // 21 * i % 55 is another of 1 to 54 for each i: every slot
    // below the seed's is written once
    for (_RWSTD_SIZE_T i = 1; i != TABLE_SIZE; ++i) {
        const _RWSTD_SIZE_T ii = 21U * i % TABLE_SIZE - 1U;
        table32 [ii] =  k;
        k            = seed - k;
        seed         = table32 [ii];
    }

    // four passes over the table before the first draw
    for (_RWSTD_SIZE_T i = 0; i != 4U; ++i) {
        for (_RWSTD_SIZE_T ii = 0; ii != TABLE_SIZE; ++ii)
            table32 [ii] -= table32 [(ii + 31U) % TABLE_SIZE];
    }

    // the second index runs 31 slots ahead of the first, at
    // the value drawn 24 draws before
    inx32 [0] = 0U;
    inx32 [1] = 31U;
}


// returns a 32-bit random number in the range [0, limit)
// if limit is non-zero, otherwise in the range [0, UINT32_MAX]
_RWSTD_UINT32_T
rw_rand32 (_RWSTD_UINT32_T limit /* = 0 */)
{
    if (0 == inx32 [0] && 0 == inx32 [1])
        rw_seed32 (161803398);

    table32 [++inx32 [0] %= TABLE_SIZE] -=
        table32 [++inx32 [1] %= TABLE_SIZE];

    return limit ? table32 [inx32 [0]] % limit : table32 [inx32 [0]];
}


#ifdef _RWSTD_UINT64_T

static _RWSTD_UINT64_T table64 [TABLE_SIZE];
static _RWSTD_SIZE_T   inx64 [2];


// seeds the 64-bit random number generator
void
rw_seed64 (_RWSTD_UINT64_T seed)
{
    if ((_RWSTD_UINT64_T)-1 == seed)
        seed = clock ();

    table64 [TABLE_SIZE - 1] = seed;

    _RWSTD_UINT64_T k = 1;

    // 21 * i % 55 is another of 1 to 54 for each i: every slot
    // below the seed's is written once
    for (_RWSTD_SIZE_T i = 1; i != TABLE_SIZE; ++i) {
        const _RWSTD_SIZE_T ii = 21U * i % TABLE_SIZE - 1U;
        table64 [ii] =  k;
        k            = seed - k;
        seed         = table64 [ii];
    }

    // four passes over the table before the first draw
    for (_RWSTD_SIZE_T i = 0; i != 4U; ++i) {
        for (_RWSTD_SIZE_T ii = 0; ii != TABLE_SIZE; ++ii)
            table64 [ii] -= table64 [(ii + 31U) % TABLE_SIZE];
    }

    // the second index runs 31 slots ahead of the first, at
    // the value drawn 24 draws before
    inx64 [0] = 0U;
    inx64 [1] = 31U;
}


// returns a 64-bit random number in the range [0, limit)
// if limit is non-zero, otherwise in the range [0, UINT64_MAX]
_RWSTD_UINT64_T
rw_rand64 (_RWSTD_UINT64_T limit)
{
    if (0 == inx64 [0] && 0 == inx64 [1])
        rw_seed64 (161803398);

    table64 [++inx64 [0] %= TABLE_SIZE] -=
        table64 [++inx64 [1] %= TABLE_SIZE];

    return limit ? table64 [inx64 [0]] % limit : table64 [inx64 [0]];
}

#endif   // _RWSTD_UINT64_T
