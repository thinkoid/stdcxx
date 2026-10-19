/***************************************************************************
 *
 * 23.deque.modifiers.cpp - test exercising [lib.deque.modifiers]
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
 * Copyright 1994-2008 Rogue Wave Software, Inc.
 * 
 **************************************************************************/

#include <deque>     // for deque

#include <cstdlib>   // for free()

#define DEFINE_REPLACEMENT_NEW_AND_DELETE
#include <rw_new.h>

#include <rw_value.h>    // for UserClass
#include <rw_driver.h>   // for rw_test(), ...
#include <rw_printf.h>   // for rw_asnprintf

/**************************************************************************/

// Runtime options
/* extern */ int rw_opt_no_assign = 0;
/* extern */ int rw_opt_no_erase  = 0;
/* extern */ int rw_opt_no_insert = 0;
/* extern */ int rw_opt_no_dr438  = 0;
/* extern */ int rw_opt_no_input_iterator = 0;
/* extern */ int rw_opt_no_forward_iterator = 0;
/* extern */ int rw_opt_no_bidirectional_iterator = 0;
/* extern */ int rw_opt_no_random_iterator = 0;
/* extern */ int rw_opt_no_right_thing = 0;

/**************************************************************************/

// For konvenience
typedef unsigned char UChar;

/**************************************************************************/

typedef std::deque<UserClass, std::allocator<UserClass> > Deque;

Deque::size_type new_capacity;

namespace __rw {

template <>
inline Deque::size_type
__rw_new_capacity<Deque>(Deque::size_type n, const Deque*)
{
    if (n) {
        // A non-zero argument requests growth of the pointer map.
        // Its capacity is measured in nodes, independently of buffers.
        return n * 2;
    }

    // zero size argument is a request for the size of the object's
    // fixed-size buffer for elements
    return new_capacity;
}

}

/**************************************************************************/

enum {
    NewThrows = 0x1          /* cause operator new to throw */,
    CopyCtorThrows = 0x2     /* cause element's copy ctor to throw */,
    AssignmentThrows = 0x4   /* cause element's assignment to throw */
};

enum MemberFunction {
    Assign_n    /* deque::assign (size_type, const_reference) */,
    AssignRange /* deque::assign (InputIterator, InputIterator) */,

    Erase_1     /* deque::erase (iterator) */,
    EraseRange  /* deque::erase (iterator, iterator) */,

    Insert_1    /* deque::insert (iterator, const_reference) */,
    Insert_n    /* deque::insert (iterator, size_type, const_reference) */,
    InsertRange /* deque::insert (iterator, InputIterator, InputIterator) */
};


// Invoke one operation with fresh range iterators on every attempt.
template <class Iterator>
void exercise (MemberFunction mfun, Deque &deq, Deque::size_type off,
               int n, const UserClass *x,
               const Iterator &first, const Iterator &last)
{
    const Iterator range_first = copy_iter (first, (UserClass*)0);
    const Iterator range_last  = copy_iter (last, (UserClass*)0);
    const Deque::size_type nelems (n);
    const Deque::iterator pos = deq.begin () + off;

    switch (mfun) {
    case Assign_n:    deq.assign (nelems, *x); break;
    case AssignRange: deq.assign (range_first, range_last); break;
    case Insert_1:    deq.insert (pos, *x); break;
    case Insert_n:    deq.insert (pos, nelems, *x); break;
    case InsertRange: deq.insert (pos, range_first, range_last); break;
    case Erase_1: {
        const Deque::iterator result = deq.erase (pos);
        rw_assert (result == deq.begin () + off, 0, __LINE__,
                   "erase(iterator) returned the wrong position");
        break;
    }
    case EraseRange: {
        const Deque::iterator result = deq.erase (pos, pos + n);
        rw_assert (result == deq.begin () + off, 0, __LINE__,
                   "erase(first, last) returned the wrong position");
        break;
    }
    }
}

// Sweep one failure source at a time. A fresh container for every attempt
// permits element assignment to leave changed values after an exception.
template <class Iterator>
void exception_loop (int line, MemberFunction mfun, const char *fcall,
                     int exceptions, Deque &deq,
                     const Deque::iterator &it, int n, const UserClass *x,
                     const Iterator &first, const Iterator &last,
                     std::size_t *n_copy, std::size_t *n_asgn)
{
    const Deque::size_type off = it - deq.begin ();

#ifndef _RWSTD_NO_EXCEPTIONS

    rwt_free_store* const pst = rwt_get_free_store (0);

    for (int source = NewThrows; source <= AssignmentThrows; source <<= 1) {
        if (!(exceptions & source))
            continue;

        for (std::size_t ordinal = 1; ; ++ordinal) {
            Deque trial (deq);
            const Deque::const_iterator begin = trial.begin ();
            const Deque::const_iterator end   = trial.end ();
            const std::size_t live = UserClass::count_;

            // InputIter copies may allocate. Make them before arming new.
            const Iterator range_first = copy_iter (first, (UserClass*)0);
            const Iterator range_last  = copy_iter (last, (UserClass*)0);

            const std::size_t copies = UserClass::n_total_copy_ctor_;
            const std::size_t assigns = UserClass::n_total_op_assign_;
            const std::size_t allocations = pst->new_calls_ [0];

            if (NewThrows == source)
                *pst->throw_at_calls_ [0] = allocations + ordinal;
            else if (CopyCtorThrows == source)
                UserClass::copy_ctor_throw_count_ = copies + ordinal;
            else
                UserClass::op_assign_throw_count_ = assigns + ordinal;

            bool caught = false;
            try {
                exercise (mfun, trial, off, n, x, range_first, range_last);
            }
            catch (...) {
                caught = true;
            }

            *pst->throw_at_calls_ [0] = _RWSTD_SIZE_MAX;
            UserClass::copy_ctor_throw_count_ = 0;
            UserClass::op_assign_throw_count_ = 0;

            const std::size_t calls = NewThrows == source
                ? pst->new_calls_ [0] - allocations : CopyCtorThrows == source
                ? UserClass::n_total_copy_ctor_ - copies
                : UserClass::n_total_op_assign_ - assigns;

            rw_assert (caught ? calls == ordinal : calls < ordinal,
                       0, line, "%s: failure source %d, ordinal %zu, "
                       "caught %d after %zu calls", fcall, source, ordinal,
                       caught, calls);

            if (!caught)
                break;

            rw_assert (trial.size () == deq.size (), 0, line,
                       "%s: size changed after source %d threw", fcall, source);
            rw_assert (trial.begin () == begin && trial.end () == end, 0, line,
                       "%s: endpoints changed after source %d threw",
                       fcall, source);
            rw_assert (UserClass::count_ == live, 0, line,
                       "%s: leaked objects after source %d threw", fcall, source);

            if (NewThrows == source) {
                for (Deque::size_type i = 0; i != deq.size (); ++i)
                    rw_assert (trial [i].data_.val_ == deq [i].data_.val_,
                               0, line, "%s: allocation failure changed "
                               "element %zu", fcall, i);
            }
        }
    }

#else

    _RWSTD_UNUSED (exceptions);
    _RWSTD_UNUSED (line);
    _RWSTD_UNUSED (fcall);

#endif   // _RWSTD_NO_EXCEPTIONS

    const std::size_t live = UserClass::count_;
    const std::size_t size = deq.size ();
    *n_copy = UserClass::n_total_copy_ctor_;
    *n_asgn = UserClass::n_total_op_assign_;
    exercise (mfun, deq, off, n, x, first, last);
    *n_copy = UserClass::n_total_copy_ctor_ - *n_copy;
    *n_asgn = UserClass::n_total_op_assign_ - *n_asgn;

    rw_assert (UserClass::count_ + size == live + deq.size (), 0, line,
               "%s: object count changed unexpectedly", fcall);
}


