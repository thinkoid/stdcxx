# The wide string functions and the C++ prototypes glibc withholds from Clang

As of 2026-09-28 (5569cc60).

`21.cwchar` passes under GCC and fails five assertions under Clang.
Same glibc, all three Clang configurations. glibc declares the C++
overloads of five wide functions for GCC and not for Clang:
`wcschr`, `wcspbrk`, `wcsrchr`, `wcsstr` and `wmemchr`. The narrow
twins in `<string.h>` get the overloads under both compilers.

This note measures the failure and shows why the configuration could
not see it. It traces how glibc's two headers came apart, what is
pending upstream, and what the other C++ libraries do. It weighs
three repairs and records the one chosen. It lists the defects found
in the dormant arms on the way. It assumes
`history-libc-declarations.md`, which explains `_RWSTD_NO_<F>` and
`_RWSTD_NO_<F>_IN_LIBC`.

## 0. The short version

- **The failure.** The C prototype of `wcschr` takes a `const`
  pointer and returns a modifiable one. C++ replaces it with a pair
  of overloads that keeps the constness. Under Clang, glibc declares
  only the C prototype. `std::wcschr` on a `const wchar_t*` returns
  `wchar_t*`.
- **The configuration could not see it.** The probe calls
  `wcschr ((wchar_t*)0, 0)`, which both shapes accept, by design.
  The GCC and Clang `config.h` agree on all five.
- **glibc's guard is a handshake, not a compiler test.** In 2009
  glibc and libstdc++ agreed on two macros. glibc defines
  `__CORRECT_ISO_CPP_STRING_H_PROTO` and
  `__CORRECT_ISO_CPP_WCHAR_H_PROTO` when it declares the overloads,
  and libstdc++ then stops adding its own. The guard
  `__GNUC_PREREQ (4, 4)` stood for "a libstdc++ that knows the
  handshake".
- **`<string.h>` was widened three times; `<wchar.h>` never was.**
  2013 opened the string guards to every C++ compiler. 2014 closed
  them again for want of asm labels. 2019 reopened `<string.h>` for
  Clang 3.5 and later. Each change touched the string headers only.
  The record shows an omission, not a decision. Nothing is pending
  upstream for `<wchar.h>`.
- **libc++ opts in from outside.** Its `<wchar.h>` defines
  `__CORRECT_ISO_CPP_WCHAR_H_PROTO` before it includes glibc's.
  libstdc++ adds the non-const overload only, a half repair.
- **The library's answer.** `using ::f;` for all five, as for the
  narrow five, and the macro defined before glibc's `<wchar.h>` is
  included. No probe, no overloads of the library's own. Overloads of
  the library's own in `std` make unqualified calls ambiguous,
  measured.
- **The library itself is unaffected.** It calls none of the five
  through `std`. The defect is in what users see.
- **On the way.** Nine defects in the dormant arms of `<cwchar>` and
  `<wchar.h>`. glibc reaches none of them. The rip-out of the
  function probes deletes them all.

## 1. The wide C API the library uses

The library touches `<wchar.h>` in two ways.

**As an interface.** `<cwchar>` re-exports the header into `std`: 57
functions by the `wchar` list in `etc/config/src/headers.inc`, plus
types and macros. The library's `<wchar.h>` wraps glibc's and
includes it by full path. Every function goes through one of the
primer's three arms.

**As a client.** The compiled library calls fewer:

| use | functions | where |
|---|---|---|
| character traits | `wmemcpy`, `wmemcmp`, `wmemmove`, `wmemset`, `wcslen`, `wmemchr` | `include/rw/_traits.h`; the `__rw_` versions in debug builds |
| conversion | `mbrtowc`, `wcrtomb`, `mbrlen`, `wcsrtombs`, `mbsinit`, and the restartless `mbtowc`, `wctomb`, `mblen`, `mbstowcs`, `wcstombs` | `src/wcodecvt.cpp`, `src/codecvt.cpp`, `src/punct.cpp`, `src/collate.cpp` |
| single characters | `btowc`, `wctob`, `wctomb` | `src/wctype.cpp` |
| collation | `wcscoll`, `wcsxfrm` | `src/collate.cpp` |
| time | `wcsftime` | `src/time_put.cpp` |

