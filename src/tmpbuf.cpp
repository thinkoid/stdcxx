/**************************************************************************
 *
 * tmpbuf.cpp - definition of the __rw_tmpbuf() helper
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

#define _RWSTD_LIB_SRC

#include <rw/_allocator.h>   // for __rw_allocate(), ...

#include <stddef.h>          // for size_t

#include <rw/_defs.h>


namespace __rw {


// extern "C" to avoid binary incompatibility due to changes
// in function arguments (e.g., their sign)
extern "C" {


// used by std::get_temporary_buffer<>()
_RWSTD_EXPORT size_t
__rw_tmpbuf (void **pptr, _RWSTD_PTRDIFF_T nelems, size_t size)
{
    _RWSTD_ASSERT (0 != pptr);
    _RWSTD_ASSERT (0 != size);

    // detect overflow and fail
    size_t nbytes = nelems * size;

    if (   nelems < 0
        || nbytes / size != static_cast< size_t >(nelems)
        || (nelems && nbytes / nelems != size)) {
        *pptr = 0;
        return 0;
    }

    if (*pptr) {
        // a buffer being returned, on any thread
        __rw_deallocate (*pptr, 0);
        return 0;
    }

    // allocating a new buffer

    if (!nbytes) {
        // don't waste memory for 0-size requests
        *pptr = 0;
        return 0;
    }

    for ( ; ; ) {
        try {
            *pptr = __rw_allocate (nbytes, 0);
            return nbytes / size;
        }
        catch (...) {
            *pptr = 0;
        }

        if (1 == nelems)
            return 0;

        // 20.4.3, p1-2: the capacity may be less than the request,
        // so a request memory cannot satisfy is halved, rounding up;
        // nelems + 1 would overflow at PTRDIFF_MAX
        nelems = nelems / 2 + nelems % 2;
        nbytes = nelems * size;
    }
}

}   // extern "C"


}   // namespace __rw
