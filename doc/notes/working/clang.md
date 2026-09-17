# Clang builds the library: an instantiation order the tree had never compiled

*Apache stdcxx revival notes, September 2026.*

Clang is the second compiler of the supported matrix, and until this
note it did not build the library. The failure looked like a template
problem in the explicit instantiation machinery, the most
compiler-characterized corner of the tree, and the platform assessment
had put it aside for verification with the actual compiler. This note
records that verification: what the configuration says under Clang and
whether it is right, why six translation units failed, the repair, and
what the test suite says once it builds. The toolchain is Clang 22.1.8
beside GCC 16.2.1, glibc 2.44, x86-64 Linux.

## 0. tl;dr

- Clang configures the tree through the GCC configuration,
  `CONFIG=gcc.config CXX=clang` on the `config` line. It reports itself
  as GCC 4.2 and walks the GCC branches; nothing in them is wrong for
  it.
- A Clang `config.h` differs from a GCC one in the compiler's header
  paths, the spelling of nine floating-point limits, which denote the
  same values, and two behavioural answers, both correct: Clang does
  not instantiate members defined after an explicit instantiation
  directive, and its `offsetof` rejects the qualified member designator
  the library uses. Two layout probes failed for a defect of their own
  and are repaired.
- The library failed in the six translation units that instantiate
  the arithmetic and pointer inserters. Under the answer Clang gives,
  the tree includes each template's definitions before its
  instantiation directives, and `<ostream>` pulls the inserter header
  in before `basic_ostream` is defined, so the inserters were defined
  and explicitly instantiated with an incomplete stream. GCC defers
  instantiation to the end of the translation unit and never saw it;
  no compiler had compiled that arrangement eagerly before. The fix
  is the pattern `<ostream>` already uses for the string inserter, and
  the GCC object code is byte-identical before and after.
- Five test sources are ill-formed in ways GCC accepts and Clang
  rejects: three narrowing conversions in braced initializers, a
  dependent using-declaration without `typename`, a return statement
  that cannot convert, and element types above Clang's array size
  limit. Repaired; one repair lowers the assertion count of one test
  by nine under both compilers.
- The suite under Clang is the GCC suite row for row, in all three
  configurations, except rows where Clang does better: the two driver
  self-tests with "misaligned address" failures pass, and the 32-bit
  numerics row is the 64-bit one. The tables are pinned beside the
  GCC ones.

## 1. The configuration

### 1.1 The GCC identity

Clang predefines `__GNUC__` 4 and `__GNUC_MINOR__` 2, so every
`__GNUG__` branch in the tree, the override header `_config-gcc.h`
among them, applies to it. That is the intended use of the GNU
identity macros, and the configuration confirms it: the
characterizations run under Clang produce the same answers as under
GCC for everything the branches assume. The version workarounds for
old GCC releases do not fire (4.2 is above every threshold the
remaining branches test); a stated GCC minimum, step 6 of the platform
assessment, would remove them and the question with them.

One dialect fact for the record: Clang's default dialect is C++17
(`__cplusplus` 201703L) where GCC 16's is C++20. The tree is built at
each compiler's default, as always; no `-std` is passed, and none is
needed.

### 1.2 The four answers that differ

A fresh 11S configuration under Clang, diffed against the GCC one,
differs in the header paths, in nine numbers, and in four
characterizations. A check program comparing the nine `_RWSTD_*_MAX`,
`_MIN` and `_EPSILON` values against `<float.h>` under both compilers,
and each compiler against the other's `config.h`, finds them equal;
the spelling differs because the probe prints the compiler's own
predefined macro and GCC spells the constants with more digits.

**`_RWSTD_NO_EXTERN_TEMPLATE_BEFORE_DEFINITION`, defined under Clang.**
The probe declares a class template whose members are defined in a
`.cc` file, explicitly instantiates it in one translation unit before
the `.cc` is included and declares it `extern template` in another,
and links the two. Under Clang the link fails: the instantiating unit
lacks the member that no other member calls,
`InstantiatedBeforeDefined<int>::instantiated_first`. That is the
standard's rule: an explicit instantiation definition of a class
template is a definition of only those members that have been defined
at the point of instantiation. GCC instantiates at the end of the
translation unit and emits every member. Six small programs pin down
the two compilers:

