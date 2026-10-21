# The characterization survey

## 0. tl;dr

Configuration asks three different questions: **what language machinery
works, what the runtime and C library provide, and how the target
represents data**. The supported compilers answer the first group
the same way; the tree keeps the probes and their branches, since it
stays C++98 and the branches are its portability record. Sizes,
layouts, calling interfaces and runtime behavior are platform
questions.

The source census contains 148 named C++ probes, five supporting C++
translation units, one shell driver and 12 other support files. The
makefile discovers all `.cpp` and `.sh` files; the support translation
units are part of that machinery, not nine independent capabilities.
**One probe does not necessarily mean one macro.**

The catalogue below pairs each named probe with its conventional
status macro, purpose and direct references. The output-family table
also covers the shell driver's declaration checks and multi-output
probes. Concrete answers belong to each build's configuration.

## 1. How the answers are produced

`etc/config/GNUmakefile.cfg` includes the build's `makefile.in` and
walks the shell scripts first, then the C++ sources. A source without
`main` is a compilation test. A source with `main` must compile, link
and run. A `.lib.cpp` source supplies a library for a cooperating
probe; source `LDOPTS` annotations request its link dependencies.

For `FEATURE.cpp`, failure normally defines `_RWSTD_NO_FEATURE`;
success records the same definition commented out. Negative probes
named `NO_FEATURE.cpp` invert the exit-status interpretation and emit
`_RWSTD_NO_FEATURE`, with no second `NO_`. Thus a defined negative
macro must be read according to the actual question, not as a compiler
error. Executables can also print definitions into `config.h`.

Dependencies are pulled before compilation: the object rule extracts
simple `_RWSTD_*` `#ifdef`/`#ifndef` dependencies from the source,
looks for their recorded answers and recursively configures missing
ones. **Alphabetical traversal is not the dependency order.** The walk
is deliberately serial and marked `.NOTPARALLEL`; it accumulates one
configuration header and log.

`config.h` is an input to `include/rw/_config.h`, which can override
or derive further settings. A recorded answer is therefore not by
itself the effective setting at every use site. Command-line build
`CXXOPTS` and `CPPOPTS` do not characterize a new configuration; the
configuration's compiler and flags come from `makefile.in`.

## 2. Reachability and retirement

**A scheduled probe, a referenced answer and an active fallback are
three different things.** All 148 named probes are discoverable by
the current configuration rule. The catalogue's references answer
the second question conservatively: where the exact macro token is
read outside its producer. Determining whether a conditional branch
is compiled requires following configuration overrides and surrounding
conditions as well. No preprocessing or build was performed here.

| group | status on the supported compilers |
|---|---|
| Old language deficiencies: partial specialization, and until 2026-10-17 `NAMESPACE`, `BOOL`, `EXPLICIT`, `TYPENAME` and the casts | Every probe passes in the six configurations and under eccp in strict C++03. These gates are being retired one by one, the branch they select with them, since no compiler the tree is built with takes that branch and it cannot be compiled; `analysis-feature-gates.md` is the census and the order, and 2.2 lists what has gone. |
| Obsolete mechanisms: dynamic exception specifications | The probes fail in the six configurations, and that is their job: a failing probe selects the path the compiler's dialect needs. |
| Runtime ABI: exception classes, allocation functions, RTTI members and namespace placement | Questions about the runtime, not the language. A historical ABI probe is not a description of public API support. |
| Platform facts: sizes, typedefs, floating point, C structures, conversion interfaces, locale naming | Retain characterization or deliberately replace it with an equally sound source of the information. |
| No direct reader of a status macro | Inspect emitted values and other probe dependencies before considering retirement. A status marker can be unused while the producer remains essential. |

### 2.1 Cases that deserve individual treatment

`ATOMIC_OPS.cpp` currently has a `main` that simply returns zero.
The vendor-backend retirement left it as a success stub. Its
commented-out `_RWSTD_NO_ATOMIC_OPS` **does not characterize the GNU
`__atomic` built-ins** the read-modify-write backend uses; five probes
do, one per width. `CHAR_ATOMIC_OPS.cpp`, `SHORT_ATOMIC_OPS.cpp`,
`INT_ATOMIC_OPS.cpp`, `LONG_ATOMIC_OPS.cpp` and `LLONG_ATOMIC_OPS.cpp`
run the five built-ins `include/rw/_atomic-builtins.h` uses on a value
that carries across every byte of the type, compiled, linked without
libatomic and run: the add, the subtract and the exchange,
sequentially consistent, and the release store and the acquire load
that back the ordered accesses. `include/rw/_atomic.h` selects the
backend when the `int` probe passes, and the backend leaves out each
width whose probe failed. Older investigation prose describing the
former Windows probe no longer describes this source.

`THREAD_SAFE_ERRNO` has no direct reader of its status macro. It is a
stronger retirement-review candidate than a useful value generator
whose status marker happens to be unused.

`CONST_CAST` and `REINTERPRET_CAST` have no direct readers of their
negative status macros either. By contrast, the `FLOAT`, `LIMITS`,
`SIZE_T`, `CTYPE_BITS` and structure probes emit data used throughout
the library. **Do not remove a producer by searching only for the
macro derived from its filename.**

Runtime and header probes also need their build context:
these probes intentionally avoid the host C++ headers and link to
runtime support. A negative constructor, placement-new or namespace
probe is an answer to its particular source/link experiment, not a
claim that the compiler cannot implement that C++ facility.

### 2.2 Retired probes

A retired probe left together with every reader of its macro, and
its row is gone from the catalogue below.

