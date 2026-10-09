/************************************************************************
 *
 * 22.locale.codecvt.unshift.mt.cpp
 *
 * test exercising the thread safety of codecvt shift-state handling
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
 **************************************************************************/

#include "codecvt_mt.h"

template <typename T>
struct test_is_char
{
    enum { value = 0 };
};

template <>
struct test_is_char<char>
{
    enum { value = 1 };
};

/**************************************************************************/

template <class internT>
void test_codecvt_unshift (const std::locale& loc)
{
    typedef char externT;
    typedef std::mbstate_t stateT;
    typedef std::codecvt<internT, externT, stateT> code_cvt_type;

    const code_cvt_type& cvt =
        std::use_facet<code_cvt_type>(loc);

    externT out_buffer [16];
    out_buffer [0] = externT ();

    const int out_len = RW_COUNT_OF (out_buffer);

    externT* to       = out_buffer;
    externT* to_limit = out_buffer + out_len;
    externT* to_next  = 0;

    std::mbstate_t state = std::mbstate_t ();

    const std::codecvt_base::result result =
        cvt.unshift (state, to, to_limit, to_next);

    // 22.2.1.5.2 p5
    RW_ASSERT (to == to_next);

    // 22.2.1.5.2 p6 required only for codecvt<char, char, mbstate_t>
    RW_ASSERT (  !test_is_char<internT>::value
               || result == std::codecvt_base::noconv);
}

/**************************************************************************/

static void
exercise_codecvt (const std::locale& loc, std::size_t,
                  std::size_t, bool narrow, bool wide)
{
    if (narrow) {
        test_codecvt_unshift<char>(loc);
    }

    if (wide) {
        test_codecvt_unshift<wchar_t>(loc);
    }
}

/**************************************************************************/

int main (int argc, char *argv[])
{
    return codecvt_test (argc, argv, __FILE__,
                         "thread safety of shift-state handling",
                         0, exercise_codecvt);
}
