/***************************************************************************
 *
 * 23.set.cpp - test exercising the members of [lib.set] and
 *              [lib.multiset]
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

#include <set>              // for set, multiset
#include <functional>       // for less
#include <memory>           // for allocator
#include <utility>          // for pair

#include <cstddef>          // for size_t
#include <cstring>          // for strcmp(), strlen()

#include <rw_value.h>       // for UserClass
#include <rw_driver.h>      // for rw_test(), rw_assert()

/**************************************************************************/

// a comparison that counts its calls and orders the keys either way;
// the containers copy and assign it, so the order is a member
template <class Key>
struct CountingCompare
{
    static std::size_t ncalls_;

    bool reversed_;

    explicit CountingCompare (bool reversed = false)
        : reversed_ (reversed) { }

    bool operator() (const Key &x, const Key &y) const {
        ++ncalls_;
        return reversed_ ? y < x : x < y;
    }
};

template <class Key>
std::size_t CountingCompare<Key>::ncalls_;


// the comparison object a container is constructed with; less has
// one order only
template <class Key>
std::less<Key>
make_compare (std::less<Key>*, bool)
{
    return std::less<Key> ();
}

template <class Key>
CountingCompare<Key>
make_compare (CountingCompare<Key>*, bool reversed)
{
    return CountingCompare<Key> (reversed);
}


template <class Key>
bool
is_reversed (const std::less<Key>&)
{
    return false;
}

template <class Key>
bool
is_reversed (const CountingCompare<Key> &cmp)
{
    return cmp.reversed_;
}


// the count of the calls of the comparison, or 0 where it keeps none
template <class Key>
std::size_t*
call_counter (std::less<Key>*)
{
    return 0;
}

template <class Key>
std::size_t*
call_counter (CountingCompare<Key>*)
{
    return &CountingCompare<Key>::ncalls_;
}

/**************************************************************************/

// the key that the character or number c stands for: c itself, or -c
// where the comparison is reversed, so that the keys take the same
// positions in either order and one table serves both; it is its own
// inverse
static int
key_of (int c, bool rev)
{
    return rev ? -c : c;
}


// the key of an element
static int
elem_key (int elem)
{
    return elem;
}

static int
elem_key (const UserClass &elem)
{
    return elem.data_.val_;
}


// the identity of an element: the id of the object a UserClass is a
// copy of, which every copy keeps; an int has none
static int
elem_tag (int)
{
    return -1;
}

static int
elem_tag (const UserClass &elem)
{
    return elem.origin_;
}


// the element holding key; the tag is for the elements of a map,
// which carry it in their mapped value
static int
make_elem (int key, int, int*)
{
    return key;
}

static UserClass
make_elem (int key, int, UserClass*)
{
    UserClass x;
    x.data_.val_ = key;
    return x;
}


// the key object holding key, an element itself in a set
template <class Key>
Key
make_key (int key, Key*)
{
    return make_elem (key, 0, (Key*)0);
}

/**************************************************************************/

// an array of elements constructed in place
template <class Value>
struct ElemArray
{
    std::allocator<Value> alloc_;
    Value*                elems_;
    std::size_t           n_;

    explicit ElemArray (std::size_t n)
        : elems_ (alloc_.allocate (n + 1)), n_ (0) { }

    ~ElemArray () {
        for (std::size_t i = 0; i != n_; ++i)
            alloc_.destroy (elems_ + i);

        alloc_.deallocate (elems_, n_ + 1);
    }

    // appends the element holding key; the elements are appended
    // in place since the value_type of a map is not Assignable
    void push (int key, int tag) {
        alloc_.construct (elems_ + n_, make_elem (key, tag, (Value*)0));
        ++n_;
    }

    const Value* begin () const { return elems_; }
    const Value* end () const { return elems_ + n_; }

private:

    // not copyable
    ElemArray (const ElemArray&);
    void operator= (const ElemArray&);
};


// inserts the keys the characters of str stand for, in order, each
// tagged with its position from 1
template <class Container>
void
fill (Container &cont, const char *str, bool rev)
{
    typedef typename Container::value_type Value;

    for (int i = 0; str [i]; ++i)
        cont.insert (make_elem (key_of (str [i], rev), i + 1, (Value*)0));
}


// the characters the keys of cont stand for, from begin () to end ()
template <class Container>
const char*
keys_of (const Container &cont, bool rev)
{
    typedef typename Container::const_iterator ConstIter;

    static char buf [80];

    std::size_t n = 0;

    for (ConstIter it = cont.begin ();
         !(it == cont.end ()) && n + 1 < sizeof buf; ++it)
        buf [n++] = char (key_of (elem_key (*it), rev));

    buf [n] = '\0';

    return buf;
}


// true when the keys of cont are the characters of str and the size
// of cont is their number
template <class Container>
bool
match (const Container &cont, const char *str, bool rev)
{
    return    0 == std::strcmp (keys_of (cont, rev), str)
           && cont.size () == std::strlen (str);
}


// the position of it from first
template <class Iterator>
int
index_of (Iterator first, Iterator it)
{
    int n = 0;

    for ( ; !(first == it); ++first)
        ++n;

    return n;
}


// the position that stands for end () in the tables
static const int End = -1;

// the iterator at position inx of cont, or end () for End
template <class Container>
typename Container::iterator
iter_at (Container &cont, int inx)
{
    typename Container::iterator it = cont.begin ();

    if (End == inx)
        return cont.end ();

    while (inx--)
        ++it;

    return it;
}


// the iterator a unique container's insert (t) returns in its pair,
// with the bool, and the one a multiple container's returns alone
template <class Iterator>
Iterator
insert_result (const std::pair<Iterator, bool> &ret, bool *inserted)
{
    *inserted = ret.second;
    return ret.first;
}

template <class Iterator>
Iterator
insert_result (const Iterator &ret, bool *inserted)
{
    *inserted = true;
    return ret;
}

/**************************************************************************/

// the ranges of the range constructor, and the containers of the
// copy, the assignment and clear
static const struct {
    int         line;
    const char *src;     // the keys of the range, in order
    const char *uniq;    // the keys of a unique container from it
    const char *multi;   // the keys of a multiple container from it
} range_rows [] = {
    { __LINE__, "",        "",        ""        },
    { __LINE__, "a",       "a",       "a"       },
    { __LINE__, "ab",      "ab",      "ab"      },
    { __LINE__, "ba",      "ab",      "ab"      },
    { __LINE__, "cab",     "abc",     "abc"     },
    { __LINE__, "aa",      "a",       "aa"      },
    { __LINE__, "bab",     "ab",      "abb"     },
    { __LINE__, "aaaa",    "a",       "aaaa"    },
    { __LINE__, "dbcadb",  "abcd",    "abbcdd"  },
    { __LINE__, "gfedcba", "abcdefg", "abcdefg" }
};