Of the five, only `wmemchr` is on this list.
`char_traits<wchar_t>::find` calls it so:

```cpp
return _RWSTD_STATIC_CAST (const char_type*,
           _RWSTD_WMEMCHR (_RWSTD_CONST_CAST (char_type*, __s),
                           __c, __n));
```

The comment above it reads "const cast in case of a const-incorrect
wmemchr()". The argument is non-const, so either shape accepts it.
The result is cast to const either way. The library was written to
survive the C shape. Its users are not.

## 2. The failure

The pinned Clang tables have `21.cwchar` at 61 assertions, 5 failed,
5 warnings. The GCC tables have 66, 0, 0. From the 15D-clang run:

```
std::wmemchr(/* const overload */) expected return type const wchar_t*, got wchar_t*
std::wmemchr(/* non-const overload */) not declared (_RWSTD_NO_WMEMCHR = 0, _RWSTD_NO_WMEMCHR_IN_LIBC = 0)
```

The same pair follows for `wcspbrk`, `wcsrchr`, `wcsstr` and
`wcschr`.

The test puts decoy templates in `std`:
`template <class T> T* wcschr (T*, T)` and
`template <class T> const T* wcschr (const T*, T)`. The decoys count
their calls. A call that lands on a decoy means the library declared
nothing better. The test then compares each call's return type with
the standard's.

- **The assertion.** `std::wcschr (L"", L'\0')`. glibc's C prototype
  is the only non-template candidate, and it wins. It returns
  `wchar_t*`; the standard requires `const wchar_t*`.
- **The warning.** `std::wcschr (wstr, L'\0')`, `wstr` a `wchar_t*`.
  The C prototype needs a qualification conversion, `wchar_t*` to
  `const wchar_t*`. The decoy `T* wcschr (T*, T)` binds by identity.
  Identity ranks better, so the decoy wins. The test concludes the
  non-const overload was never declared. The call never reached
  libc.

Both disappear once `std` holds the pair.

## 3. glibc's two shapes, and the handshake behind them

glibc 2.44, `<wchar.h>`, near line 68, and one of the five:

```c
/* Tell the caller that we provide correct C++ prototypes.  */
#if defined __cplusplus && __GNUC_PREREQ (4, 4)
# define __CORRECT_ISO_CPP_WCHAR_H_PROTO
#endif
...
#ifdef __CORRECT_ISO_CPP_WCHAR_H_PROTO
extern "C++" wchar_t *wcschr (wchar_t *__wcs, wchar_t __wc)
     __THROW __asm ("wcschr") __attribute_pure__;
extern "C++" const wchar_t *wcschr (const wchar_t *__wcs, wchar_t __wc)
     __THROW __asm ("wcschr") __attribute_pure__;
#else
extern wchar_t *wcschr (const wchar_t *__wcs, wchar_t __wc)
     __THROW __attribute_pure__;
#endif
```

The C++ pair is two `extern "C++"` declarations. An `__asm` label
binds both to the one C symbol. There is no wrapper and no cost at
run time. A C header cannot declare both shapes: the C prototype and
the const overload have the same parameters and differ only in their
return type.

The guard tests GCC's version. Clang defines `__GNUC__` 4 and
`__GNUC_MINOR__` 2, so it fails. Measured: a program reports whether
a call's result points to const, for a const and a non-const
argument.

| compiler | `wcschr (const wchar_t*)` | `wcschr (wchar_t*)` | `__CORRECT_ISO_CPP_WCHAR_H_PROTO` |
|---|---|---|---|
| GCC 16.2.1 | `const wchar_t*` | `wchar_t*` | defined |
| Clang 22.1.8 | `wchar_t*` | `wchar_t*` | undefined |

The pair came in January 2009 (glibc d8387c7b7b, glibc 2.10), with a
libstdc++ half posted to gcc-patches as "Fix std::strchr etc.
prototypes for C++". The post says why it took both libraries. It
"can't be fixed just in libstdc++, unless it implements the whole
string.h and wchar.h on its own". It "can't be fixed solely in glibc
either", because the inlines `<cstring>` and `<cwchar>` add "then
clash with glibc prototypes".

libstdc++ reads the macro. Its `<cwchar>`, GCC 16, line 214:

```cpp
#ifndef __CORRECT_ISO_CPP_WCHAR_H_PROTO
  inline wchar_t*
  wcschr(wchar_t* __p, wchar_t __c)
  { return wcschr(const_cast<const wchar_t*>(__p), __c); }
  ...
#endif
```

GCC 4.4 was the first release whose libstdc++ read the macro. So the
guard meant "the C++ library will not clash". It says nothing about
what the compiler can do.

Two details matter later. The guard only ever defines the macro; it
never undefines it. The declarations test `#ifdef`. A macro defined
before the header is included therefore selects the pair as well.

## 4. How the guards moved

Four commits changed the guards after 2009.

| year | glibc commit | headers | guard after |
|---|---|---|---|
| 2009 | d8387c7b7b | `string.h`, `wchar.h` | `__GNUC_PREREQ (4, 4)` |
| 2013 | 3f637079f5 | `string.h`, `strings.h` | `__cplusplus >= 199711L \|\| __GNUC_PREREQ (4, 4)` |
| 2014 | 8e2e833ac4, BZ #17631 | `string.h`, `strings.h` | `__GNUC_PREREQ (4, 4)` |
| 2019 | 953ceff17a, BZ #25232 | `string.h` | `__GNUC_PREREQ (4, 4) \|\| __glibc_clang_prereq (3, 5)` |

2013. The change came from users of Clang. The message: Clang "will
be unable to correctly diagnose a number of subtle bugs that will be
errors in GCC compilations". After review it opened the guard to
"all C++ compilers that claim to fully support C++98".

2014. The change was reverted. The message, in full: "The
implementation of `__CORRECT_ISO_CPP_STRING_H_PROTO` requires support
for asm aliases." Not every compiler that claims C++98 has `__asm`
labels.

2019. `<string.h>` alone was reopened, for Clang 3.5 and later. That
answers the 2014 objection: Clang supports asm labels. The message:

> Without the asm redirects, strchr et al. are not const-correct.
>
> libc++ has a wrapper header that works with and without
> `__CORRECT_ISO_CPP_STRING_H_PROTO` (using a Clang extension). But
> when Clang is used with libstdc++ or just C headers, the
> overloaded functions with the correct types are not declared.

It shipped in glibc 2.31.

`__glibc_clang_prereq` comes from `<features.h>` (cab4d74b01, 2016).
It is the twin of `__GNUC_PREREQ` for Clang. It exists because Clang
fixes `__GNUC__` and `__GNUC_MINOR__` at 4 and 2. Clang 22 still
does. Every `__GNUC_PREREQ (4, 4)` guard is false under Clang,
whatever Clang can do.

`<strings.h>` lost the widening in 2014 and never regained it. Its
guard covers `index` and `rindex`.

## 5. Why `<wchar.h>` stayed behind

Neither commit log nor source gives a reason. The evidence points to
an omission:

- The 2013 change was about diagnostics in string code. It touched
  the string headers and nothing else.
- The 2014 revert undid the 2013 change. It had nothing to undo in
  `<wchar.h>`.
- The 2019 change answered a report about `strchr`. Its message
  describes the wide case exactly: Clang, libstdc++ or plain C
  headers, overloads not declared.
- The mechanism is identical: `extern "C++"` declarations with
  `__asm` labels. The 2014 objection does not apply to Clang 3.5
  and later.
- Nothing in `<wchar.h>` behaves differently under Clang. Measured
  in chapter 10: Clang 22 with the macro defined compiles, resolves
  and links the pair in four dialects, with and without
  `_FORTIFY_SOURCE=3`.

