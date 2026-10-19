/**************************************************************************
 *
 * examples.h - common definitions to condition example code for broken
 *              compilers; must be #included last, _after_ all lib headers.
 *
 * The goal of this file is to allow completely conforming syntax
 * in examples' code even with non-conforming compilers.
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
 * Copyright 1994-2006 Rogue Wave Software.
 * 
 **************************************************************************/

#ifndef EXAMPLES_H_INCLUDED
#define EXAMPLES_H_INCLUDED




#include <cstddef>  // for ptrdiff_t

#ifdef _RWSTD_NO_LIBC_IN_STD

namespace std { 

// accomdate compilers that don't have size_t in namespace std
using ::size_t;
using ::ptrdiff_t;

}   // namespace std

#endif   // _RWSTD_NO_LIBC_IN_STD





// undefine macros #defined by some evil C libraries (e.g., MSVC)
#undef max
#undef min


#endif   // EXAMPLES_H_INCLUDED