static const std::size_t n_range_rows = sizeof range_rows / sizeof *range_rows;

/**************************************************************************/

template <class Container>
void
test_cons (Container*, const char *cname, bool multi, bool rev)
{
    typedef typename Container::key_compare    Compare;
    typedef typename Container::value_type     Value;
    typedef typename Container::allocator_type Alloc;

    rw_info (0, 0, __LINE__,
             "std::%s: construction, copy and assignment", cname);

    const Compare cmp = make_compare ((Compare*)0, rev);

    // the other order, where the comparison has one
    const bool orev = is_reversed (make_compare ((Compare*)0, !rev));

    // 23.3.3.1, p1-2 and 23.3.4.1, p1-2; Table 69, X(c) and X():
    // an empty container that uses the comparison object given,
    // or Compare()
    const Container a (cmp);
    const Container b (cmp, Alloc ());
    const Container c;

    rw_assert (   a.empty () && 0 == a.size () && a.begin () == a.end ()
               && b.empty () && 0 == b.size () && b.begin () == b.end ()
               && c.empty () && 0 == c.size () && c.begin () == c.end (),
               0, __LINE__,
               "%s: X(c), X(c, Allocator ()) and X() are empty", cname);

    rw_assert (   rev == is_reversed (a.key_comp ())
               && rev == is_reversed (b.key_comp ())
               && !is_reversed (c.key_comp ()),
               0, __LINE__,
               "%s: X(c) and X(c, Allocator ()) use c, X() Compare()",
               cname);

    // 23.1, p8: get_allocator () returns the allocator given
    rw_assert (b.get_allocator () == Alloc (), 0, __LINE__,
               "%s: get_allocator () == Allocator ()", cname);

    for (std::size_t i = 0; i != n_range_rows; ++i) {
        const char* const src    = range_rows [i].src;
        const char* const expect = multi ? range_rows [i].multi
                                         : range_rows [i].uniq;
        const int         line   = range_rows [i].line;

        ElemArray<Value> range (std::strlen (src));

        for (int j = 0; src [j]; ++j)
            range.push (key_of (src [j], rev), j + 1);

        // 23.3.3.1, p3 and 23.3.4.1, p3; Table 69, X(i, j, c): the
        // elements of the range, unique or all, in the order of c
        const Container x (range.begin (), range.end (), cmp);

        rw_assert (match (x, expect, rev), 0, line,
                   "%s: X(i, j, c) from \"%s\": keys \"%s\", expected "
                   "\"%s\"", cname, src, keys_of (x, rev), expect);

        rw_assert (rev == is_reversed (x.key_comp ()), 0, line,
                   "%s: X(i, j, c) from \"%s\" uses c", cname, src);

        const Container y (range.begin (), range.end (), cmp, Alloc ());

        rw_assert (match (y, expect, rev), 0, line,
                   "%s: X(i, j, c, Allocator ()) from \"%s\": keys \"%s\", "
                   "expected \"%s\"", cname, src, keys_of (y, rev), expect);

        if (!rev) {
            // Table 69, X(i, j): the same, with Compare()
            const Container z (range.begin (), range.end ());

            rw_assert (match (z, expect, rev), 0, line,
                       "%s: X(i, j) from \"%s\": keys \"%s\", expected "
                       "\"%s\"", cname, src, keys_of (z, rev), expect);
        }

        // Table 65, X(a) and X u = a: a copy equal to the source;
        // 23.1.2, p11: with the source's comparison object
        const Container u (x);
        const Container v = x;

        rw_assert (u == x && v == x && match (u, expect, rev),
                   0, line,
                   "%s: X(a) and X u = a of \"%s\": keys \"%s\"",
                   cname, expect, keys_of (u, rev));

        rw_assert (   rev == is_reversed (u.key_comp ())
                   && rev == is_reversed (v.key_comp ()),
                   0, line,
                   "%s: X(a) of \"%s\" uses the comparison object of a",
                   cname, expect);

        // Table 65, r = a: r == a; 23.1.2, p11: the target takes the
        // comparison object of the source, here from the other order
        Container r (make_compare ((Compare*)0, !rev));

        fill (r, "xyz", orev);

        Container &ret = (r = x);

        rw_assert (&ret == &r && r == x && match (r, expect, rev),
                   0, line,
                   "%s: r = a of \"%s\": keys \"%s\"",
                   cname, expect, keys_of (r, rev));

        rw_assert (rev == is_reversed (r.key_comp ()), 0, line,
                   "%s: r = a of \"%s\" takes the comparison object of a",
                   cname, expect);
    }
}

/**************************************************************************/

template <class Container>
void
test_insert (Container*, const char *cname, bool multi, bool rev)
{
    typedef typename Container::key_compare Compare;
    typedef typename Container::value_type  Value;
    typedef typename Container::iterator    Iter;

    rw_info (0, 0, __LINE__, "std::%s: insert (t)", cname);

    static const struct {
        int         line;
        const char *keys;    // the keys inserted first, in order
        char        key;     // the key of t
        bool        uins;    // whether a unique container inserts t
        const char *uniq;    // the keys of a unique container after
        const char *multi;   // the keys of a multiple container after
    } rows [] = {
        { __LINE__, "",      'a', true,  "a",     "a"      },
        { __LINE__, "b",     'a', true,  "ab",    "ab"     },
        { __LINE__, "b",     'c', true,  "bc",    "bc"     },
        { __LINE__, "b",     'b', false, "b",     "bb"     },
        { __LINE__, "ace",   'b', true,  "abce",  "abce"   },
        { __LINE__, "ace",   'd', true,  "acde",  "acde"   },
        { __LINE__, "ace",   'a', false, "ace",   "aace"   },
        { __LINE__, "ace",   'c', false, "ace",   "acce"   },
        { __LINE__, "ace",   'e', false, "ace",   "acee"   },
        { __LINE__, "ace",   'f', true,  "acef",  "acef"   },
        { __LINE__, "acce",  'c', false, "ace",   "accce"  },
        { __LINE__, "edcba", 'c', false, "abcde", "abccde" }
    };

    for (std::size_t i = 0; i != sizeof rows / sizeof *rows; ++i) {
        const char* const expect = multi ? rows [i].multi : rows [i].uniq;
        const bool        ins    = multi || rows [i].uins;
        const int         line   = rows [i].line;
        const int         key    = key_of (rows [i].key, rev);

        Container cont (make_compare ((Compare*)0, rev));

        fill (cont, rows [i].keys, rev);

        const Value t = make_elem (key, 100, (Value*)0);

        bool inserted;

        const Iter it = insert_result (cont.insert (t), &inserted);

        // Table 69, a_uniq.insert(t): inserts t if and only if no
        // key is equivalent, and says so in the bool; a_eq.insert(t)
        // inserts t
        rw_assert (inserted == ins, 0, line,
                   "%s: insert (%c) on \"%s\": inserted %b, expected %b",
                   cname, rows [i].key, rows [i].keys, inserted, ins);

        rw_assert (match (cont, expect, rev), 0, line,
                   "%s: insert (%c) on \"%s\": keys \"%s\", expected \"%s\"",
                   cname, rows [i].key, rows [i].keys,
                   keys_of (cont, rev), expect);

        // the iterator points to the element with the key of t: the
        // new one, or the one that stays where t was not inserted
        bool same = !(it == cont.end ()) && elem_key (*it) == key;

        if (same && 0 <= elem_tag (t))
            same = (elem_tag (*it) == elem_tag (t)) == ins;

        rw_assert (same, 0, line,
                   "%s: insert (%c) on \"%s\": the iterator points to the "
                   "%s element", cname, rows [i].key, rows [i].keys,
                   ins ? "new" : "existing");
    }
}

