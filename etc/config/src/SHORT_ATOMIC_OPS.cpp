// checking for the __atomic built-ins on a short

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

// the five built-ins <rw/_atomic-builtins.h> uses at this width must
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

    if (high != __atomic_add_fetch (&value, 1, __ATOMIC_SEQ_CST))
        return 1;

    if (type (high - 1) != __atomic_sub_fetch (&value, 1, __ATOMIC_SEQ_CST))
        return 2;

    if (   type (high - 1)
        != __atomic_exchange_n (&value, high, __ATOMIC_SEQ_CST))
        return 3;

    if (high != value)
        return 4;

    __atomic_store_n (&value, type (high - 1), __ATOMIC_RELEASE);

    if (type (high - 1) != __atomic_load_n (&value, __ATOMIC_ACQUIRE))
        return 5;

    return 0;
}