// returns the number of invocations of the assignment operator
// for a call to any of the inserts: the elements on the shorter
// side of the insertion point move over, each assigned once,
// whatever the number of elements inserted
std::size_t insert_assignments (std::size_t nelems,
                                std::size_t off,
                                std::size_t seqlen)
{
    return 0 == nelems ? 0 : off < seqlen - off ? off : seqlen - off;
}


template <class Iterator>
void test_insert (int line, int exceptions,
                  const Iterator &dummy, int nelems,
                  const char *seq, std::size_t seqlen, std::size_t off, 
                  const char *ins, std::size_t inslen, 
                  const char *res, std::size_t reslen)
{
    // Ensure that xsrc, xins are always dereferenceable
    const UserClass* const xseq = UserClass::from_char (seq, seqlen + 1);
          UserClass* const xins = UserClass::from_char (ins, inslen + 1);

    Deque deq = seqlen ? Deque (xseq, xseq + seqlen) : Deque ();

    // offset must be valid
    _RWSTD_ASSERT (off <= deq.size ());
    const Deque::iterator iter = deq.begin () + off;

    // every insert reserves the room it needs before it touches an
    // element and puts the storage back if a throw follows, so none
    // has effects on the size or the ends when an exception is thrown,
    // at any position and for any number of elements (23.2.1.3, p2);
    // the loop below exercises that with every exception enabled

    // Arbitrary source iterators are consumed before destination mutation.
    const bool twice = -1 == nelems;

    // format a string describing the function call being exercised
    // (used in diagnostic output below)
    char* funcall = 0;
    std::size_t len = 0;

    static const int cwidth = sizeof (*xseq);

    rw_asnprintf (&funcall, &len, "deque(\"%{X=*.*}\").insert("
                  "%{?}begin(), %{:}%{?}end (), %{:}begin () + %zu%{;}%{;}"
                  "%{?}%d)%{:}%{?}\"%{X=*.*}\")%{:}%d, %d)%{;}%{;}",
                  cwidth, int (seqlen), -1, xseq, 0 == off, seqlen == off,
                  off, nelems == -2, *ins, nelems == -1, 
                  cwidth, int (inslen), -1, xins, nelems, *ins);

    std::size_t n_copy = UserClass::n_total_copy_ctor_;
    std::size_t n_asgn = UserClass::n_total_op_assign_;

    if (-2 == nelems) {   // insert(iterator, const_reference)

        exception_loop (line, Insert_1, funcall, exceptions,
                        deq, iter, nelems, xins, dummy, dummy,
                        &n_copy, &n_asgn);

    }
    else if (-1 == nelems) {   // insert(iterator, Iterator, Iterator)

        const Iterator first =
            make_iter (xins, xins, xins + inslen, dummy);

        const Iterator last =
            make_iter (xins + inslen, xins, xins + inslen, dummy);

        exception_loop (line, InsertRange, funcall, exceptions,
                        deq, iter, nelems, 0, first, last,
                        &n_copy, &n_asgn);

    }
    else {   // insert(iterator, size_type, const_reference)

        exception_loop (line, Insert_n, funcall, exceptions,
                        deq, iter, nelems, xins, dummy, dummy,
                        &n_copy, &n_asgn);

    }

    // verify the expected size of the deque after insertion
    rw_assert (deq.size () == reslen, __FILE__, line,
               "line %d: %s: size == %zu, got %zu\n",
               __LINE__, funcall, reslen, deq.size ());

    // verify the expected contents of the deque after insertion
    const Deque::const_iterator resbeg = deq.begin ();
    const Deque::const_iterator resend = deq.end ();

    for (Deque::const_iterator it = resbeg; it != resend; ++it) {
        if ((*it).data_.val_ != UChar (res [it - resbeg])) {

            char* const got = new char [deq.size () + 1];

            for (Deque::const_iterator i = resbeg; i != resend; ++i) {
                got [i - resbeg] = char ((*i).data_.val_);
            }

            got [deq.size ()] = '\0';

            rw_assert (false, __FILE__, line,
                       "line %d: %s: expected %s, got %s\n",
                       __LINE__, funcall, res, got);

            delete[] got;
            break;
        }
    }

    // verify the complexity of the operation in terms of the number
    // of calls to the copy ctor and assignment operator on value_type
    const std::size_t expect_copy =
        (twice ? 2 * inslen : nelems < 0 ? inslen : nelems)
        + (!twice && (nelems < 0 || nelems) && off && off != seqlen);

    rw_assert (n_copy == expect_copy, __FILE__, line,
               "line %d: %s: expected %zu invocations "
               "of UserClass::UserClass(const UserClass&), got %d\n",
               __LINE__, funcall, expect_copy, n_copy);

    // compute the number of calls to the assignment operator
    const std::size_t expect_asgn =
        insert_assignments (nelems < 0 ? inslen : nelems, off, seqlen);

    rw_assert (n_asgn == expect_asgn, __FILE__, line,
               "line %d: %s: expected %zu invocations "
               "of UserClass::operator=(const UserClass&), got %d\n",
               __LINE__, funcall, expect_asgn, n_asgn);

    // Free funcall storage
    std::free (funcall);

    delete[] xins;

    // cast away constness to work around an HP aCC 6.16 bug
    // see http://issues.apache.org/jira/browse/STDCXX-802
    delete[] const_cast< UserClass* >(xseq);
}

/**************************************************************************/