/**************************************************************************/

template <class Container>
void
test_insert_hint (Container*, const char *cname, bool multi, bool rev)
{
    typedef typename Container::key_compare Compare;
    typedef typename Container::value_type  Value;
    typedef typename Container::iterator    Iter;

    rw_info (0, 0, __LINE__, "std::%s: insert (p, t)", cname);

    // the hint is right where t goes right after p (the amortized
    // constant case of Table 69), wrong otherwise
    static const struct {
        int         line;
        const char *keys;    // the keys of the container, distinct
        int         hint;    // the position of p, or End
        char        key;     // the key of t
        const char *uniq;    // the keys of a unique container after
        const char *multi;   // the keys of a multiple container after
    } rows [] = {
        { __LINE__, "",     End, 'c', "c",     "c"     },
        { __LINE__, "bdfh", 0,   'c', "bcdfh", "bcdfh" },   // right
        { __LINE__, "bdfh", 1,   'e', "bdefh", "bdefh" },   // right
        { __LINE__, "bdfh", 3,   'i', "bdfhi", "bdfhi" },   // right
        { __LINE__, "bdfh", 3,   'e', "bdefh", "bdefh" },   // wrong
        { __LINE__, "bdfh", 2,   'c', "bcdfh", "bcdfh" },   // wrong
        { __LINE__, "bdfh", End, 'e', "bdefh", "bdefh" },   // end ()
        { __LINE__, "bdfh", End, 'i', "bdfhi", "bdfhi" },   // end ()
        { __LINE__, "bdfh", End, 'a', "abdfh", "abdfh" },   // end ()
        { __LINE__, "bdfh", 0,   'a', "abdfh", "abdfh" },   // begin ()
        { __LINE__, "bdfh", 0,   'e', "bdefh", "bdefh" },   // begin ()
        { __LINE__, "bdfh", 0,   'i', "bdfhi", "bdfhi" },   // begin ()
        { __LINE__, "bdfh", 1,   'd', "bdfh",  "bddfh" },   // equal key
        { __LINE__, "bdfh", 0,   'd', "bdfh",  "bddfh" },
        { __LINE__, "bdfh", 3,   'b', "bdfh",  "bbdfh" },
        { __LINE__, "bdfh", End, 'h', "bdfh",  "bdfhh" },
        { __LINE__, "b",    0,   'b', "b",     "bb"    },
        { __LINE__, "b",    End, 'a', "ab",    "ab"    }
    };

    for (std::size_t i = 0; i != sizeof rows / sizeof *rows; ++i) {
        const char* const expect = multi ? rows [i].multi : rows [i].uniq;
        const int         line   = rows [i].line;
        const int         key    = key_of (rows [i].key, rev);

        Container cont (make_compare ((Compare*)0, rev));

        fill (cont, rows [i].keys, rev);

        const bool ins = cont.size () < std::strlen (expect);

        const Value t = make_elem (key, 100, (Value*)0);

        const Iter it = cont.insert (iter_at (cont, rows [i].hint), t);

        // Table 69, a.insert(p, t): inserts t if and only if no key
        // is equivalent in a unique container, always in a multiple
        // one, wherever p points
        rw_assert (match (cont, expect, rev), 0, line,
                   "%s: insert (%d, %c) on \"%s\": keys \"%s\", "
                   "expected \"%s\"", cname, rows [i].hint, rows [i].key,
                   rows [i].keys, keys_of (cont, rev), expect);

        // ... and always returns the iterator pointing to the element
        // with the key of t
        bool same = !(it == cont.end ()) && elem_key (*it) == key;

        if (same && 0 <= elem_tag (t))
            same = (elem_tag (*it) == elem_tag (t)) == ins;

        rw_assert (same, 0, line,
                   "%s: insert (%d, %c) on \"%s\": the iterator points "
                   "to the %s element", cname, rows [i].hint, rows [i].key,
                   rows [i].keys, ins ? "new" : "existing");
    }
}

/**************************************************************************/

template <class Container>
void
test_insert_range (Container*, const char *cname, bool multi, bool rev)
{
    typedef typename Container::key_compare Compare;
    typedef typename Container::value_type  Value;

    rw_info (0, 0, __LINE__, "std::%s: insert (i, j)", cname);

    static const struct {
        int         line;
        const char *keys;    // the keys of the container
        const char *src;     // the keys of the range, in order
        const char *uniq;    // the keys of a unique container after
        const char *multi;   // the keys of a multiple container after
    } rows [] = {
        { __LINE__, "",    "",      "",       ""         },
        { __LINE__, "",    "cab",   "abc",    "abc"      },
        { __LINE__, "b",   "",      "b",      "b"        },
        { __LINE__, "b",   "b",     "b",      "bb"       },
        { __LINE__, "bd",  "aceg",  "abcdeg", "abcdeg"   },
        { __LINE__, "bd",  "dcba",  "abcd",   "abbcdd"   },
        { __LINE__, "ace", "eeaa",  "ace",    "aaaceee"  },
        { __LINE__, "ace", "fdbfb", "abcdef", "abbcdeff" }
    };

    for (std::size_t i = 0; i != sizeof rows / sizeof *rows; ++i) {
        const char* const src    = rows [i].src;
        const char* const expect = multi ? rows [i].multi : rows [i].uniq;
        const int         line   = rows [i].line;

        Container cont (make_compare ((Compare*)0, rev));

        fill (cont, rows [i].keys, rev);

        ElemArray<Value> range (std::strlen (src));

        for (int j = 0; src [j]; ++j)
            range.push (key_of (src [j], rev), j + 1);

        // Table 69, a.insert(i, j): each element of the range if and
        // only if no key is equivalent in a unique container, every
        // one in a multiple container
        cont.insert (range.begin (), range.end ());

        rw_assert (match (cont, expect, rev), 0, line,
                   "%s: insert (i, j) of \"%s\" on \"%s\": keys \"%s\", "
                   "expected \"%s\"", cname, src, rows [i].keys,
                   keys_of (cont, rev), expect);
    }
}

