/***************************************************************************
 *
 * use_facet.h
 *
 * This is an internal header file used to implement the C++ Standard
 * Library. It should never be #included directly by a program.
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
 
#ifndef _RWSTD_USE_FACET_H_INCLUDED
#define _RWSTD_USE_FACET_H_INCLUDED


#include <new>
#include <rw/_defs.h>

#include "access.h"
#include "podarray.h"


namespace __rw {

// sets the numeric id of a standard facet type, (type + 1) / 2, and
// returns a pointer to it; a factory calls it once, from the
// initializer of a function-local static, before it returns a facet
inline _RWSTD_SIZE_T*
__rw_init_std_facet_id (const __rw_facet_id       &id,
                        __rw_facet::_C_facet_type  type)
{
    _RWSTD_SIZE_T* const pid = &__rw_access::_C_get_id (id);

    *pid = static_cast< _RWSTD_SIZE_T >((type + 1) / 2);

    return pid;
}

// sets the type of a facet the library constructs and its pointer
// to the id of its type
inline __rw_facet*
__rw_init_std_facet (__rw_facet                *pfacet,
                     __rw_facet::_C_facet_type  type,
                     _RWSTD_SIZE_T             *pid)
{
    __rw_access::_C_get_type (*pfacet) = type;
    __rw_access::_C_get_pid (*pfacet)  = pid;

    return pfacet;
}

}   // namespace __rw


// helper macro _RWSTD_DEFINE_FACET_FACTORY() defines a facet factory
// function called to construct, on demand, objects of specializations
// of facet class templates
// _byname facets are constructed on the heap while ordinary facets
// are constructed in static memory for efficiency and to avoid leaks,
// and are not destroyed during program lifetime
// the facet's count starts at `ref', and the caller adds the references
// it holds; the type's id and the ordinary facet are function-local
// statics, initialized once under the compiler's guard, so a facet
// shared by several threads is complete before any of them gets it

#ifndef _RWSTD_NO_BYNAME_FACET

     // definition for facets that come in both forms, plain and _byname
#  define _RWSTD_DEFINE_FACET_FACTORY(Linkage, Facet, TArgs, fid)          \
       typedef std:: Facet TArgs __rw_ ## fid ## _facet;                   \
       namespace __rw                                             \
       {                                                                   \
           Linkage __rw_facet*                                             \
           __rw_ct_ ## fid (_RWSTD_SIZE_T ref, const char *name)           \
           {                                                               \
               static _RWSTD_SIZE_T* const pid =                           \
                   __rw_init_std_facet_id (__rw_ ## fid ## _facet::id,     \
                                           __rw_facet::_C_ ## fid);        \
               if (name) {                                                 \
                   /* construct a _byname facet on the heap */             \
                   return __rw_init_std_facet (                            \
                       new std:: Facet ## _byname TArgs (name, ref),       \
                       __rw_facet::_C_ ## fid ## _byname, pid);            \
               }                                                           \
               /* construct an ordinary facet in static memory */          \
               static __rw_aligned_buffer<__rw_ ## fid ## _facet> f;       \
               static __rw_facet* const pf =                               \
                   __rw_init_std_facet (                                   \
                       new (f._C_store ()) __rw_ ## fid ## _facet (ref),   \
                       __rw_facet::_C_ ## fid, pid);                       \
               return pf;                                                  \
           }                                                               \
       } /* namespace __rw */                                              \
       typedef void __rw_unused_type

#else   // if defined (_RWSTD_NO_BYNAME_FACET)

     // definition for facets such as num_get that do not have a _byname form
#  define _RWSTD_DEFINE_FACET_FACTORY(Linkage, Facet, TArgs, fid)          \
       typedef std:: Facet TArgs __rw_ ## fid ## _facet;                   \
       namespace __rw                                             \
       {                                                                   \
           Linkage __rw_facet*                                             \
           __rw_ct_ ## fid (_RWSTD_SIZE_T ref, const char*)                \
           {                                                               \
               /* construct an ordinary facet in static memory */          \
               static __rw_aligned_buffer<__rw_ ## fid ## _facet> f;       \
               static __rw_facet* const pf =                               \
                   __rw_init_std_facet (                                   \
                       new (f._C_store ()) __rw_ ## fid ## _facet (ref),   \
                       __rw_facet::_C_ ## fid,                             \
                       __rw_init_std_facet_id (__rw_ ## fid ## _facet::id, \
                                               __rw_facet::_C_ ## fid));   \
               return pf;                                                  \
           }                                                               \
       } /* namespace __rw */                                              \
       typedef void __rw_unused_type

#endif   // _RWSTD_NO_BYNAME_FACET


#ifndef _RWSTD_NO_SPECIALIZED_FACET_ID
   // provide definitions of explicit specializations of static
   // template data members to guarantee a single instance of
   // each in a program when the compiler or linker don't
   // (e.g., VisualAge C++ on AIX)
#  define _RWSTD_SPECIALIZE_FACET_ID(Facet, TArgs)        \
      namespace std {                            \
          template <>                                     \
          __rw::__rw_facet_id Facet TArgs::               \
          id (__rw::__rw_facet_id::_C_Init);              \
      }                                                   \
      typedef void __rw_unused_type
#else   // if defined (_RWSTD_NO_SPECIALIZED_FACET_ID)
   // facet id's will not be explicitly specialized 
#  define _RWSTD_SPECIALIZE_FACET_ID(ignore1, ignore2)    \
      typedef void __rw_unused_type
#endif   // _RWSTD_NO_SPECIALIZED_FACET_ID


#define _RWSTD_SPECIALIZE_USE_FACET(fid)                              \
    namespace __rw {                                         \
        _RWSTD_EXPORT                                                 \
        const __rw_facet*                                             \
        __rw_get_facet_by_id                                          \
            (const std::locale &__loc,                                \
             const __rw_facet_id&,                                    \
             const __rw_ ## fid ## _facet*) {                         \
            return __rw_get_std_facet (__loc, __rw_facet::_C_ ## fid, \
                                       __rw_ct_ ## fid);              \
       }                                                              \
    }                                                                 \
    typedef void __rw_unused_type

#endif   // _RWSTD_USE_FACET_H_INCLUDED
