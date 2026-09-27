# Retiring the old compiler machinery

## 0. tl;dr

This is a guide for a source-tree sweep after adoption of the queued
**C++17 implementation floor**. The target is the machinery for
missing language features, obsolete compilation models and retired
compiler defects: its probes, configuration macros, alternate code,
helper files and corresponding test branches.

The unit of removal is **a mechanism and its dependency chain**, not
a macro spelling or a file's age. A successful sweep ends with one
ordinary implementation of the supported behavior, not a forest of
compatibility macros permanently set to the modern answer.

The library's public target remains a separate decision. The queue
also calls for removing nonstandard API additions while retaining
permitted implementation choices and selected defect resolutions.
Those instructions govern the extension sweep; adopting C++17 as the
implementation language does not itself remove C++03 interfaces.

This document identifies source evidence and a work order. It contains
no compiler-result matrix and authorizes no source edits by itself.
The [characterization catalogue](characterizations.md) is the inventory
of producers; this guide follows the mechanisms beyond their producers.

## 1. What counts as proof of reachability

For each candidate, follow the complete chain:

1. **Producer:** a probe, shell-generated definition, compiler option,
   configuration override or local `#define`.
2. **Effective definition:** `config.h` enters `_config.h`, which
   includes `_config-gcc.h` and makes further policy decisions;
   `_defs.h` derives the convenience macros. Record each redefinition
   and the conditions surrounding it.
3. **Consumers:** direct conditionals, wrapper invocations, generated
   names, declarations, definitions and build rules.
4. **Contract:** what the selected code preserves—syntax, overload
   resolution, linkage, representation, error behavior or public API.
5. **Survivor:** the exact branch or replacement that supplies that
   contract under the adopted floor.

Use these classifications in the sweep record:

| classification | required evidence | disposition |
|---|---|---|
| Unreachable under the adopted contract | Every definition route is excluded, or a dominating override fixes the answer before every consumer. | Keep the supported arm; remove its obsolete alternative and gate. |
| Reachable workaround for an excluded compiler | The code still executes, but history identifies the removed compiler defect as its reason. | Replace it with the intended ordinary operation and check behavior. |
| Obsolete mechanism with a live obligation | The syntax or compilation model goes away, but its symbols, initialization or guarantees remain needed. | Supply the replacement before deleting the mechanism. |
| No reader found | No reader of the exact name, including derived names and probe dependencies after inspection. | Remove only after checking additional outputs and shared helpers. |
| Still variable or policy-dependent | The condition describes ABI, C library, build mode or an undecided public contract. | Retain or put into its own explicit decision. |

**No producer found is not proof that a branch is unreachable.** A
macro can be a user option or an obsolete spelling still accepted by
headers. Conversely, a producer can be scheduled on every configure
run although nothing consumes its status result.

Three concrete paths illustrate why the distinctions matter:

- `_RWSTD_NO_STATIC_CAST` selects all three static, const and
  reinterpret cast wrappers in `_defs.h`. The separate `CONST_CAST`
  and `REINTERPRET_CAST` probe answers do not control those wrappers.
  Audit the wrapper chain, not just similarly named probes.
- `_config-gcc.h` unconditionally undefines `NO_NEW_HEADER`,
  `NO_LIBC_IN_STD` and `NO_DEPRECATED_LIBC_IN_STD` near its end.
  Their probe outputs do not determine subsequent header branches on
  that route. Earlier probe dependencies are a separate use.
- `_defs.h` derives `NO_DEBUG_ITER` from debug mode as well as class
  partial-specialization support. Removing the deficient-compiler
  condition must **preserve the debug/non-debug distinction**.

## 2. The principal retirement groups

Names in the following tables omit `_RWSTD_`. The locations are
starting points, not a claim that they contain every consumer.

### 2.1 Collapse to the supported language

These mechanisms are candidates once the floor and supported compiler
options are established. Delete the unsupported arm, then remove the
wrapper when its only purpose was to hide missing syntax.