/**************************************************************************/

template <class Container>
void
test_erase (Container*, const char *cname, bool multi, bool rev)
{
    typedef typename Container::key_compare Compare;
    typedef typename Container::key_type    Key;
    typedef typename Container::iterator    Iter;
    typedef typename Container::size_type   Size;

    rw_info (0, 0, __LINE__, "std::%s: erase and clear", cname);

    // the positions are those of the container each kind makes
    static const struct {
        int         line;
        const char *keys;    // the keys of the container
        int         upos;    // the position erased in a unique container
        const char *uniq;    // the keys of the unique container after
        int         mpos;    // the position in a multiple container
        const char *multi;   // the keys of the multiple container after
    } pos_rows [] = {
        { __LINE__, "a",       0, "",       0, ""       },
        { __LINE__, "ab",      0, "b",      0, "b"      },
        { __LINE__, "ab",      1, "a",      1, "a"      },
        { __LINE__, "abc",     1, "ac",     1, "ac"     },
        { __LINE__, "abbc",    1, "ac",     2, "abc"    },
        { __LINE__, "abbc",    2, "ab",     3, "abb"    },
        { __LINE__, "gfedcba", 3, "abcefg", 3, "abcefg" },
        { __LINE__, "gfedcba", 6, "abcdef", 0, "bcdefg" }
    };

    for (std::size_t i = 0; i != sizeof pos_rows / sizeof *pos_rows; ++i) {
        const char* const expect = multi ? pos_rows [i].multi
                                         : pos_rows [i].uniq;
        const int         pos    = multi ? pos_rows [i].mpos
                                         : pos_rows [i].upos;
        const int         line   = pos_rows [i].line;

        Container cont (make_compare ((Compare*)0, rev));

        fill (cont, pos_rows [i].keys, rev);

        // the other elements, by iterator, key and identity
        Iter others [16];
        int  keys [16];
        int  tags [16];
        int  nothers = 0;

        int inx = 0;

        for (Iter it = cont.begin (); !(it == cont.end ()); ++it, ++inx) {
            if (inx != pos && nothers < 16) {
                others [nothers] = it;
                keys [nothers]   = elem_key (*it);
                tags [nothers]   = elem_tag (*it);
                ++nothers;
            }
        }

        // Table 69, a.erase(q): erases the element q points to
        cont.erase (iter_at (cont, pos));

        rw_assert (match (cont, expect, rev), 0, line,
                   "%s: erase (%d) on \"%s\": keys \"%s\", expected \"%s\"",
                   cname, pos, pos_rows [i].keys, keys_of (cont, rev),
                   expect);

        // 23.1.2, p8: and invalidates the iterators to it alone
        bool valid = true;

        for (int j = 0; valid && j != nothers; ++j) {
            valid =    keys [j] == elem_key (*others [j])
                    && tags [j] == elem_tag (*others [j]);
        }

        rw_assert (valid, 0, line,
                   "%s: erase (%d) on \"%s\" leaves the other iterators "
                   "valid", cname, pos, pos_rows [i].keys);
    }

    static const struct {
        int         line;
        const char *keys;    // the keys of the container
        char        key;     // the key erased
        int         ucount;  // the count a unique container returns
        const char *uniq;    // the keys of the unique container after
        int         mcount;  // the count a multiple container returns
        const char *multi;   // the keys of the multiple container after
    } key_rows [] = {
        { __LINE__, "",       'a', 0, "",    0, ""      },
        { __LINE__, "a",      'a', 1, "",    1, ""      },
        { __LINE__, "b",      'a', 0, "b",   0, "b"     },
        { __LINE__, "b",      'c', 0, "b",   0, "b"     },
        { __LINE__, "bdf",    'c', 0, "bdf", 0, "bdf"   },
        { __LINE__, "bdf",    'd', 1, "bf",  1, "bf"    },
        { __LINE__, "bddf",   'd', 1, "bf",  2, "bf"    },
        { __LINE__, "bbbddf", 'b', 1, "df",  3, "ddf"   },
        { __LINE__, "bddfff", 'f', 1, "bd",  3, "bdd"   },
        { __LINE__, "aaaa",   'a', 1, "",    4, ""      }
    };

    for (std::size_t i = 0; i != sizeof key_rows / sizeof *key_rows; ++i) {
        const char* const expect = multi ? key_rows [i].multi
                                         : key_rows [i].uniq;
        const int         count  = multi ? key_rows [i].mcount
                                         : key_rows [i].ucount;
        const int         line   = key_rows [i].line;

        Container cont (make_compare ((Compare*)0, rev));

        fill (cont, key_rows [i].keys, rev);

        // Table 69, a.erase(k): erases the elements with a key
        // equivalent to k and returns their number
        const Size n = cont.erase (make_key (key_of (key_rows [i].key, rev),
                                             (Key*)0));

        rw_assert (Size (count) == n, 0, line,
                   "%s: erase (%c) on \"%s\" == %d, got %d",
                   cname, key_rows [i].key, key_rows [i].keys,
                   count, int (n));

        rw_assert (match (cont, expect, rev), 0, line,
                   "%s: erase (%c) on \"%s\": keys \"%s\", expected \"%s\"",
                   cname, key_rows [i].key, key_rows [i].keys,
                   keys_of (cont, rev), expect);
    }

    static const struct {
        int         line;
        const char *keys;    // the keys of the container
        int         ufirst;  // the range erased in a unique container
        int         ulast;
        const char *uniq;    // the keys of the unique container after
        int         mfirst;  // the range erased in a multiple container
        int         mlast;
        const char *multi;   // the keys of the multiple container after
    } span_rows [] = {
        { __LINE__, "",       0, End, "",     0, End, ""     },
        { __LINE__, "abc",    0, 0,   "abc",  0, 0,   "abc"  },
        { __LINE__, "abc",    1, 1,   "abc",  3, End, "abc"  },
        { __LINE__, "abc",    0, End, "",     0, End, ""     },
        { __LINE__, "abc",    0, 1,   "bc",   0, 1,   "bc"   },
        { __LINE__, "abc",    1, End, "a",    1, End, "a"    },
        { __LINE__, "abcde",  1, 4,   "ae",   1, 4,   "ae"   },
        { __LINE__, "abbbc",  1, 2,   "ac",   1, 4,   "ac"   },
        { __LINE__, "abbbc",  0, 2,   "c",    2, End, "ab"   }
    };

    for (std::size_t i = 0; i != sizeof span_rows / sizeof *span_rows; ++i) {
        const char* const expect = multi ? span_rows [i].multi
                                         : span_rows [i].uniq;
        const int         first  = multi ? span_rows [i].mfirst
                                         : span_rows [i].ufirst;
        const int         last   = multi ? span_rows [i].mlast
                                         : span_rows [i].ulast;
        const int         line   = span_rows [i].line;

        Container cont (make_compare ((Compare*)0, rev));

        fill (cont, span_rows [i].keys, rev);

        // Table 69, a.erase(q1, q2): erases the elements of [q1, q2)
        cont.erase (iter_at (cont, first), iter_at (cont, last));

        rw_assert (match (cont, expect, rev), 0, line,
                   "%s: erase (%d, %d) on \"%s\": keys \"%s\", "
                   "expected \"%s\"", cname, first, last,
                   span_rows [i].keys, keys_of (cont, rev), expect);
    }

    for (std::size_t i = 0; i != n_range_rows; ++i) {
        const int line = range_rows [i].line;

        Container cont (make_compare ((Compare*)0, rev));

        fill (cont, range_rows [i].src, rev);

        // Table 69, a.clear(): erase(a.begin(), a.end()); post:
        // size() == 0; the container takes new elements after
        cont.clear ();

        bool cleared =    cont.empty () && 0 == cont.size ()
                       && cont.begin () == cont.end ();

        fill (cont, "ab", rev);

        cleared = cleared && match (cont, "ab", rev);

        rw_assert (cleared, 0, line,
                   "%s: clear () on \"%s\"", cname, range_rows [i].src);
    }
}

