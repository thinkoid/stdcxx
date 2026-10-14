# After strict ANSI: the decisions the entry took and the ends it left

The TODO entry "Strict ANSI as the rule, the additions outside the
standard gone" closed on 2026-10-14 after 22 commits, a139fb29 through
f402f64c. This note is the review brief: what the entry did in one
place, which calls the sessions made on their own reading of the
entry's rule rather than on the maintainer's word, and every loose end
the work surfaced but did not settle. The outcome itself is recorded
in chapter 2 of [the conformance note](ref-conformance.md); the
reasons for each removal are in the commit messages. This note does
not repeat them beyond what the open questions need.

## 0. tl;dr

- **Done.** The strict switch is gone, nothing defines or reads an
  `_RWSTD_NO_EXT_*` macro, fourteen additions left with their code,
  tests and examples, and the gates that covered permitted choices
  and defect resolutions lost their dead branch. Every commit built
  and ran in the x86_64 GCC debug configuration with the suite table
  equal to the pin, or re-pinned where the change said it would move.
- **Three calls to review** (chapter 2): the zero/one digit parameters
  of the `bitset` string constructor stay as LWG 396's resolution; the
  library's own callers of `gbump` and `pbump` keep a `ptrdiff_t`
  path through protected `_C_gbump` and `_C_pbump` rather than
  narrowing to `int`; the staying gates went one commit per gate
  group.
- **Two findings already in TODO** (chapter 3): the one-at-a-time
  input iterator insert of `deque` and `vector` admits in its own
  comment that it misses the strong guarantee of 23.2.4.3, p1, and the
  conforming branch left with the gate; the five pins other than
  x86_64 15D still carry the old rows for the tests that moved.
- **Six ends not in TODO** (chapter 4): `long long` stays while the
  sort's own rule sent the other C++11 text away; the stream truth
  conversion departs from 27.4.4.3's `operator void*`; the replaceable
  allocation functions in `src/memory.cpp` compile to nothing on every
  supported compiler; the `time_get` and `num_get` test coverage that
  the extensions carried is gone with them and nothing asserts the
  absence of extensions any more; two examples show DIFF and the
  library build has ten `-Wnonnull-compare` warnings, both predating
  the entry; the `_RWSTD_NO_CLASS_PARTIAL_SPEC` probe now carries the
  cv-qualified `numeric_limits` alone, for a compiler the tree does
  not support.

## 1. What the entry did

The sort was made in a139fb29 and held through the work: an addition
is anything the 2003 text does not name, and it leaves; a defect
resolution adopted after 2003 (LWG 434, 559, 568) or a choice the
standard leaves to the implementation stays, with the macro that
once disabled it gone. C++11's text is an addition against 2003, so
the `bitset` constructors from character pointers and the `time_get`
format members left.

| commit | what |
|---|---|
| a139fb29 | the switch, `17.extensions`, the C masking macros unconditional |
| 9156fc3b | `ios_base::failure` subclasses |
| c91e001f | `stringbuf` character pointer constructors and `str` overloads; the two-argument `str` stays as private `_C_str` |
| 8fda5edb | deep-copy `basic_string::copy` |
| 444888d6 | `num_get` members on `short` and `int` |
| 6266fe18 | the `num_put` test gate |
| b7443c95 | `time_get` format members, private as the non-virtual `_C_get` pair |
| af1b3a88, ff4a97ba | `void` forms of `count`, `count_if` and `distance`, with the last no-partial-specialization fallbacks in the examples |
| 15c7d99e | comparison operators of `vector<bool>::reference` |
| aebf2e2a | the allocator of const elements |
| 997b23cb | `bitset` constructors from character pointers, `string` and `wstring`, with the `int`, `unsigned` and `long` companions |
| 48f1ae08 | `gbump` and `pbump` on `int`; the library's callers on protected `_C_gbump` and `_C_pbump` |
| cf434369 .. d627a991 | eight commits stripping the staying gates: in-place assign and insert, keep width on failure, cv-qualified limits, portable random, stream conversion, `bitset::to_string`, the five primary templates, operator new |
| f402f64c | chapter 2 of the conformance note and the README policy paragraph |

