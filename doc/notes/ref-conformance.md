# The conformance record

## 0. tl;dr

The library targets **ISO/IEC 14882:2003**, with extensions and a
selection of Library Working Group defect resolutions. Its public
surface covers the C++03 library's major components. That coverage
is not a claim that every requirement has been verified: the known
defects are `TODO` entries, and only clause 22 has a clause-by-clause
audit (`ref-locale.md`, chapters 8 to 13).

**Building with a modern compiler does not provide a modern standard
library.** The implementation has its C++98 foundation and C++03
interface, and keeps both. The target is the C++03 library, complete,
with the suite clean; later standards are not.

This record describes the tree as of 2026-11-02: component coverage,
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
the `num_get` and `num_put` members beyond the standard's,
the `void` forms of `count`, `count_if` and `distance`, the comparison
operators of `vector<bool>::reference`, the allocator of const
elements, and the `bitset` constructors from character pointers and
from `string` and `wstring`. Nothing reads an `_RWSTD_NO_EXT_*` macro
now.

What the gates also covered keeps its code with the macro gone:
choices the standard permits, defect resolutions the block disabled
under an extension name, and two additions kept on their merits.
The table also records the departures no gate covered, the locale's
among them. For a locale row, `ref-locale.md` holds the argument and
the row is its summary.

