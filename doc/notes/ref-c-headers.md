# The C library headers

## 0. tl;dr

The standard of 1998 and 2003 asks for two sets of headers for the C
library. `<cstdlib>` declares its names in namespace `std` and nowhere
else, and `<stdlib.h>` is `<cstdlib>` followed by a using-declaration
for each name. A C++ library that does not own the C library can
build that only by declaring the C library a second time. Almost none
did, and in 2011 the standard changed its rule to what they had built
instead.

`include/ansi` holds this library's answers to that requirement: 51
files, three for most headers, and two models selected by two macros.

- **Pure.** The library's own C90 declarations inside `std`; the C
  library's header is never read. This is 2003 to the letter.
- **Wrapper.** The C library's header included at global scope, then
  a using-declaration for each name inside `std`.

Every build takes the wrapper model. The pure model is selected by a
macro. It was the model under three compilers of 2012; today it
compiles header by header and has six defects. A third model, which
no configuration compiled, left the tree in November 2026 (3.4).

Four consequences follow from the wrapper model, and a reader should
know them before reading anything else:

- After `<cstdlib>` a name such as `atoi` is declared twice over, in
  `std` and in the global namespace. 2003 has it in `std` only;
  C++11 permits the global declaration as well.
- After `<cstdlib>` the C library's own additions are declared too,
  `random` for one. No edition permits that, and it is not the
  wrapper's doing: glibc shows them because GCC defines `_GNU_SOURCE`
  for every C++ translation unit.
- The overloads C++ adds to the C library, `abs (long)` and the
  `float` math functions among them, are defined in `std`, and
  `<stdlib.h>` and `<math.h>` name them in the global namespace, as
  every edition of the standard requires. So `abs (-5000000000L)`
  after `<stdlib.h>` is 5000000000, and `sqrt (2)` after `<math.h>`
  is ambiguous, as `std::sqrt (2)` is after `<cmath>`.
- A wrapper cannot include the C library's header by its name,
  because the name finds the wrapper. The configuration records each
  header's absolute path, and the wrappers include that.

How the two models compare depends on the compiler. Under GCC
glibc declares most of what C++ adds, and the models differ in the
global namespace and in the pure model's defects (chapter 6). Under
a strict compiler, EDG's front end with `-A`, glibc withholds those
additions and the wide functions of Amendment 1 with them. The
wrapper model cannot name what the C header does not declare; the
pure model declares it itself (7.2).

Chapter 1 is the standard and how its rule moved. Chapters 2 to 5 are
the files and the two models. Chapter 6 measures what a program
gets, chapter 7 what the pure model can do today, and chapter 9
points at the queue.

Line numbers are as of November 2026 and will drift; the names will not.
The measurements are a 15D configuration under GCC 16.2.1 with glibc
2.44 on aarch64.

## 1. The standard

### 1.1 1998 and 2003: the names are in `std`

14882:2003 names eighteen headers of the form `<cname>` (17.4.1.2
p3, Table 12) and says where their names live:

> the contents of each header cname shall be the same as that of the
> corresponding header name.h, as specified in ISO/IEC 9899:1990
> Programming Languages C (Clause 7), or ISO/IEC:1990 Programming
> Languages—C AMENDMENT 1: C Integrity, (Clause 7), as appropriate,
> as if by inclusion. In the C++ Standard Library, however, the
> declarations and definitions (except for names which are defined as
> macros in C) are within namespace scope (3.3.5) of the namespace
> std. (17.4.1.2 p4)

The `.h` forms are in Annex D, as a deprecated feature, and are
defined in terms of the new ones:

> Every C header, each of which has a name of the form name.h,
> behaves as if each name placed in the Standard library namespace by
> the corresponding cname header is also placed within the namespace
> scope of the namespace std and is followed by an explicit
> using-declaration (7.3.3). (D.5 p2)

A footnote states the intent: "The ".h" headers dump all their names
into the global namespace, whereas the newer forms keep their names
in namespace std" (footnote 160).

"In `std`" means in `std` and not in the global namespace as well.
The text does not say "only", but the rest of the standard does the
work. A program that calls `atoi` unqualified after `<cstdlib>` alone
names something no declaration in scope provides, which 3.4.1 p1
makes ill-formed ("If no declaration is found, the program is
ill-formed"), and an ill-formed program is owed a diagnostic (1.4
p2). An implementation that also declares `::atoi` compiles it in
silence.

A well-formed program can see the difference too:

```cpp
#include <cstdlib>

namespace A { char abs (...); }
using namespace A;

int main () { return sizeof (abs (1)); }
```

