/***************************************************************************
 *
 * 26.c.math.depr.cpp - test exercising the overloads of [lib.c.math]
 *                      in <stdlib.h> and <math.h>, [depr.c.headers]
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

// D.5, p2: each name.h header places in the global namespace what
// the cname header places in std; the overloads of 26.5 included

#include <stdlib.h>
#include <math.h>

#include <rw_driver.h>


// the result of a function template below: a call that selects one
// of them has found no overload at global scope for its arguments
template <class T>
struct Fallback
{
    operator T () const { return T (); }
};


// define function templates with the same parameters as the
// required overloads; where an overload is not declared at global
// scope, the corresponding specialization is the better match and
// its result is a Fallback, which fails the check of the type

#define FUN_1(name)                                                     \
    template <class T>                                                  \
    Fallback<T> name (T) {                                              \
        return Fallback<T> ();                                          \
    } typedef void unused_typedef

#define FUN_2(name)                                                     \
    template <class T>                                                  \
    Fallback<T> name (T, T) {                                           \
        return Fallback<T> ();                                          \
    } typedef void unused_typedef

#define FUN_2_PTR(name)                                                 \
    template <class T>                                                  \
    Fallback<T> name (T, T*) {                                          \
        return Fallback<T> ();                                          \
    } typedef void unused_typedef

#define FUN_2_INT(name)                                                 \
    template <class T>                                                  \
    Fallback<T> name (T, int) {                                         \
        return Fallback<T> ();                                          \
    } typedef void unused_typedef

#define FUN_2_PINT(name)                                                \
    template <class T>                                                  \
    Fallback<T> name (T, int*) {                                        \
        return Fallback<T> ();                                          \
    } typedef void unused_typedef

FUN_1 (abs);
FUN_1 (acos);
FUN_1 (asin);
FUN_1 (atan);
FUN_2 (atan2);
FUN_1 (ceil);
FUN_1 (cos);
FUN_1 (cosh);
FUN_1 (exp);
FUN_1 (fabs);
FUN_1 (floor);
FUN_2 (fmod);
FUN_2_PINT (frexp);
FUN_2_INT (ldexp);
FUN_1 (log);
FUN_1 (log10);
FUN_2_PTR (modf);
FUN_2 (pow);
FUN_2_INT (pow);
FUN_1 (sin);
FUN_1 (sinh);
FUN_1 (sqrt);
FUN_1 (tan);
FUN_1 (tanh);

template <class T>
Fallback<ldiv_t> div (T, T)
{
    return Fallback<ldiv_t> ();
}

/**************************************************************************/

template <class T>
struct Tag { };

// 1 when the type of the second argument is T, -1 when it is a
// Fallback, 0 when it is another type
template <class T>
int type_check (Tag<T>, T) { return 1; }

template <class T, class U>
int type_check (Tag<T>, U) { return 0; }

template <class T, class U>
int type_check (Tag<T>, Fallback<U>) { return -1; }


static void
check_type (int result, int line, const char *call, const char *tname)
{
    rw_assert (1 == result, 0, line,
               "%s at global scope returns %s: %{?}no overload for "
               "the arguments%{:}the result has another type%{;}",
               call, tname, -1 == result);
}

