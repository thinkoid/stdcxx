// checking for overloads of abs()

/***************************************************************************
 *
 * Licensed to the Apache Software  Foundation (ASF) under one or more
 * contributor  license agreements.  See  the NOTICE  file distributed
 * with  this  work  for  additional information  regarding  copyright
 * ownership.   The ASF  licenses this  file to  you under  the Apache
 * License, Version  2.0 (the  License); you may  not use  this file
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
 * Copyright 1999-2007 Rogue Wave Software, Inc.
 * 
 **************************************************************************/

// determine whether the abs() function (including
// any overloads) is defined in the usual libc headers

#include <math.h>
#include <stdio.h>
#include <stdlib.h>

#include "config.h"

#ifndef _RWSTD_NO_LONG_LONG
#  define LONG_LONG   long long
#endif   // _RWSTD_NO_LONG_LONG


int template_used;

template <class T>
T abs (T t)
{
    ++template_used;

    return t;
}

template <class T>
void check_abs (T t, const char *tname)
{
    template_used = 0;

    abs (t);

    if (0 == template_used)
        printf ("%s", "// ");

    printf ("#define _RWSTD_NO_ABS_%s\n", tname);
}

int main ()
{
    // <cstdlib> reads these two: it declares the overloads libc lacks
    check_abs ((long)0, "LONG");

#if defined (LONG_LONG)
    check_abs ((LONG_LONG)0, "LLONG");
#endif   // LONG_LONG

    return 0;
}