Where `abs` is declared in `std` alone, the call finds `A::abs` and
`main` returns 1. Where `::abs (int)` is declared as well, it is the
better match, and `main` returns `sizeof (int)`.

The C library meant here is C90's with Amendment 1 (1.2 p2): C90's
fifteen headers and the amendment's `<iso646.h>`, `<wchar.h>` and
`<wctype.h>`. It has no `long long`, no `lldiv` and no `sinf`; those
are C99's.

### 1.2 What C++ changes in the C headers

The contents are C's "as if by inclusion", with changes the clauses
list. A wrapper has to supply each of them:

- **Macros stay macros** (17.4.1.2 p5): `assert`, `errno`,
  `offsetof`, `setjmp`, `va_arg`, `va_end` and `va_start`.
- **Functions are functions** (17.4.1.2 p6). Footnote 159: "This
  disallows the practice, allowed in C, of providing a "masking
  macro" in addition to the function prototype."
- **Const-correct pairs** (21.4 p4 to p23). `strchr`, `strpbrk`,
  `strrchr`, `strstr` and `memchr`, and their wide counterparts, each
  become two declarations, one taking and returning a pointer to
  const and one a pointer to non-const.
- **Linkage twins** (25.4 p3, p4). `bsearch` and `qsort` each become
  two declarations, `extern "C"` and `extern "C++"`, which differ in
  the linkage of the comparison function.
- **Integer overloads** (26.5 p3, p4): `long abs (long)` and `ldiv_t
  div (long, long)` in `<cstdlib>`.
- **Floating overloads** (26.5 p5, p6): `float` and `long double`
  versions of the functions of `<cmath>`, and `pow` with an `int`
  exponent.

### 1.3 Three ways to build it

A C++ library that ships apart from the C library has three ways to
put the C library's names in `std`. Each has a cost, and this tree
has paid all three:

- **Declare everything again**, inside `std`. The declarations must
  match the C library's types and layouts, which the C++ library can
  only measure or assume: `_csetjmp.h` 36-43 hardcodes the size of
  `jmp_buf` for Linux, and `_cstdlib.h` gives `div_t` "the
  traditional layout". Nothing the C library declares beyond the
  standard exists for the program, POSIX included.
- **Include the C header inside `namespace std`.** This works for
  plain `extern "C"` declarations and for nothing else: an inline
  function, a C++ overload or a nested system header inside the
  namespace changes meaning.
- **Include the C header as it is and name its contents in `std`.**
  This always works, and leaves every name in the global namespace
  too, against 17.4.1.2 p4.

The third costs a program little, since the standard reserves those
global names in any case: "Each name from the Standard C library
declared with external linkage is reserved to the implementation for
use as a name with extern "C" linkage, both in namespace std and in
the global namespace" (17.4.3.1.3 p4). A program cannot declare one
of them for itself under either model; the difference shows only in
lookup, as in the program of 1.1.

### 1.4 2011: the rule follows the practice

LWG issue 456, "Traditional C header files are overspecified", was
resolved for C++11. The 2011 text (N3242) keeps the sentence that the
names are in `std` and adds, in 17.6.1.2 p4:

> It is unspecified whether these names are first declared within the
> global namespace scope and are then injected into namespace std by
> explicit using-declarations (7.3.3).

D.5 p2 is rewritten the other way round, the names "placed within the
global namespace scope", with the mirror sentence, and D.5 p3 says
what a program can rely on:

> The header `<cstdlib>` assuredly provides its declarations and
> definitions within the namespace std. It may also provide these
> names within the global namespace. The header `<stdlib.h>`
> assuredly provides the same declarations and definitions within the
> global namespace, much as in the C Standard. It may also provide
> these names within the namespace std.

The same edition moves to C99 (1.2) and adds `long long abs (long
long)` and `lldiv_t div (long long, long long)` to `<cstdlib>` (26.8
p7).

### 1.5 2017 and 2023

C++17 (N4659) names the overloads. 20.5.1.2 p4 now reads "these
names (including any overloads added in Clauses 21 through 33 and
Annex D)"; LWG issue 2380, "May `<cstdlib>` provide `long
::abs(long)` and `long long ::abs(long long)`?", is resolved in the
same edition. D.5 p3 excepts a few things that exist in `std` alone:
`std::byte` and its operators, and the mathematical special
functions.

C++23 (N4950) takes the `.h` headers out of Annex D. They are 17.14
now, not deprecated, with a stated purpose: "The intended use of
these headers is for interoperability only" (17.14.1 p1). The rule
for their contents, 17.14.7 p1, is C++17's with a longer list of
exceptions.