| gates and wrappers | survivor and principal locations |
|---|---|
| `NO_BOOL`, `NO_NATIVE_BOOL` | Native `bool` and its overloads; remove the keyword emulation in `rw/_defs.h`, inspect `limits`, streams, facets and `rw/_select.h`. The two macro spellings need separate searches. |
| `NO_NATIVE_WCHAR_T`, `NO_WCHAR_T`, `NO_LONG_LONG`, `_RWSTD_LONG_LONG` | Separate native language-type support from a policy that omits wide library facilities. Retire type emulation under the new contract; retain measured size, signedness and ABI facts. Follow `limits`, C wrappers, stream overloads and facet instantiations. |
| `NO_TYPENAME`, `_TYPENAME`; `NO_EXPLICIT`, `_EXPLICIT`; `NO_MUTABLE`, `_MUTABLE` | The actual keywords. Inspect `src/facet.cpp`'s const-cast alternative as well as declarations. `NO_MUTABLE` has consumers without a same-named probe in the current catalogue. |
| `NO_NAMESPACE`, `NO_HONOR_STD`, `_RWSTD_NAMESPACE`, `_STD`, `_RW`, `_USING` | Real namespaces and qualified names; `_defs.h`, C wrappers and runtime declarations. Separate compiler namespace support from runtime symbol placement. Namespace aliases used for readability can be considered separately from deficiency emulation. |
| `NO_STATIC_CAST`, static/const/reinterpret wrappers | Named casts, preserving the existing target type and expression; `_defs.h` and its callers. |
| `NO_DEFAULT_TEMPLATE_ARGS`, `_RWSTD_DEFAULT_ARG`, `_RWSTD_REDECLARED_DEFAULT` | Standard default arguments; verify where each default is first declared so it is not specified twice. The redeclaration wrapper is a surviving compiler workaround even though its expansion is already unconditional. |
| `NO_NEW_CLASS_TEMPLATE_SYNTAX`, `NO_NEW_FUNC_TEMPLATE_SYNTAX`, specialized declaration wrappers | Ordinary `template<>` declarations; `_defs.h` and specialization sites. |
| `NO_DEPENDENT_TEMPLATE`, `NO_SPECIALIZED_FRIEND`, `NO_FRIEND_TEMPLATE` | Correct dependent `template` disambiguation and friend declarations; `_defs.h`, `streambuf`, `rw/_stringio.cc`. Check overload binding, not merely parsing. |
| `NO_CLASS_PARTIAL_SPEC`, `NO_PART_SPEC_OVERLOAD`, `NO_FUNC_PARTIAL_SPEC`, `NO_OVERLOAD_OF_TEMPLATE_FUNCTION` | The normal traits/overload paths; algorithms, iterator helpers, containers, strings and streams. Names such as `FUNC_PARTIAL_SPEC` describe historical probes, not permission to invent function-template partial specialization. |
| `NO_TEMPLATE_ON_RETURN_TYPE`, `NO_SPECIALIZATION_ON_RETURN_TYPE`, `NO_DUMMY_DEFAULT_ARG`, `_RWSTD_DUMMY_ARG` | Normal template calls and signatures; `bitset`, `loc/_locale.h`, `_defs.h`. Remove dummy-argument emulation consistently from declarations and definitions. |
| `NO_MEMBER_TEMPLATES`, `NO_MEMBER_TEMPLATE`, `NO_MEMBER_TEMPLATE_OVERLOAD` | Member-template forms; containers, `complex`, `bitset`, locale. The singular spelling in `complex` is real source text and is not covered by searching only the plural. |
| `NO_SIMPLE_DEFAULT_TEMPLATES`, `_RWSTD_ALLOCATOR`, `_RWSTD_REBIND` | Supported allocator/rebind syntax; `_defs.h`, `rw/_allocator.h`, containers. Keep the allocator behavior; changing allocator requirements is another task. |

`NO_DYNAMIC_CAST` needs an explicit RTTI policy alongside this group.
`NO_EXCEPTIONS` likewise includes deliberate exception-disabled
compiler invocations: `_config-gcc.h` derives it from `__EXCEPTIONS`.
**C++17 support does not mean every invocation enables RTTI and
exceptions.** Either keep these supported modes or retire them by
decision; do not classify them as compiler deficiencies by spelling.

### 2.2 Replace the old template compilation models

