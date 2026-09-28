# The wide string functions under Clang: a shape the configuration cannot see

As of 2026-09-28 (5f3b424b).

`21.cwchar` passes under GCC and fails five assertions under Clang.
Same glibc, all three Clang configurations. The configuration writes
the same `config.h` lines for both compilers. That is the defect.
glibc gives Clang a different declaration of five functions, and the
probes cannot tell the two declarations apart.

This note lists the wide C API the library depends on and measures
the difference. It shows why the repair the `TODO` entry proposes
cannot work, and proposes one that does, prototyped and measured. It
records four defects found on the way, all on paths glibc never
takes. It assumes `primer-libc-declarations.md`, which explains
`_RWSTD_NO_<F>` and `_RWSTD_NO_<F>_IN_LIBC`.

## 0. The short version

- **The failure.** C prototypes of `wcschr`, `wcspbrk`, `wcsrchr`,
  `wcsstr` and `wmemchr` take a `const` pointer and return a
  modifiable one. C++ replaces each with a pair of overloads that
  keeps the constness. glibc declares the pair only under
  `__GNUC_PREREQ (4, 4)`. Clang presents itself as GCC 4.2 and gets
  the C prototype. `std::wcschr` on a `const wchar_t*` returns
  `wchar_t*`.
- **Why the configuration misses it.** The probe calls
  `wcschr ((wchar_t*)0, 0)`. Both shapes accept that call, by design.
  The GCC and Clang `config.h` agree on all five.
- **Why the queued fix fails.** `_RWSTD_NO_WCSCHR` would send
  `<cwchar>` into its "in libc" arm. That arm declares its own C
  prototype and a const overload. Against glibc's declaration both
  are hard errors, measured. The missing state is "declared, C
  prototype only".
- **The proposed repair.** One characterization in the shape of
  `ABS_OVERLOADS.cpp`. It asks each function what a const call
  returns and prints one macro per function. Under a macro,
  `<cwchar>` defines both overloads in `std`, forwarding to the
  global C function. The global namespace stays as glibc left it.
  Prototyped outside the tree: five macros commented under GCC, five
  defined under Clang, the pair in `std` const-correct under Clang.
- **The library itself is unaffected.** It calls none of the five
  through `std`. Its one use of `wmemchr`, in
  `char_traits<wchar_t>::find`, works with either shape. The defect
  is in what users see.
- **On the way.** `<cwchar>`'s fallback `wcschr` hangs. Its "in
  libc" `wcsrchr` calls itself. `<wchar.h>`'s `wcsrchr` block
  undefines the wrong macro. `<cwchar>` has no "in libc" arm for
  `wmemchr`. glibc reaches none of them.

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

## 3. glibc's two shapes

glibc 2.44, `<wchar.h>`, near line 68:

```c
/* Tell the caller that we provide correct C++ prototypes.  */
#if defined __cplusplus && __GNUC_PREREQ (4, 4)
# define __CORRECT_ISO_CPP_WCHAR_H_PROTO
#endif
```

And for each of the five:

```c
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

The C++ pair is two `extern "C++"` declarations. `__asm` binds both
to the one C symbol. The pair arrived in glibc 2.10 (d8387c7b7b,
2009).

The guard tests GCC's version. Clang defines `__GNUC__` 4 and
`__GNUC_MINOR__` 2, so it fails. `<string.h>` had the same guard
until glibc 2.31. That release added `|| __glibc_clang_prereq (3, 5)`
(953ceff17a, 2019, BZ #25232: "Without the asm redirects, strchr et
al. are not const-correct"). `<wchar.h>` never got the clause. The
narrow functions pass under Clang; the wide ones fail.

Measured: a program reports whether a call's result points to const,
for a const and a non-const argument.

| compiler | `wcschr (const wchar_t*)` | `wcschr (wchar_t*)` | `__CORRECT_ISO_CPP_WCHAR_H_PROTO` |
|---|---|---|---|
| GCC 16.2.1 | `const wchar_t*` | `wchar_t*` | defined |
| Clang 22.1.8 | `wchar_t*` | `wchar_t*` | undefined |

## 4. Why the configuration misses it

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

## 5. Why the existing arm cannot take over

The `TODO` entry proposes that a failing characterization let "the
`<cwchar>` branch that already defines the overloads over the C
function" take over. That branch is the "in libc" arm:

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

It serves a libc that exports the symbol and declares nothing. It
declares the global function with a signature of its own. With
glibc's C prototype in scope:

1. `extern "C" wchar_t* wcschr (wchar_t*, wchar_t)` redeclares a C
   function with another type. C linkage allows one function per
   name. Error.
2. `const wchar_t* wcschr (const wchar_t*, wchar_t)` has glibc's
   parameters and another return type. Functions do not overload on
   return type. Error.

Measured with Clang, the tree's headers and the 15D-clang
`config.h`, `-D_RWSTD_NO_WCSCHR` forced for the experiment:

```
include/ansi/cwchar:563:21: error: conflicting types for 'wcschr'
include/ansi/cwchar:565:23: error: functions that differ only in their return type cannot be overloaded
```

Without the macro the same file compiles. A characterization that
set `_RWSTD_NO_WCSCHR` under Clang would trade five failed
assertions for a library that does not build.

The global namespace cannot hold the pair while glibc's prototype is
there. The const overload would be glibc's declaration with another
return type. The pair can only live in `std`.

## 6. Other implementations

libstdc++ meets the same header under Clang. Its `<cwchar>`, GCC 16:

```cpp
#ifndef __CORRECT_ISO_CPP_WCHAR_H_PROTO
  inline wchar_t*
  wcschr(wchar_t* __p, wchar_t __c)
  { return wcschr(const_cast<const wchar_t*>(__p), __c); }
  ...
#endif
```

It adds the non-const overload in `std`. The const call still goes
to `using ::wcschr`, the C prototype. Measured with the program from
chapter 3: under Clang with libstdc++, `std::wcschr` on a
`const wchar_t*` returns `wchar_t*`. The repair is half. It also keys
on glibc's private macro. The library's rules call for a
characterization instead.

## 7. The proposed repair

### 7.1 The characterization

`ABS_OVERLOADS.cpp` answers a question of this kind already. One
program, one call per case, one printed macro per answer. The same
shape, prototyped in the session scratchpad:

```cpp
// checking for const overloads of wcschr() et al.

#include <stdio.h>
#include <wchar.h>

static int returns_const (const wchar_t*) { return 1; }
static int returns_const (wchar_t*)       { return 0; }

static void check (int is_const, const char *name)
{
    if (is_const)
        printf ("%s", "// ");

    printf ("#define _RWSTD_NO_%s_CONST_OVERLOAD\n", name);
}

int main ()
{
    const wchar_t* const s = L"";

    check (returns_const (wcschr (s, L'\0')),     "WCSCHR");
    check (returns_const (wcspbrk (s, s)),        "WCSPBRK");
    check (returns_const (wcsrchr (s, L'\0')),    "WCSRCHR");
    check (returns_const (wcsstr (s, s)),         "WCSSTR");
    check (returns_const (wmemchr (s, L'\0', 0)), "WMEMCHR");

    return 0;
}
```

Against glibc 2.44:

```
== g++                                   == clang++
// #define _RWSTD_NO_WCSCHR_CONST_OVERLOAD   #define _RWSTD_NO_WCSCHR_CONST_OVERLOAD
// #define _RWSTD_NO_WCSPBRK_CONST_OVERLOAD  #define _RWSTD_NO_WCSPBRK_CONST_OVERLOAD
// ... all five commented                 ... all five defined
```

It asks what the declaration probe cannot: not whether the function
exists, but what a const call returns. It names no compiler and no
libc version. If glibc gains the Clang clause, the answer flips with
no change to the tree.

It also settles one macro or five. glibc guards the five together,
which argued for one. The `ABS_OVERLOADS` precedent makes five
cheap. Five macros match `headers.inc` and `21.cwchar`, which both
work per function. They do not assume every libc groups the five as
glibc does. The names above are placeholders.

The real file needs two things the prototype lacks:

- **A function missing from the header** would stop the probe from
  compiling. Every macro would then be undefined, which reads as
  "const overload present". The probe must skip, in the
  preprocessor, any function `config.h` marks `_RWSTD_NO_<F>`. That
  answer is there in time: `GNUmakefile.cfg` runs the shell scripts,
  `libc_decl.sh` among them, before every `.cpp` characterization.
- **The file header** follows the other characterizations: licence
  block, the one-line description the configuration prints, nothing
  that names a compiler.

### 7.2 `<cwchar>`

Under each macro, in the Linux branch, the declared arm stops
re-exporting and defines the pair in `std`:

```cpp
#ifndef _RWSTD_NO_WCSCHR
#  ifndef _RWSTD_NO_WCSCHR_CONST_OVERLOAD
using ::wcschr;
#  else
inline const wchar_t* wcschr (const wchar_t *__s, wchar_t __c)
{
    return ::wcschr (__s, __c);
}

