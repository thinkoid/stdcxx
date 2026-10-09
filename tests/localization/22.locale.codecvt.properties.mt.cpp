/************************************************************************
 *
 * 22.locale.codecvt.properties.mt.cpp
 *
 * test exercising the thread safety of codecvt encoding properties
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
    int encoding_;
    int max_length_;
    bool always_noconv_;
};

struct MyCodecvtData
{
    MyCodecvtData_T<char> char_data_;
    MyCodecvtData_T<wchar_t> wchar_data_;

} my_codecvt_data [MAX_THREADS];

template <class internT>
void test_codecvt_properties (const std::locale& loc,
                              const MyCodecvtData_T<internT>& data)
{
    typedef char externT;
    typedef std::mbstate_t stateT;
    typedef std::codecvt<internT, externT, stateT> code_cvt_type;

    const code_cvt_type& cvt =
        std::use_facet<code_cvt_type>(loc);

    RW_ASSERT (data.encoding_ == cvt.encoding ());
    RW_ASSERT (data.max_length_ == cvt.max_length ());
    RW_ASSERT (data.always_noconv_ == cvt.always_noconv ());
}

/**************************************************************************/

static void
exercise_codecvt (const std::locale& loc, std::size_t inx,
                  std::size_t, bool narrow, bool wide)
{
    const MyCodecvtData& data = my_codecvt_data [inx];

    if (narrow) {
        test_codecvt_properties<char>(loc, data.char_data_);
    }

    if (wide) {
        test_codecvt_properties<wchar_t>(loc, data.wchar_data_);
    }
}

/**************************************************************************/

template <class internT>
void fill_codecvt_properties (const std::locale& loc,
                              MyCodecvtData_T<internT>& data)
{
    typedef char externT;
    typedef std::mbstate_t stateT;

    typedef std::codecvt<internT, externT, stateT> code_cvt_type;

    const code_cvt_type& cvt =
        std::use_facet<code_cvt_type>(loc);

    data.encoding_ = cvt.encoding ();
    data.max_length_ = cvt.max_length ();
    data.always_noconv_ = cvt.always_noconv ();
}

static void
prepare_codecvt (const std::locale& loc, std::size_t inx)
{
    MyCodecvtData& data = my_codecvt_data [inx];

    fill_codecvt_properties<char>
        (loc, data.char_data_);

    fill_codecvt_properties<wchar_t>
        (loc, data.wchar_data_);
}

int main (int argc, char *argv[])
{
    return codecvt_test (argc, argv, __FILE__,
                         "thread safety of encoding properties",
                         prepare_codecvt, exercise_codecvt);
}