| probe | macro | retired | why |
|---|---|---|---|
| `MEMBER_TEMPLATES` | `NO_MEMBER_TEMPLATES` | 2026-10-17, bfbf7ac3 and the seven commits after it | GCC, Clang and eccp in strict C++03 pass it, and no compiler since about 2000 fails it. The branch it selected, the namespace-scope twins of the containers' range members among the rest, could not be compiled and rotted unseen. |
| `MEMBER_TEMPLATE_OVERLOAD` | `NO_MEMBER_TEMPLATE_OVERLOAD` | 2026-10-17 | The same three compilers pass it. Its one reader, the condition on `bitset::to_string`, keeps the return-type gate alone. |
| `BOOL`, `TYPENAME`, `EXPLICIT`, `STATIC_CAST`, `DYNAMIC_CAST`, `CONST_CAST`, `REINTERPRET_CAST`, `NEW_CLASS_TEMPLATE_SYNTAX`, `NEW_FUNC_TEMPLATE_SYNTAX`, `NO_FOR_LOCAL_SCOPE` | `NO_BOOL` and the rest, one each | 2026-10-17, one commit | The keyword and syntax gates of C++98: every compiler of the matrix passes them. `_TYPENAME`, `_EXPLICIT`, the `_RWSTD_*_CAST` and `_RWSTD_SPECIALIZED_*` macros in `rw/_defs.h` were then spelled out as the keywords. `_RWSTD_NO_MUTABLE`, which never had a probe, went with them. |
| `NAMESPACE` | `NO_NAMESPACE` | 2026-10-17 | The `namespace` keyword; every compiler of the matrix has it. Nine probes read the gate to put their names in `std`; they do so unconditionally now. `_RWSTD_NAMESPACE`, `_STD`, `_RW` and `_USING` in `rw/_defs.h` were then spelled out as `namespace`, `std`, `__rw` and `using`. |
| `EXPLICIT_ARG`, `EXTERN_INLINE`, `FUNCTION_TRY_BLOCK`, `TEMPLATE_DEFAULT_ARG_CONVERSION`, `INSTANTIATE_DEFAULT_ARGS`, `STATIC_CONST_MEMBER_EXPR_CONST`, `EXPLICIT_MEMBER_INSTANTIATION`, `COLLAPSE_STATIC_LOCALS`, `COLLAPSE_TEMPLATE_LOCALS`, `EXTERN_C_EXCEPTIONS`, `LIB_EXCEPTIONS`, `THREAD_SAFE_ERRNO` | `NO_*`, one each | 2026-10-17, one commit | Nothing read the status macro and the probe printed nothing else; four took a support translation unit with them. |
| `EXPLICIT_CTOR_INSTANTIATION`, `IMPLICIT_INSTANTIATION`, `STATIC_TEMPLATE_MEMBER_INIT`, `DEFAULT_TEMPLATE_ARGS`, `DEPENDENT_TEMPLATE`, `SPECIALIZATION_ON_RETURN_TYPE`, `EXPLICIT_MEMBER_SPECIALIZATION`, `EXPLICIT_FUNC_INSTANTIATION`, `EXPLICIT_INSTANTIATION`, `SPECIALIZED_FRIEND`, `OVERLOAD_OF_TEMPLATE_FUNCTION` | `NO_*`, one each | 2026-10-17, one commit | The small template gates: every compiler of the matrix passes them. `_RWSTD_SPECIALIZED_FRIEND`, `_RWSTD_DEFAULT_ARG` and `_RWSTD_DUMMY_ARG` went with them, spelled out or unused; the Borland branch beside the first was the last `__TURBOC__` in the tree. |
| `FRIEND_TEMPLATE`, `FUNC_PARTIAL_SPEC`, `NONDEDUCED_CONTEXT`, `TEMPLATE_ON_RETURN_TYPE` | `NO_*`, one each | 2026-10-17, one commit | Four more template gates every compiler of the matrix passes. The facet and temporary-buffer macros that spelled the return-type workaround (`_STD_USE_FACET` and kin) and the three local size-type macros went with them, spelled out. |
| `STATIC_CONST_MEMBER_INIT`, `COLLAPSE_TEMPLATE_STATICS` | `NO_*`, one each | 2026-10-17, one commit | In-class initializers of static const members and one static per template instance across translation units; every compiler of the matrix passes. The static mutex initialization fallback, `_RWSTD_NO_STATIC_MUTEX_INIT`, had no definer left and went too; `_RWSTD_STATIC_CONST`, `_RWSTD_DEFINE_STATIC_CONST` and `_RWSTD_BITMASK_ENUM` are spelled out. |
| `CLASS_PARTIAL_SPEC`, `PART_SPEC_OVERLOAD` | `NO_*`, one each | 2026-10-17, one commit | Partial specialization and partial ordering of function templates; every compiler of the matrix passes. The branches they selected were the largest of the family: iterator traits done by overloads, a `vector<bool>` that was a plain class, swap as a friend. The iterator-traits macros they left are spelled out in the commit after. |
| `POD_ZERO_INIT`, `NESTED_CLASS_ACCESS`, `NONCLASS_ARROW_RETURN`, `EXTERN_C_COMPATIBILITY`, `DUMMY_DEFAULT_ARG`, `CV_VOID_SPECIALIZATIONS`, `EMPTY_MEM_INITIALIZER` | `NO_*`, one each | 2026-10-17, one commit | The other language rules of C++98: value-initialization of a POD, a nested class's access (CWG 45), `operator->` returning a pointer, `extern "C"` function types, a dummy default argument, specializations on cv-qualified `void`, an empty mem-initializer. Every compiler of the matrix passes; `DUMMY_DEFAULT_ARG` had lost its last reader earlier in the day. `_RWSTD_OPERATOR_ARROW` is spelled out. |
| `LONG_DOUBLE` | `NO_LONG_DOUBLE` | 2026-10-18 | Whether `long double` is a type of its own that overload resolution can pick; every compiler of the matrix passes. The gate hid the long double half of the numeric and money facets, `numeric_limits<long double>`, `complex<long double>`, the `<cmath>` overloads and the bit patterns in `limits_bits.cpp`; with it defined the library replaced a required virtual of `money_get` with a double one, so that build was never conforming. The five host-fact probes that read it (`FLOAT`, `INFINITY`, `LIMITS`, `MATH_OVERLOADS`, `VA_LIST`) print their long double answers unconditionally. |
| `WCHAR_T`, `NATIVE_WCHAR_T` | `NO_WCHAR_T`, `NO_NATIVE_WCHAR_T` | 2026-10-18, three commits | `wchar_t` has been a keyword since C++98 and every compiler of the matrix has it; `NATIVE_WCHAR_T` was Visual C++'s `/Zc:wchar_t-`, and `rw/_config.h` derived it from `WCHAR_T`. The gate hid the wide half of the library: the `w` typedefs of `<iosfwd>`, the wide facets and their instantiation units, `wcodecvt.cpp`, `wctype.cpp`, 31 conditions in `time_put.cpp` alone, and the wide cases of 74 tests. `LIMITS` prints `_RWSTD_WCHAR_SIZE` and the `wchar_t` limits unconditionally. |
| `HONOR_STD`, `PLACEMENT_DELETE` | `NO_HONOR_STD`, `NO_PLACEMENT_DELETE` | 2026-10-18 | g++ 2.9x's `-fhonor-std` and its placement-delete internal error. The tree no longer claims that compiler: every `__GNUG__ <= 2` block went with the two gates, in `rw/_config-gcc.h`, `rw/_config.h`, `<new>`, `<valarray>`, the `<cxxx>` headers, the sources, five probes, two tests and the locale utility. The 25 probes that put their names in `std` under `HONOR_STD` do so unconditionally. `RUNTIME_IN_STD` stays, by decision (`analysis-feature-gates.md` 2.5). |
| `WCSFTIME_WCHAR_T_FMAT` | `NO_WCSFTIME_WCHAR_T_FMAT`, `_RWSTD_WCSFTIME_ARG3_T` | 2026-10-18 | Whether `wcsftime` takes a `const char*` format, as HP-UX 11's did. Six calls in `time_put.cpp` have passed wide literals since 2006 whatever the answer, so a C library that failed the probe had not built the library for twenty years; the one branch that honored it, and the two `_RWSTD_FMT_*` macros nothing read, went with the probe. |

