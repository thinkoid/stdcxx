/***************************************************************************
 *
 * 23.deque.storage.cpp - deque ownership and mutation regressions
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

#include <cstddef>
#include <deque>
#include <iterator>
#include <new>
#include <stdexcept>
#include <rw_driver.h>

int rw_opt_no_alias = 0;
int rw_opt_no_source = 0;
int rw_opt_no_boundaries = 0;
int rw_opt_no_allocator = 0;

struct Value {
    static int live;
    static int copies;
    static int fail_copy;
    int data;

    Value (int n = 0): data (n) { ++live; }
    Value (const Value &rhs): data (rhs.data) {
#ifndef _RWSTD_NO_EXCEPTIONS
        if (++copies == fail_copy)
            throw copies;
#endif
        ++live;
    }
    ~Value () { --live; }
    Value& operator= (const Value &rhs) { data = rhs.data; return *this; }
};

int Value::live = 0;
int Value::copies = 0;
int Value::fail_copy = 0;

template <class Category>
struct Source {
    typedef Category iterator_category;
    typedef Value value_type;
    typedef std::ptrdiff_t difference_type;
    typedef const Value* pointer;
    typedef const Value& reference;

    static int calls;
    static int fail;
    pointer pos;

    static void touch () {
#ifndef _RWSTD_NO_EXCEPTIONS
        if (++calls == fail)
            throw calls;
#endif
    }

    Source (pointer p = 0): pos (p) { }
    Source (const Source &rhs): pos (rhs.pos) { touch (); }
    Source& operator= (const Source &rhs) {
        touch (); pos = rhs.pos; return *this;
    }
    reference operator* () const { touch (); return *pos; }
    pointer operator-> () const { touch (); return pos; }
    Source& operator++ () { touch (); ++pos; return *this; }
    Source& operator-- () { touch (); --pos; return *this; }
    Source operator++ (int) { Source old (*this); ++*this; return old; }
    Source operator-- (int) { Source old (*this); --*this; return old; }
    bool operator== (const Source &rhs) const { touch (); return pos == rhs.pos; }
    bool operator!= (const Source &rhs) const { return !(*this == rhs); }
};

template <class Category> int Source<Category>::calls = 0;
template <class Category> int Source<Category>::fail = 0;

static void test_alias ()
{
    for (int size = 1; size != 34; ++size) {
        for (int pos = 0; pos <= size; ++pos) {
            for (int alias = 0; alias != size; ++alias) {
                for (int count = 1; count != 5; ++count) {
                    std::deque<int> d;
                    for (int i = 0; i != size; ++i)
                        d.push_back (i);

                    if (1 == count) {
                        const std::deque<int>::iterator result =
                            d.insert (d.begin () + pos, d [alias]);
                        rw_assert (result == d.begin () + pos, 0, __LINE__,
                                   "insert returned the wrong position");
                    }
                    else
                        d.insert (d.begin () + pos, count, d [alias]);

                    bool correct = d.size () == std::size_t (size + count);
                    for (int i = 0; correct && i != size + count; ++i) {
                        const int expected = i < pos ? i
                            : i < pos + count ? alias : i - count;
                        correct = d [i] == expected;
                    }
                    rw_assert (correct, 0, __LINE__,
                               "alias insert: size %d, position %d, "
                               "source %d, count %d", size, pos, alias, count);
                }
            }
        }
    }
}

static void test_boundaries ()
{
    for (int dir = 0; dir != 2; ++dir) {
        for (int size = 0; size != 130; ++size) {
            std::deque<int> d;
            for (int i = 0; i != size; ++i) {
                if (dir) d.push_front (i);
                else     d.push_back (i);
            }
            rw_assert (d.end () + 0 == d.end (), 0, __LINE__,
                       "end()+0 changed at size %d, direction %d", size, dir);
            rw_assert (d.begin () + size == d.end (), 0, __LINE__,
                       "end differs from begin()+size()");
            for (int off = 0; off <= size; ++off) {
                std::deque<int> copy (d);
                const std::deque<int>::iterator result =
                    copy.erase (copy.begin () + off, copy.end ());
                rw_assert (result == copy.end (), 0, __LINE__,
                           "erase suffix returned the wrong end, "
                           "size %d, offset %d", size, off);
            }
            d.clear ();
            rw_assert (d.end () - d.begin () == 0 && d.end () + 0 == d.begin (),
                       0, __LINE__, "retained empty storage has bad iterators");
            d.push_front (1);
            d.push_back (2);
            rw_assert (d.front () == 1 && d.back () == 2, 0, __LINE__,
                       "reuse of retained storage changed values");
        }
    }
}

template <class Category>
static void test_source (Category, const char *name)
{
#ifndef _RWSTD_NO_EXCEPTIONS
    typedef Source<Category> Iter;
    typedef std::deque<Value> Deque;

    for (int size = 0; size <= 8; size += 8) {
        for (int pos = 0; pos <= size; pos += 4) {
            for (int count = 2; count <= 10; ++count) {
                for (int ordinal = 1; ; ++ordinal) {
                    Iter::fail = 0;
                    const int live = Value::live;
                    bool caught = false;
                    {
                        Deque d;
                        for (int i = 0; i != size; ++i)
                            d.push_back (Value (i));
                        Value input [10];
                        for (int i = 0; i != count; ++i)
                            input [i].data = 100 + i;
                        const Iter first (input);
                        const Iter last (input + count);
                        const Deque::iterator begin = d.begin ();
                        const Deque::iterator end = d.end ();
                        const int before = Value::live;

                        Iter::calls = 0;
                        Iter::fail = ordinal;
                        try {
                            d.insert (d.begin () + pos, first, last);
                        }
                        catch (int) {
                            caught = true;
                        }
                        Iter::fail = 0;

                        if (caught) {
                            bool correct = d.size () == std::size_t (size);
                            for (int i = 0; correct && i != size; ++i)
                                correct = d [i].data == i;
                            rw_assert (correct, 0, __LINE__,
                                       "%s source exception %d changed contents "
                                       "at position %d, size %d, count %d",
                                       name, ordinal, pos, size, count);
                            rw_assert (d.begin () == begin && d.end () == end,
                                       0, __LINE__, "%s source exception changed "
                                       "endpoints", name);
                            rw_assert (Value::live == before, 0, __LINE__,
                                       "%s source exception %d leaked a value",
                                       name, ordinal);
                        }
                        else {
                            bool correct = d.size () == std::size_t (size + count);
                            for (int i = 0; correct && i != size + count; ++i)
                                correct = d [i].data == (i < pos ? i
                                    : i < pos + count ? 100 + i - pos : i - count);
                            rw_assert (correct, 0, __LINE__,
                                       "%s source insertion changed values", name);
                        }
                    }
                    rw_assert (Value::live == live, 0, __LINE__,
                               "%s source exception leaked past destruction", name);
                    if (!caught)
                        break;
                }
            }
        }
    }
#else
    _RWSTD_UNUSED (name);
#endif
}

// Keep allocation records outside the container so deallocation size and
// ownership can be checked after both successful and failed reservations.
struct Pool {
    struct Block {
        void *ptr;
        std::size_t count;
        bool values;
    };
    Block blocks [512];
    int used;
    int calls;
    int fail;
    std::size_t value_limit;
    std::size_t node_limit;

    Pool (): used (0), calls (0), fail (0), value_limit (4096), node_limit (4096) { }

    void record (void *p, std::size_t n, bool values) {
        RW_ASSERT (used < 512);
        blocks [used].ptr = p;
        blocks [used].count = n;
        blocks [used++].values = values;
    }

    void release (void *p, std::size_t n, bool values) {
        int i = 0;
        while (i != used && blocks [i].ptr != p)
            ++i;
        RW_ASSERT (i != used);
        rw_assert (blocks [i].count == n && blocks [i].values == values,
                   0, __LINE__, "deallocation changed allocation size or type");
        blocks [i] = blocks [--used];
    }

    void check () const {
        Value **map = 0;
        std::size_t capacity = 0;
        int buffers = 0;
        for (int i = 0; i != used; ++i) {
            if (blocks [i].values)
                ++buffers;
            else {
                rw_assert (0 == map, 0, __LINE__, "more than one retained map");
                map = static_cast<Value**>(blocks [i].ptr);
                capacity = blocks [i].count;
            }
        }
        bool seen = false;
        bool ended = false;
        int owned = 0;
        for (std::size_t i = 0; i != capacity; ++i) {
            if (map [i]) {
                rw_assert (!ended, 0, __LINE__, "null hole in the owned span");
                seen = true;
                int j = 0;
                while (j != used && blocks [j].ptr != map [i])
                    ++j;
                rw_assert (j != used && blocks [j].values, 0, __LINE__,
                           "map slot does not refer to an owned buffer");
                for (std::size_t k = 0; k != i; ++k)
                    rw_assert (map [k] != map [i], 0, __LINE__,
                               "two slots own the same buffer");
                ++owned;
            }
            else if (seen)
                ended = true;
        }
        rw_assert (owned == buffers, 0, __LINE__, "buffer missing from map");
    }
};

static bool is_value (Value*) { return true; }
template <class T> static bool is_value (T*) { return false; }

template <class T>
struct Allocator: std::allocator<T> {
    Pool *pool;
    typedef std::size_t size_type;
    template <class U> struct rebind { typedef Allocator<U> other; };

    Allocator (Pool *p = 0): pool (p) { }
    template <class U> Allocator (const Allocator<U> &rhs): pool (rhs.pool) { }

    T* allocate (size_type n, const void* = 0) {
        RW_ASSERT (n <= max_size ());
#ifndef _RWSTD_NO_EXCEPTIONS
        if (++pool->calls == pool->fail)
            throw std::bad_alloc ();
#endif
        T *p = std::allocator<T>::allocate (n);
        pool->record (p, n, is_value (p));
        return p;
    }
    void deallocate (T *p, size_type n) {
        pool->release (p, n, is_value (p));
        std::allocator<T>::deallocate (p, n);
    }
    size_type max_size () const {
        return is_value ((T*)0) ? pool->value_limit : pool->node_limit;
    }
    bool operator== (const Allocator &rhs) const { return pool == rhs.pool; }
    bool operator!= (const Allocator &rhs) const { return !(*this == rhs); }
};

typedef std::deque<Value, Allocator<Value> > OwnedDeque;

namespace __rw {

template <>
inline OwnedDeque::size_type
__rw_new_capacity<OwnedDeque> (OwnedDeque::size_type n, const OwnedDeque*)
{
    return n ? n * 2 : 3;
}

}

static void test_allocator ()
{
    // Reuse from the far end, recentering, new maps, and newly allocated
    // buffers must all roll back after either allocation or construction.
    for (int dir = 0; dir != 2; ++dir) {
        for (int state = 0; state != 5; ++state) {
            for (int kind = 0; kind != 3; ++kind) {
                for (int source = 0; source != 2; ++source) {
                    for (int ordinal = 1; ; ++ordinal) {
                        Pool pool;
                        const int live = Value::live;
                        bool caught = false;
                        {
                            const Allocator<Value> alloc (&pool);
                            OwnedDeque d (alloc);
                            const int count = state * 31;
                            for (int i = 0; i != count; ++i) {
                                if (dir) d.push_front (Value (i));
                                else     d.push_back (Value (i));
                            }
                            if (state & 1) {
                                for (int i = 0; i != count / 2; ++i) {
                                    if (dir) d.pop_back ();
                                    else     d.pop_front ();
                                }
                            }
                            if (4 == state)
                                d.clear ();
                            int expected [256];
                            Value *addresses [256];
                            const std::size_t size = d.size ();
                            for (std::size_t i = 0; i != size; ++i) {
                                expected [i] = d [i].data;
                                addresses [i] = &d [i];
                            }
                            const OwnedDeque::iterator begin = d.begin ();
                            const OwnedDeque::iterator end = d.end ();
                            const int blocks = pool.used;
                            const int before = Value::live;
                            const int off = 2 == kind ? int (size / 2)
                                                     : dir ? 0 : int (size);
                            const int n = kind ? 67 : 1;
                            Value input [67];
                            for (int i = 0; i != 67; ++i)
                                input [i].data = 200 + i;
                            if (source)
                                Value::fail_copy = Value::copies + ordinal;
                            else
                                pool.fail = pool.calls + ordinal;
                            try {
                                if (2 == kind)
                                    d.insert (d.begin () + off, input, input + n);
                                else if (1 == kind)
                                    d.insert (d.begin () + off, n, input [0]);
                                else if (dir)
                                    d.push_front (input [0]);
                                else
                                    d.push_back (input [0]);
                            }
                            catch (...) {
                                caught = true;
                            }
                            pool.fail = 0;
                            Value::fail_copy = 0;
                            pool.check ();
                            if (caught) {
                                rw_assert (d.size () == size && d.begin () == begin
                                           && d.end () == end, 0, __LINE__,
                                           "reservation failure changed endpoints");
                                rw_assert (pool.used == blocks, 0, __LINE__,
                                           "reservation failure retained allocations");
                                rw_assert (Value::live == before + 67, 0, __LINE__,
                                           "reservation failure leaked values");
                                for (std::size_t i = 0; i != size; ++i)
                                    rw_assert (&d [i] == addresses [i]
                                               && d [i].data == expected [i],
                                               0, __LINE__, "failed reservation "
                                               "changed element %zu", i);
                            }
                            else {
                                bool correct = d.size () == size + n;
                                for (std::size_t i = 0; correct && i != d.size (); ++i)
                                    correct = d [i].data == (i < std::size_t (off)
                                        ? expected [i] : i < std::size_t (off + n)
                                        ? 200 + (2 == kind ? int (i) - off : 0)
                                        : expected [i - n]);
                                rw_assert (correct, 0, __LINE__,
                                           "reservation changed insertion result");
                                if (2 != kind) {
                                    for (std::size_t i = 0; i != size; ++i)
                                        rw_assert (&d [i + (dir ? n : 0)] == addresses [i],
                                                   0, __LINE__, "end insertion moved elements");
                                }
                            }
                        }
                        rw_assert (0 == pool.used && Value::live == live,
                                   0, __LINE__, "destruction leaked owned storage");
                        if (!caught)
                            break;
                    }
                }
            }
        }
    }

    // Exercise the maximum map extent, including reuse when it cannot grow.
    for (std::size_t nodes = 1; nodes != 4; ++nodes) {
        Pool pool;
        pool.node_limit = nodes + 1;
        const Allocator<Value> alloc (&pool);
        {
            OwnedDeque d (alloc);
            d.assign (d.max_size (), Value (1));
            pool.check ();
            for (int i = 0; i != 150; ++i) {
                d.pop_front ();
                d.push_back (Value (i));
                pool.check ();
            }
#ifndef _RWSTD_NO_EXCEPTIONS
            bool caught = false;
            try { d.push_back (Value (0)); }
            catch (const std::length_error&) { caught = true; }
            rw_assert (caught, 0, __LINE__, "growth past max_size did not throw");
            caught = false;
            try { d.insert (d.begin (), OwnedDeque::size_type (-1), Value (0)); }
            catch (const std::length_error&) { caught = true; }
            rw_assert (caught, 0, __LINE__, "overflowing insertion did not throw");
#endif
            pool.check ();
            d.clear ();
            const int blocks = pool.used;
            d.push_front (Value (2));
            d.pop_back ();
            rw_assert (blocks == pool.used, 0, __LINE__, "clear lost spare buffers");
        }
        rw_assert (0 == pool.used, 0, __LINE__, "bounded allocator leaked");
    }

    {
        Pool pool;
        const Allocator<Value> alloc (&pool);
        {
            OwnedDeque d (alloc);
            d.push_back (Value (0));
            rw_assert (2 == pool.used, 0, __LINE__,
                       "first push allocated more than a map and one buffer");
            for (int i = 1; i != 100; ++i)
                d.push_back (Value (i));
            for (int i = 0; i != 1024; ++i) {
                d.push_back (Value (i));
                d.pop_front ();
            }
            const int calls = pool.calls;
            for (int i = 0; i != 32768; ++i) {
                d.push_back (Value (i));
                d.pop_front ();
            }
            rw_assert (calls == pool.calls, 0, __LINE__,
                       "steady queue allocated after warm-up");
            rw_assert (d.size () == 100 && d.front ().data == 32668
                       && d.back ().data == 32767, 0, __LINE__,
                       "queue reuse changed contents");
            pool.check ();
            for (int i = 0; i != 1024; ++i) {
                d.push_front (Value (i));
                d.pop_back ();
            }
            const int reverse_calls = pool.calls;
            for (int i = 0; i != 32768; ++i) {
                d.push_front (Value (i));
                d.pop_back ();
            }
            rw_assert (reverse_calls == pool.calls, 0, __LINE__,
                       "reverse queue allocated after warm-up");
            pool.check ();
        }
        rw_assert (0 == pool.used, 0, __LINE__, "queue leaked storage");
    }

    // A deque that grew and drained refills from its spares at either
    // end: bufferfuls of pushes at the null boundary allocate nothing
    // until the spares are used up, and push_front takes the spare of
    // the slot before directly, as push_back takes the one after.
    for (int front = 0; front != 2; ++front) {
        Pool pool;
        const Allocator<Value> alloc (&pool);
        {
            OwnedDeque d (alloc);
            const int bufsize = 3;   // the growth hook above: three-element buffers
            const int buffers = 24;
            for (int i = 0; i != buffers * bufsize; ++i)
                d.push_back (Value (i));
            const int blocks = pool.used;
            if (front)
                while (d.size () != 1)
                    d.pop_front ();
            else
                while (d.size () != 1)
                    d.pop_back ();
            const int calls = pool.calls;
            for (int i = 0; i != (buffers - 2) * bufsize; ++i) {
                if (front)
                    d.push_front (Value (i));
                else
                    d.push_back (Value (i));
            }
            rw_assert (calls == pool.calls, 0, __LINE__,
                       "refill from spares allocated, %s", front ? "push_front" : "push_back");
            rw_assert (blocks == pool.used, 0, __LINE__,
                       "refill from spares changed the storage, %s",
                       front ? "push_front" : "push_back");
            rw_assert (d.size () == std::size_t ((buffers - 2) * bufsize + 1),
                       0, __LINE__, "refill lost elements");
            bool correct = true;
            for (int i = 0; i != (buffers - 2) * bufsize; ++i) {
                const int at = front ? (buffers - 2) * bufsize - 1 - i : i + 1;
                if (d [at].data != i)
                    correct = false;
            }
            rw_assert (correct, 0, __LINE__, "refill changed contents");
            pool.check ();
        }
        rw_assert (0 == pool.used, 0, __LINE__, "refilled deque leaked storage");
    }

    // Different allocators must continue owning their respective allocations.
    Pool left;
    Pool right;
    {
        const Allocator<Value> a (&left);
        const Allocator<Value> b (&right);
        OwnedDeque x (3, Value (1), a);
        OwnedDeque y (65, Value (2), b);
        x.swap (y);
        rw_assert (x.size () == 65 && x.front ().data == 2
                   && y.size () == 3 && y.back ().data == 1,
                   0, __LINE__, "unequal-allocator swap changed contents");
        left.check ();
        right.check ();
    }
    rw_assert (0 == left.used && 0 == right.used, 0, __LINE__,
               "unequal-allocator swap lost ownership");
}

static int run_test (int, char**)
{
    if (!rw_opt_no_allocator)
        test_allocator ();
    if (!rw_opt_no_alias)
        test_alias ();
    if (!rw_opt_no_boundaries)
        test_boundaries ();
    if (!rw_opt_no_source) {
        test_source (std::input_iterator_tag (), "input");
        test_source (std::forward_iterator_tag (), "forward");
        test_source (std::bidirectional_iterator_tag (), "bidirectional");
    }
    return 0;
}

int main (int argc, char **argv)
{
    return rw_test (argc, argv, __FILE__, "lib.deque", 0, run_test,
                    "|-no-alias# |-no-source# |-no-boundaries# |-no-allocator#",
                    &rw_opt_no_alias, &rw_opt_no_source, &rw_opt_no_boundaries,
                    &rw_opt_no_allocator);
}