/**************************************************************************/

template <class Container>
void
test_lookup (Container*, const char *cname, bool multi, bool rev)
{
    typedef typename Container::key_compare    Compare;
    typedef typename Container::key_type       Key;
    typedef typename Container::iterator       Iter;
    typedef typename Container::const_iterator ConstIter;

    rw_info (0, 0, __LINE__,
             "std::%s: find, count, lower_bound, upper_bound and "
             "equal_range", cname);

    // the positions are those of the container each kind makes
    static const struct {
        int         line;
        const char *keys;    // the keys of the container
        char        key;     // the key looked up
        int         ucount;  // count, lower_bound and upper_bound
        int         ulower;  // in a unique container
        int         uupper;
        int         mcount;  // the same in a multiple container
        int         mlower;
        int         mupper;
    } rows [] = {
        { __LINE__, "",          'c', 0, 0, 0,  0, 0, 0 },
        { __LINE__, "c",         'b', 0, 0, 0,  0, 0, 0 },
        { __LINE__, "c",         'c', 1, 0, 1,  1, 0, 1 },
        { __LINE__, "c",         'd', 0, 1, 1,  0, 1, 1 },
        { __LINE__, "bddf",      'a', 0, 0, 0,  0, 0, 0 },   // below all
        { __LINE__, "bddf",      'b', 1, 0, 1,  1, 0, 1 },   // the first
        { __LINE__, "bddf",      'c', 0, 1, 1,  0, 1, 1 },   // absent
        { __LINE__, "bddf",      'd', 1, 1, 2,  2, 1, 3 },   // present
        { __LINE__, "bddf",      'e', 0, 2, 2,  0, 3, 3 },   // absent
        { __LINE__, "bddf",      'f', 1, 2, 3,  1, 3, 4 },   // the last
        { __LINE__, "bddf",      'g', 0, 3, 3,  0, 4, 4 },   // above all
        { __LINE__, "aaa",       'a', 1, 0, 1,  3, 0, 3 },
        { __LINE__, "gfedcbaaa", 'a', 1, 0, 1,  3, 0, 3 },
        { __LINE__, "gfedcbaaa", 'd', 1, 3, 4,  1, 5, 6 }
    };

    for (std::size_t i = 0; i != sizeof rows / sizeof *rows; ++i) {
        const int count = multi ? rows [i].mcount : rows [i].ucount;
        const int lower = multi ? rows [i].mlower : rows [i].ulower;
        const int upper = multi ? rows [i].mupper : rows [i].uupper;
        const int line  = rows [i].line;
        const int kval  = key_of (rows [i].key, rev);
        const Key key   = make_key (kval, (Key*)0);

        Container cont (make_compare ((Compare*)0, rev));

        fill (cont, rows [i].keys, rev);

        const Container &ccont = cont;

        // Table 69, a.count(k): the number of elements with a key
        // equivalent to k
        const int n = int (ccont.count (key));

        rw_assert (count == n, 0, line,
                   "%s: count (%c) on \"%s\" == %d, got %d",
                   cname, rows [i].key, rows [i].keys, count, n);

        // Table 69, a.lower_bound(k): the first element with a key
        // not less than k; 23.3.3 declares the const forms alone, the
        // library adds the non-const ones (LWG 214), and the two
        // behave alike
        const int lb  = index_of (cont.begin (), cont.lower_bound (key));
        const int clb = index_of (ccont.begin (), ccont.lower_bound (key));

        rw_assert (lower == lb && lower == clb, 0, line,
                   "%s: lower_bound (%c) on \"%s\" at %d, got %d and "
                   "%d const", cname, rows [i].key, rows [i].keys,
                   lower, lb, clb);

        // Table 69, a.upper_bound(k): the first element with a key
        // greater than k
        const int ub  = index_of (cont.begin (), cont.upper_bound (key));
        const int cub = index_of (ccont.begin (), ccont.upper_bound (key));

        rw_assert (upper == ub && upper == cub, 0, line,
                   "%s: upper_bound (%c) on \"%s\" at %d, got %d and "
                   "%d const", cname, rows [i].key, rows [i].keys,
                   upper, ub, cub);

        // Table 69, a.equal_range(k): make_pair(a.lower_bound(k),
        // a.upper_bound(k))
        const std::pair<Iter, Iter> er = cont.equal_range (key);

        const std::pair<ConstIter, ConstIter> cer = ccont.equal_range (key);

        const int erf  = index_of (cont.begin (), er.first);
        const int ers  = index_of (cont.begin (), er.second);
        const int cerf = index_of (ccont.begin (), cer.first);
        const int cers = index_of (ccont.begin (), cer.second);

        rw_assert (   lower == erf && upper == ers
                   && lower == cerf && upper == cers, 0, line,
                   "%s: equal_range (%c) on \"%s\" at [%d, %d), got "
                   "[%d, %d) and [%d, %d) const", cname, rows [i].key,
                   rows [i].keys, lower, upper, erf, ers, cerf, cers);

        // Table 69, a.find(k): an element with a key equivalent to k,
        // or end () where none is
        const Iter      fit  = cont.find (key);
        const ConstIter cfit = ccont.find (key);

        const int fi  = index_of (cont.begin (), fit);
        const int cfi = index_of (ccont.begin (), cfit);

        bool found;

        if (0 == count)
            found = fit == cont.end () && cfit == ccont.end ();
        else
            found =    lower <= fi && fi < upper && fi == cfi
                    && kval == elem_key (*fit);

        rw_assert (found, 0, line,
                   "%s: find (%c) on \"%s\" at %s, got %d and %d const",
                   cname, rows [i].key, rows [i].keys,
                   count ? "an element of the equal range" : "end ()",
                   fi, cfi);
    }
}

