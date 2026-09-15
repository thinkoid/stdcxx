/***************************************************************************
 *
 * gencat.cpp - Utility for generating message catalogs on Windows
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

#include <cstdlib>   // for system(), getenv()
#include <cstdio>    // for printf()
#include <cstring>   // for strcmp(), strrchr()
#include <cstddef>   // for size_t
#include <cassert>   // for assert()

#include <string>

static const char
usage_text[] = {
    "Usage: %s OUTPUT-FILE INPUT-FILE\n"
    "Generate message catalog.\n"
    "\n"
    "  -?, --help                 Give this help list\n"
};

#define SLASH '/'

int main (int argc, char *argv[])
{
    const char* exe_name = "gencat";

    if (argv [0]) {
        if (const char* slash = std::strrchr (argv [0], SLASH))
            exe_name = slash + 1;
        else
            exe_name = argv [0];
    }

    assert (exe_name);

    if (1 == argc) {
        std::printf (usage_text, exe_name);
        return 0;
    }

    --argc;

    while (0 != *++argv && 0 < argc-- && '-' == **argv) {

        switch (*++*argv) {

        case '?':
            std::printf (usage_text, exe_name);
            return 0;

        case '-':
            if (0 == std::strcmp (*argv, "-help")) {
                std::printf (usage_text, exe_name);
                return 0;
            }

            // fall through...
        default:
            std::printf ("%s: invalid option -%s\n",
                         exe_name, *argv);
            return 1;
        }
    }

    if (1 > argc) {
        std::printf ("%s: missing arguments\n Try '%s --help'\n",
                     exe_name, exe_name);
        return 1;
    }

    std::string cmd;

    const char* const cat_name = argv [0];
    const char* const msg_name = argv [1];

    cmd = "/usr/bin/gencat ";
    cmd += cat_name;
    cmd += ' ';
    cmd += msg_name;

    const int ret = std::system (cmd.c_str ());

    return ret;
}