## 3. Outputs beyond the status macro

All macro names below have the `_RWSTD_` prefix. Patterns identify
families whose individual members depend on the producer.

| producer | information emitted |
|---|---|
| `libc_headers.sh` (`libc_decl.sh` until 2026-10-18) | Header availability and native paths (`NO_*_H`, `ANSI_C_*_H`). Names are generated from its header list. Until a44256a6 it also probed every function's declaration and link availability; `history-libc-declarations.md` records how. |
| `LIMITS` | Integer bounds, sizes, fixed/least-width types, character width, multibyte limit and two's-complement answer. |
| `FLOAT`, `INFINITY` | Floating-point constants, special-value byte representations, denormal support and string-to-floating underflow behavior. |
| `SIZE_T`, `SIG_ATOMIC_T`, `WINT_T`, `WCTYPE_T`, `WCTRANS_T`, `VA_LIST`, `UNISTD_DECL` | Underlying typedefs, ranges, signal/stdio constants, `va_list` shape and POSIX types/constants. |
| `STRUCT_TM`, `LCONV`, `MBSTATE_T` | C structure layout declarations or storage sizes. |
| `CTYPE_BITS`, `LOCALE_NAME_FMAT` | C character-class masks, locale category values, separators and composite-name conventions. |
| `ABS_OVERLOADS`, `DIV_OVERLOADS`, `MATH_OVERLOADS` | Per-type overload availability, in addition to the probe's own status marker. |
| `ICONV`, `MUNMAP` | Argument types for C interfaces. |
| `LDBL_PRINTF_PREFIX`, `LLONG_PRINTF_PREFIX` | Formatting prefixes for the relevant arithmetic types. |
| `TLS`, `NEWLINE`, `UNAME` | Thread-local spelling, newline representation and operating-system identifiers. |
| `VSNPRINTF_RETURN` | Distinct checks of termination and return behavior. |
| `RUNTIME_IN_STD` | Runtime namespace availability macros, also involved in the individual runtime probes. |

## 4. Probe catalogue

Each row corresponds to `etc/config/src/NAME.cpp`; its question is
the source's stated purpose, with notable qualifications discussed
above. The macro column omits `_RWSTD_`. Its value is determined by
the compiler, runtime and build configuration being characterized.

Reference labels: **L** = library headers/sources, **P** = another
probe/support source, **B** = configuration/build machinery,
**T** = tests, **E** = examples, **V** = utilities. A representative
path follows; `none` means no direct reader found. `+ output` marks
a source that prints additional results. References were collected
after removing comments and definition/undefinition targets; they are
lexical evidence, not an evaluation of conditional compilation.
Token-pasted and script-generated names need family-level inspection.

