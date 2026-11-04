/***************************************************************************
 *
 * facet.cpp - definition of the __rw_facet class and the facet
 *             database mapping
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
 * Copyright 2001-2006 Rogue Wave Software, Inc.
 * 
 **************************************************************************/

#define _RWSTD_LIB_SRC

#include <rw/_defs.h>

#include <locale.h>   // for LC_XXX constants
#include <stdlib.h>   // for getenv()
#include <string.h>   // for memcpy(), strlen()

#include <iosfwd>         // for mbstate_t

#include "mman.h"

#include <loc/_facet.h>
#include <loc/_locale.h>
#include <loc/_localedef.h>

#include <rw/_mutex.h>

#include "locale_body.h"
#include "podarray.h"    // for __rw_chararray


namespace __rw { 

// computes LC_XXX category from a numeric facet id, returns the
// LC_XXX category for standard facets, LC_ALL for all others
int __rw_get_cat (int) _THROWS (());

// retrieves the literal name for a given category value
static inline 
const char* __rw_get_cat_name (int category) _THROWS (())
{
    // FIXME: merge this struct and lookup with __rw_cats in locale_body.cpp
    // maps LC_XXX category values to their names
    static const struct {
        int         cat;           // LC_XXX category
        const char *name;          // string corresponding to LC_XXX
    } cats [] = {
        { _RWSTD_LC_COLLATE,  "LC_COLLATE"},
        { _RWSTD_LC_CTYPE,    "LC_CTYPE"},
        { _RWSTD_LC_MONETARY, "LC_MONETARY"},
        { _RWSTD_LC_NUMERIC,  "LC_NUMERIC"},
        { _RWSTD_LC_TIME,     "LC_TIME"}

#ifdef _RWSTD_LC_MESSAGES
        , { _RWSTD_LC_MESSAGES, "LC_MESSAGES" }
#endif   // _RWSTD_LC_MESSAGES
    };

    // find facet's category name
    for (size_t i = 0; i != sizeof cats / sizeof *cats; ++i) {
        if (category == cats [i].cat)
            return cats [i].name;
    }

    return 0;
}


int __rw_facet::_C_opts = 0

#ifndef _RWSTD_NO_INITIAL_CAT_SEP
    | __rw_facet::_C_prepend_cat_sep
#endif   // _RWSTD_NO_INITIAL_CAT_SEP

#ifndef _RWSTD_NO_CAT_NAMES
    | __rw_facet::_C_use_cat_names
#endif   // _RWSTD_NO_CAT_NAMES

#ifndef _RWSTD_NO_CONDENSED_NAME
    | __rw_facet::_C_condensed_name
#endif   // _RWSTD_NO_CONDENSED_NAME

