# The characterization survey

## 0. tl;dr

Configuration asks three different questions: **what language machinery
works, what the runtime and C library provide, and how the target
represents data**. The supported compilers answer the first group
the same way; the tree keeps the probes and their branches, since it
stays C++98 and the branches are its portability record. Sizes,
layouts, calling interfaces and runtime behavior are platform
questions.

The source census contains 205 named C++ probes, nine supporting C++
translation units, one shell driver and 15 other support files. The
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
three different things.** All 205 named probes are discoverable by
the current configuration rule. The catalogue's references answer
the second question conservatively: where the exact macro token is
read outside its producer. Determining whether a conditional branch
is compiled requires following configuration overrides and surrounding
conditions as well. No preprocessing or build was performed here.

| group | status on the supported compilers |
|---|---|
| Old language deficiencies: `BOOL`, `NAMESPACE`, `EXPLICIT`, `TYPENAME`, casts, member templates, partial specialization | Every probe passes in the six configurations. Passing today is not a reason to remove a probe or its branch. |
| Obsolete mechanisms: exported templates, implicit inclusion, dynamic exception specifications | The probes fail in the six configurations, and that is their job: a failing probe selects the path the compiler's dialect needs. |
| Runtime ABI: exception classes, allocation functions, RTTI members and namespace placement | Questions about the runtime, not the language. A historical ABI probe is not a description of public API support. |
| Platform facts: sizes, typedefs, floating point, C structures, conversion interfaces, locale naming | Retain characterization or deliberately replace it with an equally sound source of the information. |
| No direct reader of a status macro | Inspect emitted values and other probe dependencies before considering retirement. A status marker can be unused while the producer remains essential. |

### 2.1 Cases that deserve individual treatment

`ATOMIC_OPS.cpp` currently has a `main` that simply returns zero.
The vendor-backend retirement left it as a success stub. Its
commented-out `_RWSTD_NO_ATOMIC_OPS` **does not characterize the GNU
`__sync` built-ins** the read-modify-write backend uses; five probes
do, one per width. `CHAR_ATOMIC_OPS.cpp`, `SHORT_ATOMIC_OPS.cpp`,
`INT_ATOMIC_OPS.cpp`, `LONG_ATOMIC_OPS.cpp` and `LLONG_ATOMIC_OPS.cpp`
run the three built-ins `include/rw/_atomic-sync.h` uses on a value
that carries across every byte of the type, compiled, linked without
libatomic and run. `include/rw/_atomic.h` selects the backend when
the `int` probe passes, and the backend leaves out each width whose
probe failed. Older investigation prose describing the former Windows
probe no longer describes this source.

`ATOMIC_BUILTINS.cpp` is the newest probe and the one that does
characterize atomics: a release store and an acquire load of a
`size_t` through the `__atomic` built-ins, compiled, linked without
`libatomic` and run. Its answer gates the ordered load and store
macros in `include/rw/_defs.h`, which publish facet data and facets
and fill the scalar caches of `ctype` and `codecvt`.

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

## 3. Outputs beyond the status macro

All macro names below have the `_RWSTD_` prefix. Patterns identify
families whose individual members depend on the producer.