inline wchar_t* wcschr (wchar_t *__s, wchar_t __c)
{
    return ::wcschr (__s, __c);
}
#  endif   // _RWSTD_NO_WCSCHR_CONST_OVERLOAD
#elif ...
```

Both forwarders call the global C prototype, which takes a const
pointer. Neither needs a cast. The const one converts the result on
return. Measured standalone, the pair in a stand-in namespace, under
Clang: a const argument returns `const wchar_t*`, a non-const one
`wchar_t*`, and both find the right character.

### 7.3 `<wchar.h>`

The global namespace keeps glibc's C prototype; chapter 5 shows it
cannot hold the pair. On Linux the library's `<wchar.h>` takes its
`_RWSTD_NO_DEPRECATED_C_HEADERS` branch. That branch includes
glibc's header by path and imports nothing from `std`. Nothing
changes there on this platform. The other branch exports `std` names
with `using std::wcschr`. Under the macro that would collide with
glibc's declaration, so it must skip the five.

### 7.4 The check

- 15D-clang, 11S-clang, 11s-clang: the five macros defined,
  `21.cwchar` at 66 assertions, 0 failed, 0 warnings.
- The three GCC configurations: `config.h` gains five commented
  lines, nothing else. Suite tables unchanged.
- Bite: the characterization forced to report "present" under Clang
  brings back the five assertions and five warnings.
- The three Clang tables re-pinned for the `21.cwchar` row.
  `ref-test-baseline.md` chapter 5 updated to match.

## 8. Found on the way

Four defects in the dormant arms of the same headers. glibc reaches
none of them. Each has its check. None is in `TODO` yet.

1. **`<cwchar>`'s own `wcschr` never advances.** The "neither" arm,
   for a libc with no `wcschr` at all:

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
2. **`<cwchar>`'s "in libc" `wcsrchr` calls itself.** The arm
   declares `extern "C" const wchar_t* wcsrchr (const wchar_t*,
   wchar_t)`. No libc has that prototype; C returns `wchar_t*`. It
   then defines a non-inline `wchar_t* wcsrchr (wchar_t*, wchar_t)`
   in a header. The body, `wcsrchr (_RWSTD_CONST_CAST (wchar_t*,
   __s), __c)`, selects itself by exact match. Check: compiled
   standalone, GCC warns `-Winfinite-recursion` and the call dies of
   SIGSEGV. The sibling arms have the constness the other way round;
   this one looks transposed.
3. **`<wchar.h>`'s `wcsrchr` block undefines `_RWSTD_NO_WCSCHR`.**
   The block guarded by `_RWSTD_NO_WCSRCHR` ends with
   `#undef _RWSTD_NO_WCSCHR` and a closing comment naming `WCSCHR`.
   It is a copy of the block above. Check: reading. The macro it
   should undefine stays set for the rest of the translation unit.
4. **`<cwchar>` has no "in libc" arm for `wmemchr`.**
   `#ifndef _RWSTD_NO_WMEMCHR` goes straight to `#else`, the
   library's own definition. Every sibling tests `_IN_LIBC` first. A
   libc that exports `wmemchr` without declaring it would get a
   second definition. `<wchar.h>` has the arm. Check: reading. The
   fallback body itself is correct.

The primer's chapter 6.2 makes the general point: arms no
configuration takes are code no test compiles. A test that includes
the headers with the macros forced, one translation unit per arm,
would have caught all four.

## 9. Outbound

glibc's `<wchar.h>` could take the clause `<string.h>` took in 2.31:
`|| __glibc_clang_prereq (3, 5)` in the guard of
`__CORRECT_ISO_CPP_WCHAR_H_PROTO`. Clang would then get the pair.
The characterization of 7.1 would report "present", and the
library's repair would go dormant with no edit. A report would cite
BZ #25232 as precedent. Whether to file one is open. The repair does
not depend on it.
