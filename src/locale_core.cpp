/***************************************************************************
 *
 * locale_core.cpp - definitions of core members of class std::locale
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
 * Copyright 2001-2007 Rogue Wave Software, Inc.
 * 
 **************************************************************************/

#define _RWSTD_LIB_SRC

#include <rw/_defs.h>

#include <loc/_facet.h>
#include <loc/_locale.h>

#include "locale_body.h"


namespace std { 


// outlined to hide implementation details
locale::locale () _THROWS (())
    : _C_body (__rw::__rw_locale::_C_manage (0, 0))
{
    _RWSTD_ASSERT (0 != _C_body);
}


// outlined to hide implementation details
locale::locale (const locale &rhs) _THROWS (())
    : _C_body (rhs._C_body)
{
    _RWSTD_ATOMIC_PREINCREMENT (_C_body->_C_ref, false);

    _RWSTD_ASSERT (0 != _C_body);
}


locale::~locale () _THROWS (())
{
    _RWSTD_ASSERT (0 != _C_body);

    if (_C_body->_C_is_managed (__rw::__rw_cat_none)) {
        // body is globally managed, let manager dispose of it
        __rw::__rw_locale::_C_manage (_C_body, _C_body->_C_name);
    }
    else {
        // body is not globally managed, delete it
        // only when its ref count drops down to 0
        if (!_RWSTD_ATOMIC_PREDECREMENT (_C_body->_C_ref, false))
            delete _C_body;
    }
}


}   // namespace std