### 1.6 The rule, edition by edition

| edition | `<cstdlib>` | `<stdlib.h>` |
|---|---|---|
| 1998, 2003 | in `std` only | `<cstdlib>`, then a using-declaration for each name |
| 2011, 2014 | in `std`; may also be global | the same declarations, global; may also be in `std` |
| 2017, 2020 | the same, overloads named | the same; Annex D |
| 2023 | the same | the same; 17.14, for interoperability |

One thing holds in every row: `<stdlib.h>` provides in the global
namespace what `<cstdlib>` provides in `std`, the overloads included.
In 2003 that is D.5 p2's using-declaration, which names every
function called `abs` in `std`.

## 2. The files

### 2.1 Three files a header

`include/ansi` has 51 files:

- **eighteen `<cname>` headers**, `cassert` to `cwctype`, the
  standard's Table 12;
- **seventeen `<name.h>` wrappers**, one for each but `<iso646.h>`,
  which the compiler supplies;
- **sixteen pure headers**, `_cassert.h` to `_cwctype.h`, one for
  each but `<ciso646>` and `<climits>`.

So `<cstdlib>` is three files: `cstdlib`, `stdlib.h` and
`_cstdlib.h`. A program includes the first two by their standard
names; the third is included by the first, as `<ansi/_cstdlib.h>`.

### 2.2 How the compiler finds them

The compiler's configuration file puts the directory first on the
search path (`etc/config/gcc.config` 80, `CPPFLAGS =
-I$(TOPDIR)/include/ansi`), and `etc/config/makefile.common` 43
appends `-I$(TOPDIR)/include -I$(BUILDDIR)/include`. `-nostdinc++`
(`gcc.config` 75) takes the compiler's own C++ library off the path.
Thus:

- `<cstdlib>` can only be `include/ansi/cstdlib`;
- `<stdlib.h>` finds `include/ansi/stdlib.h` ahead of
  `/usr/include/stdlib.h`;
- `<ansi/_cstdlib.h>` resolves through `-I$(TOPDIR)/include`.

A program built against the library needs the same two directories
in the same order. With `include` alone, `<string>` and `<iostream>`
compile, `<cstdlib>` is not found, and `<complex>` fails on its own
`<cmath>`.

The configuration itself must see the C library and not the
wrappers, so `etc/config/GNUmakefile.cfg` 63 filters the directory
out of `CPPFLAGS` for the characterizations.

### 2.3 How a wrapper reaches the C library's header

`include/ansi/stdlib.h` cannot say `#include <stdlib.h>`: the name
finds the file itself. C++98 has no way to say "the next one".

The configuration solves it. `etc/config/src/libc_headers.sh`, the
first characterization to run, preprocesses `#include <stdlib.h>`
with every `-I` removed, reads the path from the line markers of the
output and emits it:

```cpp
#define _RWSTD_ANSI_C_STDLIB_H "/usr/include/stdlib.h"
#define _RWSTD_ANSI_C_STDDEF_H "/usr/lib/gcc/aarch64-unknown-linux-gnu/16/include/stddef.h"
```

The wrappers then say `#include _RWSTD_ANSI_C_STDLIB_H`. There are
eighteen such macros in `config.h`, one for each header, and a
`config.h` therefore names the compiler installation it was made
with.

The paths are quoted strings for a reason. In `#include MACRO` the
tokens are macro-expanded, and `errno`, `assert` and `setjmp` are
macros, so `<errno.h>` would expand in the middle (STDCXX-71,
5baf1b17). The contents of a string literal are not expanded.

Where the compiler implements a header internally and no path
appears, the script falls back on `<name.h>`.

### 2.4 The C library's headers come back through the wrappers

The C library's headers include one another by name, and those names
find the wrappers too. The include tree of `<cstdlib>` shows it:

```
. include/ansi/cstdlib
.. include/rw/_defs.h
.. /usr/include/stdlib.h
... /usr/include/bits/libc-header-start.h
... include/ansi/stddef.h
.... GCC/stddef.h
... /usr/include/sys/types.h
.... include/ansi/stddef.h
..... GCC/stddef.h
```

`GCC/` stands for the compiler's own include directory. glibc's
`<stdlib.h>` asks for `<stddef.h>`, gets the library's wrapper, and
the wrapper passes it on to the compiler's header.
`<sys/types.h>` does the same a few lines later, and gets through
again.