| mechanism | source chain | removal obligation |
|---|---|---|
| Exported templates | `EXPORT`, `EXPORT_KEYWORD` probes; `NO_EXPORT`, `_EXPORT`; declarations and build configuration | Remove the old exported-template model and annotations. Do not confuse `_EXPORT` with the separate library visibility macros `_RWSTD_EXPORT`, `_RWSTD_CLASS_EXPORT`, `_RWSTD_MEMBER_EXPORT`. |
| Implicit inclusion | `IMPLICIT_INCLUSION`, `EXPLICIT_INSTANTIATION_WITH_IMPLICIT_INCLUSION`; `_config-gcc.h`; `_RWSTD_DEFINE_TEMPLATE*`; header endings and `.c`/`.cc` files | Keep explicit inclusion of the definitions callers need. Check wrapper files and build/install references before deleting files; `include/string.c` is a wrapper around `string.cc`, while `string.cc` contains the implementation. |
| Explicit instantiation and declaration ordering | `NO_EXPLICIT_INSTANTIATION`, `NO_IMPLICIT_INSTANTIATION`, `NO_EXTERN_TEMPLATE`, `NO_EXTERN_FUNCTION_TEMPLATE`, `NO_EXPLICIT_INSTANTIATION_BEFORE_DEFINITION`, `NO_EXTERN_TEMPLATE_BEFORE_DEFINITION` | Simplify the strategy, but retain deliberate instantiations and symbol ownership in `src/ti_*.cpp`. The supported ordering must work for both library compilation and consumer translation units. |
| Suppression and generated names | `NO_TEMPLATE_DEFINITIONS`, `NO_INSTANTIATE`, `_RWSTD_INSTANTIATE*`, `_RWSTD_DEFINE_TEMPLATE_FIRST/LAST` | These also control compilation cost and which definitions enter a translation unit. Follow token-pasted `NO_<name>_DEFINITION` and `INSTANTIATE_<name>` families before collapsing them. |
| Outlined string member templates | `NO_EXTERN_MEMBER_TEMPLATE` and `NO_MEMBER_TEMPLATES` derive `NO_STRING_OUTLINED_MEMBER_TEMPLATES`; `include/string` and `string.cc` | Select the supported member implementation and remove the free-function alternatives together with their declarations and friend access. Preserve the range-dispatch and aliasing repair. |
| Facet-id emission | `NO_EXPLICIT_MEMBER_SPECIALIZATION`, `NO_EXTERN_TEMPLATE` derive `NO_SPECIALIZED_FACET_ID`; `loc/_facet.h`, facet headers, `src/use_facet.h`, `src/ti_*.cpp` | Preserve a single facet identity across translation units and shared-library boundaries. Inline variables are a possible redesign, not a textual replacement for every static declaration. |

There are closely named gates that are not interchangeable:
`NO_EXPLICIT_FUNC_INSTANTIATION` is used in `src/collate.cpp`, while
`NO_FUNCTION_EXPLICIT_INSTANTIATION` appears in `_defs.h`.
Inspect both. A sweep built from probe filenames alone will miss
some of the surviving mechanism.

### 2.3 Replace old facilities while preserving their contracts

| mechanism | intended direction | boundary to preserve |
|---|---|---|
| `_THROWS`, `_NEW_THROWS`, `NO_EXCEPTION_SPECIFICATION`, `NO_EXCEPTION_SPECIFICATION_ON_NEW` | Replace dynamic exception specifications with the chosen C++17 declarations. | **`throw(T)` must not become bare `noexcept`.** A permitted-throwing function stays potentially throwing; an empty specification has a different obligation. Match declarations, definitions, overrides and runtime declarations. |
| `_LIBC_THROWS`, `NO_LIBC_EXCEPTION_SPEC`, nothrow attributes | Reconcile declarations with the actual C/runtime ABI. | These declarations describe externally supplied functions. A compiler attribute and a C++ exception specification are not interchangeable in every context. |
| `_RWSTD_COMPILE_ASSERT`, `__rw_compile_assert` | Use `static_assert`. | The existing macro is an expression. Inspect its call context before replacing it with a declaration, and retain the assertion's predicate. |
| Internal traits and integral dispatch (`rw/_typetraits.h`, `_select.h`) | Simplify with modern language facilities or the library's own traits implementation. | **This is the standard library being built.** There is no supplied public `<type_traits>` here; importing the host C++ library is not a bootstrap solution. Preserve integral-versus-iterator overload selection. |
| TLS, static mutexes and initialization (`NO_TLS`, `_RWSTD_THREAD`, `NO_STATIC_MUTEX_INIT`, `NO_COLLAPSE_TEMPLATE_STATICS`) | Choose modern storage and initialization mechanisms deliberately. | Storage duration, destruction order, ABI and synchronization matter. Keep the body/facet publication problem separate from the syntax used to initialize a static. |
| Null spelling, override annotations | Apply the queued modernization after the floor decision. | Classify each `0` by type and purpose. Null, numeric zero and sentinel encodings are not the same substitution. |

