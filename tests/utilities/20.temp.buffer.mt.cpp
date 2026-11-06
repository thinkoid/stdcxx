/***************************************************************************
 *
 * 20.temp.buffer.mt.cpp - test exercising the thread safety
 *                         of std::get_temporary_buffer() and
 *                         std::return_temporary_buffer()
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
 * Copyright 2005-2008 Rogue Wave Software, Inc.
 * 
 **************************************************************************/

#include <memory>    // for get_temporary_buffer
#include <utility>   // for pair

#include <cassert>   // for assert
#include <cstddef>   // for ptrdiff_t
#include <cstdio>    // for printf()
#include <cstring>   // for memset()

#include <rw/_mutex.h>   // for __rw_guard, __rw_mutex

#include <rw_driver.h>   // for rw_test(), ...
#include <rw_thread.h>   // for rw_thread_create(), ...

/**************************************************************************/

#ifndef _RWSTD_REENTRANT
#  define MAX_THREADS 0
#else
#  define MAX_THREADS 32
#endif

/* extern */ unsigned rw_opt_nthreads = 4 <= MAX_THREADS ? 4 : MAX_THREADS;
/* extern */ unsigned rw_opt_nloops   = 1024 * 1024;

/**************************************************************************/

struct thr_args
{
    unsigned threadno_;   // thread ordinal number
    unsigned ngets_;      // number of calls to get_temporary_buffer
    unsigned nreturns_;   // number of calls to return_temporary_buffer
};


#ifndef _RWSTD_INT64_T

typedef char Type64 [8];

#else   // if defined (_RWSTD_INT64_T)

typedef _RWSTD_INT64_T Type64;

#endif   // _RWSTD_INT64_T

/**************************************************************************/

// a buffer on its way from the thread that obtained it
// to the thread that returns it
template <class T>
struct handoff
{
    T*             ptr_;      // null when the slot is empty
    std::ptrdiff_t size_;     // capacity in elements
    unsigned       origin_;   // ordinal of the thread that obtained it
};


// slot_ [i] receives the buffers of type T that thread i - 1 (modulo
// the number of threads) obtained, for thread i to return; held_ [i]
// receives those thread i still holds when it finishes, for the main
// thread to return; mailbox_mutex guards slot_
template <class T>
struct mailbox
{
    static handoff<T> slot_ [MAX_THREADS + 1];
    static handoff<T> held_ [MAX_THREADS + 1];
};

template <class T>
handoff<T> mailbox<T>::slot_ [MAX_THREADS + 1];

template <class T>
handoff<T> mailbox<T>::held_ [MAX_THREADS + 1];


static __rw::__rw_mutex mailbox_mutex;

/**************************************************************************/

template <class T>
void
test_get_buffer (thr_args *args, std::pair<T*, std::ptrdiff_t> &buf,
                 unsigned nobjs)
{
    // a buffer the next thread has not been able to take stays held
    if (buf.first)
        return;

    ++args->ngets_;

    buf = std::get_temporary_buffer<T>(std::ptrdiff_t (nobjs));

//     std::printf ("T%u: get %s: { %p, %d } == %u\n",
//                  args->threadno_, type_name (T ()),
//                  buf.first, buf.second, buf.second * sizeof (T));

    if (buf.first && buf.second) {

        const std::size_t nbytes = buf.second * sizeof (T);

        std::memset (buf.first, args->threadno_ + 1, nbytes);

        *((char*)buf.first + nbytes - 1) = '\0';
    }
}

/**************************************************************************/

// verifies the fill of the thread that obtained the buffer
// and returns it on the calling thread
template <class T>
void
test_return_buffer (thr_args *args, std::pair<T*, std::ptrdiff_t> &buf,
                    unsigned origin)
{
    ++args->nreturns_;

    std::size_t len = 0;

    if (buf.first) {

        typedef unsigned char UChar;

        for (const UChar *pb = (const UChar*)buf.first; ; ++pb, ++len) {

            if (*pb != (origin + 1)) {
                if (0 == *pb)
                    ++len;
                break;
            }
        }
    }

//     std::printf ("T%u: return %s: { %p, %d } == %u\n",
//                  args->threadno_, type_name (T ()),
//                  buf.first, buf.second, len);

    assert (len == buf.second * sizeof (T));

    std::return_temporary_buffer<T>(buf.first);
    buf.first = 0;
}

/**************************************************************************/

// deposits buf in the slot of the next thread and returns the buffer
// the previous thread deposited in the calling thread's slot, so that
// a buffer is returned on a thread other than the one that obtained
// it whenever there are two threads or more; buf stays held while the
// next thread has not taken the buffer deposited before it
template <class T>
void
test_hand_off (thr_args *args, std::pair<T*, std::ptrdiff_t> &buf)
{
    handoff<T>* const slot = mailbox<T>::slot_;

    const unsigned nthreads = rw_opt_nthreads ? rw_opt_nthreads : 1;
    const unsigned next     = (args->threadno_ + 1) % nthreads;

    handoff<T> in;

    {
        const __rw::__rw_guard guard (mailbox_mutex);

        in = slot [args->threadno_];
        slot [args->threadno_].ptr_ = 0;

        if (buf.first && 0 == slot [next].ptr_) {
            slot [next].ptr_    = buf.first;
            slot [next].size_   = buf.second;
            slot [next].origin_ = args->threadno_;

            buf.first  = 0;
            buf.second = 0;
        }
    }

    if (in.ptr_) {
        std::pair<T*, std::ptrdiff_t> taken (in.ptr_, in.size_);
        test_return_buffer (args, taken, in.origin_);
    }
}