That second passage is why the live arm of a `.h` wrapper has no
include guard. glibc defines a macro such as `__need_size_t` and
includes `<stddef.h>` for one typedef, several times over, and a
guard in the wrapper would cut every inclusion after the first. The
wrapper is transparent: `#include <rw/_defs.h>`, then the C header.

Two wrappers do more. `<stdlib.h>` and `<math.h>` go on to include
`<cstdlib>` and `<cmath>` and name the overloads of 26.5 in the
global namespace (3.3). The C header still comes first, so an
inclusion by the C library gets the C header before anything of
C++. What follows needs no guard: the `<cname>` header has its own,
and a using-declaration may be repeated at namespace scope. It
relies on one fact of glibc: no header reached from the two includes
`<stdlib.h>` or `<math.h>` by name, and `g++ -H` shows each wrapper
once, at depth 1. A nested passage would reach the using-declarations
before `std` has the names.

## 3. The two models

### 3.1 Two switches

`include/rw/_config.h` 97-106 sets them, the same under every
compiler since 2c76243c:

```cpp
#if !defined (_RWSTD_USE_PURE_C_HEADERS)
     // pure C headers disabled except when requested
#  define _RWSTD_NO_PURE_C_HEADERS
#endif   // !_RWSTD_USE_PURE_C_HEADERS

#ifdef _RWSTD_NO_PURE_C_HEADERS
     // deprecated C headers (the <xxx.h> kind) are fully implemented
     // in terms of the pure C++ C library headers (the <cxxx> kind)
#  define _RWSTD_NO_DEPRECATED_C_HEADERS
#endif   // _RWSTD_NO_PURE_C_HEADERS
```

"Deprecated" is Annex D's word for the `.h` headers. The second
macro says that the library does not implement them itself, and
hands `<stdlib.h>` to the C library.

A `<cname>` header tests the first macro and has two bodies
(`cstdlib` 32, 34). A `.h` wrapper tests the second and has two.

| the program defines | `NO_PURE` | `NO_DEPRECATED` | `<cstdlib>` is | `<stdlib.h>` is |
|---|---|---|---|---|
| `_RWSTD_USE_PURE_C_HEADERS` | no | no | pure | `<cstdlib>` + using |
| nothing | yes | yes | wrapper | the C library's; for `<stdlib.h>` and `<math.h>`, then `<cname>` + using for the overloads |

The second macro is defined wherever the first is, so the two
switches move together.

### 3.2 Pure

```
<stdlib.h>   include/ansi/stdlib.h
               #include <cstdlib>
               using std::abs;  using std::atoi;  ...
<cstdlib>    include/ansi/cstdlib
               #include <ansi/_cstdlib.h>
_cstdlib.h   namespace std { extern "C" { int abs (int); ... } }
```

`_cstdlib.h` declares the C90 functions itself, inside `std`, with
types from the configuration (`_RWSTD_SIZE_T` for `size_t`). The C
library's header is not read at all. The `.h` wrapper adds one
using-declaration a name: 31 in `stdlib.h`, 43 in `stdio.h`.

This is 17.4.1.2 p4 and D.5 p2 as written. Chapter 7 says what it
can do today.

### 3.3 Wrapper

```
<stdlib.h>   include/ansi/stdlib.h
               #include "/usr/include/stdlib.h"
               #include <cstdlib>
               using std::abs;  using std::div;
<cstdlib>    include/ansi/cstdlib
               #include "/usr/include/stdlib.h"
               extern "C++" {
               namespace std {
                   inline long abs (long);      // where the C library has none
                   using ::abs;  using ::atoi;  ...
               }
               }
```

This is the model of every build. The C library's header is included
at global scope, and `<cname>` then does three things:

- undefines the masking macros of 1.2, by name;
- defines in `std` the overloads C++ adds, where a characterization
  found that the C library has none;
- names each standard function and type in `std` with a
  using-declaration.

Fifteen `.h` wrappers are the C library's header and nothing more.
`<stdlib.h>` and `<math.h>` are the C library's header followed by
their `<cname>` header and a using-declaration in the global
namespace for each function the `<cname>` header defines in `std`
itself: in `<stdlib.h>` `abs` and `div`, and `bsearch` and `qsort`
where they have linkage twins; in `<math.h>` `abs` and the 22
functions of `<cmath>`. The overloads are thus in both namespaces,
as D.5 p2 has them; this is the third consequence of chapter 0.

