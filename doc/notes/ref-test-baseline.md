# The test-suite baseline

As of 2026-09-28 (a44256a6).

The whole suite, run under its own harness on the current toolchain.
It is the reference for every change after it: which tests fail, by
how much, and why, where that is known. The numbers are pinned under
`doc/notes/baseline/`, one file per configuration. Each file is the
harness's own table without the timing columns.

## 0. The short version

- Every test builds. The harness runs 272 programs per configuration.
- 17 assertions fail in every configuration, under both compilers.
  Most failures of the first tables are repaired. Chapter 3 has each
  one.
- What still fails, by cause:
  - `27.basic.ios`, `27.filebuf` and `27.std.manip`, 17 assertions,
    and the regression test `27.ostream.inserters.stdcxx-51`, which
    aborts. All configurations. Not investigated; chapter 3.11 records
    what they report. None has a `TODO` entry.
  - `21.string.stdcxx-162` in 15D. Its fix breaks the 4.2.x binary
    interface and waits for the next minor version (chapter 4).
  - The locale MT rows of 15D. They measure two limits of the
    harness, not the library (chapter 3.4).
- Two harness defects are fixed: a wrong source directory when
  `TOPDIR` was unset, and a test whose name collided with the
  harness's output file (chapter 3.1).
- The input files of two locale tests never existed in this
  repository. They are regenerated (chapter 3.2).
- The `std::locale` race the revival set out to find is fixed. The
  MT tests that caught it pass when run bare at 16 threads.
  `analysis-locale-mt-race.md` has the work.
- One library defect in strings is fixed: a source range inside the
  string being modified (chapter 3.7).
- The locale facet rows (chapter 3.9) and the Numerics rows (chapter
  3.10) are repaired. The causes were mostly test and driver defects,
  plus two facet limits.

Toolchain: GCC 16.2.1, Clang 22.1.8, glibc 2.44, x86-64 Linux.

## 1. Running the suite

Run `make run` from a build directory's `tests` subdirectory. It runs
every built test through `bin/exec`, the harness, and prints one row
per program and a summary. The timeout comes from the build's
`makefile.in`: `RUNFLAGS = -t 60`.

The rule exports `TOPDIR`, `TMPDIR`, `TZ` and `LD_LIBRARY_PATH`. To
run a test by hand, set `TOPDIR` to the source tree and work from
the build's `tests` directory. The locale tests exec
`../bin/localedef`.

Set only the timeout, and set it in `makefile.in`. `RUNFLAGS` on the
`make` command line replaces the whole variable. It drops the
`--compat` and `--ulimit` options the rules append, and the tables
come out different.

The timeout was 300 seconds from 2007. It was cut to 30 on
2026-09-16, on the belief that the MT locale tests hung. They do
not: each threaded section runs to the test's own 60-second soft
timeout. The timeout is 60 seconds since 2026-09-27 (efa441f7). The
slowest single-threaded test, `22.locale.codecvt`, takes 24 seconds
in the 32-bit debug build.

The status column separates process failure from output
classification:

- A nonzero exit or a signal is reported before the output is read.
  `ABRT`, `SEGV` and the like name signals. `HUP` is a timeout.
- After a zero exit the harness looks for the driver's summary.
  `NOUT` means no output. `FORMAT` means no summary it recognizes.
- `EXEC` is a file that could not be executed. `COMP` is a test that
  did not build.

The harness's own zero exit means it finished, not that every test
passed. Read the rows and the summary.

Some tests succeed without a driver summary. The 82 regression tests
under `tests/regress` are plain programs that assert and exit. They
print nothing, so a pass is `NOUT`. Several driver self-tests report
on their own; chapter 3.6.2 has their contracts and their `FORMAT`
and `NOUT` rows. These are known conventions for those programs. A
test that uses driver assertions still needs its summary; exit zero
alone does not show success.

The harness writes each program's output next to it as
`<program>.out`. That collided with the test `22.locale.codecvt.out`,
named by the suite's `clause.facet.member` convention. The harness
built it, overwrote it with the output of `22.locale.codecvt`, and
failed to execute the text file. The test is now
`22.locale.codecvt.do_out`. It ran for the first time with the
rename: 1494 assertions, all passing.

## 2. The reference

Three configurations, named as the build system names them. The
number is the debugging, optimization and thread-safety level. `s`
is an archive, `d` a shared library. Lowercase is 32-bit code,
uppercase 64-bit.

| configuration | file | programs | assertions | failed | non-zero exits | signalled |
|---|---|---|---|---|---|---|
| 11S, debug, archive, 64-bit | `baseline/x86_64-11S.txt` | 272 | 10,105,653 | 17 | 0 | 1 |
| 11s, debug, archive, 32-bit | `baseline/i386-11s.txt` | 272 | 10,105,535 | 17 | 0 | 1 |
| 15D, debug, shared, threads, 64-bit | `baseline/x86_64-15D.txt` | 272 | 10,105,719 | 17 | 9 | 7 |

Chapter 4 lists where the three differ.

The tables were last measured in full on 2026-09-28, at a44256a6,
from build directories configured afresh. The measurement closed the
removal of the C library function probes (f43be017..a44256a6). Every
row matched the earlier tables but two kinds: `21.cwchar` under
Clang, now repaired (chapter 3.8), and the harness rows of 15D, which
vary from run to run (chapter 3.4).