template <class Iterator>
void test_insert_range (const Iterator &it, const char* itname)
{
    rw_info (0, 0 ,0, 
             "std::deque<UserClass>::insert(iterator, %s, %s)", itname, itname);

#undef TEST
#define TEST(seq, off, ins, res)                        \
    test_insert (__LINE__, -1,                          \
                            it, -1,                     \
                            seq, sizeof seq - 1,        \
                            std::size_t (off),          \
                            ins, sizeof ins - 1,        \
                            res, sizeof res - 1)

    //    +---------------------------------------- seq
    //    |      +--------------------------------- off
    //    |      |   +----------------------------- ins
    //    |      |   |      +---------------------- res
    //    |      |   |      |
    //    v      v   v      v
    TEST ("",    +0, "",    "");
    TEST ("",    +0, "a",   "a");
    TEST ("",    +0, "ab",  "ab");
    TEST ("",    +0, "abc", "abc");
    TEST ("a",   +0, "",    "a");
    TEST ("b",   +0, "a",   "ab");
    TEST ("c",   +0, "ab",  "abc");
    TEST ("cd",  +0, "ab",  "abcd");
    TEST ("def", +0, "abc", "abcdef");

    TEST ("a",   +1, "",    "a");
    TEST ("a",   +1, "b",   "ab");
    TEST ("a",   +1, "bc",  "abc");
    TEST ("a",   +1, "bcd", "abcd");

    TEST ("ab",  +1, "",    "ab");
    TEST ("ac",  +1, "b",   "abc");
    TEST ("acd", +1, "b",   "abcd");

    TEST ("ab",  +2, "",    "ab");
    TEST ("ab",  +2, "c",   "abc");
    TEST ("ab",  +2, "cd",  "abcd");

    TEST ("abc", +2, "",    "abc");
    TEST ("abd", +2, "c",   "abcd");
    TEST ("abe", +2, "cd",  "abcde");
    TEST ("abf", +2, "cde", "abcdef");

    TEST ("abc", +3, "",    "abc");
    TEST ("abc", +3, "d",   "abcd");
    TEST ("abc", +3, "de",  "abcde");
    TEST ("abc", +3, "def", "abcdef");
    

#define UPPER "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
#define LOWER "abcdefghijklmnopqrstuvwxyz"

    TEST (UPPER,  +0, LOWER, "" LOWER "ABCDEFGHIJKLMNOPQRSTUVWXYZ");
    TEST (UPPER,  +1, LOWER, "A" LOWER "BCDEFGHIJKLMNOPQRSTUVWXYZ");
    TEST (UPPER,  +2, LOWER, "AB" LOWER "CDEFGHIJKLMNOPQRSTUVWXYZ");
    TEST (UPPER,  +3, LOWER, "ABC" LOWER "DEFGHIJKLMNOPQRSTUVWXYZ");
    TEST (UPPER,  +4, LOWER, "ABCD" LOWER "EFGHIJKLMNOPQRSTUVWXYZ");
    TEST (UPPER,  +5, LOWER, "ABCDE" LOWER "FGHIJKLMNOPQRSTUVWXYZ");
    TEST (UPPER,  +6, LOWER, "ABCDEF" LOWER "GHIJKLMNOPQRSTUVWXYZ");
    TEST (UPPER,  +7, LOWER, "ABCDEFG" LOWER "HIJKLMNOPQRSTUVWXYZ");
    TEST (UPPER,  +8, LOWER, "ABCDEFGH" LOWER "IJKLMNOPQRSTUVWXYZ");
    TEST (UPPER,  +9, LOWER, "ABCDEFGHI" LOWER "JKLMNOPQRSTUVWXYZ");
    TEST (UPPER, +10, LOWER, "ABCDEFGHIJ" LOWER "KLMNOPQRSTUVWXYZ");
    TEST (UPPER, +11, LOWER, "ABCDEFGHIJK" LOWER "LMNOPQRSTUVWXYZ");
    TEST (UPPER, +12, LOWER, "ABCDEFGHIJKL" LOWER "MNOPQRSTUVWXYZ");
    TEST (UPPER, +13, LOWER, "ABCDEFGHIJKLM" LOWER "NOPQRSTUVWXYZ");
    TEST (UPPER, +14, LOWER, "ABCDEFGHIJKLMN" LOWER "OPQRSTUVWXYZ");
    TEST (UPPER, +15, LOWER, "ABCDEFGHIJKLMNO" LOWER "PQRSTUVWXYZ");
    TEST (UPPER, +16, LOWER, "ABCDEFGHIJKLMNOP" LOWER "QRSTUVWXYZ");
    TEST (UPPER, +17, LOWER, "ABCDEFGHIJKLMNOPQ" LOWER "RSTUVWXYZ");
    TEST (UPPER, +18, LOWER, "ABCDEFGHIJKLMNOPQR" LOWER "STUVWXYZ");
    TEST (UPPER, +19, LOWER, "ABCDEFGHIJKLMNOPQRS" LOWER "TUVWXYZ");
    TEST (UPPER, +20, LOWER, "ABCDEFGHIJKLMNOPQRST" LOWER "UVWXYZ");
    TEST (UPPER, +21, LOWER, "ABCDEFGHIJKLMNOPQRSTU" LOWER "VWXYZ");
    TEST (UPPER, +22, LOWER, "ABCDEFGHIJKLMNOPQRSTUV" LOWER "WXYZ");
    TEST (UPPER, +23, LOWER, "ABCDEFGHIJKLMNOPQRSTUVW" LOWER "XYZ");
    TEST (UPPER, +24, LOWER, "ABCDEFGHIJKLMNOPQRSTUVWX" LOWER "YZ");
    TEST (UPPER, +25, LOWER, "ABCDEFGHIJKLMNOPQRSTUVWXY" LOWER "Z");
    TEST (UPPER, +26, LOWER, "ABCDEFGHIJKLMNOPQRSTUVWXYZ" LOWER "");
}

/**************************************************************************/

template <class T, class IntType>
void test_insert_int_range (const T&, const IntType&,
                            const char* t_name, const char* int_name)
{
    rw_info (0, 0, 0, 
             "std::deque<%s>::insert(iterator, %s, %s)", 
             t_name, int_name, int_name);

    std::deque<T> d;

    typename std::deque<T>::iterator it = d.begin ();

    // deque<T>::insert(iterator, size_type, const_reference)

    d.insert (it, IntType (1), IntType (0));

    rw_assert (1 == d.size (), 0, __LINE__,
               "deque<%s>::insert(begin(), %s = 1, %s = 0); size() == 1,"
               " got %zu", t_name, int_name, int_name, d.size ());

    it = d.begin ();
    ++it;

    d.insert (it, IntType (3), IntType (2));

    rw_assert (4 == d.size (), 0, __LINE__,
               "deque<%s>::insert(begin() + 1, %s = 3, %s = 2); size() == 4,"
               " got %zu", t_name, int_name, int_name, d.size ());

    it = d.begin ();
    ++it;

    d.insert (it, IntType (2), IntType (1));

    rw_assert (6 == d.size (), 0, __LINE__,
               "deque<%s>::insert(begin() + 1, %s = 2, %s = 1); size() == 6,"
               " got %zu", t_name, int_name, int_name, d.size ());
}


template <class T>
void test_insert_int_range (const T &dummy, const char* tname)
{
    test_insert_int_range (dummy, (signed char)0, tname, "signed char");
    test_insert_int_range (dummy, (unsigned char)0, tname, "unsigned char");
    test_insert_int_range (dummy, short (), tname, "short");
    test_insert_int_range (dummy, (unsigned short)0, tname, "unsigned short");
    test_insert_int_range (dummy, int (), tname, "int");
    test_insert_int_range (dummy, (unsigned int)0, tname, "unsigned int");
    test_insert_int_range (dummy, long (), tname, "long");
    test_insert_int_range (dummy, (unsigned long)0, tname, "unsigned long");

#ifdef _RWSTD_LONG_LONG

    test_insert_int_range (dummy, (_RWSTD_LONG_LONG)0, 
                           tname, "long long");
    test_insert_int_range (dummy, (unsigned _RWSTD_LONG_LONG)0, 
                           tname, "unsigned long long");

#endif   // _RWSTD_LONG_LONG

}