/**************************************************************************/

template <class Container>
void
test_observers (Container*, const char *cname, bool, bool rev)
{
    typedef typename Container::key_compare   Compare;
    typedef typename Container::value_compare ValueCompare;
    typedef typename Container::value_type    Value;

    rw_info (0, 0, __LINE__,
             "std::%s: key_comp, value_comp, size, empty and max_size",
             cname);

    Container cont (make_compare ((Compare*)0, rev));

    // Table 69, a.key_comp(): the comparison object a was
    // constructed out of
    const Compare kc = cont.key_comp ();

    rw_assert (rev == is_reversed (kc), 0, __LINE__,
               "%s: key_comp () is the comparison object given", cname);

    const Value a = make_elem (key_of ('a', rev), 1, (Value*)0);
    const Value b = make_elem (key_of ('b', rev), 2, (Value*)0);

    rw_assert (kc (a, b) && !kc (b, a) && !kc (a, a), 0, __LINE__,
               "%s: key_comp () orders the keys as the container does",
               cname);

    // Table 69, a.value_comp(): an object of value_compare out of the
    // comparison object; the same as key_compare for a set (23.3.3, p2)
    const ValueCompare vc = cont.value_comp ();

    rw_assert (vc (a, b) && !vc (b, a) && !vc (a, a), 0, __LINE__,
               "%s: value_comp () orders the elements by their keys",
               cname);

    // Table 65, a.empty() and a.size(): a.size() == 0 and the number
    // of elements
    bool sizes = cont.empty () && 0 == cont.size ();

    fill (cont, "ba", rev);

    sizes = sizes && !cont.empty () && 2 == cont.size ();

    cont.erase (cont.begin ());

    sizes = sizes && !cont.empty () && 1 == cont.size ();

    rw_assert (sizes, 0, __LINE__,
               "%s: empty () and size () through two inserts and an erase",
               cname);

    // Table 65, a.max_size(): size() of the largest possible
    // container, the same for every container of the type
    const Container other;

    rw_assert (   cont.size () <= cont.max_size () && 0 < other.max_size ()
               && cont.max_size () == other.max_size (), 0, __LINE__,
               "%s: max_size () == %zu, the same for every container",
               cname, cont.max_size ());
}

/**************************************************************************/

template <class Container>
void
test_relational (Container*, const char *cname, bool multi, bool rev)
{
    typedef typename Container::key_compare Compare;

    rw_info (0, 0, __LINE__, "std::%s: ==, !=, <, >, <= and >=", cname);

    // Table 65: == is size() and equal(); < lexicographical_compare()
    // with the operator< of the elements, not the comparison object;
    // with the keys negated for the reversed order the first unequal
    // pair of elements compares the other way, a proper prefix still
    // compares less
    static const struct {
        int         line;
        const char *lhs;     // the keys of the containers compared
        const char *rhs;
        int         usign;   // lhs - rhs for the unique containers
        int         msign;   // lhs - rhs for the multiple containers
        int         urev;    // the same in the reversed order
        int         mrev;
    } rows [] = {
        { __LINE__, "",     "",     0,  0,  0,  0 },
        { __LINE__, "",     "a",   -1, -1, -1, -1 },
        { __LINE__, "a",    "",     1,  1,  1,  1 },
        { __LINE__, "a",    "a",    0,  0,  0,  0 },
        { __LINE__, "a",    "b",   -1, -1,  1,  1 },
        { __LINE__, "abc",  "cba",  0,  0,  0,  0 },
        { __LINE__, "abc",  "abd", -1, -1,  1,  1 },
        { __LINE__, "abd",  "abc",  1,  1, -1, -1 },
        { __LINE__, "ab",   "abc", -1, -1, -1, -1 },
        { __LINE__, "b",    "abc",  1,  1, -1, -1 },
        { __LINE__, "ab",   "abb",  0, -1,  0, -1 },
        { __LINE__, "abbc", "abc",  0, -1,  0,  1 },
        { __LINE__, "abbc", "abbb", 1,  1,  1, -1 }
    };

    for (std::size_t i = 0; i != sizeof rows / sizeof *rows; ++i) {
        const int sign = rev ? multi ? rows [i].mrev  : rows [i].urev
                             : multi ? rows [i].msign : rows [i].usign;
        const int line = rows [i].line;

        Container lhs (make_compare ((Compare*)0, rev));
        Container rhs (make_compare ((Compare*)0, rev));

        fill (lhs, rows [i].lhs, rev);
        fill (rhs, rows [i].rhs, rev);

        const Container &x = lhs;
        const Container &y = rhs;

        // 23.3.3, synopsis: the six non-members
        const bool eq = x == y;
        const bool ne = x != y;
        const bool lt = x <  y;
        const bool gt = x >  y;
        const bool le = x <= y;
        const bool ge = x >= y;

        rw_assert (   eq == (0 == sign) && ne == (0 != sign)
                   && lt == (sign < 0)  && gt == (0 < sign)
                   && le == (sign <= 0) && ge == (0 <= sign),
                   0, line,
                   "%s: \"%s\" against \"%s\": == %b, != %b, < %b, > %b, "
                   "<= %b, >= %b, expected the sign %d", cname,
                   rows [i].lhs, rows [i].rhs, eq, ne, lt, gt, le, ge, sign);
    }
}