| case | Clang | GCC |
|---|---|---|
| explicit instantiation of a function template before its definition, in one unit | accepted, emitted | accepted, emitted |
| explicit instantiation of a class template before a member's definition | member not emitted | member emitted |
| function template defined, then explicitly instantiated while a class its body needs is incomplete, class defined after | error, implicit instantiation of undefined template | accepted |
| the same with `extern template` instead | accepted | accepted |
| declared, explicitly instantiated, class defined, template defined (the `_LAST` order) | accepted, emitted | accepted, emitted |
| defined, class defined, explicitly instantiated (definitions first with the class complete) | accepted, emitted | accepted, emitted |

So Clang defers a function template's instantiation when the
definition is not yet available and instantiates at the directive when
it is; GCC defers in both cases. The probe's answer is correct for
Clang, and the answer selects the `_FIRST` placement described in 1.3.

**`_RWSTD_NO_OFFSETOF`, defined under Clang.** The probe evaluates
`offsetof (S, S::i [0][1])`, a member designator qualified with the
class name, because that is the form the library uses in
`src/time_put.cpp` for the offsets of `__rw_time_t`'s members. Clang
stops at the qualifier ("expected ')'"); GCC accepts the form. The
standard defines the designator only as an unqualified member
followed by array subscripts, so Clang is within its rights and the
answer is correct: under Clang the library takes its pointer
arithmetic fallback, `(char*)&((T*)0)->m - (char*)0`, for those
offsets, which Clang in turn flags 100 times as a null pointer
subtraction. A first attempt at this note "repaired" the probe by
unqualifying the designator; that made Clang report `offsetof` as
working and the library failed to compile at the qualified use. The
probe characterizes the consumer's form, and that is the property it
must keep.

**`_RWSTD_NO_LCONV` and `_RWSTD_NO_STRUCT_TM`, defined under Clang
before the repair.** The two layout probes print the members and
padding of `struct lconv` and `struct tm` for the library's pure C
headers. With `offsetof` reported absent they take the same pointer
arithmetic fallback, whose result is a `ptrdiff_t`, and initialize a
`size_t` member from it inside a braced initializer. That is a
narrowing conversion, a warning in GCC and an error in Clang since
C++11, so both probes failed to compile and reported the layouts
absent. The fallback now casts to `size_t`; the probes compile and
print the same layouts GCC's do, and the GCC `config.h` is unchanged.

### 1.3 The switchboard

The macros that decide where a template's definitions are included
relative to its instantiation directives, for the record and for the
next reader who has to trace them:

- `_RWSTD_NO_IMPLICIT_INCLUSION`: the compiler does not find a
  template's `.cc` file by itself. Defined unconditionally in
  `_config-gcc.h`; only IBM's VisualAge had implicit inclusion, and it
  is retired. The block of `_defs.h` guarded on it is the only live
  one.
- `_RWSTD_NO_EXTERN_TEMPLATE_BEFORE_DEFINITION`: the probe above. It
  implies `_RWSTD_NO_EXPLICIT_INSTANTIATION_BEFORE_DEFINITION`.
- `_RWSTD_DEFINE_TEMPLATE_FIRST (name)` and
  `_RWSTD_DEFINE_TEMPLATE_LAST (name)`: the consumers, in nineteen
  headers. Without the macro above, `_FIRST` is 0 and `_LAST` includes
  the `.cc` after the instantiation directives, the GCC arrangement.
  With it, `_FIRST` includes the `.cc` before them and `_LAST` is 0,
  the Clang arrangement. Which templates each expands for depends on
  `_RWSTD_INSTANTIATE_<name>`, set by the `ti_*.cpp` sources when
  building the library, and on `_RWSTD_NO_<name>_DEFINITION` when
  using it.
- `_RWSTD_INSTANTIATE_TEMPLATES`, defined by the `ti_*.cpp` sources:
  the `_RWSTD_INSTANTIATE_n` and `_RWSTD_INSTANTIATE_FUN_n` directives
  expand to `template`, explicit instantiation definitions; without
  it, and without `_RWSTD_NO_EXTERN_TEMPLATE`, to `extern template`,
  declarations. `_RWSTD_NO_EXTERN_TEMPLATE` is defined for GCC 2 only.
- `_RWSTD_NO_EXTERN_FUNCTION_TEMPLATE`,
  `_RWSTD_NO_FUNCTION_EXPLICIT_INSTANTIATION`,
  `_RWSTD_NO_EXPLICIT_INSTANTIATION`, `_RWSTD_NO_IMPLICIT_INSTANTIATION`,
  `_RWSTD_NO_INSTANTIATE`, `_RWSTD_NO_TEMPLATE_DEFINITIONS` and
  `_RWSTD_NO_EXPLICIT_INSTANTIATION_WITH_IMPLICIT_INCLUSION`: further
  switches of the same family, none of them set on the matrix.

