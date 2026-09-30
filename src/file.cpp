/**************************************************************************
 *
 * file.cpp - definition of file I/O helper functions
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
 * Copyright 2002-2008 Rogue Wave Software, Inc.
 * 
 **************************************************************************/

#define _RWSTD_LIB_SRC

// both macros must be defined before #including <rw/_defs.h>
#ifndef _RWSTD_NO_PURE_C_HEADERS
#  define _RWSTD_NO_PURE_C_HEADERS
#endif   // _RWSTD_NO_PURE_C_HEADERS

#ifndef _RWSTD_NO_DEPRECATED_C_HEADERS
#  define _RWSTD_NO_DEPRECATED_C_HEADERS
#endif   // _RWSTD_NO_DEPRECATED_C_HEADERS

#include <errno.h>    // for ERANGE, errno
#include <stddef.h>   // for ptrdiff_t
#include <stdio.h>    // for std{err,in,out}
#include <stdlib.h>   // for strtoul(), size_t
#include <ctype.h>    // for isalpha(), isspace(), toupper()


#include <unistd.h>
#include <fcntl.h>

#define _BINARY 0

#include <rw/_file.h>
#include <rw/_defs.h>



#ifndef _RWSTD_NO_PURE_C_HEADERS

// declare fileno in case it's not declared (for strict ANSI conformance)
extern "C" {

_RWSTD_DLLIMPORT int (fileno)(FILE*) _LIBC_THROWS ();

}   // extern "C"

#endif   // _RWSTD_NO_PURE_C_HEADERS


