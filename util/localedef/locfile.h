/***************************************************************************
 *
 * locfile.h
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

#ifndef RWSTD_UTIL_LOCALEDEF_LOCFILE_H_INCLUDED
#define RWSTD_UTIL_LOCALEDEF_LOCFILE_H_INCLUDED

#include "cmap.h"
#include "diag.h"
#include "model.h"

#include <string>


// Reads the definition at path into def, its strings resolved against
// cmap.  A category taken by copy is read from the locale of that name
// in dir.  False if it reported an error.  Throws open_error_t when
// path cannot be opened.
bool read_locale (const std::string &path, const std::string &dir,
                  const cmap_t &cmap, locale_def_t &def, diag_t &diag);

#endif   // RWSTD_UTIL_LOCALEDEF_LOCFILE_H_INCLUDED
