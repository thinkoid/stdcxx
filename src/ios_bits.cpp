/**************************************************************************
 *
 * ios_bits.cpp - definitions of ios_base static const data members
 *
 * $Id$
 *
 ***************************************************************************
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
 * Copyright 1994-2006 Rogue Wave Software.
 * 
 **************************************************************************/

#define _RWSTD_LIB_SRC


#include <rw/_iosbase.h>
#include <rw/_defs.h>


namespace std { 

// required to be defined by 9.4.2, p4, even if initialized inside class
const ios_base::fmtflags ios_base::boolalpha;
const ios_base::fmtflags ios_base::dec;
const ios_base::fmtflags ios_base::fixed;
const ios_base::fmtflags ios_base::hex;
const ios_base::fmtflags ios_base::internal;
const ios_base::fmtflags ios_base::left;
const ios_base::fmtflags ios_base::oct;
const ios_base::fmtflags ios_base::right;
const ios_base::fmtflags ios_base::scientific;
const ios_base::fmtflags ios_base::showbase;
const ios_base::fmtflags ios_base::showpoint;
const ios_base::fmtflags ios_base::showpos;
const ios_base::fmtflags ios_base::skipws;
const ios_base::fmtflags ios_base::unitbuf;
const ios_base::fmtflags ios_base::uppercase;

const ios_base::fmtflags ios_base::adjustfield;
const ios_base::fmtflags ios_base::basefield;
const ios_base::fmtflags ios_base::floatfield;

const ios_base::iostate ios_base::goodbit;
const ios_base::iostate ios_base::badbit;
const ios_base::iostate ios_base::eofbit;
const ios_base::iostate ios_base::failbit;

const ios_base::openmode ios_base::app;
const ios_base::openmode ios_base::binary;
const ios_base::openmode ios_base::in;
const ios_base::openmode ios_base::out;
const ios_base::openmode ios_base::trunc;
const ios_base::openmode ios_base::ate;

const ios_base::seekdir ios_base::beg;
const ios_base::seekdir ios_base::cur;
const ios_base::seekdir ios_base::end;


}   // namespace std