| choice in the tree | effect and source |
|---|---|
| Stream truth conversion | `basic_ios` converts to a private pointer-to-member type where 14882:2003's 27.4.4.3 writes `operator void*`. That is LWG 468's resolution, adopted into the first C++0x draft: a conversion to an unspecified type that converts only to `bool`, pointer-to-member the suggested shape, so a stream cannot be deleted, compared to a pointer or passed as an integer by accident, and `void* p = cin;` does not compile. C++11 then replaced it with `explicit operator bool` (LWG 1094), the same contract with language support; the tree keeps the 468 form (`include/rw/_basic_ios.h`). |
| In-place assign | `deque` and `vector` assign over their existing elements where 23.1.1's table writes an erase and an insert; the element count and the result are the standard's, the copy and assignment counts are not (`include/deque.cc`, `include/vector.cc`). |
| Retained deque buffers and staged sources | `deque` keeps every buffer it allocates until it is destroyed or swapped away; pops and `clear` empty it without freeing, as `vector`'s capacity survives `clear`. A range insert from any source other than a pointer or the deque's own iterator is consumed into a temporary first, so an exception from the source leaves the deque unchanged, which 23.2.1.3, p2 asks of every exception but `T`'s; other libraries trust the source and do not pay the second copy (`include/deque.cc`, `doc/notes/analysis-deque-redesign.md`). |
| Primary template definitions | `char_traits`, `ctype`, `collate`, `codecvt`, `numpunct` and `moneypunct` have primary templates beyond the required specializations (`include/rw/_traits.h`, `include/loc/`). The templates exist; not all of them are usable. `ctype` and `codecvt` define degenerate members for any type, so streams on `signed char` and `unsigned char` instantiate. `collate`'s defines the class and none of its three virtuals: `collate<unsigned char>` compiles and fails to link. Every virtual of `numpunct` and `moneypunct` reaches a stub that asserts in a debug build and returns a null pointer the virtual dereferences in a release build (`include/loc/_punct.h`, `__rw_get_punct`). (`ref-locale.md` ch. 9, 10 and 11) |
| Portable random sequence | The `random_shuffle` generator works in 32-bit words, so the sequence is the same on every target (`src/random.cpp`). |
| Replaceable allocation functions | `src/memory.cpp` defines the `operator new` and `operator delete` variants a support library lacks, per the `_RWSTD_NO_OPERATOR_*` probes; none fires with GCC or Clang. |
| Bump by `streamsize` | `gbump` and `pbump` take a `streamsize` where 27.5.2.3.1, p4 and 27.5.2.3.2, p4 write `int`. The library's own callers pass stream sizes and pointer differences, and an `int` cannot hold the advance of a buffer past `INT_MAX` characters on a 64-bit target. LWG issue 255 asked for this change in 2000 and was closed as not a defect in 2017, to spare the many derived classes in the wild; the library has no such constituency, so this addition stays, the only one of the strict switch's that did (`include/streambuf`). |
| A composed locale keeps a name when its categories are whole | 14882:2003's 22.1.1.2, p13 and 22.1.1.3, p4 give a locale built with a facet no name. The library names it when the facet completes a category of one name: with named `de` and `en`, `de.combine<messages<char> >(en)` is unnamed, and its `combine<messages<wchar_t> >(en)` is named, `LC_MESSAGES` from `en` among `de`'s categories (`src/locale_combine.cpp`, `_C_combine`). (`ref-locale.md` ch. 8) |
| Unnamed locales compare by content | 22.1.1.4, p1 makes an unnamed locale equal only to its copies. `operator==` compares the facets, so two locales built apart as `locale (src, &f)` compare equal. Where both hold user facets, the comparison skips the standard slots, a defect queued in `TODO` ("`operator==` ignores the standard facets when both locales hold user facets") (`src/locale_eq.cpp`). (`ref-locale.md` ch. 8) |
| `has_facet` without `throw ()` | The 22.1 synopsis and 22.1.2 declare `has_facet` `throw ()`. The library declares no exception specification: the first `has_facet` of a standard facet constructs it and can throw `bad_alloc` (`include/loc/_locale.h`). (`ref-locale.md` ch. 8) |
| `use_facet` checks the dynamic type | 22.1.2, p2 returns the locale's facet for the id. The library `dynamic_cast`s it to the requested type: `use_facet<collate_byname<char> >(locale::classic ())` throws `bad_cast`. `22.locale.globals` pins it for every byname facet (`include/loc/_locale.h`). (`ref-locale.md` ch. 8) |
| An invalid category throws | 22.1.1.2, p14 has no Throws clause. The two-locale constructor with a category outside `all` throws `runtime_error`. The name-and-category constructor throws too, with the message for a bad name (`TODO`, "A bad category throws with the bad-name message") (`src/locale_combine.cpp`). (`ref-locale.md` ch. 8) |
| A mixed name does not reach the C library | 22.1.1.5, p2 has `locale::global` call `setlocale (LC_ALL, loc.name ().c_str ())` for a named locale. The library makes the call, but glibc takes a composite name only when it lists all twelve of glibc's categories, and the library's lists six: the call fails and the C locale stays as it was. Which way to settle it is a decision queued in `TODO` ("A mixed-name global does not reach the C library") (`src/locale_global.cpp`). (`ref-locale.md` ch. 8) |
| Cached `narrow`, `widen` and `always_noconv` | 22.2.1.3.2, p10-11, 22.2.1.1.1, p6-7 and 22.2.1.5.1, p5 have `narrow`, `widen` and `always_noconv` return their virtual's answer. The library calls each virtual once per character and facet and returns the stored answer after; a `do_narrow` result equal to the default is not stored. A derived virtual whose answer changes, or that counts its calls, sees the difference. `22.locale.ctype.mt` pins the narrow and widen caches (`include/loc/_ctype.h`, `include/loc/_codecvt.h`). (`ref-locale.md` ch. 9) |
| `length` counts complete characters | 22.2.1.5.2, p10 has the required instantiations, the wide one among them, return "the lesser of max and (from_end-from)". The byname wide facet returns the bytes of at most `max` complete characters, as LWG 305's resolution reads, which narrows the sentence to `codecvt<char, char, mbstate_t>`; the classic facet gives the same number either way. `22.locale.codecvt.length` pins it (`src/wcodecvt.cpp`, `__rw_libstd_do_length`). (`ref-locale.md` ch. 9) |
| The classic wide `codecvt` narrows by truncation | 22.2.1.5.2, p2 has `out` stop at a character it cannot convert. `codecvt<wchar_t, char, mbstate_t>::do_out` stores the low eight bits of every `wchar_t` and returns `ok`: U+20AC becomes `'\xac'`. No test pins it (`src/wcodecvt.cpp`). (`ref-locale.md` ch. 9) |
| The blank bit through `ctype<char>::is` | 22.2.1.3.2, p4 has the vector `is` assign `table ()[(unsigned char)*p]`. Over a database locale the table carries bit 0x400 for `blank`, which no `ctype_base::mask` enumerator names: under `de_DE.ISO-8859-1` a space gives 0x403 where the classic facet gives 0x003, and `table ()` hands out the same words. The wide vector `is` strips the bit. Whether the narrow one strips it too is a decision queued in `TODO` ("The blank bit is visible through `ctype<char>::is` under a database") (`plan-localedef-backend.md` 3.1). (`ref-locale.md` ch. 9) |
| Fixed with a negative precision | 22.2.2.2.2, p11 puts the precision in the conversion only under `fixed` or when it is positive. Under `fixed` with a negative precision, `num_put` places a decimal point that many digits from the right: 1234 at -3 is `1.234`. The 2005 import has it, `doc/stdlibref/num-put.html` documents it, and no gate covered it. `22.locale.num.put` pins it. A large negative precision overruns the buffer (`TODO`) (`src/num_put.cpp`, `__rw_put_num`). (`ref-locale.md` ch. 10) |
| Floating values are grouped | 22.2.2.2.2, p16 inserts thousands separators "For integral types". `num_put` groups the integral part of a floating value too: `1,234,567.500000` under `"\3"`. That is C++11's text, 22.4.2.2.2 stage 2, "For arithmetic types". `22.locale.num.put` pins it (`src/num_put.cpp`, `__rw_put_groups`). (`ref-locale.md` ch. 10) |
| A zero precision is specified | 22.2.2.2.2, p11 puts the precision in the conversion only under `fixed` or when it is positive. The library writes `.0` for a zero precision without `fixed`, so 1234.5 at precision 0 is `1e+03`, not `%g`'s `1234.5`. That is LWG 231's resolution, C++11's text. `22.locale.num.put` pins it (`src/punct.cpp`, `__rw_get_stdio_fmat`). (`ref-locale.md` ch. 10) |
| The `0x` prefix in stage 2 | 22.2.2.1.2, p8's atoms hold no `x` or `X`. `num_get` accepts `0x` and `0X` before hex digits under `hex`, under an empty `basefield` and for `void*`, as `scanf`'s `%x`, `%i` and `%p` do; C++11's atoms add `x` and `X`. `22.locale.num.get` pins it (`include/loc/_num_get.cc`). (`ref-locale.md` ch. 10) |
| A grouping mismatch stores the value | 22.2.2.1.2, p1 and 22.2.6.1.2, p1 leave the value unchanged on an error. Where the separators disagree with `grouping ()`, `num_get` stores the value and sets `failbit`: `12,34` under `"\3"` reads 1234. `money_get` does the same for `units` and `digits`. C++11's 22.4.2.1.2, p3-4 stores first and checks the grouping after. `22.locale.num.get` pins it (`src/num_get.cpp`, `__rw_get_num`; `include/loc/_money_get.cc`). (`ref-locale.md` ch. 10) |
| `money_get` sets `eofbit` on success | 22.2.6.1.2, p1 leaves `err` alone when a valid sequence is recognized. `money_get` adds `eofbit` whenever the input ended, valid or not: `1,234.56` under en_US reads 123456 with `eofbit`. `22.locale.money.get` pins it (`include/loc/_money_get.cc`). (`ref-locale.md` ch. 10) |
| `collate<char>` orders bytes as `unsigned char` | 22.2.4.1.2, p1 has `collate<char>` compare lexicographically, by `operator<`, which on a signed `char` puts `'\x80'` before `'a'`. The library compares with `memcmp`, `'\x80'` after `'a'`, and so agrees with `char_traits<char>::lt` under LWG 467 (chapter 3) and with `basic_string<char>::compare` (`src/collate.cpp`). (`ref-locale.md` ch. 11) |
| `time_get` with a format | `time_get::get` with a conversion specifier and modifier, `get` over a `strptime`-style format range, and the virtual `do_get` behind them are C++11's 22.4.5.1.1, p6-10 and 22.4.5.1.2, p11-12, not 2003's. They stay as an exception to the 2003 baseline: the facet shipped them in 2001, N2070 and N2211 took them into the standard from this implementation, and the POSIX specifier engine is reachable only through them; it reads every specifier but `%G`, `%g`, `%u` and `%V` (`TODO`). They left in b7443c95 and came back the same day (`include/loc/_time_get.h`). (`ref-locale.md` ch. 12) |
| Narrow names matched by element | `time_get<char>` matches a locale's names, the months, weekdays, `am_pm` and alternate digits, one `char` at a time; under a multibyte charmap a name is a byte string. 14882:2003's 22.2.5.1.2, p6 has the facet read on past a complete name while the characters that follow could still match a longer one, and fail when they do not; the input iterator it reads through cannot give those characters back. A name that is a prefix of longer names, where the longer names' next character shares its first byte with the text after the name, cannot be read through the narrow facet: lzh_TW's alternate digit 廿 before 年 is the case, since 廿四, 廿六 and 廿八 go on with the same first byte as 年. The wide facet matches one `wchar_t` per character, sees the mismatch in its one element of lookahead and reads the name. This is the standard's bound, not the library's; glibc's `strptime` backtracks over a string held in memory, which the iterator interface cannot. The narrow facet's unit is the byte, and CJK date text parses through the wide facet (`include/loc/_punct.cc`, `__rw_match_name`). (`ref-locale.md` ch. 12) |
| Empty `am_pm` names | hy_AM, sm_WS and su_ID define `am_pm` as two empty strings and format a time with `%r`, hy_AM and sm_WS in `t_fmt`, su_ID in `d_t_fmt`; `time_put` writes 09:05:09 and 21:05:09 under `%r` both as "09:05:09 ". `time_get`'s `%p` matches the empty AM name at once, as glibc's `strptime` does, and reads the text back as 09:05:09. The PM half has no bytes to carry and no parser recovers it; the afternoon hours of these three locales do not round-trip under `%r` (`include/loc/_time_get.cc`, `do_get`). (`ref-locale.md` ch. 12) |
| An era mismatch is not a fallback | XSH's `strptime` takes the unmodified conversion "If the alternative format or specification does not exist in the current locale". A locale with era records has the alternative, so `%EC`, `%Ey` and `%EY` on text that is no era of the locale's, "19" under `%EC` in ja_JP, set `eofbit` and read nothing, per C++11 22.4.5.1.2, p13, which makes every failure of a valid `E` directive `eofbit` with unspecified members. glibc's `strptime` falls back to `%C`, `%y` and `%Y` there. A locale without era records reads the three as the plain ones, and `%Ec`, `%Ex` and `%EX` as `%c`, `%x` and `%X` where the era format is empty or missing (`include/loc/_time_get.cc`, `__rw_read_era`; `src/time_put.cpp`, `__rw_get_timepunct`). (`ref-locale.md` ch. 12) |
| A zeroed `tm` lies in an era | `%Ey` reads its year in the era the date in the `tm` lies in, since the `tm` is the only carrier between two `do_get` calls and holds no era. A zeroed `tm` is 1900, which lies in an era under ja_JP (明治 33), th_TH (พ.ศ. 2443) and zh_TW (民前 12), so a lone `%Ey` is determined and p13 does not apply: "8" alone is 明治 8, 1875, under ja_JP. `%EC` before or after it moves the year into the era it names (`doc/notes/analysis-time-get-state.md` chapter 9). (`ref-locale.md` ch. 12) |
| `%Ey` reads four digits | `%Ey` reads up to four digits, 0 to 9999, without a sign, as glibc's `strptime` does: an era's year runs to 2569 under th_TH and 1872 under ja_JP's 西暦. A year outside its era sets `eofbit`, as 平成 40 does (`include/loc/_time_get.cc`, `__rw_read_era_year`). (`ref-locale.md` ch. 12) |
| An era is every record of its name | ja_JP writes each era after 明治 as two records with one name: a first-year record whose format writes 元年, and a record from the next January. Their offsets agree on one count. The era of the reader's lookup and of `%Ey`'s bounds is the union of the records of the name, from the earliest bound to the latest. C++11 22.4.5.1.2, p13 sets `eofbit` where the input does not "unambiguously determine" the members, and 令和 1 names one year: "%EC %Ey" reads "令和 01", which `time_put` writes for 2019-06-01, as 2019. glibc bounds the year by each record alone and cannot read it (`src/era.cpp`, `__rw_get_era_span`). (`ref-locale.md` ch. 12) |
| A plain digit under `%O` only where no symbol exists | `time_get` reads an `%O`-modified value as the locale's alternative symbol where the table defines one, and as ordinary digits only for a value beyond the table. The table is a list of whole numerals, not a digit set: lzh_TW's 32 entries run 〇 to 卅一, and 45 has no spelling in them. That is the inverse of `time_put`, which writes the symbol below the table and ordinary digits from its end, as glibc's `strftime` does, so the reader takes the one text the writer produces for each value: "三" under `%Od` reads 3 and "3" fails; "45" under `%OM` reads 45. glibc's `strptime` takes ordinary digits for any value and latches that choice for the rest of the call. The one text the reader takes that the writer does not produce is "0" for value 0 where the first symbol is empty, a table no shipped locale has. XSH's `strptime` says only that `%O` uses the alternative numeric symbols; the strict reading is kept (`include/loc/_punct.cc`, `__rw_match_name`; `include/loc/_time_get.cc`, `do_get`). (`ref-locale.md` ch. 12) |
| Compiler types beyond 2003 | `long long` and `unsigned long long` are supported wherever the compiler has them: `numeric_limits`, the `num_get` and `num_put` virtuals and their public callers, the stream inserters and extractors, `abs` and `div`. 14882:2003 does not name the type; the library follows the compiler's type set, and the `LONG_LONG` characterization decides, not a policy switch. The extra virtuals widen two facet vtables; a program that overrides the 2003 virtuals is unaffected (`include/rw/_config.h`, `etc/config/src/LONG_LONG.cpp`). (`ref-locale.md` ch. 10) |
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
| 23 | Integer extraction checks the range of the destination, sets `failbit` and clamps overflow in the narrowing helpers. (`ref-locale.md` ch. 10) | `include/loc/_num_get.h` |
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
and 64-bit reentrant shared builds. The pinned tables contain 268
programs apiece. No table records a failed assertion, a non-zero
exit or a signal. **These are test counts, not
percentages of the standard implemented.** Assertions have unequal scope, and
process failures can prevent later assertions from running.

