#!/bin/sh
#
# $Id$
#
##############################################################################
#
# Licensed to the Apache Software  Foundation (ASF) under one or more
# contributor  license agreements.  See  the NOTICE  file distributed
# with  this  work  for  additional information  regarding  copyright
# ownership.   The ASF  licenses this  file to  you under  the Apache
# License, Version  2.0 (the  "License"); you may  not use  this file
# except in  compliance with the License.   You may obtain  a copy of
# the License at
#
# http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the  License is distributed on an  "AS IS" BASIS,
# WITHOUT  WARRANTIES OR CONDITIONS  OF ANY  KIND, either  express or
# implied.   See  the License  for  the  specific language  governing
# permissions and limitations under the License.
#
# Copyright 2001-2008 Rogue Wave Software, Inc.
#
##############################################################################
#
# Test to determine which C library headers exist and, for each, the
# full pathname the library's own wrapper headers use to include it
#
# To test, perform the following steps after making sure that vars.sh
# exists in the cwd, or after defining the appropriate shell variables:
#
#   $ rm -f header.h logfile.log
#   $ touch header.h logfile.log
#   $ libc_headers header.h logfile.log [headers]
#
##############################################################################

output=/dev/tty
logfile=/dev/tty

[ ! -d "$TMPDIR" ] && TMPDIR=/tmp

# set the output file if specified, otherwise use the console
if [ -f "$1" ]; then
    output="$1"
fi

# set the log file if specified, otherwise use the console
if [ -f "$2" ]; then
    logfile="$2"
fi

[ -r vars.sh ] && . ./vars.sh

# the headers to look for: the C library's by their C++ names without
# the c prefix, the two C++ ones, and two POSIX ones by file name
hdrs="assert ctype errno float iso646 limits locale math setjmp signal       \
      stdarg stddef stdio stdlib string time wchar wctype new typeinfo"
hdrs="$hdrs ieeefp.h pthread.h"

if [ ! -z "$3" ]; then
    hdrs="$3"
fi



CPPFLAGS="`echo $CPPFLAGS | sed 's:-I *[^ ]*::g'`"

capitalize='sed y/abcdefghijklmnopqrstuvwxyz/ABCDEFGHIJKLMNOPQRSTUVWXYZ/'

##############################################################################
# determine whether each header exists and its full pathname

for h in $hdrs ; do

    hdr_base=`basename $h \.h`
    hdr=$h

    if [ "$hdr_base" = "$h" ] ; then
        # check for C++ C library headers first
        if [ $h != new -a $h != typeinfo ] ; then
            hdr="c$h"
        fi;

        printf "%-50.50s " "checking for <$hdr>"

        tmpfile=$TMPDIR/$h-$$
        echo "#include <$hdr>" > $tmpfile.cpp
        sym="`echo $hdr | $capitalize`"

        echo "$CXX -E $CPPFLAGS $CXXFLAGS $tmpfile.cpp >$tmpfile.i" >>$logfile
        $CXX -E $CPPFLAGS $CXXFLAGS $tmpfile.cpp >$tmpfile.i 2>>$logfile
        if [ $? -eq 0 ] ; then
            echo "ok"
            echo "// #define _RWSTD_NO_${sym}" >>$output
        else
            echo "no (_RWSTD_NO_${sym})"
            echo "#define _RWSTD_NO_${sym}" >> $output
        fi
    fi

    # check for (deprecated C++) C library headers
    # or for any headers specified with the .h suffix
    printf "%-50.50s " "checking for <$hdr_base.h>"

    tmpfile=$TMPDIR/$hdr_base-$$
    echo "#include <$hdr_base.h>" > $tmpfile.cpp
    sym="`echo $hdr_base | $capitalize`"

    echo "$CXX -E $CPPFLAGS $CXXFLAGS $tmpfile.cpp >$tmpfile.i" >>$logfile
    $CXX -E $CPPFLAGS $CXXFLAGS $tmpfile.cpp >$tmpfile.i 2>>$logfile
    if [ $? -eq 0 ] ; then
        path=`  cat $tmpfile.i \
              | sed -n "s|[^\"]* \(\".*/$hdr_base\.h\"\).*|\1|p" \
              | head -n 1`

        # handle headers implemented internally by some compilers
        # (such as <stdarg.h> with the vanilla EDG eccp)
        [ "$path" = "" ] && path="<$hdr_base.h>"

        echo "ok ($path)"
        if [ "$hdr_base" = "$h" ]; then
            echo "#define _RWSTD_ANSI_C_${sym}_H $path" >>$output
        else
            echo "// #define _RWSTD_NO_${sym}_H   /* $path */" >>$output
        fi
    else
        echo "no (_RWSTD_NO_${sym}_H)"
        echo "#define _RWSTD_NO_${sym}_H" >>$output
    fi

    rm -f $tmpfile.cpp $tmpfile.i

done
