/***************************************************************************
 *
 * filebuf.cpp - basic_filebuf example
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
 * Copyright 1994-2006 Rogue Wave Software.
 * 
 **************************************************************************/
 
#include <fstream>    // for filebuf, ifstream, istream
#include <iostream>   // for cout, endl
#include <cstdio>     // for remove()

#include <examples.h>

int main ()
{
    // the name of a temporary file in the current directory
    const char fname[] = "filebuf.tmp";

    // create a filebuf object for reading and writing of a temporary file
    std::filebuf outbuf;

    outbuf.open (fname, std::ios::in | std::ios::out | std::ios::trunc);

    // set the internal character buffer size
    // buffer will be allocated internally and deallocated in object dtor
    outbuf.pubsetbuf (0, 256);
 
    // associate the filebuf object with an iostream object
    std::iostream out (&outbuf);

    // open this source code file and output it to the temporary file
    out << std::ifstream (__FILE__).rdbuf ();
  
    // seek to the beginning of the stream
    out.seekp (0);

    // output the contents of the `out' object to standard output
    std::cout << out.rdbuf ();  

    // close the filebuf object before removing the underlying file
    outbuf.close ();
  
    // remove the temporary file
    std::remove (fname);

    return 0;
}
