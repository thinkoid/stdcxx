/***************************************************************************
 *
 * 20.allocator.members.cpp - test exercising allocate and max_size of
 *                              [lib.allocator.members]
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

#include <memory>       // for allocator
#include <new>          // for bad_alloc

#include <cstddef>      // for size_t
#include <cstring>      // for memset

#include <rw_driver.h>  // for rw_test(), rw_assert()

/**************************************************************************/

static const std::size_t size_max = _RWSTD_SIZE_MAX;

struct Odd { char data_ [3]; };

struct Big { char data_ [1 << 20]; };

/**************************************************************************/

// true when each of the first nbytes bytes at ptr has the value
static bool
is_filled (const void *ptr, std::size_t nbytes, char value)
{
    const char* const bytes = static_cast<const char*>(ptr);

    for (std::size_t i = 0; i != nbytes; ++i) {
        if (bytes [i] != value)
            return false;
    }

    return true;
}


// allocates two blocks of n elements and writes all of each: returns
// true when they are two blocks and neither write reaches the other
template <class T>
bool
allocate_storage (std::size_t n, const T*)
{
    std::allocator<T> alloc;

    T* const first  = alloc.allocate (n);
    T* const second = alloc.allocate (n);

    const std::size_t nbytes = n * sizeof (T);

    std::memset (first, 0x55, nbytes);
    std::memset (second, 0x2a, nbytes);

    const bool success =
           first && second && first != second
        && is_filled (first, nbytes, 0x55)
        && is_filled (second, nbytes, 0x2a);

    alloc.deallocate (second, n);
    alloc.deallocate (first, n);

    return success;
}


// returns 1 when allocate (n) throws bad_alloc, 0 when it returns
// and -1 when it throws anything else
template <class T>
int
allocate_throws (std::size_t n, const T*)
{
    std::allocator<T> alloc;

    try {
        T* const ptr = alloc.allocate (n);

        alloc.deallocate (ptr, n);
    }
    catch (const std::bad_alloc&) {
        return 1;
    }
    catch (...) {
        return -1;
    }

    return 0;
}

/**************************************************************************/

template <class T>
void
test_allocate (const T *dummy, const char *tname)
{
    rw_info (0, 0, __LINE__, "std::allocator<%s>, a type of %zu bytes",
             tname, sizeof (T));

    const std::allocator<T> alloc;

    const std::size_t max = alloc.max_size ();

    // 20.4.1.1, p11: no count past max_size () can succeed, so the
    // bytes of max_size () elements are a size_t
    rw_assert (1 <= max && max <= size_max / sizeof (T), 0, __LINE__,
               "allocator<%s>::max_size () == %zu, expected a value "
               "from 1 to %zu",
               tname, max, size_max / sizeof (T));

    // 20.4.1.1, p5: storage for n objects of type T
    static const std::size_t counts [] = { 1, 2, 17, 1000 };

    for (unsigned i = 0; i != sizeof counts / sizeof *counts; ++i) {
        const std::size_t n = counts [i];

        // 16MB a block is enough
        if ((std::size_t (1) << 24) / sizeof (T) < n)
            continue;

        rw_assert (allocate_storage (n, dummy), 0, __LINE__,
                   "allocator<%s>::allocate (%zu) returns storage of "
                   "%zu bytes, twice",
                   tname, n, n * sizeof (T));
    }

    // 20.4.1.1, p7: bad_alloc when the storage cannot be obtained;
    // the bytes of max_size () elements are more than any block has
    int result = allocate_throws (max, dummy);

    rw_assert (1 == result, 0, __LINE__,
               "allocator<%s>::allocate (max_size ()) throws bad_alloc, "
               "%{?}returned%{:}threw another exception%{;}",
               tname, 0 == result);

    if (1 == sizeof (T))
        return;

    // the bytes of each of these counts are no size_t: the product
    // wraps to the size of a block that can be obtained
    const std::size_t past [] = { max + 1, size_max / 2 + 1, size_max };

    for (unsigned i = 0; i != sizeof past / sizeof *past; ++i) {
        result = allocate_throws (past [i], dummy);

        rw_assert (1 == result, 0, __LINE__,
                   "allocator<%s>::allocate (%zu), a count past "
                   "max_size (), throws bad_alloc, "
                   "%{?}returned%{:}threw another exception%{;}",
                   tname, past [i], 0 == result);
    }
}

/**************************************************************************/

static int
run_test (int, char**)
{
    test_allocate ((char*)0, "char");
    test_allocate ((int*)0, "int");
    test_allocate ((double*)0, "double");
    test_allocate ((Odd*)0, "Odd");
    test_allocate ((Big*)0, "Big");

    return 0;
}

/**************************************************************************/

int main (int argc, char** argv)
{
    return rw_test (argc, argv, __FILE__,
                    "lib.allocator.members",
                    0 /* no comment */,
                    run_test,
                    0 /* no command line options */);
}
