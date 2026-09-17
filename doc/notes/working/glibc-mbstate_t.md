# The `__mbstate_t` clash: how a 2006 header protocol met glibc 2.16

*Apache stdcxx revival notes, September 2026.*

The first thing that fails when the Apache C++ Standard Library is
built on a current Linux toolchain is a conflicting declaration of
`__mbstate_t`. This note traces the failure to its root, which is not
the compiler and not the language dialect but a change of meaning in a
single glibc macro, made in 2012, five months before the last commit
in the stdcxx tree. It then records the fix, made in the library's own
idiom, and how it was verified against the glibc headers of each era.

## 1. The symptom

Toolchain: GCC 16.2, glibc 2.44, the library configured at the
compiler's default dialect (C++20) with `BUILDTYPE=15D` (debug,
shared, threads). The configuration phase succeeds and characterizes
the compiler correctly on its own. The library build then fails at ten
sites with one message:

```
include/rw/_mbstate.h:146:3: error: conflicting declaration
    'typedef struct __mbstate_t __mbstate_t'
/usr/include/bits/types/__mbstate_t.h:21:3: note: previous declaration
    as 'typedef struct __mbstate_t __mbstate_t'
```

Both include orders fail. When a translation unit includes `<stdio.h>`
first, glibc's definition comes first and stdcxx's is the conflicting
one. When it includes `<iosfwd>` or `<string>` first, the roles swap.
The struct members are identical in both; the two anonymous structs
are nonetheless distinct types, and a typedef name cannot denote both.

## 2. What stdcxx was doing, and why

`include/rw/_mbstate.h` exists to give `<iosfwd>`, `<codecvt>` and
`<locale>` the conversion-state type through a single macro,
`_RWSTD_MBSTATE_T`, without pulling `<wchar.h>` into every
translation unit that names `streampos`. It dates from January 2006
and has one branch per platform:

| platform | `_RWSTD_MBSTATE_T` | how obtained |
|---|---|---|
| AIX | `char*` | typedef in `__rw` |
| HP-UX | `mbstate_t` | the struct replicated, or the system's |
| IRIX | `char` | |
| MSVC | `int` | |
| Solaris | `__mbstate_t` | the struct replicated under Sun's guard |
| Darwin | `__mbstate_t` | `#include <machine/_types.h>` |
| Linux/glibc | `__mbstate_t` | the struct replicated under glibc's guard |
| other | `std::mbstate_t` | `#include <cwchar>` |

The Linux branch replicated glibc's private struct and placed it under
glibc's own guard macro:

```c
#ifndef __mbstate_t_defined
#  define __mbstate_t_defined 1
extern "C" {
typedef struct {
    int __count;
    union {
        _RWSTD_WINT_T __wch;
        char          __wchb [4];
    } __value;
} __mbstate_t;
}
#endif
```

That is a protocol with glibc: whichever side is seen first defines
the struct and sets the guard, and the other side skips. It was
correct for every glibc that existed when it was written. The design
was deliberate, and the Darwin branch beside it shows the alternative
the library used where the platform offered one: include the platform's
small internal header instead of copying the struct.

One detail explains why the Linux branch copies rather than includes.
stdcxx ships its own `<wchar.h>` wrapper in `include/ansi`, and the
build puts that directory first on the include path. From inside the
library, `#include <wchar.h>` resolves to the wrapper, not to glibc.
The real system header is reachable only by the absolute path the
configuration records in `config.h` as `_RWSTD_ANSI_C_WCHAR_H`.

## 3. How glibc guards `mbstate_t`

### 3.1 The 1999 split

glibc has two names for the conversion state. `__mbstate_t` is the
private representation, an anonymous struct typedef under a reserved
identifier. `mbstate_t` is the public alias, `typedef __mbstate_t
mbstate_t`, introduced by `<wchar.h>` and later also by `<uchar.h>`.

The split dates from Ulrich Drepper's wide-stream work of June 1999
(commit `d64b6ad075`). It exists because `<stdio.h>` needs the
representation and must not expose the name. C requires `fgetpos` and
`fsetpos` to save and restore the conversion state together with the
byte offset (ISO C11 7.21.2p6), so `fpos_t` embeds the state:

```c
typedef struct _G_fpos_t {
    __off_t     __pos;
    __mbstate_t __state;
} __fpos_t;
```

In a conforming ISO C mode `<stdio.h>` alone must leave the ordinary
identifier `mbstate_t` free for the program. The reserved name
`__mbstate_t` carries the dependency without introducing the public
one. From this follow two independent facts a header may need to
know: *has the representation been defined*, and *has the public name
been introduced*. The first can be true while the second is false;
that is the intended state after `<stdio.h>`.

