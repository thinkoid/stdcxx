/***************************************************************************
 *
 * diag.h
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

#ifndef RWSTD_UTIL_LOCALEDEF_DIAG_H_INCLUDED
#define RWSTD_UTIL_LOCALEDEF_DIAG_H_INCLUDED

#include <string>


// The numbers of the old compiler's scheme (util/diagnostic.h): errors
// below 700, warnings from 700 to 799.  A log of either compiler names
// the same fault by the same number.
enum diag_id_t {
    e_symused = 101,  // a weight names what the sequence does not order
    e_openrd = 109,   // a file cannot be opened for reading
    e_openwr = 110,   // a file cannot be opened for writing
    e_pair   = 111,   // invalid pair of names in a range
    e_symend = 127,   // unterminated symbolic name
    e_ifmt   = 135,   // invalid integer format
    e_mbchar = 308,   // ill-formed multibyte character
    e_range  = 311,   // invalid range
    e_syntax = 312,   // bad syntax
    e_ucs    = 315,   // a name that gives no UCS character
    e_inval  = 399,   // invalid value
    e_cmdarg = 401,   // invalid command line argument

    w_missing = 704,  // characters the collation sequence leaves out
    w_syntax = 709,   // recoverable syntax error
    w_inval  = 711    // recoverable invalid value
};


// A position in a source: the file, the physical line and the column,
// both counted from 1.  A diagnostic with no file has no position.
struct where_t {
    const char *file;
    int         line;
    int         column;
};

static const where_t nowhere = { 0, 0, 0 };


// The one sink of diagnostics.  It prints each to stderr in the old
// compiler's line format, "file:line: Error 312: message", and counts
// them.  The column is carried for the caller but the line format has
// no field for it.
class diag_t
{
public:

    diag_t ();

    // Both return false, so that a production reports and fails in one
    // statement: return diag.error (...).
    bool error (int id, const where_t &where, const char *fmt, ...);
    bool warning (int id, const where_t &where, const char *fmt, ...);

    int errors () const {
        return m_errors;
    }

    int warnings () const {
        return m_warnings;
    }

    // 4 when any error was counted, as the old compiler exits.
    int exit_status () const {
        return m_errors ? 4 : 0;
    }

    // -w of the old driver: a warning is counted and not printed.
    void mute_warnings () {
        m_muted = true;
    }

private:

    diag_t (const diag_t&);           // not defined
    void operator= (const diag_t&);   // not defined

    int  m_errors;
    int  m_warnings;
    bool m_muted;
};


// Thrown out of the reader when a file cannot be opened.  An error
// inside a file is a diagnostic; a file that is not there ends the
// statement that named it, and usually the run.
struct open_error_t {
    std::string path;
    int         error;   // errno of the failed open
};

#endif   // RWSTD_UTIL_LOCALEDEF_DIAG_H_INCLUDED