Earlier refreshes followed each repair in chapter 3, and each moved
only that repair's rows and the unstable MT rows of 15D.

The last measurement on a current toolchain before this one was made
by hand in 2026-09, before every test linked: 10,066,804 assertions,
1,417 failures. The 2008 nightly results were never clean either;
the suite ran in the seventies percent on the platforms of the day.
A failure in the tables is a fact to record, not a regression to
chase, until its entry says otherwise.

## 3. Findings

Each finding states the fact, the cause where it is known, and the
fix. Applied fixes are in the tree. Intended ones are `TODO` entries.

### 3.1 The harness: `TOPDIR` and the output file name

Fact. When `TOPDIR` is unset, `tests/src/locale.cpp` derives it from
`__FILE__`. It truncated at the character before the last slash,
which turns `.../tests/src/locale.cpp` into `.../tests/sr`. Every
locale test run by hand then failed to open `tests/sr/etc/nls/...`.
The fallback was wrong from its first commit in 2008. `make run`
exports the variable, so the nightly runs never reached it.

Fix, applied. The fallback strips the file name and the two
directories above it. Check: `22.locale.codecvt` run bare, without
`TOPDIR`, from the build's `tests` directory, finds the locale
sources and runs `localedef` with the right paths.

Fact. `22.locale.codecvt.out` collided with the harness's output
naming (chapter 1). Fix, applied: renamed `22.locale.codecvt.do_out`.

Fact. `22.locale.codecvt` also read `TOPDIR` itself. It reported the
variable missing, then used the null pointer: a segmentation fault
in a bare run. Fix, applied: the driver's fallback is one function,
`rw_topdir ()`, and the test calls it
(`analysis-codecvt-invalid-state.md`, chapter 6).

### 3.2 The input files under `tests/etc`

Fact. `22.locale.codecvt` reads `tests/etc/codecvt.<locale>.in` for
five locales. `22.locale.collate` reads `tests/etc/collate.<locale>.in`
for six. No branch of the repository ever had `tests/etc`. Both tests
came from the vendor's Perforce repository in May 2008, "warts and
all" in the commit's words, and their fixtures stayed behind. The
missing files caused 20 of the 21 codecvt failures and both collate
failures, in every configuration.

Fix, applied. The files are regenerated from what the tests expect.
The codecvt table fixes each file's byte and character count. The
collate table fixes its encoding. `analysis-test-fixtures.md` records
how they were made and the order oracle for the collate lists.

With the files in place, the codecvt test showed three defects of its
own, repaired with them:

- its `length` check compared against a C locale it had not set;
- its `length` expectation counted characters where the standard
  counts bytes;
- a synthetic charmap named after the native codeset made `localedef`
  reuse a single-byte stub for `ja_JP.UTF-8`.

One more codecvt assertion predated the files. It went with the abort
of chapter 3.3.

The collate test showed a library defect. The narrow transform could
not spell a weight that is a multiple of 127. The letter "l" in
Croatian and ko kai in Thai sorted last in the `char` half of the
test, 101 lines. The `wchar_t` half sorted all six lists as glibc
does. Repaired: the spelling is prefix-free, and it sorts below the
IGNORE mark of the position orderings, which used to be the same byte
as the run byte (`analysis-collate-transform.md`). The test builds
the mark's case. Its own `exit (1)` for an unset `TOPDIR` is gone.

### 3.3 `22.locale.codecvt` on a corrupt `mbstate_t`

Fact. The test's last section passes a corrupt `mbstate_t` to the
facets. In a debug build it expects the library's assertion and
catches the `abort()` with a `SIGABRT` handler that jumps back. In an
optimized build it expects the error result. glibc 2.41 changed
`abort()`: after the jump `SIGABRT` stayed blocked, and the second
assertion killed the process. Hence `ABRT` in every debug
configuration. The optimized `8S` build met its half of the contract
on half the library's paths. The libc-backed wide facet looped inside
glibc's `mbrtowc` on the corrupt state.

Fix, applied. The handler saves and restores the signal mask. Every
facet path returns the error result when its state check fails; the
assertion stays in debug builds. The thread-safe debug build skips
the two libc-backed facets, whose check sits under the locale lock.
That skip is the only branch of the test that depends on the build,
and it is why 15D runs 306 assertions where the others run 322.
`analysis-codecvt-invalid-state.md` has the whole of it.

### 3.4 The MT locale tests

Fact, 2026-09-15. In 15D the locale MT tests ended differently on
every run of the same binary. Three bare runs of
`22.locale.numpunct.mt`: the first died on the test's assertion in a
thread, a thread reading another locale's `truename` (line 147); the
second died of an uncaught `std::runtime_error` thrown in a thread,
a locale that failed to construct; the third passed.

Cause, found. A ThreadSanitizer build named it: facet caches written
by every thread with no synchronization, facet data and facet slots
published without ordering, a facet id race, and a reference count
updated two incompatible ways. The fixes are in the tree, each with
a test where one could fail; `22.locale.use_facet.mt`,
`22.locale.time.put.libc.mt`, `22.locale.id.mt` and
`22.locale.facet.mt` are theirs, and pass everywhere.
ThreadSanitizer reports from a round of the locale MT tests went
from 979 to 1, the global locale's `ginit` flag, which the `TODO`
entry on atomic integer flags covers.
`analysis-locale-mt-race.md` has the detail.

