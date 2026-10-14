# The conformance record

## 0. tl;dr

The library targets **ISO/IEC 14882:2003**, with extensions and a
selection of Library Working Group defect resolutions. Its public
surface covers the C++03 library's major components. That coverage
is not a claim that every requirement has been verified: the revival
has outstanding failures and no completed clause-by-clause audit.

**Building with a modern compiler does not provide a modern standard
library.** The implementation has its C++98 foundation and C++03
interface, and keeps both. The target is the C++03 library, complete,
with the suite clean; later standards are not.

This record describes the tree as of 2026-10-14: component coverage,
concrete departures, selected defect resolutions checked against the
code, and the broad gap to later libraries. Measurements are the
pinned baseline tables in `doc/notes/baseline/`.

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
library builds and runs its suite there, with lock-free atomic
operations, but no suite tables are pinned for it yet.
Compiler characterizations determine the active implementation paths.
A conformance result must identify that configuration, not just the
source revision.

## 2. Deliberate extensions and departures

The strict switch is gone. `_RWSTD_NO_EXTENSIONS` used to select
`_RWSTD_STRICT_ANSI`, and a block in `include/rw/_config.h` defined
the `_RWSTD_NO_EXT_*` macros under it. The additions behind those
macros left the tree in October 2026, one subsystem per commit: the
`ios_base::failure` subclasses, the `stringbuf` character pointer
constructors and `str` overloads, the deep-copy `basic_string::copy`,
the `num_get`, `num_put` and `time_get` members beyond the standard's,
the `void` forms of `count`, `count_if` and `distance`, the comparison
operators of `vector<bool>::reference`, the allocator of const
elements, and the `bitset` constructors from character pointers and
from `string` and `wstring`. Nothing reads an `_RWSTD_NO_EXT_*` macro
now.

What the gates also covered keeps its code with the macro gone:
choices the standard permits, defect resolutions the block disabled
under an extension name, and one addition kept on its merits.

| choice in the tree | effect and source |
|---|---|
| Stream truth conversion | `basic_ios` converts to a private pointer-to-member type where 27.4.4.3 writes `operator void*`; a stream cannot be deleted or passed as an integer by accident, but `void* p = cin;` does not compile (`include/rw/_basic_ios.h`). Not C++11's explicit `operator bool` either. |
| Width after failed insertion | The string inserter and `num_put` keep the stream's width when the insertion fails and reset it on success (`include/rw/_ioinsert.cc`, `include/loc/_num_put.cc`). |
| In-place assign and insert | `deque` and `vector` assign over their existing elements, and insert an input iterator range one element at a time; the vector insert's own comment says it does not meet the strong guarantee of 23.2.4.3, p1, a question the `TODO` carries (`include/deque.cc`, `include/vector.cc`). |
| Primary template definitions | `char_traits`, `ctype`, `collate`, `codecvt` and `numpunct` have primary template definitions beyond the required specializations (`include/rw/_traits.h`, `include/loc/`). |
| Portable random sequence | The `random_shuffle` generator works in 32-bit words, so the sequence is the same on every target (`src/random.cpp`). |
| Replaceable allocation functions | `src/memory.cpp` defines the `operator new` and `operator delete` variants a support library lacks, per the `_RWSTD_NO_OPERATOR_*` probes; none fires with GCC or Clang. |
| Bump by `streamsize` | `gbump` and `pbump` take a `streamsize` where 27.5.2.3.1, p4 and 27.5.2.3.2, p4 write `int`. The library's own callers pass stream sizes and pointer differences, and an `int` cannot hold the advance of a buffer past `INT_MAX` characters on a 64-bit target. LWG issue 255 asked for this change in 2000 and was closed as not a defect in 2017, to spare the many derived classes in the wild; the library has no such constituency, so this addition stays, the only one of the strict switch's that did (`include/streambuf`). |
| Later defect corrections | LWG 559's cv-qualified `numeric_limits` and LWG 434's `bitset::to_string` overloads are unconditional (chapter 3). |

Copy-on-write strings and the compatibility mutex in their bodies are
implementation choices within the historical design, not automatically
C++03 defects. Their suitability for a later standard's iterator,
invalidation and concurrency requirements requires a separate audit.
The [string tour](ref-string-replace.md) explains the current representation.

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
| 559 | cv-qualified `numeric_limits` specializations inherit the unqualified specialization. | `include/limits` |
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

The [baseline record](ref-test-baseline.md) covers six debug
configurations: GCC and Clang, each with 64-bit archive, 32-bit archive
and 64-bit reentrant shared builds. The pinned tables contain 272
programs apiece. No table records a failed assertion; every table
records one aborted stream regression test,
`27.ostream.inserters.stdcxx-51`. **These are test counts, not
percentages of the standard implemented.** Assertions have unequal scope, and
process failures can prevent later assertions from running.

| area | current evidence and its limit |
|---|---|
| String range self-aliasing | Repaired; the relevant assignment, insertion and replacement tests and regressions pass in the recorded six configurations. This is evidence for the repaired path, not every string requirement. |
| Locale facets | The `num_get`, `time_get` and `money_get` rows pass. `num_get` and `money_get` grow their digit buffers instead of failing or overrunning, and `money_get` throws `bad_alloc` where the buffer size would wrap. |
| Numerics | The limits, math and valarray rows pass; the failures were test defects and a driver comparison. The extensions row is gone with the switch. |
| Memory/lifetime symptoms | The intermittent string-iterator result was the test driver reading past short buffers, and the `21.cwchar` exit-time corruption a test's use of a write-only stream; both are repaired. |
| Streams | `27.ostream.inserters.stdcxx-51` aborts: the library prints quiet and signaling NaNs alike, and the test expects distinct names; it has not yet been settled. The file streams have the standard's interface only: the additions from classic iostreams (descriptor and `FILE*` constructors, `fd ()`, `attach ()`, `detach ()`, extra open modes, a permissions argument) are gone. `basic_ios::flags ()` meets the postcondition of `init` in every configuration since the locking extension left the format flags. `setbase` and the clearing of `basefield` follow the standard since the `setbase` extension and `bin` were removed; `27.std.manip` pins them. |
| Reentrant locale use | The sanitizer investigation found races and use-after-free in the locale; they are repaired, and each repair has a multithreaded test. Seven locale MT rows of the reentrant tables time out: on many processors they measure the harness's time limit, a test-design question queued in `TODO`. C++03 did not itself specify the later thread library and memory model. |
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

Existing headers differ too. The inspected `string` and `vector`
interfaces lack the later move/initializer-list overloads and
emplacement interfaces; `<memory>` supplies the historical allocator,
raw-storage and `auto_ptr` facilities. Internal traits and atomic
helpers do not supply public `<type_traits>` or `<atomic>`. The
copy-on-write string itself is outside C++11. **New headers alone
would not make a later library.**

## 6. Direction of the conformance work

The target is C++03. The revival does not raise the implementation
language past C++98 and does not implement later libraries; chapter 5
is a record of the distance, not a plan. The work has three tracks:

1. Establish the C++03 baseline: resolve current failures, distinguish
   test defects from library defects, and record the active extension
   policy for each measurement.
2. Expand the defect-resolution ledger: compare each cited issue with
   its adopted wording, identify configuration gates, and attach a
   focused regression or an existing test that actually exercises it.
3. Run the suite against other libraries. Most of it checks standard
   behaviour, not this library's additions. A disagreement with
   libstdc++ or libc++ is a defect in one of them or a question the
   standard leaves open (`TODO`).

Written by OpenAI LeChuck.
Brought up to date with the tree of 2026-09-30 by Claude.
