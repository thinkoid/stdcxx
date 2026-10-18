# The feature gates

## 0. tl;dr

A gate is a `_RWSTD_NO_*` macro that a characterization in
`etc/config/src/` yields and a header or source reads. The gates fall
into three families: language features, build modes, and host facts.
This note lists the language-feature gates that no compiler the tree
is built with defines, so that they can be retired one by one. The
member templates gate went first, on 2026-10-17 in bfbf7ac3 and the
seven commits after it; the rest follow its pattern.

- Three compilers have voted: GCC 16 and Clang (their `config.h` are
  equal, `analysis-clang.md`) and EDG's eccp 7.0 in strict C++03.
  A language-feature gate none of them defines has had no reader
  since about 2000 and the branch behind it cannot be compiled.
- 45 gates qualify (chapter 2). The three largest are `WCHAR_T`
  (524 lines, 48 library files), `MEMBER_TEMPLATES` (146 lines, 17
  files) and `LONG_DOUBLE` (154 lines, 12 files).
- 16 probes have no reader at all and print nothing; the probe
  alone goes (chapter 3).
- Build modes, the runtime ABI probes, the host facts and the
  compiler extensions stay (chapter 4). The runtime probes were ruled
  on 2026-10-17: they stay, the characterization rule's other half.
- A gate is retired with the probes that read it (chapter 5), and
  the retirement is recorded in `ref-characterizations.md`.

## 1. Method

Three fresh 11S configurations from master 93d8b4e9 on 2026-10-17:
gcc 16.2.1 (also 15D), and eccp 7.0 in strict mode. Clang was not
configured on that machine; `analysis-clang.md` finds its `config.h`
equal to GCC's but for the compiler's header paths. For every probe
`NAME.cpp`, the macro `_RWSTD_NO_NAME` was looked up in each
`config.h`, and its readers counted with `git grep -w` over
`include` and `src` (the library), `tests`, `util` and `examples`,
and `etc/config` less the probe itself. Line counts are grep hits,
not the lines the gate controls; a gate with ten hits in `_defs.h`
may select a hundred lines elsewhere.

The families, by what the probe asks:

- **Language features.** Does the compiler accept this C++98
  construct? `BOOL`, `MEMBER_TEMPLATES`, `CLASS_PARTIAL_SPEC`.
  Every compiler since about 2000 does. The gate is a vote nobody
  has cast against in twenty years, and the branch it selects cannot
  be compiled, so it rots: the `deque` rewrite wrote its friend
  declarations for the no-member-templates twins blind, and the
  groom after it removed a forward declaration there that had lost
  its definition. These are the candidates.
- **Build modes.** The user's choice, not the compiler's: exceptions
  off (`-fno-exceptions` sets `_RWSTD_NO_EXCEPTIONS` in
  `_config-gcc.h`), the dialect's exception specifications, the
  thread model. They stay.