Fact, now. Fourteen older locale MT tests, `22.locale.codecvt.mt`
through `22.locale.time.put.mt`, still end in 15D with `1`, `9`,
`23`, `31`, `ABRT` or `HUP`. They measure the harness:

- **The address-space limit.** The harness runs every test under a
  1 GiB limit (`--ulimit=as:1073741824`,
  `etc/config/GNUmakefile.tst`). A test runs one thread per
  processor, 32 on the machine the tables come from. Each thread
  takes an 8 MiB stack and a 64 MiB `malloc` arena. The 24th stack
  fails to map (`ENOMEM`), `rw_thread_pool` reports the pool failing
  to start, and the test exits nonzero.
- **The timeout.** A test that starts runs each threaded section to
  its own 60-second soft timeout, and has up to three sections. The
  harness's 60 seconds cut it short: `HUP`.

Check, at ae1fbe04. Run bare at 16 threads under the same
address-space limit, all sixteen locale MT tests of that commit pass,
these fourteen among them, in 2 to 143 seconds.

Intended. Whether to change the limit or the default thread count,
and how an MT test should provoke contention quickly instead of by
repetition, is the `TODO` entry "MT tests that provoke contention
instead of waiting for it". Until then the 15D rows of these tests
record the harness, not a reference.

### 3.5 `23.bitset.cons` and `25.reverse` in 11S

Fact. In 11S, `23.bitset.cons` failed 240, 440 and 680 assertions in
three runs of one binary, and 0 in others. 15D and Clang showed the
same spread. `25.reverse` died of a segmentation fault in 11S only.
Both passed in 11s.

Cause: two unrelated test defects, neither in the library.

- The bitset test passes an expected exception as a small integer in
  the `const char*` that otherwise carries the expected bit string.
  It told the two apart with `int (bitstr - (char*)0) < 3`, which
  truncates the pointer to `int`. On a 64-bit target a heap address
  with bit 31 set turns negative, and the test expected a throw from
  a valid call. Which strings sat above that line depended on where
  address-space randomization put the heap. With randomization off
  the binary passed every time. Valgrind reported nothing; nothing
  was uninitialized.
- The reverse test formatted "the mismatched element" as an assertion
  argument, `xsrc [nsrc - i - 1]`, evaluated whether or not the
  assertion fails. On success the loop ends with `i == nsrc`, and the
  index is -1: one element before the array. In the archive build
  that was a buffer the driver had just freed (270 invalid reads
  under valgrind), or, at the start of a mapping, the fault.

Both idioms date from the tests' first commits, 2006 and 2008.

Fix, applied. The comparison at `ptrdiff_t` width. The mismatched
element read only when there is one. Check: eight runs of the bitset
test with randomization on, 2347 of 2347 each; the reverse test
under valgrind with no error and a clean exit, in 11S.

The run that pinned the tables after this fix showed a new
intermittent. `21.string.iterators` failed 6 of 5725 in 11S, all in
the `UserChar` instantiation: `begin () const` and `end () - 1`
reported as returning a null element. The row had been clean in every
earlier table and passed 22 further runs, with randomization on and
off.

Cause, found 2026-09-18 with an address-sanitized build of the test
and of the driver's `char.cpp`. Another test defect. The test handed
`rw_match` a single `char` on the stack as the expected value. The
directive parser behind `rw_match` looks past every character for a
`@` repeat count, so it read the two bytes after the local. They were
the low bytes of a stale pointer in the same stack slot, randomized
with the address space. When they spelled `@` and a digit, the parser
took a repeat count from the neighbour.

The "null element" was a second artifact. `%{#c}` reads an `int` from
the argument list. The `UserChar` result is a 32-byte struct passed
in memory, so the directive printed the first bytes of its
`long double` member.

The one-character idiom had been repaired in `21.string.access` and
`21.string.copy` in 2007, and missed in `21.string.iterators`,
`21.string.capacity` and `21.string.io`. The narrow overload parses
its second argument too. That caught `21.string.access` reading one
element past the accessed character, and `21.string.io` one past the
test streambuf's exactly sized output buffer.

Fix, applied. Every buffer the string tests hand to `rw_match` is
terminated: a two-element array for a single character, a sentinel
after the test streambuf's output buffer, as its input buffer already
had. Check: the sanitized builds of the three tests with stack locals
report nothing; every string test, and the two istream tests that
share the streambuf, run clean under valgrind in 11S; the rows are
unchanged in all six tables. The `%{#c}` artifact is closed too. The
string tests pass the directive `char_value ()` from `rw_char.h`, the
character's value as an `int` for each character type. `0.char` pins
the helper against the directive.

### 3.6 The driver's self-tests

Fact. `0.inputiter`, `0.outputiter` and `0.new` aborted without a
message. They trigger assertions on purpose, to test the driver's
iterators and replacement allocation functions. They catch `SIGABRT`
and jump back with `longjmp`. The iterator tests close `stderr`, and
the allocation test disables its expected diagnostics: hence the
silence.

