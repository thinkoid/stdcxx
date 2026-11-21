/***************************************************************************
 *
 * 23.list.ops.cpp - test exercising list::remove, remove_if, unique,
 *                   sort and reverse
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

#include <list>             // for list
#include <cstddef>          // for size_t

#include <rw_lists.h>       // for ListIds
#include <rw_driver.h>      // for rw_test(), rw_assert()
#include <rw_allocator.h>   // for UserAlloc

/**************************************************************************/

// the number of calls made to the predicates and comparisons below
// since the last reset; the call that brings it to throw_at throws
static std::size_t n_calls;
static std::size_t throw_at = std::size_t (-1);

// thrown by the predicates and comparisons below
struct CallThrown { };


static void
count_call ()
{
    if (++n_calls == throw_at)
        throw CallThrown ();
}


// remove (const_reference), unique () and sort () compare UserPOD
// with == and <, which it does not define
bool operator== (const UserPOD &x, const UserPOD &y)
{
    count_call ();

    return x.data_.val_ == y.data_.val_;
}


bool operator< (const UserPOD &x, const UserPOD &y)
{
    count_call ();

    return x.data_.val_ < y.data_.val_;
}


// the two cases of a letter fold to one value
static int
fold (int c)
{
    return 'A' <= c && c <= 'Z' ? c - 'A' + 'a' : c;
}


// the predicate of remove_if: true for the elements of one value
template <class T>
struct EqualTo
{
    int val_;

    EqualTo (int val): val_ (val) { }

    bool operator() (const T &x) const {
        count_call ();

        return x.data_.val_ == val_;
    }
};


// the predicate of unique: an equivalence coarser than equality,
// so that the result shows which element of a group stays
template <class T>
struct SameLetter
{
    bool operator() (const T &x, const T &y) const {
        count_call ();

        return fold (x.data_.val_) == fold (y.data_.val_);
    }
};


// the comparison of sort: a strict weak ordering under which the
// two cases of a letter are equivalent, so that the result shows
// whether equivalent elements keep their order
template <class T>
struct LessLetter
{
    bool operator() (const T &x, const T &y) const {
        count_call ();

        return fold (x.data_.val_) < fold (y.data_.val_);
    }
};

/**************************************************************************/

// used to exercise
// remove (const_reference) and remove_if (Predicate)
static const ContainerTestCase
remove_test_cases [] = {

#undef TEST
#define TEST(str, val, res, bthrow) {                                   \
        __LINE__, NPOS, NPOS, NPOS, NPOS, val, str, sizeof str - 1,     \
        0, 0, res, sizeof res - 1, bthrow }

    // 23.2.2.4, p15: the elements of the value go; p17: the others
    // keep their order

    //    +----------------------------------------- controlled sequence
    //    |                +------------------------ value to remove
    //    |                |     +------------------ expected result sequence
    //    |                |     |               +-- exception info
    //    |                |     |               |      0 - no exception
    //    |                |     |               |     -1 - exc. safety
    //    |                |     |               |
    //    V                V     V               V
    TEST ("",              'a',  "",             0),
    TEST ("a",             'a',  "",             0),
    TEST ("a",             'b',  "a",            0),
    TEST ("ab",            'a',  "b",            0),
    TEST ("ab",            'b',  "a",            0),
    TEST ("aaa",           'a',  "",             0),
    TEST ("abc",           'b',  "ac",           0),
    TEST ("abcd",          'e',  "abcd",         0),
    TEST ("abacada",       'a',  "bcd",          0),
    TEST ("babcbdb",       'b',  "acd",          0),
    TEST ("aAaA",          'a',  "AA",           0),
    TEST ("<U0>a<U0>b",    '\0', "ab",           0),
    TEST ("x@4096",        'x',  "",             0),
    TEST ("x@2048y@2048",  'x',  "y@2048",       0),
    TEST ("x@2048y@2048",  'y',  "x@2048",       0),

    // p16: an exception from a comparison or the predicate
    // propagates
    TEST ("abacada",       'a',  "bcd",         -1),
    TEST ("babcbdb",       'b',  "acd",         -1),

    TEST ("last test",     't',  "las es",       0)
};

/**************************************************************************/

