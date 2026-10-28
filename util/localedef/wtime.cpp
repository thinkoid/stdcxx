/***************************************************************************
 *
 * wtime.cpp
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

// LC_TIME: the __rw_time_t header, the era records, the offset pairs
// of the alternate digits, then the strings, every wide one before
// every narrow one in the order the header lists them, the eras'
// names and formats and the alternate digits after the formats.

#include "pool.h"
#include "writer.h"

#include <loc/_localedef.h>

#include <climits>   // for CHAR_MAX, CHAR_MIN, INT_MAX, INT_MIN
#include <cstring>   // for memset()


typedef __rw::__rw_time_t::era_t era_record_t;


// The offsets of one alternate digit, narrow then wide, as the
// alt_digits array holds them.
struct alt_digit_off_t {
    _RWSTD_UINT32_T narrow;
    _RWSTD_UINT32_T wide;
};


// One string of the image and the two header words that hold its
// offsets, narrow and wide.  The strings are listed once, in the order
// of the header, and placed in two passes, so the pointers keep the
// order from being spelled twice.
struct slot_t {
    const ucs_string_t *ucs;
    _RWSTD_UINT32_T    *off [2];
};


static void
add_slot (std::vector<slot_t> &slots, const ucs_string_t &ucs,
          _RWSTD_UINT32_T &narrow_off, _RWSTD_UINT32_T &wide_off)
{
    slot_t slot;

    slot.ucs     = &ucs;
    slot.off [0] = &narrow_off;
    slot.off [1] = &wide_off;

    slots.push_back (slot);
}


// The n strings of a list, empty ones for a list the source does not
// set, the old compiler's default for every string of the category.
static std::vector<ucs_string_t>
strings_of (const field_t<std::vector<ucs_string_t> > &list, std::size_t n)
{
    std::vector<ucs_string_t> strings (list.value);

    strings.resize (n);

    return strings;
}


// One bound of an era: the year in full, the month from 0, the day
// from 1, and for -* and +* the infinities _localedef.h names.  The
// model holds glibc's year, less 1900 and with no year 0, so the year
// written is the astronomical one, -542 for an era declared at -543,
// which the runtime compares with 1900 + tm_year.
static void
era_bound (const int date [3], _RWSTD_INT32_T &year, char &month, char &day)
{
    if (era_minus_infinity == date [0]) {
        year  = INT_MIN;
        month = CHAR_MIN;
        day   = CHAR_MIN;
    }
    else if (era_plus_infinity == date [0]) {
        year  = INT_MAX;
        month = CHAR_MAX;
        day   = CHAR_MAX;
    }
    else {
        year  = date [0] + 1900;
        month = char (date [1]);
        day   = char (date [2]);
    }
}


void
write_time (const locale_def_t &def, const cmap_t &cmap,
            const image_names_t &names, std::vector<char> &image)
{
    const time_def_t &time     = def.time;
    const translit_t &translit = def.ctype.translit;

    const std::vector<era_t>      &eras   = time.era;
    const std::vector<ucs_string_t> &digits = time.alt_digits.value;

    __rw::__rw_time_t hdr;
    std::memset (&hdr, 0, sizeof hdr);

    hdr.num_eras       = unsigned (eras.size ());
    hdr.num_alt_digits = unsigned (digits.size ());

    std::vector<era_record_t>    records (eras.size ());
    std::vector<alt_digit_off_t> alt_offs (digits.size ());

    pool_t pool;

    // the records and the offset pairs lead the data, their offsets
    // filled once the strings are placed
    hdr.era_off =
        pool.reserve (sizeof (era_record_t) * records.size ());
    hdr.alt_digits_off =
        pool.reserve (sizeof (alt_digit_off_t) * alt_offs.size ());

    const std::vector<ucs_string_t> abday = strings_of (time.abday, 7);
    const std::vector<ucs_string_t> day   = strings_of (time.day, 7);
    const std::vector<ucs_string_t> abmon = strings_of (time.abmon, 12);
    const std::vector<ucs_string_t> mon   = strings_of (time.mon, 12);
    const std::vector<ucs_string_t> am_pm = strings_of (time.am_pm, 2);

    std::vector<slot_t> slots;

    for (std::size_t i = 0; i != 7; ++i)
        add_slot (slots, abday [i],
                  hdr.abday_off [0][i], hdr.abday_off [1][i]);

    for (std::size_t i = 0; i != 7; ++i)
        add_slot (slots, day [i], hdr.day_off [0][i], hdr.day_off [1][i]);

    for (std::size_t i = 0; i != 12; ++i)
        add_slot (slots, abmon [i],
                  hdr.abmon_off [0][i], hdr.abmon_off [1][i]);

    for (std::size_t i = 0; i != 12; ++i)
        add_slot (slots, mon [i], hdr.mon_off [0][i], hdr.mon_off [1][i]);

    for (std::size_t i = 0; i != 2; ++i)
        add_slot (slots, am_pm [i],
                  hdr.am_pm_off [0][i], hdr.am_pm_off [1][i]);

    add_slot (slots, time.d_t_fmt.value,
              hdr.d_t_fmt_off [0], hdr.d_t_fmt_off [1]);
    add_slot (slots, time.d_fmt.value,
              hdr.d_fmt_off [0], hdr.d_fmt_off [1]);
    add_slot (slots, time.t_fmt.value,
              hdr.t_fmt_off [0], hdr.t_fmt_off [1]);
    add_slot (slots, time.t_fmt_ampm.value,
              hdr.t_fmt_ampm_off [0], hdr.t_fmt_ampm_off [1]);
    add_slot (slots, time.era_d_t_fmt.value,
              hdr.era_d_t_fmt_off [0], hdr.era_d_t_fmt_off [1]);
    add_slot (slots, time.era_d_fmt.value,
              hdr.era_d_fmt_off [0], hdr.era_d_fmt_off [1]);
    add_slot (slots, time.era_t_fmt.value,
              hdr.era_t_fmt_off [0], hdr.era_t_fmt_off [1]);

    for (std::size_t i = 0; i != eras.size (); ++i) {
        add_slot (slots, eras [i].name,
                  records [i].name_off [0], records [i].name_off [1]);
        add_slot (slots, eras [i].format,
                  records [i].fmt_off [0], records [i].fmt_off [1]);
    }

    for (std::size_t i = 0; i != digits.size (); ++i)
        add_slot (slots, digits [i], alt_offs [i].narrow, alt_offs [i].wide);

    // every wide string, then every narrow one, then the names
    for (std::size_t i = 0; i != slots.size (); ++i)
        *slots [i].off [1] = pool.add (*slots [i].ucs);

    for (std::size_t i = 0; i != slots.size (); ++i)
        *slots [i].off [0] =
            pool.add (narrow (*slots [i].ucs, cmap, translit));

    hdr.codeset_off = pool.add (names.codeset);
    hdr.charmap_off = pool.add (names.charmap);

    // the records: the direction as the sign of the offset
    for (std::size_t i = 0; i != eras.size (); ++i) {

        era_record_t &rec = records [i];

        rec.offset = '-' == eras [i].direction
            ? -eras [i].offset : eras [i].offset;

        era_bound (eras [i].start, rec.year [0], rec.month [0], rec.day [0]);
        era_bound (eras [i].end, rec.year [1], rec.month [1], rec.day [1]);

        pool.put (hdr.era_off + unsigned (i * sizeof rec), &rec, sizeof rec);
    }

    for (std::size_t i = 0; i != digits.size (); ++i)
        pool.put (hdr.alt_digits_off + unsigned (i * sizeof alt_offs [i]),
                  &alt_offs [i], sizeof alt_offs [i]);

    image.clear ();

    append (image, &hdr, sizeof hdr);
    pool.append_to (image);
}