    | 0;


__rw_facet::__rw_facet (size_t __ref /* = 0 */) _THROWS (())
    : _C_name (0), _C_buf (0), _C_impdata (0), _C_impsize (0),
    _C_type (_C_unknown), _C_ref (__ref), _C_pid (0 /* invalid */)
{
}


__rw_facet::~__rw_facet () // nothrow
{
    _RWSTD_ASSERT ((!_C_name && !_C_buf) || (_C_name && _C_buf));

    static const char destroyed[] = "*** destroyed facet ***";

    // (try to) detect double deletion
    _RWSTD_ASSERT (destroyed != _C_name);
    _RWSTD_ASSERT (_C_invalid != _C_type);

    // mark invalid
    _C_type = _C_invalid;

    if (_C_name != _C_buf)
        //  MSVC requires that we cast away const before calling delete.
        delete[] const_cast< char* >(_C_name);

    // mark destroyed
    _C_name = destroyed;

    // unmap only if both `impdata' and `impsize' are valid
    // (derived facets may want to set `impdata' to their own,
    // not necessarily memory mapped, data w/o setting `impsize'
    // to something meaningful)
    if (size_t (-1) == _C_impsize)
        ::operator delete (const_cast< void* >(_C_impdata));
    else if (_C_impdata && _C_impsize) 
        __rw_release_facet_data (_C_impdata, _C_impsize);
}


void __rw_facet::_C_set_name (const char *name, char *buf, size_t size)
{
    _RWSTD_ASSERT (0 != name);
    _RWSTD_ASSERT (0 != buf || 0 == size);

    // expected to be called only from the ctor
    _RWSTD_ASSERT (0 == _C_name);
    _RWSTD_ASSERT (0 == _C_buf);

    size_t len = strlen (name) + 1;

    char *tmp = len <= size ? buf : new char [len];

    ::memcpy (tmp, name, len);
    _C_name = tmp;
    _C_buf  = buf;
}


const void* __rw_facet::_C_get_data ()
{
    // check initialization; the acquire load pairs with the release
    // stores below, which publish `impdata' through `impsize'; the
    // builders of data from the C library run once `impsize' is set,
    // under a lock of their own, and publish `impdata' itself with a
    // release store (src/punct.cpp), so it is read with acquire
    if (_RWSTD_ATOMIC_LOAD_ACQUIRE (_C_impsize, false))
        return _RWSTD_ATOMIC_LOAD_ACQUIRE (_C_impdata, false);

    // `pid' may be 0 if the facet has not been obtained
    // by a call to use_facet (but instead constructed
    // and used directly without having been installed
    // in a locale object); in that case, locale database
    // mapping for the facet is not available
    if (!_C_pid)
        return _RWSTD_ATOMIC_LOAD_ACQUIRE (_C_impdata, false);

    // lock the object
    _RWSTD_MT_GUARD (&_C_mutex);

    // check again for initialization; the object's lock does not
    // exclude the builders, hence the atomic loads
    if (_RWSTD_ATOMIC_LOAD_ACQUIRE (_C_impsize, false))
        return _RWSTD_ATOMIC_LOAD_ACQUIRE (_C_impdata, false);

    __rw_chararray locname (_C_name && _C_name [0] ? _C_name : "C");

    // implementation database mapping data
    const void* pdata = 0;
    size_t sz = 0;

    _RWSTD_ASSERT (0 != _C_pid);
    const int cat = __rw_get_cat (int (*_C_pid));

    // use facet's id and name to determine whether the type of the facet is
    // codecvt_byname<wchar_t, char, mbstate_t> (_C_type is not sufficiently
    // reliable since it is set to _C_unknown for all facet objects constructed
    // outside of the library)
    const bool is_wcodecvt_byname =
        _C_wcodecvt_byname == __rw_locale::_C_get_facet_type (*this);

    // map the database; first try with the full name (including
    // any modifiers) since such a database may actually exist
    pdata = __rw_get_facet_data (cat, sz, locname.data ());
    if (!pdata) {

        // the mapping failure can be attributed to the use of a
        // special name, e.g., <locale>.<codeset>@UCS-4 which calls
        // for the stripping of the special suffix
        __rw::__rw_chararray strip_name;

        const char*  pstr = strstr (locname.data (), "@UCS");
        const size_t plen = size_t (pstr - locname.data ());

        if (pstr && !pstr [4]) {

            // @UCS suffix is recognized irrespective of the size of wchar_t
            strip_name.append (locname.data (), plen);
            pdata = __rw_get_facet_data (cat, sz, strip_name.data ());
        }
        else if (   pstr && pstr [4] == '-' && !pstr [6]
                 && (   ('4' == pstr [5] && sizeof (wchar_t) == 4)
                     || ('2' == pstr [5] && sizeof (wchar_t) == 2))) {

            // @UCS-4 is only recognized where sizeof (wchar_t) == 4
            // @UCS-2 is only recognized where sizeof (wchar_t) == 2
            // no other modifier is recognized
            strip_name.append (locname.data (), plen);
            pdata = __rw_get_facet_data (cat, sz, strip_name.data ());
        }

        // if the type of the facet is codecvt_byname<wchar_t,char,mbstate_t>
        // then the name is possibly in the format codeset | codeset@modifier
        if (!pdata && is_wcodecvt_byname) {

            pstr = pstr ? strip_name.data () : locname.data ();
            pdata = __rw_get_facet_data (cat, sz, 0, pstr /* codeset */);
        }
        else
            pstr = 0;

        // this is the definitive result for facets other than 
        // codecvt_byname<wchar_t, char, mbstate_t>
        if (!pdata) {
            _RWSTD_ASSERT (0 == _C_impdata);

            // set `impsize' to non-zero value to avoid subsequent
            // attempts at reinitialization
            _RWSTD_ATOMIC_STORE_RELEASE (_C_impsize, size_t (-1), false);
            return 0;
        }

        if (!is_wcodecvt_byname || (pstr && pdata)) {
            _C_impdata = pdata; 
            _RWSTD_ATOMIC_STORE_RELEASE (_C_impsize, sz, false);
            return _C_impdata;
        }
    }
    else if (!is_wcodecvt_byname) {
        _C_impdata = pdata;
        _RWSTD_ATOMIC_STORE_RELEASE (_C_impsize, sz, false);
        return _C_impdata;
    }

    // for codecvt_byname facet map the database indicated by 
    // codeset_name offset in LC_CTYPE database
    const __rw::__rw_ctype_t* pctype = 
        static_cast< const __rw::__rw_ctype_t* >(pdata);
 
    _RWSTD_ASSERT (0 != pctype);

    // the facet name may be an ordinary filename or a (relative
    // or absolute) pathname; unless it's the former, prepend
    // the name of the directory containing the facet to the
    // name of the codeset before trying to map it
    __rw_chararray codeset_fname;
    const char* codeset = pctype->codeset_name ();

    const char* const slash = strrchr (locname.data (), _RWSTD_PATH_SEP);
    if (slash) {
        const size_t dirlen = size_t (slash - locname.data ()) + 1;

        codeset_fname.assign (locname.data (), dirlen);
        codeset_fname.append (codeset);
        codeset = codeset_fname.data ();
    }

    // map the codecvt database into locals: a non-zero _C_impsize is
    // what _C_data() takes to mean _C_impdata is ready, so the data
    // is stored before the size (doc/notes/analysis-facet-first-use.md,
    // chapter 2); a database that cannot be mapped is marked as none,
    // as the general path above marks it
    size_t cvtsize = 0;
    const void* const cvtdata =
        __rw_get_facet_data (cat, cvtsize, 0, codeset);

    _C_impdata = cvtdata;
    _RWSTD_ATOMIC_STORE_RELEASE (_C_impsize,
                                 cvtdata ? cvtsize : size_t (-1), false);

    // unmap the LC_CTYPE database
    __rw_munmap (pdata, sz);

    return _C_impdata;
}


const void* __rw_get_facet_data (int cat, size_t &impsize, 
                                 const char* name, const char* encoding) 
{
    __rw_chararray  pathname;
    const char     *path;

    // null name means C locale
    const char *locname = name ? name : "C";

    if ('/' == *locname) {
        // facet name is an absolute pathname to a locale/category database
        path = locname;
    }
    else {

#ifndef _RWSTD_LOCALE_ROOT_ENVVAR
#  define _RWSTD_LOCALE_ROOT_ENVVAR "RWSTD_LOCALE_ROOT"
#endif

        // look up the value of the environment variable
        const char *root = getenv (_RWSTD_LOCALE_ROOT_ENVVAR);

        // use the cwd if none given
        if (!root || !*root)
            root = "./";

        // compose the pathname of the locale/category database
        pathname.append (root);
        if (pathname.data () [pathname.size () - 1] != '/')
            pathname.append ("/", 1);

        // the database name for the facet will hold either the
        // name of the category (ie LC_CTYPE, etc.) or the name
        // of the encoding (codecvt database)
        const char* db;

        if (encoding)
            db = encoding;
        else {
            // only append the locname if we are not going to be 
            // appending an encoding
            pathname.append (locname).append ("/", 1);
            db = __rw_get_cat_name (cat);
        }
        if (!db)
            return 0;

        // append database name to the end of the pathname
        // to form a complete name of the locale/category database
        pathname.append (db);
        path = pathname.data ();
    }

    // map the database
    return __rw_mmap (path, &impsize);
}

// the counterpart of __rw_get_facet_data(), unmaps the database
void __rw_release_facet_data (const void* pv, size_t sz) _THROWS (())
{
    __rw_munmap (pv, sz);
}


// standard facet id's (i.e., those instantiated and installed
// by the library) are in the range [1, __rw_facet::_C_last_type]
// all others (i.e., id's of all user-defined facet types or
// specializations of standard facets on user-defined types)
// are in the range (__rw_facet::_C_last_type, ...)
static size_t __rw_id_gen = __rw_facet::_C_last_type + 1;


size_t __rw_facet_id::_C_init () const _THROWS (())
{
    // return immediately if already initialized; the id is stored
    // once, with release, below
    const size_t id = _RWSTD_ATOMIC_LOAD_ACQUIRE (_C_id, false);
    if (id)
        return id;

    // threads that race the first use of a facet type all find the
    // id 0; without the lock each generated an id of its own and the
    // last store won, while the others went on with ids that no
    // longer matched the facets installed under them
    _RWSTD_MT_STATIC_GUARD (__rw_facet_id);

    if (_C_id)
        return _C_id;

    // generate a unique id
    const size_t new_id = _RWSTD_ATOMIC_PREINCREMENT (__rw_id_gen, false);

    _RWSTD_ATOMIC_STORE_RELEASE (_C_id, new_id, false);

    return new_id;
}

}   // namespace __rw