`<cstdlib>` and `<cmath>` hold their part of `std` in an
`extern "C++"` block. A program may include `<stdlib.h>` inside an
`extern "C"` block of its own. Without the `extern "C++"` block the
definition of `std::abs (long)` would then have C linkage: a second
C function named `abs`, which GCC, Clang and EDG reject. With it,
such a program compiles with the `.h` forms and with `<cstdlib>` and
`<cmath>` alike.

`<cwchar>` put this model in words when it
adopted it throughout (STDCXX-129, a6ca23c8): all declarations go
"into the global scope first, before injecting each into namespace
std via a using declaration".

### 3.4 Who took which

The tree held three models until November 2026, and they were not
three experiments. At the tip of 4.2.x (bcdd75c7, 2012) each
compiler's configuration header chose one:

- **wrapper:** GCC, HP aC++, Intel C++, MIPSpro, Visual C++ and XL
  C++, each of which defined both macros;
- **pure:** EDG's `eccp`, whose header undefined both, and Sun C++
  and Compaq C++, for which nothing in the tree turned the pure
  model off;
- **nested:** none.

The platform retirement of 2026 removed those headers, and 2c76243c
put the two macros in `_config.h` for every compiler. GCC, Clang and
`eccp` all take the wrapper model since.

The nested model was a third body in each `<cname>` header, which
included the C library's header with `namespace std` open.
`_RWSTD_NAMESPACE_STD_OPEN` kept a nested inclusion (2.4) from
opening the namespace a second time: a header wrote its number in the
macro and closed the namespace only if the number was its own.
Fourteen headers had the body under the macro, and `cmath` had it
without. No configuration compiled it, and under GCC none had since
the 2005 import, so it left the tree in November 2026.

## 4. The wrapper model, header by header

| header | names in `std` | what the wrapper adds |
|---|---|---|
| `<cassert>` | none; `assert` is a macro | nothing: the C header, included again each time |
| `<cctype>` | 13 functions | undefines their masking macros |
| `<cerrno>` | `errno`, if it is not a macro | see 5.2 |
| `<cfloat>` | none; all macros | nothing |
| `<ciso646>` | none | nothing: the compiler's header, no models |
| `<climits>` | none; all macros | see 5.1 |
| `<clocale>` | `lconv`, `localeconv`, `setlocale` | nothing |
| `<cmath>` | 22 functions | the `float` and `long double` overloads, `abs` for the three floating types, `pow` with an `int` exponent, in an `extern "C++"` block, and `<math.h>` names them in the global namespace; undefines masking macros |
| `<csetjmp>` | `jmp_buf`, `longjmp` | defines `setjmp` as a macro if the C library made it a function (STDCXX-670) |
| `<csignal>` | `sig_atomic_t`, `raise`, `signal` | nothing |
| `<cstdarg>` | `va_list` | nothing |
| `<cstddef>` | `size_t`, `ptrdiff_t` | nothing |
| `<cstdio>` | `FILE`, `fpos_t`, `size_t`, 40 functions | undefines 42 names a C library may mask with macros (STDCXX-195); defines `stdin`, `stdout` and `stderr` as macros if they are not |
| `<cstdlib>` | `size_t`, `div_t`, `ldiv_t`, 28 functions | `abs (long)`, `abs (long long)`, `div (long, long)`; the `extern "C++"` twins of `bsearch` and `qsort`; all in an `extern "C++"` block, and `<stdlib.h>` names them in the global namespace |
| `<cstring>` | `size_t`, 22 functions | undefines masking macros |
| `<ctime>` | `clock_t`, `time_t`, `size_t`, `tm`, 9 functions | nothing |
| `<cwchar>` | `mbstate_t`, `wint_t`, `size_t`, `tm`, the wide functions | see 5.3 |
| `<cwctype>` | `wint_t`, `wctype_t`, `wctrans_t`, 17 functions | `WEOF` if missing |

The additions are gated by characterizations, each of which asks
whether the C library's header already has the declaration:

- **`ABS_OVERLOADS`, `DIV_OVERLOADS`** yield `_RWSTD_NO_ABS_LONG`,
  `_RWSTD_NO_ABS_LLONG` and `_RWSTD_NO_DIV_LONG`. Under glibc all
  three are defined, so `<cstdlib>` defines the three functions.
  `abs (long long)` is not 2003's; it follows the compiler's type
  set (`ref-conformance.md`, "Compiler types beyond 2003").
- **`MATH_OVERLOADS`** yields one macro an overload,
  `_RWSTD_NO_SIN_FLT` and its like.
- **`C99_MATH`** says whether `sinf` and `sinl` are declared. Where
  they are, the overload calls them; where they are not, it calls
  the `double` function (92969c1e).