## 3. Reachable workarounds outside the macro inventory

Some of the oldest accommodation is ordinary code, or remains active
behind an overly broad historical guard. Search comments as well as
preprocessor directives, then read the history before changing it.

| site | observed mechanism | sweep treatment |
|---|---|---|
| `src/locale_body.cpp` | An element loop replaces bulk pointer initialization for an old GCC/SPARC optimizer defect; the conditions extend beyond that original compiler. | Reachable workaround, not dead code. Choose the ordinary pointer operation for the supported targets and preserve representation assumptions. |
| `src/locale_combine.cpp` | The body copies once had the same workaround; they are now loops for a different reason, one acquire load per slot, because another thread may fill a slot of the source body. | Not a workaround any more. Keep the per-slot loads; a bulk copy would reintroduce the race. |
| `include/loc/_locale.h` facet constructor | A cast through `size_t**` is justified by an old compiler bug. | Re-express the intended member assignment without changing facet-id setup or constness of the actual object. |
| `include/rw/_strref.h`, `_C_size` | A named union carries a compiler workaround, but its `_CharT` member also establishes alignment. | Removing the name does not authorize removing the union's alignment role or changing the body layout. |
| `include/rw/_mutex.h` | Historical indirections, a named union and explicit static-member instantiation support old compilers. | Separate syntax workarounds from mutex initialization, lifetime and shared-library identity. |
| `include/rw/_config-gcc.h` | GCC 2.x placement-delete, extern-template and iostream-reference branches; old version conditions on attributes. | Retire excluded-version alternatives, checking Clang's GNU compatibility macros rather than reading `__GNUG__` as its actual version. |
| `include/algorithm`, `rw/_heap.h`, `rw/_iterbase.h` | Helper overloads compensate for missing iterator traits/partial specialization. | Keep the normal iterator-category behavior and user-iterator support; remove only the emulation paths. |
| `include/valarray.cc`, `include/map`, `rw/_allocator.h`, `rw/_ioiter.h` | Local syntax choices and helper forms carry retired-compiler explanations. | Review individually. Equivalent working syntax need not be churned merely to erase an old comment. |

The guide's tables are a sweep seed, not a claim that every historical
workaround has been found. An uncatalogued site gets the same contract
and reachability examination before joining a removal batch.

## 4. Boundaries with the other retirement work

### 4.1 The extension policy

Follow the maintained `TODO` entry for strict conformance. It splits
the `_RWSTD_NO_EXT_*` gates into **API additions to remove** and
**behavior to retain while removing the gate**. Its 38-name inventory
needs a per-gate disposition before the umbrella policy is deleted.

The removal side includes extra stream flags and failure subclasses,
deep string copy, nonstandard facet overloads, old void-returning
algorithm forms and other added interfaces. The retained side includes
in-place container operations, width behavior, portable random
sequences, primary-template definitions and the selected defect
resolutions. `long long` follows the new floor.

Do not force all negative extension macros on and call that the trim:
the old strict block intentionally selects different behavior for
some retained cases. Each addition loses its implementation, macro,
test branches and dedicated extension cases together. Each retained
behavior loses only the gate and unwanted alternative.

### 4.2 Things whose age is not a retirement argument

Keep platform characterization for C types/layouts, numeric limits,
conversion signatures, native-header routing and runtime linkage until
their replacement is justified. Preserve debug modes, thread modes,
allocator contracts, iterator checks and error paths unless the
corresponding policy is explicitly changed.

`auto_ptr`, `strstream` and historical function adaptors belong to the
public C++03 surface; their later deprecation/removal does not classify
them as deficient-compiler workarounds. Public conformance advances
need their own inventory and decision.