Cause. A signal is blocked while its handler runs. `setjmp` does not
save the signal mask on glibc, and `longjmp` skips the handler's
return, so `SIGABRT` stays blocked. Reinstalling the handler does not
unblock it. Before glibc 2.41, `abort()` unblocked the signal before
raising it. The change described in
`analysis-codecvt-invalid-state.md`, chapter 2, removed that. The
second assertion now ends the process.

Fix, applied. All three tests use `sigjmp_buf`, save the mask with
`sigsetjmp (env, 1)` on both paths, and restore it with
`siglongjmp`. The assertions and their expected outcomes are
unchanged.

Check, 2026-09-17. All three rebuilt and run directly and through
`make run RUN='0.inputiter 0.outputiter 0.new'`, in `11S`, `11s` and
`15D`, with GCC and Clang. All three were `ABRT` before. Now:

| test | exit | assertions | failed |
|---|---|---|---|
| `0.inputiter` | 0 | 20 | 0 |
| `0.outputiter` | 0 | 0 | 0 |
| `0.new` | 0 | 26 | 0 |

The output-iterator test counts diagnostics only when a case fails,
so zero assertions is its pass. Copies of each test were made to miss
an expected assertion and, separately, to raise an unexpected one,
under GCC and Clang in `11S` and `15D`. Each copy finished with one
failed assertion and its diagnostic. The iterator tests returned 1.
`0.new` returned 0, as its callback always does, so its summary is
the oracle. The original sources still abort when rebuilt, as
controls.

`0.printf` and `0.char` also failed under GCC: four and one
assertions on misaligned wide-string inputs. 57c34d96 gives the empty
inputs named arrays, which keep their alignment despite GCC PR127457.
The tables now have `0.char` at 487 of 487 with 8 warnings, and
`0.printf` as `FORMAT` with exit 0; its own summary reports all 1876
passed (chapter 3.6.2).

#### 3.6.1 Replace the custom pattern matcher

`rw_fnmatch` belongs to the test-support library, not the standard
library. Its only caller outside its self-test is `rw_locale_query`,
which filters canonical locale names. It came to trunk in December
2007 (STDCXX-683, `c6496b81`) with its header and self-test, and to
4.2.x in April 2008 (`222c045b`). A portable stand-in for POSIX
`fnmatch` served the old platform matrix. The supported targets are
now Linux with GCC and Clang. libc has the function, and the
self-test already depended on it. The portability reason for a
second implementation is gone.

The deleted parser had several independent defects:

- An unterminated bracket expression could return the pattern's NUL
  as a closing bracket. The caller stepped past it and read beyond
  the allocation. `[a` against `a` gave two invalid reads under
  Valgrind and could report a false match.
- An unmatched `[` did not fall back to a literal. The self-test
  expected failure for `[` against `[` and `[a` against `[a`,
  contradicting its native oracle.
- A leading literal `-` or `]` kept later members from matching;
  negated lists mishandled them too. A trailing `-` was read as a
  range operator, not a literal.
- Escape state never reset inside brackets. Closing-bracket scans
  checked only the previous byte, not the parity of consecutive
  backslashes. Ranges and the closing `]` were misread.
- Named character classes, equivalence classes, collating symbols and
  multibyte matching were absent, despite the POSIX contract claimed.

The helper now calls POSIX `fnmatch` with flags zero. It keeps its
assertions, its ignored third argument and its 0/1 result. Native
matching uses the current locale rather than bytes, which also brings
the missing classes and multibyte support. No new library dependency
or compiler option.

The self-test states expected results instead of comparing two calls
to the same native function. Both wrong expectations are corrected.
14 checks cover the bracket defects and an exact-size heap allocation
for memory checking: 337 in all.

A scratch comparison during the investigation used 1,911,679 pairs of
short ASCII patterns and strings. It found 14,476 disagreements, all
involving brackets. The corpus includes malformed inputs and is not
part of the suite; it is not a count of distinct POSIX violations.
The regression cases establish the defects.

Check, 2026-09-17. The self-test passes all 337 cases directly and
through the harness in `11S`, `11s` and `15D`, under GCC and Clang.
Its row is `NOUT`: it prints only failures and returns zero. Valgrind
reports no errors. As a negative control, the same self-test linked
against the deleted parser fails 14 cases with two invalid reads,
including the exact allocation.

The test-support library and all tests were rebuilt, and `make run`
ran in all six configurations with the harness options unchanged.
The six tables were re-pinned from those runs.

#### 3.6.2 Standalone self-test reporting

Decision, 2026-09-17: keep the independent reporting. These programs
test the driver's support functions. They do not test the harness's
output parser. None calls `rw_test` or reports through `rw_assert`.

| test | successful output | failure contract | successful runner row |
|---|---|---|---|
| `0.cmdopts` | diagnostics from deliberately rejected arguments | unexpected parser or callback results set exit status 1 | `FORMAT` |
| `0.strncmp` | overload banners | a mismatch prints a diagnostic and sets exit status 2 | `FORMAT` |
| `0.valcmp` | specialization banners | a mismatch prints a diagnostic and sets exit status 1 | `FORMAT` |
| `0.braceexp` | none | a mismatch increments `nerrors`; main returns 1 if any occurred | `NOUT` |
| `0.printf` | progress and its own assertion summary | failed checks print a summary and return 1 | `FORMAT` |

