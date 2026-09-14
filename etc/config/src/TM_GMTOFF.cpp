// checking for tm_gmtoff in struct tm in <time.h>

/***************************************************************************
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
 * Copyright 1999-2007 Rogue Wave Software, Inc.
 * 
 **************************************************************************/

#include <time.h>

// check to see if struct tm has the BSD extension members tm_gmtoff
// and tm_zone; GNU glibc offers them by default but renames them
// into the reserved namespace in the strict ISO C environment

long get_tm_gmtoff (const struct tm *tmb)
{
    return tmb->tm_gmtoff;
}

const char* get_tm_zone (const struct tm *tmb)
{
    return tmb->tm_zone;
}
