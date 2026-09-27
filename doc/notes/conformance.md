# The conformance record

## 0. tl;dr

The library targets **ISO/IEC 14882:2003**, with extensions and a
selection of Library Working Group defect resolutions. Its public
surface covers the C++03 library's major components. That coverage
is not a claim that every requirement has been verified: the revival
has outstanding failures and no completed clause-by-clause audit.

**Building with a modern compiler does not provide a modern standard
library.** The implementation still has its C++98 foundation and
C++03 interface. Raising the implementation floor to C++17 and
implementing the C++17 library are separate projects.

This record describes the tree as of 2026-09-27: component coverage,
concrete departures, selected defect resolutions checked against the
code, and the broad gap to later libraries. Measurements are the
pinned baseline tables of that date.

## 1. What the C++03 target covers

The original `README` states the target and lists the public headers.
The implementation follows the C++03 library's organization:

| C++03 clause | implemented surface | principal entry points |
|---|---|---|
| 17: library introduction | Configuration, implementation types and library-wide conventions | `include/rw/_config.h`, `_defs.h` |
| 18: language support | Limits, allocation, type information, exceptions and C support headers | `<limits>`, `<new>`, `<typeinfo>`, `<exception>`, `include/ansi/` |
| 19: diagnostics | Standard exception classes and assertion/error facilities | `<stdexcept>`, `<cassert>`, `<cerrno>` |
| 20: utilities | Pairs, function objects and adaptors, allocators, raw-storage algorithms, `auto_ptr` | `<utility>`, `<functional>`, `<memory>` |
| 21: strings | Character traits and `basic_string` | `<string>`, `include/rw/_traits.h` |
| 22: localization | Locale composition and the standard narrow/wide facets | `<locale>`, `include/loc/`, `src/locale_*.cpp` |
| 23: containers | Sequence and ordered associative containers, adaptors and bitsets | `<vector>`, `<deque>`, `<list>`, `<map>`, `<set>`, `<queue>`, `<stack>`, `<bitset>` |
| 24: iterators | Traits, categories, adaptors and stream iterators | `<iterator>`, `include/rw/_*iter*.h` |
| 25: algorithms | Nonmodifying and modifying algorithms, sorting, searching, set and heap operations | `<algorithm>` |
| 26: numerics | Numeric algorithms, complex numbers, valarrays and C mathematics | `<numeric>`, `<complex>`, `<valarray>`, `<cmath>` |
| 27: input/output | Stream state, buffers, formatted/unformatted I/O, files and string streams | `<ios>`, `<streambuf>`, `<istream>`, `<ostream>`, `<fstream>`, `<sstream>`, `<iostream>` |
| Annex D: compatibility | Deprecated facilities including `strstream` and C-style header forms | `<strstream>`, `include/ansi/*.h` |

These are **coverage entries, not certification marks**. In particular,
the C wrappers depend on the host C library, and language-support
facilities meet compiler/runtime interfaces. Their presence cannot
establish every required signature, value or runtime behavior.

The supported platform scope is also narrower than the historical
manual: GCC and Clang on x86 and x86-64 Linux, with aarch64 next; the
library builds and runs its suite there, but its atomic operations
still fall back to mutexes.
Compiler characterizations determine the active implementation paths.
A conformance result must identify that configuration, not just the
source revision.

## 2. Deliberate extensions and departures

The default interface includes extensions. Some merely add facilities;
others change a standard operation's signature or behavior. They must
be distinguished from unresolved defects.

