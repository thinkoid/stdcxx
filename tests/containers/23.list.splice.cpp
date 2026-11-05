/***************************************************************************
 *
 * 23.list.splice.cpp - test exercising list::splice and list::merge
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

#include <list>         // for list
#include <cstddef>      // for size_t
#include <cstring>      // for strlen

#include <rw_driver.h>  // for rw_test(), rw_assert()

/**************************************************************************/

// counts the copies and destructions a list makes of its elements;
// a copy throws while throw_on_copy is set
struct Value
{
    static int  copies;
    static int  destroyed;
    static bool throw_on_copy;

    char data;

    Value (char c): data (c) { }

    Value (const Value &rhs): data (rhs.data) {
        ++copies;

        if (throw_on_copy)
            throw copies;
    }

    ~Value () { ++destroyed; }
};

int  Value::copies;
int  Value::destroyed;
bool Value::throw_on_copy;

bool operator< (const Value &x, const Value &y)
{
    return x.data < y.data;
}

struct Less
{
    bool operator() (const Value &x, const Value &y) const {
        return x.data < y.data;
    }
};

typedef std::list<Value> List;

/**************************************************************************/

static void
fill (List &lst, const char *str)
{
    for ( ; *str; ++str)
        lst.push_back (Value (*str));
}


static List::iterator
at (List &lst, std::size_t n)
{
    List::iterator it = lst.begin ();

    while (n--)
        ++it;

    return it;
}


static void
reset_counts ()
{
    Value::copies    = 0;
    Value::destroyed = 0;
}


static void
check_counts (int line, const char *what)
{
    rw_assert (0 == Value::copies && 0 == Value::destroyed, 0, line,
               "%s: %d copies and %d destructions, expected none",
               what, Value::copies, Value::destroyed);
}


// the size of lst and its elements in order are those of str
static void
check_list (int line, const char *what, const List &lst, const char *str)
{
    const std::size_t len = std::strlen (str);

    bool same = lst.size () == len;

    List::const_iterator it = lst.begin ();

    for (std::size_t i = 0; same && i != len; ++i, ++it)
        same = it->data == str [i];

    rw_assert (same && it == lst.end (), 0, line,
               "%s: expected \"%s\" of size %zu, got size %zu",
               what, str, len, lst.size ());
}

/**************************************************************************/

static void
test_splice_list ()
{
    rw_info (0, 0, 0, "std::list<Value>::splice (iterator, list&)");

    List dst;
    List src;

    fill (dst, "abc");
    fill (src, "xyz");

    const List::iterator y = ++src.begin ();

    reset_counts ();
    dst.splice (++dst.begin (), src);

    check_counts (__LINE__, "splice (it, x)");
    check_list (__LINE__, "splice (it, x), destination", dst, "axyzbc");
    check_list (__LINE__, "splice (it, x), source", src, "");

    rw_assert ('y' == y->data && &*y == &*at (dst, 2), 0, __LINE__,
               "splice (it, x): an iterator to a spliced element "
               "refers to it in its new place");
}


static void
test_splice_element ()
{
    rw_info (0, 0, 0, "std::list<Value>::splice (iterator, list&, iterator)");

    List dst;
    List src;

    fill (dst, "abc");
    fill (src, "xyz");

    const List::iterator y = ++src.begin ();

    reset_counts ();
    dst.splice (dst.end (), src, y);

    check_counts (__LINE__, "splice (it, x, i)");
    check_list (__LINE__, "splice (it, x, i), destination", dst, "abcy");
    check_list (__LINE__, "splice (it, x, i), source", src, "xz");

    rw_assert ('y' == y->data && &*y == &dst.back (), 0, __LINE__,
               "splice (it, x, i): an iterator to the spliced element "
               "refers to it in its new place");
}


static void
test_splice_range ()
{
    rw_info (0, 0, 0, "std::list<Value>::splice "
             "(iterator, list&, iterator, iterator)");

    List dst;
    List src;

    fill (dst, "abc");
    fill (src, "vwxyz");

    const List::iterator w = ++src.begin ();

    reset_counts ();
    dst.splice (dst.begin (), src, w, --src.end ());

    check_counts (__LINE__, "splice (it, x, first, last)");
    check_list (__LINE__, "splice (it, x, first, last), destination",
                dst, "wxyabc");
    check_list (__LINE__, "splice (it, x, first, last), source", src, "vz");

    rw_assert ('w' == w->data && &*w == &dst.front (), 0, __LINE__,
               "splice (it, x, first, last): an iterator to a spliced "
               "element refers to it in its new place");
}


