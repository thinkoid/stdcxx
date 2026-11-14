/***************************************************************************
 *
 * 23.vector.bool.cpp - test exercising [lib.vector.bool]
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

#include <vector>           // for vector
#include <cstddef>          // for size_t
#include <cstring>          // for strlen

#include <rw_alg_test.h>    // for InputIter
#include <rw_allocator.h>   // for SharedAlloc, UserAlloc
#include <rw_driver.h>      // for rw_test(), rw_assert()

/**************************************************************************/

typedef std::vector<bool> Vector;

// more elements than one word of the vector holds
static const char bits [] = "1011001110001111000011111000001111110000";

/**************************************************************************/

// stores in buf the elements a string of '0' and '1' stands for,
// and returns their number
static std::size_t
to_bools (bool *buf, const char *str)
{
    std::size_t n = 0;

    for ( ; str [n]; ++n)
        buf [n] = '1' == str [n];

    return n;
}


// the size of vec and its elements in order are those of str,
// a string of '0' and '1'
template <class Allocator>
void
check_vector (int line, const char *what,
              const std::vector<bool, Allocator> &vec, const char *str)
{
    const std::size_t len = std::strlen (str);

    bool same = vec.size () == len;

    for (std::size_t i = 0; same && i != len; ++i)
        same = vec [i] == ('1' == str [i]);

    rw_assert (same, 0, line,
               "%s: expected \"%s\" of size %zu, got size %zu",
               what, str, len, std::size_t (vec.size ()));
}

/**************************************************************************/

// 23.1.1, p9: the range constructor on an integral type
// is the constructor of a count and a value
static void
test_ctor_integers ()
{
    rw_info (0, 0, 0, "std::vector<bool>::vector (InputIterator, "
             "InputIterator, const allocator_type&) on an integral type");

    const Vector ones (5, 1);
    check_vector (__LINE__, "vector (5, 1)", ones, "11111");

    const Vector zeros (3, 0);
    check_vector (__LINE__, "vector (3, 0)", zeros, "000");

    // the value converts as static_cast<bool> does
    const Vector twos (3, 2);
    check_vector (__LINE__, "vector (3, 2)", twos, "111");

    const Vector longs (4L, 1L);
    check_vector (__LINE__, "vector (4L, 1L)", longs, "1111");

    const Vector none (0, 1);
    check_vector (__LINE__, "vector (0, 1)", none, "");

    const Vector with_alloc (2, 1, std::allocator<bool> ());
    check_vector (__LINE__, "vector (2, 1, alloc)", with_alloc, "11");
}


static void
test_ctor_range ()
{
    rw_info (0, 0, 0, "std::vector<bool>::vector (InputIterator, "
             "InputIterator, const allocator_type&)");

    bool buf [sizeof bits];

    const std::size_t n = to_bools (buf, bits);

    const Vector vec (buf, buf + n);
    check_vector (__LINE__, "vector (first, last)", vec, bits);

    const Vector empty (buf, buf);
    check_vector (__LINE__, "vector (first, first)", empty, "");

    typedef UserAlloc<bool>          Alloc;
    typedef std::vector<bool, Alloc> UserVector;

    // a pool of its own, so that alloc compares unequal
    // to an allocator that is default-constructed
    SharedAlloc pool;
    const Alloc alloc (&pool);

    const UserVector user (buf, buf + n, alloc);
    check_vector (__LINE__, "vector (first, last, alloc)", user, bits);

    rw_assert (user.get_allocator () == alloc, 0, __LINE__,
               "vector (first, last, alloc): get_allocator () "
               "compares unequal to alloc");
}


static void
test_assign ()
{
    rw_info (0, 0, 0, "std::vector<bool>::assign "
             "(InputIterator, InputIterator)");

    bool buf [sizeof bits];

    const std::size_t n = to_bools (buf, bits);

    Vector vec (2, false);

    vec.assign (buf, buf + n);
    check_vector (__LINE__, "assign (first, last)", vec, bits);

    // 23.1.1, p9: on an integral type, a count and a value
    vec.assign (5, 1);
    check_vector (__LINE__, "assign (5, 1)", vec, "11111");

    vec.assign (3, 0);
    check_vector (__LINE__, "assign (3, 0)", vec, "000");
}


static void
test_insert ()
{
    rw_info (0, 0, 0, "std::vector<bool>::insert "
             "(iterator, InputIterator, InputIterator)");

    bool buf [sizeof bits];

    to_bools (buf, bits);

    Vector vec (4, false);

    vec.insert (vec.begin () + 1, buf, buf + 4);
    check_vector (__LINE__, "insert (it, first, last)", vec, "01011000");

    // 23.1.1, p9: on an integral type, a count and a value
    vec.insert (vec.begin () + 2, 3, 1);
    check_vector (__LINE__, "insert (it, 3, 1)", vec, "01111011000");

    vec.insert (vec.end (), 2, 0);
    check_vector (__LINE__, "insert (end (), 2, 0)", vec, "0111101100000");
}


// 24.1.1, p3: a range of input iterators is read once
static void
test_input_range ()
{
    rw_info (0, 0, 0, "std::vector<bool> constructor, assign and insert "
             "on a range of input iterators");

    typedef InputIter<bool> Iter;

    bool buf [sizeof bits];

    const std::size_t n   = to_bools (buf, bits);
    const bool* const end = buf + n;

    const Vector ctor (Iter (buf, buf, end), Iter (end, end, end));
    check_vector (__LINE__, "vector (first, last)", ctor, bits);

    Vector vec (2, false);

    vec.assign (Iter (buf, buf, end), Iter (end, end, end));
    check_vector (__LINE__, "assign (first, last)", vec, bits);

    // more elements than the capacity: the storage is reallocated
    Vector grown;

    grown.insert (grown.end (), Iter (buf, buf, end), Iter (end, end, end));
    check_vector (__LINE__, "insert (it, first, last), reallocating",
                  grown, bits);

    // fewer than the capacity left: the elements are inserted in place
    Vector fits (4, false);

    const bool* const mid = buf + 4;

    fits.insert (fits.begin () + 1, Iter (buf, buf, mid), Iter (mid, mid, mid));
    check_vector (__LINE__, "insert (it, first, last), in place",
                  fits, "01011000");
}

/**************************************************************************/

static int
run_test (int, char**)
{
    test_ctor_integers ();
    test_ctor_range ();
    test_assign ();
    test_insert ();
    test_input_range ();

    return 0;
}

/**************************************************************************/

int main (int argc, char** argv)
{
    return rw_test (argc, argv, __FILE__,
                    "lib.vector.bool",
                    0 /* no comment */,
                    run_test,
                    0 /* no command line options */);
}
