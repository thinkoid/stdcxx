/***************************************************************************
 *
 * diag.cpp
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

#include "diag.h"

#include <cstdarg>   // for va_list
#include <cstdio>    // for fprintf(), vfprintf()


static void
print (const char *severity, int id, const where_t &where,
       const char *fmt, std::va_list va)
{
    if (where.file)
        std::fprintf (stderr, "%s:%d: ", where.file, where.line);

    std::fprintf (stderr, "%s %-3d: ", severity, id);
    std::vfprintf (stderr, fmt, va);
    std::fputc ('\n', stderr);
}


diag_t::
diag_t ()
    : m_errors (0), m_warnings (0), m_muted (false)
{
}


bool diag_t::
error (int id, const where_t &where, const char *fmt, ...)
{
    ++m_errors;

    std::va_list va;
    va_start (va, fmt);
    print ("Error", id, where, fmt, va);
    va_end (va);

    return false;
}


bool diag_t::
warning (int id, const where_t &where, const char *fmt, ...)
{
    ++m_warnings;

    if (m_muted)
        return false;

    std::va_list va;
    va_start (va, fmt);
    print ("Warning", id, where, fmt, va);
    va_end (va);

    return false;
}