#define CHECK_TYPE(T, call)                                             \
    check_type (type_check (Tag< T > (), call), __LINE__, #call, #T)

// the float and the long double overload of a function of <cmath>
#define CHECK_FLT_LDBL(name, args_flt, args_ldbl)                       \
    CHECK_TYPE (float, name args_flt);                                  \
    CHECK_TYPE (long double, name args_ldbl)

/**************************************************************************/

static void
test_abs_div ()
{
    rw_info (0, 0, __LINE__, "26.5, p4: abs (long) and div (long, long)");

    long l = -1;

    CHECK_TYPE (long, abs (l));
    CHECK_TYPE (ldiv_t, div (l, l));

#if _RWSTD_LONG_MAX > _RWSTD_INT_MAX

    // abs (int) and div (int, int) would see the argument truncated
    l = -5000000000L;

    const long labs_val = abs (l);

    rw_assert (5000000000L == labs_val, 0, __LINE__,
               "abs (%ld) == %ld, expected 5000000000", l, labs_val);

    const ldiv_t ldiv_val = div (l, 2L);

    rw_assert (-2500000000L == ldiv_val.quot && 0 == ldiv_val.rem,
               0, __LINE__,
               "div (%ld, 2) == { %ld, %ld }, expected { -2500000000, 0 }",
               l, ldiv_val.quot, ldiv_val.rem);

#else   // if _RWSTD_LONG_MAX <= _RWSTD_INT_MAX

    rw_note (0, 0, __LINE__,
             "long is no wider than int: abs (long) and div (long, long) "
             "values not checked");

#endif   // _RWSTD_LONG_MAX > _RWSTD_INT_MAX

#ifdef _RWSTD_LONG_LONG

    typedef _RWSTD_LONG_LONG LLong;

    // abs (int) would see the argument truncated
    const LLong ll = LLong (-5000000) * 1000;

    CHECK_TYPE (LLong, abs (ll));

    const LLong llabs_val = abs (ll);

    rw_assert (LLong (5000000) * 1000 == llabs_val, 0, __LINE__,
               "abs (%lld) == %lld, expected 5000000000", ll, llabs_val);

#endif   // _RWSTD_LONG_LONG

}


static void
test_abs_floating ()
{
    rw_info (0, 0, __LINE__,
             "26.5, p6: abs (float), abs (double), abs (long double)");

    // abs (int) would see the argument truncated
    const float       f  = -1.5f;
    const double      d  = -1.5;
    const long double ld = -1.5L;

    CHECK_TYPE (float, abs (f));
    CHECK_TYPE (double, abs (d));
    CHECK_TYPE (long double, abs (ld));

    const float       fabs_val  = abs (f);
    const double      dabs_val  = abs (d);
    const long double ldabs_val = abs (ld);

    rw_assert (1.5f == fabs_val, 0, __LINE__,
               "abs (%g) == %g, expected 1.5", f, fabs_val);
    rw_assert (1.5 == dabs_val, 0, __LINE__,
               "abs (%g) == %g, expected 1.5", d, dabs_val);
    rw_assert (1.5L == ldabs_val, 0, __LINE__,
               "abs (%Lg) == %Lg, expected 1.5", ld, ldabs_val);
}


static void
test_math ()
{
    rw_info (0, 0, __LINE__,
             "26.5, p6: the float and long double overloads of <math.h>");

    float       f  = 0.5f;
    long double ld = 0.5L;
    double      d  = 0.5;
    int         i  = 1;

    CHECK_FLT_LDBL (acos, (f), (ld));
    CHECK_FLT_LDBL (asin, (f), (ld));
    CHECK_FLT_LDBL (atan, (f), (ld));
    CHECK_FLT_LDBL (atan2, (f, f), (ld, ld));
    CHECK_FLT_LDBL (ceil, (f), (ld));
    CHECK_FLT_LDBL (cos, (f), (ld));
    CHECK_FLT_LDBL (cosh, (f), (ld));
    CHECK_FLT_LDBL (exp, (f), (ld));
    CHECK_FLT_LDBL (fabs, (f), (ld));
    CHECK_FLT_LDBL (floor, (f), (ld));
    CHECK_FLT_LDBL (fmod, (f, f), (ld, ld));
    CHECK_FLT_LDBL (frexp, (f, &i), (ld, &i));
    CHECK_FLT_LDBL (ldexp, (f, i), (ld, i));
    CHECK_FLT_LDBL (log, (f), (ld));
    CHECK_FLT_LDBL (log10, (f), (ld));
    CHECK_FLT_LDBL (modf, (f, &f), (ld, &ld));
    CHECK_FLT_LDBL (pow, (f, f), (ld, ld));
    CHECK_FLT_LDBL (pow, (f, i), (ld, i));
    CHECK_FLT_LDBL (sin, (f), (ld));
    CHECK_FLT_LDBL (sinh, (f), (ld));
    CHECK_FLT_LDBL (sqrt, (f), (ld));
    CHECK_FLT_LDBL (tan, (f), (ld));
    CHECK_FLT_LDBL (tanh, (f), (ld));

    CHECK_TYPE (double, pow (d, i));
}

/**************************************************************************/

static int
run_test (int, char*[])
{
    test_abs_div ();
    test_abs_floating ();
    test_math ();

    return 0;
}

/**************************************************************************/

int main (int argc, char *argv[])
{
    return rw_test (argc, argv, __FILE__,
                    "depr.c.headers",
                    0 /* no comment */,
                    run_test,
                    0 /* no command line options */);
}
