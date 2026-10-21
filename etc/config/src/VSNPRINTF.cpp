// checking for vsnprintf() in <stdio.h>

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

#include <stdarg.h>   // for va_list, va_start(), va_end()
#include <stdio.h>    // for vsnprintf()
#include <string.h>   // for strcmp()


int format (char *buf, size_t size, const char *fmat, ...)
{
    va_list va;
    va_start (va, fmat);

    const int n = vsnprintf (buf, size, fmat, va);

    va_end (va);

    return n;
}


int main (int argc, char *argv[])
{
    // use the command line argument, if any, to keep the call
    // from being folded
    const char* const str = argc < 2 ? "abc" : argv [1];

    char buf [8] = "";

    const int n = format (buf, sizeof buf, "%s", str);

    if (argc < 2)
        return !(3 == n && 0 == strcmp (buf, "abc"));

    return 0;
}
