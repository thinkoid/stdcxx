/************************************************************************
 *
 * rw_typeinfo.h - a type local to a file of the test library
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

#ifndef RW_TYPEINFO_H_INCLUDED
#define RW_TYPEINFO_H_INCLUDED


#include <typeinfo>   // for type_info

#include <rw_testdefs.h>


// the test library defines a class Unnamed in the unnamed namespace
// of one of its files; a class Unnamed in the unnamed namespace of
// another file is another type with the same name

// returns the type_info of the test library's Unnamed
const std::type_info&
rw_unnamed_typeid ();

// throws an object of the test library's Unnamed
void
rw_throw_unnamed ();


#endif   // RW_TYPEINFO_H_INCLUDED
