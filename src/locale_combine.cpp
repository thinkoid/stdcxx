/***************************************************************************
 *
 * locale_combine.cpp - definition of std::locale::combine() and related
 *                      member functions
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
 * Copyright 1994-2008 Rogue Wave Software, Inc.
 * 
 **************************************************************************/

#define _RWSTD_LIB_SRC

#include <rw/_defs.h>

#include <locale.h>
#include <stdio.h>
#include <string.h>   // for memxxx() and strxxx() functions

#include <loc/_locale.h>

#include "access.h"
#include "locale_body.h"
#include "setlocale.h"


namespace __rw {


// constructs a new locale name from two locale names, combining
// all but the category given by `cat' from `name_1' and the rest
// from `name_2'
static void
__rw_combine_names (__rw_chararray &locname,
                    const char *name_1, const char *name_2, int cat)
{
    _RWSTD_ASSERT (0 != name_1);
    _RWSTD_ASSERT (0 != name_2);

    // `cat' must be a locale::category (and not a LC_XXX value)
    _RWSTD_ASSERT (__rw_locale::_C_check_category (cat));

    for (size_t i = 0; i != __rw_n_cats; ++i) {
        const char *nxt1 = strchr (name_1, *_RWSTD_CAT_SEP);
        const char *nxt2 = strchr (name_2, *_RWSTD_CAT_SEP);

        if (!nxt1 || !*nxt1)
            nxt1 = name_1 + strlen (name_1);

        if (!nxt2 || !*nxt2)
            nxt2 = name_2 + strlen (name_2);

        // convert a LC_XXX constant to a locale::category value
        const int c = __rw_locale::_C_LC2category (__rw_cats [i].cat);

        const char *from;   // copy from where
        size_t      len;    // this many chars

        if (cat & c) {   // category comes from `other'
            from = name_2;
            len  = nxt2 - name_2;
        }
        else {   // category comes from `one'
            from = name_1;
            len  = nxt1 - name_1;
        }

        if (locname.size ())
            locname.append (_RWSTD_CAT_SEP, 1);

        locname.append (from, len);

        // advance to the next category name in each name
        if (*nxt1)
            name_1 = nxt1 + 1;

        if (*nxt2)
            name_2 = nxt2 + 1;
    }
}


// reduces a combined name whose parts are all the same to that one
// part, the form __rw_expand_name() gives such a name, so that a
// locale combined from equal parts has the name of the named locale
static void
__rw_condense_name (__rw_chararray &locname)
{
    if (0 == (__rw_facet::_C_opts & __rw_facet::_C_condensed_name))
        return;

    char* const name = locname.data ();

    const char* const sep = strchr (name, *_RWSTD_CAT_SEP);
    if (0 == sep || '\0' == *sep)
        return;

    const size_t len = size_t (sep - name);

    for (const char *part = sep + 1; ; ) {

        const char *next = strchr (part, *_RWSTD_CAT_SEP);
        if (0 == next)
            next = part + strlen (part);

        if (size_t (next - part) != len || memcmp (part, name, len))
            return;

        if ('\0' == *next)
            break;

        part = next + 1;
    }

    // NUL-terminate at the first separator
    name [len] = '\0';
    locname.acquire (name, len);
}


void __rw_locale::
_C_construct (const __rw_locale &rhs, const __rw_facet *pfacet)
{
    // called from ctor only, members are not initialized
    _C_ref = 1;

    // copy standard facets from other locale
    for (size_t i = 0; i != _C_n_std_facets; i++)
        _C_std_facets [i] = rhs._C_std_facets [i];

    _RWSTD_ASSERT (!pfacet || pfacet->_C_pid);

    // get facet's index into one of the facet arrays
    size_t inx = pfacet
        ? rhs._C_get_facet_inx (*pfacet->_C_pid) : size_t (-1);

    // extend size if facet isn't installed in `rhs'
    _C_n_usr_facets =   rhs._C_n_usr_facets
                      + (inx > rhs._C_n_usr_facets + _C_n_std_facets);

    // allocate a buffer for user facets if necessary
    _C_usr_facets = _C_n_usr_facets ? new __rw_facet* [_C_n_usr_facets] : 0;

    // copy facets from other locale
    memcpy (_C_usr_facets, rhs._C_usr_facets,
            rhs._C_n_usr_facets * sizeof *_C_usr_facets);

    // 22.1.1.2, p13: the resulting locale has no name; with no facet
    // it is a copy of `rhs' and has its name, if any
    _C_name = 0;

    if (pfacet) {
        __rw_facet* const pf = const_cast< __rw_facet* >(pfacet);

        // overwrite an existing pointer or add a new one
        if (inx < _C_n_std_facets)
            _C_std_facets [inx] = pf;
        else if (size_t (-1) == inx)
            _C_usr_facets [_C_n_usr_facets - 1] = pf;
        else
            _C_usr_facets [inx - _C_n_std_facets] = pf;
    }
    else if (rhs._C_name) {
        // simply copy name from `rhs'
        const size_t size = strlen (rhs._C_name) + 1;
        char* const name = size <= sizeof _C_namebuf
            ? _C_namebuf : new char [size];

        memcpy (name, rhs._C_name, size);
        _C_name = name;
    }

    // increment reference counts for facets (including the newly added one)
    // with the same lock-free operation that every other update of a
    // facet's count uses, _C_add_ref() and _C_remove_ref(): the atomic
    // helpers taking a mutex are atomic only with respect to each other,
    // not to the lock-free ones (include/rw/_atomic-mutex.h)
    for (inx = 0; inx != _C_n_std_facets; ++inx) {
        _C_add_ref (*_C_std_facets [inx]);
    }
    
    for (inx = 0; inx != _C_n_usr_facets; ++inx) {
        _C_add_ref (*_C_usr_facets [inx]);
    }
}


void __rw_locale::
_C_construct (const __rw_locale &one, const __rw_locale &other, int cat)
{
    // category must be valid at this point (checked by caller)
    _RWSTD_ASSERT (_C_check_category (cat));

    // called from ctor only, members are not initialized
    _C_ref = 1;

    // size is the same as that of `one'
    _C_n_usr_facets = one._C_n_usr_facets;

    // allocate a buffer for user facets if necessary
    _C_usr_facets = _C_n_usr_facets ? new __rw_facet* [_C_n_usr_facets] : 0;

    // copy standard facets from `one', and below those in `cat' from
    // `other'
    for (size_t i = 0; i != _C_n_std_facets; ++i)
        _C_std_facets [i] = one._C_std_facets [i];

    // copy user-defined facets (if any)
    memcpy (_C_usr_facets, one._C_usr_facets,
            _C_n_usr_facets * sizeof *_C_usr_facets);

    for (size_t i = 0; i != _C_n_std_facets; ++i) {

        // each facet is stored at an index equal to its (id - 1)
        // convert an LC_XXX constant to a locale::category value
        const int c = _C_LC2category (__rw_get_cat (int (i) + 1));

        if (cat & c) {
            // assign/overwrite corresponding facets
            _C_std_facets [i] = other._C_std_facets [i];
        }

        // bump up the reference count of the facet in *this
        _C_add_ref (*_C_std_facets [i]);
    }

    // increment the ref count of user-defined facets (if any)
    for (size_t j = 0; j != _C_n_usr_facets; ++j) {

        _RWSTD_ASSERT (_C_usr_facets [j]);

        _C_add_ref (*_C_usr_facets [j]);
    }

    // 22.1.1.2, p15: the resulting locale has a name if and only if
    // the first two arguments have names; the name takes each category
    // from the argument the category's facets come from
    _C_name = 0;

    if (0 == one._C_name || 0 == other._C_name)
        return;

    __rw_chararray locname;

    __rw_combine_names (locname, one._C_name, other._C_name, cat);
    __rw_condense_name (locname);

    const size_t size = locname.size () + 1;
    char* const name = size <= sizeof _C_namebuf
        ? _C_namebuf : new char [size];

    memcpy (name, locname.data (), size);
    _C_name = name;
}


__rw::__rw_locale* __rw_locale::
_C_combine (const __rw_facet *pfacet) const
{
    _RWSTD_ASSERT (pfacet);

    // template ctor: locale::locale (const locale&, Facet*)
    // with `facet' being installed (e.g., adding or replacing
    // an existing one)

    // verify that Facet::id has been initialized
    _RWSTD_ASSERT (pfacet->_C_pid);
    _RWSTD_ASSERT (*pfacet->_C_pid);

    // always a new body: the resulting locale has no name, and is equal
    // only to its copies (22.1.1.2, p13; 22.1.1.4, p1)
    __rw_locale* const plocale = new __rw_locale (*this, pfacet);

    _RWSTD_ASSERT (0 == plocale->_C_name);

    return plocale;
}


__rw_locale* __rw_locale::
_C_combine (const __rw_locale &other, int cat)
{
    // convert a possible LC_XXX constant to a locale::category value;
    // 0 selects nothing
    cat = _C_arg2category (cat);

    // verify that the category argument is valid
    if (!_C_check_category (cat))
        return 0;

    // combining ctor: locale (const locale&, const locale&, cat)

    if (this == &other || std::locale::none == cat) {
        // same as copy ctor (copying all facets from `*this')
        _RWSTD_ATOMIC_PREINCREMENT (_C_ref, false);

        return this;
    }

    // a new body, named if and only if both are named; `all' is not
    // a copy of `other', since the user facets come from `*this'
    return new __rw_locale (*this, other, cat);
}


/* static */ __rw_locale* __rw_locale::
_C_get_body (__rw_locale      *one,
             __rw_locale      *other,
             const char       *locname,
             int               cat,
             const __rw_facet *pfacet)
{
    __rw_chararray realname;

    // see if `locname' contains embedded "LC_XXX=..." sequences
    if (locname && !strncmp (locname, "LC_", 3)) {

        // convert a combined locale name (consisting of a sequence
        // of `sep'-separated LC_XXX=<name> substrings) to a normalized
        // form in the expected (internal) format and order

        const char *pcatnames [__rw_n_cats] = { 0 };

        // try the libc "native" separator first, semicolon next
        const char* const psep = strchr (locname, *_RWSTD_CAT_SEP);
        const char sep = psep ? *psep : ';';

        // redundant check for s being non-null shuts up a bogus
        // HP cadvise null pointer dereference warning #20200
        for (const char *s = locname; s && *s; ) {

            const char* const endcat = strchr (s, '=');
            if (!endcat)
                return 0;   // error: missing `=' after LC_XXX

            // go through the LC_XXX category names, trying to find
            // a match; if found, remember its position (duplicates
            // are not allowed)

            for (size_t i = 0; i != __rw_n_cats; ++i) {
                if (!strncmp (__rw_cats [i].name, s, endcat - s)) {
                    if (pcatnames [i])
                        return 0;    // error: duplicate LC_XXX

                    pcatnames [i] = endcat + 1;
                    break;
                }
            }

            // advance just past the next separator if one exists
            const char* const next = strchr (s, sep);
            s = next ? next + 1 : s + strlen (s);
        }

        // compose a normalized locale name out of category names
        for (size_t i = 0; i != __rw_n_cats; ++i) {

            if (!pcatnames [i]) {
                // use "C" for missing LC_XXX values
                pcatnames [i] = "C";
            }

            const char *endcat = strchr (pcatnames [i], sep);

            if (!endcat)
                endcat = pcatnames [i] + strlen (pcatnames [i]);

            const size_t catsize = endcat - pcatnames [i];

            // append name followed by the libc "native" separator
            realname.append (pcatnames [i], catsize);
            realname.append (_RWSTD_CAT_SEP, 1);
        }

        locname = realname.data ();
    }
    else if (locname && *locname == *_RWSTD_CAT_SEP) {
        // advance past the leading category separator if present
        ++locname;
    }

    if (one && !other && !locname && std::locale::none == cat && !pfacet) {
        // template ctor: locale::locale (const locale&, Facet*) with
        // Facet* being 0 or a facet already installed in the locale
        _RWSTD_ATOMIC_PREINCREMENT (one->_C_ref, false);
        return one;
    }

    if (!one && !other && locname && std::locale::none == cat && !pfacet) {
        // ctor: locale::locale (const char*)
        return _C_get_named_body (locname);
    }

    if (one && !other && !locname && std::locale::none == cat && pfacet) {
        // template ctor: locale::locale (const locale&, Facet*)
        // with `pfacet' being installed (e.g., adding or replacing
        // an existing one)
        // template <class Facet> locale::combine(const locale&) is
        // trivially implemented inline in terms of the template ctor
        __rw_locale* const plocale = one->_C_combine (pfacet);

        _RWSTD_ASSERT (0 != plocale);

        return plocale;
    }

    if (one && !other && locname && !pfacet) {
        // combining ctor: locale (const locale&, const char*, category)

        // construct a temporary locale from locale name `locname'
        const std::locale tmp (locname);

        // construct a locale combined from `one' and the temporary
        __rw_locale* const plocale = one->_C_combine (*tmp._C_body, cat);

        return plocale;
    }

    if (one && other && !locname && !pfacet) {
        // combining ctor: locale (const locale&, const locale&, category)
        return one->_C_combine (*other, cat);
    }

    _RWSTD_ASSERT (!"logic error: bad combination of arguments");

    return 0;
}

}   // namespace __rw


