/***************************************************************************
 *
 * locale_body.h - definition of the __rw_locale class
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

#ifndef _RWSTD_LOC_RW_LOCALE_H_INCLUDED
#define _RWSTD_LOC_RW_LOCALE_H_INCLUDED


#include <loc/_facet.h>
#include <loc/_locale.h>
#include <string.h>

#include "podarray.h"

#include <rw/_defs.h>


namespace std { 

struct _RWSTD_CLASS_EXPORT locale;

}   // namespace std


namespace __rw { 


// the factories of the standard facets (src/use_facet.h,
// src/ctype.cpp), for the table the body constructor builds its
// facets with (src/locale_body.cpp)
__rw_facet* __rw_ct_collate (_RWSTD_SIZE_T, const char*);
__rw_facet* __rw_ct_ctype (_RWSTD_SIZE_T, const char*);
__rw_facet* __rw_ct_codecvt (_RWSTD_SIZE_T, const char*);
__rw_facet* __rw_ct_numpunct (_RWSTD_SIZE_T, const char*);
__rw_facet* __rw_ct_num_get (_RWSTD_SIZE_T, const char*);
__rw_facet* __rw_ct_num_put (_RWSTD_SIZE_T, const char*);
__rw_facet* __rw_ct_moneypunct (_RWSTD_SIZE_T, const char*);
__rw_facet* __rw_ct_moneypunct_intl (_RWSTD_SIZE_T, const char*);
__rw_facet* __rw_ct_money_get (_RWSTD_SIZE_T, const char*);
__rw_facet* __rw_ct_money_put (_RWSTD_SIZE_T, const char*);
__rw_facet* __rw_ct_time_get (_RWSTD_SIZE_T, const char*);
__rw_facet* __rw_ct_time_put (_RWSTD_SIZE_T, const char*);
__rw_facet* __rw_ct_messages (_RWSTD_SIZE_T, const char*);
__rw_facet* __rw_ct_wcollate (_RWSTD_SIZE_T, const char*);
__rw_facet* __rw_ct_wcodecvt (_RWSTD_SIZE_T, const char*);
__rw_facet* __rw_ct_wctype (_RWSTD_SIZE_T, const char*);
__rw_facet* __rw_ct_wmoneypunct (_RWSTD_SIZE_T, const char*);
__rw_facet* __rw_ct_wmoneypunct_intl (_RWSTD_SIZE_T, const char*);
__rw_facet* __rw_ct_wmoney_get (_RWSTD_SIZE_T, const char*);
__rw_facet* __rw_ct_wmoney_put (_RWSTD_SIZE_T, const char*);
__rw_facet* __rw_ct_wnumpunct (_RWSTD_SIZE_T, const char*);
__rw_facet* __rw_ct_wnum_get (_RWSTD_SIZE_T, const char*);
__rw_facet* __rw_ct_wnum_put (_RWSTD_SIZE_T, const char*);
__rw_facet* __rw_ct_wtime_get (_RWSTD_SIZE_T, const char*);
__rw_facet* __rw_ct_wtime_put (_RWSTD_SIZE_T, const char*);
__rw_facet* __rw_ct_wmessages (_RWSTD_SIZE_T, const char*);


class _RWSTD_EXPORT __rw_locale
{
public:
    // total number of standard facets (char and wchar specializations)
    // ordinary (unnamed) and byname facets share the same array slot
    enum { _C_n_std_facets = __rw_facet::_C_last_type / 2 };
    enum { _C_namebuf_size = 40 };

private:
    // standard facets are always present, user defined facets (i.e.,
    // non-standard specializations of standard facets, or user-defined
    // facets with own id's) are dynamically allocated only if installed

    // each standard facet is stored at an index, inx, computed from its
    // numeric id, as follows: inx = (Facet::id._C_id - 1) / 2
    // each slot holds either an ordinary (unnamed) or a byname standard
    // facet (if the latter exists); the constructors fill every slot,
    // and a slot does not change while the body lives, so lookup reads
    // it with no lock
    __rw_facet     *_C_std_facets [_C_n_std_facets];
    __rw_facet    **_C_usr_facets;     // array of pointers to user facets
    _RWSTD_SIZE_T   _C_n_usr_facets;   // number of user-defined facets

    // name buffer sufficiently large to accommodate 6 basic 5-character
    // POSIX locale names such as "en_US" (one for each LC_XXX category)
    char           _C_namebuf [_C_namebuf_size];
    const char    *_C_name;        // name of locale, 0 when unnamed
    _RWSTD_SIZE_T  _C_ref;         // reference count (>= 0)

    friend struct std::locale;

public:


    // create a locale body from the name of a locale
    __rw_locale (const char*);

    // create a locale body from another locale combined with a facet
    __rw_locale (const __rw_locale &__rhs, const __rw_facet *__facet) {
        _C_construct (__rhs, __facet);
    }

    // create a locale body from two locales taking the categories
    // in `cat' from `l2'
    __rw_locale (const __rw_locale &__l1, const __rw_locale &__l2, int __cat) {
        _C_construct (__l1, __l2, __cat);
    }

    ~__rw_locale () _THROWS (());

    // returns the classic body with a reference; built once, the body
    // holds one reference of its own forever, so its count never drops
    // to zero and it is never destroyed
    static __rw_locale* _C_classic ();

    static _RWSTD_SIZE_T _C_add_ref (__rw_facet &__facet) {
        return _RWSTD_ATOMIC_PREINCREMENT (__facet._C_ref, false);
    }

    static _RWSTD_SIZE_T _C_remove_ref (__rw_facet &__facet) {
        return _RWSTD_ATOMIC_PREDECREMENT (__facet._C_ref, false);
    }

    // converts a LC_XXX constant to a locale::category value
    static int _C_LC2category (int) _THROWS (());

    static bool _C_check_category (int) _THROWS (());

    // converts the category argument of a locale constructor to
    // a locale::category value: an LC_XXX constant maps to its
    // category, and 0, the intersection of disjoint categories,
    // selects nothing (22.1.1.1.1, p1), even where LC_CTYPE is 0
    static int _C_arg2category (int __cat) _THROWS (()) {
        return 0 == __cat ? int (__rw_cat_none) : _C_LC2category (__cat);
    }

    // returns the type of the standard facet object or `unknown'
    // if the facet is of a user-defined type
    static __rw_facet::_C_facet_type
    _C_get_facet_type (const __rw_facet&) _THROWS (());

private:

    // the global locale, for the default locale ctor and
    // locale::global(): the first returns its body with a reference;
    // the second makes `body' the global with a reference and returns
    // the previous body with the reference the global held
    static __rw_locale* _C_get_global ();
    static __rw_locale* _C_set_global (__rw_locale*);

    // implements locale ctor: locale::locale (const char*); returns,
    // with a reference, the classic body for a name of the C locale
    // and a new body otherwise, or 0 for an invalid name
    static __rw_locale* _C_get_named_body (const char*);

    // implements all but the default locale ctor; the function
    // may construct a new object or return one of its first two
    // arguments or the classic body
    static __rw_locale*
    _C_get_body (__rw_locale*, __rw_locale*,
                 const char*, int, const __rw_facet*);

    // obtains from its factory, with no reference, the standard facet
    // for a slot of the body of a locale name; called from the ctor
    // from a name
    static __rw_facet* _C_get_slot_facet (const char*, _RWSTD_SIZE_T);

    // called from ctors (only) to fully construct a combined locale
    void _C_construct (const __rw_locale&, const __rw_facet*);
    void _C_construct (const __rw_locale&, const __rw_locale&, int);

    // implements locale template ctor: locale::locale (const locale&, Facet*)
    __rw_locale* _C_combine (const __rw_facet*) const;

    // implements locale ctor: locale (const locale&, const locale&, category)
    __rw_locale* _C_combine (const __rw_locale&, int);

    // returns the index into one of the facet arrays
    // corresponding to the given facet id:
    //   -1       if the facet isn't installed
    //   value <  _C_n_std_facets for a standard facet
    //   value >= _C_n_std_facets otherwise (i.e., for user defined facets)
    _RWSTD_SIZE_T _C_get_facet_inx (_RWSTD_SIZE_T) const _THROWS (());
};


/* static */ inline bool
__rw_locale::
_C_check_category (int cat) _THROWS (())
{
    // `cat' is assumed to be a C++ locale category (i.e., not an LC_XXX)
    _RWSTD_ASSERT (cat == _C_LC2category (cat));

    return __rw_cat_none == cat || !(cat & ~__rw_cat_all);
}