// used to exercise
// unique ()
static const ContainerTestCase
unique_void_test_cases [] = {

#undef TEST
#define TEST(str, res, bthrow) {                                        \
        __LINE__, NPOS, NPOS, NPOS, NPOS, -1, str, sizeof str - 1,      \
        0, 0, res, sizeof res - 1, bthrow }

    // 23.2.2.4, p19: the first element of each group of consecutive
    // equal elements stays, the others go

    //    +----------------------------------------- controlled sequence
    //    |                +------------------------ expected result sequence
    //    |                |               +-------- exception info
    //    |                |               |            0 - no exception
    //    |                |               |           -1 - exc. safety
    //    |                |               |
    //    V                V               V
    TEST ("",              "",             0),
    TEST ("a",             "a",            0),
    TEST ("aa",            "a",            0),
    TEST ("ab",            "ab",           0),
    TEST ("aab",           "ab",           0),
    TEST ("abb",           "ab",           0),
    TEST ("aba",           "aba",          0),
    TEST ("aabbaa",        "aba",          0),
    TEST ("abcabc",        "abcabc",       0),
    TEST ("aaabbbccc",     "abc",          0),
    TEST ("aAaA",          "aAaA",         0),
    TEST ("aaAA",          "aA",           0),
    TEST ("<U0>@3a<U0>",   "<U0>a<U0>",    0),
    TEST ("x@4096",        "x",            0),
    TEST ("x@2048y@2048",  "xy",           0),

    // p20: an exception from a comparison propagates
    TEST ("aabbaa",        "aba",         -1),
    TEST ("aaabbbccc",     "abc",         -1),

    TEST ("last  test",    "last test",    0)
};

/**************************************************************************/

// used to exercise
// unique (BinaryPredicate)
static const ContainerTestCase
unique_test_cases [] = {

#undef TEST
#define TEST(str, res, bthrow) {                                        \
        __LINE__, NPOS, NPOS, NPOS, NPOS, -1, str, sizeof str - 1,      \
        0, 0, res, sizeof res - 1, bthrow }

    // 23.2.2.4, p19: the first element of each group of consecutive
    // equivalent elements stays, the others go; the predicate takes
    // the two cases of a letter as equivalent

    //    +----------------------------------------- controlled sequence
    //    |                +------------------------ expected result sequence
    //    |                |               +-------- exception info
    //    |                |               |            0 - no exception
    //    |                |               |           -1 - exc. safety
    //    |                |               |
    //    V                V               V
    TEST ("",              "",             0),
    TEST ("a",             "a",            0),
    TEST ("aa",            "a",            0),
    TEST ("aA",            "a",            0),
    TEST ("Aa",            "A",            0),
    TEST ("ab",            "ab",           0),
    TEST ("aba",           "aba",          0),
    TEST ("abba",          "aba",          0),
    TEST ("aAbBa",         "aba",          0),
    TEST ("AaaBbbAaa",     "ABA",          0),
    TEST ("abcabc",        "abcabc",       0),
    TEST ("aaabbbccc",     "abc",          0),
    TEST ("<U0>@3a<U0>",   "<U0>a<U0>",    0),
    TEST ("x@4096",        "x",            0),
    TEST ("x@2048X@2048",  "x",            0),
    TEST ("x@2048y@2048",  "xy",           0),

    // p20: an exception from the predicate propagates
    TEST ("aAbBa",         "aba",         -1),
    TEST ("AaaBbbAaa",     "ABA",         -1),

    TEST ("last  test",    "last test",    0)
};

/**************************************************************************/

