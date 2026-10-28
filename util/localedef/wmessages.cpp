/***************************************************************************
 *
 * wmessages.cpp
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

// LC_MESSAGES: the two expressions, wide then narrow, after the
// __rw_messages_t header.

#include "pool.h"
#include "writer.h"

#include <loc/_localedef.h>

#include <cstring>   // for memset()


void
write_messages (const locale_def_t &def, const cmap_t &cmap,
                const image_names_t &names, std::vector<char> &image)
{
    const messages_def_t &msg      = def.messages;
    const translit_t     &translit = def.ctype.translit;

    __rw::__rw_messages_t hdr;
    std::memset (&hdr, 0, sizeof hdr);

    pool_t pool;

    hdr.yesexpr_off [1] = pool.add (msg.yesexpr.value);
    hdr.noexpr_off [1]  = pool.add (msg.noexpr.value);
    hdr.yesexpr_off [0] =
        pool.add (narrow (msg.yesexpr.value, cmap, translit));
    hdr.noexpr_off [0]  =
        pool.add (narrow (msg.noexpr.value, cmap, translit));
    hdr.codeset_off     = pool.add (names.codeset);
    hdr.charmap_off     = pool.add (names.charmap);

    image.clear ();

    append (image, &hdr, sizeof hdr);
    pool.append_to (image);
}
