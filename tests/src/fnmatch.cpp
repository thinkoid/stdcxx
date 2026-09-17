/************************************************************************
 *
 * fnmatch.cpp - definitions of testsuite helpers
 *
 * $Id$
 *
 ************************************************************************
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

// expand _TEST_EXPORT macros
#define _RWSTD_TEST_SRC
#include <rw_fnmatch.h>
#include <fnmatch.h>   // for fnmatch()


_TEST_EXPORT int
rw_fnmatch (const char *pattern, const char *string, int)
{
    RW_ASSERT (pattern);
    RW_ASSERT (string);

    // The supported platforms provide POSIX matching; see
    // doc/notes/test-baseline.md, section 3.6.1.
    return 0 != ::fnmatch (pattern, string, 0);
}