The original comparison tests (`5b7a4c57`, December 2005) and option
test (`c41878f4`, December 2005) already report through the exit
status. The original brace-expansion test (`7749e93f`, February 2008,
merged in `222c045b`) says "return 0 on success, 1 on failure" and
prints only on failure. Banners and silent success are the
convention, not a missing driver integration.

Independence also fits the design. `0.cmdopts` clears and replaces
the option registry the driver uses. `0.printf` tests the formatter
the driver's diagnostics use. Reporting through libc keeps the
observation apart from what is observed. This reason is inferred from
the code; no history requires every self-test to avoid the driver,
and other self-tests use it.

The harness keeps these contracts. `run_target` in `util/runall.cpp`
calls `parse_output` only after a zero, unsignalled exit. `check_test`
and `check_compat_test` in `util/output.cpp` then classify empty
output as `ST_NO_OUTPUT` and a missing summary as `ST_FORMAT`. The
empty-output case came in `21e213d9` (June 2007) with the comment
"regression test?". Silence can be a program's normal success.

Check, 2026-09-17. The five GCC `11S` binaries ran directly with exit
zero. `0.cmdopts` printed only the expected rejection diagnostics to
stderr. The comparison tests printed banners. `0.braceexp` was
silent. Copies run through `bin/exec --compat -t 30` gave the
classifications above. Two synthetic controls, one silent with exit 7
and one printing a diagnostic with exit 9, kept statuses 7 and 9, and
the summary counted two nonzero exits. The harness returned zero on
completion.

These controls check the classification order, not every self-test
check. The failure branches were read, not mutated. The decision
changed no test or harness behaviour.

Investigation and resolution: OpenAI LeChuck.

### 3.7 Strings: ranges into the string itself

Fact. `21.string.assign`, `21.string.insert` and `21.string.replace`
failed 60, 150 and 240 assertions. Every one had a source range inside
the string being modified: `string ("abc").insert (begin (), begin ()
+ 1, begin () + 2)` expected "babc" and got "aabc". The regression
tests `stdcxx-629`, `stdcxx-632` and `stdcxx-170` aborted on the same
cases.

Cause, read 2026-09-17. Every range overload of `assign`, `insert`
and `append` is `replace`: over the whole string, over an empty range
at the point, or over an empty range at the end. A bidirectional or
random-access source goes to `__replace_aux` in `include/string.cc`.
Its in-place branch moves the tail, then copies the source element by
element, with no overlap test.

- The new-representation branch reads the old buffer before unlinking
  it, so forced growth is safe.
- The input-iterator branch stages the source in a temporary for
  exception safety, so it is safe by accident.
- Where the write lands relative to the read decides the rest:
  `assign` failed only through reverse iterators, `append` never,
  `insert` and `replace` at the front for every kind.

`insert` guarded `const_pointer` with an address test and a copy
through a temporary string. A `char*` argument is an exact match for
the member template and skipped the guard. `replace` had no guard.
The tree tried twice in 2008 (bf077568 and 966011ea, both reverted).
Each took the address of `*first` inside the template, which the list
discussion of the time showed is ill-formed for iterators that return
by value. Neither trunk nor 4.3.x fixed it afterwards. STDCXX-170 is
open upstream.

Fix, applied 2026-09-17 (a1d757e3). The generic path builds a
temporary from the source first, as 21.3.5.6 specifies. Non-template
overloads for `pointer`, `const_pointer` and, under debug iterators,
the string's own iterators route to the count overload of `replace`,
which already allocates a new representation when the source lies in
the buffer. The template forms no address of a dereferenced iterator,
so an iterator returning by value instantiates. Check: the three
tests at 100% and the three regressions exit 0 in all six
configurations; a 24-case probe that fails 11 rows against the old
headers passes. The guarded `insert` overloads were redundant after
that and are removed (937a2394), with the tables unchanged.

`21.string.stdcxx-162` was listed here once. It probes the layout
of the string body, not this path (chapter 4).

### 3.8 `21.cwchar` aborts after passing

Fact. All 66 assertions passed, then the process died at exit with
"double free or corruption (top)" in every configuration.

Cause, from valgrind: a test defect that glibc turns into a double
free. `test_file_functions` opened one stream on the null device for
writing and called every file function on it, input functions
included, in the order of the standard's synopsis. The last,
`ungetwc` after unflushed output, has no contract. A stream opened
for output takes no input. On a stream opened for update, C99
7.19.5.3, p6 requires a flush or a positioning call between output
and input.

glibc's pushback on such a stream allocates a backup buffer and swaps
it with the get area, whose base is the stream's buffer. The flush at
exit writes the pending character and resets the get area to the
buffer without leaving backup mode. The swap back at exit leaves both
pointers on the buffer, which is freed once as the backup area and
again as the buffer. The narrow pushback has the same shape and is
benign: its second free is reached only under a memory checker's
`__libc_freeres`. The wide free at exit dates from glibc 2.41, the one
on `fclose` from 2.44. A C program doing `fopen ("w")`, `fputwc`,
`ungetwc` reproduces the abort on 2.44.

Fix, applied. The input functions get a second stream opened for
reading; the order of the calls is unchanged, and both streams are
closed. Check: exit 0, valgrind clean in 11S, the row at 100% in the
three GCC configurations.