## 2. The library

### 2.1 The failure

Twenty-five errors, all the same line, all in six translation units:

```text
In file included from src/ti_insert_int.cpp:41:
In file included from include/ostream:46:
In file included from include/rw/_ioinsert.h:98:
include/rw/_ioinsert.cc:42:21: error: implicit instantiation of undefined
    template 'std::basic_ostream<char>'
include/rw/_ioinsert.h:138:27: note: in instantiation of function template
    specialization '__rw::__rw_insert<char, std::char_traits<char>, double>'
    requested here
```

`ti_insert_int.cpp`, `ti_insert_dbl.cpp`, `ti_insert_ptr.cpp` and
their wide counterparts define `_RWSTD_INSTANTIATE_INSERTER` and
include `<ostream>`. `<ostream>` includes `<rw/_ioinsert.h>` at its
line 46, before the definition of `basic_ostream`, because the class
body calls `__rw_insert` and needs the declarations. Under the
`_FIRST` placement the inserter header then includes `_ioinsert.cc`,
the definitions, at its line 98 and the explicit instantiation
directives follow at line 106 on. Clang, with the definitions in
hand, instantiates `__rw_insert<char, char_traits<char>, double>` at
the directive; its first line constructs a `basic_ostream::sentry`,
and `basic_ostream` is still a forward declaration.

Under GCC's `_LAST` placement the directives come first and the
definitions after them, still before the class; GCC defers both to the
end of the unit, where everything is complete. The `_FIRST` placement
had been taken before only by VisualAge, through its override header,
and VisualAge had implicit inclusion, so the block that expands
`_FIRST` did not apply to it; and by GCC 4.0.2, for whose bug 24511 the
probe was written, which deferred like every GCC. Clang is the first
compiler to take the placement and instantiate at the directive.
Nothing else in the nineteen headers on the switch has the problem: the
other `_FIRST` inclusions sit after the classes their templates need,
in the same header.

### 2.2 The repair

`<ostream>` already handles a helper whose templates need the complete
stream: `<rw/_stringio.h>` declares the string inserter on first
inclusion and defines and instantiates it only when the includer
defines `_RWSTD_INCLUDE_STRING_INSERTER`, which `<ostream>` does at
its end. The inserter header now has the same two parts. Its include
guard closes after the declarations; the `.cc` inclusions and the
instantiation directives follow under `_RWSTD_INCLUDE_IOINSERT`, once,
and `<ostream>` requests them after the class, next to the string
inserter. The `system_header` pragma moves above the guard, as in
`_stringio.h`, so that it covers the second pass. The `ti_*insert*.cpp`
sources still include the header after `<ostream>`, which is now a
no-op, as it was.

### 2.3 The check

Under GCC the change reorders text and must change nothing else. The
11S tree was rebuilt with `make -B`, every phase, and the 557 objects
of the library, the driver, the utilities, the tests and the examples
compared with the ones built before: 556 byte-identical, and the
driver's object, which embeds `__TIME__`, identical in disassembly.
Under Clang the library builds, and the rest of this note follows.

## 3. The suite

### 3.1 Five sources Clang rejects

| source | Clang says | what it was | the repair |
|---|---|---|---|
| `tests/include/rw_char.h`, `make_char` | narrowing `char` to `unsigned char` in a braced initializer | `{ 0.0, c }` for a `UserChar` | `UChar (c)`, the conversion the `wchar_t` overload beside it already spells out |
| `tests/localization/22.locale.moneypunct.cpp`, `check_format` | the same, eighteen times | a `UChar` table filled from `lconv`'s `char` members | `UChar (lc.m)` per member |
| `tests/regress/21.string.append.stdcxx-1060.cpp` | dependent using declaration resolved to type without `typename` | `using std::char_traits<ch>::char_type;` in a class deriving from the dependent base | `using typename` on the five type members |
| `tests/strings/21.cwctype.cpp`, `TEST_FUNCTION` | cannot initialize a `wctrans_t`, `const int*` on glibc, from an `int` | detection shims returning `-1` from a function template with a non-dependent return type; never instantiated, but the statement is checked at the definition | `return ret (-1);`, the `-1` the callers compare against, converted as the return type requires |
| `tests/utilities/20.temp.buffer.cpp`, `run_test` | array is too large | element types of `PTRDIFF_MAX / 2`, `PTRDIFF_MAX - 1` and `PTRDIFF_MAX` bytes, to exercise arithmetic overflow in `get_temporary_buffer` | `MAX_SIZE` is `PTRDIFF_MAX / 8`; Clang forms array types up to just under 2^60 bytes and rejects larger ones as an implementation limit |