- **`EXTERN_C_OVERLOAD`** says whether two functions may differ in
  the language linkage of a parameter's function type alone. GCC
  says no, so under GCC `bsearch` and `qsort` have no `extern "C++"`
  twin, and need none: GCC passes either kind of function.

The const-correct pairs of 21.4 are not the wrapper's work under
glibc. glibc's `<string.h>` declares them itself for a C++ compiler,
and `using ::strchr` takes both.

## 5. The headers that do not fit

### 5.1 `<climits>` and `<limits.h>`

GCC's own `<limits.h>` reaches the C library's with `#include_next`.
Included by its absolute path it has no next directory to search, and
gcc 4.3 made that an error: "no include path in which to search for
limits.h" (STDCXX-768). So under GCC these two wrappers use
`#include_next <limits.h>` themselves and not the path macro
(`climits` 105, `limits.h` 103).

`limits.h` then includes it a second time with `_GCC_NEXT_LIMITS_H`
defined (107-108). The first inclusion gives the compiler's limits;
the second makes GCC's header chain to the system's, for the POSIX
macros (STDCXX-988, f6a07654).

Both end by setting `MB_LEN_MAX` to the value the configuration
measured, whatever the headers said (`climits` 111, `limits.h` 114).

In the pure model these two define the C90 macros from the
configuration's `_RWSTD_CHAR_BIT` and its like; there is no
`_climits.h`.

### 5.2 `<cerrno>`

`errno` must be a macro (17.4.1.2 p5, and LWG issue 310), so:

- the C header is included only while `errno`, `EDOM`, `ERANGE` or
  `EAGAIN` is still undefined (`cerrno` 37), which also serves as the
  include guard;
- if `errno` is not a macro after that, `cerrno` names it in `std`
  and then defines `errno` as `errno` (45-48).

### 5.3 `<cwchar>`

Three things are particular to it:

- glibc declares the const-correct pairs of the wide functions only
  when `__CORRECT_ISO_CPP_WCHAR_H_PROTO` is defined, and defines it
  for GCC alone. The wrappers define it first, where the
  `CORRECT_ISO_CPP_WCHAR_H_PROTO` characterization found that the
  compiler accepts the declarations (`cwchar` 38-42, e4707fd8).
  `analysis-glibc-wchar-proto.md` has the story.
- `mbstate_t` comes through `rw/_mbstate.h`, which the library's own
  headers use to name the type without including `<wchar.h>`
  (`analysis-glibc-mbstate_t.md`).
- `WEOF`, `WCHAR_MIN` and `WCHAR_MAX` are defined from the
  configuration where the C header leaves them out.

### 5.4 `<cassert>` and `<ciso646>`

Neither has names to place. `<cassert>` includes the C header with no
guard, since `<assert.h>` is meant to be included again after
`NDEBUG` changes. `<ciso646>` is the compiler's header in every
model.

## 6. What a program gets

Each row is a program compiled against the library's headers. The
second column is what 14882:2003 requires.

| program | 2003 | wrapper | pure |
|---|---|---|---|
| `std::atoi ("1")` after `<cstdlib>` | compiles | compiles | compiles |
| `::atoi ("1")` after `<cstdlib>` | error: 17.4.1.2 p4 | compiles | error |
| `random ()` after `<cstdlib>` | error: C90 has no such function | compiles | error |
| `sizeof (std::abs (0L))` after `<cstdlib>` | 8: 26.5 p4 | 8 | 4 |
| `std::div (7L, 2L)` as an `ldiv_t` | compiles: 26.5 p4 | compiles | error |
| `sizeof (abs (0L))` after `<stdlib.h>` | 8: D.5 p2 | 8 | 4 |
| `abs (-5000000000L)` after `<stdlib.h>` | 5000000000 | 5000000000 | 705032704 |
| `sizeof (std::sin (0.0f))` after `<cmath>` | 4: 26.5 p6 | 4 | 8 |
| `sizeof (sin (0.0f))` after `<math.h>` | 4: D.5 p2 | 4 | 8 |
| `char* p = std::strchr (cs, 'x')`, `cs` a `const char*` | error: 21.4 p4 | error | error |
| the program of 1.1 returns | 1 | 4 | 1 |
| a global `int random;` after `<cstdlib>` | compiles: 1.4 p2 | error | compiles |

The wrapper model departs from 2003 in rows 2, 3, 11 and 12:

- **Rows 2 and 11** are the model itself, the global declaration
  beside the one in `std`. C++11 permits it (1.4).