static void
test_merge ()
{
    rw_info (0, 0, 0, "std::list<Value>::merge (list&)");

    List dst;
    List src;

    fill (dst, "aceg");
    fill (src, "bdfh");

    const List::iterator d = ++src.begin ();

    reset_counts ();
    dst.merge (src);

    check_counts (__LINE__, "merge (x)");
    check_list (__LINE__, "merge (x), destination", dst, "abcdefgh");
    check_list (__LINE__, "merge (x), source", src, "");

    rw_assert ('d' == d->data && &*d == &*at (dst, 3), 0, __LINE__,
               "merge (x): an iterator to a merged element "
               "refers to it in its new place");
}


static void
test_merge_compare ()
{
    rw_info (0, 0, 0, "std::list<Value>::merge (list&, Less)");

    List dst;
    List src;

    fill (dst, "adg");
    fill (src, "bcefhi");

    const List::iterator h = at (src, 4);

    reset_counts ();
    dst.merge (src, Less ());

    check_counts (__LINE__, "merge (x, comp)");
    check_list (__LINE__, "merge (x, comp), destination", dst, "abcdefghi");
    check_list (__LINE__, "merge (x, comp), source", src, "");

    rw_assert ('h' == h->data && &*h == &*at (dst, 7), 0, __LINE__,
               "merge (x, comp): an iterator to a merged element "
               "refers to it in its new place");
}


static void
test_splice_within ()
{
    rw_info (0, 0, 0, "std::list<Value>::splice within one list");

    List lst;

    fill (lst, "abcde");

    const List::iterator e = --lst.end ();

    reset_counts ();
    lst.splice (lst.begin (), lst, e);

    check_counts (__LINE__, "splice (it, *this, i)");
    check_list (__LINE__, "splice (it, *this, i)", lst, "eabcd");

    rw_assert (&*e == &lst.front (), 0, __LINE__,
               "splice (it, *this, i): an iterator to the spliced "
               "element refers to it in its new place");

    lst.clear ();
    fill (lst, "abcdef");

    const List::iterator d = at (lst, 3);

    reset_counts ();
    lst.splice (lst.begin (), lst, d, lst.end ());

    check_counts (__LINE__, "splice (it, *this, first, last)");
    check_list (__LINE__, "splice (it, *this, first, last)", lst, "defabc");

    rw_assert (&*d == &lst.front (), 0, __LINE__,
               "splice (it, *this, first, last): an iterator to a "
               "spliced element refers to it in its new place");
}


static void
test_throwing_copy ()
{
    rw_info (0, 0, 0, "std::list<Value>::splice and merge "
             "with a copy constructor that throws");

    List dst;
    List src;

    fill (dst, "aceg");
    fill (src, "bdfhxyz");

    reset_counts ();
    Value::throw_on_copy = true;

    bool thrown = false;

    try {
        List tmp;

        tmp.splice (tmp.end (), src, at (src, 4), src.end ());
        dst.merge (src);
        dst.splice (dst.end (), tmp, tmp.begin ());
        dst.splice (dst.end (), tmp);
    }
    catch (int) {
        thrown = true;
    }

    Value::throw_on_copy = false;

    rw_assert (!thrown, 0, __LINE__,
               "splice and merge invoked a copy constructor that throws");

    check_list (__LINE__, "splice and merge, destination", dst, "abcdefghxyz");
    check_list (__LINE__, "splice and merge, source", src, "");
}

/**************************************************************************/

static int
run_test (int, char**)
{
    test_splice_list ();
    test_splice_element ();
    test_splice_range ();
    test_merge ();
    test_merge_compare ();
    test_splice_within ();
    test_throwing_copy ();

    return 0;
}

/**************************************************************************/

int main (int argc, char** argv)
{
    return rw_test (argc, argv, __FILE__,
                    "lib.list.ops",
                    0 /* no comment */,
                    run_test,
                    0 /* no command line options */);
}
