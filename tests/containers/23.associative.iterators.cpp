/***************************************************************************
 *
 * 23.associative.iterators.cpp - test exercising the iterators of
 *                                [lib.associative]
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

#include <map>              // for map, multimap
#include <set>              // for set, multiset
#include <utility>          // for pair

#include <rw_driver.h>      // for rw_test(), rw_assert()

/**************************************************************************/

typedef std::pair<const int, int> Pair;

// the orders the keys 0 to nkeys - 1 are inserted in; each gives the
// tree another shape on the way to the same sequence
enum Order { Ascending, Descending, Scattered, NOrders };

static const char* const order_names [] = {
    "ascending", "descending", "scattered"
};

/**************************************************************************/

// the key of an element of a set and of a map
static int
key_of (int elem)
{
    return elem;
}

static int
key_of (const Pair &elem)
{
    return elem.first;
}


// the element of a set and of a map that holds key
static int
elem_of (int key, const int*)
{
    return key;
}

static Pair
elem_of (int key, const Pair*)
{
    return Pair (key, -key);
}


// the i-th of nkeys keys in the given order
static int
nth_key (int i, int nkeys, Order order)
{
    if (Ascending == order)
        return i;

    if (Descending == order)
        return nkeys - 1 - i;

    // 5 has no factor in common with any number of keys tested
    return i * 5 % nkeys;
}

/**************************************************************************/

// walks from first up to last by pre-increment; returns the number of
// steps when there are nelems of them and the keys met are 0, step,
// 2 * step and so on, each ndups times; returns -1 otherwise
template <class Iterator>
int
walk_up (Iterator first, Iterator last, int nelems, int ndups, int step)
{
    int n = 0;

    for ( ; !(first == last); ++first, ++n) {
        if (n == nelems || key_of (*first) != n / ndups * step)
            return -1;
    }

    return n;
}


// walks from last down to first by pre-decrement and returns
// what walk_up () does for the same sequence
template <class Iterator>
int
walk_down (Iterator first, Iterator last, int nelems, int ndups, int step)
{
    int n = nelems;

    while (!(last == first)) {
        if (0 == n)
            return -1;

        --last;
        --n;

        if (key_of (*last) != n / ndups * step)
            return -1;
    }

    return nelems - n;
}


// walks a range of reverse iterators from rbegin () to rend () and
// returns what walk_up () does for the same sequence
template <class Iterator>
int
walk_reverse (Iterator first, Iterator last, int nelems, int ndups, int step)
{
    int n = nelems;

    for ( ; !(first == last); ++first) {
        if (0 == n)
            return -1;

        --n;

        if (key_of (*first) != n / ndups * step)
            return -1;
    }

    return nelems - n;
}

/**************************************************************************/