/**************************************************************************/

// asserts that the seqlen elements whose addresses addr holds are at
// those addresses with the values of seq, nfront elements from the
// front of the deque; call names the modifier called last
void check_references (int line, const char *call,
                       const Deque &deq, const UserClass* const *addr,
                       const char *seq, std::size_t seqlen,
                       std::size_t nfront)
{
    std::size_t i = 0;

    for ( ; i != seqlen; ++i) {
        const UserClass &x = deq [nfront + i];

        if (&x != addr [i] || x.data_.val_ != UChar (seq [i]))
            break;
    }

    const UserClass* const got = i == seqlen ? 0 : &deq [nfront + i];

    rw_assert (0 == got, 0, line,
               "line %d: deque<UserClass>::%s: size() == %zu, element "
               "%zu of the first %zu expected %c at %p, got %c at %p",
               __LINE__, call, deq.size (), i, seqlen,
               got ? seq [i] : ' ', (const void*)(got ? addr [i] : 0),
               got ? got->data_.val_ : ' ', (const void*)got);
}


// 23.2.1.3, p1: an insert at either end of the deque leaves every
// reference to its elements valid; verifies that the elements the
// deque holds first stay at their addresses with their values through
// inserts of every kind at both ends, while the live range crosses
// buffer after buffer and the vector of nodes grows
void test_insert_references ()
{
    rw_info (0, 0, 0,
             "std::deque<UserClass>::insert(iterator, ...) at either end "
             "leaves references to the elements valid");

    static const char seq[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZ";
    static const std::size_t seqlen = sizeof seq - 1;

    UserClass* const xseq = UserClass::from_char (seq, seqlen + 1);

    Deque deq (xseq, xseq + seqlen);

    // the address of every element before the first insert
    const UserClass* addr [seqlen];

    for (std::size_t i = 0; i != seqlen; ++i)
        addr [i] = &deq [i];

    // the number of elements inserted before the first of them
    std::size_t nfront = 0;

#define CHECK(call)                                                    \
    check_references (__LINE__, call, deq, addr, seq, seqlen, nfront)

    const InputIter<UserClass> in (0, 0, 0);

    for (std::size_t n = 1; n < 40; n += 5) {

        deq.push_front (xseq [0]);
        ++nfront;
        CHECK ("push_front(const_reference)");

        deq.push_back (xseq [0]);
        CHECK ("push_back(const_reference)");

        deq.insert (deq.begin (), xseq [1]);
        ++nfront;
        CHECK ("insert(begin(), const_reference)");

        deq.insert (deq.end (), xseq [1]);
        CHECK ("insert(end(), const_reference)");

        deq.insert (deq.begin (), n, xseq [2]);
        nfront += n;
        CHECK ("insert(begin(), size_type, const_reference)");

        deq.insert (deq.end (), n, xseq [2]);
        CHECK ("insert(end(), size_type, const_reference)");

        deq.insert (deq.begin (),
                    make_iter (xseq, xseq, xseq + seqlen, in),
                    make_iter (xseq + seqlen, xseq, xseq + seqlen, in));
        nfront += seqlen;
        CHECK ("insert(begin(), InputIterator, InputIterator)");

        deq.insert (deq.end (),
                    make_iter (xseq, xseq, xseq + seqlen, in),
                    make_iter (xseq + seqlen, xseq, xseq + seqlen, in));
        CHECK ("insert(end(), InputIterator, InputIterator)");

        deq.insert (deq.begin (), xseq, xseq + seqlen);
        nfront += seqlen;
        CHECK ("insert(begin(), const_pointer, const_pointer)");

        deq.insert (deq.end (), xseq, xseq + seqlen);
        CHECK ("insert(end(), const_pointer, const_pointer)");
    }

#undef CHECK

    delete[] xseq;
}

/**************************************************************************/

void test_insert ()
{
    //////////////////////////////////////////////////////////////////
    // exercise deque::insert(iterator, const_reference)

    rw_info (0, 0, 0,
             "std::deque<UserClass>::insert(iterator, const_reference)");

#undef TEST
#define TEST(seq, off, ins, res) do {                   \
      const char insseq [] = { ins, '\0' };             \
      test_insert (__LINE__, -1,                        \
                              (UserClass*)0, -2,        \
                              seq, sizeof seq - 1,      \
                              std::size_t (off),        \
                              insseq, 1,                \
                              res, sizeof res - 1);     \
    } while (0)

    //    +------------------- original sequence
    //    |       +----------- insertion offset
    //    |       |    +------ element to insert
    //    |       |    |   +-- resulting sequence
    //    |       |    |   |
    //    V       V    V   V
    TEST ("",     +0, 'a', "a");
    TEST ("b",    +0, 'a', "ab");
    TEST ("bc",   +0, 'a', "abc");
    TEST ("bcd",  +0, 'a', "abcd");
    TEST ("bcde", +0, 'a', "abcde");

    TEST ("a",    +1, 'b', "ab");
    TEST ("ac",   +1, 'b', "abc");
    TEST ("acd",  +1, 'b', "abcd");
    TEST ("acde", +1, 'b', "abcde");

    TEST ("ab",   +2, 'c', "abc");
    TEST ("abd",  +2, 'c', "abcd");
    TEST ("abde", +2, 'c', "abcde");

    TEST ("abc",  +3, 'd', "abcd");
    TEST ("abce", +3, 'd', "abcde");

    TEST ("abcd", +4, 'e', "abcde");

#define A_to_B "AB"
#define A_to_C "ABC"
#define A_to_D "ABCD"
#define A_to_E "ABCDE"
#define A_to_F "ABCDEF"
#define A_to_G "ABCDEFG"
#define A_to_H "ABCDEFGH"
#define A_to_I "ABCDEFGHI"
#define A_to_J "ABCDEFGHIJ"
#define A_to_K "ABCDEFGHIJK"
#define A_to_L "ABCDEFGHIJKL"
#define A_to_M "ABCDEFGHIJKLM"
#define A_to_N "ABCDEFGHIJKLMN"
#define A_to_O "ABCDEFGHIJKLMNO"
#define A_to_P "ABCDEFGHIJKLMNOP"
#define A_to_Q "ABCDEFGHIJKLMNOPQ"
#define A_to_R "ABCDEFGHIJKLMNOPQR"
#define A_to_S "ABCDEFGHIJKLMNOPQRS"
#define A_to_T "ABCDEFGHIJKLMNOPQRST"
#define A_to_U "ABCDEFGHIJKLMNOPQRSTU"
#define A_to_V "ABCDEFGHIJKLMNOPQRSTUV"
#define A_to_W "ABCDEFGHIJKLMNOPQRSTUVW"
#define A_to_X "ABCDEFGHIJKLMNOPQRSTUVWX"
#define A_to_Y "ABCDEFGHIJKLMNOPQRSTUVWXY"
#define A_to_Z "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
#define B_to_Z "BCDEFGHIJKLMNOPQRSTUVWXYZ"
#define C_to_Z "CDEFGHIJKLMNOPQRSTUVWXYZ"
#define D_to_Z "DEFGHIJKLMNOPQRSTUVWXYZ"
#define E_to_Z "EFGHIJKLMNOPQRSTUVWXYZ"
#define F_to_Z "FGHIJKLMNOPQRSTUVWXYZ"
#define G_to_Z "GHIJKLMNOPQRSTUVWXYZ"
#define H_to_Z "HIJKLMNOPQRSTUVWXYZ"
#define I_to_Z "IJKLMNOPQRSTUVWXYZ"
#define J_to_Z "JKLMNOPQRSTUVWXYZ"
#define K_to_Z "KLMNOPQRSTUVWXYZ"
#define L_to_Z "LMNOPQRSTUVWXYZ"
#define M_to_Z "MNOPQRSTUVWXYZ"
#define N_to_Z "NOPQRSTUVWXYZ"
#define O_to_Z "OPQRSTUVWXYZ"
#define P_to_Z "PQRSTUVWXYZ"
#define Q_to_Z "QRSTUVWXYZ"
#define R_to_Z "RSTUVWXYZ"
#define S_to_Z "STUVWXYZ"
#define T_to_Z "TUVWXYZ"
#define U_to_Z "UVWXYZ"
#define V_to_Z "VWXYZ"
#define W_to_Z "WXYZ"
#define X_to_Z "XYZ"
#define Y_to_Z "YZ"

    TEST (A_to_Z, + 0, '^', ""     "^" A_to_Z);
    TEST (A_to_Z, + 1, '^', "A"    "^" B_to_Z);
    TEST (A_to_Z, + 2, '^', A_to_B "^" C_to_Z);
    TEST (A_to_Z, + 3, '^', A_to_C "^" D_to_Z);
    TEST (A_to_Z, + 4, '^', A_to_D "^" E_to_Z);
    TEST (A_to_Z, + 5, '^', A_to_E "^" F_to_Z);
    TEST (A_to_Z, + 6, '^', A_to_F "^" G_to_Z);
    TEST (A_to_Z, + 7, '^', A_to_G "^" H_to_Z);
    TEST (A_to_Z, + 8, '^', A_to_H "^" I_to_Z);
    TEST (A_to_Z, + 9, '^', A_to_I "^" J_to_Z);
    TEST (A_to_Z, +10, '^', A_to_J "^" K_to_Z);
    TEST (A_to_Z, +11, '^', A_to_K "^" L_to_Z);
    TEST (A_to_Z, +12, '^', A_to_L "^" M_to_Z);
    TEST (A_to_Z, +13, '^', A_to_M "^" N_to_Z);
    TEST (A_to_Z, +14, '^', A_to_N "^" O_to_Z);
    TEST (A_to_Z, +15, '^', A_to_O "^" P_to_Z);
    TEST (A_to_Z, +16, '^', A_to_P "^" Q_to_Z);
    TEST (A_to_Z, +17, '^', A_to_Q "^" R_to_Z);
    TEST (A_to_Z, +18, '^', A_to_R "^" S_to_Z);
    TEST (A_to_Z, +19, '^', A_to_S "^" T_to_Z);
    TEST (A_to_Z, +20, '^', A_to_T "^" U_to_Z);
    TEST (A_to_Z, +21, '^', A_to_U "^" V_to_Z);
    TEST (A_to_Z, +22, '^', A_to_V "^" W_to_Z);
    TEST (A_to_Z, +23, '^', A_to_W "^" X_to_Z);
    TEST (A_to_Z, +24, '^', A_to_X "^" Y_to_Z);
    TEST (A_to_Z, +25, '^', A_to_Y "^" "Z");
    TEST (A_to_Z, +26, '^', A_to_Z "^" "");

    //////////////////////////////////////////////////////////////////
    // exercise deque::insert(iterator, size_type, const_reference)

    rw_info (0, 0, 0, 
             "std::deque<UserClass>::insert(iterator, size_type, "
             "const_reference)");

#undef TEST
#define TEST(seq, off, n, ins, res) do {                \
      const char insseq [] = { ins, '\0' };             \
      test_insert (__LINE__, -1,                        \
                              (UserClass*)0, n,         \
                              seq, sizeof seq - 1,      \
                              std::size_t (off),        \
                              insseq, 1,                \
                              res, sizeof res - 1);     \
    } while (0)
    
    TEST ("",     +0, 0, 'a', "");
    TEST ("",     +0, 1, 'a', "a");
    TEST ("",     +0, 2, 'b', "bb");
    TEST ("",     +0, 3, 'c', "ccc");

    TEST ("a",    +0, 0, 'a', "a");
    TEST ("b",    +0, 1, 'a', "ab");
    TEST ("b",    +0, 2, 'a', "aab");
    TEST ("b",    +0, 3, 'a', "aaab");

    TEST ("ab",   +1, 0, 'b', "ab");
    TEST ("ac",   +1, 1, 'b', "abc");
    TEST ("ac",   +1, 2, 'b', "abbc");
    TEST ("ac",   +1, 3, 'b', "abbbc");

    TEST ("abcd", +2, 0, 'c', "abcd");
    TEST ("abde", +2, 1, 'c', "abcde");
    TEST ("abde", +2, 2, 'c', "abccde");
    TEST ("abde", +2, 3, 'c', "abcccde");

    //////////////////////////////////////////////////////////////////
    // exercise deque::insert(iterator, InputIterator, InputIterator)

    rw_info (0, 0, 0, 
             "template <class InputIterator> std::deque<UserClass>::"
             "insert(iterator, InputIterator, InputIterator)");

    if (0 == rw_opt_no_input_iterator)
        test_insert_range (InputIter<UserClass>(0, 0, 0),
                           "InputIter<UserClass>");
    else 
        rw_note (0, 0, __LINE__, 
                 "template <class T> "
                 "std::deque<UserClass>::insert(iterator, T, T) "
                 "[with T = InputIterator] test disabled.");

    if (0 == rw_opt_no_forward_iterator)
        test_insert_range (FwdIter<UserClass>(), "FwdIter<UserClass>");
    else 
        rw_note (0, 0, __LINE__, 
                 "template <class T> "
                 "std::deque<UserClass>::insert(iterator, T, T) "
                 "[with T = ForwardIterator] test disabled.");

    if (0 == rw_opt_no_bidirectional_iterator)
        test_insert_range (BidirIter<UserClass>(), "BidirIter<UserClass>");
    else 
        rw_note (0, 0, __LINE__, 
                 "template <class T> "
                 "std::deque<UserClass>::insert(iterator, T, T) "
                 "[with T = BidirectionalIterator] test disabled.");

    if (0 == rw_opt_no_random_iterator)
        test_insert_range (RandomAccessIter<UserClass>(),
                           "RandomAccessIter<UserClass>");
    else 
        rw_note (0, 0, __LINE__, 
                 "template <class T> "
                 "std::deque<UserClass>::insert(iterator, T, T) "
                 "[with T = RandomAccessIterator] test disabled."); 

    //////////////////////////////////////////////////////////////////
    // exercise deque::insert(iterator, int, int)

    rw_info (0, 0, 0, 
             "template <class IntType> "
             "std::deque<IntType>::"
             "insert(iterator, IntType, IntType)");

    if (0 == rw_opt_no_right_thing) {
        test_insert_int_range ((signed char)0, "signed char");
        test_insert_int_range ((unsigned char)0, "unsigned char");
        test_insert_int_range (short (), "short");
        test_insert_int_range ((unsigned short)0, "unsigned short");
        test_insert_int_range (int (), "int");
        test_insert_int_range ((unsigned int)0, "unsigned int");
        test_insert_int_range (long (), "long");
        test_insert_int_range ((unsigned long)0, "unsigned long");

#ifdef _RWSTD_LONG_LONG

        test_insert_int_range ((_RWSTD_LONG_LONG)0, 
                               "long long");
        test_insert_int_range ((unsigned _RWSTD_LONG_LONG)0, 
                               "unsigned long long");

#endif   // _RWSTD_LONG_LONG
    }
    else
        rw_note (0, 0, __LINE__, 
                 "template <class T> "
                 "std::deque<UserClass>::insert(iterator, T, T) "
                 "[with T = IntegralType] tests disabled."); 

    //////////////////////////////////////////////////////////////////
    // exercise the validity of references across inserts at the ends

    test_insert_references ();
}