- **Host facts.** What the runtime, the C library and the target
  provide: the exception classes' members, `operator new` forms,
  `long long`, atomics, TLS, sizes, `struct tm`, locale names. The
  compilers differ here today (eccp's `config.h` has 31 macros GCC's
  lacks and lacks 102 GCC's has). They stay.

A gate counts as a candidate when it is a language feature and no
`config.h` of the three defines it. The negative probes
(`NO_FEATURE.cpp` yields `_RWSTD_NO_FEATURE` on success) were read
by their own macro.

## 2. Candidates

Lines are grep hits in the library (`include`, `src`) and in the
tests (`tests`, `util`, `examples`); files are library files. "Read
by" names the probes that read the gate (chapter 5).

### 2.1 Keywords and syntax

| gate | question | lib | tests | files | read by |
| --- | --- | --- | --- | --- | --- |
| `NAMESPACE` | the `namespace` keyword | 75 | 36 | 32 | nine probes; retired 2026-10-17 |
| `BOOL` | `bool`, `true`, `false` | 18 | 23 | 8 | `LIMITS`; retired 2026-10-17 |
| `TYPENAME` | the `typename` keyword | 5 | 4 | 2 | three probes; retired 2026-10-17 |
| `EXPLICIT` | the `explicit` keyword | 1 | 17 | 1 | retired 2026-10-17 |
| `STATIC_CAST` | `static_cast` | 3 | 0 | 1 | retired 2026-10-17 |
| `DYNAMIC_CAST` | `dynamic_cast` | 3 | 11 | 1 | retired 2026-10-17 |
| `NEW_CLASS_TEMPLATE_SYNTAX` | `template <>` on a class specialization | 1 | 0 | 1 | `CV_VOID_SPECIALIZATIONS`; retired 2026-10-17 |
| `NEW_FUNC_TEMPLATE_SYNTAX` | `template <>` on a function specialization | 1 | 0 | 1 | retired 2026-10-17 |
| `NO_FOR_LOCAL_SCOPE` | the scope of a for-init-statement | 0 | 4 | 0 | retired 2026-10-17 |

`CONST_CAST` and `REINTERPRET_CAST` have no reader; chapter 3. Both
went with the bundle, as did `_RWSTD_NO_MUTABLE`, a gate this census
could not list: no probe yields it, nothing defines it, and two
places read it. The bundle left `_TYPENAME`, `_EXPLICIT`, the
`_RWSTD_*_CAST` and the `_RWSTD_SPECIALIZED_*` macros in `rw/_defs.h`
with their one definition, as the `NAMESPACE` retirement left
`_RWSTD_NAMESPACE`, `_STD`, `_RW` and `_USING`. Liviu, 2026-10-17:
"those have no reason to exist"; five commits after the gates spelled
every one of them out, keyword by keyword, the casts last.

### 2.2 Templates

| gate | question | lib | tests | files | read by |
| --- | --- | --- | --- | --- | --- |
| `MEMBER_TEMPLATES` | member templates | 107 | 31 | 17 | `EXTERN_MEMBER_TEMPLATE`, `extern_template_imp`; retired 2026-10-17 |
| `CLASS_PARTIAL_SPEC` | partial specialization | 79 | 6 | 16 | `VA_LIST`; retired 2026-10-17 |
| `PART_SPEC_OVERLOAD` | partial ordering of function templates | 29 | 3 | 7 | `VA_LIST`; retired 2026-10-17 |
| `TEMPLATE_ON_RETURN_TYPE` | function template overloads on the return type | 16 | 4 | 4 | retired 2026-10-17 |
| `STATIC_CONST_MEMBER_INIT` | in-class initializer of a static const member | 12 | 7 | 5 | retired 2026-10-17 |
| `NONDEDUCED_CONTEXT` | nondeduced contexts | 10 | 0 | 4 | retired 2026-10-17 |
| `COLLAPSE_TEMPLATE_STATICS` | one static member per template instance across translation units | 10 | 0 | 5 | retired 2026-10-17 |
| `EXPLICIT_INSTANTIATION_BEFORE_DEFINITION` | explicit instantiation before the definition | 9 | 0 | 1 | not a candidate after all: `rw/_defs.h` defines it by hand when `EXTERN_TEMPLATE_BEFORE_DEFINITION` fails, so the branch is live under eccp (chapter 4) |
| `FUNC_PARTIAL_SPEC` | overloading a function template | 9 | 0 | 2 | retired 2026-10-17 |
| `FRIEND_TEMPLATE` | friend templates of templates | 8 | 0 | 2 | reads `EXPLICIT_FUNC_INSTANTIATION`; retired 2026-10-17 |
| `OVERLOAD_OF_TEMPLATE_FUNCTION` | partial ordering, the hard case | 4 | 0 | 1 | retired 2026-10-17 |
| `SPECIALIZED_FRIEND` | `template <>` in a friend declaration | 3 | 0 | 1 | retired 2026-10-17 |
| `EXPLICIT_INSTANTIATION` | explicit instantiation | 2 | 78 | 1 | `IMPLICIT_INSTANTIATION`, `INSTANTIATE_DEFAULT_ARGS`; retired 2026-10-17 |
| `EXPLICIT_FUNC_INSTANTIATION` | explicit instantiation of a function template | 2 | 0 | 1 | `FRIEND_TEMPLATE`; retired 2026-10-17 |
| `EXPLICIT_MEMBER_SPECIALIZATION` | explicit specialization of a member | 2 | 0 | 1 | retired 2026-10-17 |
| `MEMBER_TEMPLATE_OVERLOAD` | member template overloads on the return type | 2 | 0 | 1 | retired 2026-10-17 |
| `SPECIALIZATION_ON_RETURN_TYPE` | specialization on the return type | 2 | 0 | 1 | retired 2026-10-17 |
| `DEPENDENT_TEMPLATE` | `::template` | 2 | 0 | 1 | retired 2026-10-17 |
| `DEFAULT_TEMPLATE_ARGS` | default template arguments | 1 | 6 | 1 | retired 2026-10-17 |
| `IMPLICIT_INSTANTIATION` | implicit instantiation | 1 | 0 | 1 | retired 2026-10-17 |
| `STATIC_TEMPLATE_MEMBER_INIT` | initialization of a static member of a template | 1 | 0 | 1 | retired 2026-10-17 |
| `EXPLICIT_CTOR_INSTANTIATION` | explicit instantiation of a constructor | 0 | 2 | 0 | retired 2026-10-17 |

`COLLAPSE_TEMPLATE_STATICS` and `STATIC_TEMPLATE_MEMBER_INIT`
together select `_RWSTD_NO_STATIC_MUTEX_INIT` in `_config.h`, read
in 19 library lines. That derived gate goes with them.

### 2.3 Types

| gate | question | lib | tests | files | read by |
| --- | --- | --- | --- | --- | --- |
| `WCHAR_T` | `wchar_t` support | 157 | 365 | 48 | `LIMITS`, `NATIVE_WCHAR_T` |
| `LONG_DOUBLE` | `long double` a native type | 52 | 80 | 12 | `FLOAT`, `INFINITY`, `LIMITS`, `MATH_OVERLOADS`, `VA_LIST` |
| `NATIVE_WCHAR_T` | `wchar_t` a distinct type, not a typedef | 9 | 21 | 5 | |

`NATIVE_WCHAR_T` was Visual C++'s `/Zc:wchar_t-`; that compiler is
retired. `_config.h` derives it from `WCHAR_T`.

### 2.4 Other language rules

| gate | question | lib | tests | files | read by |
| --- | --- | --- | --- | --- | --- |
| `POD_ZERO_INIT` | `T ()` zero-initializes a POD | 8 | 0 | 2 | |
| `NESTED_CLASS_ACCESS` | a nested class's access to its enclosing class | 6 | 2 | 3 | |
| `NONCLASS_ARROW_RETURN` | `operator->` returning a non-class type | 4 | 3 | 2 | |
| `EXTERN_C_COMPATIBILITY` | `extern "C"` and `extern "C++"` function types | 4 | 0 | 2 | |
| `DUMMY_DEFAULT_ARG` | a dummy default argument | 3 | 0 | 1 | |
| `CV_VOID_SPECIALIZATIONS` | specializations on cv-qualified `void` | 2 | 0 | 1 | |
| `EMPTY_MEM_INITIALIZER` | an empty mem-initializer list | 2 | 5 | 1 | |

### 2.5 For a decision

Three gates sit between the families. Each passes under all three
compilers.

| gate | question | lib | tests | files | read by |
| --- | --- | --- | --- | --- | --- |
| `HONOR_STD` | the compiler honors `namespace std` (g++ 2's `-fhonor-std`) | 38 | 14 | 17 | 27 probes |
| `RUNTIME_IN_STD` | the runtime's classes live in `std` | 33 | 7 | 5 | 30 probes; prints macros |
| `PLACEMENT_DELETE` | placement `operator delete` | 13 | 4 | 4 | `_config-gcc.h` defines it for gcc 2.9x |

`HONOR_STD` is a language question with a runtime answer: g++ 2.x
put the runtime in the global namespace. `RUNTIME_IN_STD` is the
runtime half of it, and the runtime probes depend on both. They go
together or not at all, after the runtime probes' own review.
`PLACEMENT_DELETE` is a language feature with a hand-set workaround
for a gcc 2.9x internal error; the workaround goes with it.

## 3. Probes nobody reads

No reader of the status macro anywhere in the tree and no other
output. The probe file goes, with its support file where one exists.
Done 2026-10-17, one commit, the twelve below (`CONST_CAST` and
`REINTERPRET_CAST` had gone with the keyword bundle).

`CONST_CAST`, `REINTERPRET_CAST`, `EXPLICIT_ARG`, `EXTERN_INLINE`
(`extern_inline.lib.cpp`), `FUNCTION_TRY_BLOCK`,
`TEMPLATE_DEFAULT_ARG_CONVERSION`, `INSTANTIATE_DEFAULT_ARGS`,
`STATIC_CONST_MEMBER_EXPR_CONST`, `EXPLICIT_MEMBER_INSTANTIATION`,
`COLLAPSE_STATIC_LOCALS` (`collapse_static_locals.lib.cpp`),
`COLLAPSE_TEMPLATE_LOCALS` (`collapse_template_locals.lib.cpp`),
`EXTERN_C_EXCEPTIONS`, `LIB_EXCEPTIONS` (`lib_exceptions.lib.cpp`),
`THREAD_SAFE_ERRNO`.

Not in this list, though their status macro has no reader: the
probes that print values (`LIMITS`, `FLOAT`, `LCONV`, `VA_LIST`,
`SIG_ATOMIC_T`, `UNISTD_DECL`, `CTYPE_BITS`, `UNAME`, `NEWLINE`, the
`*_OVERLOADS` and `*_PRINTF_PREFIX` probes, `VSNPRINTF_RETURN`), and
`SETLOCALE` and `POSIX_MADVISE`, which are host facts.

## 4. Not candidates

- **Build modes**: `EXCEPTIONS`, `EXCEPTION_SPECIFICATION`,
  `EXCEPTION_SPECIFICATION_ON_NEW`, `PTR_EXCEPTION_SPEC`,
  `LIBC_EXCEPTION_SPEC`. The first is set by `-fno-exceptions`; the
  others fail by dialect and select the path the dialect needs.
- **The runtime ABI probes**: `STD_*`, `GLOBAL_*`, the exception
  classes' special members, `TYPE_INFO_*`, `OPERATOR_NEW_*` and
  `OPERATOR_DELETE_*`, `NEW_THROWS`, `NEW_OFLOW_SAFE`. Liviu,
  2026-10-17: they stay. GCC and eccp answer them differently today
  (`_RWSTD_NO_BAD_ALLOC_DEFAULT_CTOR` and kin are defined under GCC,
  `_RWSTD_NO_STD_NOTHROW` and `_RWSTD_NO_STD_TYPE_INFO` under eccp).
- **Gates eccp defines**: `LONG_LONG`, `EXTERN_TEMPLATE` and its
  three relatives, the five `*_ATOMIC_OPS`, `TLS`,
  `STATICS_IN_TEMPLATE`. Live by construction. So is
  `EXPLICIT_INSTANTIATION_BEFORE_DEFINITION`, which the census listed
  in 2.2: no probe answer defines it, but `rw/_defs.h` defines it by
  hand whenever `EXTERN_TEMPLATE_BEFORE_DEFINITION` is, and eccp's is.
- **Host facts**: everything that prints a value, the C header and
  function probes, `WINT_T`, `WCTYPE_T`, `WCTRANS_T`, `MBSTATE_T`,
  `FPOS_T`, `STRUCT_TM`, `OFFSETOF`, `INFINITY`, `QUIET_NAN`, the
  negative traps probes, `NO_OBJECT_MANGLING`, `EQUAL_CTYPE_MASK`,
  `NL_LANGINFO`, `NL_TYPES_H`, `MMAP`, `MUNMAP`.
- **Compiler extensions**: `FUNC` (`__func__`, C99 and C++11),
  `PRETTY_FUNCTION`. A conforming C++03 compiler may lack them;
  eccp happens to have both.
- `ATOMIC_OPS`, a success stub (`ref-characterizations.md` 2.1).

## 5. Dependencies

Probes read other gates through `config.h`. Retiring a gate edits
the probes that read it, or they fail to configure.

| gate | read by |
| --- | --- |
| `NAMESPACE` | `EXCEPTION_SPECIFICATION_ON_NEW`, `FPOS_T`, `OPERATOR_DELETE_ARRAY_NOTHROW`, `OPERATOR_DELETE_NOTHROW`, `OPERATOR_NEW_ARRAY_NOTHROW`, `OPERATOR_NEW_ARRAY`, `OPERATOR_NEW_NOTHROW`, `SETLOCALE`, `WCSFTIME_WCHAR_T_FMAT` |
| `TYPENAME` | `DEPENDENT_TEMPLATE`, `NONDEDUCED_CONTEXT`, `STATIC_CONST_MEMBER_EXPR_CONST` |
| `BOOL`, `WCHAR_T` | `LIMITS` |
| `LONG_DOUBLE` | `FLOAT`, `INFINITY`, `LIMITS`, `MATH_OVERLOADS`, `VA_LIST` |
| `CLASS_PARTIAL_SPEC`, `PART_SPEC_OVERLOAD` | `VA_LIST` |
| `MEMBER_TEMPLATES` | `EXTERN_MEMBER_TEMPLATE`, `extern_template_imp.cpp` |
| `EXPLICIT_INSTANTIATION` | `IMPLICIT_INSTANTIATION`, `INSTANTIATE_DEFAULT_ARGS` |
| `EXPLICIT_FUNC_INSTANTIATION` | `FRIEND_TEMPLATE` |
| `NEW_CLASS_TEMPLATE_SYNTAX` | `CV_VOID_SPECIALIZATIONS` |
| `HONOR_STD` | 27 probes, the runtime family |
| `RUNTIME_IN_STD` | 30 probes, the runtime family |

## 6. Order

One gate or one bundle per commit, each taking the `#ifdef`
branches, the twins, the dispatch macros, the probe and the probes
that read it together; the suite equal to the pin after every
commit; `make config` under eccp unchanged in every other macro.
Small first, so the pattern settles before the big three.

1. `MEMBER_TEMPLATES`: retired, bfbf7ac3 and the seven commits after
   it, one header group per commit, the probe and its two readers
   last. `MEMBER_TEMPLATE_OVERLOAD`: retired in the commit after
   them.
2. The keyword gates of 2.1 as one bundle, done 2026-10-17 in one
   commit; `NAMESPACE` on its own for its nine probes, done the same
   day in the commit after.
3. The template gates of 2.2, smallest first; `CLASS_PARTIAL_SPEC`
   with `PART_SPEC_OVERLOAD` and `FUNC_PARTIAL_SPEC` last of them.
   The eleven with four library hits or fewer went 2026-10-17 in one
   commit; `FRIEND_TEMPLATE`, `FUNC_PARTIAL_SPEC`, `NONDEDUCED_CONTEXT`
   and `TEMPLATE_ON_RETURN_TYPE` in the next; `STATIC_CONST_MEMBER_INIT`
   and `COLLAPSE_TEMPLATE_STATICS`, with the static mutex fallback they
   alone enabled, in the one after; `CLASS_PARTIAL_SPEC` and
   `PART_SPEC_OVERLOAD` last, as planned. Item 3 is done.
4. The other language rules of 2.4.
5. `LONG_DOUBLE`.
6. `WCHAR_T` with `NATIVE_WCHAR_T`, the largest.
7. The probes of chapter 3, any time, one commit; done 2026-10-17.
8. The three of 2.5, after a decision.
