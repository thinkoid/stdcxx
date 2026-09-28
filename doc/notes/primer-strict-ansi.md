# `__STRICT_ANSI__`: what the compiler promises and what the C library withholds

A macro with a misleading air of authority. It is not in any standard,
it is not defined by the C library, and on the platform most people
build on it decides less than its name suggests. This note traces it
from the compiler switch that defines it to the header lines that read
it, with every claim checked against GCC 16.2.1, glibc 2.44's installed
headers, and the glibc source history.

## 0. The short version

- **Who defines it.** The compiler, and only the compiler, if and only
  if a non-`gnu` dialect was requested: `-ansi` or `-std=c++17`, never
  `-std=gnu++17` or the default. Clang follows the same rule.
  `-pedantic` does not touch it.
- **What the compiler changes.** The complete predefined-macro diff
  between `gnu++17` and `c++17` is four lines: the macro appears, the
  unprefixed `unix` and `linux` disappear, and `__int128` stops being
  advertised as an extended integer type. At the language level the
  `asm`, `typeof` and `inline` extension keywords go. That is all.
- **What glibc does with it.** It is an input to `features.h`, since
  1997, with one job: switch off the default environment. If you asked
  for nothing, glibc gives you ISO C plus POSIX plus the BSD and
  System V extensions. If the compiler says strict and nothing else is
  requested, glibc gives you the named ISO standard and nothing more.
  `strdup` and `fileno` vanish, and the `struct tm` extension members
  retreat to their reserved names, since the layout must not change
  with the dialect. Explicit `_*_SOURCE` macros bring things back
  piecemeal; `_DEFAULT_SOURCE` or `_GNU_SOURCE` bring everything back.
- **The C++ twist.** The `g++` driver adds `-D_GNU_SOURCE` to every
  C++ compilation, and in `features.h` that rule sits above the strict
  rule and wins. So under `g++` or `clang++` on GNU/Linux the library
  half of the macro is moot in every dialect. `-U_GNU_SOURCE` on the
  command line undoes the driver's define, and that one flag shows
  what a C++ compiler that does not predefine it would have seen.
- **Other readers.** GCC's own `limits.h` and `stdarg.h`, and about a
  dozen libstdc++ sites, each withholding a GNU extension in strict
  mode.

## 1. Who defines it, and when

The compiler, and only the compiler. GCC's preprocessor manual states
the whole contract in one sentence:

> GCC defines this macro if and only if the `-ansi` switch, or a `-std`
> switch specifying strict conformance to some version of ISO C or ISO
> C++, was specified when GCC was invoked. It is defined to `1`. This
> macro exists primarily to direct GNU libc's header files to use only
> definitions found in standard C.

Measured with `-dM -E` on an empty translation unit:

| invocation | `__STRICT_ANSI__` |
|---|---|
| `g++` (default, gnu++20 on this compiler) | no |
| `g++ -std=gnu++98`, `gnu++17`, `gnu++23` | no |
| `g++ -ansi` (equals `-std=c++98`) | yes |
| `g++ -std=c++98`, `c++17`, `c++23` | yes |
| `gcc` (default), `-std=gnu89`, `gnu11` | no |
| `gcc -ansi` (equals `-std=c90`), `-std=c89`, `c99`, `c11`, `c23` | yes |

The rule is exactly the presence or absence of the `gnu` prefix on the
dialect. Clang follows the same convention. `-pedantic` and
`-pedantic-errors` do not define it: they change diagnostics, not the
language or the macro set.

## 2. What the compiler itself changes in strict mode

Diffing the complete predefined-macro set of `gnu++17` against
`c++17` gives four lines. The same diff for C gives three.

- `__STRICT_ANSI__` appears.
- The unprefixed system macros `unix` and `linux` disappear. Only the
  reserved spellings `__unix__`, `__linux__` remain, since a conforming
  program may use `linux` as an identifier.
- (C++ only) `__GLIBCXX_TYPE_INT_N_0` and its bit size disappear: the
  compiler stops advertising `__int128` as an extended integer type.
  libstdc++ then reaches for `__int128` under `__STRICT_ANSI__`
  directly so that `is_integral` still says yes; its own comment in
  `<type_traits>` explains that conditionalising on the macro alone
  would break ports that use such a type for `size_t`.

At the language level the GCC manual is explicit: a base standard
"turns off certain features of GCC that are incompatible" with it,
"such as the `asm` and `typeof` keywords, but not other GNU extensions
that do not have a meaning" in the standard. Probed:

| source | `gnu89` | `c89` | `gnu11` | `c11` | `c23` |
|---|---|---|---|---|---|
| `typeof(int) a; asm("");` | ok | 3 errors | ok | 4 errors | 1 error |