/**************************************************************************/

template <class Iterator>
void test_assign (int line, int exceptions,
                  const Iterator &dummy, int nelems,
                  const char *seq, std::size_t seqlen,
                  const char *asn, std::size_t asnlen,
                  const char *res, std::size_t reslen)
{
    const UserClass* const xseq = UserClass::from_char (seq, seqlen + 1);
          UserClass* const xasn = UserClass::from_char (asn, asnlen + 1);

    Deque deq = seqlen ? Deque (xseq, xseq + seqlen) : Deque ();

    // format a string describing the function call being exercised
    // (used in diagnostic output below)
    char* funcall = 0;
    std::size_t len = 0;

    static const int cwidth = sizeof (*xseq);

    rw_asnprintf (&funcall, &len, 
                  "deque(\"%{X=*.*}\").assign("
                  "%{?}\"%{X=*.*}\")%{:}%d, %d)%{;}", 
                  cwidth, seqlen, -1, xseq, 
                  nelems < 0, 
                  cwidth, asnlen, -1, xasn, 
                  nelems, *asn);

    std::size_t n_copy = UserClass::n_total_copy_ctor_;
    std::size_t n_asgn = UserClass::n_total_op_assign_;

    // create a dummy deque iterator to pass to exception_loop
    // (the object will not be used by the functiuon)
    const Deque::iterator dummy_it = deq.begin ();

    if (nelems < 0) {   // assign(Iterator, Iterator)

        if (asnlen > 1)
            exceptions = 0;

        const Iterator first =
            make_iter (xasn, xasn, xasn + asnlen, dummy);

        const Iterator last =
            make_iter (xasn + asnlen, xasn, xasn + asnlen, dummy);

        exception_loop (line, AssignRange, funcall, exceptions,
                        deq, dummy_it, nelems, 0, first, last,
                        &n_copy, &n_asgn);
    }
    else {   // assign(size_type, const_reference)
        if (nelems > 1)
            exceptions = 0;

        exception_loop (line, Assign_n, funcall, exceptions,
                        deq, dummy_it, nelems, xasn, dummy, dummy,
                        &n_copy, &n_asgn);
    }

    // verify the expected size of the deque after assignment
    rw_assert (deq.size () == reslen, 0, line,
               "line %d: %s: size == %zu, got %zu\n",
               __LINE__, funcall, reslen, deq.size ());

    // verify the expected contents of the deque after assignment
    const Deque::const_iterator resbeg = deq.begin ();
    const Deque::const_iterator resend = deq.end ();

    for (Deque::const_iterator it = resbeg; it != resend; ++it) {

        const Deque::size_type inx = it - resbeg;

        _RWSTD_ASSERT (inx < deq.size ());

        if ((*it).data_.val_ != UChar (res [inx])) {

            char* const got = new char [deq.size () + 1];

            for (Deque::const_iterator i = resbeg; i != resend; ++i) {

                const Deque::size_type inx_2 = i - resbeg;

                _RWSTD_ASSERT (inx_2 < deq.size ());

                got [inx_2] = char ((*i).data_.val_);
            }

            got [deq.size ()] = '\0';

            rw_assert (false, 0, line,
                       "line %d: %s: expected %s, got %s\n",
                       __LINE__, funcall, res, got);

            delete[] got;
            break;
        }
    }

    // set asnlen to the number of elements assigned to the container
    if (0 <= nelems)
        asnlen = std::size_t (nelems);

    // verify the complexity of the operation in terms of the number
    // of calls to the copy ctor and assignment operator on value_type

    // the number of invocations of the copy ctor and the assignment
    // operator depends on whether the implementation of assign()
    // strictly follows the requirements in 23.2.1.1, p7 or p8 and
    // destroys the existing elements before inserting the new ones,
    // or whether it assigns the new elements over the existing ones

    const std::size_t expect_copy = seqlen < asnlen ? asnlen - seqlen : 0;
    const std::size_t expect_asgn = asnlen < seqlen ? asnlen : seqlen;

    rw_assert (n_copy == expect_copy, __FILE__, line,
               "line %d: %s: expected %zu invocations "
               "of UserClass::UserClass(const UserClass&), got %d\n",
               __LINE__, funcall, expect_copy, n_copy);

    rw_assert (n_asgn == expect_asgn, __FILE__, line,
               "line %d: %s: expected %zu invocations "
               "of UserClass::operator=(const UserClass&), got %d\n",
               __LINE__, funcall, expect_asgn, n_asgn);

    // Free funcall storage
    std::free (funcall);

    delete[] xasn;

    // cast away constness to work around an HP aCC 6.16 bug
    // see http://issues.apache.org/jira/browse/STDCXX-802
    delete[] const_cast< UserClass* >(xseq);
}