- **Rows 3 and 12** are the C library's additions, and no edition
  permits them: `random` is not a name the standard reserves, so a
  program may use it, and row 12 is a well-formed program the
  implementation rejects. The cause is outside the wrapper. GCC
  defines `_GNU_SOURCE` for every C++ translation unit, and glibc
  then declares everything it has. With `-U_GNU_SOURCE -std=c++98`
  row 12 compiles.

Rows 6, 7 and 9 hold because `<stdlib.h>` and `<math.h>` name the
overloads of 26.5 in the global namespace (3.3). So an unqualified
call with an integer argument meets the overload sets of 2003, which
have no overload for most integer types. `sqrt (2)` after
`<math.h>` is ambiguous among the three floating functions, as
`std::sqrt (2)` is after `<cmath>`, and `abs (u)` for an
`unsigned u` after `<stdlib.h>` is ambiguous among the three integer
ones. A program writes the conversion: `sqrt (2.0)`. C++11 adds the
overloads that make `sqrt (2)` a call of the `double` function
(N3242 26.8 p11).

The pure model was written to conform, and it does in rows 2, 3, 11
and 12. It departs in rows 4 to 9, for the reasons of chapter 7.

## 7. The pure model today

### 7.1 Under GCC

Each of the 35 headers compiles alone with
`-D_RWSTD_USE_PURE_C_HEADERS`, and without threads so do the
library's C++ headers, `<string>`, `<iostream>`, `<locale>`,
`<fstream>` and `<complex>` among them. Six things stop a program
from going further:

- **Threads.** A thread-safe build includes `<pthread.h>`
  (`rw/_mutex-pthread.h` 44), which includes `<time.h>` for
  `clockid_t` and gets the library's pure `<ctime>`, which is C90's
  and has none. `<string>` then fails with four errors, all inside
  `/usr/include/pthread.h`. This is the first cost of 1.3 at work:
  the system's own headers need more of the C library than the
  standard names. The repair is one include: with glibc's
  `<bits/types/clockid_t.h>` ahead of `<pthread.h>`, the way
  `rw/_mbstate.h` fetches `__mbstate_t`, the library's C++ headers
  compile and a program with `<string>` and `<iostream>` links and
  runs.
- **`va_list`.** `_cstdarg.h` 37 declares it as the configuration's
  `_RWSTD_VA_LIST`, a layout-compatible structure, inside `std`, and
  defines `va_start` and `va_arg` by pointer arithmetic that names
  `va_list` unqualified (42-47). A function that calls `va_start`
  does not compile, and on an ABI that passes arguments in registers
  the arithmetic would be wrong if it did. `_cstdio.h` 141-143
  declares the `v` functions with the same type, against the
  compiler's built-in declarations.
- **`abs` and `div` for `long`.** `_cstdlib.h` 132 and 138 define
  them inside the block for the linkage twins, under
  `_RWSTD_NO_EXTERN_C_OVERLOAD` (107-143), where 50b33c68 put them.
  GCC defines that macro, so under GCC the pure `<cstdlib>` has
  neither.
- **The floating overloads.** `_cmath.h` declares the 22 `double`
  functions and nothing else. The overloads of 26.5 p5 and p6 exist
  only in the wrapper body of `cmath`.
- **`jmp_buf`.** `_csetjmp.h` 36-43 declares it as 25 `long` on a
  64-bit Linux, 200 bytes, which is glibc's size on x86-64. On
  aarch64 glibc's is 312, so `setjmp` would write past the object.
- **`qsort`.** `_cstdlib.h` 127 qualifies its call with `_RWSTD_C`,
  while `bsearch` nine lines above does not.

The library's own sources are safe from the model. They include the
`.h` forms, which hand them the C library's full header, POSIX and
all, and `src/file.cpp` 31-38 defines both switches before its first
include so that it gets that header whatever the build asked for.

### 7.2 Under a strict compiler

EDG's front end with `-A` is not GCC to glibc and defines no
`_GNU_SOURCE`. glibc then shows it C90 and withholds two things 2003
requires:

- the wide I/O functions of Amendment 1, `swprintf` and nine more,
  which it declares under `__USE_ISOC95`;
- the const-correct pairs of 21.4, which `<string.h>` declares only
  where `__CORRECT_ISO_CPP_STRING_H_PROTO` is defined, for GCC 4.4
  and Clang 3.5 or later (`/usr/include/string.h` 40-43).

The wrapper model can only name what the C header declares. The pure
model declares all of it itself. Each row below is a requirement of
2003 and whether the model meets it, measured with an 11S
configuration under EDG 7.0 and the same glibc:

| requirement | wrapper | pure |
|---|---|---|
| `<cwchar>` compiles | no: 10 errors | yes |
| `std::swprintf` is declared | no | yes |
| `std::wcschr` is the const-correct pair (21.4 p14) | no | yes |
| `std::strchr` is the const-correct pair (21.4 p4) | no | yes |
| `::atoi` is not declared after `<cstdlib>` (17.4.1.2 p4) | no | yes |
| the program of 1.1 returns 1 | no | yes |
| `random` is not declared after `<cstdlib>` | yes | yes |
| `std::abs (long)` and `std::div (long, long)` (26.5 p4) | yes | yes |
| `abs (long)` after `<stdlib.h>` (D.5 p2) | yes | yes |
| `std::sin (float)` (26.5 p6) | yes | no |
| `sin (float)` after `<math.h>` (D.5 p2) | yes | no |
| `va_start`, then `std::vprintf` | yes | no |
| `sizeof (std::jmp_buf)` is glibc's | yes | no |

The wrapper meets `sin (float)` after `<math.h>` with
`_RWSTD_NO_C99_MATH`, the answer `C99_MATH` gives under `eccp`. All
35 headers compile alone in the pure model there, and `<string>`,
`<iostream>`, `<locale>`, `<fstream>`, `<complex>` and `<valarray>`
with them. EDG overloads on language linkage, so the `abs` and `div`
defect of 7.1 does not arise. The four rows the pure model fails are
three defects of 7.1: the floating overloads, `va_list` and
`jmp_buf`. A thread-safe configuration was not tried.

The library's sources gain nothing from the pure model under either
compiler: they include the C library's headers, and what glibc
withholds there (`snprintf`, `strtof`) is characterized and replaced
one function at a time.

## 8. What the library's own headers include

Almost nothing, by design. A C++ header such as `<string>` must not
bring the names of `<string.h>` into a program that did not ask for
them, so the library's headers reach the C library in three other
ways:

- **Types from the configuration.** `_RWSTD_SIZE_T` and
  `_RWSTD_PTRDIFF_T` are spelled in `config.h`, so `<string>` needs
  no `<stddef.h>`.
- **Built-ins.** Under GCC `rw/_traits.h` 86-92 maps
  `_RWSTD_MEMCPY` and its siblings to `__builtin_memcpy`.
- **Eight includes in all**, through the macros of `rw/_defs.h`
  44-71, `_RWSTD_CMATH` for `<cmath>` and so on: `<cmath>` three
  times, `<ctime>` and `<cwchar>` twice each, `<cstring>` once.

Where a library header does call a C function by name it writes
`_RWSTD_C::sqrt`, 79 times in all, most of them in `<complex>` and
`<valarray>`. `_RWSTD_C` is `std` (`rw/_defs.h` 478).

## 9. Where to look

| path | what |
|---|---|
| `include/ansi/c*` | the eighteen `<cname>` headers, two bodies each but `<ciso646>`, which has one |
| `include/ansi/*.h` without the underscore | the seventeen `.h` wrappers |
| `include/ansi/_c*.h` | the sixteen pure headers |
| `include/rw/_config.h` 97-106 | the two switches |
| `include/rw/_defs.h` 44-71, 478 | the include macros and `_RWSTD_C` |
| `etc/config/src/libc_headers.sh` | the path macros |
| `etc/config/src/{ABS,DIV,MATH}_OVERLOADS.cpp`, `C99_MATH.cpp`, `EXTERN_C_OVERLOAD.cpp` | the gates of chapter 4 |
| `etc/config/gcc.config` 75, 80 | the search path |
| `tests/strings/21.cctype.cpp`, `21.cwchar.cpp`, `21.cwctype.cpp`, `tests/numerics/26.c.math.cpp`, `26.c.math.depr.cpp`, `tests/algorithms/25.libc.cpp`, `tests/diagnostics/19.cerrno.cpp`, `tests/support/18.csetjmp.cpp` | the tests, all run in the wrapper model |

`TODO` carries what this note found open:

- "The pure C headers" is chapter 7: the model is repaired, or it
  retires.
- "A program needs two include directories, and the original README
  names one" is 2.2.

The pure model is the decision the rest hangs on:

- If it retires, the D.5 arm of every `.h` wrapper goes with it, and
  each thing a strict compiler's C library withholds is declared in
  a wrapper behind a characterization of its own, as 7.2 lists.
- If it stays, it takes the repairs of 7.1 and is the model for a
  strict compiler, as it was in 2012 (3.4).