| producer | information emitted |
|---|---|
| `libc_decl.sh` | Header availability and native paths (`NO_*_H`, `ANSI_C_*_H`). Names are generated from its header list. Until a44256a6 it also probed every function's declaration and link availability; `history-libc-declarations.md` records how. |
| `LIMITS` | Integer bounds, sizes, fixed/least-width types, character width, multibyte limit and two's-complement answer. |
| `FLOAT`, `INFINITY` | Floating-point constants, special-value byte representations, denormal support and string-to-floating underflow behavior. |
| `SIZE_T`, `SIG_ATOMIC_T`, `WINT_T`, `WCTYPE_T`, `WCTRANS_T`, `VA_LIST`, `UNISTD_DECL` | Underlying typedefs, ranges, signal/stdio constants, `va_list` shape and POSIX types/constants. |
| `STRUCT_TM`, `LCONV`, `MBSTATE_T` | C structure layout declarations or storage sizes. |
| `CTYPE_BITS`, `LOCALE_NAME_FMAT` | C character-class masks, locale category values, separators and composite-name conventions. |
| `ABS_OVERLOADS`, `DIV_OVERLOADS`, `MATH_OVERLOADS` | Per-type overload availability, in addition to the probe's own status marker. |
| `ICONV`, `MUNMAP`, `WCSFTIME_WCHAR_T_FMAT` | Argument types for C interfaces. |
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
| [ATOMIC_BUILTINS](../../etc/config/src/ATOMIC_BUILTINS.cpp) / `NO_ATOMIC_BUILTINS` | `__atomic` release store and acquire load on `size_t` | L; `include/rw/_defs.h` |
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
| [BOOL](../../etc/config/src/BOOL.cpp) / `NO_BOOL` | bool, false, and true keywords | L/P/T/E; `include/limits` |
| [CLASS_PARTIAL_SPEC](../../etc/config/src/CLASS_PARTIAL_SPEC.cpp) / `NO_CLASS_PARTIAL_SPEC` | partial template specialization | L/P/T/E; `include/algorithm` |
| [COLLAPSE_STATIC_LOCALS](../../etc/config/src/COLLAPSE_STATIC_LOCALS.cpp) / `NO_COLLAPSE_STATIC_LOCALS` | static locals in inline code | none |
| [COLLAPSE_TEMPLATE_LOCALS](../../etc/config/src/COLLAPSE_TEMPLATE_LOCALS.cpp) / `NO_COLLAPSE_TEMPLATE_LOCALS` | static locals in template code | none |
| [COLLAPSE_TEMPLATE_STATICS](../../etc/config/src/COLLAPSE_TEMPLATE_STATICS.cpp) / `NO_COLLAPSE_TEMPLATE_STATICS` | static template members | L; `include/rw/_config.h` |
| [CONST_CAST](../../etc/config/src/CONST_CAST.cpp) / `NO_CONST_CAST` | const_cast | none |
| [CTYPE_BITS](../../etc/config/src/CTYPE_BITS.cpp) / `NO_CTYPE_BITS` + output | ctype constants | none |
| [CV_VOID_SPECIALIZATIONS](../../etc/config/src/CV_VOID_SPECIALIZATIONS.cpp) / `NO_CV_VOID_SPECIALIZATIONS` | cv qualifiers on type void | L; `include/rw/_autoptr.h` |
| [DAYLIGHT](../../etc/config/src/DAYLIGHT.cpp) / `NO_DAYLIGHT` | daylight variable in &lt;time.h&gt; | L; `src/time_put.cpp` |
| [DEFAULT_TEMPLATE_ARGS](../../etc/config/src/DEFAULT_TEMPLATE_ARGS.cpp) / `NO_DEFAULT_TEMPLATE_ARGS` | default template arguments | L/T; `include/rw/_defs.h` |
| [DEPENDENT_TEMPLATE](../../etc/config/src/DEPENDENT_TEMPLATE.cpp) / `NO_DEPENDENT_TEMPLATE` | dependent template | L; `include/rw/_defs.h` |
| [DEPRECATED_LIBC_IN_STD](../../etc/config/src/DEPRECATED_LIBC_IN_STD.cpp) / `NO_DEPRECATED_LIBC_IN_STD` | deprecated C headers and namespace std | none |
| [DIV_OVERLOADS](../../etc/config/src/DIV_OVERLOADS.cpp) / `NO_DIV_OVERLOADS` + output | overloads of div() | none |
| [DUMMY_DEFAULT_ARG](../../etc/config/src/DUMMY_DEFAULT_ARG.cpp) / `NO_DUMMY_DEFAULT_ARG` | dummy default arguments | L; `include/rw/_defs.h` |
| [DYNAMIC_CAST](../../etc/config/src/DYNAMIC_CAST.cpp) / `NO_DYNAMIC_CAST` | dynamic_cast | L/T; `include/rw/_defs.h` |
| [EMPTY_MEM_INITIALIZER](../../etc/config/src/EMPTY_MEM_INITIALIZER.cpp) / `NO_EMPTY_MEM_INITIALIZER` | empty mem-initializer arglist | L/T; `include/rw/_pair.h` |
| [EQUAL_CTYPE_MASK](../../etc/config/src/EQUAL_CTYPE_MASK.cpp) / `NO_EQUAL_CTYPE_MASK` | that ctype mask's for char and wchar_t are equal | L; `src/wctype.cpp` |
| [EXCEPTIONS](../../etc/config/src/EXCEPTIONS.cpp) / `NO_EXCEPTIONS` | exceptions | L/P/T/E; `include/rw/_config-gcc.h` |
| [EXCEPTION_ASSIGNMENT](../../etc/config/src/EXCEPTION_ASSIGNMENT.cpp) / `NO_EXCEPTION_ASSIGNMENT` | exception assignment operator | L; `src/exception.cpp` |
| [EXCEPTION_COPY_CTOR](../../etc/config/src/EXCEPTION_COPY_CTOR.cpp) / `NO_EXCEPTION_COPY_CTOR` | exception copy ctor | L; `src/exception.cpp` |
| [EXCEPTION_DEFAULT_CTOR](../../etc/config/src/EXCEPTION_DEFAULT_CTOR.cpp) / `NO_EXCEPTION_DEFAULT_CTOR` | exception default ctor | L; `src/exception.cpp` |
| [EXCEPTION_DTOR](../../etc/config/src/EXCEPTION_DTOR.cpp) / `NO_EXCEPTION_DTOR` | exception dtor | L; `src/exception.cpp` |
| [EXCEPTION_SPECIFICATION](../../etc/config/src/EXCEPTION_SPECIFICATION.cpp) / `NO_EXCEPTION_SPECIFICATION` | exception specification | L/P/T; `include/rw/_defs.h` |
| [EXCEPTION_SPECIFICATION_ON_NEW](../../etc/config/src/EXCEPTION_SPECIFICATION_ON_NEW.cpp) / `NO_EXCEPTION_SPECIFICATION_ON_NEW` | exception specification on new | L/P; `include/rw/_defs.h` |
| [EXCEPTION_WHAT](../../etc/config/src/EXCEPTION_WHAT.cpp) / `NO_EXCEPTION_WHAT` | exception::what() | L; `src/exception.cpp` |
| [EXPLICIT](../../etc/config/src/EXPLICIT.cpp) / `NO_EXPLICIT` | explicit keyword | L/T; `include/rw/_defs.h` |
| [EXPLICIT_ARG](../../etc/config/src/EXPLICIT_ARG.cpp) / `NO_EXPLICIT_ARG` | explicit function template arguments | none |
| [EXPLICIT_CTOR_INSTANTIATION](../../etc/config/src/EXPLICIT_CTOR_INSTANTIATION.cpp) / `NO_EXPLICIT_CTOR_INSTANTIATION` | explicit instantiation of ctors | T; `tests/utilities/20.pairs.cpp` |
| [EXPLICIT_FUNC_INSTANTIATION](../../etc/config/src/EXPLICIT_FUNC_INSTANTIATION.cpp) / `NO_EXPLICIT_FUNC_INSTANTIATION` | explicit function instantiation | L/P; `src/collate.cpp` |
| [EXPLICIT_INSTANTIATION](../../etc/config/src/EXPLICIT_INSTANTIATION.cpp) / `NO_EXPLICIT_INSTANTIATION` | explicit instantiation | L/P/T; `include/rw/_defs.h` |
| [EXPLICIT_INSTANTIATION_BEFORE_DEFINITION](../../etc/config/src/EXPLICIT_INSTANTIATION_BEFORE_DEFINITION.cpp) / `NO_EXPLICIT_INSTANTIATION_BEFORE_DEFINITION` | instantiation before definition | L; `include/rw/_defs.h` |
| [EXPLICIT_INSTANTIATION_WITH_IMPLICIT_INCLUSION](../../etc/config/src/EXPLICIT_INSTANTIATION_WITH_IMPLICIT_INCLUSION.cpp) / `NO_EXPLICIT_INSTANTIATION_WITH_IMPLICIT_INCLUSION` | explicit instantiation with implicit inclusion | L; `include/rw/_defs.h` |
| [EXPLICIT_MEMBER_INSTANTIATION](../../etc/config/src/EXPLICIT_MEMBER_INSTANTIATION.cpp) / `NO_EXPLICIT_MEMBER_INSTANTIATION` | explicit instantiation of members | none |
| [EXPLICIT_MEMBER_SPECIALIZATION](../../etc/config/src/EXPLICIT_MEMBER_SPECIALIZATION.cpp) / `NO_EXPLICIT_MEMBER_SPECIALIZATION` | explicit member specialization | L; `include/rw/_defs.h` |
| [EXTERN_C_COMPATIBILITY](../../etc/config/src/EXTERN_C_COMPATIBILITY.cpp) / `NO_EXTERN_C_COMPATIBILITY` | compatibility of extern "C" and "C++" | L; `include/ansi/_cstdlib.h` |
| [EXTERN_C_EXCEPTIONS](../../etc/config/src/EXTERN_C_EXCEPTIONS.cpp) / `NO_EXTERN_C_EXCEPTIONS` | exceptions from extern "C" functions | none |
| [EXTERN_C_OVERLOAD](../../etc/config/src/EXTERN_C_OVERLOAD.cpp) / `NO_EXTERN_C_OVERLOAD` | overloading on extern "C" | L; `include/ansi/_cstdlib.h` |
| [EXTERN_FUNCTION_TEMPLATE](../../etc/config/src/EXTERN_FUNCTION_TEMPLATE.cpp) / `NO_EXTERN_FUNCTION_TEMPLATE` | extern function template extension | L/P; `include/rw/_defs.h` |
| [EXTERN_INLINE](../../etc/config/src/EXTERN_INLINE.cpp) / `NO_EXTERN_INLINE` | truly extern inline | none |
| [EXTERN_MEMBER_TEMPLATE](../../etc/config/src/EXTERN_MEMBER_TEMPLATE.cpp) / `NO_EXTERN_MEMBER_TEMPLATE` | extern template extension | L; `include/string` |
| [EXTERN_TEMPLATE](../../etc/config/src/EXTERN_TEMPLATE.cpp) / `NO_EXTERN_TEMPLATE` | extern template extension | L; `include/rw/_config-gcc.h` |
| [EXTERN_TEMPLATE_BEFORE_DEFINITION](../../etc/config/src/EXTERN_TEMPLATE_BEFORE_DEFINITION.cpp) / `NO_EXTERN_TEMPLATE_BEFORE_DEFINITION` | extern template before definition | L; `include/rw/_defs.h` |
| [FLOAT](../../etc/config/src/FLOAT.cpp) / `NO_FLOAT` + output | floating point properties | none |
| [FPOS_T](../../etc/config/src/FPOS_T.cpp) / `NO_FPOS_T` | fpos_t in &lt;stdio.h&gt; | L; `include/ansi/cstdio` |
| [FRIEND_TEMPLATE](../../etc/config/src/FRIEND_TEMPLATE.cpp) / `NO_FRIEND_TEMPLATE` | friend templates of templates | L; `include/rw/_stringio.cc` |
| [FUNC](../../etc/config/src/FUNC.cpp) / `NO_FUNC` | the __func__ special macro | L/T; `include/ansi/_cassert.h` |
| [FUNCTION_TRY_BLOCK](../../etc/config/src/FUNCTION_TRY_BLOCK.cpp) / `NO_FUNCTION_TRY_BLOCK` | function-try-block | none |
| [FUNC_PARTIAL_SPEC](../../etc/config/src/FUNC_PARTIAL_SPEC.cpp) / `NO_FUNC_PARTIAL_SPEC` | function template overload | L; `include/ostream` |
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
| [HONOR_STD](../../etc/config/src/HONOR_STD.cpp) / `NO_HONOR_STD` | if namespace std is honored | L/P/T; `include/ansi/cctype` |
| [ICONV](../../etc/config/src/ICONV.cpp) / `NO_ICONV` + output | iconv() in &lt;iconv.h&gt; | V; `util/charmap.cpp` |
| [ICONV_CONST_CHAR](../../etc/config/src/ICONV_CONST_CHAR.cpp) / `NO_ICONV_CONST_CHAR` | POSIX iconv() | P/T/V; `etc/config/src/ICONV.cpp` |
| [IMPLICIT_INSTANTIATION](../../etc/config/src/IMPLICIT_INSTANTIATION.cpp) / `NO_IMPLICIT_INSTANTIATION` | implicit instantiation | L; `include/rw/_defs.h` |
| [INFINITY](../../etc/config/src/INFINITY.cpp) / `NO_INFINITY` + output | infinity and NaN's | L/P; `src/limits_bits.cpp` |
| [INSTANTIATE_DEFAULT_ARGS](../../etc/config/src/INSTANTIATE_DEFAULT_ARGS.cpp) / `NO_INSTANTIATE_DEFAULT_ARGS` | if default args are instantiated | none |
| [LCONV](../../etc/config/src/LCONV.cpp) / `NO_LCONV` + output | struct lconv in &lt;locale.h&gt; | none |
| [LCONV_INT_FMAT](../../etc/config/src/LCONV_INT_FMAT.cpp) / `NO_LCONV_INT_FMAT` | C99 international lconv members | L/P/T; `src/punct.cpp` |
| [LDBL_PRINTF_PREFIX](../../etc/config/src/LDBL_PRINTF_PREFIX.cpp) / `NO_LDBL_PRINTF_PREFIX` + output | long double printf format prefix | none |
| [LIBC_EXCEPTION_SPEC](../../etc/config/src/LIBC_EXCEPTION_SPEC.cpp) / `NO_LIBC_EXCEPTION_SPEC` | exception specification in libc | L/P; `include/rw/_defs.h` |
| [LIBC_IN_STD](../../etc/config/src/LIBC_IN_STD.cpp) / `NO_LIBC_IN_STD` | C library in namespace std | L/E; `include/rw/_config-gcc.h` |
| [LIB_EXCEPTIONS](../../etc/config/src/LIB_EXCEPTIONS.cpp) / `NO_LIB_EXCEPTIONS` | library exceptions | none |
| [LIMITS](../../etc/config/src/LIMITS.cpp) / `NO_LIMITS` + output | numerical limits | none |
| [LLONG_PRINTF_PREFIX](../../etc/config/src/LLONG_PRINTF_PREFIX.cpp) / `NO_LLONG_PRINTF_PREFIX` + output | long long printf format prefix | none |
| [LOCALE_NAME_FMAT](../../etc/config/src/LOCALE_NAME_FMAT.cpp) / `NO_LOCALE_NAME_FMAT` + output | combined locale name format | L; `src/locale_body.cpp` |
| [LONG_DOUBLE](../../etc/config/src/LONG_DOUBLE.cpp) / `NO_LONG_DOUBLE` | if long double is a native type | L/P/T; `include/ansi/cmath` |
| [LONG_LONG](../../etc/config/src/LONG_LONG.cpp) / `NO_LONG_LONG` | long long | L/P/T; `include/rw/_config.h` |
| [MADVISE](../../etc/config/src/MADVISE.cpp) / `NO_MADVISE` | madvise() in &lt;sys/mman.h&gt; | L; `src/memattr.cpp` |
| [MATH_EXCEPTION](../../etc/config/src/MATH_EXCEPTION.cpp) / `NO_MATH_EXCEPTION` | struct exception in &lt;math.h&gt; | L; `include/ansi/cmath` |
| [MATH_OVERLOADS](../../etc/config/src/MATH_OVERLOADS.cpp) / `NO_MATH_OVERLOADS` + output | function overloads in &lt;math.h&gt; | none |
| [MBSTATE_T](../../etc/config/src/MBSTATE_T.cpp) / `NO_MBSTATE_T` + output | mbstate_t in &lt;wchar.h&gt; and &lt;wctype.h&gt; | L; `include/ansi/wchar.h` |
| [MEMBER_TEMPLATES](../../etc/config/src/MEMBER_TEMPLATES.cpp) / `NO_MEMBER_TEMPLATES` | member templates | L/P/T/E; `include/bitset` |
| [MEMBER_TEMPLATE_OVERLOAD](../../etc/config/src/MEMBER_TEMPLATE_OVERLOAD.cpp) / `NO_MEMBER_TEMPLATE_OVERLOAD` | member template overloads on return type | L; `include/bitset` |
| [MMAP](../../etc/config/src/MMAP.cpp) / `NO_MMAP` | mmap() and munmap() in &lt;sys/mman.h&gt; | L; `src/mman.cpp` |
| [MUNMAP](../../etc/config/src/MUNMAP.cpp) / `NO_MUNMAP` + output | munmap() in &lt;sys/mman.h&gt; | L; `src/mman.cpp` |
| [NAMESPACE](../../etc/config/src/NAMESPACE.cpp) / `NO_NAMESPACE` | namespaces | L/P/T/E; `include/ansi/cctype` |
| [NATIVE_WCHAR_T](../../etc/config/src/NATIVE_WCHAR_T.cpp) / `NO_NATIVE_WCHAR_T` | if wchar_t is a native type | L/P/T; `include/limits` |
| [NESTED_CLASS_ACCESS](../../etc/config/src/NESTED_CLASS_ACCESS.cpp) / `NO_NESTED_CLASS_ACCESS` | member class access | L/T; `include/istream` |
| [NEWLINE](../../etc/config/src/NEWLINE.cpp) / `NO_NEWLINE` + output | newline (CR or CR-LF) | none |
| [NEW_CLASS_TEMPLATE_SYNTAX](../../etc/config/src/NEW_CLASS_TEMPLATE_SYNTAX.cpp) / `NO_NEW_CLASS_TEMPLATE_SYNTAX` | class template specialization | L/P; `include/rw/_defs.h` |
| [NEW_FUNC_TEMPLATE_SYNTAX](../../etc/config/src/NEW_FUNC_TEMPLATE_SYNTAX.cpp) / `NO_NEW_FUNC_TEMPLATE_SYNTAX` | template specialization syntax | L; `include/rw/_defs.h` |
| [NEW_HEADER](../../etc/config/src/NEW_HEADER.cpp) / `NO_NEW_HEADER` | C++-style C library headers | L/P/T; `include/rw/_config-gcc.h` |
| [NEW_OFLOW_SAFE](../../etc/config/src/NEW_OFLOW_SAFE.cpp) / `NO_NEW_OFLOW_SAFE` | if operator new() checks the argument for overflow | L/P; `src/memory.cpp` |
| [NEW_THROWS](../../etc/config/src/NEW_THROWS.cpp) / `NO_NEW_THROWS` | if operator new() throws | L/P/T; `src/iostore.cpp` |
| [NL_LANGINFO](../../etc/config/src/NL_LANGINFO.cpp) / `NO_NL_LANGINFO` | nl_langinfo() in &lt;langinfo.h&gt; | L/T/V; `src/time_put.cpp` |
| [NL_TYPES_H](../../etc/config/src/NL_TYPES_H.cpp) / `NO_NL_TYPES_H` | catopen() in &lt;nl_types.h&gt; | L; `src/catalog.cpp` |
| [NONCLASS_ARROW_RETURN](../../etc/config/src/NONCLASS_ARROW_RETURN.cpp) / `NO_NONCLASS_ARROW_RETURN` | operator-&gt;() | L/T/E; `include/rw/_autoptr.h` |
| [NONDEDUCED_CONTEXT](../../etc/config/src/NONDEDUCED_CONTEXT.cpp) / `NO_NONDEDUCED_CONTEXT` | nondeduced context | L; `include/bitset` |
| [NO_DBL_TRAPS](../../etc/config/src/NO_DBL_TRAPS.cpp) / `NO_DBL_TRAPS` | if floating point math traps | L/P/T; `include/limits` |
| [NO_FOR_LOCAL_SCOPE](../../etc/config/src/NO_FOR_LOCAL_SCOPE.cpp) / `NO_FOR_LOCAL_SCOPE` | scope of for-init-statements | T/E; `tests/include/rw_testdefs.h` |
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
| [OVERLOAD_OF_TEMPLATE_FUNCTION](../../etc/config/src/OVERLOAD_OF_TEMPLATE_FUNCTION.cpp) / `NO_OVERLOAD_OF_TEMPLATE_FUNCTION` | complicated partial ordering | L; `include/ostream` |
| [PART_SPEC_OVERLOAD](../../etc/config/src/PART_SPEC_OVERLOAD.cpp) / `NO_PART_SPEC_OVERLOAD` | partial ordering of function templates | L/P/T; `include/deque` |
| [PLACEMENT_DELETE](../../etc/config/src/PLACEMENT_DELETE.cpp) / `NO_PLACEMENT_DELETE` | placement delete | L/T; `include/new` |
| [POD_ZERO_INIT](../../etc/config/src/POD_ZERO_INIT.cpp) / `NO_POD_ZERO_INIT` | POD zero initialization | L; `include/rw/_traits.h` |
| [POSIX_MADVISE](../../etc/config/src/POSIX_MADVISE.cpp) / `NO_POSIX_MADVISE` | posix_madvise() in &lt;sys/mman.h&gt; | none |
| [PRETTY_FUNCTION](../../etc/config/src/PRETTY_FUNCTION.cpp) / `NO_PRETTY_FUNCTION` | __PRETTY_FUNCTION__ | L/T; `include/rw/_defs.h` |
| [PTR_EXCEPTION_SPEC](../../etc/config/src/PTR_EXCEPTION_SPEC.cpp) / `NO_PTR_EXCEPTION_SPEC` | pointer exception specification | T; `tests/diagnostics/19.std.exceptions.cpp` |
| [PUTENV_CONST_CHAR](../../etc/config/src/PUTENV_CONST_CHAR.cpp) / `NO_PUTENV_CONST_CHAR` | putenv() in &lt;stdlib.h&gt; | T; `tests/src/environ.cpp` |
| [QUIET_NAN](../../etc/config/src/QUIET_NAN.cpp) / `NO_QUIET_NAN` | quiet NaN | L; `include/limits` |
| [REINTERPRET_CAST](../../etc/config/src/REINTERPRET_CAST.cpp) / `NO_REINTERPRET_CAST` | reinterpret_cast | none |
| [RUNTIME_IN_STD](../../etc/config/src/RUNTIME_IN_STD.cpp) / `NO_RUNTIME_IN_STD` + output | runtime support in namespace std | L/P/T; `include/exception` |
| [SETLOCALE](../../etc/config/src/SETLOCALE.cpp) / `NO_SETLOCALE` | setlocale() in &lt;locale.h&gt; | none |
| [SETRLIMIT](../../etc/config/src/SETRLIMIT.cpp) / `NO_SETRLIMIT` | setrlimit() in &lt;sys/resource.h&gt; | P/T; `etc/config/src/GLOBAL_BAD_ALLOC.cpp` |
| [SIG_ATOMIC_T](../../etc/config/src/SIG_ATOMIC_T.cpp) / `NO_SIG_ATOMIC_T` + output | signal support in &lt;signal.h&gt; | none |
| [SIZE_T](../../etc/config/src/SIZE_T.cpp) / `NO_SIZE_T` + output | fpos_t, ptrdiff_t, and size_t | L; `include/ansi/cwchar` |
| [SPECIALIZATION_ON_RETURN_TYPE](../../etc/config/src/SPECIALIZATION_ON_RETURN_TYPE.cpp) / `NO_SPECIALIZATION_ON_RETURN_TYPE` | specialization on return type | L; `include/rw/_defs.h` |
| [SPECIALIZED_FRIEND](../../etc/config/src/SPECIALIZED_FRIEND.cpp) / `NO_SPECIALIZED_FRIEND` | template&lt;&gt; in friend specializations | L; `include/rw/_defs.h` |
| [STATICS_IN_TEMPLATE](../../etc/config/src/STATICS_IN_TEMPLATE.cpp) / `NO_STATICS_IN_TEMPLATE` | references to static symbols in template | L; `include/rw/_defs.h` |
| [STATIC_CAST](../../etc/config/src/STATIC_CAST.cpp) / `NO_STATIC_CAST` | static_cast | L; `include/rw/_defs.h` |
| [STATIC_CONST_MEMBER_EXPR_CONST](../../etc/config/src/STATIC_CONST_MEMBER_EXPR_CONST.cpp) / `NO_STATIC_CONST_MEMBER_EXPR_CONST` | type-dependent constant expressions | none |
| [STATIC_CONST_MEMBER_INIT](../../etc/config/src/STATIC_CONST_MEMBER_INIT.cpp) / `NO_STATIC_CONST_MEMBER_INIT` | const member initializer | L/T; `include/limits.cc` |
| [STATIC_TEMPLATE_MEMBER_INIT](../../etc/config/src/STATIC_TEMPLATE_MEMBER_INIT.cpp) / `NO_STATIC_TEMPLATE_MEMBER_INIT` | initialization of static template data memebers | L; `include/rw/_config.h` |
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
| [STRUCT_TM](../../etc/config/src/STRUCT_TM.cpp) / `NO_STRUCT_TM` + output | struct tm in &lt;time.h&gt; | L; `include/ansi/cwchar` |
| [STRUCT_TM_IN_WCHAR_H](../../etc/config/src/STRUCT_TM_IN_WCHAR_H.cpp) / `NO_STRUCT_TM_IN_WCHAR_H` | struct tm declaration in &lt;wchar.h&gt; | L; `include/ansi/cwchar` |
| [TEMPLATE_DEFAULT_ARG_CONVERSION](../../etc/config/src/TEMPLATE_DEFAULT_ARG_CONVERSION.cpp) / `NO_TEMPLATE_DEFAULT_ARG_CONVERSION` | conversion in template default arguments | none |
| [TEMPLATE_ON_RETURN_TYPE](../../etc/config/src/TEMPLATE_ON_RETURN_TYPE.cpp) / `NO_TEMPLATE_ON_RETURN_TYPE` | template overloads on return type | L/E; `include/bitset` |
| [THREAD_SAFE_ERRNO](../../etc/config/src/THREAD_SAFE_ERRNO.cpp) / `NO_THREAD_SAFE_ERRNO` | if errno is thread safe | none |
| [THREAD_SAFE_EXCEPTIONS](../../etc/config/src/THREAD_SAFE_EXCEPTIONS.cpp) / `NO_THREAD_SAFE_EXCEPTIONS` | if exceptions are thread safe | T; `tests/localization/22.locale.globals.mt.cpp` |
| [TIMEZONE](../../etc/config/src/TIMEZONE.cpp) / `NO_TIMEZONE` | int timezone in &lt;time.h&gt; | L; `src/time_put.cpp` |
| [TLS](../../etc/config/src/TLS.cpp) / `NO_TLS` + output | thread-local storage | L; `include/rw/_defs.h` |
| [TM_GMTOFF](../../etc/config/src/TM_GMTOFF.cpp) / `NO_TM_GMTOFF` | tm_gmtoff in struct tm in &lt;time.h&gt; | L/T; `src/time_put.cpp` |
| [TYPENAME](../../etc/config/src/TYPENAME.cpp) / `NO_TYPENAME` | the typename keyword | L/P/T/E; `include/rw/_defs.h` |
| [TYPE_INFO_BEFORE](../../etc/config/src/TYPE_INFO_BEFORE.cpp) / `NO_TYPE_INFO_BEFORE` | type_info::before() | L; `src/typeinfo.cpp` |
| [TYPE_INFO_DTOR](../../etc/config/src/TYPE_INFO_DTOR.cpp) / `NO_TYPE_INFO_DTOR` | type_info dtor | L/P; `src/typeinfo.cpp` |
| [TYPE_INFO_EQUALITY](../../etc/config/src/TYPE_INFO_EQUALITY.cpp) / `NO_TYPE_INFO_EQUALITY` | type_info::operator==() | L; `src/typeinfo.cpp` |
| [TYPE_INFO_INEQUALITY](../../etc/config/src/TYPE_INFO_INEQUALITY.cpp) / `NO_TYPE_INFO_INEQUALITY` | type_info::operator!=() | L; `src/typeinfo.cpp` |
| [TYPE_INFO_NAME](../../etc/config/src/TYPE_INFO_NAME.cpp) / `NO_TYPE_INFO_NAME` | type_info::name() | L; `src/typeinfo.cpp` |
| [UNAME](../../etc/config/src/UNAME.cpp) / `NO_UNAME` + output | OS name and version | none |
| [UNISTD_DECL](../../etc/config/src/UNISTD_DECL.cpp) / `NO_UNISTD_DECL` + output | the contents of &lt;unistd.h&gt; | none |
| [VA_LIST](../../etc/config/src/VA_LIST.cpp) / `NO_VA_LIST` + output | the type of va_list | none |
| [VSNPRINTF_RETURN](../../etc/config/src/VSNPRINTF_RETURN.cpp) / `NO_VSNPRINTF_RETURN` + output | vsnprintf() return value | none |
| [WCHAR_T](../../etc/config/src/WCHAR_T.cpp) / `NO_WCHAR_T` | wchar_t support | L/P/T/V/E; `include/ansi/stdlib.h` |
| [WCSFTIME_WCHAR_T_FMAT](../../etc/config/src/WCSFTIME_WCHAR_T_FMAT.cpp) / `NO_WCSFTIME_WCHAR_T_FMAT` + output | wcsftime() in &lt;wchar.h&gt; | L/T; `include/rw/_defs.h` |
| [WCTRANS_T](../../etc/config/src/WCTRANS_T.cpp) / `NO_WCTRANS_T` + output | wctrans_t in &lt;wchar.h&gt; and &lt;wctype.h&gt; | L; `include/ansi/cwctype` |
| [WCTYPE_T](../../etc/config/src/WCTYPE_T.cpp) / `NO_WCTYPE_T` + output | wctype_t in &lt;wchar.h&gt; and &lt;wctype.h&gt; | L; `include/ansi/cwctype` |
| [WINT_T](../../etc/config/src/WINT_T.cpp) / `NO_WINT_T` + output | wint_t in &lt;wchar.h&gt; and &lt;wctype.h&gt; | L/P; `include/ansi/_cwchar.h` |