template <class Iterator>
void test_assign_range (const Iterator &it, const char* itname)
{
    rw_info (0, 0, 0, "std::deque<UserClass>::assign(%s, %s)", itname, itname);

    static const char seq[] = "abcdefghijklmnopqrstuvwxyz";
    static const char asn[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZ";

    for (std::size_t i = 0; i != sizeof seq - 1; ++i) {
        for (std::size_t j = 0; j != sizeof asn - 1; ++j) {

            test_assign (__LINE__, 0, it, -1, seq, i, asn, j, asn, j);
        }
    }
}


void test_assign ()
{
    //////////////////////////////////////////////////////////////////
    // exercise
    //   deque::assign(size_type, const_reference)

    rw_info (0, 0, 0,
             "std::deque<UserClass>::assign(size_type, const_reference)");

    static const char seq[] = "abcdefghijklmnopqrstuvwxyz";
    static const char res[] = "AAAAAAAAAAAAAAAAAAAAAAAAAA";

    for (std::size_t i = 0; i != sizeof seq - 1; ++i) {
        for (std::size_t j = 0; j != sizeof seq - 1; ++j) {

            test_assign (__LINE__, -1, (UserClass*)0, int (j),
                         seq, i, res, 1U, res, j);
        }
    }

    //////////////////////////////////////////////////////////////////
    // exercise
    //   template <class InputIterator>
    //   deque::assign(InputIterator, InputIterator)

    rw_info (0, 0, 0, 
             "template <class InputIterator> "
             "std::deque<UserClass>::assign(InputIterator, InputIterator)");

    if (0 == rw_opt_no_input_iterator)
        test_assign_range (InputIter<UserClass>(0, 0, 0),
                           "InputIter<UserClass>");
    else 
        rw_note (0, 0, __LINE__, 
                 "template <class T> "
                 "std::deque<UserClass>::assign(T, T) [with T = InputIterator]"
                 "test disabled.");

    if (0 == rw_opt_no_forward_iterator)
        test_assign_range (FwdIter<UserClass>(), "FwdIter<UserClass>");
    else 
        rw_note (0, 0, __LINE__, 
                 "template <class T> "
                 "std::deque<UserClass>::assign(T, T) "
                 "[with T = ForwardIterator] test disabled.");

    if (0 == rw_opt_no_bidirectional_iterator)
        test_assign_range (BidirIter<UserClass>(), "BidirIter<UserClass>");
    else 
        rw_note (0, 0, __LINE__, 
                 "template <class T> "
                 "std::deque<UserClass>::assign(T, T) "
                 "[with T = BidirectionalIterator] test disabled.");

    if (0 == rw_opt_no_random_iterator)
        test_assign_range (RandomAccessIter<UserClass>(),
                           "RandomAccessIter<UserClass>");
    else 
        rw_note (0, 0, __LINE__, 
                 "template <class T> "
                 "std::deque<UserClass>::assign(T, T) "
                 "[with T = RandomAccessIterator] test disabled.");
}

/**************************************************************************/

void test_erase (int line,
                 const char *seq, std::size_t seqlen,
                 std::size_t begoff, std::size_t len,
                 const char *res, std::size_t reslen)
{
    const UserClass* const xseq = UserClass::from_char (seq, seqlen + 1);

    Deque deq = seqlen ? Deque (xseq, xseq + seqlen) : Deque ();
    const Deque::iterator start = deq.begin () + begoff;
 
    std::size_t n_copy = UserClass::n_total_copy_ctor_;
    std::size_t n_asgn = UserClass::n_total_op_assign_;

    char* funcall = 0;
    std::size_t buflen = 0;

    static const int cwidth = sizeof (*xseq);

    if (std::size_t (-1) == len) {   // erase(iterator)

        rw_asnprintf (&funcall, &buflen,
                      "deque(\"%{X=*.*}\").erase(%{?}end()%{:}"
                      "%{?}begin () + %zu%{:}begin ()%{;}%{;}",
                      cwidth, seqlen, -1, xseq,
                      begoff == deq.size (), begoff, begoff);
        
        exception_loop (line, Erase_1, funcall, 0,
                        deq, start, 1, 0, (UserClass*)0, (UserClass*)0,
                        &n_copy, &n_asgn);
    }
    else {   // assign(size_type, const_reference)

        const Deque::iterator end = start + len;

        rw_asnprintf (&funcall, &buflen,
                      "deque(\"%{X=*.*}\").erase(%{?}end()%{:}"
                      "%{?}begin () + %zu%{:}begin ()%{;}%{;}"
                      "%{?})%{:}%{?}, end ())%{:}%{?}, begin ())"
                      "%{:}begin () + %zu%{;}%{;}%{;}",
                      cwidth, seqlen, -1, xseq,
                      begoff == deq.size (), begoff, begoff,
                      std::size_t (-1) == len,
                      end == deq.end (),
                      end == deq.begin (),
                      end - deq.begin ());
        
        exception_loop (line, EraseRange, funcall, 0,
                        deq, start, int (len), 0,
                        (UserClass*)0, (UserClass*)0,
                        &n_copy, &n_asgn);

    }

    // verify the expected size of the deque after erasure
    rw_assert (deq.size () == reslen, 0, line,
               "line %d: %s: size == %zu, got %zu\n",
               __LINE__, funcall, reslen, deq.size ());

    // verify the expected contents of the deque after assignment
    const Deque::const_iterator resbeg = deq.begin ();
    const Deque::const_iterator resend = deq.end ();

    for (Deque::const_iterator it = resbeg; it != resend; ++it) {
        if ((*it).data_.val_ != UChar (res [it - resbeg])) {

            char* const got = new char [deq.size () + 1];

            for (Deque::const_iterator i = resbeg; i != resend; ++i) {
                got [i - resbeg] = char ((*i).data_.val_);
            }

            got [deq.size ()] = '\0';

            rw_assert (false, 0, line,
                       "line %d: %s: expected %s, got %s\n",
                       __LINE__, funcall, res, got);

            delete[] got;
            break;
        }
    }

#if 0
    // set asnlen to the number of elements assigned to the container
    if (0 <= nelems)
        asnlen = std::size_t (nelems);

    // verify the complexity of the operation in terms of the number
    // of calls to the copy ctor and assignment operator on value_type

    // the number of invocations of the copy ctor and the assignment
    // operator depends on whether the implementation of assign()
    // strictly follows the requirements in 23.2.1.1, p7 or p8 and
    // destroys the existing elements before inserting the new ones,
    // or whether it assigns the new elements over the existing ones

    const std::size_t expect_copy = seqlen < asnlen ? asnlen - seqlen : 0;
    const std::size_t expect_asgn = asnlen < seqlen ? asnlen : seqlen;

    rw_assert (n_copy == int (expect_copy), 0, line,
               "line %d: %s: expected %zu invocations "
               "of UserClass::UserClass(const UserClass&), got %d\n",
               __LINE__, funcall, expect_copy, n_copy);

    rw_assert (n_asgn == int (expect_asgn), 0, line,
               "line %d: %s: expected %zu invocations "
               "of UserClass::operator=(const UserClass&), got %d\n",
               __LINE__, funcall, expect_asgn, n_asgn);
#endif

    std::free (funcall);

    // cast away constness to work around an HP aCC 6.16 bug
    // see http://issues.apache.org/jira/browse/STDCXX-802
    delete[] const_cast< UserClass* >(xseq);
}

void test_erase ()
{
    //////////////////////////////////////////////////////////////////
    // exercise deque::erase(iterator)

    rw_info (0, 0, 0, "std::deque<UserClass>::erase(iterator)");

#undef TEST
#define TEST(seq, off, res) do {                        \
      test_erase (__LINE__,                             \
                             seq, sizeof seq - 1,       \
                             std::size_t (off),         \
                             std::size_t (-1),          \
                             res, sizeof res - 1);      \
    } while (0)

    TEST ("a", 0, "");

    TEST ("ab", 0, "b");
    TEST ("ab", 1, "a");

    TEST ("abc", 0, "bc");
    TEST ("abc", 1, "ac");
    TEST ("abc", 2, "ab");

    TEST ("abcd", 0, "bcd");
    TEST ("abcd", 1, "acd");
    TEST ("abcd", 2, "abd");
    TEST ("abcd", 3, "abc");

    TEST ("abcde", 0, "bcde");
    TEST ("abcde", 1, "acde");
    TEST ("abcde", 2, "abde");
    TEST ("abcde", 3, "abce");
    TEST ("abcde", 4, "abcd");

    TEST ("abcdef", 0, "bcdef");
    TEST ("abcdef", 1, "acdef");
    TEST ("abcdef", 2, "abdef");
    TEST ("abcdef", 3, "abcef");
    TEST ("abcdef", 4, "abcdf");
    TEST ("abcdef", 5, "abcde");

    TEST ("abcdefg", 0, "bcdefg");
    TEST ("abcdefg", 1, "acdefg");
    TEST ("abcdefg", 2, "abdefg");
    TEST ("abcdefg", 3, "abcefg");
    TEST ("abcdefg", 4, "abcdfg");
    TEST ("abcdefg", 5, "abcdeg");
    TEST ("abcdefg", 6, "abcdef");

    TEST ("abcdefgh", 0, "bcdefgh");
    TEST ("abcdefgh", 1, "acdefgh");
    TEST ("abcdefgh", 2, "abdefgh");
    TEST ("abcdefgh", 3, "abcefgh");
    TEST ("abcdefgh", 4, "abcdfgh");
    TEST ("abcdefgh", 5, "abcdegh");
    TEST ("abcdefgh", 6, "abcdefh");
    TEST ("abcdefgh", 7, "abcdefg");

    //////////////////////////////////////////////////////////////////
    // exercise deque::erase(iterator, iterator)

    rw_info (0, 0, 0, "std::deque<UserClass>::erase(iterator, iterator)");
}