Three members kept a private form for the library's own use, the
precedent set in c91e001f: `_C_str`, `_C_get`, `_C_gbump` and
`_C_pbump`. The entry did not touch `src/mapfile.gcc`; the map lists
the public `int` forms of `gbump` and `pbump` and all four are inline.

Suite table rows moved by the entry, all in
`doc/notes/baseline/x86_64-15D.txt`: `17.extensions` gone;
`18.exception` 1097 for 1109; `21.cwchar` 118 for 66; `21.cwctype`
294 for 260; `22.locale.num.get` 51894 for 63720; `22.locale.time.get`
726 for 2010; `23.bitset.cons` 1677 for 2347. No failure count moved.

## 2. Decisions taken by the sessions, for review

These three were made by reading the entry's rule back at the case,
not on the maintainer's word. Each is reversible with a small commit.

### 2.1 The zero/one digit parameters of the `bitset` string constructor stay

`bitset (const basic_string&, pos, n, _CharT __b0 = '0', _CharT
__b1 = '1')` was ungated and sits beside the removed constructors.
A survey ahead of the commit proposed taking the two parameters with
the rest as C++11's text. The commit kept them: they are LWG 396's
resolution against C++03, and the entry's own sort keeps defect
resolutions, as it did for 434, 559 and 568. The distinction between
"C++11's text" and "a resolution later folded into C++11" is the
entry's, and it is defensible, but it was never put to the maintainer
as a yes or no. If the answer is that C++03's signature is the rule,
the two parameters go and `23.bitset.cons` loses the cases that pass
them.

### 2.2 `gbump` and `pbump` keep a wide path privately

27.5.2.3.1 declares both on `int`. The tree took `ptrdiff_t`, and its
eighteen callers in seven files pass stream sizes and offsets:
`xsgetn`, `xsputn`, the `stringbuf` growth path, `seekoff`,
`getline`, the string extractor. On a 64-bit target a string stream
past 2 GB would have silently wrapped the advance through an `int`
parameter. 48f1ae08 made the public members `int` and routed the
callers through protected `_C_gbump` and `_C_pbump` on `ptrdiff_t`,
each asserting the buffer stays valid. libstdc++ has the same shape.
The alternative, the standard signature alone and truncation above
`INT_MAX`, is what the entry's rule literally asks for; the commit
judged the private wide path to be the `_C_str` and `_C_get`
precedent rather than an extension, since nothing outside the class
can call it.

### 2.3 One commit per gate group

The staying gates' dead branches went eight commits, one per gate
group, each with a full build and run, rather than one per file or one
for all. The in-place gates of `deque` and `vector` went together as
four macros of one shape; the five `*_PRIMARY` gates likewise. Nothing
depends on this beyond bisectability.

## 3. Findings already filed in TODO

### 3.1 The input iterator insert against 23.2.4.3, p1