/**************************************************************************/

template <class Container>
void
test_swap (Container*, const char *cname, bool, bool rev)
{
    typedef typename Container::key_compare    Compare;
    typedef typename Container::const_iterator ConstIter;

    rw_info (0, 0, __LINE__, "std::%s: swap", cname);

    // the containers swapped and their keys; distinct, so that each
    // kind holds them all
    static const struct {
        int         line;
        const char *lhs;
        const char *rhs;
        const char *lkeys;
        const char *rkeys;
    } rows [] = {
        { __LINE__, "",     "",     "",     ""      },
        { __LINE__, "a",    "",     "a",    ""      },
        { __LINE__, "",     "a",    "",     "a"     },
        { __LINE__, "ba",   "c",    "ab",   "c"     },
        { __LINE__, "dcba", "fgeh", "abcd", "efgh"  }
    };

    // the other order, where the comparison has one: the swap
    // exchanges the comparison objects too
    const bool orev = is_reversed (make_compare ((Compare*)0, !rev));

    for (std::size_t i = 0; i != sizeof rows / sizeof *rows; ++i) {
        const char* const lkeys = rows [i].lkeys;
        const char* const rkeys = rows [i].rkeys;
        const int         line  = rows [i].line;

        Container lhs (make_compare ((Compare*)0, rev));
        Container rhs (make_compare ((Compare*)0, orev));

        fill (lhs, rows [i].lhs, rev);
        fill (rhs, rows [i].rhs, orev);

        const ConstIter lbeg = lhs.begin ();
        const ConstIter rbeg = rhs.begin ();

        // Table 65, a.swap(b)
        lhs.swap (rhs);

        rw_assert (match (lhs, rkeys, orev) && match (rhs, lkeys, rev),
                   0, line,
                   "%s: \"%s\".swap (\"%s\"): keys \"%s\" and ...",
                   cname, lkeys, rkeys, keys_of (lhs, orev));

        rw_assert (   orev == is_reversed (lhs.key_comp ())
                   && rev  == is_reversed (rhs.key_comp ()),
                   0, line,
                   "%s: \"%s\".swap (\"%s\") exchanges the comparison "
                   "objects", cname, lkeys, rkeys);

        // 23.1, p10: no swap() invalidates the iterators to the
        // elements, which now belong to the other container
        bool valid = true;

        if (*lkeys)
            valid = lbeg == ConstIter (rhs.begin ());

        if (*rkeys)
            valid = valid && rbeg == ConstIter (lhs.begin ());

        rw_assert (valid, 0, line,
                   "%s: \"%s\".swap (\"%s\") leaves the iterators valid",
                   cname, lkeys, rkeys);

        // 23.3.3.2 and 23.3.4.2: swap(x, y) is x.swap(y)
        std::swap (lhs, rhs);

        rw_assert (   match (lhs, lkeys, rev) && match (rhs, rkeys, orev)
                   && rev  == is_reversed (lhs.key_comp ())
                   && orev == is_reversed (rhs.key_comp ()),
                   0, line,
                   "%s: swap (\"%s\", \"%s\"): keys \"%s\" and ...",
                   cname, rkeys, lkeys, keys_of (lhs, rev));
    }
}

/**************************************************************************/

// the number of comparisons Table 69 allows a logarithmic operation
// on a container of n elements: it states no constant, so the rows
// take 4 lg (n + 1) + 4, twice the descents of a tree whose depth is
// at most 2 lg (n + 1), for an operation that descends it twice
static std::size_t
log_bound (std::size_t n)
{
    std::size_t lg = 0;

    for (std::size_t m = n + 1; 1 < m; m = (m + 1) / 2)
        ++lg;

    return 4 * lg + 4;
}


// the comparisons per element the rows allow an insertion that is
// amortized constant, and a construction from a sorted range, which
// is a sequence of such insertions, each right after the last
static const std::size_t const_bound = 4;


