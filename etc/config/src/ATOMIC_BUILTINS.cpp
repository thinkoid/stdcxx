// checking for the __atomic built-ins with memory orders

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

#include <stddef.h>

// the built-ins must compile, link and run on a size_t without help
// from a library such as libatomic, which the configuration does not
// link; a release store publishes the data written before it to a
// thread that reads the flag with an acquire load

static const void* data;
static size_t      flag;

int main ()
{
    static int value;

    data = &value;
    __atomic_store_n (&flag, sizeof value, __ATOMIC_RELEASE);

    const size_t size = __atomic_load_n (&flag, __ATOMIC_ACQUIRE);

    return !(sizeof value == size && &value == data);
}