The single `c23` error is `asm`: C23 made `typeof` a keyword, so it
came back. The double-underscore forms `__typeof__`, `__asm__`,
`__inline__` work in every mode; that is what they are for. The same
`typeof` line fails under `-std=c++17` and passes under `gnu++17`.

That is the whole of the compiler's side. It is small. The macro's
weight is on the other side of the `#include`.

## 3. What glibc does with it

`features.h` is the first header every glibc header includes, and its
job is to turn a set of *input* macros, defined by the user or the
compiler, into a set of *output* macros `__USE_*` that every other
header branches on. The outputs are `#undef`'d at the top of the file
on every inclusion; nothing outside `features.h` may define them,
which is the mistake the companion note on `struct tm` is about.

`__STRICT_ANSI__` has been an input since 1997 (commit 5107cf1d7d,
the oldest form of the file in the history). Then as now it has one
job: to switch off the *default* feature set. From the 1997 file:

    /* If nothing (other than _GNU_SOURCE) is defined,
       define _BSD_SOURCE and _SVID_SOURCE.  */
    #if (!defined __STRICT_ANSI__ && !defined _ISOC9X_SOURCE && \
         !defined _POSIX_SOURCE && !defined _POSIX_C_SOURCE && \
         !defined _XOPEN_SOURCE && !defined _XOPEN_SOURCE_EXTENDED && \
         !defined _BSD_SOURCE && !defined _SVID_SOURCE)
    # define _BSD_SOURCE 1
    # define _SVID_SOURCE 1
    #endif

and the 2.44 file, after `_DEFAULT_SOURCE` replaced the BSD/SVID pair
(introduced in commit c688b41960, 2013-12-19; the pair removed in
c941736c92 and `__USE_BSD`/`__USE_SVID` folded into `__USE_MISC` in
498afc54df, both 2014-02, all released in glibc 2.20):

    /* If nothing (other than _GNU_SOURCE and _DEFAULT_SOURCE) is defined,
       define _DEFAULT_SOURCE.  */
    #if (defined _DEFAULT_SOURCE                                 \
         || (!defined __STRICT_ANSI__                            \
             && !defined _ISOC99_SOURCE && !defined _ISOC11_SOURCE \
             && !defined _ISOC23_SOURCE && !defined _ISOC2Y_SOURCE \
             && !defined _POSIX_SOURCE && !defined _POSIX_C_SOURCE \
             && !defined _XOPEN_SOURCE))
    # undef  _DEFAULT_SOURCE
    # define _DEFAULT_SOURCE 1
    #endif

Read it as a policy. If you asked for nothing, glibc assumes you want
the traditional Unix programming environment: ISO C, POSIX, X/Open,
and the BSD and System V extensions that every real program uses. If
you asked for *anything* specific, by naming a standard with
`_POSIX_C_SOURCE` or `_XOPEN_SOURCE`, or by telling the compiler you
want strict ISO C, glibc takes you at your word and gives you exactly
that and nothing more. `__STRICT_ANSI__` is the compiler's way of
asking for something specific on your behalf.

A second rule, a little further down, adds POSIX to the default:

    #if ((!defined __STRICT_ANSI__                                    \
          || (defined _XOPEN_SOURCE && (_XOPEN_SOURCE - 0) >= 500))   \
         && !defined _POSIX_SOURCE && !defined _POSIX_C_SOURCE)
    # define _POSIX_SOURCE 1
    ...

So in strict mode with no other request, `_POSIX_SOURCE` is not
defined either. The result is the smallest environment glibc offers:
the ISO C library of the version the compiler reports, and only that.

### What that hides

A three-line probe, compiled with `-fsyntax-only`:

    char *d(const char *s)  { return strdup(s); }      /* POSIX 2008 */
    long  g(struct tm *t)   { return t->tm_gmtoff; }   /* BSD, in glibc since 1996 */
    int   p(FILE *f)        { return fileno(f); }      /* POSIX.1 */

| C invocation | result |
|---|---|
| `gcc -std=gnu11` | clean |
| `gcc -std=c11` | `strdup` undeclared, `tm_gmtoff` no such member ("did you mean `__tm_gmtoff`?"), `fileno` undeclared |
| `gcc -std=c11 -D_POSIX_C_SOURCE=200809L` | only `tm_gmtoff` fails: POSIX back, BSD still hidden |
| `gcc -std=c11 -D_DEFAULT_SOURCE` | clean |
| `gcc -std=c11 -D_GNU_SOURCE` | clean |

