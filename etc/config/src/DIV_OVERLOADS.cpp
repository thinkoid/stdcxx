// checking for overloads of div()

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

// determine whether the div() function (including
// any overloads) is defined in the usual libc headers

#include <math.h>
#include <stdio.h>
#include <stdlib.h>

#include "config.h"


int template_used;

template <class T>
ldiv_t div (T, T)
{
    ++template_used;

    return ldiv_t ();
}

template <class T>
void check_div (T t, const char *tname)
{
    template_used = 0;

    div (t, t);

    if (0 == template_used)
        printf ("%s", "// ");

    printf ("#define _RWSTD_NO_DIV_%s\n", tname);
}

int main ()
{
    // <cstdlib> reads this one: it declares the overload libc lacks
    check_div ((long)1, "LONG");

    return 0;
}
