/***************************************************************************
 *
 * 21.char.traits.compare.cpp - wide-character comparison boundary cases
 *
 ***************************************************************************
 *
 * Licensed to the Apache Software Foundation (ASF) under one or more
 * contributor license agreements. See the NOTICE file distributed
 * with this work for additional information regarding copyright
 * ownership. The ASF licenses this file to you under the Apache
 * License, Version 2.0 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * permissions and limitations under the License.
 *
 **************************************************************************/

#include <cassert>
#include <limits>
#include <string>


int main ()
{
#ifndef _RWSTD_NO_WCHAR_T

    typedef std::char_traits<wchar_t> Traits;

    const wchar_t low[] = { 0, 0, std::numeric_limits<wchar_t>::min () };
    const wchar_t high[] = { 0, 0, std::numeric_limits<wchar_t>::max () };

    assert (0 == Traits::compare (0, 0, 0));
    assert (0 == Traits::compare (low, high, 2));
    assert (0 == Traits::compare (low, low, 3));
    assert (0 > Traits::compare (low, high, 3));
    assert (0 < Traits::compare (high, low, 3));

    // Collation position weights can compare 128 with an IGNORE marker.
    const wchar_t weight[] = { 128 };
    const wchar_t ignore[] = { std::numeric_limits<wchar_t>::max () };

    assert (0 > Traits::compare (weight, ignore, 1));
    assert (0 < Traits::compare (ignore, weight, 1));

    const wchar_t first[] = { low[2], high[2] };
    const wchar_t second[] = { high[2], low[2] };

    assert (0 > Traits::compare (first, second, 2));
    assert (0 < Traits::compare (second, first, 2));

#endif   // _RWSTD_NO_WCHAR_T

    return 0;
}
