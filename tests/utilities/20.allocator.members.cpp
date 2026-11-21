/***************************************************************************
 *
 * 20.allocator.members.cpp - test exercising the members of
 *                              [lib.allocator.members] and the
 *                              comparisons of [lib.allocator.globals]
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
#include <rw_new.h>     // for rwt_get_free_store()

/**************************************************************************/

static const std::size_t size_max = _RWSTD_SIZE_MAX;

struct Odd { char data_ [3]; };

struct Big { char data_ [1 << 20]; };

// counts its copies and its destructions, and records the address of
// the last of each
struct Traced
{
    static int         ncopies;
    static const void* copy_to;
    static const void* copy_from;

    static int         ndtors;
    static const void* dtor_at;

    int value_;

    explicit Traced (int value): value_ (value) { }

    Traced (const Traced &rhs): value_ (rhs.value_) {
        ++ncopies;
        copy_to   = this;
        copy_from = &rhs;
    }

    ~Traced () {
        ++ndtors;
        dtor_at = this;
    }
};

int         Traced::ncopies;
const void* Traced::copy_to;
const void* Traced::copy_from;

int         Traced::ndtors;
const void* Traced::dtor_at;


// value is 1 when T and U are the same type
template <class T, class U>
struct same_type { enum { value = 0 }; };

template <class T>
struct same_type<T, T> { enum { value = 1 }; };

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


// allocates two blocks of n elements, the second with the first as
// its hint when hint is true, and writes all of each: returns true
// when they are two blocks and neither write reaches the other
template <class T>
bool
allocate_storage (std::size_t n, bool hint, const T*)
{
    std::allocator<T> alloc;

    T* const first  = alloc.allocate (n);
    T* const second = alloc.allocate (n, hint ? first : 0);

    const std::size_t nbytes = n * sizeof (T);

    std::memset (first, 0x55, nbytes);
    std::memset (second, 0x2a, nbytes);

    const bool success =
           first && second && first != second
        && is_filled (first, nbytes, 0x55)
        && is_filled (second, nbytes, 0x2a);

    // the same block twice is deallocated once
    if (second != first)
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

        rw_assert (allocate_storage (n, false, dummy), 0, __LINE__,
                   "allocator<%s>::allocate (%zu) returns storage of "
                   "%zu bytes, twice",
                   tname, n, n * sizeof (T));
    }

    // 20.4.1.1, p4: a hint from allocate, not yet deallocated; p6
    // leaves its use unspecified, so the result is storage apart
    rw_assert (allocate_storage (2, true, dummy), 0, __LINE__,
               "allocator<%s>::allocate (2, p), p from allocate (2), "
               "returns storage of %zu bytes apart from p",
               tname, 2 * sizeof (T));

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

template <class T>
void
test_deallocate (const T*, const char *tname)
{
    rw_info (0, 0, __LINE__, "std::allocator<%s>::allocate () and "
             "deallocate () through operator new and delete", tname);

    std::allocator<T> alloc;

    const rwt_free_store* const store = rwt_get_free_store (0);

    const std::size_t blocks = store->blocks_ [0];
    const std::size_t bytes  = store->bytes_ [0];

    // 20.4.1.1, p3: the storage comes from ::operator new, here the
    // driver's replacement; p6 leaves when and how often unspecified,
    // and this allocator calls it once for each call
    T* const ptr = alloc.allocate (2);

    rw_assert (   blocks + 1 == store->blocks_ [0]
               && bytes + 2 * sizeof (T) == store->bytes_ [0], 0, __LINE__,
               "allocator<%s>::allocate (2) obtains one block of %zu "
               "bytes from operator new, got %zu blocks of %zu bytes",
               tname, 2 * sizeof (T), store->blocks_ [0] - blocks,
               store->bytes_ [0] - bytes);

    // 20.4.1.1, p8-10: deallocate (p, n), p and n those of a call to
    // allocate, returns the storage through ::operator delete; p10
    // leaves when unspecified, and this allocator calls it at once
    alloc.deallocate (ptr, 2);

    rw_assert (blocks == store->blocks_ [0] && bytes == store->bytes_ [0],
               0, __LINE__,
               "allocator<%s>::deallocate (p, 2) returns the block of "
               "allocate (2), got %zu blocks of %zu bytes left",
               tname, store->blocks_ [0] - blocks,
               store->bytes_ [0] - bytes);
}

/**************************************************************************/