// used to exercise
// sort ()
static const ContainerTestCase
sort_test_cases [] = {

#undef TEST
#define TEST(str, res, bthrow) {                                        \
        __LINE__, NPOS, NPOS, NPOS, NPOS, -1, str, sizeof str - 1,      \
        0, 0, res, sizeof res - 1, bthrow }

    // 23.2.2.4, p30: the elements in the order of operator<; p31:
    // equal elements in their order before the call

    //    +----------------------------------------- controlled sequence
    //    |                +------------------------ expected result sequence
    //    |                |               +-------- exception info
    //    |                |               |            0 - no exception
    //    |                |               |           -1 - exc. safety
    //    |                |               |
    //    V                V               V
    TEST ("",              "",             0),
    TEST ("a",             "a",            0),
    TEST ("ab",            "ab",           0),
    TEST ("ba",            "ab",           0),
    TEST ("aa",            "aa",           0),
    TEST ("abc",           "abc",          0),
    TEST ("acb",           "abc",          0),
    TEST ("bac",           "abc",          0),
    TEST ("bca",           "abc",          0),
    TEST ("cab",           "abc",          0),
    TEST ("cba",           "abc",          0),
    TEST ("abcdefgh",      "abcdefgh",     0),
    TEST ("hgfedcba",      "abcdefgh",     0),
    TEST ("abababab",      "aaaabbbb",     0),
    TEST ("bbbbaaaa",      "aaaabbbb",     0),
    TEST ("cbacbacba",     "aaabbbccc",    0),
    TEST ("aAbB",          "ABab",         0),
    TEST ("<U0>b<U0>a",    "<U0>@2ab",     0),

    TEST ("the quick brown fox jumps over the lazy dog",
          "        abcdeeefghhijklmnoooopqrrsttuuvwxyz",      0),

    TEST ("dcbadcbadcbadcbadcbadcbadcbadcba"
          "dcbadcbadcbadcbadcbadcbadcbadcba",
          "a@16b@16c@16d@16",                                 0),

    TEST ("x@4096",                     "x@4096",             0),
    TEST ("y@2048x@2048",               "x@2048y@2048",       0),
    TEST ("x@2047yx@2048",              "x@4095y",            0),
    TEST ("d@1024c@1024b@1024a@1024",   "a@1024b@1024c@1024d@1024", 0),

    // p31: an exception from a comparison propagates and leaves
    // the elements in the list, in an indeterminate order
    TEST ("cba",           "abc",         -1),
    TEST ("hgfedcba",      "abcdefgh",    -1),
    TEST ("cbacbacba",     "aaabbbccc",   -1),

    TEST ("last test",     " aelssttt",    0)
};

/**************************************************************************/

// used to exercise
// sort (Compare)
static const ContainerTestCase
sort_comp_test_cases [] = {

#undef TEST
#define TEST(str, res, bthrow) {                                        \
        __LINE__, NPOS, NPOS, NPOS, NPOS, -1, str, sizeof str - 1,      \
        0, 0, res, sizeof res - 1, bthrow }

    // 23.2.2.4, p30: the elements in the order of the comparison,
    // which takes the two cases of a letter as equivalent; p31:
    // equivalent elements in their order before the call

    //    +----------------------------------------- controlled sequence
    //    |                +------------------------ expected result sequence
    //    |                |               +-------- exception info
    //    |                |               |            0 - no exception
    //    |                |               |           -1 - exc. safety
    //    |                |               |
    //    V                V               V
    TEST ("",              "",             0),
    TEST ("a",             "a",            0),
    TEST ("ab",            "ab",           0),
    TEST ("Ba",            "aB",           0),
    TEST ("aA",            "aA",           0),
    TEST ("Aa",            "Aa",           0),
    TEST ("bBaA",          "aAbB",         0),
    TEST ("BbAa",          "AaBb",         0),
    TEST ("bAcaBC",        "AabBcC",       0),
    TEST ("cbaCBA",        "aAbBcC",       0),
    TEST ("CBAcba",        "AaBbCc",       0),

    TEST ("The Quick Brown Fox",        "   BceFhiknooQrTuwx",    0),

    TEST ("x@2048X@2048",               "x@2048X@2048",           0),
    TEST ("X@2048x@2048",               "X@2048x@2048",           0),
    TEST ("y@1024X@1024Y@1024x@1024",   "X@1024x@1024y@1024Y@1024", 0),

    // p31: an exception from the comparison propagates and leaves
    // the elements in the list, in an indeterminate order
    TEST ("bBaA",          "aAbB",        -1),
    TEST ("bAcaBC",        "AabBcC",      -1),

    TEST ("Last Test",     " aeLsstTt",    0)
};

/**************************************************************************/

// used to exercise
// reverse ()
static const ContainerTestCase
reverse_test_cases [] = {

#undef TEST
#define TEST(str, res) {                                                \
        __LINE__, NPOS, NPOS, NPOS, NPOS, -1, str, sizeof str - 1,      \
        0, 0, res, sizeof res - 1, 0 }

    // 23.2.2.4, p26: the elements in the reverse order

    //    +----------------------------------------- controlled sequence
    //    |                +------------------------ expected result sequence
    //    |                |
    //    V                V
    TEST ("",              ""),
    TEST ("a",             "a"),
    TEST ("ab",            "ba"),
    TEST ("abc",           "cba"),
    TEST ("abcd",          "dcba"),
    TEST ("aab",           "baa"),
    TEST ("<U0>ab",        "ba<U0>"),
    TEST ("x@4096",        "x@4096"),
    TEST ("x@4095y",       "yx@4095"),
    TEST ("x@2048y@2048",  "y@2048x@2048"),

    TEST ("last test",     "tset tsal")
};