namespace std {


// outlined to hide implementation details
locale::
locale (const char *locname)
{
    // verify 22.1.1.2, p7: `locname' must not be null
    _RWSTD_REQUIRES (locname,
                     (_RWSTD_ERROR_LOCALE_BAD_NAME,
                      _RWSTD_FUNC ("locale::locale (const char*)"), "(null)"));

    _C_body = __rw::__rw_locale::_C_get_body (0, 0, locname, none, 0);

    // verify 22.1.1.2, p7: `locname' must be valid
    _RWSTD_REQUIRES (_C_body,
                     (_RWSTD_ERROR_LOCALE_BAD_NAME,
                      _RWSTD_FUNC ("locale::locale (const char*)"), locname));
}


// outlined to hide implementation details
locale::
locale (const locale &rhs, const char *locname, category cat)
{
    // verify 22.1.1.2, p10: `locname' must not be null
    _RWSTD_REQUIRES (locname,
                     (_RWSTD_ERROR_LOCALE_BAD_NAME,
                      _RWSTD_FUNC ("locale::locale (const locale&, "
                                   "const char*, category)"), "(null)"));

    // verify 22.1.1.2, p10 (missing requirement, as p14): `cat' must
    // be valid; checked here since a bad category and a bad name both
    // come back as a null body
    _RWSTD_REQUIRES (__rw::__rw_locale::_C_check_category (
                         __rw::__rw_locale::_C_arg2category (cat)),
                     (_RWSTD_ERROR_LOCALE_BAD_CAT,
                      _RWSTD_FUNC ("locale::locale (const locale&, "
                                   "const char*, category)"), cat));

    _C_body = __rw::__rw_locale::_C_get_body (rhs._C_body, 0, locname, cat, 0);

    // verify 22.1.1.2, p10: `locname' must be valid
    _RWSTD_REQUIRES (_C_body,
                     (_RWSTD_ERROR_LOCALE_BAD_NAME,
                      _RWSTD_FUNC ("locale::locale (const locale&, "
                                   "const char*, category)"), locname));
}


// outlined to hide implementation details
locale::
locale (const locale &one, const locale &other, category cat)
    : _C_body (__rw::__rw_locale::_C_get_body (one._C_body, other._C_body,
                                              0, cat, 0))
{
    // verify 22.1.1.2, p14 (missing requirement): category must be valid
    _RWSTD_REQUIRES (_C_body,
                     (_RWSTD_ERROR_LOCALE_BAD_CAT,
                      _RWSTD_FUNC ("locale::locale (const locale&, "
                                   "const locale&, category)"), cat));
}


// outlined to hide implementation details
/* private */ locale::
locale (__rw::__rw_locale &rhs, const facet *pfacet)
    : _C_body (__rw::__rw_locale::_C_get_body (&rhs, 0, 0, none, pfacet))
{
    // empty
}


const locale::facet* locale::
_C_get_facet (const id &fid) const
{
    // initialize id if not yet initialized, and use the value the
    // initialization returns rather than read the member again
    const size_t facet_id = fid._C_init ();

    // properly initialized facet id's are non-zero
    _RWSTD_ASSERT (0 != facet_id);

    _RWSTD_ASSERT (0 != _C_body);

    // find the index at which the facet is stored in one of the facet arrays
    const size_t inx = _C_body->_C_get_facet_inx (facet_id);
    if (size_t (-1) == inx)
        return 0;

    const locale::facet* const pfacet = inx < __rw::__rw_locale::_C_n_std_facets
        ? _C_body->_C_std_facets [inx]
        : _C_body->_C_usr_facets [inx - __rw::__rw_locale::_C_n_std_facets];

    if (pfacet) {
        // facet id must have been initialized to point
        // to the numeric member of the static Facet::id
        _RWSTD_ASSERT (pfacet->_C_pid);

        // make sure the static Facet::id matches its (non-static) id member
        _RWSTD_ASSERT (*pfacet->_C_pid == facet_id);
    }

    return pfacet;
}


}   // namespace std
