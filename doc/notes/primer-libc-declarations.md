# Declared, or only defined: how the configuration asks the C library

As of 2026-09-28 (5f3b424b).

A C library can export a function its headers do not declare. A
dialect switch hides it, a standard revision withdraws it, or a
vendor never got round to the prototype. The function is still in
the binary, and a program that declares it itself can call it.

The configuration tells these cases apart, one function at a time.
The headers repair each case in its own way. This note follows the
machinery from the list of functions to the header lines that act on
the answer. It ends with the one question the machinery does not
ask. Every claim was checked against the tree at the commit above,
GCC 16.2.1, Clang 22.1.8 and glibc 2.44.

## 0. The short version

- **Two probes per function.** `etc/config/src/libc_decl.sh` walks
  the functions listed in `etc/config/src/headers.inc`. The first
  probe includes the libc header, uses the function, and links. If
  that fails, the second probe includes nothing, declares the name
  as `extern "C" void f ()`, and links.
- **Three states, two macros.** The first probe yields
  `_RWSTD_NO_<F>`, the second `_RWSTD_NO_<F>_IN_LIBC`:

  | `_RWSTD_NO_<F>` | `_RWSTD_NO_<F>_IN_LIBC` | meaning |
  |---|---|---|
  | undefined | not written | declared in the header |
  | defined | undefined | not declared, but the symbol is in libc |
  | defined | defined | neither; the library must provide it |

- **Three repairs.** The headers under `include/ansi/` read the pair.
  A declared function is re-exported (`using ::f;`). A function only
  in libc gets a prototype from the header. A missing function gets
  a definition. After a repair the header undefines `_RWSTD_NO_<F>`.
- **On glibc, one function takes the second probe:** `gets`. C11 and
  C++14 withdrew it, glibc no longer declares it, and libc still
  exports it. Every other function is declared. The repair arms are
  dormant on the platform the tree is tested on.
- **Neither probe sees the type.** The first accepts either shape of
  an overloaded function, by design. The second never sees a
  prototype. A header that declares a function with the wrong C++
  signature reads as "declared". `analysis-glibc-wchar-proto.md`
  is about that gap.

## 1. The problem it solves

C89 let a C library be incomplete, and the vendor libraries of the
1990s were. Headers lagged the binaries. A new function appeared in
the header only under a feature macro, or only in the next release.
The wide-character functions of the 1995 amendment made this common:
libc exported them years before `<wchar.h>` declared them all.

A C++ library that re-exports the C library has three options for a
function missing from a header:

- Drop the name from `std`. Conforming programs fail.
- Declare it. Right only if the symbol exists.
- Define it. Right only if the symbol does not exist. A second
  definition of a libc function is a conflict at best and a silent
  replacement at worst.

The choice between the last two needs a fact about the binary. The
header cannot supply it. Hence two probes.

## 2. The list: `headers.inc`

`etc/config/src/headers.inc` is a shell fragment, sourced by the
probe script. `hdrs` names the headers to look for: the C90 set,
`wchar`, `wctype`, `new`, `typeinfo`, `ieeefp.h` and `pthread.h`.
One variable per header lists its functions: `math`, `stdio`,
`stdlib`, `string`, `time`, `wchar`, `wctype`. Some lists carry C99
and POSIX groups. `math` and `time` test only the C90 group.

An entry is a bare name or a name with arguments:

```
wchar="btowc fgetwc ... wcschr((wchar_t*)0,0) wcscmp ...
       wcspbrk((wchar_t*)0,(wchar_t*)0) ... wmemchr((wchar_t*)0,0,0) ..."
```

The spelling picks the test. A bare name has its address taken. A
name with arguments is called. The address of an overloaded function
cannot be taken without a target type, so the functions C++
overloads are called: `strchr`, `memchr`, `wcschr`, the `<cmath>`
functions. The arguments resolve against either the C prototype or
the C++ pair. `memchr((void*)0,0,0)` replaced a `char*` argument
that was ambiguous "in the presence of C++ overloads" (ed68a657,
2006).

The lists moved from the script into this file in 2006 (9f5c21aa).
The Windows configuration, since retired, read them too.

## 3. The probes: `libc_decl.sh`

`GNUmakefile.cfg` runs the script before any `.cpp`
characterization. It first writes `vars.sh`, a shell rendering of
the build directory's `makefile.in`. The script takes its compiler,
flags and linker from there. It sees what every characterization
sees: the compiler and `makefile.in`, nothing from the command line.
It logs every command to `<build>/include/config.log`.

### 3.1 The headers