| choice in the tree | effect and source |
|---|---|
| Extra public facilities | Binary stream I/O, extended file-buffer operations, additional facet overloads and `basic_string::copy()` for a deep copy; documented in `README` §8.3.10 and controlled by `_RWSTD_NO_EXT_*` macros. |
| Legacy algorithm overloads | Four-argument `count` and `count_if` accumulate into an output argument and return `void`, alongside the standard forms (`include/algorithm`). These are compatibility additions. |
| Stream truth conversion | By default `basic_ios` converts to a private pointer-to-member type. `_RWSTD_NO_EXT_IOS_SAFE_CONVERSION` selects the C++03 `operator void*` form (`include/rw/_basic_ios.h`). The default is not C++11's explicit `operator bool`. |
| Width after failed insertion | The string-insertion helper normally preserves width when writing fails; `_RWSTD_NO_EXT_KEEP_WIDTH_ON_FAILURE` selects unconditional reset on its normal write-failure paths (`include/rw/_ioinsert.cc`). |
| Extra template definitions | Primary templates such as `char_traits` and several facets have implementation-provided definitions beyond the required standard specializations; the original manual describes their extension controls. |
| Later defect corrections | Some are treated as optional extensions, notably cv-qualified `numeric_limits` (chapter 3). |

`_RWSTD_NO_EXTENSIONS` selects `_RWSTD_STRICT_ANSI`, which in turn
defines individual suppression macros in `include/rw/_config.h`.
**This is an extension policy, not an independently verified
“conforming mode”.** The same block explicitly leaves several primary
facet suppression macros commented out as unimplemented. It also
disables a later defect resolution. Enabling the switch therefore
cannot be summarized as “apply every correction and remove every
extension”.

Copy-on-write strings and the compatibility mutex in their bodies are
implementation choices within the historical design, not automatically
C++03 defects. Their suitability for a later standard's iterator,
invalidation and concurrency requirements requires a separate audit.
The [string tour](string-replace.md) explains the current representation.

## 3. Defect resolutions present in the implementation

LWG issue numbers in comments preserve the reasoning behind particular
changes. **A citation alone is not proof that the final resolution is
fully implemented.** The following selected entries have corresponding
code, rather than just a historical mention. “Present” here describes
that code; it does not claim a new conformance test or an audit of every
later amendment to the issue.

| LWG issue | implementation evidence | location |
|---|---|---|
| 23 | Integer extraction checks the range of the destination, sets `failbit` and clamps overflow in the narrowing helpers. | `include/loc/_num_get.h` |
| 186 | `bitset::set(pos, value)` takes `bool`, defaulting to `true`; the comment calls this the proposed resolution. | `include/bitset` |
| 214 | `set` and `multiset` supply non-const lookup overloads alongside const ones; comments identify the proposed resolution. | `include/set` |
| 307 | Container adaptors expose the underlying container's reference typedefs; `queue::front/back` return those types. | `include/queue`, `include/stack` |
| 350 | `allocator::address` obtains the address through casts to character references, bypassing an overloaded address-of operator. | `include/rw/_allocator.h` |
| 453 | String-buffer seeking rejects a nonzero offset when the relevant buffer pointer is null. | `include/sstream.cc` |
| 467 | `char_traits<char>::lt` compares after conversion to `unsigned char`, matching bytewise ordering. | `include/rw/_traits.h` |
| 559 | cv-qualified `numeric_limits` specializations inherit the unqualified specialization; `_RWSTD_NO_EXT_CV_QUALIFIED_LIMITS` disables them. | `include/limits`, `include/rw/_config.h` |
| 622 | `basic_filebuf` destruction catches exceptions from `close`. | `include/fstream` |

Further cited issues include string construction and search (5, 7,
42), locale conversion (19, 74, 75, 305, 382), stream input and output
(60, 117, 129, 136, 243, 581), string-buffer positioning (432, 562),
and utility/iterator behavior (110, 111, 112, 127, 181, 265, 348,
353). These are pointers for expanding the record, **not additional
audited resolutions**. Core Working Group citations in the same files
concern compiler language behavior and should not be counted as library
resolutions.

## 4. What the measured failures establish

The [baseline record](working/test-baseline.md) covers six debug
configurations: GCC and Clang, each with 64-bit archive, 32-bit archive
and 64-bit reentrant shared builds. The pinned tables contain 272
programs apiece. The GCC tables record 17 failed assertions, all in
three stream tests (`27.basic.ios` 1, `27.filebuf` 8, `27.std.manip`
8); the Clang tables add 5 in `21.cwchar`, a C-library declaration
question queued in `TODO`. **These are test counts, not percentages
of the standard implemented.** Assertions have unequal scope, and
process failures can prevent later assertions from running.

