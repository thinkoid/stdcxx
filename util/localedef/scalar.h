/***************************************************************************
 *
 * scalar.h
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

#ifndef RWSTD_UTIL_LOCALEDEF_SCALAR_H_INCLUDED
#define RWSTD_UTIL_LOCALEDEF_SCALAR_H_INCLUDED

#include "cmap.h"
#include "diag.h"
#include "model.h"
#include "reader.h"

#include <string>


// The readers of the four scalar categories, LC_NUMERIC, LC_MONETARY,
// LC_TIME and LC_MESSAGES.  Each reads the lines of its category after
// head, the line of the category keyword, up to and including the END
// line, or to the end of head's file.  A category whose only statement
// is copy is left as it is and the name is returned in copy, for the
// caller to read that locale's category; copy is empty otherwise.

void read_numeric (reader_t &reader, const line_t &head, const cmap_t &cmap,
                   numeric_def_t &def, std::string &copy, diag_t &diag);

void read_monetary (reader_t &reader, const line_t &head, const cmap_t &cmap,
                    monetary_def_t &def, std::string &copy, diag_t &diag);

void read_time (reader_t &reader, const line_t &head, const cmap_t &cmap,
                time_def_t &def, std::string &copy, diag_t &diag);

void read_messages (reader_t &reader, const line_t &head, const cmap_t &cmap,
                    messages_def_t &def, std::string &copy, diag_t &diag);


// END category, the last line of a section, at it.  glibc ends the
// section at any END line and reports one that names another category.
bool end_of_section (tok_iter &it, tok_iter last, const token_t &head,
                     diag_t &diag);

#endif   // RWSTD_UTIL_LOCALEDEF_SCALAR_H_INCLUDED