For each header the script preprocesses `#include <cH>` and
`#include <H.h>`. It writes:

- `_RWSTD_NO_C<H>` when `<cH>` is missing;
- `_RWSTD_ANSI_C_<H>_H "/usr/include/H.h"`, the full path of the C
  header. The library's own `<H.h>` includes the real one by this
  path (`#include _RWSTD_ANSI_C_WCHAR_H`), so it cannot find itself;
- `_RWSTD_NO_<H>_H` when `<H.h>` is missing.

The configuration compiles with `-nostdinc++`. Every `<cH>` is
missing, so `_RWSTD_NO_CWCHAR` and its siblings are always defined
on Linux. The function probes therefore include the `.h` form, which
is glibc's header.

### 3.2 Declared?

One template source, compiled per function with
`-DCHECK_DECL -DHDRNAME="<wchar.h>" -DFUNNAME=wcschr
-DFUN=wcschr((wchar_t*)0,0) -DTAKE_ADDR=0`:

```cpp
#include HDRNAME
namespace std { }
using namespace std;

int main (int argc, char**)
{
#if TAKE_ADDR
    funptri.pf = (funptr_t)&FUNNAME;     // bare names
    return (argc < 1 ? !funptri.ul : !!funptri.ul) + 0;
#else
    (void)FUN;                            // names with arguments
    return 0;
#endif
}
```

The object is linked, so a declaration without a definition fails
too. The `funptri` union and the `argc` test keep the optimizer from
folding the address away. One compiler objected to the union's type;
989e8e9c (2006) fixed that. Success writes
`// #define _RWSTD_NO_WCSCHR`. Failure writes the macro live and
runs the second probe.

### 3.3 Only defined?

The same template without `CHECK_DECL`:

```cpp
extern "C" void FUNNAME ();

int main (int argc, char**)
{
    funptri.pf = (funptr_t)&FUNNAME;
    return (argc < 1 ? !funptri.ul : !!funptri.ul) + 0;
}
```

No header, a deliberately wrong prototype, a link. The linker matches
a C symbol by name alone. The question is exactly "does libc export
a symbol of this name". A successful link writes
`// #define _RWSTD_NO_WCSCHR_IN_LIBC`. For `<math.h>` the macro ends
in `_IN_LIBM` and the link adds `$(LIBM)` (5ef3d92e, 2011).

### 3.4 Other readers, and one collision

`FLOAT.cpp` reads `_RWSTD_NO_STRTOF_IN_LIBC` and the `strtod` and
`strtold` siblings. It declares the functions itself when they are
only in libc. On glibc the script never writes these three macros:
the functions are declared. `config.h` still carries them, commented,
among `FLOAT.cpp`'s output. The dependency rule of `GNUmakefile.cfg`
put them there; it resolves every `#ifdef _RWSTD_` in a
characterization before compiling it. How it spells a macro that has
no characterization was not traced.

Header and function macros share a prefix, and they meet once.
`_RWSTD_NO_CTIME` means both "no `<ctime>`" and "no `ctime ()`". The
header probe defines it. The function probe writes a commented line
after it. The macro stays defined. No header reads it for the
function, so `std::ctime` is unaffected.

## 4. The consumers

### 4.1 The headers

`include/ansi/cwchar` has one block per function. On Linux it takes
the `_RWSTD_NO_DEPRECATED_C_HEADERS` branch, set by `_config-gcc.h`.
A block reads:

```cpp
#ifndef _RWSTD_NO_BTOWC
using ::btowc;                          // declared: re-export
#elif !defined (_RWSTD_NO_BTOWC_IN_LIBC)
}   // namespace std
extern "C" wint_t btowc (int);          // in libc: declare it
namespace std {
using ::btowc;
#  undef _RWSTD_NO_BTOWC
#endif   // _RWSTD_NO_BTOWC
```

Where the library can implement the function, a third arm defines
it in the global namespace, `extern "C"` and `inline`, and
re-exports it. `wcscat`, `wcschr`, `wcsrchr` and `wmemchr` have one.
`include/ansi/wchar.h` has the matching prototype blocks for the
global namespace. `<cstring>`, `<cstdio>`, `<cwctype>` and `<cmath>`
repeat the pattern.

For the functions C++ overloads on constness, the "in libc" arm
declares the C function non-const. It adds the const overload as an
inline forwarder to the same symbol:

```cpp
extern "C" wchar_t* wcschr (wchar_t*, wchar_t);

inline const wchar_t* wcschr (const wchar_t *__s, wchar_t __c)
{
    return wcschr (_RWSTD_CONST_CAST (wchar_t*, __s), __c);
}
```