Three things to notice. First, the extension members of `struct tm`
are not removed in strict mode, they are *renamed* into the reserved
namespace (`__tm_gmtoff`, `__tm_zone`), because the layout must not
change with the dialect: a `struct tm` filled by `localtime` in one
translation unit is read in another. Second, the explicit feature
macros are additive and independent of the dialect: `-std=c11` with
`_DEFAULT_SOURCE` is the same environment as `-std=gnu11`. Third, the
dialect version is a separate input: `features.h` reads
`__STDC_VERSION__` and `__cplusplus` directly to enable the ISO C99,
C11, C23 and C2Y parts of the library, so `-std=c23` in strict mode
still gets everything C23 defines. Strict means "the standard you
named, entire", not "C89".

### Other glibc lines that read it

Few, and all of the same shape, a strict-mode narrowing:

- `bits/time.h`: `CLK_TCK` is offered unless strict mode without POSIX.
- `sys/cdefs.h`: the `_Static_assert` emulation is used for old
  compilers or strict pre-C11 modes.
- `wchar.h`: selects the ISO-conforming `wcstold` redirect in strict
  mode on long-double-compat targets.

## 4. The C++ twist: `g++` asks for everything anyway

On GNU/Linux the `g++` driver adds `-D_GNU_SOURCE` to every C++
compilation, in every dialect. It is in the driver's language spec,
not in any header, and `g++ -v` shows it on the `cc1plus` command
line. libstdc++ needs the full library to implement the C++ standard
library (`<thread>`, `<cmath>`, wide streams), and it must get it
whatever `-std` the user picked.

In `features.h`, `_GNU_SOURCE` turns on every other input
unconditionally, `_DEFAULT_SOURCE` included, and that rule sits
*above* the `__STRICT_ANSI__` rule, which only fires when nothing else
is defined. The compiler's request for strictness is therefore
overruled by the driver's request for everything, before any header
has read either. Measured with the same probe:

| C++ invocation | `__STRICT_ANSI__` | `_GNU_SOURCE` | result |
|---|---|---|---|
| `g++ -std=gnu++17` | no | yes | clean |
| `g++ -std=c++17` | yes | yes | clean |
| `g++ -std=c++17 -U_GNU_SOURCE` | yes | no | `strdup`, `tm_gmtoff`, `fileno` all fail |
| `g++ -std=gnu++17 -U_GNU_SOURCE` | no | no | clean (the default rule fires) |

The `-U` works because the driver's `-D` precedes the user's options,
and it is the one-line way to see what a C++ compiler that does *not*
predefine `_GNU_SOURCE` (an EDG front end, for one) would see from
glibc in strict mode. Clang on Linux predefines it too.

So for C++ under GCC or Clang on GNU/Linux, `__STRICT_ANSI__` changes
the compiler's keywords and the two unprefixed macros, and changes
nothing at all about which library declarations are visible. The
library side of the macro is a C-compiler phenomenon, and among C++
compilers it belonged to the ones that are gone.

## 5. Other readers of the macro

- GCC's own `<limits.h>`: `LONG_LONG_MAX` and friends are offered under
  `__USE_GNU` when glibc is present, else when not strict.
- GCC's own `<stdarg.h>`: `va_copy` is hidden in strict pre-C99 and
  pre-C++11 modes, since those standards do not define it; `__va_copy`
  is always there.
- libstdc++: about a dozen sites, each withholding a GNU extension in
  strict mode, such as `<thread>`'s convenience overload for
  pointer-to-member callables taking a `stop_token`, `<numbers>`'
  `float128` constants, `<complex.h>` not including the C library's
  header of the same name. And one warning in `c++config.h` for the
  case where someone `#undef`'d the macro by hand, which it calls
  unsupported.

## 6. Summary

- Defined by the compiler if and only if a non-`gnu` dialect was
  requested. Not by the library, never by a header.
- To the compiler it means: no `asm`, `typeof`, `inline`-as-extension
  keywords, no `unix`/`linux` macros, no advertised `__int128`.
- To glibc it means: do not assume the default environment; give the
  named ISO standard and nothing else. Extensions vanish or retreat to
  reserved names; explicit `_*_SOURCE` macros bring them back
  piecemeal, `_DEFAULT_SOURCE` or `_GNU_SOURCE` bring them all back.
- Under `g++` and `clang++` on GNU/Linux the library half is moot:
  the driver defines `_GNU_SOURCE`, and `_GNU_SOURCE` wins.
- Code that keys the *spelling of a glibc member* on
  `__STRICT_ANSI__` is therefore reasoning about a C compiler, or a
  C++ compiler that no longer exists, and the right question was
  never "is the mode strict" but "is the member there", which a
  configuration test answers without naming either macro.
