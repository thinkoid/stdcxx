/************************************************************************
 *
 * 22.locale.codecvt.length.mt.cpp
 *
 * test exercising the thread safety of codecvt conversion length
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

template <class internT>
struct MyCodecvtData_T
{
    std::size_t length_;
};

struct MyCodecvtData
{
    MyCodecvtData_T<char> char_data_;
    MyCodecvtData_T<wchar_t> wchar_data_;

} my_codecvt_data [MAX_THREADS];

template <class charT>
struct MyBuffer {
    const charT* const str;
    const int str_len;
};

const MyBuffer<char> nsrc [] = {
    { "a\x80",       2 },
    { "b",           1 },
    { "c\0c",        3 },
    { "ddd",         3 },
    { "e\fce\0",     4 },
    { "ff\0ffff",    5 },
    { "gggg\0g",     6 },
    { "hh\0hhhh",    7 },
    { "i\ni\tiiii",  8 },
    { "jjjjjjjjj",   9 },
    { "kkkkkkkkkk", 10 }
};

/**************************************************************************/

template <class internT>
void test_codecvt_length (const std::locale& loc,
                          const MyBuffer<char>& out,
                          const MyCodecvtData_T<internT>& data)
{
    typedef char externT;
    typedef std::mbstate_t stateT;
    typedef std::size_t sizeT;

    typedef std::codecvt<internT, externT, stateT> code_cvt_type;

    const code_cvt_type& cvt =
        std::use_facet<code_cvt_type>(loc);

    const externT* from = out.str;
    const externT* from_end  = out.str + out.str_len;
    const size_t   max  = 32;

    std::mbstate_t state = std::mbstate_t ();

    const sizeT len = cvt.length (state, from, from_end, max);

    RW_ASSERT (data.length_ == len);
}

/**************************************************************************/

static void
exercise_codecvt (const std::locale& loc, std::size_t inx,
                  std::size_t, bool narrow, bool wide)
{
    const int ni = RW_COUNT_OF (nsrc);

    const MyCodecvtData& data = my_codecvt_data [inx];

    if (narrow) {
        test_codecvt_length<char>(loc, nsrc [inx % ni], data.char_data_);
    }

    if (wide) {
        test_codecvt_length<wchar_t>(loc, nsrc [inx % ni], data.wchar_data_);
    }
}

/**************************************************************************/

template <class internT>
void fill_codecvt_length (const std::locale& loc,
                          const MyBuffer<char>& out,
                          MyCodecvtData_T<internT>& data)
{
    typedef char externT;
    typedef std::mbstate_t stateT;

    typedef std::codecvt<internT, externT, stateT> code_cvt_type;

    const code_cvt_type& cvt =
        std::use_facet<code_cvt_type>(loc);

    const externT* from = out.str;
    const externT* from_end = out.str + out.str_len;
    const std::size_t max = 32;

    std::mbstate_t state = std::mbstate_t ();
    data.length_ = cvt.length (state, from, from_end, max);
}

static void
prepare_codecvt (const std::locale& loc, std::size_t inx)
{
    const int ni = RW_COUNT_OF (nsrc);

    MyCodecvtData& data = my_codecvt_data [inx];

    fill_codecvt_length<char>
        (loc, nsrc [inx % ni], data.char_data_);

    fill_codecvt_length<wchar_t>
        (loc, nsrc [inx % ni], data.wchar_data_);
}

int main (int argc, char *argv[])
{
    return codecvt_test (argc, argv, __FILE__,
                         "thread safety of conversion length",
                         prepare_codecvt, exercise_codecvt);
}
