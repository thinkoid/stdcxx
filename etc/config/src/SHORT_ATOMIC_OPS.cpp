// checking for the __sync built-ins on a short

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
 **************************************************************************/

#include <limits.h>   // for CHAR_BIT

// the three built-ins <rw/_atomic-sync.h> uses at this width must
// compile, link and run without help from a library such as libatomic,
// which the configuration does not link; the values carry across every
// byte of the object, so an operation on fewer bytes than the type
// holds gives a wrong answer

typedef unsigned short type;

static type value;

int main ()
{
    const type high = type (1) << (sizeof (type) * CHAR_BIT - 1);

    value = type (high - 1);

    if (high != __sync_add_and_fetch (&value, 1))
        return 1;

    if (type (high - 1) != __sync_sub_and_fetch (&value, 1))
        return 2;

    if (type (high - 1) != __sync_lock_test_and_set (&value, high))
        return 3;

    return high != value ? 4 : 0;
}