/* static */ inline __rw_facet::_C_facet_type
__rw_locale::
_C_get_facet_type (const __rw_facet &__facet) _THROWS (())
{
    _RWSTD_ASSERT (0 != __facet._C_pid);
    _RWSTD_ASSERT (0 != *__facet._C_pid);

    // compute the numeric value of the facet's type
    const _RWSTD_SIZE_T __numtype =
        *__facet._C_pid * 2 - 1 + (0 != __facet._C_name);

    // return `unknown' for user-defined facets
    return __numtype > __rw_facet::_C_last_type ?
        __rw_facet::_C_unknown : __rw_facet::_C_facet_type (__numtype);
}


// a one-character string to separate LC_XXX categories in locale names
#ifndef _RWSTD_CAT_SEP
#  define _RWSTD_CAT_SEP ""
#endif


// maps LC_XXX category values to their names
extern const struct __rw_cats_t
{
    int         cat;          // LC_XXX category
    const char *name;         // LC_XXX name
} __rw_cats [6];

static const _RWSTD_SIZE_T __rw_n_cats = 6;


// computes LC_XXX category from a numeric facet id, returns the
// LC_XXX category for standard facets, LC_ALL for all others
inline int __rw_get_cat (int id) _THROWS (())
{
#if _RWSTD_LC_MIN >= 0 && _RWSTD_LC_MAX < _RWSTD_UCHAR_MAX
    typedef unsigned char LC_type;
#else
    typedef int LC_type;
#endif   // _RWSTD_LC_MIN >= 0 && _RWSTD_LC_MAX < _RWSTD_UCHAR_MAX

    const LC_type lc_cats[] = {
        _RWSTD_LC_COLLATE,
        // _C_codecvt, _C_ctype (all derivatives and specializations)
        _RWSTD_LC_CTYPE, _RWSTD_LC_CTYPE,
        // _C_moneypunct, _C_moneypunct_intl, _C_money_get, _C_money_put
        _RWSTD_LC_MONETARY, _RWSTD_LC_MONETARY,
        _RWSTD_LC_MONETARY, _RWSTD_LC_MONETARY,
        // _C_numpunct, _C_num_get, _C_num_put
        _RWSTD_LC_NUMERIC, _RWSTD_LC_NUMERIC, _RWSTD_LC_NUMERIC,
        // _C_time_get, _C_time_put
        _RWSTD_LC_TIME, _RWSTD_LC_TIME,
#ifdef _RWSTD_LC_MESSAGES
        // _C_messages
        _RWSTD_LC_MESSAGES
#else
        _RWSTD_LC_MAX + 1
#endif
    };

    return id > 0 && id <= __rw_locale::_C_n_std_facets ?
          lc_cats [((id - 1) % (sizeof lc_cats / sizeof *lc_cats))]
        : _RWSTD_LC_ALL;
}


#ifndef _RWSTD_NO_CONDENSED_NAME

static const char __rw_C_locale_name[] = "C";

#else   // if defined (_RWSTD_NO_CONDENSED_NAME)

static const char __rw_C_locale_name[] = {
    'C', *_RWSTD_CAT_SEP, 'C', *_RWSTD_CAT_SEP, 'C', *_RWSTD_CAT_SEP,
    'C', *_RWSTD_CAT_SEP, 'C', *_RWSTD_CAT_SEP, 'C', '\0'
};

#endif   // _RWSTD_NO_CONDENSED_NAME


// returns true iff `name' is a name of the classic C locale
static inline bool __rw_is_C (const char *name) _THROWS (())
{
    _RWSTD_ASSERT (0 != name);

    return    'C' == name [0] && '\0' == name [1]

#ifdef _RWSTD_NO_CONDENSED_NAME

           || !strcmp (__rw_C_locale_name, name)

#endif   // _RWSTD_NO_CONDENSED_NAME

        ;
}


}   // namespace __rw


#endif   // _RWSTD_LOC_RW_LOCALE_H_INCLUDED
