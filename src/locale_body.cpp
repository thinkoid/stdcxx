/***************************************************************************
 *
 * locale_body.cpp - implementation of the __rw_locale class and of
 *                   std::locale::operator=()
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
 * Copyright 2001-2008 Rogue Wave Software, Inc.
 * 
 **************************************************************************/

#define _RWSTD_LIB_SRC

#include <rw/_defs.h>

#include <new>        // for placement new
#include <locale.h>
#include <stdio.h>
#include <string.h>   // for memxxx() and strxxx() functions

#include <loc/_locale.h>
#include <rw/_once.h>

#include "access.h"
#include "locale_body.h"
#include "setlocale.h"


#ifdef _RWSTD_NO_LOCALE_NAME_FMAT

// pick an arbitrary order for categories in locale names
#  define _RWSTD_CAT_0(pfx) \
       { _RWSTD_LC_CTYPE, "LC_CTYPE", pfx::_C_ctype }
#  define _RWSTD_CAT_1(pfx) \
       { _RWSTD_LC_NUMERIC, "LC_NUMERIC", pfx::_C_numeric }
#  define _RWSTD_CAT_2(pfx) \
       { _RWSTD_LC_TIME, "LC_TIME", pfx::_C_time }
#  define _RWSTD_CAT_3(pfx) \
       { _RWSTD_LC_COLLATE, "LC_COLLATE", pfx::_C_collate }
#  define _RWSTD_CAT_4(pfx) \
       { _RWSTD_LC_MONETARY, "LC_MONETARY", pfx::_C_monetary }
#  define _RWSTD_CAT_5(pfx) \
       { _RWSTD_LC_MESSAGES, "LC_MESSAGES", pfx::_C_messages }

#endif   // _RWSTD_NO_LOCALE_NAME_FMAT