## 5. Support files and scope

The remaining C++ sources supply cooperating translation units or
libraries. The `.h`, `.c`, `.cc` and `.inc` files support those probes
and the shell driver; they are not independently counted capabilities.
The complete support-file inventory is:

`collapse_static_locals.lib.cpp`, `collapse_template_locals.lib.cpp`, `collapse_template_statics.lib.cpp`, `extern_function_template_imp.cpp`, `extern_function_template_imp.h`, `extern_inline.lib.cpp`, `extern_template_before_definition_imp.cpp`, `extern_template_imp.cpp`, `extern_template_imp.h`, `float_defs.h`, `headers.inc`, `instantiation_before_definition.cc`, `instantiation_before_definition.h`, `instantiation_with_implicit_inclusion.c`, `instantiation_with_implicit_inclusion.cc`, `instantiation_with_implicit_inclusion.h`, `lib_exceptions.lib.cpp`, `locale_names.h`, `nodbg.h`, `object_mangling_imp.cpp`, `proclimits.h`, `terminate.h`, `thread.h`, `types.h`.

Descriptions and exact-token references provide a survey, not a proof
that every probe still asks the best question. The individually
reviewed cases in chapter 2 illustrate where a probe needs a closer
reading than its name suggests. Removing probes
requires examining their consumers, emitted families and dependencies
together.

Written by OpenAI LeChuck.
Brought up to date with the tree of 2026-09-27 by Claude.