The arm assumes the header declared nothing. Chapter 6 covers a
header that declared something else.

### 4.2 The sources

The compiled library reads the pair where it calls libc directly.

- `src/collate.cpp` declares `wcscoll`, `wcsxfrm` and `wcstombs` when
  they are only in libc. With no `wcsxfrm` at all it uses its own
  `__rw_wcsxfrm`.
- `src/wcodecvt.cpp` does the same for the restartable conversions,
  `mbrlen`, `mbrtowc`, `wcrtomb`, `wcsrtombs`, and the restartless
  ones.
- `include/rw/_traits.h` picks libc's `wmemcpy`, `wmemcmp`,
  `wmemmove`, `wmemset`, `wcslen` and `wmemchr`, or an exported
  `__rw_` version of each. Debug builds always take the `__rw_`
  versions.

### 4.3 The `#undef` convention

An arm that makes a function available undefines `_RWSTD_NO_<F>`.
The macro then means "not usable", not "not declared by libc". A
later `#ifndef _RWSTD_NO_<F>` in the same translation unit uses the
function without deciding again. Two consequences. The order of
inclusion matters. And an `#undef` of the wrong name goes unnoticed
while the arm is never taken; chapter 6.2 has one.

## 5. On glibc today

The six standing build directories, GCC and Clang, 32 and 64 bits,
write byte-identical lines for every function in `headers.inc`. One
declaration probe fails:

```
error: 'gets' was not declared in this scope; did you mean 'getw'?
```

glibc declares `gets` only under `__GLIBC_USE (DEPRECATED_GETS)`.
The C++ default, `_GNU_SOURCE`, no longer turns that on. The symbol
is still exported. So `config.h` has `#define _RWSTD_NO_GETS` and
`// #define _RWSTD_NO_GETS_IN_LIBC`: only defined. `<cstdio>` leaves
`gets` out of `std`, as C++14 requires.

Every other function is declared. On this platform the "in libc" and
"neither" arms are code no test compiles.

## 6. What it cannot see

### 6.1 The type

The declaration probe asks whether a name is callable with given
arguments. Its call forms pass the C prototype and the C++ pair
alike, on purpose. The libc probe asks whether a symbol exists, and
declares everything `void ()`. Neither records what the header
declared.

For most of the C library that is enough. The C standard fixes the
signature. It is not enough for the ten functions C++ replaces with
a const and non-const pair: `strchr`, `strpbrk`, `strrchr`, `strstr`,
`memchr`, and `wcschr`, `wcspbrk`, `wcsrchr`, `wcsstr`, `wmemchr`.
Their header can be in a fourth state, with no row in chapter 0's
table: declared, C prototype only. `wchar_t* wcschr (const wchar_t*,
wchar_t)` returns a modifiable pointer into a const string, and there
is no const overload.

Under Clang, glibc's `<wchar.h>` is in that state. The probes report
"declared". The headers re-export the C prototype unchanged.
`analysis-glibc-wchar-proto.md` has the measurement, glibc's
history, and the repair.

### 6.2 Its own dormant arms

No platform in the matrix takes the second and third arms. Nothing
compiles them, and a defect there survives. Four were found while
writing these notes, all on paths glibc never takes:

- `<cwchar>`'s own `wcschr` never advances its pointer.
- `<cwchar>`'s "in libc" `wcsrchr` calls itself.
- `<wchar.h>`'s `wcsrchr` block undefines `_RWSTD_NO_WCSCHR`.
- `<cwchar>` has no "in libc" arm for `wmemchr`.

The analysis note has the check for each. The instrument that would
catch them is a test that includes the headers with the macros forced
and calls each definition. None exists.

## 7. History

The script and the list came with the initial import of the 4.1.2
sources in 2005 (e6c7726b). The mechanism is older than the Apache
repository. The log since:

| commit | year | change |
|---|---|---|
| 5595d828 | 2005 | the POSIX `basename` on Solaris |
| 989e8e9c | 2006 | the address union given linkage, for an EDG front end |
| 3064b116, ed68a657 | 2006 | `memchr`/`wmemchr` call forms that resolve under C++ overloads |
| 9f5c21aa | 2006 | the lists moved to `headers.inc`, shared with the Windows configuration |
| cdf13acc | 2006 | `putenv`, `setenv`, `unsetenv` added |
| b913a877 | 2008 | the link command echoed to the log |
| 5ef3d92e | 2011 | `$(LIBM)` instead of a hard-coded `-lm` |
| 67c1f7b6 | 2026 | retired-platform branches removed from the build system around it |