The last repair changes the test's arithmetic: the overflow loop
doubles a divisor up to the element size, so three iterations fewer
run for each of the three types and the test asserts nine times less
under GCC as well. The pinned tables carry the new counts, 10891 in
64-bit and 10792 in 32-bit.

### 3.2 The tables

The suite was run under the harness in the 11S, 11s and 15D Clang
builds and pinned under `doc/notes/working/baseline/` beside the GCC tables,
with `-clang` in the file names. Row for row they are the GCC tables,
with these differences:

- `0.char` and `0.printf` pass every assertion. Their four and one
  "misaligned address" failures under GCC, from the `%{#ls}`, `%{Ac}`
  and `%{/*Gs}` directives, do not occur under Clang. The harness
  reports `0.printf` as `FORMAT`, since the test prints its own
  report, with exit 0.
- `23.bitset.cons` passed all 2347 assertions under the harness in the
  64-bit run and failed 440 and 680 in bare runs of the same binary,
  the counts GCC shows. The harness runs each test under an
  address-space limit; the read the test depends on is a property of
  the process's memory, not of the compiler.
- `18.numeric.special.float` fails 3 assertions in the Clang 32-bit
  build, the 64-bit count, where GCC's 32-bit build fails 6.
- The locale MT rows of 15D vary from run to run under Clang as they
  do under GCC; `locale-mt-race.md` names the cause. They are not a
  reference under either compiler.

The summary lines differ accordingly: the 64-bit Clang run counts
10068122 assertions and 736 failures against GCC's 10067682 and 1177,
the differences being the two self-tests, the bitset row and the
temporary-buffer row.

## 4. Warnings

Clang's diagnostics differ from GCC's in kind and number. From the
complete 32-bit builds, the library first, then the driver, the
utilities, the tests and the examples together:

| compiler | library | the rest |
|---|---|---|
| GCC | 15: nonnull-compare 10, implicit-fallthrough 3, unused-but-set-variable 1, switch 1 | 3020, led by deprecated-copy 2184, unused-local-typedefs 248, sizeof-array-div 145, int-in-bool-context 86, narrowing 71, catch-value 67, sized-deallocation 62 |
| Clang | 201: null-pointer-subtraction 100, implicit-exception-spec-mismatch 74, missing-exception-spec 12, undefined-bool-conversion 6, tautological-undefined-compare 3, shift-negative-value 3, three singles | 1023, led by deprecated-copy-with-user-provided-copy 254, unused-local-typedef 248, missing-exception-spec 172, implicit-exception-spec-mismatch 82, null-pointer-subtraction 52, logical-op-parentheses 29, tautological-undefined-compare 25, sizeof-array-decay 23 |

None was acted on here. Three are worth a line each:

- The 100 null pointer subtractions in the library are the `offsetof`
  fallback of 1.2, all in `time_put.cpp`. An `offsetof` with
  unqualified designators would remove both the fallback and the
  qualified form the probe exists for; that is a change to the library
  and its characterization together, for the floor step.
- The exception specification mismatches are the tree's declarations
  of the replaceable `operator new` and `operator delete` with
  `throw ()` where the compiler's implicit declarations are `noexcept`
  under C++11 and later, and the missing specifications are the
  converse. Clang notes them; GCC does not. They are inert, and they
  are the `_RWSTD_NO_EXT_OPERATOR_NEW` question of the platform
  retirement seen from the other side.
- `deprecated-copy`, under either name, is the test suite's classes
  with a user-provided copy constructor and an implicit copy
  assignment, a C++11 deprecation the tree predates.

## 5. What is not settled

- The Clang build links the language support library GCC ships,
  `-lsupc++`, through the GCC configuration; on this machine that is
  the only support library present. Whether to characterize the
  choice, so that a Clang with `libc++abi` and no GCC beside it builds,
  is a matrix question for a machine that has one.
- The MT rows under Clang add nothing to the race analysis; the fix
  named in `locale-mt-race.md` is the way to a 15D reference under
  both compilers.
- The GCC-only failures of `0.char`, `0.printf` and the 32-bit
  numerics row are narrowed, not explained: the same source passes
  under a second compiler. The `TODO` entries carry the fact.