What would contradict this: a reviewer excluding `<wchar.h>` on
purpose. That would show in the discussion of the 2013 patch ("as per
discussion on earlier versions of this patch") or of BZ #25232. Both
live on sourceware.org, behind a bot wall the session's tools could
not pass. A browser can.

## 6. What is pending upstream

Nothing for `<wchar.h>`. glibc mainline at cbf59b8366 (2026-09-28)
has the 2009 guard.

The nearest work is an open series on libc-alpha from November 2025.
It raises glibc's minimum compilers. Patch 16/35, "Assume that Clang
is at least Clang 4", turns `__glibc_clang_prereq (3, 5)` in
`<string.h>` into `defined(__clang__)`. Patch 17/35, "Assume that
GCC is at least GCC 5", reduces the `<string.h>` guard to
`defined __cplusplus`. That is the 2013 position again. Neither patch
touches `<wchar.h>`. Mainline does not carry the series.

If the series lands, `<wchar.h>` becomes the only string header whose
guard names a GCC version.

## 7. Why the configuration missed it

`headers.inc` spells the five as calls: `wcschr((wchar_t*)0,0)`,
`wcspbrk((wchar_t*)0,(wchar_t*)0)`, `wmemchr((wchar_t*)0,0,0)`, and
so on. The probe includes `<wchar.h>`, makes the call, and links. A
`wchar_t*` argument suits the C prototype and the non-const overload
alike. That was the intent. The forms date from 2006 (ed68a657),
chosen so that C++ overloads would not make the probe ambiguous.

The probe asks "is `wcschr` callable". Under both compilers it is.
All six standing configurations write

```
// #define _RWSTD_NO_WCSCHR
```

and the same for the other four. Nothing downstream can tell GCC's
glibc from Clang's.

Setting the macro would not have helped either. It sends `<cwchar>`
into its "in libc" arm, which serves a libc that exports the symbol
and declares nothing:

```cpp
#elif !defined (_RWSTD_NO_WCSCHR_IN_LIBC)
}   // namespace std

extern "C" wchar_t* wcschr (wchar_t*, wchar_t);

inline const wchar_t* wcschr (const wchar_t *__s, wchar_t __c)
{
    return wcschr (_RWSTD_CONST_CAST (wchar_t*, __s), __c);
}

namespace std {
using ::wcschr;
```

With glibc's C prototype in scope, both declarations are errors. The
first redeclares a C function with another type. The second has
glibc's parameters and another return type. Measured with Clang, the
tree's headers and the 15D-clang `config.h`, `-D_RWSTD_NO_WCSCHR`
forced:

```
include/ansi/cwchar:563:21: error: conflicting types for 'wcschr'
include/ansi/cwchar:565:23: error: functions that differ only in their return type cannot be overloaded
```

The global namespace cannot hold a pair of the library's own while
glibc's C prototype is there.

## 8. What the other C++ libraries do

**libstdc++** adds the non-const overload in `std` when the macro is
absent (chapter 3). The const call still reaches the C prototype.
Under Clang with libstdc++, `std::wcschr` on a `const wchar_t*`
returns `wchar_t*`. That is half a repair.

**libc++** answers the two headers differently. Its `<string.h>`
reads `__CORRECT_ISO_CPP_STRING_H_PROTO` and copes either way. When
the macro is absent it adds the missing overloads next to the C
prototypes, with a Clang extension (`_LIBCPP_PREFERRED_OVERLOAD`).
Its `<wchar.h>` also defines the wide macro itself, before it
includes glibc's:

```cpp
// We define this here to support older versions of glibc <wchar.h> that do
// not define this for clang.
#  if defined(__cplusplus) && !defined(__CORRECT_ISO_CPP_WCHAR_H_PROTO)
#    define __CORRECT_ISO_CPP_WCHAR_H_PROTO
#  endif
```

libc++ treats the missing clause as glibc's defect. It opts in from
outside and does not wait for a fix.

The opt-in has one cost. The define must precede the first inclusion
of glibc's `<wchar.h>`. In 2023 a libc++ change let `<iosfwd>` include
glibc's header first (LLVM review D150015). glibc then declared the C
prototype, libc++ declared its const overload, and the two clashed:
"functions that differ only in their return type cannot be
overloaded". The fix moved the define into libc++'s `__mbstate_t.h`,
ahead of every path to glibc's header.

## 9. Three repairs for the library

The first idea was a characterization. It asks each function what a
const call returns, in the shape of `ABS_OVERLOADS.cpp`, and prints
one macro per function:

```cpp
static int returns_const (const wchar_t*) { return 1; }
static int returns_const (wchar_t*)       { return 0; }
...
check (returns_const (wcschr (s, L'\0')), "WCSCHR");
```

Prototyped against glibc 2.44, it reports all five present under GCC
and all five absent under Clang. Under the macro, `<cwchar>` would
define a pair in `std`, forwarding to the global C function.

The second idea dropped the probe. A pair defined unconditionally in
`std` is const-correct against both of glibc's shapes, under both
compilers. It is also a second function with the same parameters as
glibc's pair in the global namespace. An unqualified call under
`using namespace std;` sees both.

Measured: `#include <wchar.h>`, the pair in `std`, `using namespace
std;`, then `wcschr (p, L'b')`.

| compiler | `p` is `wchar_t*` | `p` is `const wchar_t*` |
|---|---|---|
| GCC 16.2.1 | ambiguous | ambiguous |
| Clang 22.1.8 | compiles | ambiguous |

With `using ::wcschr;` in `std`, as today, all four compile. The
suite cannot see the difference: its tests call `std::wcschr`. C++03
17.4.1.2 and 21.4 specify one pair per name, not two.

The three repairs:

- **A.** `using ::f;` for all five, as for the narrow five. Inert on
  every configuration. Alone, it leaves the Clang row of `21.cwchar`
  at five failures until glibc is fixed.
- **B.** The unconditional pair. Fixes the Clang row. Breaks the
  unqualified calls above on the default GCC path.
- **C.** The pair gated on the characterization. Confines the pair
  to Clang. Adds a probe, where the rip-out of the function probes
  removes them. Clang still sees an ambiguity for a `const`
  argument.

Chosen: A, with the macro defined by the library before glibc's
`<wchar.h>` is included. That is libc++'s opt-in, without libc++'s
overloads. `std` then holds glibc's pair through `using ::f;`, under
both compilers. It landed in 1f6e5dd2, with the removal of the
function probes.

## 10. The define in the library

The library reaches glibc's header through three sites:
`include/ansi/cwchar` (two includes) and `include/ansi/wchar.h`, each
`#include _RWSTD_ANSI_C_WCHAR_H`. A fourth, in `include/rw/_mbstate.h`,
is the partial include under `__need_mbstate_t` for glibc before
2.26; it declares no functions. The define goes before the three,
guarded by `#ifndef`.

A late define costs nothing. Suppose glibc's header arrives first,
through a path outside the library's wrappers. glibc then declares
the C prototype, and `using ::f;` brings it into `std`. That is
today's behaviour, not an error. The libc++ failure of chapter 8
needed libc++'s own overloads next to glibc's; the library declares
none.

Measured: a translation unit with and without the define, `using
::f;` for the five in `std`, a type check of `std::wcschr`,
`std::wmemchr` and `std::wcsstr` on `const` and non-`const`
arguments, and four unqualified calls under `using namespace std;`.

| compiler | without the define | with the define |
|---|---|---|
| GCC 16.2.1 | correct types, calls resolve | the same |
| Clang 22.1.8 | `std::wcschr (const wchar_t*)` returns `wchar_t*` | correct types, calls resolve |

Under Clang with the define, the result held in C++98, C++11, C++17
and C++23, each at `-O0` and at `-O2 -D_FORTIFY_SOURCE=3`, with
`-Wall -Wextra` silent. The program links to `wcschr@GLIBC_2.2.5`.
Under GCC, glibc's own `# define` repeats the library's, an identical
redefinition; no diagnostic.

The objection. The define writes a glibc-private, double-underscore
macro. The library's rules say to characterize, never to pin. The
sweep of 01b6f4b9 took the tree off glibc's internals. The case for
the define:

- The sweep removed reads of internal macros whose meaning had
  changed (`__USE_BSD`). This is a write into an `#ifdef` whose
  meaning has not changed since 2009.
- libc++ depends on the same write. glibc cannot repurpose the macro
  without breaking libc++.
- A C library that does not read the macro ignores it. The define
  replaces a probe; it adds none.
- No characterization can repair Clang. The only repairs are
  overloads of the library's own (chapter 9) or glibc's pair.

The define goes redundant when glibc takes the clause. It stays
harmless.

## 11. Found on the way

Nine defects in the dormant arms of `<cwchar>` and `<wchar.h>`, at
5569cc60. glibc reaches none of them. The rip-out of the function
probes deletes every arm they live in.

1. **`<cwchar>`'s own `wcschr` never advances** (`cwchar:580`). The
   "neither" arm, for a libc with no `wcschr` at all:

   ```cpp
   inline wchar_t* wcschr (wchar_t *__s, wchar_t __c)
   {
       do {
           if (*__s == __c)
               return __s;
       } while (*__s);
       return 0;
   }
   ```

   Nothing increments `__s`. Any string whose first character is
   neither `__c` nor NUL loops forever. Check: the body compiled
   standalone, called on `L"ab"` for `L'b'`, timed out at 3 s. For
   `L'a'` it returns.
2. **`<cwchar>`'s "in libc" `wcsrchr` calls itself** (`cwchar:891`).
   The arm declares `extern "C" const wchar_t* wcsrchr (const
   wchar_t*, wchar_t)`. No libc has that prototype; C returns
   `wchar_t*`. It then defines a non-inline `wchar_t* wcsrchr
   (wchar_t*, wchar_t)` in a header. The body, `wcsrchr
   (_RWSTD_CONST_CAST (wchar_t*, __s), __c)`, selects itself by exact
   match. Check: compiled standalone, GCC warns `-Winfinite-recursion`
   and the call dies of SIGSEGV. The sibling arms have the constness
   the other way round; this one looks transposed.
3. **`<wchar.h>`'s `wcsrchr` block undefines `_RWSTD_NO_WCSCHR`**
   (`wchar.h:330`). The block ends with `#undef _RWSTD_NO_WCSCHR` and
   a closing comment naming `WCSCHR`. It is a copy of the block
   above. Check: reading. The macro it should undefine stays set for
   the rest of the translation unit.
4. **`<cwchar>` has no "in libc" arm for `wmemchr`, `wcscpy` and
   `wcsspn`** (`cwchar:1086`, `635`, `947`). `#ifndef
   _RWSTD_NO_<F>` goes straight to `#else`, the library's own
   definition. Every other sibling tests `_IN_LIBC` first. A libc
   that exports the function without declaring it would get a second
   definition. Check: reading. The fallback bodies themselves are
   correct.
5. **`fgetws` declared as returning `wchar_t`** (`cwchar:171`, the
   "in libc" prototype). C returns `wchar_t*`. Check: reading against
   C99 7.24.3.2.
6. **`#undef _RWSTD_MBRTOWC`** (`cwchar:472`). The convention needs
   `_RWSTD_NO_MBRTOWC`. `_RWSTD_MBRTOWC` is defined nowhere. The
   `#endif` comment repeats the misspelling. Check: reading.
7. **The `wcsftime` "in libc" arm has no `#undef`** (`cwchar:698`),
   unlike every sibling. A later reader in the same translation unit
   would decide again. Check: reading.
8. **`mbsrtowcs` declared with a `const wchar_t*` destination**
   (`wchar.h:281`). C has `wchar_t*`. Check: reading.
9. **`wcsrtombs` declared with a `const char**` source**
   (`wchar.h:292`). C has `const wchar_t**`. Check: reading.

Defects 8 and 9 declare `extern "C"` functions, so a wrong type does
not change the symbol. It miscompiles calls. Against a header that
declares the function correctly, it is a redeclaration error.

The primer's chapter 6.2 makes the general point: arms no
configuration takes are code no test compiles.

## 12. Outbound

The report is a patch, not a question: `|| __glibc_clang_prereq (3,
5)` in the `<wchar.h>` guard, as `<string.h>` took in 2.31. Its case
is the 2019 commit's own message, which fits word for word, with
BZ #25232 as precedent and chapter 10's measurements as evidence.
`<strings.h>` has the same gap for `index` and `rindex`; one patch
can carry both.

The venue is libc-alpha. A reply to the open series of chapter 6, as
the `<wchar.h>` twin of patch 16/35, may be the shortest route.

Whether to file is open. The library's repair does not depend on it.

## References

- glibc commits d8387c7b7b, 3f637079f5, 8e2e833ac4, 953ceff17a,
  cab4d74b01; glibc mainline at cbf59b8366.
- The 2009 libstdc++ half:
  <https://gcc.gnu.org/legacy-ml/gcc-patches/2009-01/msg01457.html>
- The open series, patches 16/35 and 17/35:
  <https://marc.info/?l=glibc-alpha&m=176272563907594&w=2>,
  <https://marc.info/?l=glibc-alpha&m=176272518507368&w=2>
- libc++ `<wchar.h>` and `<string.h>`:
  <https://github.com/llvm/llvm-project/tree/main/libcxx/include>
- LLVM review D150015: <https://reviews.llvm.org/D150015>
