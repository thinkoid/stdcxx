/***************************************************************************
 *
 * era.h - the era records of LC_TIME, for time_put and time_get
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

#ifndef _RWSTD_ERA_H_INCLUDED
#define _RWSTD_ERA_H_INCLUDED


#include <time.h>   // for struct tm

#include <rw/_defs.h>
#include <loc/_localedef.h>


namespace __rw {


// returns a pointer to the era_t element describing the era
// corresponding to the date pointed to by `tmb', or 0 if no
// such era exists; the writer's rule, each bound compared as
// a whole date
const __rw_time_t::era_t*
__rw_get_era (const __rw_time_t*, const tm*);


// returns the number of the year of the date pointed to by `tmb'
// within the era `pera' that contains it
int
__rw_get_era_year (const __rw_time_t::era_t*, const tm*);


}   // namespace __rw


#endif   // _RWSTD_ERA_H_INCLUDED
