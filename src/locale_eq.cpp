/***************************************************************************
 *
 * locale_eq.cpp - definition of std::locale::operator==()
 *
 * $Id$
 *
 ***************************************************************************
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
 * Copyright 1994-2006 Rogue Wave Software, Inc.
 * 
 **************************************************************************/

#define _RWSTD_LIB_SRC

#include <rw/_defs.h>

#include <string.h>

#include <loc/_locale.h>

#include "locale_body.h"


namespace std {


bool locale::operator== (const locale &rhs) const
{
    _RWSTD_ASSERT (_C_body);
    _RWSTD_ASSERT (rhs._C_body);

    // 22.1.1.4, p1: the same locale or a copy, which shares the body,
    // or two locales that each have a name and the names are identical;
    // an unnamed locale is equal only to its copies (22.1.1, p8)
    if (_C_body == rhs._C_body)
        return true;

    return    0 != _C_body->_C_name
           && 0 != rhs._C_body->_C_name
           && 0 == strcmp (_C_body->_C_name, rhs._C_body->_C_name);
}


}   // namespace std