| area | current evidence and its limit |
|---|---|
| String range self-aliasing | Repaired; the relevant assignment, insertion and replacement tests and regressions pass in the recorded six configurations. This is evidence for the repaired path, not every string requirement. |
| Locale facets | The `num_get`, `time_get` and `money_get` rows pass. `num_get` and `money_get` grow their digit buffers instead of failing or overrunning, and both throw `bad_alloc` where the buffer size would wrap, `num_get` since e2bc861d. `num_put` and `money_put` do not grow theirs: a grouped or long value overruns them (`TODO`, four entries). (`ref-locale.md` ch. 10 and 12) |
| Numerics | The limits, math and valarray rows pass; the failures were test defects and a driver comparison. The extensions row is gone with the switch. |
| Memory/lifetime symptoms | The intermittent string-iterator result was the test driver reading past short buffers, and the `21.cwchar` exit-time corruption a test's use of a write-only stream; both are repaired. |
| Streams | The file streams have the standard's interface only: the additions from classic iostreams (descriptor and `FILE*` constructors, `fd ()`, `attach ()`, `detach ()`, extra open modes, a permissions argument) are gone. `basic_ios::flags ()` meets the postcondition of `init` in every configuration since the locking extension left the format flags. `setbase` and the clearing of `basefield` follow the standard since the `setbase` extension and `bin` were removed; `27.std.manip` pins them. |
| Reentrant locale use | The sanitizer investigation found races and use-after-free in the locale; they are repaired, and each repair has a multithreaded test. The eleven gated locale MT programs pass in every table: they took sizes of their own on 2026-10-13, and six older ones were retired on 2026-10-19 (`ref-test-baseline.md` chapter 3.2). C++03 did not itself specify the later thread library and memory model. |
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
Brought up to date with the tree of 2026-11-02 by Claude.
