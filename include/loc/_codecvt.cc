/***************************************************************************
 *
 * _codecvt .cc - definition of std::codecvt members
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
 * Copyright 1994-2006 Rogue Wave Software, Inc.
 * 
 **************************************************************************/


namespace std { 


template <class _InternT, class _ExternT, class _StateT>
__rw::__rw_facet_id codecvt<_InternT, _ExternT, _StateT>::id;


// outlined to avoid generating a vtable in each translation unit
// that uses the class
template <class _InternT, class _ExternT, class _StateT>
/* virtual */ codecvt<_InternT, _ExternT, _StateT>::
~codecvt () /* nothrow */
{
    // no-op
}


// outlined to avoid generating a vtable in each translation unit
// that uses the class
template <class _InternT, class _ExternT, class _StateT>
/* virtual */ codecvt_byname<_InternT, _ExternT, _StateT>::
~codecvt_byname () /* nothrow */
{
    // no-op
}

}   // namespace std