Under Library. With `*_INSERT_IN_PLACE` the kept branch of
`__rw_insert_range` for an input iterator inserts one element at a
time, and the `vector` version's comment says it does not satisfy
the strong exception safety requirement of 23.2.4.3, p1; the `deque`
version is the same shape. The branch that met the requirement, a
temporary container filled first and inserted whole, was the gate's
other side and is in history only (cf434369's parent).
`23.vector.modifiers.cpp` turns exceptions off below forward iterators
on that account. The entry sorted the gate as a permitted choice;
the comment says otherwise. The fix is a decision, then either the
temporary returns as the only code or the comment is corrected to
say what the standard permits.

### 3.2 The five other pins

Under Certification. Only the x86_64 15D pin carries the rows of
chapter 1; the five other baseline files have the old rows and will
mismatch their next run until patched in their own layout, as 15D was.

## 4. Ends not filed

### 4.1 `long long` stays while C++11's text left

The sort of a139fb29 lists `long long` among what stays, and the
`num_put` gate on its assertions went as a test-only gate. Yet the
same message sends the `bitset` constructors and the `time_get`
format members away because they are "C++11's text, not resolutions
against 2003". `long long` is C++11's text too, from C99, and the
strict switch used to undefine `_RWSTD_LONG_LONG` for exactly that
reason. The two halves of the rule do not meet here. Under the strict
reading the `long long` overloads of `num_get`, `num_put` and the
stream inserters and extractors would leave; under the practical
reading every supported compiler has the type and the overloads are
what a program written for the platform expects. The entry chose the
practical reading without saying so.

### 4.2 The stream truth conversion is not 27.4.4.3's

`basic_ios` converts to a private pointer-to-member type where
27.4.4.3 writes `operator void*`. The gate went in e72719aa and the
pointer-to-member form is now the only one. A stream can no longer be
deleted or passed as an integer by accident, which is the point, but
`void* p = cin;` does not compile, which a conforming program may
write. The sort credited this to LWG 568; the resolution of that
issue should be re-read against the tree before the row stands as a
defect correction rather than a departure. C++11's `explicit operator
bool` is a third shape the tree does not have.

### 4.3 The replaceable allocation functions compile to nothing

`src/memory.cpp` defines the `operator new` and `operator delete`
variants a support library lacks, behind the `_RWSTD_NO_OPERATOR_*`
probes. None of those probes fires under GCC or Clang, so on every
supported compiler the file contributes no allocation function; the
old open question in `_config.h` about defining them from the library
was moot and went in d627a991. What remains is dead on the supported
matrix and live only for a compiler the tree does not build with.
The same holds for the other compiler-characterization probes, of
which `_RWSTD_NO_CLASS_PARTIAL_SPEC` is the sharpest case: after
7e4d4199 it is the sole condition on the cv-qualified
`numeric_limits` specializations, and it is defined by no supported
compiler. Whether such probes stay as portability insurance or leave
as unreachable code is a question for the tree as a whole, not this
entry.

### 4.4 Coverage that left with the extensions

`22.locale.time.get` lost its POSIX specifier walk and the whole
generated-locale test with its `LC_TIME` generator, about a thousand
lines, 726 assertions for 2010. The `strptime` engine is still in the
library as `_C_get`, reachable only through the standard members, and
the remaining assertions exercise it through them. `22.locale.num.get`
lost the `short` and `int` cases, 51894 for 63720. The rule given for
this was that an extension is non-standard by definition and the
orphaned coverage does not matter; the fact stands that the engine's
specifier handling has less direct exercise than before. Separately,
`17.extensions` retired with the switch, so no test asserts that an
extension is absent; the check is the empty
`git grep _RWSTD_NO_EXT_` and the compiler on each removal.

### 4.5 Pre-existing and untouched

Two examples, `numpunct` and `time_get`, show DIFF against their
expected output in the x86_64 GCC debug build, and did before
a139fb29. Examples are not certified; nothing in the entry looked
into them. The full library build carries ten `-Wnonnull-compare`
warnings in `ios_base::width`, the same ten before and after. One
blank line after the opening brace of `test_random_shuffle` in
`25.random.shuffle.cpp` slipped through 397e6233's hunk trimming;
the no-reformatting rule leaves it.

### 4.6 The method, for the next sweep

The gate stripping was a scratchpad script that resolved
`#ifndef`/`#ifdef MACRO` blocks nesting-aware and reported compound
`#if` lines for hand editing, then a pass trimming the blank lines it
left at braces. The verification was one script running the five
build phases serially, the two `make run` targets, the pin derivation
and a `diff -w` against the baseline. Neither is in the tree; both
are twenty lines and worth rebuilding the next time a family of
macros goes.

## 5. Where the record is

- The outcome: chapter 2 of [the conformance note](ref-conformance.md).
- The reasons per removal: the commit messages, a139fb29 through
  f402f64c, each with its measured rows.
- The open items: `TODO`, Library chapter for 3.1, Certification for
  3.2; the ends of chapter 4 are here only until one is taken up.
- The switch's history: [the strict ANSI primer](primer-strict-ansi.md)
  on what the compiler macro itself does and does not promise.