| probe / macro suffix | question | direct readers |
|---|---|---|
| [ABS_OVERLOADS](../../etc/config/src/ABS_OVERLOADS.cpp) / `NO_ABS_OVERLOADS` + output | overloads of abs() | none |
| [ATOMIC_OPS](../../etc/config/src/ATOMIC_OPS.cpp) / `NO_ATOMIC_OPS` | Success stub; does not exercise atomics | L; `include/rw/_atomic.h` |
| [BAD_ALLOC_ASSIGNMENT](../../etc/config/src/BAD_ALLOC_ASSIGNMENT.cpp) / `NO_BAD_ALLOC_ASSIGNMENT` | bad_alloc assignment operator | L; `src/memory.cpp` |
| [BAD_ALLOC_COPY_CTOR](../../etc/config/src/BAD_ALLOC_COPY_CTOR.cpp) / `NO_BAD_ALLOC_COPY_CTOR` | bad_alloc copy ctor | L; `src/memory.cpp` |
| [BAD_ALLOC_DEFAULT_CTOR](../../etc/config/src/BAD_ALLOC_DEFAULT_CTOR.cpp) / `NO_BAD_ALLOC_DEFAULT_CTOR` | bad_alloc default ctor | L; `src/memory.cpp` |
| [BAD_ALLOC_DTOR](../../etc/config/src/BAD_ALLOC_DTOR.cpp) / `NO_BAD_ALLOC_DTOR` | bad_alloc dtor | L; `src/memory.cpp` |
| [BAD_ALLOC_WHAT](../../etc/config/src/BAD_ALLOC_WHAT.cpp) / `NO_BAD_ALLOC_WHAT` | bad_alloc::what() | L; `src/memory.cpp` |
| [BAD_CAST_ASSIGNMENT](../../etc/config/src/BAD_CAST_ASSIGNMENT.cpp) / `NO_BAD_CAST_ASSIGNMENT` | bad_cast assignment operator | L; `src/typeinfo.cpp` |
| [BAD_CAST_COPY_CTOR](../../etc/config/src/BAD_CAST_COPY_CTOR.cpp) / `NO_BAD_CAST_COPY_CTOR` | bad_cast copy ctor | L; `src/typeinfo.cpp` |
| [BAD_CAST_DEFAULT_CTOR](../../etc/config/src/BAD_CAST_DEFAULT_CTOR.cpp) / `NO_BAD_CAST_DEFAULT_CTOR` | bad_cast default ctor | L; `src/typeinfo.cpp` |
| [BAD_CAST_DTOR](../../etc/config/src/BAD_CAST_DTOR.cpp) / `NO_BAD_CAST_DTOR` | bad_cast dtor | L; `src/typeinfo.cpp` |
| [BAD_CAST_WHAT](../../etc/config/src/BAD_CAST_WHAT.cpp) / `NO_BAD_CAST_WHAT` | bad_cast::what() | L; `src/typeinfo.cpp` |
| [BAD_EXCEPTION_ASSIGNMENT](../../etc/config/src/BAD_EXCEPTION_ASSIGNMENT.cpp) / `NO_BAD_EXCEPTION_ASSIGNMENT` | bad_exception assignment operator | L; `src/exception.cpp` |
| [BAD_EXCEPTION_COPY_CTOR](../../etc/config/src/BAD_EXCEPTION_COPY_CTOR.cpp) / `NO_BAD_EXCEPTION_COPY_CTOR` | bad_exception copy ctor | L; `src/exception.cpp` |
| [BAD_EXCEPTION_DEFAULT_CTOR](../../etc/config/src/BAD_EXCEPTION_DEFAULT_CTOR.cpp) / `NO_BAD_EXCEPTION_DEFAULT_CTOR` | bad_exception default ctor | L; `src/exception.cpp` |
| [BAD_EXCEPTION_DTOR](../../etc/config/src/BAD_EXCEPTION_DTOR.cpp) / `NO_BAD_EXCEPTION_DTOR` | bad_exception dtor | L; `src/exception.cpp` |
| [BAD_EXCEPTION_WHAT](../../etc/config/src/BAD_EXCEPTION_WHAT.cpp) / `NO_BAD_EXCEPTION_WHAT` | bad_exception::what() | L; `src/exception.cpp` |
| [BAD_TYPEID_ASSIGNMENT](../../etc/config/src/BAD_TYPEID_ASSIGNMENT.cpp) / `NO_BAD_TYPEID_ASSIGNMENT` | bad_typeid assignment operator | L; `src/typeinfo.cpp` |
| [BAD_TYPEID_COPY_CTOR](../../etc/config/src/BAD_TYPEID_COPY_CTOR.cpp) / `NO_BAD_TYPEID_COPY_CTOR` | bad_typeid copy ctor | L; `src/typeinfo.cpp` |
| [BAD_TYPEID_DEFAULT_CTOR](../../etc/config/src/BAD_TYPEID_DEFAULT_CTOR.cpp) / `NO_BAD_TYPEID_DEFAULT_CTOR` | bad_typeid default ctor | L; `src/typeinfo.cpp` |
| [BAD_TYPEID_DTOR](../../etc/config/src/BAD_TYPEID_DTOR.cpp) / `NO_BAD_TYPEID_DTOR` | bad_typeid dtor | L; `src/typeinfo.cpp` |
| [BAD_TYPEID_WHAT](../../etc/config/src/BAD_TYPEID_WHAT.cpp) / `NO_BAD_TYPEID_WHAT` | bad_typeid::what() | L; `src/typeinfo.cpp` |
| [BITS_TYPES___MBSTATE_T_H](../../etc/config/src/BITS_TYPES___MBSTATE_T_H.cpp) / `NO_BITS_TYPES___MBSTATE_T_H` | __mbstate_t in &lt;bits/types/__mbstate_t.h&gt; | L; `include/rw/_mbstate.h` |
| [C99_MATH](../../etc/config/src/C99_MATH.cpp) / `NO_C99_MATH` | the float and long double functions of C99 in &lt;math.h&gt; | L; `include/ansi/cmath` |
| [CORRECT_ISO_CPP_WCHAR_H_PROTO](../../etc/config/src/CORRECT_ISO_CPP_WCHAR_H_PROTO.cpp) / `NO_CORRECT_ISO_CPP_WCHAR_H_PROTO` | glibc's C++ overloads in &lt;wchar.h&gt; compile | L; `include/ansi/cwchar`, `include/ansi/wchar.h` |
| [CTYPE_BITS](../../etc/config/src/CTYPE_BITS.cpp) / `NO_CTYPE_BITS` + output | ctype constants | none |
| [DAYLIGHT](../../etc/config/src/DAYLIGHT.cpp) / `NO_DAYLIGHT` | daylight variable in &lt;time.h&gt; | L; `src/time_put.cpp` |
| [DEPRECATED_LIBC_IN_STD](../../etc/config/src/DEPRECATED_LIBC_IN_STD.cpp) / `NO_DEPRECATED_LIBC_IN_STD` | deprecated C headers and namespace std | none |
| [DIV_OVERLOADS](../../etc/config/src/DIV_OVERLOADS.cpp) / `NO_DIV_OVERLOADS` + output | overloads of div() | none |
| [EQUAL_CTYPE_MASK](../../etc/config/src/EQUAL_CTYPE_MASK.cpp) / `NO_EQUAL_CTYPE_MASK` | that ctype mask's for char and wchar_t are equal | L; `src/wctype.cpp` |
| [EXCEPTION_ASSIGNMENT](../../etc/config/src/EXCEPTION_ASSIGNMENT.cpp) / `NO_EXCEPTION_ASSIGNMENT` | exception assignment operator | L; `src/exception.cpp` |
| [EXCEPTION_COPY_CTOR](../../etc/config/src/EXCEPTION_COPY_CTOR.cpp) / `NO_EXCEPTION_COPY_CTOR` | exception copy ctor | L; `src/exception.cpp` |
| [EXCEPTION_DEFAULT_CTOR](../../etc/config/src/EXCEPTION_DEFAULT_CTOR.cpp) / `NO_EXCEPTION_DEFAULT_CTOR` | exception default ctor | L; `src/exception.cpp` |
| [EXCEPTION_DTOR](../../etc/config/src/EXCEPTION_DTOR.cpp) / `NO_EXCEPTION_DTOR` | exception dtor | L; `src/exception.cpp` |
| [EXCEPTION_SPECIFICATION](../../etc/config/src/EXCEPTION_SPECIFICATION.cpp) / `NO_EXCEPTION_SPECIFICATION` | exception specification | L/P/T; `include/rw/_defs.h` |
| [EXCEPTION_SPECIFICATION_ON_NEW](../../etc/config/src/EXCEPTION_SPECIFICATION_ON_NEW.cpp) / `NO_EXCEPTION_SPECIFICATION_ON_NEW` | exception specification on new | L/P; `include/rw/_defs.h` |
| [EXCEPTION_WHAT](../../etc/config/src/EXCEPTION_WHAT.cpp) / `NO_EXCEPTION_WHAT` | exception::what() | L; `src/exception.cpp` |
| [EXPLICIT_INSTANTIATION_BEFORE_DEFINITION](../../etc/config/src/EXPLICIT_INSTANTIATION_BEFORE_DEFINITION.cpp) / `NO_EXPLICIT_INSTANTIATION_BEFORE_DEFINITION` | instantiation before definition | L; `include/rw/_defs.h` |
| [EXTERN_C_OVERLOAD](../../etc/config/src/EXTERN_C_OVERLOAD.cpp) / `NO_EXTERN_C_OVERLOAD` | overloading on extern "C" | L; `include/ansi/_cstdlib.h` |
| [EXTERN_FUNCTION_TEMPLATE](../../etc/config/src/EXTERN_FUNCTION_TEMPLATE.cpp) / `NO_EXTERN_FUNCTION_TEMPLATE` | extern function template extension | L/P; `include/rw/_defs.h` |
| [EXTERN_MEMBER_TEMPLATE](../../etc/config/src/EXTERN_MEMBER_TEMPLATE.cpp) / `NO_EXTERN_MEMBER_TEMPLATE` | extern template extension | L; `include/string` |
| [EXTERN_TEMPLATE](../../etc/config/src/EXTERN_TEMPLATE.cpp) / `NO_EXTERN_TEMPLATE` | extern template extension | L; `include/rw/_config-gcc.h` |
| [EXTERN_TEMPLATE_BEFORE_DEFINITION](../../etc/config/src/EXTERN_TEMPLATE_BEFORE_DEFINITION.cpp) / `NO_EXTERN_TEMPLATE_BEFORE_DEFINITION` | extern template before definition | L; `include/rw/_defs.h` |
| [FLOAT](../../etc/config/src/FLOAT.cpp) / `NO_FLOAT` + output | floating point properties | none |
| [FPOS_T](../../etc/config/src/FPOS_T.cpp) / `NO_FPOS_T` | fpos_t in &lt;stdio.h&gt; | L; `include/ansi/cstdio` |
| [FUNC](../../etc/config/src/FUNC.cpp) / `NO_FUNC` | the __func__ special macro | L/T; `include/ansi/_cassert.h` |
| [GLOBAL_BAD_ALLOC](../../etc/config/src/GLOBAL_BAD_ALLOC.cpp) / `NO_GLOBAL_BAD_ALLOC` | class ::bad_alloc | P; `etc/config/src/RUNTIME_IN_STD.cpp` |
| [GLOBAL_BAD_CAST](../../etc/config/src/GLOBAL_BAD_CAST.cpp) / `NO_GLOBAL_BAD_CAST` | class ::bad_cast | P; `etc/config/src/RUNTIME_IN_STD.cpp` |
| [GLOBAL_BAD_EXCEPTION](../../etc/config/src/GLOBAL_BAD_EXCEPTION.cpp) / `NO_GLOBAL_BAD_EXCEPTION` | class ::bad_exception | P; `etc/config/src/RUNTIME_IN_STD.cpp` |
| [GLOBAL_BAD_TYPEID](../../etc/config/src/GLOBAL_BAD_TYPEID.cpp) / `NO_GLOBAL_BAD_TYPEID` | class ::bad_typeid | P/T; `etc/config/src/RUNTIME_IN_STD.cpp` |
| [GLOBAL_EXCEPTION](../../etc/config/src/GLOBAL_EXCEPTION.cpp) / `NO_GLOBAL_EXCEPTION` | class ::exception | P; `etc/config/src/RUNTIME_IN_STD.cpp` |
| [GLOBAL_NOTHROW](../../etc/config/src/GLOBAL_NOTHROW.cpp) / `NO_GLOBAL_NOTHROW` | ::nothrow | L; `include/new` |
| [GLOBAL_NOTHROW_T](../../etc/config/src/GLOBAL_NOTHROW_T.cpp) / `NO_GLOBAL_NOTHROW_T` | ::nothrow_t | L; `include/new` |
| [GLOBAL_SET_NEW_HANDLER](../../etc/config/src/GLOBAL_SET_NEW_HANDLER.cpp) / `NO_GLOBAL_SET_NEW_HANDLER` | ::set_new_handler() | P; `etc/config/src/RUNTIME_IN_STD.cpp` |
| [GLOBAL_SET_TERMINATE](../../etc/config/src/GLOBAL_SET_TERMINATE.cpp) / `NO_GLOBAL_SET_TERMINATE` | ::set_terminate() | L/P; `src/exception.cpp` |
| [GLOBAL_SET_UNEXPECTED](../../etc/config/src/GLOBAL_SET_UNEXPECTED.cpp) / `NO_GLOBAL_SET_UNEXPECTED` | ::set_unexpected() | L/P; `src/exception.cpp` |
| [GLOBAL_TERMINATE](../../etc/config/src/GLOBAL_TERMINATE.cpp) / `NO_GLOBAL_TERMINATE` | ::terminate() | L/P; `src/exception.cpp` |
| [GLOBAL_TYPE_INFO](../../etc/config/src/GLOBAL_TYPE_INFO.cpp) / `NO_GLOBAL_TYPE_INFO` | class ::type_info | L/P; `src/typeinfo.cpp` |
| [GLOBAL_UNCAUGHT_EXCEPTION](../../etc/config/src/GLOBAL_UNCAUGHT_EXCEPTION.cpp) / `NO_GLOBAL_UNCAUGHT_EXCEPTION` | ::uncaught_exception() | L/P/T; `include/rw/_defs.h` |
| [GLOBAL_UNEXPECTED](../../etc/config/src/GLOBAL_UNEXPECTED.cpp) / `NO_GLOBAL_UNEXPECTED` | ::unexpected() | L/P; `src/exception.cpp` |
| [ICONV](../../etc/config/src/ICONV.cpp) / `NO_ICONV` + output | iconv() in &lt;iconv.h&gt; | V; `util/charmap.cpp` |
| [ICONV_CONST_CHAR](../../etc/config/src/ICONV_CONST_CHAR.cpp) / `NO_ICONV_CONST_CHAR` | POSIX iconv() | P/T/V; `etc/config/src/ICONV.cpp` |
| [INFINITY](../../etc/config/src/INFINITY.cpp) / `NO_INFINITY` + output | infinity and NaN's | L/P; `src/limits_bits.cpp` |
| [LCONV](../../etc/config/src/LCONV.cpp) / `NO_LCONV` + output | struct lconv in &lt;locale.h&gt; | none |
| [LCONV_INT_FMAT](../../etc/config/src/LCONV_INT_FMAT.cpp) / `NO_LCONV_INT_FMAT` | C99 international lconv members | L/P/T; `src/punct.cpp` |
| [LDBL_PRINTF_PREFIX](../../etc/config/src/LDBL_PRINTF_PREFIX.cpp) / `NO_LDBL_PRINTF_PREFIX` + output | long double printf format prefix | none |
| [LIBC_EXCEPTION_SPEC](../../etc/config/src/LIBC_EXCEPTION_SPEC.cpp) / `NO_LIBC_EXCEPTION_SPEC` | exception specification in libc | L/P; `include/rw/_defs.h` |
| [LIBC_IN_STD](../../etc/config/src/LIBC_IN_STD.cpp) / `NO_LIBC_IN_STD` | C library in namespace std | L/E; `include/rw/_config-gcc.h` |
| [LIMITS](../../etc/config/src/LIMITS.cpp) / `NO_LIMITS` + output | numerical limits | none |
| [LLONG_PRINTF_PREFIX](../../etc/config/src/LLONG_PRINTF_PREFIX.cpp) / `NO_LLONG_PRINTF_PREFIX` + output | long long printf format prefix | none |
| [LOCALE_NAME_FMAT](../../etc/config/src/LOCALE_NAME_FMAT.cpp) / `NO_LOCALE_NAME_FMAT` + output | combined locale name format | L; `src/locale_body.cpp` |
| [LONG_LONG](../../etc/config/src/LONG_LONG.cpp) / `NO_LONG_LONG` | long long | L/P/T; `include/rw/_config.h` |
| [MADVISE](../../etc/config/src/MADVISE.cpp) / `NO_MADVISE` | madvise() in &lt;sys/mman.h&gt; | L; `src/memattr.cpp` |
| [MATH_EXCEPTION](../../etc/config/src/MATH_EXCEPTION.cpp) / `NO_MATH_EXCEPTION` | struct exception in &lt;math.h&gt; | L; `include/ansi/cmath` |
| [MATH_OVERLOADS](../../etc/config/src/MATH_OVERLOADS.cpp) / `NO_MATH_OVERLOADS` + output | function overloads in &lt;math.h&gt; | none |
| [MBSTATE_T](../../etc/config/src/MBSTATE_T.cpp) / `NO_MBSTATE_T` + output | mbstate_t in &lt;wchar.h&gt; and &lt;wctype.h&gt; | L; `include/ansi/wchar.h` |
| [MMAP](../../etc/config/src/MMAP.cpp) / `NO_MMAP` | mmap() and munmap() in &lt;sys/mman.h&gt; | L; `src/mman.cpp` |
| [MUNMAP](../../etc/config/src/MUNMAP.cpp) / `NO_MUNMAP` + output | munmap() in &lt;sys/mman.h&gt; | L; `src/mman.cpp` |
| [NEWLINE](../../etc/config/src/NEWLINE.cpp) / `NO_NEWLINE` + output | newline (CR or CR-LF) | none |
| [NEW_HEADER](../../etc/config/src/NEW_HEADER.cpp) / `NO_NEW_HEADER` | C++-style C library headers | L/P/T; `include/rw/_config-gcc.h` |
| [NEW_OFLOW_SAFE](../../etc/config/src/NEW_OFLOW_SAFE.cpp) / `NO_NEW_OFLOW_SAFE` | if operator new() checks the argument for overflow | L/P; `src/memory.cpp` |
| [NEW_THROWS](../../etc/config/src/NEW_THROWS.cpp) / `NO_NEW_THROWS` | if operator new() throws | L/P/T; `src/iostore.cpp` |
| [NL_LANGINFO](../../etc/config/src/NL_LANGINFO.cpp) / `NO_NL_LANGINFO` | nl_langinfo() in &lt;langinfo.h&gt; | L/T/V; `src/time_put.cpp` |
| [NL_TYPES_H](../../etc/config/src/NL_TYPES_H.cpp) / `NO_NL_TYPES_H` | catopen() in &lt;nl_types.h&gt; | L; `src/catalog.cpp` |
| [NO_DBL_TRAPS](../../etc/config/src/NO_DBL_TRAPS.cpp) / `NO_DBL_TRAPS` | if floating point math traps | L/P/T; `include/limits` |
| [NO_INT_TRAPS](../../etc/config/src/NO_INT_TRAPS.cpp) / `NO_INT_TRAPS` | if integer math traps | L; `include/limits` |
| [NO_OBJECT_MANGLING](../../etc/config/src/NO_OBJECT_MANGLING.cpp) / `NO_OBJECT_MANGLING` | object name mangling | L; `include/limits` |
| [NO_SIGNALING_NAN](../../etc/config/src/NO_SIGNALING_NAN.cpp) / `NO_SIGNALING_NAN` | signaling NaN | L/T; `include/limits` |
| [OFFSETOF](../../etc/config/src/OFFSETOF.cpp) / `NO_OFFSETOF` | a working offsetof() in &lt;stddef.h&gt; | L/P; `src/time_put.cpp` |
| [OPERATOR_DELETE_ARRAY](../../etc/config/src/OPERATOR_DELETE_ARRAY.cpp) / `NO_OPERATOR_DELETE_ARRAY` | operator delete[] (void*) | L/T; `include/new` |
| [OPERATOR_DELETE_ARRAY_NOTHROW](../../etc/config/src/OPERATOR_DELETE_ARRAY_NOTHROW.cpp) / `NO_OPERATOR_DELETE_ARRAY_NOTHROW` | operator delete[] (void*, nothrow_t) | L/T; `include/new` |
| [OPERATOR_DELETE_ARRAY_PLACEMENT](../../etc/config/src/OPERATOR_DELETE_ARRAY_PLACEMENT.cpp) / `NO_OPERATOR_DELETE_ARRAY_PLACEMENT` | operator delete[] (void*, void*) | L; `include/new` |
| [OPERATOR_DELETE_NOTHROW](../../etc/config/src/OPERATOR_DELETE_NOTHROW.cpp) / `NO_OPERATOR_DELETE_NOTHROW` | operator delete (void*, nothrow_t) | L/T; `include/new` |
| [OPERATOR_DELETE_PLACEMENT](../../etc/config/src/OPERATOR_DELETE_PLACEMENT.cpp) / `NO_OPERATOR_DELETE_PLACEMENT` | operator delete (void*, void*) | L; `include/rw/_new.h` |
| [OPERATOR_NEW_ARRAY](../../etc/config/src/OPERATOR_NEW_ARRAY.cpp) / `NO_OPERATOR_NEW_ARRAY` | operator new[] (size_t) | L/T; `include/new` |
| [OPERATOR_NEW_ARRAY_NOTHROW](../../etc/config/src/OPERATOR_NEW_ARRAY_NOTHROW.cpp) / `NO_OPERATOR_NEW_ARRAY_NOTHROW` | operator new[] (size_t, nothrow_t) | L/T; `include/new` |
| [OPERATOR_NEW_ARRAY_PLACEMENT](../../etc/config/src/OPERATOR_NEW_ARRAY_PLACEMENT.cpp) / `NO_OPERATOR_NEW_ARRAY_PLACEMENT` | operator new[] (size_t, void*) | L; `include/new` |
| [OPERATOR_NEW_NOTHROW](../../etc/config/src/OPERATOR_NEW_NOTHROW.cpp) / `NO_OPERATOR_NEW_NOTHROW` | operator new (size_t, nothrow_t) | L/T; `include/new` |
| [OPERATOR_NEW_PLACEMENT](../../etc/config/src/OPERATOR_NEW_PLACEMENT.cpp) / `NO_OPERATOR_NEW_PLACEMENT` | operator new (size_t, void*) | L; `include/rw/_new.h` |
| [POSIX_MADVISE](../../etc/config/src/POSIX_MADVISE.cpp) / `NO_POSIX_MADVISE` | posix_madvise() in &lt;sys/mman.h&gt; | none |
| [PRETTY_FUNCTION](../../etc/config/src/PRETTY_FUNCTION.cpp) / `NO_PRETTY_FUNCTION` | __PRETTY_FUNCTION__ | L/T; `include/rw/_defs.h` |
| [PTR_EXCEPTION_SPEC](../../etc/config/src/PTR_EXCEPTION_SPEC.cpp) / `NO_PTR_EXCEPTION_SPEC` | pointer exception specification | T; `tests/diagnostics/19.std.exceptions.cpp` |
| [PUTENV_CONST_CHAR](../../etc/config/src/PUTENV_CONST_CHAR.cpp) / `NO_PUTENV_CONST_CHAR` | putenv() in &lt;stdlib.h&gt; | T; `tests/src/environ.cpp` |
| [QUIET_NAN](../../etc/config/src/QUIET_NAN.cpp) / `NO_QUIET_NAN` | quiet NaN | L; `include/limits` |
| [RUNTIME_IN_STD](../../etc/config/src/RUNTIME_IN_STD.cpp) / `NO_RUNTIME_IN_STD` + output | runtime support in namespace std | L/P/T; `include/exception` |
| [SETLOCALE](../../etc/config/src/SETLOCALE.cpp) / `NO_SETLOCALE` | setlocale() in &lt;locale.h&gt; | none |
| [SETRLIMIT](../../etc/config/src/SETRLIMIT.cpp) / `NO_SETRLIMIT` | setrlimit() in &lt;sys/resource.h&gt; | P/T; `etc/config/src/GLOBAL_BAD_ALLOC.cpp` |
| [SIG_ATOMIC_T](../../etc/config/src/SIG_ATOMIC_T.cpp) / `NO_SIG_ATOMIC_T` + output | signal support in &lt;signal.h&gt; | none |
| [SIZE_T](../../etc/config/src/SIZE_T.cpp) / `NO_SIZE_T` + output | fpos_t, ptrdiff_t, and size_t | L; `include/ansi/cwchar` |
| [SNPRINTF](../../etc/config/src/SNPRINTF.cpp) / `NO_SNPRINTF` | snprintf() in &lt;stdio.h&gt; | L; `src/num_put.cpp` |
| [STATICS_IN_TEMPLATE](../../etc/config/src/STATICS_IN_TEMPLATE.cpp) / `NO_STATICS_IN_TEMPLATE` | references to static symbols in template | L; `include/rw/_defs.h` |
| [STD_BAD_ALLOC](../../etc/config/src/STD_BAD_ALLOC.cpp) / `NO_STD_BAD_ALLOC` | class std::bad_alloc | L/P/T; `include/new` |
| [STD_BAD_CAST](../../etc/config/src/STD_BAD_CAST.cpp) / `NO_STD_BAD_CAST` | class std::bad_cast | L/P/T; `include/typeinfo` |
| [STD_BAD_EXCEPTION](../../etc/config/src/STD_BAD_EXCEPTION.cpp) / `NO_STD_BAD_EXCEPTION` | class std::bad_exception | L/P; `include/exception` |
| [STD_BAD_TYPEID](../../etc/config/src/STD_BAD_TYPEID.cpp) / `NO_STD_BAD_TYPEID` | class std::bad_typeid | L/P/T; `include/typeinfo` |
| [STD_EXCEPTION](../../etc/config/src/STD_EXCEPTION.cpp) / `NO_STD_EXCEPTION` | class std::exception | L/P; `include/exception` |
| [STD_MBSTATE_T](../../etc/config/src/STD_MBSTATE_T.cpp) / `NO_STD_MBSTATE_T` | std::mbstate_t in &lt;wchar.h&gt; and &lt;wctype.h&gt; | L; `include/ansi/cwchar` |
| [STD_NOTHROW](../../etc/config/src/STD_NOTHROW.cpp) / `NO_STD_NOTHROW` | std::nothrow | L; `include/new` |
| [STD_NOTHROW_T](../../etc/config/src/STD_NOTHROW_T.cpp) / `NO_STD_NOTHROW_T` | std::nothrow_t | L/P; `include/new` |
| [STD_SET_NEW_HANDLER](../../etc/config/src/STD_SET_NEW_HANDLER.cpp) / `NO_STD_SET_NEW_HANDLER` | std::set_new_handler() | L/P; `include/new` |
| [STD_SET_TERMINATE](../../etc/config/src/STD_SET_TERMINATE.cpp) / `NO_STD_SET_TERMINATE` | std::set_terminate() | L/P; `src/exception.cpp` |
| [STD_SET_UNEXPECTED](../../etc/config/src/STD_SET_UNEXPECTED.cpp) / `NO_STD_SET_UNEXPECTED` | std::set_unexpected() | L/P; `src/exception.cpp` |
| [STD_TERMINATE](../../etc/config/src/STD_TERMINATE.cpp) / `NO_STD_TERMINATE` | std::terminate() | L/P; `src/exception.cpp` |
| [STD_TYPE_INFO](../../etc/config/src/STD_TYPE_INFO.cpp) / `NO_STD_TYPE_INFO` | class std::type_info | L/P; `include/typeinfo` |
| [STD_UNCAUGHT_EXCEPTION](../../etc/config/src/STD_UNCAUGHT_EXCEPTION.cpp) / `NO_STD_UNCAUGHT_EXCEPTION` | std::uncaught_exception() | L/P/T; `include/rw/_defs.h` |
| [STD_UNEXPECTED](../../etc/config/src/STD_UNEXPECTED.cpp) / `NO_STD_UNEXPECTED` | std::unexpected() | L/P; `src/exception.cpp` |
| [STRTOF](../../etc/config/src/STRTOF.cpp) / `NO_STRTOF` | strtof() in &lt;stdlib.h&gt; | L; `src/num_get.cpp` |
| [STRTOLD](../../etc/config/src/STRTOLD.cpp) / `NO_STRTOLD` | strtold() in &lt;stdlib.h&gt; | L; `src/num_get.cpp` |
| [STRUCT_TM](../../etc/config/src/STRUCT_TM.cpp) / `NO_STRUCT_TM` + output | struct tm in &lt;time.h&gt; | L; `include/ansi/cwchar` |
| [STRUCT_TM_IN_WCHAR_H](../../etc/config/src/STRUCT_TM_IN_WCHAR_H.cpp) / `NO_STRUCT_TM_IN_WCHAR_H` | struct tm declaration in &lt;wchar.h&gt; | L; `include/ansi/cwchar` |
| [SWPRINTF](../../etc/config/src/SWPRINTF.cpp) / `NO_SWPRINTF` | swprintf() in &lt;wchar.h&gt; | L; `src/time_put.cpp` |
| [THREAD_SAFE_EXCEPTIONS](../../etc/config/src/THREAD_SAFE_EXCEPTIONS.cpp) / `NO_THREAD_SAFE_EXCEPTIONS` | if exceptions are thread safe | none |
| [TIMEZONE](../../etc/config/src/TIMEZONE.cpp) / `NO_TIMEZONE` | int timezone in &lt;time.h&gt; | L; `src/time_put.cpp` |
| [TLS](../../etc/config/src/TLS.cpp) / `NO_TLS` + output | thread-local storage | L; `include/rw/_defs.h` |
| [TM_GMTOFF](../../etc/config/src/TM_GMTOFF.cpp) / `NO_TM_GMTOFF` | tm_gmtoff in struct tm in &lt;time.h&gt; | L/T; `src/time_put.cpp` |
| [TYPE_INFO_BEFORE](../../etc/config/src/TYPE_INFO_BEFORE.cpp) / `NO_TYPE_INFO_BEFORE` | type_info::before() | L; `src/typeinfo.cpp` |
| [TYPE_INFO_DTOR](../../etc/config/src/TYPE_INFO_DTOR.cpp) / `NO_TYPE_INFO_DTOR` | type_info dtor | L/P; `src/typeinfo.cpp` |
| [TYPE_INFO_EQUALITY](../../etc/config/src/TYPE_INFO_EQUALITY.cpp) / `NO_TYPE_INFO_EQUALITY` | type_info::operator==() | L; `src/typeinfo.cpp` |
| [TYPE_INFO_INEQUALITY](../../etc/config/src/TYPE_INFO_INEQUALITY.cpp) / `NO_TYPE_INFO_INEQUALITY` | type_info::operator!=() | L; `src/typeinfo.cpp` |
| [TYPE_INFO_NAME](../../etc/config/src/TYPE_INFO_NAME.cpp) / `NO_TYPE_INFO_NAME` | type_info::name() | L; `src/typeinfo.cpp` |
| [UNAME](../../etc/config/src/UNAME.cpp) / `NO_UNAME` + output | OS name and version | none |
| [UNISTD_DECL](../../etc/config/src/UNISTD_DECL.cpp) / `NO_UNISTD_DECL` + output | the contents of &lt;unistd.h&gt; | none |
| [VA_LIST](../../etc/config/src/VA_LIST.cpp) / `NO_VA_LIST` + output | the type of va_list | none |
| [VSNPRINTF](../../etc/config/src/VSNPRINTF.cpp) / `NO_VSNPRINTF` | vsnprintf() in &lt;stdio.h&gt; | L; `src/exception.cpp` |
| [VSNPRINTF_RETURN](../../etc/config/src/VSNPRINTF_RETURN.cpp) / `NO_VSNPRINTF_RETURN` + output | vsnprintf() return value | none |
| [WCTRANS_T](../../etc/config/src/WCTRANS_T.cpp) / `NO_WCTRANS_T` + output | wctrans_t in &lt;wchar.h&gt; and &lt;wctype.h&gt; | L; `include/ansi/cwctype` |
| [WCTYPE_T](../../etc/config/src/WCTYPE_T.cpp) / `NO_WCTYPE_T` + output | wctype_t in &lt;wchar.h&gt; and &lt;wctype.h&gt; | L; `include/ansi/cwctype` |
| [WINT_T](../../etc/config/src/WINT_T.cpp) / `NO_WINT_T` + output | wint_t in &lt;wchar.h&gt; and &lt;wctype.h&gt; | L/P; `include/ansi/_cwchar.h` |

## 5. Support files and scope

The remaining C++ sources supply cooperating translation units or
libraries. The `.h`, `.c`, `.cc` and `.inc` files support those probes
and the shell driver; they are not independently counted capabilities.
The complete support-file inventory is:

`collapse_template_statics.lib.cpp`, `extern_function_template_imp.cpp`, `extern_function_template_imp.h`, `extern_template_before_definition_imp.cpp`, `extern_template_imp.cpp`, `extern_template_imp.h`, `float_defs.h`, `headers.inc`, `instantiation_before_definition.cc`, `instantiation_before_definition.h`, `locale_names.h`, `nodbg.h`, `object_mangling_imp.cpp`, `proclimits.h`, `terminate.h`, `thread.h`, `types.h`.

Descriptions and exact-token references provide a survey, not a proof
that every probe still asks the best question. The individually
reviewed cases in chapter 2 illustrate where a probe needs a closer
reading than its name suggests. Removing probes
requires examining their consumers, emitted families and dependencies
together.

Written by OpenAI LeChuck.
Brought up to date with the tree of 2026-09-27 by Claude.