_RWSTD_NAMESPACE (__rw) {


#define _RWSTD_FILENO(f) fileno (f)


static const int __rw_io_modes [] = {
    /*  0 */ -1,
    /*  1 */ _RWSTD_O_CREAT | _RWSTD_O_APPEND | _RWSTD_O_WRONLY,
    /*  2 */ -1,
    /*  3 */ _RWSTD_O_CREAT | _RWSTD_O_APPEND | _RWSTD_O_WRONLY | _BINARY,
    /*  4 */ _RWSTD_O_RDONLY,
    /*  5 */ _RWSTD_O_RDONLY | _RWSTD_O_APPEND,
    /*  6 */ _RWSTD_O_RDONLY | _BINARY,
    /*  7 */ _RWSTD_O_CREAT | _RWSTD_O_APPEND | _RWSTD_O_RDONLY | _BINARY,
    /*  8 */ _RWSTD_O_CREAT | _RWSTD_O_TRUNC  | _RWSTD_O_WRONLY,
    /*  9 */ _RWSTD_O_CREAT | _RWSTD_O_APPEND | _RWSTD_O_WRONLY,
    /* 10 */ _RWSTD_O_CREAT | _RWSTD_O_TRUNC | _RWSTD_O_WRONLY | _BINARY,
    /* 11 */ _RWSTD_O_CREAT | _RWSTD_O_APPEND | _RWSTD_O_WRONLY | _BINARY,
    /* 12 */ _RWSTD_O_RDWR,
    /* 13 */ _RWSTD_O_CREAT | _RWSTD_O_APPEND | _RWSTD_O_RDWR,
    /* 14 */ _RWSTD_O_RDWR | _BINARY,
    /* 15 */ _RWSTD_O_CREAT | _RWSTD_O_APPEND | _RWSTD_O_RDWR | _BINARY,
    /* 16 */ _RWSTD_O_TRUNC,
    /* 17 */ _RWSTD_O_TRUNC | _RWSTD_O_APPEND,
    /* 18 */ _RWSTD_O_TRUNC | _BINARY,
    /* 19 */ _RWSTD_O_TRUNC | _RWSTD_O_APPEND | _BINARY,
    /* 20 */ _RWSTD_O_TRUNC | _RWSTD_O_RDONLY,
    /* 21 */ _RWSTD_O_TRUNC | _RWSTD_O_APPEND | _RWSTD_O_RDONLY,
    /* 22 */ _RWSTD_O_TRUNC | _RWSTD_O_RDONLY | _BINARY,
    /* 23 */ _RWSTD_O_TRUNC | _RWSTD_O_APPEND | _RWSTD_O_RDONLY | _BINARY,
    /* 24 */ _RWSTD_O_CREAT | _RWSTD_O_TRUNC | _RWSTD_O_WRONLY,
    /* 25 */ _RWSTD_O_CREAT | _RWSTD_O_TRUNC | _RWSTD_O_APPEND
                            | _RWSTD_O_WRONLY,
    /* 26 */ _RWSTD_O_CREAT | _RWSTD_O_TRUNC | _RWSTD_O_WRONLY | _BINARY,
    /* 27 */ _RWSTD_O_CREAT | _RWSTD_O_TRUNC | _RWSTD_O_APPEND
                            | _RWSTD_O_WRONLY | _BINARY,
    /* 28 */ _RWSTD_O_CREAT | _RWSTD_O_TRUNC | _RWSTD_O_RDWR,
    /* 29 */ _RWSTD_O_CREAT | _RWSTD_O_TRUNC | _RWSTD_O_APPEND | _RWSTD_O_RDWR,
    /* 30 */ _RWSTD_O_CREAT | _RWSTD_O_TRUNC | _RWSTD_O_RDWR | _BINARY,
    /* 31 */ _RWSTD_O_CREAT | _RWSTD_O_TRUNC | _RWSTD_O_APPEND | _RWSTD_O_RDWR
                            | _BINARY
};


static const char __rw_stdio_modes [][4] = {
    /*  0 */ "",      // error
    /*  1 */ "a",     // app
    /*  2 */ "",      // binary
    /*  3 */ "ab",    // binary|app
    /*  4 */ "r",     // in
    /*  5 */ "a",     // in|app
    /*  6 */ "rb",    // in|binary
    /*  7 */ "ab",    // in|binary|app
    /*  8 */ "w",     // out
    /*  9 */ "a",     // out|app
    /* 10 */ "wb",    // out|binary
    /* 11 */ "ab",    // out|binary|app
    /* 12 */ "r+",    // out|in
    /* 13 */ "a+",    // out|in|app
    /* 14 */ "r+b",   // out|in|binary
    /* 15 */ "a+b",   // out|in|binary|app
    /* 16 */ "w",     // trunc
    /* 17 */ "w",     // trunc|app
    /* 18 */ "wb",    // trunc|binary
    /* 19 */ "wb",    // trunc|binary|app
    /* 20 */ "",      // trunc|in
    /* 21 */ "",      // trunc|in|app
    /* 22 */ "",      // trunc|in|binary
    /* 23 */ "",      // trunc|in|binary|app
    /* 24 */ "w",     // trunc|out
    /* 25 */ "w",     // trunc|out|app
    /* 26 */ "wb",    // trunc|out|binary
    /* 27 */ "wb",    // trunc|out|binary|app
    /* 28 */ "w+",    // trunc|out|in
    /* 29 */ "w+",    // trunc|out|in|app
    /* 30 */ "w+b",   // trunc|out|in|binary
    /* 31 */ "w+b"    // trunc|out|in|binary|app
};


static const int
__rw_openmode_mask = ~(_RWSTD_IOS_ATE | _RWSTD_IOS_STDIO);


// converts iostream open mode to POSIX file access mode
static inline int
__rw_io_mode (int openmode)
{
    // mask out irrelevant bits
    const size_t inx = openmode & __rw_openmode_mask;

    if (sizeof __rw_io_modes / sizeof *__rw_io_modes <= inx)
        return -1;

    return __rw_io_modes [inx];
}


// converts iostream open mode to C stdio file open mode
// or the empty string on error (never returns null)
static inline const char*
__rw_stdio_mode (int openmode)
{
    // mask out irrelevant bits
    const size_t inx = openmode & __rw_openmode_mask;

    if (sizeof __rw_stdio_modes / sizeof *__rw_stdio_modes <= inx)
        return "";   // error -- bad mode

    const char* const stdio_mode = __rw_stdio_modes [inx];

    // verify postcondition: function never returns null
    _RWSTD_ASSERT (0 != stdio_mode);

    return stdio_mode;
}


_RWSTD_EXPORT void*
__rw_fopen (const char *fname, int openmode, long prot)
{
    // the null pointer names no file
    if (!fname)
        return 0;

    if (openmode & _RWSTD_IOS_STDIO) {

        // convert the iostream open mode to the C stdio file open mode
        const char* const fmode = __rw_stdio_mode (openmode);

        _RWSTD_ASSERT (0 != fmode);

        // fmode is the empty string on failure
        if ('\0' == *fmode)
            return 0;

        // open the named file using C stdio
        FILE* const file = fopen (fname, fmode);

        if (0 == file)
            return 0;

        return _RWSTD_STATIC_CAST (void*, file);
    }

    // convert the iostream open mode to the POSIX file access mode
    const int fdmode = __rw_io_mode (openmode);

    if (fdmode < 0)
        return 0;

    // open the named file
    const int fd = open (fname, fdmode, prot);

    if (fd < 0)
        return 0;

    return _RWSTD_REINTERPRET_CAST (void*, fd + 1);
}


_RWSTD_EXPORT void*
__rw_fdopen (int fd)
{
    if (fd < 0)
        return 0;

    return _RWSTD_REINTERPRET_CAST (void*, fd + 1);
}


_RWSTD_EXPORT int
__rw_fclose (void *file, int flags)
{
    if (flags & _RWSTD_IOS_STDIO) {
        FILE* const fp = _RWSTD_STATIC_CAST (FILE*, file);

        return fclose (fp);
    }

    const int fd = int (_RWSTD_STATIC_CAST (char*, file) - 1 - (char*)0);

    return close (fd);
}


_RWSTD_EXPORT int
__rw_fileno (void *file, int flags)
{
    if (flags & _RWSTD_IOS_STDIO)
        return _RWSTD_FILENO (_RWSTD_STATIC_CAST (FILE*, file));

    const int fd = int (_RWSTD_STATIC_CAST (char*, file) - 1 - (char*)0);

    return fd;
}


_RWSTD_EXPORT int
__rw_fdmode (int fd)
{
// FIXME -- need to have equivalent of fcntl() on win32.

   const int m = fcntl (fd, _RWSTD_F_GETFL);

   if (-1 == m)
       return _RWSTD_INVALID_OPENMODE;

   int mode = m & _RWSTD_O_APPEND ? _RWSTD_IOS_APP : 0;

   switch (m & _RWSTD_O_ACCMODE) {
       case _RWSTD_O_RDONLY: mode |= _RWSTD_IOS_IN; break;
       case _RWSTD_O_WRONLY: mode |= _RWSTD_IOS_OUT; break;
       case _RWSTD_O_RDWR:   mode |= _RWSTD_IOS_IN | _RWSTD_IOS_OUT; break;
   }

   return mode;

}


_RWSTD_EXPORT int
__rw_fmode (void *file, int flags)
{
    if (flags & _RWSTD_IOS_STDIO) {
        FILE* const fp = _RWSTD_STATIC_CAST (FILE*, file);

        return __rw_fdmode (_RWSTD_FILENO (fp));
    }

    const int fd = int (_RWSTD_STATIC_CAST (char*, file) - 1 - (char*)0);

    return __rw_fdmode (fd);
}


_RWSTD_EXPORT long
__rw_fseek (void *file, int flags, ptrdiff_t offset, int origin)
{
    if (flags & _RWSTD_IOS_STDIO) {
        FILE* const fp = _RWSTD_STATIC_CAST (FILE*, file);

        const int pos = fseek (fp, long (offset), origin);
        if (pos < 0)
            return long (pos);

        return ftell (fp);
    }

    const int fd = int (_RWSTD_STATIC_CAST (char*, file) - 1 - (char*)0);

    return lseek (fd, offset, origin);
}


_RWSTD_EXPORT size_t
__rw_fread (void *file, int flags, void *buf, size_t size)
{
    if (flags & _RWSTD_IOS_STDIO) {
        FILE* const fp = _RWSTD_STATIC_CAST (FILE*, file);

        return fread (buf, 1, size, fp);
    }

    const int fd = int (_RWSTD_STATIC_CAST (char*, file) - 1 - (char*)0);

    return read (fd, buf, size);
}


_RWSTD_EXPORT ptrdiff_t
__rw_fwrite (void *file, int flags, const void *buf, size_t size)
{
    if (flags & _RWSTD_IOS_STDIO) {
        FILE* const fp = _RWSTD_STATIC_CAST (FILE*, file);

        return _RWSTD_STATIC_CAST (ptrdiff_t, fwrite (buf, 1, size, fp));
    }

    const int fd = int (_RWSTD_STATIC_CAST (char*, file) - 1 - (char*)0);

    return write (fd, buf, size);
}


_RWSTD_EXPORT extern const void* __rw_std_streams[];


_RWSTD_EXPORT int
__rw_fflush (void *file, int flags)
{
    if (   file == __rw_std_streams [1] /* cout */
        || file == __rw_std_streams [5] /* wcout */)
        return fflush (stdout);

    if (   file == __rw_std_streams [2] /* cerr */
        || file == __rw_std_streams [3] /* clog */
        || file == __rw_std_streams [6] /* wcerr */
        || file == __rw_std_streams [7] /* wclog */)
        return fflush (stderr);

    if (file) {
        if (flags & _RWSTD_IOS_STDIO) {

            // `file' is a C file objec, flush it
            FILE* const fp = _RWSTD_STATIC_CAST (FILE*, file);

            return fflush (fp);
        }

        // `file' is a file descriptor, no need to flush it
    }
    else {

        // convenience call to flush both C files
        return fflush (stdout) + fflush (stderr);
    }

    return 0;
}


FILE* __rw_stderr = stderr;
FILE* __rw_stdin  = stdin;
FILE* __rw_stdout = stdout;

}   // namespace __rw