### 3.2 Selective inclusion, 1999 to 2.25

`<stdio.h>` obtained the representation by asking `<wchar.h>` for it
alone:

```c
#define __need_mbstate_t
#include <wchar.h>
```

`<wchar.h>` answered with a selector at its top:

```c
#ifndef _WCHAR_H
#if !defined __need_mbstate_t && !defined __need_wint_t
# define _WCHAR_H 1
# include <features.h>
#endif
```

A partial request left `_WCHAR_H` unset, defined the struct, consumed
the request macro with `#undef`, and skipped the rest of the file. A
later ordinary inclusion then supplied the public alias and the API.
`_WCHAR_H` was both the header's include guard and the selector for
its full content. Despite its spelling, `__need_mbstate_t` requested
the private type, not the public alias.

Through glibc 2.15 the struct was guarded by `__mbstate_t_defined`,
and the public alias had no guard of its own beyond `_WCHAR_H`:

```c
#ifndef __mbstate_t_defined
# define __mbstate_t_defined 1
typedef struct { ... } __mbstate_t;
#endif
#undef __need_mbstate_t

#ifdef _WCHAR_H
typedef __mbstate_t mbstate_t;
```

This is the state of affairs the library's protocol was written against,
and under it the protocol is sound.

### 3.3 glibc 2.16: the guard is repurposed

C11 added `<uchar.h>`, whose conversion functions `mbrtoc16`,
`c16rtomb`, `mbrtoc32` and `c32rtomb` take `mbstate_t*`. `<uchar.h>`
therefore had to introduce the public alias without exposing the rest
of `<wchar.h>`. Drepper's "Add uchar.h support, part 1" (authored
2011-12-28, committed 2012-01-01, commit `db6af3ebf4`, released in
glibc 2.16 on 2012-06-30) gave the public alias a guard shared by the
two headers, and it reused the existing name for it. The struct's
guard was given a new name with two more underscores:

```c
#if (defined _WCHAR_H || defined __need_mbstate_t) \
    && !defined ____mbstate_t_defined
# define ____mbstate_t_defined 1
typedef struct { ... } __mbstate_t;
#endif
#undef __need_mbstate_t

#ifdef _WCHAR_H
# ifndef __mbstate_t_defined
typedef __mbstate_t mbstate_t;
#  define __mbstate_t_defined 1
# endif
```

| guard | through 2.15 | from 2.16 |
|---|---|---|
| `__mbstate_t_defined` (two underscores) | the private struct | the public alias |
| `____mbstate_t_defined` (four underscores) | unused | the private struct |

The same spelling now records a different fact. The struct's fields
and layout did not change; nothing broke at the ABI. What broke is
any code outside glibc that coordinated with the old spelling.

### 3.4 glibc 2.26: the guards move house

Zack Weinberg's "Remove __need macros from stdio.h and wchar.h"
(authored 2016-11-20, committed 2017-06-08, commit `199fc19d3a`,
released in glibc 2.26 on 2017-08-02) applied to these headers the
single-type-header treatment already used for `<time.h>`. The two
guards kept their 2.16 meanings and moved into two files:

```c
/* bits/types/__mbstate_t.h */
#ifndef ____mbstate_t_defined
#define ____mbstate_t_defined 1
typedef struct { ... } __mbstate_t;
#endif

/* bits/types/mbstate_t.h */
#ifndef __mbstate_t_defined
#define __mbstate_t_defined 1
#include <bits/types/__mbstate_t.h>
typedef __mbstate_t mbstate_t;
#endif
```

`<stdio.h>` now reaches the struct through `bits/types/__fpos_t.h`;
`<wchar.h>` and `<uchar.h>` include `bits/types/mbstate_t.h`. The
public guard encloses both the include of the private header and the
typedef: it guards the fact "the public type is defined", and
predefining it suppresses the private include along with the alias.
`<wchar.h>` acquired an ordinary include guard, and `__need_mbstate_t`
is no longer honored anywhere: defining it and including `<wchar.h>`
yields the whole header.

### 3.5 Chronology