The per-string mutex for 4.1.x binary compatibility, string reference
counting, facet layout and exported instantiations are representation
or ABI decisions. Keep them out of an allegedly inert syntax sweep.
Similarly, a private probe without consumers can be retired, but the
atomic success stub needs a replacement characterization, and the
aarch64 entry in `TODO` queues one: the `__sync` built-ins at each
width, in place of the architecture list in `_atomic.h`.

## 5. Work order for a sweep

1. Establish the C++17 implementation contract, including supported
   compiler options and public-header use. Express required capability
   through the configuration machinery; do not silently pin a dialect
   to make deficient branches disappear. Resolve sequencing with the
   extension policy before its umbrella switches are removed.
2. Take small keyword and syntax families first. Read their history,
   select the surviving arm, simplify its callers, remove the derived
   wrappers, then remove the producer and exclusively owned helpers.
   A shared helper stays until its last consumer leaves.
3. Collapse member-template emulation and iterator-trait workarounds.
   Keep overload selection and user-defined types as explicit checks.
4. Simplify template inclusion and instantiation as a separate batch.
   Verify both library and consumer builds before removing wrappers,
   suppression switches or translation units.
5. Address exception declarations, traits bootstrap and static/facet
   initialization in separate changes with their own contracts.
6. Apply the subsystem-by-subsystem extension policy. Make ABI and
   concurrency redesigns separate from behavior-preserving retirement.
7. Revisit configuration dependencies, build/install lists, tests,
   examples and documentation. The original historical `README` stays
   untouched; current build guidance belongs in `README.md`.

For each family, record its exact macro names, producer, overrides,
consumers, surviving behavior, helper ownership and validation. This
small removal ledger is what lets the next pass distinguish a completed
retirement from an unfinished half-removal. An empty search for a
selected family is useful; **an empty search for all `_RWSTD_NO_*`
would be the wrong objective**.

## 6. Search and validation guide

From the source root, these read-only searches find the main layers:

```sh
rg -n 'NO_MEMBER_TEMPLATES|NO_MEMBER_TEMPLATE\b|NO_STRING_OUTLINED_MEMBER_TEMPLATES' include src tests etc examples
rg -n 'NO_.*INSTANTIATION|NO_EXTERN_.*TEMPLATE|DEFINE_TEMPLATE|INSTANTIATE|##' include src etc/config
rg -n 'NO_EXPORT|NO_IMPLICIT_INCLUSION|_EXPORT\b' include src etc/config
rg -n 'NO_EXCEPTIONS|NO_DYNAMIC_CAST|_THROWS|_NEW_THROWS|_LIBC_THROWS' include src tests etc/config
rg -n -i 'work.?around|compiler bug|optimizer bug|MSVC|SunPro|HP aCC|VisualAge' include src tests etc/config
git log -S'_RWSTD_NO_MEMBER_TEMPLATES' -- include src etc/config
```

For a candidate definition, inspect every `#define`/`#undef`, including
replacement lists which construct other names. Look for build-supplied
options and source-local definitions as well as `config.h`. Review
the complete conditional, not just the matching line. Compiler-brand
tests and unconditional workarounds need the same treatment.

Validation belongs to the eventual implementation pass. Use build
directories of its own; do not reconfigure or clean another working
build. Start from fresh configuration after probe or
configuration changes, with phases serial and parallelism within a
phase. Header changes need forced test recompilation because the
existing test build can miss their dependencies.

For a behavior-preserving batch, compare the relevant suite rows and
then the six-configuration suite with its baseline; account separately
for the known unstable MT rows. Build the examples. Template and
facet-id changes additionally need small consumer programs across
multiple translation units and a shared-library boundary; exception
changes need throwing and nonthrowing paths; representation changes
need size/alignment and symbol checks. Debug-only success is
insufficient for a change that removes a non-debug branch.

The check must exercise the contract that justified the removed
machinery. Use targeted negative controls where a new check is needed.
No configuration, build or runtime validation was performed for this
guide; its classifications are source-based starting points for the
sweep.

Written by OpenAI LeChuck.
Brought up to date with the tree of 2026-09-27 by Claude.