/**************************************************************************/

// 23.2.2.4, p32 asks for approximately N log N comparisons; the bound
// is N times the number of passes of a merge sort, the number of times
// N is halved to reach 1
static std::size_t
sort_bound (std::size_t n)
{
    std::size_t passes = 0;

    for (std::size_t m = 1; m < n; m *= 2)
        ++passes;

    return n * passes;
}


// stores in order [i] the position before the call of the i-th
// element of lst, or n where the element was not in the list then;
// the search starts where the element is likeliest to have been,
// after the one before it or, when reversed, at the mirror position
template <class List, class T>
void find_nodes (const List &lst, const T* const *nodes, std::size_t n,
                 bool reversed, std::size_t *order)
{
    std::size_t inx = 0;

    for (typename List::const_iterator it = lst.begin (); it != lst.end ();
         ++it, ++inx) {
        const std::size_t hint =
              reversed ? n - 1 - inx
            : inx      ? order [inx - 1] + 1
            : 0;

        std::size_t pos = n;

        for (std::size_t i = 0; pos == n && i != n; ++i) {
            if (nodes [(hint + i) % n] == &*it)
                pos = (hint + i) % n;
        }

        order [inx] = pos;
    }
}


template <class T, class Allocator>
void test_ops (T*, Allocator*,
               const ContainerTestCaseData<T> &tdata)
{
    typedef std::list <T, Allocator>      List;
    typedef typename List::const_iterator ListCIter;

    const ContainerFunc     &func  = tdata.func_;
    const ContainerTestCase &tcase = tdata.tcase_;

    const bool is_class = ListIds::UserClass == func.elem_id_;

    const bool is_remove =    ListIds::remove_cref    == func.which_
                           || ListIds::remove_if_pred == func.which_;
    const bool is_unique =    ListIds::unique_void    == func.which_
                           || ListIds::unique_bpred   == func.which_;
    const bool is_sort   =    ListIds::sort_void      == func.which_
                           || ListIds::sort_comp      == func.which_;

    // the value to remove, made before the counts are reset
    const T val = T::from_char (char (tcase.val));

    // an exception safety row makes each comparison or call to the
    // predicate throw in turn, from the first, until the operation
    // completes; another row calls the operation once
    for (std::size_t throw_count = 1; ; ++throw_count) {
        // construct the list object to be modified
        List lst (tdata.str_, tdata.str_ + tdata.strlen_);

        // the elements before the call, in order: the operations
        // relink and erase them, and copy none
        const std::size_t size = lst.size ();
        const T** const nodes = new const T* [size + 1];

        std::size_t inx = 0;
        for (ListCIter it = lst.begin (); it != lst.end (); ++it)
            nodes [inx++] = &*it;

        const std::size_t at = -1 == tcase.bthrow ?
            throw_count : std::size_t (-1);

        n_calls  = 0;
        throw_at = at;

        UserClass::op_eq_throw_count_ = at;
        UserClass::op_lt_throw_count_ = at;

        const std::size_t x_count_save = UserClass::count_;

        // reset function call counters
        if (is_class)
            UserClass::reset_totals ();

        bool thrown = false;

        try {
            switch (func.which_) {
            case ListIds::remove_cref:
                lst.remove (val);
                break;

            case ListIds::remove_if_pred:
                lst.remove_if (EqualTo<T> (tcase.val));
                break;

            case ListIds::unique_void:
                lst.unique ();
                break;

            case ListIds::unique_bpred:
                lst.unique (SameLetter<T> ());
                break;

            case ListIds::sort_void:
                lst.sort ();
                break;

            case ListIds::sort_comp:
                lst.sort (LessLetter<T> ());
                break;

            case ListIds::reverse_void:
                lst.reverse ();
                break;

            default:
                RW_ASSERT (!"test logic error: unknown list operation");
            }
        }
        catch (const CallThrown&) {
            thrown = true;
        }
        catch (const UserClass::Exception&) {
            thrown = true;
        }
        catch (...) {
            rw_assert (0, 0, tcase.line,
                       "line %d. %{$FUNCALL} unexpectedly caught "
                       "unknown exception", __LINE__);
        }

        throw_at = std::size_t (-1);

        UserClass::op_eq_throw_count_ = std::size_t (-1);
        UserClass::op_lt_throw_count_ = std::size_t (-1);

        // the comparisons and predicate calls the operation made
        const std::size_t ncalls = is_class ?
              n_calls + UserClass::n_total_op_eq_ + UserClass::n_total_op_lt_
            : n_calls;

        rw_assert (!thrown || -1 == tcase.bthrow, 0, tcase.line,
                   "line %d. %{$FUNCALL} unexpectedly caught an exception "
                   "from a comparison", __LINE__);

        std::size_t got_size = 0;
        for (ListCIter it = lst.begin (); it != lst.end (); ++it)
            ++got_size;

        rw_assert (got_size == lst.size (), 0, tcase.line,
                   "line %d. %{$FUNCALL}: size () == %zu, "
                   "the list holds %zu elements",
                   __LINE__, lst.size (), got_size);

        if (is_class) {
            // the operations erase elements and construct, copy or
            // assign none
            const std::size_t erased = size - got_size;

            const bool success =
                   UserClass::is_total (x_count_save - erased,
                                        0, 0, 0, std::size_t (-1),
                                        std::size_t (-1))
                && UserClass::n_total_dtor_ == erased;

            rw_assert (success, 0, tcase.line,
                       "line %d. %{$FUNCALL} called default/copy ctor, "
                       "operator=() and dtor %zu, %zu, %zu, and %zu times, "
                       "respectively, 0, 0, 0, %zu expected",
                       __LINE__,
                       UserClass::n_total_def_ctor_,
                       UserClass::n_total_copy_ctor_,
                       UserClass::n_total_op_assign_,
                       UserClass::n_total_dtor_, erased);
        }

        // 23.1, p11: the elements that stay are the elements of the
        // list before the call, and iterators to them stay valid
        std::size_t* const order = new std::size_t [got_size + 1];

        find_nodes (lst, nodes, size, ListIds::reverse_void == func.which_,
                    order);

        if (is_remove || is_unique) {
            // 23.2.2.4, p17: the elements that stay keep their order
            bool success = true;

            for (inx = 0; success && inx != got_size; ++inx)
                success = order [inx] < size
                    && (0 == inx || order [inx - 1] < order [inx]);

            rw_assert (success, 0, tcase.line,
                       "line %d. %{$FUNCALL}: element %zu is not the "
                       "element of the list it was before the call",
                       __LINE__, inx - 1);
        }
        else if (is_sort) {
            // 23.2.2.4, p31: the call loses no element, even when
            // it throws
            bool* const seen = new bool [size + 1];

            for (inx = 0; inx != size; ++inx)
                seen [inx] = false;

            bool success = got_size == size;

            for (inx = 0; success && inx != got_size; ++inx) {
                success = order [inx] < size && !seen [order [inx]];

                if (success)
                    seen [order [inx]] = true;
            }

            delete[] seen;

            rw_assert (success, 0, tcase.line,
                       "line %d. %{$FUNCALL}: %{?}%zu elements, %zu "
                       "expected%{:}element %zu is not one of the elements "
                       "of the list before the call%{;}",
                       __LINE__, got_size != size, got_size, size, inx - 1);
        }
        else {
            // 23.2.2.4, p26: the elements themselves in the reverse
            // order
            bool success = got_size == size;

            for (inx = 0; success && inx != got_size; ++inx)
                success = order [inx] == size - 1 - inx;

            rw_assert (success, 0, tcase.line,
                       "line %d. %{$FUNCALL}: %{?}%zu elements, %zu "
                       "expected%{:}element %zu is not the element %zu "
                       "before the call%{;}",
                       __LINE__, got_size != size, got_size, size,
                       inx - 1, size - inx);
        }

        if (thrown && -1 == tcase.bthrow) {
            delete[] nodes;
            delete[] order;

            // let this call succeed and make the next one throw
            continue;
        }

        if (is_sort) {
            // 23.2.2.4, p31: equivalent elements keep their order
            const bool fold_case = ListIds::sort_comp == func.which_;

            bool success = got_size == size;

            ListCIter it = lst.begin ();

            for (inx = 1; success && inx < got_size; ++inx) {
                const int prev = it->data_.val_;
                const int next = (++it)->data_.val_;

                const bool equiv = fold_case ?
                    fold (prev) == fold (next) : prev == next;

                success = !equiv || order [inx - 1] < order [inx];
            }

            rw_assert (success, 0, tcase.line,
                       "line %d. %{$FUNCALL}: equivalent elements %zu "
                       "and %zu out of their order before the call",
                       __LINE__, inx - 2, inx - 1);
        }

        if (is_unique) {
            // 23.2.2.4, p19: the element that stays of each group is
            // its first, so the element before it in the list before
            // the call, if any, is not equivalent to it
            const bool fold_case = ListIds::unique_bpred == func.which_;

            bool success = true;

            for (inx = 0; success && inx != got_size; ++inx) {
                const std::size_t pos = order [inx];

                if (0 < pos && pos < size) {
                    const int prev = tdata.str_ [pos - 1].data_.val_;
                    const int next = tdata.str_ [pos].data_.val_;

                    success = fold_case ?
                        fold (prev) != fold (next) : prev != next;
                }
            }

            rw_assert (success, 0, tcase.line,
                       "line %d. %{$FUNCALL}: element %zu is not the "
                       "first of its group", __LINE__, inx - 1);
        }

        // 23.2.2.4, p18 and p21: exactly as many applications of the
        // predicate as the list has elements, and for unique one fewer;
        // p32: about N log N comparisons; p28: reverse, in linear
        // time, makes none
        const std::size_t exp_calls =
              is_remove ? size
            : is_unique ? (size ? size - 1 : 0)
            : is_sort   ? sort_bound (size)
            : 0;

        rw_assert (is_sort ? ncalls <= exp_calls : ncalls == exp_calls,
                   0, tcase.line,
                   "line %d. %{$FUNCALL} made %zu comparisons, "
                   "%{?}at most %{;}%zu expected",
                   __LINE__, ncalls, is_sort, exp_calls);

        // for convenience
        static const int cwidth = sizeof (T);

        char* const got = new char [got_size + 1];

        inx = 0;
        for (ListCIter it = lst.begin (); it != lst.end (); ++it)
            got [inx++] = char (it->data_.val_);

        got [got_size] = '\0';

        // verify that strings length are equal
        rw_assert (tdata.reslen_ == got_size, 0, tcase.line,
                   "line %d. %{$FUNCALL} expected \"%{X=*}\" with length "
                   "%zu, got %{/*.*Gs} with length %zu",
                   __LINE__, cwidth, int (tdata.reslen_), tdata.res_,
                   tdata.reslen_, 1, int (got_size), got, got_size);

        if (tdata.reslen_ == got_size) {
            // if the result length matches the expected length
            // (and only then), also verify that the modified
            // list matches the expected result
            const T* pmatch = T::mismatch (tdata.res_, got, got_size);

            rw_assert (0 == pmatch, 0, tcase.line,
                       "line %d. %{$FUNCALL} expected \"%{X=*}\", "
                       "got %{/*.*Gs}, difference at offset %zu",
                       __LINE__, cwidth, int (tdata.reslen_), tdata.res_,
                       1, int (got_size), got, pmatch - tdata.res_);
        }

        delete[] got;
        delete[] nodes;
        delete[] order;

        break;
    }
}

/**************************************************************************/

DEFINE_CONTAINER_TEST_FUNCTIONS (test_ops);

int main (int argc, char** argv)
{
    static const ContainerTest
    tests [] = {

#undef TEST
#define TEST(sig, cases) {                                              \
        ListIds::sig, cases ## _test_cases,                             \
        sizeof cases ## _test_cases / sizeof *cases ## _test_cases      \
    }

        TEST (remove_cref, remove),
        TEST (remove_if_pred, remove),
        TEST (unique_void, unique_void),
        TEST (unique_bpred, unique),
        TEST (sort_void, sort),
        TEST (sort_comp, sort_comp),
        TEST (reverse_void, reverse)
    };

    const std::size_t test_count = sizeof tests / sizeof *tests;

    return rw_run_cont_test (argc, argv, __FILE__,
                             "lib.list.ops",
                             ContainerIds::List,
                             test_ops_func_array, tests, test_count);
}