| date | glibc | change |
|---|---|---|
| 1999-06-16 | 2.1.x | private `__mbstate_t` and public alias split; `__need_mbstate_t` selective inclusion (`d64b6ad075`) |
| 2012-01-01 | 2.16 | public alias guarded by the repurposed `__mbstate_t_defined`; struct guarded by `____mbstate_t_defined` (`db6af3ebf4`) |
| 2017-03-16 | 2.26 | obsolete C++ namespace wrappers removed from the headers (`2072f5c34e`) |
| 2017-06-08 | 2.26 | guards moved into `bits/types/{__mbstate_t,mbstate_t}.h`; need-macros dropped (`199fc19d3a`) |
| 2018-01 | 2.27 | `fpos_t` moved into `bits/types/__fpos_t.h`, keeping the private dependency |

## 4. Why the protocol fails from 2.16 on

Both orders, traced against the 2.16 and later guards:

- **stdcxx first.** `_mbstate.h` defines the struct and sets
  `__mbstate_t_defined`. glibc later checks `____mbstate_t_defined`,
  finds it unset, and defines the struct again: the conflicting
  declaration. Then `<wchar.h>` checks `__mbstate_t_defined`, finds it
  set, and skips the public alias. The program has no `mbstate_t`.
- **glibc first.** `<stdio.h>` defines the struct and sets
  `____mbstate_t_defined`. `_mbstate.h` checks `__mbstate_t_defined`,
  finds it unset, and defines the struct again.

No adjustment of the guard alone repairs this. Switching stdcxx to the
four-underscore guard breaks every glibc through 2.15. Setting both
guards suppresses the public alias that `<wchar.h>` must provide.
Testing the glibc version number would work, but it is a pin on
version knowledge rather than a characterization of the platform,
which is not how this library handles a platform that has moved.

The compiler is not implicated. GCC's default C++ dialect did move
during the same years (gnu++98 through GCC 5, gnu++14 from GCC 6 in
2016, gnu++17 from GCC 11, gnu++20 by GCC 16), and stdcxx at the
default dialect does trip over several C++98-isms elsewhere, all of
them handled by the configuration correctly. This failure reproduces
in C and at any dialect. It belongs to the C library.

## 5. The fix, in the library's idiom

stdcxx detects a platform feature with a *characterization*: a small
source under `etc/config/src/` compiled during configuration, whose
failure yields an `_RWSTD_NO_<NAME>` macro in `config.h`. Headers then
branch on the macro. The configuration is demand-driven, so a new
characterization needs no wiring beyond the file.

The new characterization, `BITS_TYPES___MBSTATE_T_H.cpp`, is
compile-only and asks the one question the Linux branch needs:

```c
#include <bits/types/__mbstate_t.h>
__mbstate_t state;
```

The Linux branch of `_mbstate.h` becomes a decision branch on its
answer:

```c
#ifndef _RWSTD_NO_BITS_TYPES___MBSTATE_T_H
   // glibc 2.26 and beyond: the private type has a header of its own
#  include <bits/types/__mbstate_t.h>
#else
   // earlier glibc: ask <wchar.h> for just the private type, the way
   // <stdio.h> does for fpos_t
#  include <features.h>
#  define __need_mbstate_t
#  include _RWSTD_ANSI_C_WCHAR_H
#endif
#define _RWSTD_MBSTATE_T __mbstate_t
```

The default path is the Darwin precedent applied to glibc: include the
platform's designated single-type header. It is minimal, and it is
immune to any change in the struct's layout, which a copied struct is
not.

The fallback path replicates nothing. It makes exactly the request
libio's `_G_config.h` made for `fpos_t` from 1999 to 2.25, so it
behaves identically on every glibc of that span with no knowledge of
which guard means what. Two details matter:

- `<features.h>` comes first. The partial path of `<wchar.h>` does not
  include it, and from 2.15 on that path evaluates `__GNUC_PREREQ`
  when it has to obtain `wint_t`. glibc's own callers never noticed
  because `<stdio.h>` had included `<features.h>` already; stdcxx's
  header can be the first glibc-touching line of a translation unit.
- Only `__need_mbstate_t` is requested. libio requested `__need_wint_t`
  as well, but only when building glibc itself or under libstdc++'s
  `_GLIBCPP_USE_WCHAR_T`; an ordinary program never did. The struct
  takes its member type from the compiler's `__WINT_TYPE__` when that
  is defined, and `<features.h>` makes the other case safe.

On a libc that honors neither mechanism, the fallback serves the whole
`<wchar.h>`. That is what the generic branch does on every other
platform: correct, merely not minimal, and never a conflicting
declaration. The failure mode of an unknown future glibc is therefore
a slower compile, not a broken one.

The name of the characterization keeps the header's spelling, three
underscores and all, because `BITS_TYPES_MBSTATE_T_H` would read as
the *other* header.

## 6. Verification