/**************************************************************************/

struct DR_438
{
    static bool cast_used;

    DR_438 () { }

    explicit DR_438 (std::size_t) { cast_used = true; }

    template <class T> DR_438 (T) { }
};

bool DR_438::cast_used;


void test_dr_438 ()
{
    //////////////////////////////////////////////////////////////////
    // exercise the resolution of DR 438:
    //////////////////////////////////////////////////////////////////
    //
    // For every sequence defined in clause [lib.containers]
    // and in clause [lib.strings]:

    // *  If the constructor
    //
    //    template <class InputIterator>
    //    UserClass (InputIterator f, InputIterator l,
    //       const allocator_type& a = allocator_type())
    //
    //    is called with a type InputIterator that does not qualify
    //    as an input iterator, then the constructor will behave
    //    as if the overloaded constructor:
    //
    //    UserClass (size_type, const value_type& = value_type(),
    //       const allocator_type& = allocator_type())
    //
    //    were called instead, with the arguments static_cast<size_type>(f),
    //    l and a, respectively.
    //
    // *  If the member functions of the forms:
    //
    //    template <class InputIterator>       // such as insert()
    //    rt fx1(iterator p, InputIterator f, InputIterator l);
    //
    //    template <class InputIterator>       // such as append(), assign()
    //    rt fx2(InputIterator f, InputIterator l);
    //
    //    template <class InputIterator>        // such as replace()
    //    rt fx3(iterator i1, iterator i2, InputIterator f, InputIterator l);
    //
    //    are called with a type InputIterator that does not qualify
    //    as an input iterator, then these functions will behave
    //    as if the overloaded member functions:
    //
    //    rt fx1(iterator, size_type, const value_type&);
    //
    //    rt fx2(size_type, const value_type&);
    //
    //    rt fx3(iterator, iterator, size_type, const value_type&);
    //
    //    were called instead, with the same arguments.
    //
    // In the previous paragraph the alternative binding will fail
    // if f is not implicitly convertible to UserClass::size_type or
    // if l is not implicitly convertible to UserClass::value_type.
    //
    // The extent to which an implementation determines that a type
    // cannot be an input iterator is unspecified, except that
    // as a minimum integral types shall not qualify as input iterators.
    //////////////////////////////////////////////////////////////////

    rw_info (0, 0, 0, "resolution of DR 438");

#ifndef NO_DR_438_TEST

    std::deque<DR_438, std::allocator<DR_438> > dq;

    dq.assign (1, 2);

    rw_assert (!DR_438::cast_used, 0, __LINE__,
               "deque::assign(InputIterator, InputIterator)"
               "[ with InputIterator = <integral type> ] unexpectedly "
               "used explicit argument conversion");
    
    dq.insert (dq.begin (), 1, 2);

    rw_assert (!DR_438::cast_used, 0, __LINE__,
               "deque::insert(iterator, InputIterator, InputIterator) "
               "[ with InputIterator = <integral type> ] unexpectedly "
               "used explicit argument conversion");
#else   // if defined (NO_DR_438_TEST)

    rw_warning (0, 0, __LINE__, "%s; skipping test", NO_DR_438_TEST);

#endif   // NO_DR_438_TEST

}

