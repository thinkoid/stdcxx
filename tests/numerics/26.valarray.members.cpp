/***************************************************************************
 *
 * 26.valarray.members.cpp - test exercising [lib.valarray.members]
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

#include <valarray>      // for valarray
#include <cstddef>       // for size_t

#include <rw_driver.h>   // for rw_test(), rw_assert()

/**************************************************************************/

// has operator+= and no operator+, and a default value
// that is not the identity of its addition
struct Addend
{
    int val;

    Addend (): val (1) { }

    Addend (int v): val (v) { }

    Addend& operator+= (const Addend &rhs) {
        val += rhs.val;
        return *this;
    }
};

/**************************************************************************/

// 26.3.2.7, p2
static void
test_sum ()
{
    rw_info (0, 0, 0, "std::valarray<T>::sum ()");

    const int ints [] = { 1, 2, 3, 4 };

    const std::valarray<int> four (ints, 4);

    rw_assert (10 == four.sum (), 0, __LINE__,
               "valarray<int>::sum () of 1, 2, 3, 4: expected 10, got %d",
               four.sum ());

    const std::valarray<int> one (ints + 2, 1);

    rw_assert (3 == one.sum (), 0, __LINE__,
               "valarray<int>::sum () of 3: expected 3, got %d",
               one.sum ());

    // the sum starts from a copy of an element, not from T ()
    const Addend addends [] = { Addend (2), Addend (3) };

    const std::valarray<Addend> two (addends, 2);

    rw_assert (5 == two.sum ().val, 0, __LINE__,
               "valarray<Addend>::sum () of 2, 3: expected 5, got %d",
               two.sum ().val);

    // of one element, the sum is the element: its sign is kept
    const double zeros [] = { -0.0 };

    const std::valarray<double> minus (zeros, 1);

    rw_assert (1.0 / minus.sum () < 0.0, 0, __LINE__,
               "valarray<double>::sum () of -0.0: expected -0.0, got %g",
               minus.sum ());
}

/**************************************************************************/

static int
run_test (int, char**)
{
    test_sum ();

    return 0;
}

/**************************************************************************/

int main (int argc, char** argv)
{
    return rw_test (argc, argv, __FILE__,
                    "lib.valarray.members",
                    0 /* no comment */,
                    run_test,
                    0 /* no command line options */);
}
