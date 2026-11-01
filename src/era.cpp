/***************************************************************************
 *
 * era.cpp - the era records of LC_TIME, for time_put and time_get
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
 **************************************************************************/

#define _RWSTD_LIB_SRC
#include <rw/_defs.h>

#include <string.h>   // for strcmp()
#include <time.h>     // for struct tm

#include <loc/_facet.h>
#include <loc/_localedef.h>

#include "era.h"


namespace __rw {


const void*
__rw_get_timepunct (const __rw_facet*, int, size_t);



// compares the date given by `year', `mon' and `mday' with the
// bound `k' of the era `pera' (0 for its start date, 1 for its
// end date), the year first, then the month, then the day, and
// returns a negative, zero, or positive value as the date comes
// before, falls on, or comes after the bound; the infinities
// of a bound at either end of time decide at the year
static int
__rw_era_date_cmp (int year, int mon, int mday,
                   const __rw_time_t::era_t *pera, int k)
{
    if (year != pera->year [k])
        return year < pera->year [k] ? -1 : 1;

    if (mon != pera->month [k])
        return mon < pera->month [k] ? -1 : 1;

    return mday - pera->day [k];
}


// returns a pointer to the era_t element describing the era
// corresponding to the date pointed to by `tmb', or 0 if no
// such era exists
const __rw_time_t::era_t*
__rw_get_era (const __rw_time_t *ptime, const tm *tmb)
{
    _RWSTD_ASSERT (0 != tmb);

    const int year = 1900 + tmb->tm_year;

    // get the number of eras in this locale
    const size_t neras = size_t (ptime->era_count ());

    for (size_t i = 0; i != neras; ++i) {
        const __rw_time_t::era_t* const pera = ptime->era (i);

        _RWSTD_ASSERT (0 != pera);

        // the start date is the one nearest the era's first year
        // and may be the later of the two (an era that counts into
        // the past ends at the beginning of time): the date belongs
        // to the era when it lies between the bounds in either order,
        // each bound compared as a whole date so that a day in the
        // first or last month of an era is tested against the bound
        // of that month alone
        const int cs =
            __rw_era_date_cmp (year, tmb->tm_mon, tmb->tm_mday, pera, 0);
        const int ce =
            __rw_era_date_cmp (year, tmb->tm_mon, tmb->tm_mday, pera, 1);

        if ((cs <= 0 && 0 <= ce) || (ce <= 0 && 0 <= cs))
            return pera;
    }

    return 0;
}


// returns the number of the year of the date pointed to by `tmb'
// within the era `pera' that contains it
int
__rw_get_era_year (const __rw_time_t::era_t *pera, const tm *tmb)
{
    // the offset numbers the year at the start date and its sign
    // says whether the numbers grow toward the end date ('+') or
    // shrink toward it ('-'); the end date may be earlier in time
    // than the start date, and then the count against the calendar
    // runs the other way
    const bool backward =
        0 < __rw_era_date_cmp (pera->year [0], pera->month [0],
                               pera->day [0], pera, 1);
    const bool shrinks = pera->offset < 0;

    const int offset = shrinks ? -pera->offset : pera->offset;
    const int delta  = 1900 + tmb->tm_year - pera->year [0];

    return backward == shrinks ? offset + delta : offset - delta;
}


// returns the year, astronomical, that the number `n' stands for in
// the era `pera': the inverse of __rw_get_era_year
static int
__rw_get_era_year_of (const __rw_time_t::era_t *pera, int n)
{
    const bool backward =
        0 < __rw_era_date_cmp (pera->year [0], pera->month [0],
                               pera->day [0], pera, 1);
    const bool shrinks = pera->offset < 0;

    const int offset = shrinks ? -pera->offset : pera->offset;

    return backward == shrinks ?
        pera->year [0] + (n - offset) : pera->year [0] - (n - offset);
}


// the number of the year at the start date of the era record `pera'
static int
__rw_era_offset (const __rw_time_t::era_t *pera)
{
    return pera->offset < 0 ? -pera->offset : pera->offset;
}


// an era for the reader: every record with one name, in the order of
// the locale; ja_JP's Reiwa is its first-year record of 2019 and its
// record from 2020; the records of one name number their years
// consistently, one era and one count (both of Reiwa's make 2019 its
// year 1), so any of them serves for the arithmetic, and the first
// one does
struct __rw_era_span
{
    const __rw_time_t::era_t *first;   // the first record of the name
    const __rw_time_t::era_t *base;    // the record of the smallest offset
    const __rw_time_t::era_t *lo;      // the record of the earliest bound
    const __rw_time_t::era_t *hi;      // the record of the latest bound
    int                       klo;     // the earliest bound of `lo'
    int                       khi;     // the latest bound of `hi'
};


// fills `span' with the era of the record `inx': the records with its
// name, the earliest and the latest of their bounds, and the record
// whose start date is in the first year of the era, the one with the
// smallest offset (2019 for Reiwa); bounds at either end of time stay
// infinite
static void
__rw_get_era_span (const __rw_time_t *ptime, size_t inx,
                   __rw_era_span *span)
{
    const char* const name =
        static_cast< const char* >(ptime->era_name (inx, false));

    const size_t neras = size_t (ptime->era_count ());

    span->first = 0;

    for (size_t i = 0; i != neras; ++i) {

        if (strcmp (name, static_cast< const char* >(
                        ptime->era_name (i, false))))
            continue;

        const __rw_time_t::era_t* const pera = ptime->era (i);

        _RWSTD_ASSERT (0 != pera);

        if (0 == span->first) {
            span->first = span->base = span->lo = span->hi = pera;
            span->klo   = span->khi  = 0;
        }

        if (__rw_era_offset (pera) < __rw_era_offset (span->base))
            span->base = pera;

        for (int k = 0; k != 2; ++k) {
            const int cl = __rw_era_date_cmp (pera->year [k], pera->month [k],
                                              pera->day [k], span->lo,
                                              span->klo);
            const int ch = __rw_era_date_cmp (pera->year [k], pera->month [k],
                                              pera->day [k], span->hi,
                                              span->khi);
            if (cl < 0) {
                span->lo  = pera;
                span->klo = k;
            }

            if (0 < ch) {
                span->hi  = pera;
                span->khi = k;
            }
        }
    }

    _RWSTD_ASSERT (0 != span->first);
}


// the reader's membership, a sibling of the writer's: a year strictly
// between the first and the last year of the era lies in the era
// whatever the month and the day of the tm hold, and only a date in
// the first or the last year is compared as a whole, against the
// bound of the record that holds it, with a day below 1 taken for
// the first of the month; a zeroed tm, January 0, would otherwise
// come before an era that begins on January 1
static bool
__rw_era_holds (const __rw_era_span *span, const tm *tmb)
{
    const int year = 1900 + tmb->tm_year;
    const int mday = tmb->tm_mday < 1 ? 1 : tmb->tm_mday;

    const int first = span->lo->year [span->klo];
    const int last  = span->hi->year [span->khi];

    if (year < first || last < year)
        return false;

    if (year == first) {
        if (__rw_era_date_cmp (year, tmb->tm_mon, mday,
                               span->lo, span->klo) < 0)
            return false;
    }

    if (year == last) {
        if (0 < __rw_era_date_cmp (year, tmb->tm_mon, mday,
                                   span->hi, span->khi))
            return false;
    }

    return true;
}


// the reader's lookup, a sibling of the writer's __rw_get_era: fills
// `span' with the era the date in `tmb' lies in and returns true, or
// returns false when it lies in none; the eras are tried in the order
// of their first records, forward and backward alike
static bool
__rw_get_era_of_year (const __rw_time_t *ptime, const tm *tmb,
                      __rw_era_span *span)
{
    _RWSTD_ASSERT (0 != tmb);

    const size_t neras = size_t (ptime->era_count ());

    for (size_t i = 0; i != neras; ++i) {
        __rw_get_era_span (ptime, i, span);

        // each era once, at its first record
        if (span->first == ptime->era (i) && __rw_era_holds (span, tmb))
            return true;
    }

    return false;
}


// %EC: moves the year in `tmb' into the era of the record `inx'; a
// year in the era stays, any other keeps its number within its own
// era where the era named holds that number, and is otherwise the
// era's first year, so that a %Ey after it finds the era by the year
_RWSTD_EXPORT void
__rw_set_era (const __rw_facet *pfacet, size_t inx, tm *tmb)
{
    _RWSTD_ASSERT (0 != tmb);

    const __rw_time_t* const ptime =
        static_cast< const __rw_time_t* >(__rw_get_timepunct (pfacet, 0, 0));

    _RWSTD_ASSERT (inx < size_t (ptime->era_count ()));

    __rw_era_span named;
    __rw_get_era_span (ptime, inx, &named);

    __rw_era_span span;
    const bool    found = __rw_get_era_of_year (ptime, tmb, &span);

    if (found && span.first == named.first)
        return;

    // the number of the year within its era, or the number of the
    // era's first year when the year lies in no era
    const int n = found ?
        __rw_get_era_year (span.first, tmb) : __rw_era_offset (named.base);

    tm tmp = *tmb;

    tmp.tm_year = __rw_get_era_year_of (named.first, n) - 1900;

    if (__rw_era_holds (&named, &tmp)) {
        tmb->tm_year = tmp.tm_year;
        return;
    }

    // the era's first year, unless the month and the day in the tm
    // fall before the era in that year, as a zeroed tm's January 0
    // falls before Heisei, which begins on January 8 of 1989: then the
    // year next to it inside the era, so that the lookup of a %Ey
    // after it does not find the era before
    const int first = named.base->year [0];

    tmp.tm_year = first - 1900;

    if (!__rw_era_holds (&named, &tmp)) {
        tmp.tm_year += first == named.lo->year [named.klo] ? 1 : -1;

        if (!__rw_era_holds (&named, &tmp))
            tmp.tm_year = first - 1900;
    }

    tmb->tm_year = tmp.tm_year;
}


// %Ey: sets the year in `tmb' to the year `n' of the era of the record
// `inx', or of the era the date in `tmb' lies in when `inx' is SIZE_MAX,
// or to the start year of the record `inx' when `n' is INT_MIN; returns
// false, the tm unchanged, when there is no such era or the year lies
// outside it
_RWSTD_EXPORT bool
__rw_set_era_year (const __rw_facet *pfacet, size_t inx, int n, tm *tmb)
{
    _RWSTD_ASSERT (0 != tmb);
    _RWSTD_ASSERT (_RWSTD_INT_MIN != n || _RWSTD_SIZE_MAX != inx);

    const __rw_time_t* const ptime =
        static_cast< const __rw_time_t* >(__rw_get_timepunct (pfacet, 0, 0));

    __rw_era_span span;

    if (_RWSTD_SIZE_MAX != inx)
        __rw_get_era_span (ptime, inx, &span);
    else if (!__rw_get_era_of_year (ptime, tmb, &span))
        return false;

    const int year = _RWSTD_INT_MIN == n ?
        ptime->era (inx)->year [0] : __rw_get_era_year_of (span.first, n);

    // the era's span bounds the directive, as 1..31 bound %d: all its
    // records together, so that Reiwa 1 is 2019 by its first-year
    // record; an era open at either end has no bound there
    if (year < span.lo->year [span.klo] || span.hi->year [span.khi] < year)
        return false;

    tmb->tm_year = year - 1900;

    return true;
}


}   // namespace __rw