namespace __rw {


// maps LC_XXX category values to their names
extern const __rw_cats_t
__rw_cats [] = {

    // expands to { LC_XXX, "LC_XXX", __rw::__rw_locale::_C_xxx }
    // the order of these is determined at config time and corresponds to
    // the order mixed/combined locale categories returned by setlocale()
    _RWSTD_CAT_0 (__rw::__rw_locale),
    _RWSTD_CAT_1 (__rw::__rw_locale),
    _RWSTD_CAT_2 (__rw::__rw_locale),
    _RWSTD_CAT_3 (__rw::__rw_locale),
    _RWSTD_CAT_4 (__rw::__rw_locale),

#ifdef _RWSTD_CAT_5
    _RWSTD_CAT_5 (__rw::__rw_locale)
#else   // if !defined (_RWSTD_CAT_5)
    // for Windows (no LC_MESSAGES there)
    _RWSTD_CAT_0 (__rw::__rw_locale)
#endif   // _RWSTD_CAT_5

};


// fills provided buffer (if large enough) with the names of locales
// for each of the standard LC_XXX categories; if all categories come
// from the same locale, copies the name of the common locale into
// the provided buffer
//
// names of categories are in the following order:
//     LC_COLLATE, LC_CTYPE, LC_MONETARY, LC_NUMERIC, LC_TIME, LC_MESSAGES
//
// examples of output (assuming ';' as a separator):
//     "C"
//         all facets of all categories come from the "C" locale
//
//     "C;C;de_DE;C;C;C;"
//         all facets come from the "C" locale except those that belong
//         to the LC_MONETARY category, which all come from "de_DE"
// 
//     "C;en_US;*;*;*;*;"
//         facets that belong to the LC_COLLATE category come from the
//         "C" locale, those belonging to the LC_CTYPE category come
//         from "en_US", all others are from mixed locales within
//         their respective categories
//
//     "*"
//         facets belonging to each category come from all different
//         locales (e.g., ctype<char> might come from the "C" locale,
//         and ctype<wchar_t> from "ja_JP", etc.)

char* __rw_locale::
_C_get_cat_names (char *buf, size_t bufsize) const
{
    const size_t  savesize = bufsize;
    char         *savebuf  = buf;

    if (!buf)
        buf = new char [bufsize ? bufsize : (bufsize = 256)];

    *buf = '\0';

    // will point at the locale name common across all categories
    // if one such name exists; otherwise at "*" (mixed locale)
    const char *common_name = 0;

    // iterate over all categories, searching for all facets that belong
    // to that category and comparing their names. if all such facets come
    // from the same locale, the name of the category is the name of the
    // locale; otherwise "*"
    for (size_t i = 0; i != __rw_n_cats; ++i) {

        const char *cat_name = 0;

        for (size_t j = 0; j != _C_n_std_facets; ++j) {

            // verify that all facet pointers are set
            _RWSTD_ASSERT (_C_std_facets [j]);

            // ignore the non-localized {money,num}_{get,put} facets
            // and consider only moneypunct and numpunct for the
            // monetary category
            switch (_C_std_facets [j]->_C_type) {
            case __rw_facet::_C_num_get:
            case __rw_facet::_C_wnum_get:
            case __rw_facet::_C_num_put:
            case __rw_facet::_C_wnum_put:
            case __rw_facet::_C_money_get:
            case __rw_facet::_C_wmoney_get:
            case __rw_facet::_C_money_put:
            case __rw_facet::_C_wmoney_put:
                continue;
            default:
                break;
            }

            _RWSTD_ASSERT (0 != _C_std_facets [j]->_C_pid);

            // one category at a time
            if (   __rw_cats [i].cat
                != __rw_get_cat (int (*_C_std_facets [j]->_C_pid)))
                continue;

            // default facet name (if 0) is "C"
            const char *facet_name = _C_std_facets [j]->_C_name
                ? _C_std_facets [j]->_C_name : "C";

            if (cat_name && strcmp (cat_name, facet_name))
                cat_name = "*";
            else
                cat_name = facet_name;

            if (common_name) {
                // if the name of the locale for this category doesn't
                // match the common locale name so far, assign "*"
                if (strcmp (common_name, cat_name))
                    common_name = "*";
            }
            else
                common_name = cat_name;
        }

        // will be 0 iff all facets in a category are 0
        // (i.e., an internal logic error)
        _RWSTD_ASSERT (cat_name);

        if (   strlen (buf) + strlen (cat_name) + 2
            >= bufsize) {
            // reallocate buffer if necessary
            char *tmp = 0;

            try {
                tmp = new char [bufsize *= 2];
            }
            catch (...) {
                if (buf != savebuf)
                    delete[] buf;
                throw;
            }

            strcpy (tmp, buf);
            if (buf != savebuf)
                delete[] buf;
            buf = tmp;
        }

        strcat (buf, cat_name);
        strcat (buf, _RWSTD_CAT_SEP);
    }

    _RWSTD_ASSERT (common_name);

    if (__rw_facet::_C_opts & __rw_facet::_C_condensed_name) {
        // if all categories come from the same locale, copy the name
        // of that locale into the buffer
        if ('*' != common_name [0] || '\0' != common_name [1]) {
            if (   savebuf && buf != savebuf
                && savesize >= strlen (common_name)) {
                // delete allocated memory and repoint at original buffer
                delete[] buf;
                buf = savebuf;
            }
            strcpy (buf, common_name);
        }
    }

    // remove trailing separator
    const size_t len = strlen (buf);
    if (len && *_RWSTD_CAT_SEP == buf [len - 1])
        buf [len - 1] = '\0';

    return buf;
}


// returns a pointer to a character array containing locale names separated
// by _RWSTD_CAT_SEP corresponding to the named locale (typically all the
// names will be the same except possibly for the native locale which can
// consist of facets from different named locales specified by the LC_XXX
// environment variables)
static const char*
__rw_expand_name (__rw_chararray &namebuf, const char *name)
{
    const char *sep = strchr (name, *_RWSTD_CAT_SEP);

    if (sep == name) {
        // skip the first separator if it's at the beginning of `name'
        // (e.g., SunOS locale names have the format "/en/C/ja/C/it/de")
        sep = strchr (++name, *_RWSTD_CAT_SEP);
    }

    // true if all categories are from the same locale, false otherwise
    bool same_cats = true;

    if (sep) {

        // `name' contains a locale category separator; iterate over
        // all locale categories to determine the name of the locale
        // for each one of them

        for (size_t i = 0; ; sep = strchr (name, *_RWSTD_CAT_SEP)) {

            if (!sep)
                sep = name + strlen (name);

            const size_t catlen = sep - name;

            // copy the read-only substring forming a locale name
            // into a writeable buffer so that it can be NUL-terminated
            const __rw_chararray shortname (name, catlen);

            __rw_chararray longname;

            // convert the (possibly abbreviated) name or a locale alias
            // into the full name uniquely identifying the locale (e.g.,
            // all of "de," "de_DE," and "german" can expand into the
            // full canonical name "de_DE.ISO-8859-1"; this is necessary
            // to guarantee that the internal locale repository doesn't
            // contain duplicate entries)
            const char* const fullname =
                __rw_locale_name (__rw_cats [i].cat, shortname.data (),
                                  longname);

            if (!fullname)
                return 0;

            const size_t namelen = strlen (fullname);

            _RWSTD_ASSERT (0 != namelen);

            // determine if the new category name is the same as the first
            // one (if it is, it is also the same as all subsequent ones,
            // otherwise `same_cats' is already set to false)
            if (same_cats && i)
                same_cats =    !memcmp (namebuf.data (), fullname, namelen)
                            && *_RWSTD_CAT_SEP == namebuf.data () [namelen];

            namebuf.append (fullname, namelen);

            if (++i == __rw_n_cats || !*sep)
                break;

            namebuf.append (_RWSTD_CAT_SEP, 1);

            name = sep + 1;
        }
    }
    else if ('\0' == *name) {

        // no separator found, set all categories
        const __rw_setlocale loc (name, _RWSTD_LC_ALL, 1 /* nothrow */);

        if (!loc.name ())
            return 0;

        // compose a string of category names separated by CAT_SEP
        for (size_t i = 0; ; ) {

            // retrieve the name of each individual category
            const char* const fullname = setlocale (__rw_cats [i].cat, 0);

            // call to setlocale() must succeed if `loc' succeeded
            _RWSTD_ASSERT (0 != fullname);

            const size_t namelen = strlen (fullname);

            if (same_cats && i)
                same_cats =    !memcmp (namebuf.data (), fullname, namelen)
                            && *_RWSTD_CAT_SEP == namebuf.data () [namelen];

            namebuf.append (fullname, namelen);

            if (++i == __rw_n_cats)
                break;

            namebuf.append (_RWSTD_CAT_SEP, 1);
        }
    }
    
    if (*namebuf.data ()) {

        if (same_cats && __rw_facet::_C_opts & __rw_facet::_C_condensed_name) {

            // all categories were the same, store just the first one

            char* const  data   = namebuf.data ();
            const size_t sepinx = strchr (data, *_RWSTD_CAT_SEP) - data;

            // NUL-terminate at the first separator
            namebuf.acquire (data, sepinx);
            namebuf.append ("", 1);
        }
    }
    else {

        const char* const fullname =
            __rw_locale_name (_RWSTD_LC_ALL, name, namebuf);

        if (!fullname)
            return 0;
    }

    return namebuf.data ();
}


size_t __rw_locale::
_C_get_facet_inx (size_t id) const _THROWS (())
{
    // verify that facet's id is initialized
    _RWSTD_ASSERT (id);

    // standard facet's id are one greater than their index
    if (id <= _C_n_std_facets)
        return id - 1;

    _RWSTD_ASSERT (!_C_n_usr_facets || _C_usr_facets);

    // linear search through user-defined facets (if any)
    for (size_t i = 0; i != _C_n_usr_facets; ++i) {

        _RWSTD_ASSERT (_C_usr_facets [i]->_C_pid);

        if (*_C_usr_facets [i]->_C_pid == id)
            return i + _C_n_std_facets;
    }

    return _RWSTD_SIZE_MAX;
}


// the factories of the standard facets, one per slot, in the order
// of __rw_facet::_C_facet_type: slot i holds the ordinary facet of
// type 2 * i + 1 or its byname form, type 2 * i + 2
static __rw_facet::_C_ctor_t* const
__rw_std_facet_ctors [__rw_locale::_C_n_std_facets] = {
    __rw_ct_collate,
    __rw_ct_codecvt,
    __rw_ct_ctype,
    __rw_ct_moneypunct,
    __rw_ct_moneypunct_intl,
    __rw_ct_money_get,
    __rw_ct_money_put,
    __rw_ct_numpunct,
    __rw_ct_num_get,
    __rw_ct_num_put,
    __rw_ct_time_get,
    __rw_ct_time_put,
    __rw_ct_messages,
    __rw_ct_wcollate,
    __rw_ct_wcodecvt,
    __rw_ct_wctype,
    __rw_ct_wmoneypunct,
    __rw_ct_wmoneypunct_intl,
    __rw_ct_wmoney_get,
    __rw_ct_wmoney_put,
    __rw_ct_wnumpunct,
    __rw_ct_wnum_get,
    __rw_ct_wnum_put,
    __rw_ct_wtime_get,
    __rw_ct_wtime_put,
    __rw_ct_wmessages
};


// obtains from its factory the facet for slot `inx' of a body named
// `name', with no reference: the category's locale is its part of a
// combined name or the whole name, and the facet is the byname form
// unless that locale is C; {money,num}_{get,put} have no byname form
// and are always the ordinary facet
/* static */ __rw_facet* __rw_locale::
_C_get_slot_facet (const char *name, size_t inx)
{
    const __rw_facet::_C_facet_type type =
        __rw_facet::_C_facet_type (2 * inx + 1);

    __rw_facet::_C_ctor_t* const ctor = __rw_std_facet_ctors [inx];

    switch (type) {
    case __rw_facet::_C_money_get:
    case __rw_facet::_C_money_put:
    case __rw_facet::_C_num_get:
    case __rw_facet::_C_num_put:
    case __rw_facet::_C_wmoney_get:
    case __rw_facet::_C_wmoney_put:
    case __rw_facet::_C_wnum_get:
    case __rw_facet::_C_wnum_put:
        return ctor (0, 0);
    default:
        break;
    }

    __rw_chararray catname;

    const char *next = strchr (name, *_RWSTD_CAT_SEP);

    if (next) {
        // the parts of a combined name are in the order of __rw_cats
        const int cat = __rw_get_cat (int (inx + 1));

        for (size_t i = 0; i != __rw_n_cats; ++i) {
            if (__rw_cats [i].cat == cat) {
                catname.assign (name, size_t (next - name));
                break;
            }

            name = next + ('\0' != *next);
            next = strchr (name, *_RWSTD_CAT_SEP);
            if (0 == next)
                next = name + strlen (name);
        }

        name = catname.data ();
    }

    return ctor (0, __rw_is_C (name) ? 0 : name);
}


__rw_locale::
__rw_locale (const char *name)
    : _C_usr_facets (0), _C_n_usr_facets (0), _C_ref (1)
{
    _RWSTD_ASSERT (name);

    // verify that name doesn't refer to an unnamed (combined) locale
    _RWSTD_ASSERT (!strchr (name, '*'));

    if ('\0' == *name) {

        __rw_chararray fullname;
        fullname.acquire (_C_namebuf, 0);

        // get the names of locales for all standard categories
        // avoiding dynamic memory allocation if possible
        _C_name = __rw_expand_name (fullname, name);

        fullname.release ();

        // caller must guarantee that `name' is valid
        _RWSTD_ASSERT (0 != _C_name);

        // note that `name' may not be the same as `_C_name' at this point
    }
    else {
        const size_t namelen = strlen (name) + 1;

        // assume name is well-formed and valid (checked by caller)
        char* const tmp = namelen < sizeof _C_namebuf
            ? _C_namebuf : new char [namelen];

        memcpy (tmp, name, namelen);

        _C_name = tmp;
    }

    // every standard facet is built here, and no slot changes while
    // the body is shared; the factories construct a facet with no
    // reference, refs 0 in the standard's terms, and the body adds the
    // one it holds; when one cannot be built, the body drops the
    // references to the facets before it as its dtor does
    size_t inx = 0;

    try {
        for (; inx != _C_n_std_facets; ++inx) {
            _C_std_facets [inx] = _C_get_slot_facet (_C_name, inx);
            _C_add_ref (*_C_std_facets [inx]);
        }
    }
    catch (...) {
        while (inx--) {
            __rw_facet* const pfacet = _C_std_facets [inx];

            if (   0 == _C_remove_ref (*pfacet)
                && (__rw_facet::_C_unknown == pfacet->_C_type
                    || pfacet->_C_name))
                delete pfacet;
        }

        if (_C_name != _C_namebuf)
            delete[] const_cast< char* >(_C_name);

        throw;
    }
}


// convert a LC_XXX constant to a locale::category value
/* static */ int __rw_locale::
_C_LC2category (int cat) _THROWS (())
{
    switch (cat) {
    case _RWSTD_LC_ALL:      cat = __rw_cat_all; break;
    case _RWSTD_LC_COLLATE:  cat = __rw_cat_collate; break;
    case _RWSTD_LC_CTYPE:    cat = __rw_cat_ctype; break;
    case _RWSTD_LC_MONETARY: cat = __rw_cat_monetary; break;
    case _RWSTD_LC_NUMERIC:  cat = __rw_cat_numeric; break;
    case _RWSTD_LC_TIME:     cat = __rw_cat_time; break;
#ifdef _RWSTD_LC_MESSAGES
    case _RWSTD_LC_MESSAGES: cat = __rw_cat_messages; break;
#endif

    default:
        // assume `cat' is already a locale::category value
        break;
    }
    return cat;
}


__rw_locale::
~__rw_locale () _THROWS (())
{
    // verify that object isn't being destroyed prematurely
    _RWSTD_ASSERT (0 == _C_ref);

    if (_C_name != _C_namebuf)
        delete[] const_cast< char* >(_C_name);

    // decrement the reference count of each installed facet and delete
    // each facet whose refcount drops to zero, unless it is an ordinary
    // standard facet (a library type and no name): those are statics of
    // their factories (src/use_facet.h) and live on at count zero; a
    // program's facet (type 0) and a byname facet (a name) are heap
    for (size_t i = 0; i != _C_n_std_facets; ++i) {

        __rw_facet* const pfacet = _C_std_facets [i];

        _RWSTD_ASSERT (pfacet->_C_type != __rw_facet::_C_invalid);

        // decrement ref count (must not drop below 0)
        const size_t ref = _C_remove_ref (*pfacet);
        _RWSTD_ASSERT (ref + 1U != 0);

        if (   0 == ref
            && (__rw_facet::_C_unknown == pfacet->_C_type || pfacet->_C_name))
            delete pfacet;
    }

    // verify that either both are 0 or neither is 0
    _RWSTD_ASSERT (!_C_n_usr_facets == !_C_usr_facets);

    // decrement ref counts of user-defined facets and delete if necessary
    for (size_t j = 0; j != _C_n_usr_facets; ++j) {

        _RWSTD_ASSERT (_C_usr_facets [j]);

        _RWSTD_ASSERT (_C_usr_facets [j]->_C_type != __rw_facet::_C_invalid);

        // decrement ref count (must not drop below 0)
        const size_t ref = _C_remove_ref (*_C_usr_facets [j]);

        _RWSTD_ASSERT (ref + 1U != 0);

        if (0 == ref)
            delete _C_usr_facets [j];
    }

    delete[] _C_usr_facets;
}


/* static */ __rw_locale*
__rw_locale::
_C_classic ()
{
    static __rw_aligned_buffer<__rw_locale> classic_body;
    static __rw_once_t                      classic_once;

    // the body is constructed in place with the count of 1 that it
    // keeps for itself
    struct _C_init_classic {
        static void _C_init () {
            void* const store = classic_body._C_store ();
            new (store) __rw_locale (__rw_C_locale_name);
        }
    };

    __rw_once (&classic_once, _C_init_classic::_C_init);

    __rw_locale* const body = classic_body._C_data ();

    _RWSTD_ATOMIC_PREINCREMENT (body->_C_ref, false);

    return body;
}


// the body of the global locale, the classic body until the first call
// to locale::global(), and the flag of its one-time initialization
static __rw_locale* __rw_global_body;
static __rw_once_t  __rw_global_once;

// the type whose static mutex guards __rw_global_body: both accessors
// below take that one mutex, where _RWSTD_MT_STATIC_GUARD would key a
// mutex of its own on the line of each
struct __rw_global_guard_t;

// gives the global locale the classic body; runs before the guard is
// taken, so that the classic body is not built under it
static void
__rw_init_global ()
{
    __rw_global_body = __rw_locale::_C_classic ();
}


/* static */ __rw_locale*
__rw_locale::
_C_get_global ()
{
    __rw_once (&__rw_global_once, __rw_init_global);

    // the reference is taken under the guard so that a concurrent
    // locale::global() cannot release the body in between
    _RWSTD_MT_CLASS_GUARD (__rw_global_guard_t);

    __rw_locale* const body = __rw_global_body;

    _RWSTD_ASSERT (0 != body);

    _RWSTD_ATOMIC_PREINCREMENT (body->_C_ref, false);

    return body;
}


/* static */ __rw_locale*
__rw_locale::
_C_set_global (__rw_locale *body)
{
    _RWSTD_ASSERT (0 != body);

    __rw_once (&__rw_global_once, __rw_init_global);

    // the caller holds `body', so its reference for the global can be
    // taken before the guard; the previous body's passes to the caller
    _RWSTD_ATOMIC_PREINCREMENT (body->_C_ref, false);

    _RWSTD_MT_CLASS_GUARD (__rw_global_guard_t);

    __rw_locale* const prev = __rw_global_body;

    __rw_global_body = body;

    return prev;
}


/* static */ __rw_locale*
__rw_locale::
_C_get_named_body (const char *locname)
{
    _RWSTD_ASSERT (0 != locname);

    // "C" gives the classic body, and so does a name that expands to
    // it (e.g., "POSIX")
    if (__rw_is_C (locname))
        return _C_classic ();

    __rw_chararray fullname;

    {
        // __rw_locale_name(), called from the expansion, keeps the
        // locale root in a static buffer with no lock of its own
        _RWSTD_MT_STATIC_GUARD (__rw::__rw_locale);

        locname = __rw_expand_name (fullname, locname);
    }

    if (0 == locname)
        return 0;   // invalid name

    if (__rw_is_C (locname))
        return _C_classic ();

    __rw_locale* const body = new __rw_locale (locname);

    _RWSTD_ASSERT (1 == body->_C_ref);

    return body;
}


}   // namespace __rw


/***************************************************************************/


#include <memory>
#include <loc/_locale.h>

#include <rw/_defs.h>


namespace std { 


const locale&
locale::
operator= (const locale &rhs) _THROWS (())
{
    // copy body and bump up a ref count in `rhs'
    __rw::__rw_locale* const body = rhs._C_body;

    _RWSTD_ATOMIC_PREINCREMENT (rhs._C_body->_C_ref, false);


    // _C_body is never 0 except in objects still being constructed by
    // the combining ctor locale::locale<Facet>(const locale&, Facet*)

    if (_C_body) {
        // dispose of old body (dtor decrements ref count and possibly deletes)
        locale destroy (*_C_body);

        _RWSTD_UNUSED (destroy);
    }

    // assign new body
    _C_body = body;

    return *this;
}


}   // namespace std