Under Clang the row then showed what the abort hid: 61 assertions,
5 failed, 5 warnings, on the const and non-const overloads of
`wmemchr`, `wcspbrk`, `wcsrchr`, `wcsstr` and `wcschr`. glibc's
`<wchar.h>` declares the C++ overloads only for GCC 4.4 and later,
and Clang presents itself as 4.2. The library now defines
`__CORRECT_ISO_CPP_WCHAR_H_PROTO` before it includes glibc's header,
and glibc declares the overloads under Clang too (1f6e5dd2). The row
is 66 assertions, 0 failed, 0 warnings under both compilers.
`analysis-glibc-wchar-proto.md` has the history.

### 3.9 Locale facets

Fact, first tables. `22.locale.num.get` failed 204 assertions: the
`wchar_t` overloads reading `unsigned long` consumed a different count
than expected. `22.locale.time.get` failed 33: `get_date
("01/01/2000")` consumed 10 characters where 8 were expected, and
`get_weekday ("torsdag")` was not recognized. `22.locale.money.get`
failed 16: `long double` reads with `showbase` ended in `failbit`.

Read against the library on 2026-09-18, none of the three was what
the fact said.

`22.locale.num.get`, two causes.

- 108 failures were the test's. A 2008 commit that silenced 64-bit
  conversion warnings rewrote `sizeof lmin - 1` as
  `INTSIZE (lmin - 1)`, the size of a pointer, at 22 sites: the limit
  strings of `long`, `unsigned long` and `void*`, and the
  `long double` cases. They expected 8 characters consumed in 64-bit
  and 4 in 32-bit, for every type.
- 96 were the facet's. `num_get` collected digits in a 130-byte stack
  array, sized for the widest integer in base 2, and set `failbit`
  when the input outgrew it, under a FIXME from the 2005 import. The
  test, from the same year, feeds it a `long double` of
  `LDBL_MAX_10_EXP + 1` zeros and a grouped `long` of 257 characters
  and more, on purpose.

Fix, applied. The array, and the array of group sizes beside it, move
to one heap block of twice the size when full; the pointers into them
carry over. The exit test is the tree's own: the pointer compared
with the array and freed if it moved, held in a guard so that early
returns and a facet's exception take it too. The row is 65508/0
everywhere. The 96 extra assertions are the grouping loop running all
nine lengths instead of stopping at the first failure. Valgrind on the
test in 11S is clean.

`22.locale.time.get`: 32 of the 33 were the driver's.
`rw_locale_query` kept its result in a static buffer whose length was
never reset. Every call appended to the last result, and a query
matching nothing returned the previous answer. The test asks for an
English, a German and a Danish locale in turn. On a machine with
`en_US` alone, the German and Danish queries came back `en_US.utf8`
and their sections ran under it: "Sonntag" was consumed as the S of
Sunday. With the length reset on entry, the two sections are skipped
where their locale is absent, as the test intends. The same defect
inflated `22.locale.ctype.tolower`, which queries on every loop entry:
1538 assertions to `toupper`'s 1028, now equal.

The last failure expected the English "%x" to be 8 characters, the
length of "%m/%d/%y" on the vendor Unixes of 2008. glibc's `en_US` has
formatted it as "%m/%d/%Y" since 2000, as does the tree's own `en_US`
source. The length now comes from `strftime`, as the same function
already sized "%X".