// fills a container with nkeys keys, each inserted ndups times in the
// given order, and walks it with each of its iterator types; multi
// says whether the container keeps the equal keys
template <class Container>
void
test_walk (Container*, const char *cname, bool multi,
           int nkeys, int ndups, Order order)
{
    typedef typename Container::value_type             Value;
    typedef typename Container::iterator               Iter;
    typedef typename Container::const_iterator         ConstIter;
    typedef typename Container::reverse_iterator       RevIter;
    typedef typename Container::const_reverse_iterator ConstRevIter;

    Container cont;

    for (int dup = 0; dup != ndups; ++dup) {
        for (int i = 0; i != nkeys; ++i)
            cont.insert (elem_of (nth_key (i, nkeys, order), (Value*)0));
    }

    const Container &ccont = cont;

    // the number of times a key is met, and the number of elements
    const int nmet   = multi ? ndups : 1;
    int       nelems = nkeys * nmet;

    const char* const oname = order_names [order];

    rw_assert (int (cont.size ()) == nelems, 0, __LINE__,
               "%s, %d keys %d times %s: size () == %d, got %d",
               cname, nkeys, ndups, oname, nelems, int (cont.size ()));

    int nsteps = walk_up (cont.begin (), cont.end (), nelems, nmet, 1);

    rw_assert (nsteps == nelems, 0, __LINE__,
               "%s, %d keys %d times %s: iterator, begin () up to end ()",
               cname, nkeys, ndups, oname);

    nsteps = walk_down (cont.begin (), cont.end (), nelems, nmet, 1);

    rw_assert (nsteps == nelems, 0, __LINE__,
               "%s, %d keys %d times %s: iterator, end () down to begin ()",
               cname, nkeys, ndups, oname);

    nsteps = walk_up (ccont.begin (), ccont.end (), nelems, nmet, 1);

    if (nsteps == nelems)
        nsteps = walk_down (ccont.begin (), ccont.end (), nelems, nmet, 1);

    rw_assert (nsteps == nelems, 0, __LINE__,
               "%s, %d keys %d times %s: const_iterator, up and down",
               cname, nkeys, ndups, oname);

    const RevIter      rbeg  = cont.rbegin ();
    const RevIter      rend  = cont.rend ();
    const ConstRevIter crbeg = ccont.rbegin ();
    const ConstRevIter crend = ccont.rend ();

    nsteps = walk_reverse (rbeg, rend, nelems, nmet, 1);

    if (nsteps == nelems)
        nsteps = walk_reverse (crbeg, crend, nelems, nmet, 1);

    rw_assert (nsteps == nelems, 0, __LINE__,
               "%s, %d keys %d times %s: reverse_iterator and "
               "const_reverse_iterator, rbegin () to rend ()",
               cname, nkeys, ndups, oname);

    // post-increment and post-decrement return the position they left
    bool same = true;

    for (Iter it = cont.begin (); same && !(it == cont.end ()); ) {
        const Iter here = it;

        Iter next = here;
        ++next;

        const Iter left_up = it++;

        same = left_up == here && it == next;

        const Iter left_down = it--;

        same = same && left_down == next && it == here;

        ++it;
    }

    rw_assert (same, 0, __LINE__,
               "%s, %d keys %d times %s: it++ and it-- return the "
               "position they left",
               cname, nkeys, ndups, oname);

    // the iterator converts to a const_iterator (23.1, Table 65) and
    // compares with it, either way round, at every position
    same = true;

    ConstIter cit = ccont.begin ();

    for (Iter it = cont.begin (); same; ++it, ++cit) {
        const ConstIter conv = it;

        same =    conv == cit
               && it == cit && cit == it && !(it != cit) && !(cit != it);

        if (it == cont.end ())
            break;

        const ConstIter cend = cont.end ();

        same = same && it != cend && cend != it && !(it == cend);
    }

    rw_assert (same, 0, __LINE__,
               "%s, %d keys %d times %s: iterator converts to "
               "const_iterator and compares with it",
               cname, nkeys, ndups, oname);

    // without the odd keys the tree has another shape again
    for (int key = 1; key < nkeys; key += 2)
        cont.erase (key);

    nelems = (nkeys + 1) / 2 * nmet;

    nsteps = walk_up (cont.begin (), cont.end (), nelems, nmet, 2);

    if (nsteps == nelems)
        nsteps = walk_down (cont.begin (), cont.end (), nelems, nmet, 2);

    rw_assert (nsteps == nelems, 0, __LINE__,
               "%s, %d keys %d times %s: iterator, up and down "
               "after erasing the odd keys",
               cname, nkeys, ndups, oname);
}

/**************************************************************************/

template <class Container>
void
test_container (Container *dummy, const char *cname, bool multi)
{
    // from the empty tree to one seven levels deep
    static const int sizes [] = { 0, 1, 2, 3, 7, 31, 127 };

    rw_info (0, 0, __LINE__, "iterators of std::%s", cname);

    for (unsigned i = 0; i != sizeof sizes / sizeof *sizes; ++i) {
        for (int order = 0; order != NOrders; ++order) {
            test_walk (dummy, cname, multi, sizes [i], 1, Order (order));
            test_walk (dummy, cname, multi, sizes [i], 2, Order (order));
        }
    }
}

/**************************************************************************/

static int
run_test (int, char**)
{
    test_container ((std::set<int>*)0, "set<int>", false);
    test_container ((std::multiset<int>*)0, "multiset<int>", true);
    test_container ((std::map<int, int>*)0, "map<int, int>", false);
    test_container ((std::multimap<int, int>*)0, "multimap<int, int>", true);

    return 0;
}

/**************************************************************************/

int main (int argc, char** argv)
{
    return rw_test (argc, argv, __FILE__,
                    "lib.associative",
                    0 /* no comment */,
                    run_test,
                    0 /* no command line options */);
}
