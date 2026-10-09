/************************************************************************
 *
 * 22.locale.codecvt.in.mt.cpp
 *
 * test exercising the thread safety of codecvt input conversion
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

#include <rw_valcmp.h>    // for rw_strncmp ()

template <class internT>
struct MyCodecvtData_T
{
    enum { BufferSize = 16 };

    typedef char externT;
    typedef std::mbstate_t stateT;
    typedef std::codecvt_base::result resultT;
    typedef std::size_t sizeT;

    externT out_buffer_ [BufferSize];
    stateT out_state_;
    internT in_buffer_ [BufferSize];
    resultT in_result_;
    sizeT in_length_;
    stateT in_state_;
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

const MyBuffer<wchar_t> wsrc [] = {
    { L"\x0905\x0916", 2 },
    { L"bb",           2 },
    { L"\x106c",       2 },
    { L"dddd",         4 },
    { L"\xd800\xd801", 2 },
    { L"ffffff",       6 },
    { L"\xdfff\xffff", 2 },
    { L"hhhhhhhh",     8 },
    { L"i\0i\1i",      4 },
    { L"jjjjjjjjjj",  10 },
    { L"kkkkkkkkkkk", 11 }
};

/**************************************************************************/

template <class internT>
void test_codecvt_in (const std::locale& loc,
                      const MyCodecvtData_T<internT>& data)
{
    typedef char externT;
    typedef std::mbstate_t stateT;
    typedef std::size_t sizeT;

    typedef std::codecvt<internT, externT, stateT> code_cvt_type;

    const code_cvt_type& cvt =
        std::use_facet<code_cvt_type>(loc);

    internT in_buffer [MyCodecvtData_T<internT>::BufferSize];
    in_buffer [0] = internT ();

    const int in_len  = RW_COUNT_OF (in_buffer);
    const int out_len = RW_COUNT_OF (data.out_buffer_);

    const externT* from      = data.out_buffer_;
    const externT* from_end  = data.out_buffer_ + out_len;
    const externT* from_next = 0;

    internT* to       = in_buffer;
    internT* to_limit = in_buffer + in_len;
    internT* to_next  = 0;

    std::mbstate_t state = std::mbstate_t ();

    const typename MyCodecvtData_T<internT>::resultT result =
        cvt.in (state, from, from_end, from_next,
                to, to_limit, to_next);

    const sizeT len = to_next - to;

    RW_ASSERT (data.in_result_ == result);
    RW_ASSERT (len == data.in_length_);
    RW_ASSERT (!rw_strncmp (in_buffer, data.in_buffer_, len));
}

/**************************************************************************/

static void
exercise_codecvt (const std::locale& loc, std::size_t inx,
                  std::size_t, bool narrow, bool wide)
{
    const MyCodecvtData& data = my_codecvt_data [inx];

    if (narrow) {
        test_codecvt_in<char>(loc, data.char_data_);
    }

    if (wide) {
        test_codecvt_in<wchar_t>(loc, data.wchar_data_);
    }
}

/**************************************************************************/

template <class internT>
void fill_codecvt_in (const std::locale& loc,
                      const MyBuffer<internT>& in,
                      MyCodecvtData_T<internT>& data)
{
    typedef char externT;
    typedef std::mbstate_t stateT;

    typedef std::codecvt<internT, externT, stateT> code_cvt_type;

    const code_cvt_type& cvt =
        std::use_facet<code_cvt_type>(loc);

    // prepare the external input for the in () checks
    {
        const int out_len = RW_COUNT_OF (data.out_buffer_);

        const internT* from      = in.str;
        const internT* from_end  = in.str + in.str_len;
        const internT* from_next = 0;

        externT* to       = data.out_buffer_;
        externT* to_limit = data.out_buffer_ + out_len;
        externT* to_next  = 0;

        cvt.out (data.out_state_, from, from_end, from_next,
                 to, to_limit, to_next);
    }

    // in
    {
        const int out_len = RW_COUNT_OF (data.out_buffer_);
        const int in_len  = RW_COUNT_OF (data.in_buffer_);

        const externT* from      = data.out_buffer_;
        const externT* from_end  = data.out_buffer_ + out_len;
        const externT* from_next = 0;

        internT* to       = data.in_buffer_;
        internT* to_limit = data.in_buffer_ + in_len;
        internT* to_next  = 0;

        data.in_result_ = cvt.in (data.in_state_,
                                  from, from_end, from_next,
                                  to, to_limit, to_next);

        data.in_length_ = to_next - to;
    }
}

static void
prepare_codecvt (const std::locale& loc, std::size_t inx)
{
    const int ni = RW_COUNT_OF (nsrc);
    const int wi = RW_COUNT_OF (wsrc);

    MyCodecvtData& data = my_codecvt_data [inx];

    fill_codecvt_in<char>
        (loc, nsrc [inx % ni], data.char_data_);

    fill_codecvt_in<wchar_t>
        (loc, wsrc [inx % wi], data.wchar_data_);
}

int main (int argc, char *argv[])
{
    return codecvt_test (argc, argv, __FILE__,
                         "thread safety of input conversion",
                         prepare_codecvt, exercise_codecvt);
}
