/**************************************************************************
 *
 * iostream.cpp - source for the Standard Library iostream objects
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

#define _RWSTD_LIB_SRC

#include <rw/_defs.h>

#ifndef _RWSTD_NO_NATIVE_IO
#  include <unistd.h>
#else   // if defined _RWSTD_NO_NATIVE_IO
   // <stdio.h> precedes <fstream>: the private FILE* basic_filebuf ctor
   // that ios_base::Init uses is declared only where stdin is defined
#  include <stdio.h>
#endif   // _RWSTD_NO_NATIVE_IO

#include <iosfwd>
#include <istream>
#include <ostream>
#include <fstream>
#include <memory>
#include <new>

#include <rw/_error.h>
#include <rw/_once.h>

#include "iosdata.h"
#include "podarray.h"   // for __rw_aligned_buffer

#include <stdio.h>    // fprintf(), stderr
#include <stdlib.h>   // abort()



// #defined  below
#undef _INIT

// stream objects are initialized with static basic_filebuf objects
// based on static data buffers to avoid dynamic memory allocation
// at library initialization time

// cin, cout, and the other standard iostream objects are defined below
// in namespace std as __rw_aligned_buffer storage for their streams,
// zero-initialized and with no constructor or destructor (27.3)
// ios_base::Init constructs each stream and its filebuf in their storage
// via calls to placement new; the storage is never destroyed, so the
// objects outlive the destructors of other static objects (27.3, p2)
// <iostream> declares each object with its stream type and this unit
// defines it with the type of its storage, so this unit does not
// #include <iostream> (3.5, p10; departure)

// buftype: basic_filebuf specialization
// stream:  [w]istream or [w]ostream
// name:    name of the standard iostream object
#define _RWSTD_DEFINE_STREAM_OBJECT(buftype, stream, name)                    \
  namespace __rw {                                                   \
    /* static character buffer to prevent dynamic allocation */               \
    static std::stream::char_type __rw_##name##_databuf [512];                \
    /* static indestructible `basic_filebuf<>' object */                      \
    static __rw_aligned_buffer<std::buftype> __rw_##name##_buffer;            \
  }   /* namespace __rw */                                                    \
  namespace std {                                                    \
    /* extern indestructible `basic_[i|o]stream<>' object */                  \
    _RWSTD_EXPORT __rw::__rw_aligned_buffer<stream> name;                     \
  }   /* namespace std */                                                     \
  typedef void __rw_unused_typedef