template <class Container>
void
test_complexity (Container*, const char *cname, bool, bool rev)
{
    typedef typename Container::key_compare    Compare;
    typedef typename Container::key_type       Key;
    typedef typename Container::value_type     Value;
    typedef typename Container::iterator       Iter;
    typedef typename Container::const_iterator ConstIter;

    std::size_t* const ncalls = call_counter ((Compare*)0);

    if (0 == ncalls)
        return;

    rw_info (0, 0, __LINE__, "std::%s: the comparisons of the members "
             "whose complexity Table 69 states", cname);

    static const int sizes [] = { 1, 15, 255, 4095 };

    for (std::size_t i = 0; i != sizeof sizes / sizeof *sizes; ++i) {
        const int         n     = sizes [i];
        const std::size_t bound = log_bound (std::size_t (n));

        // the keys 0, 2, ... 2 * (n - 1), so that the odd ones are
        // absent; -1 lies below them all and 2 * n - 1 above
        Container cont (make_compare ((Compare*)0, rev));

        for (int k = 0; k != n; ++k)
            cont.insert (make_elem (key_of (2 * k, rev), k + 1, (Value*)0));

        const Container &ccont = cont;

        // the most comparisons each lookup made over the keys
        std::size_t most [9] = { 0, 0, 0, 0, 0, 0, 0, 0, 0 };

        for (int k = -1; k != 2 * n; ++k) {
            const Key key = make_key (key_of (k, rev), (Key*)0);

            std::size_t calls [9];

            *ncalls = 0;
            cont.find (key);
            calls [0] = *ncalls;

            *ncalls = 0;
            ccont.find (key);
            calls [1] = *ncalls;

            *ncalls = 0;
            cont.lower_bound (key);
            calls [2] = *ncalls;

            *ncalls = 0;
            ccont.lower_bound (key);
            calls [3] = *ncalls;

            *ncalls = 0;
            cont.upper_bound (key);
            calls [4] = *ncalls;

            *ncalls = 0;
            ccont.upper_bound (key);
            calls [5] = *ncalls;

            *ncalls = 0;
            cont.equal_range (key);
            calls [6] = *ncalls;

            *ncalls = 0;
            ccont.equal_range (key);
            calls [7] = *ncalls;

            // Table 69, a.count(k): log(size()) + count(k)
            *ncalls = 0;
            const std::size_t count = ccont.count (key);
            calls [8] = *ncalls - count;

            for (int j = 0; j != 9; ++j) {
                if (most [j] < calls [j])
                    most [j] = calls [j];
            }
        }

        static const char* const names [] = {
            "find", "find const", "lower_bound", "lower_bound const",
            "upper_bound", "upper_bound const", "equal_range",
            "equal_range const", "count"
        };

        // Table 69: find, lower_bound, upper_bound and equal_range
        // are logarithmic, count log(size()) + count(k)
        for (int j = 0; j != 9; ++j) {
            rw_assert (most [j] <= bound, 0, __LINE__,
                       "%s: %s on %d elements made %zu comparisons, "
                       "more than %zu", cname, names [j], n, most [j],
                       bound);
        }

        // Table 69, a_uniq.insert(t) and a_eq.insert(t): logarithmic;
        // a.erase(k): log(size()) + count(k); each absent key is
        // inserted and erased again
        std::size_t most_insert = 0;
        std::size_t most_erase  = 0;

        for (int k = 1; k < 2 * n; k += 2) {
            const Key key = make_key (key_of (k, rev), (Key*)0);

            *ncalls = 0;
            cont.insert (make_elem (key_of (k, rev), 0, (Value*)0));

            if (most_insert < *ncalls)
                most_insert = *ncalls;

            *ncalls = 0;
            const std::size_t count = cont.erase (key);

            if (most_erase < *ncalls - count)
                most_erase = *ncalls - count;
        }

        rw_assert (most_insert <= bound, 0, __LINE__,
                   "%s: insert (t) on %d elements made %zu comparisons, "
                   "more than %zu", cname, n, most_insert, bound);

        rw_assert (most_erase <= bound, 0, __LINE__,
                   "%s: erase (k) on %d elements made %zu comparisons "
                   "beyond count (k), more than %zu", cname, n, most_erase,
                   bound);

        // 23.3.3.1, p4 and 23.3.4.1, p4; Table 69, X(i, j, c): linear
        // in N if the range is already sorted using comp
        ElemArray<Value> sorted ((std::size_t (n)));

        for (int k = 0; k != n; ++k)
            sorted.push (key_of (k, rev), k + 1);

        *ncalls = 0;

        const Container x (sorted.begin (), sorted.end (),
                           make_compare ((Compare*)0, rev));

        const std::size_t ncons = *ncalls;

        rw_assert (   std::size_t (n) == x.size ()
                   && ncons <= const_bound * std::size_t (n),
                   0, __LINE__,
                   "%s: X(i, j, c) from %d sorted elements made %zu "
                   "comparisons, more than %zu", cname, n, ncons,
                   const_bound * std::size_t (n));

        // Table 69, a.insert(p, t): amortized constant if t is
        // inserted right after p; each key after the last one
        Container h (make_compare ((Compare*)0, rev));

        Iter hint = h.end ();

        *ncalls = 0;

        for (int k = 0; k != n; ++k)
            hint = h.insert (hint, make_elem (key_of (k, rev), 0, (Value*)0));

        const std::size_t nlast = *ncalls;

        rw_assert (   std::size_t (n) == h.size ()
                   && nlast <= const_bound * std::size_t (n),
                   0, __LINE__,
                   "%s: insert (p, t) of %d keys each right after the "
                   "last made %zu comparisons, more than %zu", cname, n,
                   nlast, const_bound * std::size_t (n));

        // ... and each odd key right after the even one below it, so
        // that most go between two elements
        *ncalls = 0;

        for (Iter it = cont.begin (); !(it == cont.end ()); ++it) {
            const int k = key_of (elem_key (*it), rev);

            it = cont.insert (it, make_elem (key_of (k + 1, rev), 0,
                                             (Value*)0));
        }

        const std::size_t nmid = *ncalls;

        rw_assert (   2 * std::size_t (n) == cont.size ()
                   && nmid <= const_bound * std::size_t (n),
                   0, __LINE__,
                   "%s: insert (p, t) of %d keys each right after the "
                   "one below made %zu comparisons, more than %zu", cname,
                   n, nmid, const_bound * std::size_t (n));

        // the elements are in order after it all
        bool ordered = true;

        int k = 0;

        for (ConstIter it = ccont.begin (); ordered && !(it == ccont.end ());
             ++it, ++k)
            ordered = key_of (k, rev) == elem_key (*it);

        rw_assert (ordered, 0, __LINE__,
                   "%s: the keys 0 to %d in order after the hinted "
                   "insertions", cname, 2 * n - 1);
    }
}

/**************************************************************************/

template <class Container>
void
test_container (Container *dummy, const char *cname, bool multi, bool rev)
{
    test_cons (dummy, cname, multi, rev);
    test_insert (dummy, cname, multi, rev);
    test_insert_hint (dummy, cname, multi, rev);
    test_insert_range (dummy, cname, multi, rev);
    test_erase (dummy, cname, multi, rev);
    test_lookup (dummy, cname, multi, rev);
    test_observers (dummy, cname, multi, rev);
    test_relational (dummy, cname, multi, rev);
    test_swap (dummy, cname, multi, rev);
    test_complexity (dummy, cname, multi, rev);
}

/**************************************************************************/

typedef CountingCompare<int>       IntCompare;
typedef CountingCompare<UserClass> UserCompare;

static int
run_test (int, char**)
{
    test_container ((std::set<int>*)0, "set<int>", false, false);
    test_container ((std::set<int, IntCompare>*)0,
                    "set<int, Compare>", false, false);
    test_container ((std::set<int, IntCompare>*)0,
                    "set<int, Compare> reversed", false, true);
    test_container ((std::set<UserClass>*)0, "set<UserClass>", false, false);
    test_container ((std::set<UserClass, UserCompare>*)0,
                    "set<UserClass, Compare>", false, false);
    test_container ((std::set<UserClass, UserCompare>*)0,
                    "set<UserClass, Compare> reversed", false, true);

    test_container ((std::multiset<int>*)0, "multiset<int>", true, false);
    test_container ((std::multiset<int, IntCompare>*)0,
                    "multiset<int, Compare>", true, false);
    test_container ((std::multiset<int, IntCompare>*)0,
                    "multiset<int, Compare> reversed", true, true);
    test_container ((std::multiset<UserClass>*)0,
                    "multiset<UserClass>", true, false);
    test_container ((std::multiset<UserClass, UserCompare>*)0,
                    "multiset<UserClass, Compare>", true, false);
    test_container ((std::multiset<UserClass, UserCompare>*)0,
                    "multiset<UserClass, Compare> reversed", true, true);

    return 0;
}

/**************************************************************************/

int main (int argc, char** argv)
{
    return rw_test (argc, argv, __FILE__,
                    "lib.set",
                    0 /* no comment */,
                    run_test,
                    0 /* no command line options */);
}
