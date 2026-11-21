#!/bin/sh
########################################################################
#
# script to run the codecvt oracle over the locales of SUPPORTED
#
########################################################################
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
########################################################################
#
# USAGE:
#
#     codecvt-all.sh [-n count] <oracle> [<SUPPORTED>]
#
# DESCRIPTION:
#
#     Runs the codecvt oracle, built from codecvt.cpp, once for each
#     line of SUPPORTED (etc/nls/SUPPORTED by default), with the
#     databases under RWSTD_LOCALE_ROOT and the charmap of the line
#     from the charmaps directory beside SUPPORTED. Each line names a
#     locale and its charmap, "aa_DJ ISO-8859-1", "aa_DJ.UTF-8 UTF-8",
#     "de_DE@euro ISO-8859-15". The library's name of the locale is
#     composed as etc/config/GNUmakefile.bin composes it, the codeset
#     the charmap's and a modifier last: aa_DJ.ISO-8859-1,
#     aa_DJ.UTF-8, de_DE.ISO-8859-15@euro. The C library's name is
#     the first column with its codeset, if any, spelled as glibc
#     normalizes it, lower case and letters and digits only:
#     aa_DJ, aa_DJ.utf8, de_DE@euro. A name "locale -a" does not
#     list is reported and counted as an error.
#
#     Writes the disagreements the oracle lists, then its line per
#     locale, "<name>: K items, N disagreements (...)", then the
#     totals. -n passes the number of disagreements the oracle lists
#     per check. The exit status is 0 when nothing disagrees and
#     every locale ran.
#
# EXAMPLE:
#
#     RWSTD_LOCALE_ROOT=$B/nls bin/oracle/codecvt-all.sh ./codecvt-oracle
#
#     The command above checks the 502 locales of a build directory
#     $B whose "make locales" has run.
#
########################################################################

##########
# global constants

# the name of the script (used in diagnostic messages)
readonly myname=${0##*/}


##########
# main body of script

listed=
if [ "$1" = "-n" -a $# -gt 1 ]; then
    listed="-n $2"
    shift 2
fi

if [ $# -lt 1 -o $# -gt 2 ]; then
    echo "$myname: usage: $myname [-n count] <oracle> [<SUPPORTED>]" >&2
    exit 2
fi

oracle=$1
supported=${2:-`dirname $0`/../../etc/nls/SUPPORTED}
charmaps=`dirname $supported`/charmaps

if [ -z "$RWSTD_LOCALE_ROOT" ]; then
    echo "$myname: RWSTD_LOCALE_ROOT is not set" >&2
    exit 2
fi

available=`locale -a`

locales=0
clean=0
errors=0
total_dis=0

while read name charmap; do

    # the makefile's two expressions, on one line of SUPPORTED
    locname=`echo "$name $charmap" \
        | sed -e s:"^ *\([^ .@]*\)\.[^ @]*":"\1":g \
              -e s:"^\([^ .@]*\)\([^ ]*\)  *\([^ ]*\) *":"\1\.\3\2":g`

    libcname=`echo "$name" | awk '{
        if (match ($0, /\.[^@]*/)) {
            cs = tolower (substr ($0, RSTART + 1, RLENGTH - 1))
            gsub (/[^a-z0-9]/, "", cs)
            $0 = substr ($0, 1, RSTART) cs substr ($0, RSTART + RLENGTH)
        }
        print
    }'`

    locales=$((locales + 1))

    if ! echo "$available" | grep -qxF "$libcname"; then
        echo "$locname: the C library has no $libcname"
        errors=$((errors + 1))
        continue
    fi

    out=`"$oracle" $listed -c "$libcname" -f "$charmaps/$charmap" \
                   "$locname" 2>&1`
    status=$?

    # the oracle's last line: "<name>: K items, N disagreements (...)"
    last=`echo "$out" | tail -n 1`
    dis=`echo "$last" | sed -n \
        "s/^.*: [0-9]* items, \([0-9]*\) disagreements .*$/\1/p"`

    if [ $status -gt 1 -o -z "$dis" ]; then
        echo "$out"
        echo "$locname: error, exit status $status"
        errors=$((errors + 1))
        continue
    fi

    echo "$out" | grep "^  "
    echo "$last"

    total_dis=$((total_dis + dis))

    if [ $dis -eq 0 ]; then
        clean=$((clean + 1))
    fi

done < "$supported"

echo "total: $locales locales, $clean without disagreements," \
     "$total_dis disagreements, $errors errors"

[ $total_dis -eq 0 -a $errors -eq 0 ]