With German and Danish locales generated into a scratch directory
(`localedef`, `LOCPATH`, and a `locale` shim on `PATH` that lists
them, since glibc's `locale -a` never lists `LOCPATH`), the German
section passes. The Danish one showed its seven abbreviated weekday
rows counting three bytes, which "søn" and "lør" are not in UTF-8.
They are locale-decided now, as the German rows were. The row is
2010/0 everywhere, the 24 warnings unchanged.

`22.locale.money.get`: two cases from the 2005 import of the test,
whose comments describe the intended behaviour, against facet rules
unchanged since the same import. Where `money_base::none` or
`money_base::space` appears in the pattern, the facet read all the
whitespace it found. If the currency symbol required after it, or the
rest of a multicharacter sign, began with whitespace, nothing was
left for it. A symbol of " " with `showbase` failed at the end of
"103 ". A negative sign of "- " failed at the end of "-109  ".

The standard makes the symbol and the rest of the sign required after
the other components, and the whitespace optional. A reader of input
iterators cannot tell the optional space from the symbol's until the
run ends. The facet now credits whitespace read beyond the one
character `space` requires to what follows: to whitespace the symbol
begins with, before any character of it is read, and to whitespace
the rest of either sign begins with. A sign character, a value or a
symbol character read from the input cancels the credit. A credited
character satisfies a whitespace character of the symbol or sign by
class, not identity. No real locale's symbol or sign begins with
whitespace, so outside the two cases the credit is never spent. The
row is 3888/0 everywhere. The 8 extra assertions are the string
overload's value check, reached for the two cases now.

Found on the way. The driver's `rw_ldblcmp` returned a sign, so the
`long double` value assertions could not fail; it is repaired in
chapter 3.10. `money_get` kept the digits of the value in a 304-byte
stack array and the sizes of their groups in a second one. Nothing
checked either bound. Three inputs overran them: a long value, a long
run of thousands separators, and a large `frac_digits`. Each
separator records a group, empty or not, and the zeros that pad the
value to `frac_digits` are written after the loop that reads it. A
20000-digit value died of SIGSEGV.

Fix, applied. The arrays move to one heap block when either runs
short, as in `num_get`. The block leaves room for the padding. Four
rows pin the three inputs: a value of 10000 characters, 4000
separators, and `frac_digits` of 305 and 4000. Against the old facet
three of them abort with stack smashing detected. The 305 row
overruns by four bytes, which only AddressSanitizer sees. The row is
3952/0 everywhere.

The size of the block could wrap. It is `(bufsize + npad) * 4` bytes,
where `npad` is `frac_digits`. With a 32-bit `size_t`, a
`frac_digits` of 2^30 wraps the size to 1216 bytes, and the padding
overruns the block. The facet now throws `bad_alloc` where the size
would wrap. A user `moneypunct` returning 2^30 pins it: against the
old facet the row dies of SIGSEGV. With a 64-bit `size_t` nothing
wraps and the case is skipped with a note. The row is 3960/0 in the
i386 configurations and 3952/0 in the x86_64 ones. The harness runs
the tests in compatibility mode, which counts warnings but not notes,
so the 8 skipped cases add no warnings.

`22.locale.moneypunct`, measured 2026-09-17. The row, previously 316
passing assertions, aborted in all six configurations with stack
smashing detected. The abort was at the return from
`Test<char>::check_moneypunct`, called for the native empty locale
name. The helper copied `setlocale`'s result into a 256-byte stack
array with `strcpy`. A 276-byte composite name plus its terminator
overwrote the array and the stack canary.

Fix, applied. The test uses a stack buffer with a `malloc` fallback
and exercises mixed-category names. The tables record 540 passing
assertions in all six configurations; with a C environment, 500.
`analysis-moneypunct-locale-name.md` has the debugger evidence and
the AddressSanitizer negative control.

### 3.10 Numerics

Fact, first tables. `18.numeric.special.float` failed `has_denorm`
for all three types in every configuration; the 32-bit GCC build
failed three `has_signaling_NaN` checks too. `26.c.math` failed the
`modf` overloads for `float` and `long double` in 64-bit.
`26.valarray.transcend` failed 3 everywhere. `17.extensions` failed
5 of 16, on the five `_RWSTD_NO_EXT_*_PRIMARY` macros.

Fix, applied 2026-09-18. Four causes, none in the library's numerics.

- **`26.c.math`.** The test's `check_bits` counts a `size_t` down past
  zero and compares the exhausted counter with `unsigned (-1)`. The
  two are equal only where the types have the same width. On LP64
  every call answered no, so the `modf` overloads failed for writing
  past their argument, which they never did. The sentinel is
  `std::size_t (-1)` now. The helper dates from 2005, before the
  64-bit builds.
- **`18.numeric.special.float`.** The expected `has_denorm` was a
  pinned answer, not a measurement: `denorm_present` for three
  retired platforms, `denorm_indeterminate` for all others. The
  library's answer comes from `INFINITY.cpp` reading the value of the
  smallest denormal. The test now derives its expectation the way it
  derives its other values: half the smallest normal, stored through
  a volatile, is a denormal where they exist and zero where they are
  absent or flushed, under the same `_RWSTD_NO_DBL_TRAPS` guard.

  The three 32-bit GCC failures were `has_signaling_NaN`, asserted
  true under `is_iec559` by the test and false by the library, whose
  `NO_SIGNALING_NAN.cpp` found none there. Under GCC's x87 code
  generation a signaling NaN copied through the floating-point unit
  comes back quiet: `0x7ff0` in the top bytes becomes `0x7ff8`. SSE
  code generation preserves it; that is Clang's default for i386 and
  everyone's for x86-64. The type has the representation; the
  compiler cannot carry it. The library's answer describes what its
  `signaling_NaN()` returns. The test checks `has_signaling_NaN`
  against the characterization under `is_iec559`, as it already did
  outside it. The 32-bit GCC row runs 119 assertions with the
  signaling NaN section skipped; the others run 134. `VERIFY_CONST`
  printed the trait's value as the expected and the literal as the
  observed; it prints the literal and the member now.
- **`26.valarray.transcend`.** `acos` and `asin` over 0 to 9 are NaN
  from 2 on, and the test compares element by element with
  `rw_equal`. For float and double the driver compares
  representations, so two NaNs of the same bits are equal. For long
  double the sign-only `rw_ldblcmp` called any NaN unequal.

  `rw_ldblcmp` compares representations now. The exponent and
  significand are read as one ordinal in two 64-bit parts, so the
  result is the number of representable values between the
  arguments, as for float and double. That covers the x87 extended
  format (the explicit integer bit dropped, so the significands of
  consecutive exponents adjoin) and the 128-bit interchange format.
  Other formats keep the relative-error path and its FIXME. Checked
  against `nextafterl` through all four driver libraries, and, with
  the same code retargeted at `__float128` through libquadmath, for
  the 128-bit format.

  Two callers had been written against the tolerant version.
  `26.c.math` compares `pow (long double, int)`, the library's exact
  power and one rounded division, with `powl`, which glibc does not
  round correctly on x87: one unit apart for 24 of 400 pairs, now the
  stated tolerance. `22.locale.money.get` expected `-109.1` for the
  input `-109` at zero fraction digits, a 2005 typo beside its
  `-109.0` sibling. Its two checks were one-sided (`2 > dist`,
  `1 < dist`). The row reads `-109.0`, and the checks accept one unit
  either way. With every expected value shifted two units up, then
  down, 1704 of the row's 3888 assertions go red each way.
  `22.locale.num.get` spelled a row's expected value as the tested
  type cast from a double literal, so its long double rows expected a
  double's precision where the facet reads with `strtold`: 240 rows
  within the tolerance and not within one unit. The literal carries
  the `L` suffix now. The float and double rows are unmoved.
- **`17.extensions`.** The five failing assertions demanded
  `_RWSTD_NO_EXT_MONEYPUNCT_PRIMARY` and its four siblings under
  strict ANSI. `rw/_config.h` has carried the five commented out as
  "not implemented yet" since 2005. The strict ANSI entry in `TODO`
  keeps the primary templates and retires the macros. The test no
  longer asks for gates the library never grew. The bogus primary
  templates it defined behind the same macros were dead code. 11
  assertions remain.

The four rows are at 100% in the six configurations.

### 3.11 The iostreams rows

Fact. Four rows fail in every configuration, GCC and Clang alike,
and have since the first tables. They have no investigation and no
`TODO` entry. What they report, from a run in 11S on 2026-09-28:

- **`27.basic.ios`, 1 assertion.** "basic_ios<char>::flags () ==
  dec | skipws, got dec | skipws". Expected and observed print the
  same, so they differ in a bit the message does not name.
- **`27.filebuf`, 8 assertions.** Four checks of `detach()`, for
  `char` and `wchar_t`: `is_open()` true after a detach, `fd()` 3
  where a negative value is expected, and the destructor closing the
  detached descriptor.
- **`27.std.manip`, 8 assertions.** All eight name `std::setbase (10)`.
- **`27.ostream.inserters.stdcxx-51`, `ABRT`.** The test expects
  `qnan` and `snan`, and `QNAN` and `SNAN` under `uppercase`, for
  quiet and signaling NaNs of all three floating types. The library
  prints `nan` and `NAN` for both. The test's `assert` at line 123
  fails.

These are the 17 failed assertions of the GCC tables and their one
signalled program.

## 4. Differences between configurations

| row | 11S | 11s | 15D |
|---|---|---|---|
| `18.numeric.special.float` | 134 | 119 | 134 |
| `20.temp.buffer` | 10891 | 10792 | 10891 |
| `22.locale.num.put` | 1671 | 1667 | 1671 |
| `22.locale.codecvt` | 322 | 322 | 306 |
| `atomic_add`, `atomic_xchg` | 0 | 0 | 66, 22 |
| `21.string.stdcxx-162` | `NOUT` | `NOUT` | `ABRT` |
| fourteen `22.locale.*.mt` | pass | pass | chapter 3.4 |

Reasons, in order:

- 32-bit GCC skips the signaling NaN section (chapter 3.10).
- `20.temp.buffer` and `22.locale.num.put` run a few more cases in
  64-bit. Both pass everywhere.
- 15D skips the libc-backed facets on a corrupt state (chapter 3.3).
- The atomic operation tests count only in the thread-safe build.
- `21.string.stdcxx-162` requires a string body of at most 24 bytes.
  In 15D every string body carries its own mutex, 64 bytes: the
  layout of every 4.2.x release on Linux/x86-64. Atomic reference
  counts would satisfy the test and break that binary interface.
  They wait for the next minor version (`TODO`).
- The 15D rows of the older locale MT tests measure the harness
  (chapter 3.4).

The single-threaded builds run each MT test with one thread. Most
count nothing there. The newer ones count in every build:
`22.locale.use_facet.mt` and `22.locale.time.put.libc.mt` 200 each,
`22.locale.id.mt` 999, `22.locale.facet.mt` 40,
`21.string.cons.mt` 16, `21.string.push_back.mt` 2. These pass in
15D too. `22.locale.money.put.mt` counts 6 in the single-threaded
builds and is one of the fourteen in 15D.

## 5. The same suite under Clang

`x86_64-11S-clang.txt`, `i386-11s-clang.txt` and
`x86_64-15D-clang.txt` are the same three configurations built with
Clang (`CONFIG=gcc.config CXX=clang` on the `config` line), run and
pinned the same way. `analysis-clang.md` records what it took.

| configuration | assertions | failed | non-zero exits | signalled |
|---|---|---|---|---|
| 11S-clang | 10,105,653 | 17 | 0 | 1 |
| 11s-clang | 10,105,550 | 17 | 0 | 1 |
| 15D-clang | 10,105,719 | 17 | 10 | 6 |

Row for row they are the GCC tables, with these exceptions:

- **`18.numeric.special.float`** runs 134 assertions in the 32-bit
  Clang build, where GCC's runs 119. Clang generates SSE code for
  i386, which carries a signaling NaN (chapter 3.10).
- **The 15D MT rows** differ in their exit statuses, as they differ
  from run to run under GCC. They measure the harness under either
  compiler (chapter 3.4).

`0.char` and `0.printf` passed under Clang before the alignment
repair of chapter 3.6; the misaligned-address failures were GCC's
alone. The rows are the same under both compilers now.
`23.bitset.cons` varied under Clang as under GCC until chapter 3.5;
it passes under both.
