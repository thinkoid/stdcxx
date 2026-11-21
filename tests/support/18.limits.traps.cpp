/***************************************************************************
 *
 * 18.limits_traps.cpp - test exercising std::numeric_limits::traps
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
 * Copyright 2005-2006 Rogue Wave Software.
 * 
 **************************************************************************/

#include <limits>

#include <csignal>    // for SIGFPE, signal

#include <rw_any.h>      // for rw_any_t
#include <rw_cmdopt.h>   // for rw_enabled()
#include <rw_driver.h>   // for rw_test(), ...

/**************************************************************************/

// use siglongjmp() and sigsetjmp() on Linux to avoid
// http://sourceware.org/bugzilla/show_bug.cgi?id=2351
#include <setjmp.h>    // for siglongjmp, sigsetjmp

sigjmp_buf jmp_env;

extern "C" {

void handle_fpe (int)
{
    siglongjmp (jmp_env, 1);
}

}   // extern "C"


/**************************************************************************/

// divides in T, the type of the quotient (C++03 5.6, p2); read from
// volatile objects, the operands have no value the compiler can know,
// and stored in one, the quotient is computed though nothing reads it
template <class T>
void divide (T one, T zero)
{
    const volatile T dividend = one;
    const volatile T divisor  = zero;
    volatile T       quotient = dividend / divisor;

    _RWSTD_UNUSED (quotient);
}


template <class numT>
void test_traps (numT, int lineno)
{
    static const char* const tname = rw_any_t (numT ()).type_name ();

    if (!rw_enabled (tname)) {
        rw_note (0, 0, 0, "numeric_limits<%s>::traps test disabled", tname);
        return;
    }

    const bool traps = std::numeric_limits<numT>::traps;

    rw_info (0, 0, 0, "std::numeric_limits<%s>::traps = %b", tname, traps);

    std::signal (SIGFPE, handle_fpe);

    // the call is the whole controlling expression (C99 7.13.1.1, p4),
    // and trapped changes only after the jump, so its value is not
    // indeterminate there (C99 7.13.2.1, p3)
    bool trapped = false;

    if (sigsetjmp (jmp_env, SIGFPE)) {
        // the handler of SIGFPE jumped back from the division below
        trapped = true;
    }
    else {
        // the operands are divided as they promote (C++03 5.3.1, p6):
        // a bool divisor can only be 1 where the quotient is defined,
        // so GCC folds the division of two bools to the dividend
        divide (+numT (1), +numT ());
    }

    rw_assert (trapped == traps, 0, lineno,
               "numeric_limits<%s>::traps == %b, got %b",
               tname,  trapped, traps);
}

/**************************************************************************/

static int
run_test (int, char*[])
{
#define TEST(T)   test_traps ((T)0, __LINE__)

#ifndef _RWSTD_NO_NATIVE_BOOL
    TEST (bool);
#endif   // _RWSTD_NO_NATIVE_BOOL

    TEST (char);
    TEST (signed char);
    TEST (unsigned char);

    TEST (short);
    TEST (unsigned short);
    TEST (int);
    TEST (unsigned int);
    TEST (long);
    TEST (unsigned long);

#ifndef _RWSTD_NO_LONG_LONG
    TEST (_RWSTD_LONG_LONG);
    TEST (unsigned _RWSTD_LONG_LONG);
#endif   // _RWSTD_NO_LONG_LONG

    TEST (wchar_t);

    TEST (float);
    TEST (double);

    TEST (long double);

    return 0;
}

/**************************************************************************/

int main (int argc, char *argv[])
{
    return rw_test (argc, argv, __FILE__,
                    "lib.numeric.limits.members",
                    "traps data member",
                    run_test,
                    0, 0);
}