**Default path, on the current toolchain.** A fresh configuration
answers "ok" for the characterization; the library builds with zero
errors and `libstd15D.so.4.2.2` links. Two programs with opposite
include orders (`<cstdio>` before `<cwchar>` and `<ios>`, and the
reverse with `<wchar.h>` added) compile with `-nostdinc++`, link
against the library, and run, using `std::mbstate_t`, `::mbstate_t`,
`std::streampos` and `std::mbsinit`.

**Fallback path, against the headers of each epoch.** No glibc older
than 2.44 is installed anywhere convenient, so the fallback was
exercised the way glibc's own maintainers test header coordination:
the real `wchar.h` and `features.h` of each release tag, taken from a
glibc clone, with every unrelated header they include stubbed empty,
preprocessed by GCC 16. Four translation units cover the orders the
library produces:

| case | sequence |
|---|---|
| A | the fallback alone |
| B | the fallback, then a full `<wchar.h>` |
| C | a full `<wchar.h>`, then the fallback |
| D | a `<stdio.h>`-style partial request, then the fallback, then a full `<wchar.h>` |

The check counts definitions in the preprocessed output: the private
struct must appear exactly once in every case, the public alias
exactly once in B, C and D and not at all in A, and after A neither
`_WCHAR_H` nor `__need_mbstate_t` may be left defined.

| glibc | A | B | C | D |
|---|---|---|---|---|
| 2.3.6 (2005) | 1 / 0 | 1 / 1 | 1 / 1 | 1 / 1 |
| 2.15 (2012) | 1 / 0 | 1 / 1 | 1 / 1 | 1 / 1 |
| 2.17 (2012) | 1 / 0 | 1 / 1 | 1 / 1 | 1 / 1 |
| 2.25 (2017) | 1 / 0 | 1 / 1 | 1 / 1 | 1 / 1 |

(private / public definitions.) Two variants were run to confirm the
details in section 5: requesting `__need_wint_t` without `<features.h>`
fails on 2.15, 2.17 and 2.25 with `missing binary operator before
token '('` at the `__GNUC_PREREQ` line, and requesting only
`__need_mbstate_t` without `<features.h>` passes everywhere under GCC
because `__WINT_TYPE__` is predefined.

**Degradation.** Forcing the fallback on glibc 2.44 by predefining the
`_RWSTD_NO_` macro compiles, links and runs the same smoke program,
with the whole `<wchar.h>` served, as predicted.

The epoch matrix verifies conditional declaration behavior, not a full
historical build or a target ABI. It is the check that settles the
question the fix asks, and no more.

## 7. What this one teaches

The library's protocol was correct within its epoch and fails completely
outside it. It is representative of a kind of solution this library
uses in places: coordination with a platform's internals through the
platform's own private macros. Such a solution has no expiry date
written on it, and when the platform repurposes a name the failure
arrives without warning, years later, in a tree nobody is building.

The durable form is the one the library already had for Darwin:
obtain the thing from the platform's designated mechanism, and let a
characterization discover which mechanism this platform offers. When
the mechanism moves again, the characterization says "no" and the
fallback degrades to the generic case rather than to a conflicting
declaration.

## Sources

- glibc commits `d64b6ad075` (1999-06-16), `db6af3ebf4` (2012-01-01),
  `2072f5c34e` (2017-03-16), `199fc19d3a` (2017-06-08), in the glibc
  git repository at sourceware.org.
- Ulrich Drepper, "Add uchar.h support, part 1", glibc-cvs notification,
  2012-01-03: https://sourceware.org/pipermail/glibc-cvs/2012q1/051306.html
- Zack Weinberg, "The bits/types/*.h treatment for stdio and wchar",
  libc-alpha, 2016-11-21: https://sourceware.org/pipermail/libc-alpha/2016-November/076366.html
- Joseph Myers, "Remove C++ namespace handling from glibc headers",
  libc-alpha, 2017-03-10: https://sourceware.org/pipermail/libc-alpha/2017-March/079184.html
- Gert Ohme, "glibc-2.1.x current CVS; proposed patches", libc-alpha,
  1999-07-31: https://sourceware.org/pipermail/libc-alpha/1999-July/001261.html
- ISO/IEC 9899:2011 (N1570), 7.1.3, 7.21.2, 7.28, 7.29.1:
  https://www.open-std.org/jtc1/sc22/wg14/www/docs/n1570.pdf
- stdcxx commits `75dd7664`, `e01920a8`, `bb381444` (January 2006), the
  history of `include/rw/_mbstate.h`.