template <class T>
void
test_address (const T*, const char *tname)
{
    rw_info (0, 0, __LINE__, "std::allocator<%s>::address ()", tname);

    const std::allocator<T> alloc;

    static T x;
    const T &cx = x;

    // 20.4.1.1, p1: &x
    T* const ptr = alloc.address (x);

    rw_assert (&x == ptr, 0, __LINE__,
               "allocator<%s>::address (reference) == %#p, got %#p",
               tname, &x, ptr);

    // 20.4.1.1, p2: &x
    const T* const cptr = alloc.address (cx);

    rw_assert (&x == cptr, 0, __LINE__,
               "allocator<%s>::address (const_reference) == %#p, got %#p",
               tname, &x, cptr);
}

/**************************************************************************/

template <class T>
void
test_rebind (const T*, const char *tname)
{
    rw_info (0, 0, __LINE__, "std::allocator<%s>::rebind", tname);

    // 20.1.5, p3: allocator<T>::rebind<U>::other is allocator<U>
    rw_assert (same_type<typename std::allocator<T>::template
                         rebind<int>::other, std::allocator<int> >::value,
               0, __LINE__,
               "allocator<%s>::rebind<int>::other is allocator<int>",
               tname);

    // 20.1.5, Table 32: Y::rebind<T>::other is X for all U, T included
    rw_assert (same_type<typename std::allocator<int>::template
                         rebind<T>::other, std::allocator<T> >::value,
               0, __LINE__,
               "allocator<int>::rebind<%s>::other is allocator<%s>",
               tname, tname);

    rw_assert (same_type<typename std::allocator<T>::template
                         rebind<T>::other, std::allocator<T> >::value,
               0, __LINE__,
               "allocator<%s>::rebind<%s>::other is allocator<%s>",
               tname, tname, tname);

    // 20.4.1: the specialization for void rebinds as the template
    rw_assert (same_type<typename std::allocator<void>::template
                         rebind<T>::other, std::allocator<T> >::value,
               0, __LINE__,
               "allocator<void>::rebind<%s>::other is allocator<%s>",
               tname, tname);
}

/**************************************************************************/

template <class T>
void
test_compare (const T*, const char *tname)
{
    rw_info (0, 0, __LINE__, "std::allocator<%s>::allocator "
             "(const allocator<int>&) and the comparisons", tname);

    const std::allocator<T>   a1;
    const std::allocator<T>   a2;
    const std::allocator<int> b;

    // 20.4.1.2, p1: true
    rw_assert (a1 == a2, 0, __LINE__,
               "allocator<%s>() == allocator<%s>()", tname, tname);

    rw_assert (a1 == b, 0, __LINE__,
               "allocator<%s>() == allocator<int>()", tname);

    // 20.4.1.2, p2: false
    rw_assert (!(a1 != a2), 0, __LINE__,
               "allocator<%s>() != allocator<%s>() is false", tname, tname);

    rw_assert (!(a1 != b), 0, __LINE__,
               "allocator<%s>() != allocator<int>() is false", tname);

    // 20.4.1, the converting constructor; 20.1.5, Table 32: after
    // X a (b), Y (a) == b
    const std::allocator<T> a (b);

    rw_assert (std::allocator<int>(a) == b, 0, __LINE__,
               "allocator<int>(allocator<%s>(b)) == b", tname);
}

/**************************************************************************/

static void
test_construct ()
{
    rw_info (0, 0, __LINE__,
             "std::allocator<Traced>::construct () and destroy ()");

    std::allocator<Traced> alloc;

    Traced* const ptr = alloc.allocate (1);
    const Traced  val (17);

    // 20.4.1.1, p12: new ((void*)p) T (val), one copy of val at p
    alloc.construct (ptr, val);

    rw_assert (   1 == Traced::ncopies && ptr == Traced::copy_to
               && &val == Traced::copy_from && 17 == ptr->value_,
               0, __LINE__,
               "allocator<Traced>::construct (%#p, val) copies val once "
               "to %#p, got %d copies, the last to %#p",
               ptr, ptr, Traced::ncopies, Traced::copy_to);

    // 20.4.1.1, p13: ((T*)p)->~T (), one destruction at p
    alloc.destroy (ptr);

    rw_assert (1 == Traced::ndtors && ptr == Traced::dtor_at, 0, __LINE__,
               "allocator<Traced>::destroy (%#p) destroys the object at "
               "%#p once, got %d destructions, the last at %#p",
               ptr, ptr, Traced::ndtors, Traced::dtor_at);

    alloc.deallocate (ptr, 1);
}

/**************************************************************************/

template <class T>
void
test_members (const T *dummy, const char *tname)
{
    test_allocate (dummy, tname);
    test_deallocate (dummy, tname);
    test_address (dummy, tname);
    test_rebind (dummy, tname);
    test_compare (dummy, tname);
}


static int
run_test (int, char**)
{
    test_members ((char*)0, "char");
    test_members ((int*)0, "int");
    test_members ((double*)0, "double");
    test_members ((Odd*)0, "Odd");
    test_members ((Big*)0, "Big");

    test_construct ();

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
