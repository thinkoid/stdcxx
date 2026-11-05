/***************************************************************************
 *
 * 27.strstream.cpp - test exercising ostrstream and strstream
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

#include <strstream>    // for ostrstream, strstream

#include <rw_driver.h>  // for rw_test(), rw_assert()

/**************************************************************************/

// D.7.3.1, p2 and D.7.4.1, p2: the stream writes at the end of the
// array's string only when the mode has app; ate does not select it
template <class Stream>
static void
test_open_mode (const char *sname, std::ios::openmode mode,
                const char *mname, int pos)
{
    char buf [8] = "ab";

    Stream strm (buf, int (sizeof buf), mode);

    strm << 'x';

    rw_assert (strm.good (), 0, __LINE__,
               "%s (\"ab\", 8, %s) << 'x': good ()", sname, mname);

    rw_assert ('x' == buf [pos], 0, __LINE__,
               "%s (\"ab\", 8, %s) << 'x': buf [%d] == 'x', got %{#c}",
               sname, mname, pos, buf [pos]);
}


static void
test_open_modes ()
{
    rw_info (0, 0, __LINE__, "D.7.3.1 and D.7.4.1: app and ate");

    const std::ios::openmode out = std::ios::out;
    const std::ios::openmode io  = std::ios::in | std::ios::out;

    test_open_mode<std::ostrstream>("ostrstream", out | std::ios::ate,
                                    "out | ate", 0);
    test_open_mode<std::ostrstream>("ostrstream", out | std::ios::app,
                                    "out | app", 2);

    test_open_mode<std::strstream>("strstream", io | std::ios::ate,
                                   "in | out | ate", 0);
    test_open_mode<std::strstream>("strstream", io | std::ios::app,
                                   "in | out | app", 2);
}

/**************************************************************************/

static int
run_test (int, char**)
{
    test_open_modes ();

    return 0;
}

/**************************************************************************/

int main (int argc, char** argv)
{
    return rw_test (argc, argv, __FILE__,
                    "depr.str.strstreams",
                    0 /* no comment */,
                    run_test,
                    0 /* no command line options */);
}
