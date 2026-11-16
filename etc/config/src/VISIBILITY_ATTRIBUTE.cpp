// checking for the visibility attribute

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

// _RWSTD_EXPORT in <rw/_defs.h> spells the attribute this way and puts
// it in each of the places below; a compiler that rejects it in any of
// them gets the macro empty

#define EXPORT   __attribute__ ((__visibility__ ("default")))

// a class, its static data member and an object
struct EXPORT S
{
    void f ();

    static EXPORT int i;
};

EXPORT extern const int obj;

// a class template
template <class T>
struct EXPORT TS
{
    void f () { }
};

// an explicit specialization of a class template
template <>
struct EXPORT TS<char>
{
    void f ();
};

// a function
EXPORT void fun ();

// a function template
template <class T>
EXPORT void tfun (T) { }

// an explicit instantiation of a class template and of a function
// template
template <class T>
struct TI
{
    void f () { }
};

template struct EXPORT TI<int>;

template EXPORT void tfun (int);