/**************************************************************************/

// leaves a buffer the calling thread still holds for the main thread
template <class T>
void
test_leave_buffer (thr_args *args, std::pair<T*, std::ptrdiff_t> &buf)
{
    handoff<T> &held = mailbox<T>::held_ [args->threadno_];

    held.ptr_    = buf.first;
    held.size_   = buf.second;
    held.origin_ = args->threadno_;

    buf.first = 0;
}

/**************************************************************************/

// returns, on the main thread once the others have been joined,
// the buffers left in the slots and those the threads still held
template <class T>
void
test_drain (thr_args *args, T*)
{
    for (unsigned i = 0; i != MAX_THREADS + 1; ++i) {
        handoff<T>* const left [] = {
            mailbox<T>::slot_ + i, mailbox<T>::held_ + i
        };

        for (unsigned j = 0; j != sizeof left / sizeof *left; ++j) {
            if (left [j]->ptr_) {
                std::pair<T*, std::ptrdiff_t>
                    buf (left [j]->ptr_, left [j]->size_);

                test_return_buffer (args, buf, left [j]->origin_);

                left [j]->ptr_ = 0;
            }
        }
    }
}

/**************************************************************************/

extern "C" void* thr_func (void *arg)
{
    thr_args* const targs = (thr_args*)arg;

    std::printf ("thread %u starting...\n", targs->threadno_);

    std::pair<_RWSTD_INT8_T*,  std::ptrdiff_t> buf0;
    std::pair<_RWSTD_INT16_T*, std::ptrdiff_t> buf1;
    std::pair<_RWSTD_INT32_T*, std::ptrdiff_t> buf2;
    std::pair<Type64*,         std::ptrdiff_t> buf3;

    for (unsigned i = 0; i != rw_opt_nloops; ++i) {

        const unsigned nelems = (i + targs->threadno_) % 32;

        switch ((i + targs->threadno_) % 8) {

        case 0:
            test_get_buffer (targs, buf0, nelems);
            break;

        case 1:
            test_get_buffer (targs, buf1, nelems);
            break;

        case 2:
            test_get_buffer (targs, buf2, nelems);
            break;

        case 3:
            test_get_buffer (targs, buf3, nelems);
            break;

        case 4:
            test_hand_off (targs, buf0);
            break;

        case 5:
            test_hand_off (targs, buf2);
            break;

        case 6:
            test_hand_off (targs, buf1);
            break;

        case 7:
            test_hand_off (targs, buf3);
            break;
        }
    }

    // leave any buffers that remain allocated for the main thread
    test_leave_buffer (targs, buf0);
    test_leave_buffer (targs, buf1);
    test_leave_buffer (targs, buf2);
    test_leave_buffer (targs, buf3);

    return 0;
}

/**************************************************************************/

static int
run_test (int, char*[])
{
    rw_info (0, 0, 0, "std::get_temporary_buffer(ptrdiff_t)");
    rw_info (0, 0, 0, "std::return_temporary_buffer(ptrdiff_t)");

    if (0 < rw_opt_nthreads) {

        rw_thread_t tid [MAX_THREADS + 1];

        thr_args targs [MAX_THREADS + 1];

        std::memset (targs, 0, sizeof targs);

        for (unsigned i = 0; i != rw_opt_nthreads; ++i) {

            targs [i].threadno_ = i;

            rw_fatal (0 == rw_thread_create (tid + i, 0, thr_func, targs + i),
                      0, 0, "thread_create() failed");
        }
            
        for (unsigned i = 0; i != rw_opt_nthreads; ++i) {

            rw_error (0 == rw_thread_join (tid [i], 0), 0, 0,
                      "thread_join() failed");
        }
    }
    else {

        thr_args targs;

        std::memset (&targs, 0, sizeof targs);

        thr_func (&targs);

    }

    thr_args margs;

    std::memset (&margs, 0, sizeof margs);

    test_drain (&margs, (_RWSTD_INT8_T*)0);
    test_drain (&margs, (_RWSTD_INT16_T*)0);
    test_drain (&margs, (_RWSTD_INT32_T*)0);
    test_drain (&margs, (Type64*)0);

    return 0;
}

/**************************************************************************/

int main (int argc, char *argv[])
{
    return rw_test (argc, argv, __FILE__,
                    "lib.temporary.buffer",
                    0 /* no comment */,
                    run_test,
                    "|-nloops#0 "       // must be non-negative
                    "|-nthreads#0-*",   // must be in specified range
                    &rw_opt_nloops,
                    int (MAX_THREADS),
                    &rw_opt_nthreads);
}