/**************************************************************************/

int run_test (int, char**)
{
    if (0 == rw_opt_no_dr438)
        test_dr_438 ();

    static const std::size_t caps[] = {
        2, 3, 4, 5, 16, 32
    };

    for (std::size_t i = 0; i != sizeof caps / sizeof *caps; ++i) {

        new_capacity = caps [i];

        rw_info (0, 0, 0, 
                 "__rw::__rw_new_capacity<std::deque<UserClass> >(0) = %zu",
                 __rw::__rw_new_capacity (0, (Deque*)0));

        if (0 == rw_opt_no_assign)
            test_assign ();
        
        if (0 == rw_opt_no_erase)
            test_erase ();
        
        if (0 == rw_opt_no_insert)
            test_insert ();
    }

    return 0;
}

/**************************************************************************/

int main (int argc, char** argv)
{
    return rw_test (argc, argv, __FILE__,
                    "lib.deque.modifiers",
                    0 /* no comment */, run_test,
                    "|-no-dr438#"
                    "|-no-assign#"
                    "|-no-erase#"
                    "|-no-insert#"
                    "|-no-InputIterator#"
                    "|-no-ForwardIterator#"
                    "|-no-BidirectionalIterator#"
                    "|-no-RandomIterator#"
                    "|-no-right-thing#",
                    &rw_opt_no_dr438,
                    &rw_opt_no_assign,
                    &rw_opt_no_erase,
                    &rw_opt_no_insert,
                    &rw_opt_no_input_iterator,
                    &rw_opt_no_forward_iterator,
                    &rw_opt_no_bidirectional_iterator,
                    &rw_opt_no_random_iterator,
                    &rw_opt_no_right_thing);
}