| area | current evidence and its limit |
|---|---|
| String range self-aliasing | Repaired; the relevant assignment, insertion and replacement tests and regressions pass in the recorded six configurations. This is evidence for the repaired path, not every string requirement. |
| Locale facets | The `num_get`, `time_get` and `money_get` rows pass: the failures were test defects, a driver buffer, platform-decided lengths and two facet limits, repaired (`test-baseline.md`, chapter 3.9). `money_get`'s unbounded value buffer is queued. |
| Numerics and extension controls | The limits, math, valarray and extension rows pass; the failures were test defects and a driver comparison (chapter 3.10). |
| Memory/lifetime symptoms | The intermittent string-iterator result was the test driver reading past short buffers, and the `21.cwchar` exit-time corruption a test's use of a write-only stream; both are repaired. |
| Streams | `27.basic.ios`, `27.filebuf` and `27.std.manip` fail 17 assertions; they have not yet been read against the library. |
| Reentrant locale use | The sanitizer investigation found races and use-after-free in the locale; they are repaired, and each repair has a multithreaded test. The locale MT rows of the reentrant tables still vary with the machine: on many processors they measure the harness's memory and time limits, a test-design question queued in `TODO`. C++03 did not itself specify the later thread library and memory model. |
| Test infrastructure | Several repaired failures belonged to tests or their driver. Known successful `NOUT` and `FORMAT` rows have individual reporting contracts; neither label alone proves success. |

The tables do not cover every optimization mode, configuration switch,
allocator, character type or permitted input. A completed conformance
audit will need requirements and focused checks, not merely a clean
aggregate suite result.

## 5. The gap to later libraries

The public-header inventory remains that of the C++03 implementation.
Representative later facilities absent from this tree are:

| standard generation | missing public facilities |
|---|---|
| C++11 | `array`, `forward_list`, unordered containers, tuples, public type traits, smart pointers such as `unique_ptr` and `shared_ptr`, regular expressions, random distributions, clocks, threads, mutexes, futures and atomics |
| C++14 | Facilities building on that foundation, including `make_unique` and `shared_timed_mutex` |
| C++17 | `string_view`, `optional`, `variant`, `any`, filesystem, memory resources and execution policies |

The generations above are compared against the WG21 working drafts
[N3337](https://www.open-std.org/jtc1/sc22/wg21/docs/papers/2012/n3337.pdf)
and [N4659](https://www.open-std.org/jtc1/sc22/wg21/docs/papers/2017/n4659.pdf).
They are a coarse inventory, not a complete list of additions or
wording changes.

Existing headers need work too. The inspected `string` and `vector`
interfaces lack the later move/initializer-list overloads and
emplacement interfaces; `<memory>` supplies the historical allocator,
raw-storage and `auto_ptr` facilities. Internal traits and atomic
helpers do not supply public `<type_traits>` or `<atomic>`.
**New headers alone would not modernize the existing contracts.**

## 6. Direction of the conformance work

The queue's C++17 implementation floor permits newer language tools
inside the library. It does not settle the user-visible conformance
target, and this record does not make that decision on its behalf.
The work separates naturally into three tracks:

1. Establish the C++03 baseline: resolve current failures, distinguish
   test defects from library defects, and record the active extension
   policy for each measurement.
2. Expand the defect-resolution ledger: compare each cited issue with
   its adopted wording, identify configuration gates, and attach a
   focused regression or an existing test that actually exercises it.
3. Choose a later library target: inventory both new facilities and
   changed contracts in existing ones, including ownership, allocators,
   exceptions, invalidation and concurrency. Representation and ABI
   decisions belong here as well as new interfaces.

Written by OpenAI LeChuck.
Brought up to date with the tree of 2026-09-27 by Claude.
