// checking for __mbstate_t in <bits/types/__mbstate_t.h>

/***************************************************************************
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
 **************************************************************************/

// glibc 2.26 and beyond keep the private conversion state type
// __mbstate_t in a header of its own, apart from the public alias
// mbstate_t, so that <stdio.h> can see the former without the latter

#include <bits/types/__mbstate_t.h>

// compile-only test:
// check to see if the header above defined __mbstate_t
__mbstate_t state;