// expression returning the address of the initialized object
#define _INIT(buftype, stream, name, f)                                       \
  /* construct basic_[i|o]stream object backed by a filebuf object */         \
  /* backed by a static characater array                           */         \
  new (std::name._C_store ()) stream                                          \
          (new (&__rw::__rw_##name##_buffer) buftype                          \
           (FILEREF (f), __rw::__rw_##name##_databuf,                         \
     sizeof __rw::__rw_##name##_databuf / sizeof *__rw::__rw_##name##_databuf)) \


#ifndef _RWSTD_NO_NATIVE_IO
   // use STDIN_FILENO etc. to initialize basic_filebuf
#  define FILEREF(name)   name ##_FILENO
#else
   // use stdin etc. to initialize basic_filebuf
#  define STDIN_FILEPTR   stdin
#  define STDOUT_FILEPTR  stdout
#  define STDERR_FILEPTR  stderr
#  define FILEREF(name)   name ##_FILEPTR
#endif   // _RWSTD_NO_NATIVE_IO


// define iostream objects and their corresponding "buffers"
_RWSTD_DEFINE_STREAM_OBJECT (filebuf, istream, cin);
_RWSTD_DEFINE_STREAM_OBJECT (filebuf, ostream, cout);
_RWSTD_DEFINE_STREAM_OBJECT (filebuf, ostream, cerr);
_RWSTD_DEFINE_STREAM_OBJECT (filebuf, ostream, clog);

_RWSTD_DEFINE_STREAM_OBJECT (wfilebuf, wistream, wcin);
_RWSTD_DEFINE_STREAM_OBJECT (wfilebuf, wostream, wcout);
_RWSTD_DEFINE_STREAM_OBJECT (wfilebuf, wostream, wcerr);
_RWSTD_DEFINE_STREAM_OBJECT (wfilebuf, wostream, wclog);


// allow basic_ostream<> inline member functions to refer to the standard
// iostream objects without making those objects visible to user code (so
// that they can safely be dereferenced only after #including <iostream>
// which is the only header where they are declared and by the means of
// which they are initialized - through the static initialization object)

namespace __rw { 

// array of pointers to the 8 standard iostream objects
extern const void* __rw_std_streams [];


// iostream initialization counter
static int __rw_ios_initcnt /* = 0 */;

// the standard iostream objects are constructed once
static __rw_once_t __rw_ios_once;

}   // namespace __rw


namespace std {

ios_base::Init::Init ()
{
    // every Init is counted, and the last one destroyed flushes the
    // streams; only the first constructs them, and the others wait
    // until it has (27.4.2.1.6, p3: the count never returns to zero)
    _RWSTD_ATOMIC_PREINCREMENT (__rw::__rw_ios_initcnt, false);

    __rw::__rw_once (&__rw::__rw_ios_once, _C_init);
}


/* static */ void
ios_base::Init::_C_init ()
{
    // the pointers address the streams constructed in the storage of
    // cin, et al; in this unit the names have the type of the storage
    istream* const pcin  = _INIT (filebuf, istream, cin,  STDIN);
    ostream* const pcout = _INIT (filebuf, ostream, cout, STDOUT);
    ostream* const pcerr = _INIT (filebuf, ostream, cerr, STDERR);
    ostream* const pclog = _INIT (filebuf, ostream, clog, STDERR);

    // stream objects expected to be arranged in this order
    // must be initialized dynamically (as opposed to statically)
    __rw::__rw_std_streams[0] = static_cast< ios_base* >(pcin);
    __rw::__rw_std_streams[1] = static_cast< ios_base* >(pcout);
    __rw::__rw_std_streams[2] = static_cast< ios_base* >(pcerr);
    __rw::__rw_std_streams[3] = static_cast< ios_base* >(pclog);

    // 27.3.1, p2
    pcin->_C_usr = &_C_usr_data::_C_std_usr_data [0];
    pcin->_C_usr->_C_tie = pcout;

    // 27.3.1, p5
    pcerr->_C_fmtfl |= __rw::__rw_unitbuf;

    pcin->_C_sync  = 1;
    pcout->_C_sync = 1;
    pcerr->_C_sync = 1;
    pclog->_C_sync = 1;

    // the standard objects are locked ([iostream.objects]), unlike the
    // streams basic_ios<>::init() leaves unlocked
    pcin->_C_nolock  = 0;
    pcout->_C_nolock = 0;
    pcerr->_C_nolock = 0;
    pclog->_C_nolock = 0;


    wistream* const pwcin  = _INIT (wfilebuf, wistream, wcin,  STDIN);
    wostream* const pwcout = _INIT (wfilebuf, wostream, wcout, STDOUT);
    wostream* const pwcerr = _INIT (wfilebuf, wostream, wcerr, STDERR);
    wostream* const pwclog = _INIT (wfilebuf, wostream, wclog, STDERR);

    // stream objects expected to be arranged in this order
    // must be initialized dynamically (as opposed to statically)
    __rw::__rw_std_streams[4] = static_cast< ios_base* >(pwcin);
    __rw::__rw_std_streams[5] = static_cast< ios_base* >(pwcout);
    __rw::__rw_std_streams[6] = static_cast< ios_base* >(pwcerr);
    __rw::__rw_std_streams[7] = static_cast< ios_base* >(pwclog);

    // 27.3.2, p2
    pwcin->_C_usr = &_C_usr_data::_C_std_usr_data [1];
    pwcin->_C_usr->_C_tie = pwcout;

    // 27.3.2, p5
    pwcerr->_C_fmtfl |= __rw::__rw_unitbuf;

    pwcin->_C_sync  = 1;
    pwcout->_C_sync = 1;
    pwcerr->_C_sync = 1;
    pwclog->_C_sync = 1;

    // the standard objects are locked ([iostream.objects])
    pwcin->_C_nolock  = 0;
    pwcout->_C_nolock = 0;
    pwcerr->_C_nolock = 0;
    pwclog->_C_nolock = 0;

}


ios_base::Init::~Init ()
{
    if (_RWSTD_ATOMIC_PREDECREMENT (__rw::__rw_ios_initcnt, false))
        return;

    // put all standard iostream objects in unbuffered mode so that
    // output made from dtors of user objects with static storage
    // duration destroyed from this point on (i.e., after the destruction
    // of *this, the last initializer object) is not lost

    const char fmt[] = "exception during std::%s cleanup\n";

    int caught = 0;

    try {
        cout._C_data ()->rdbuf ()->pubsetbuf (0, 0);
    }
    catch (...) {
        fprintf (stderr, fmt, "cout");
        ++caught;
    }
    try {
        cerr._C_data ()->rdbuf ()->pubsetbuf (0, 0);
    }
    catch (...) {
        fprintf (stderr, fmt, "cerr");
        ++caught;
    }
    try {
        clog._C_data ()->rdbuf ()->pubsetbuf (0, 0);
    }
    catch (...) {
        fprintf (stderr, fmt, "clog");
        ++caught;
    }

    try {
        wcout._C_data ()->rdbuf ()->pubsetbuf (0, 0);
    }
    catch (...) {
        fprintf (stderr, fmt, "wcout");
        ++caught;
    }
    try {
        wcerr._C_data ()->rdbuf ()->pubsetbuf (0, 0);
    }
    catch (...) {
        fprintf (stderr, fmt, "wcerr");
        ++caught;
    }
    try {
        wclog._C_data ()->rdbuf ()->pubsetbuf (0, 0);
    }
    catch (...) {
        fprintf (stderr, fmt, "wclog");
        ++caught;
    }

    if (caught)
        abort ();
}


}   // namespace std
