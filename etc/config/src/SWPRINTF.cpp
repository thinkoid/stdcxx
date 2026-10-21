// checking for swprintf() in <wchar.h>

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

#include <wchar.h>    // for swprintf(), wcscmp()


int main (int argc, char *argv[])
{
    // use the command line argument, if any, to keep the call
    // from being folded
    const int val = argc < 2 ? 12 : argc;

    wchar_t buf [8] = L"";

    const int n = swprintf (buf, sizeof buf / sizeof *buf, L"%d", val);

    if (argc < 2)
        return !(2 == n && 0 == wcscmp (buf, L"12"));

    return 0;
}
