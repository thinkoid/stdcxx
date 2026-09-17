# The test-suite baseline

The whole suite, run under its own harness on the current toolchain,
recorded once as the reference for everything after it: which tests
fail, by how much, and why where that is known. The numbers are
pinned under `doc/notes/baseline/`, one file per configuration, in
the harness's own table minus the timing columns.

## 0. tl;dr

- Every test builds. The harness runs 268 programs per configuration
  in about a minute for the single-threaded builds and about ten for
  the thread-safe one.
- The failures cluster into a dozen causes. Two were defects of the
  harness and are fixed here: the driver derived the wrong source
  directory when `TOPDIR` was unset, and one test's name collided
  with the harness's output file so that it had never run. The rest
  are queue entries in `TODO`, each with a hypothesis and the check
  that settles it.
- Two clusters were fixtures that never existed in this repository:
  the input files two locale tests read under `tests/etc` stayed
  behind in the 2008 migration from the vendor's repository. They
  are regenerated (`test-fixtures.md`), and what they found is in
  chapter 3.2.
- The thread-safety results are not a baseline yet, and that is the
  finding: in the thread-safe configuration the locale MT tests fail
  differently on every run of the same binary, an assertion in a
  thread, an uncaught exception, a thread pool that cannot start, or
  a pass. The `std::locale` race the revival set out to find shows
  itself on x86-64, without waiting for weakly ordered hardware.
- The former 64-bit archive-only bitset and reverse failures are
  repaired (chapter 3.5), as are the driver's self-tests (chapter
  3.6) and the moneypunct test's own stack overwrite (chapter 3.9).

Toolchain: GCC 16.2.1, glibc 2.44, x86-64 Linux. Tables refreshed
2026-09-17, including the three corresponding Clang configurations.

## 1. Running the suite

From a build directory's `tests` subdirectory, `make run` runs every
built test through `bin/exec`, the harness, with the timeout the
build's `makefile.in` sets (`RUNFLAGS = -t 30`), and prints one row
per program followed by a summary. The rule exports `TOPDIR`,
`TMPDIR`, `TZ` and `LD_LIBRARY_PATH`; a test run by hand needs
`TOPDIR` set to the source tree and the build's `tests` directory as
the working directory, because the locale tests exec `../bin/localedef`.
The timeout was 300 seconds from 2007 until the collate change,
whose thread-safe tables were the first run at 30: the
MT locale tests that hang (chapter 3.4) ran the 300-second limit out
one after another, and the slowest honest test, `22.locale.codecvt`,
takes 24 seconds in the 32-bit debug build. The timeout is the only
option to set by hand; `RUNFLAGS` on the `make` command line replaces
the whole variable and drops the `--compat` and `--ulimit` options
the rules append to it, and the tables come out different.

The status column distinguishes process failure from output classification.
A nonzero exit or signal is reported before output parsing: `ABRT`,
`SEGV` and the like name signals, and `HUP` denotes a timeout. After
an unsignalled zero exit, the harness looks for the driver's summary;
`NOUT` means no output and `FORMAT` means no recognized summary.
`EXEC` denotes a file that could not be executed, and `COMP` a test
that did not build. The runner's own zero exit means it completed,
not that all its children passed; read the rows and program summary.

Some successful tests deliberately produce no driver summary. The 82
regression tests under `tests/regress` are plain programs that assert
and exit; they print nothing, so successful runs are reported as
`NOUT` (verified for a sample by hand). Several driver self-tests
also use independent reporting: chapter 3.6.2 records their contracts
and why their successful rows are `FORMAT` or `NOUT`. These are
specific known conventions, not a rule that every such row passes.
A test using driver assertions still needs its assertion summary;
exit zero alone does not establish success.

The harness writes each program's output next to it as
`<program>.out`. That naming is what collided with the test
`22.locale.codecvt.out`, whose name follows the suite's
`clause.facet.member` convention: the harness built it, overwrote it
with the output of `22.locale.codecvt`, then failed to execute the
text file. The test is renamed `22.locale.codecvt.do_out` in this
change and runs for the first time, 1494 assertions, all passing.

## 2. The reference

Three configurations, named as the build system names them: the
number is the debugging, optimization and thread-safety level, `s`
an archive and `d` a shared library, lowercase 32-bit and uppercase
64-bit code.

| configuration | file | programs | assertions | failed | non-zero exits | signalled |
|---|---|---|---|---|---|---|
| 11S, debug, archive, 64-bit | `baseline/x86_64-11S.txt` | 268 | 10,069,481 | 734 | 0 | 5 |
| 11s, debug, archive, 32-bit | `baseline/i386-11s.txt` | 268 | 10,069,363 | 734 | 0 | 5 |
| 15D, debug, shared, threads, 64-bit | `baseline/x86_64-15D.txt` | 268 | 10,069,547 | 734 | 6 | 14 |

The 15D counts include the MT locale tests, which are not stable
(chapter 3.4), and that is the whole of the difference between the
15D row and the other two. Until chapter 3.5 was done the 64-bit
counts also included `23.bitset.cons`, which varied from run to run;
the tables were pinned first with the two locale tests failing to
read their input files, re-pinned with the files in place (chapter
3.2), again with the narrow transform repaired, and again with the
bitset count settled; each time the assertion totals moved by those
rows and by the unstable ones. The 2026-09-17 refreshes incorporate
the four driver self-test repairs (chapter 3.6), a clean
`21.string.iterators` run, and the moneypunct repair in chapter 3.9;
the last of the day pins the alignment and moneypunct repairs the
morning tables still lacked.

The last measurement on a current toolchain before this one, made
by hand in 2026-09 before every test linked, was 10,066,804
assertions with 1,417 failures. The 2008 nightly results were never
clean either; the suite was in the seventies percent across the
platforms of the day. A failure in the tables is a fact to record,
not a regression to chase, until its entry says otherwise.

## 3. Findings

Each finding states the fact, the hypothesis where there is one, and
the fix, applied or intended. The applied ones are in this change;
the intended ones are `TODO` entries.

### 3.1 The harness: `TOPDIR` and the output file name

Fact. `tests/src/locale.cpp` derives `TOPDIR` from `__FILE__` when
the variable is unset, and did it by truncating at the character
before the last slash, which turns `.../tests/src/locale.cpp` into
`.../tests/sr`. Every locale test run by hand then failed to open
`tests/sr/etc/nls/...`. The fallback was wrong from the commit that
introduced it in 2008; `make run` exports the variable, so the
nightly runs never reached it.

Fix, applied. The fallback strips the file name and the two
directories above it. Verified: a bare run of `22.locale.codecvt`
without `TOPDIR` from the build's `tests` directory finds the
locale sources and runs `localedef` with the right paths.

Fact. `22.locale.codecvt.out` collided with the harness's output
naming (chapter 1). Fix, applied: renamed to
`22.locale.codecvt.do_out`.

Fact. `22.locale.codecvt` read `TOPDIR` itself as well, reported the
variable missing, and then used the null pointer: a segmentation
fault in a bare run without the variable. Fix, applied: the driver's
fallback is one function, `rw_topdir ()`, and the test takes the
directory from it (`codecvt-invalid-state.md`, chapter 6).

### 3.2 The input files under `tests/etc`

Fact. `22.locale.codecvt` reads `tests/etc/codecvt.<locale>.in` for
five locales and `22.locale.collate` reads
`tests/etc/collate.<locale>.in` for six. No branch of the repository
has ever had `tests/etc`. Both tests were migrated from the vendor's
Perforce repository in May 2008, "warts and all" in the words of the
commit, and their fixtures did not come with them. The reads are 20
of the 21 codecvt failures and both collate failures, on every
configuration.

Fix, applied. The files are regenerated from what the tests expect
of them, the codecvt table fixing each file's byte and character
count and the collate table its encoding; `test-fixtures.md` records
their construction and the order oracle for the collate lists. With
the files in place the codecvt test exposed three defects of its
own, repaired with them: its `length` check compared against a C
locale it had not set, its `length` expectation counted characters
where the standard counts bytes, and a synthetic charmap named after
the native codeset made `localedef` reuse a single-byte stub for the
`ja_JP.UTF-8` locale. One codecvt assertion remained before the abort
of chapter 3.3, predating the files; it is repaired with the abort.
The collate test exposed a
library defect: the narrow transform could not spell a weight that is
a multiple of 127, and the letter "l" in Croatian and ko kai in Thai
sorted last in the `char` half of the test, 101 lines, while the
`wchar_t` half sorted all six lists as glibc does. Repaired since:
the spelling is prefix-free and sorts below the IGNORE mark of the
position orderings, which had been the same byte as its run byte
(`collate-transform.md`); the test builds the mark's case, and its
own `exit (1)` when `TOPDIR` was unset went with it.

### 3.3 `22.locale.codecvt` on a corrupt `mbstate_t`

Fact. The test's last section passes a corrupt `mbstate_t` to the
facets, expects the library's assertion in a debug build and catches
its `abort()` with a `SIGABRT` handler that jumps back; in an
optimized build it expects the error result. glibc 2.41 changed
`abort()` so that the jump left `SIGABRT` blocked and the second
assertion killed the process, hence `ABRT` in every debug
configuration. The `8S` run showed the optimized half of the
contract met on half the library's paths, and the libc-backed wide
facet looping inside glibc's `mbrtowc` on the corrupt state.

Fix, applied. The handler saves and restores the signal mask; every
facet path returns the error result when its state check fails, the
assertion staying in debug builds; the thread-safe debug build skips
the two libc-backed facets, whose check sits under the locale lock.
`codecvt-invalid-state.md` has the whole of it.

### 3.4 The MT locale tests

Fact. In 15D the locale MT tests, `22.locale.codecvt.mt` through
`22.locale.time.put.mt`, end differently on every run of the same
binary. Under `make run`, two runs of the suite gave two sets of
exit statuses. Three bare runs of `22.locale.numpunct.mt` from the
build's `tests` directory: the first died on the test's own
assertion in a thread, `0 == rw_strncmp (tn.c_str (),
data.truename_.c_str ())` at line 147, a thread reading another
locale's `truename`; the second died of an uncaught
`std::runtime_error` thrown in a thread, a locale that failed to
construct; the third passed. `22.locale.cons.mt` reports its thread
pool failing to start within a millisecond. Each test runs 32
threads over the locales it generates with `localedef`; a copy run
where `localedef` is not reachable passes with 4 assertions, which
is how "passes when run alone" first misled this note.

Hypothesis. This is the `std::locale` MT-safety defect the revival
set out to find, `22.locale.numpunct.mt` being the test it was
first seen in, and it reproduces on x86-64 in a debug shared build:
facet data read under no lock, or a facet id race, with the exact
symptom depending on the interleaving. The thread pool failure is
the same defect seen from the pool, or a second one; the errno of
the failing `pthread_create` says which.

Intended. The entry for it is the MT entry in `TODO`. The
instruments are a ThreadSanitizer build of the library and the MT
tests, then the aarch64 box for the ordering hypothesis. Until the
cause is found the 15D rows for `22.locale.*.mt` record symptoms,
not a reference.

### 3.5 `23.bitset.cons` and `25.reverse` in 11S

Fact. In 11S, `23.bitset.cons` failed 240, 440 and 680 assertions in
three consecutive runs of one binary, and 0 in others, with the same
walk in 15D and under Clang; `25.reverse` died of a segmentation
fault in 11S only. Both passed in 11s.

Cause, two unrelated test defects, neither in the library. The
bitset test passes the exception it expects a constructor to throw
as a small integer in the `const char*` parameter that otherwise
carries the expected bit string, and told the two apart with `int
(bitstr - (char*)0) < 3`: the pointer truncated to `int`. On a
64-bit target a heap address with bit 31 set is negative after the
truncation, so the test expected a throw from a valid call, and
which strings sat above that line depended on where the address
space randomization had put the heap. With randomization off the
binary passed every time; under valgrind, likewise, and with no
error reported, since nothing was uninitialized. The reverse test
formatted "the mismatched element" as an assertion argument with
`xsrc [nsrc - i - 1]`, evaluated whether the assertion fails or
not; on success the loop leaves `i == nsrc` and the index is -1,
one element before the array, which in the archive build was a
buffer the driver had just freed (270 invalid reads under valgrind)
or, at the start of a mapping, the fault. Both idioms date from the
tests' first commits, 2006 and 2008.

Fix, applied. The comparison at `ptrdiff_t` width; the mismatched
element taken only when there is one. Check: eight runs of the
bitset test with randomization on, 2347 of 2347 each; the reverse
test under valgrind with no error and a clean exit, in 11S.

The run that pinned the tables after this fix showed a new
intermittent: `21.string.iterators` failed 6 of 5725 in 11S, all in
the `UserChar` instantiation, `begin () const` and `end () - 1`
returning a null element where the value was expected. The row had
been clean in every earlier table and passed 22 further runs, bare,
through the harness and under its address-space limit, with
randomization on and off. The September 16 table kept the 6 as
measured; the September 17 rerun passed. The `TODO` entry remains
open because a clean run does not explain the intermittent failure.

### 3.6 The driver's self-tests

Fact. `0.inputiter`, `0.outputiter` and `0.new` aborted without a
message. They deliberately trigger assertions to test the driver's
iterators and replacement allocation functions, catch `SIGABRT`,
and jump back with `longjmp`. The iterator tests close `stderr` and
the allocation test disables its expected error diagnostics, which
explains the silence.

Cause. The signal is blocked while its handler runs. `setjmp` does
not save the signal mask on glibc, and `longjmp` bypasses the normal
handler return, leaving `SIGABRT` blocked. Reinstalling the handler
does not unblock it. Before glibc 2.41, `abort()` unblocked the
signal before raising it, repairing the mask for the next assertion.
The change described in `codecvt-invalid-state.md`, chapter 2,
removed that accommodation; the second assertion now terminates
the process instead of reaching the handler.

Fix, applied. All three tests use `sigjmp_buf`, save the mask with
`sigsetjmp (env, 1)` in both the expected-success and expected-failure
paths, and restore it with `siglongjmp`. The assertions and their
expected outcomes stay the same.

Check, 2026-09-17. Rebuilt and ran all three directly and through
`make run RUN='0.inputiter 0.outputiter 0.new'` in `11S`, `11s` and
`15D`, with both GCC and Clang. The three rows were `ABRT` before
the repair; afterwards every configuration gives:

| test | exit | assertions | failed |
|---|---|---|---|
| `0.inputiter` | 0 | 20 | 0 |
| `0.outputiter` | 0 | 0 | 0 |
| `0.new` | 0 | 26 | 0 |

The output-iterator test counts diagnostics only when a case fails,
so zero assertions is its successful result. Isolated copies of
each repaired test were also made to miss an expected assertion and,
separately, to raise an unexpected one, under GCC and Clang in `11S`
and `15D`. Every copy finished with exactly one failed assertion and
the corresponding diagnostic. The iterator tests returned 1;
`0.new` returned 0, as its callback always does, so its summary is
the failure oracle. The original sources still abort when rebuilt
as controls. The full suite was not rerun for that self-test-only repair. The
subsequent fnmatch run below incorporates these results into all six
pinned tables.

The pinned tables still contain the GCC alignment failures: four in
`0.printf` and one in `0.char`. Commit `57c34d96` repairs their empty
wide-string inputs with named arrays to preserve alignment despite
GCC PR127457. Targeted runs pass all 1876 and 479 assertions in the
six configurations, and the full-suite tables refreshed later that
day carry the rows: `0.char` 479 of 479, and `0.printf` `FORMAT`
with exit 0, its own summary reporting all 1876 passed (chapter
3.6.2). The standalone reporting question is resolved in
chapter 3.6.2.

#### 3.6.1 Replace the custom pattern matcher

`rw_fnmatch` is part of the test-support library, not the standard
library. Its only caller outside its self-test is `rw_locale_query`,
which filters canonical locale names. The helper was introduced on
trunk in December 2007 (STDCXX-683, `c6496b81`) alongside its header
and self-test, then merged into 4.2.x in April 2008 (`222c045b`).
A portable replacement for POSIX `fnmatch` served the former platform
matrix. The supported targets are now Linux with GCC and Clang;
libc supplies the function, and the self-test already depended on it.
That portability rationale no longer warrants a second implementation.

The deleted parser had several independent defects:

- An unterminated bracket expression could return the pattern's NUL
  as if it were a closing bracket. The caller advanced past it and
  read beyond the allocation. Matching `[a` against `a` produced two
  invalid reads under Valgrind and could falsely report a match.
- Unmatched `[` did not fall back to a literal. The self-test itself
  incorrectly expected failure for `[` against `[` and `[a` against
  `[a`, contradicting its native oracle.
- Leading literal `-` and `]` prevented later members from matching;
  negated lists mishandled them too. A trailing `-` was treated as a
  range operator instead of a literal.
- Escape state never reset within brackets. Closing-bracket scans
  checked only the previous byte rather than the parity of consecutive
  backslashes, corrupting ranges and recognition of the closing `]`.
- Named character classes, equivalence classes, collating symbols and
  multibyte matching were absent despite the advertised POSIX contract.

The helper now delegates to POSIX `fnmatch` with flags zero, preserving
its assertions, ignored third argument and normalized 0/1 result.
Native matching uses the current locale rather than matching bytes;
this also supplies the missing character-class and multibyte semantics.
No additional library dependency or compiler option is required.

The self-test uses explicit expected results instead of comparing two
calls to the same native function. Both incorrect expectations are
corrected, and 14 checks cover the bracket defects and an exact-size
heap allocation for memory checking, bringing the total to 337.

The investigation's separate scratch comparison used 1,911,679 pairs
of short ASCII patterns and strings. It found 14,476 disagreements,
all involving brackets. This generated corpus is not part of the
suite; it includes malformed inputs and is not a count of distinct
POSIX violations. Concrete regression cases establish the defects.

Check, 2026-09-17. The actual self-test passes all 337 cases directly
and through the harness in `11S`, `11s` and `15D` under GCC and Clang.
Its successful harness row is `NOUT`: it prints only failures and
returns zero. Valgrind reports zero errors. As a negative control,
linking the same self-test against the deleted parser produces 14
failed cases and two invalid reads, including the exact allocation.

Rebuilt the test-support library and all tests, then ran `make run`
with the existing harness options in all six configurations: 268
programs each, 1,608 executions. All six tables are re-pinned from
those runs. Besides the fnmatch and previous signal-recovery repairs,
the differences are the now-passing intermittent string-iterator row,
the moneypunct abort discussed in chapter 3.9, and the already unstable
MT locale rows. The suite still has the outstanding failures recorded
here; successful `make run` completion is not an all-tests-pass claim.

#### 3.6.2 Standalone self-test reporting

Decision, 2026-09-17: retain the independent reporting. These programs
exercise the test driver's support functions; they do not themselves
exercise the external runner's output parser. None calls `rw_test`
or reports its checks through `rw_assert`.

| test | successful output | failure contract | successful runner row |
|---|---|---|---|
| `0.cmdopts` | diagnostics from deliberately rejected arguments | unexpected parser or callback results set exit status 1 | `FORMAT` |
| `0.strncmp` | overload banners | a mismatch prints a diagnostic and sets exit status 2 | `FORMAT` |
| `0.valcmp` | specialization banners | a mismatch prints a diagnostic and sets exit status 1 | `FORMAT` |
| `0.braceexp` | none | a mismatch increments `nerrors`; main returns 1 if any occurred | `NOUT` |
| `0.printf` | progress and its own assertion summary | failed checks print a summary and return 1 | `FORMAT` |

The original comparison tests (`5b7a4c57`, December 2005) and option
test (`c41878f4`, December 2005) already use independent exit status.
The original brace-expansion test (`7749e93f`, February 2008, merged
in `222c045b`) explicitly says "return 0 on success, 1 on failure"
and prints only on failure. The banners and silent success are
established conventions, not evidence of missing driver integration.

Independence also fits the architecture. `0.cmdopts` clears and
replaces the option registry that the normal driver uses, while
`0.printf` tests the formatter used by normal driver diagnostics.
Their libc reporting keeps the observation separate from those
mechanisms. This rationale is inferred from the code, not an explicit
historical requirement that every self-test avoid the driver; other
self-tests use it.

The runner preserves these failure contracts. `run_target` in
`util/runall.cpp` calls `parse_output` only after a zero, unsignalled
exit. `check_test` and `check_compat_test` in `util/output.cpp` then
classify empty output as `ST_NO_OUTPUT`, or a missing summary as
`ST_FORMAT`. The deliberate empty-output distinction was added in
`21e213d9` (June 2007), with the comment "regression test?". It
recognizes that silence can be a program's normal success behavior.

Check, 2026-09-17. All five existing GCC `11S` binaries ran directly
with exit zero. `0.cmdopts` printed only expected argument-rejection
diagnostics to stderr; the comparison tests printed banners;
`0.braceexp` was silent. Copies run through `bin/exec --compat -t 30`
produced the classifications above. Two synthetic executable controls,
one silent with exit 7 and one printing a diagnostic with exit 9,
retained numeric statuses 7 and 9; the program summary counted two
nonzero exits. The runner itself returned zero on completion.

These controls verify classification precedence, not the sensitivity
of every self-test check. Failure branches were inspected, not
mutated. No rebuild or full-suite run was needed for this reporting
decision, and the pinned tables remain measurements of their earlier
runs. No test or runner behavior is changed.

Investigation and resolution: OpenAI LeChuck.

### 3.7 Strings: ranges into the string itself

Fact. `21.string.assign`, `21.string.insert` and `21.string.replace`
fail 60, 150 and 240 assertions, every one with a source range
inside the string being modified: `string ("abc").insert (begin (),
begin () + 1, begin () + 2)` expected "babc", got "aabc". The
regression tests `stdcxx-629`, `stdcxx-632` and `stdcxx-170` abort
on the same cases.

Cause, read 2026-09-17. Every range overload of `assign`, `insert`
and `append` is `replace` over the whole string, over an empty range
at the point, or over an empty range at the end, and a bidirectional
or random-access source goes to `__replace_aux` in
`include/string.cc`, whose in-place branch moves the tail and then
copies the source element by element with no overlap test. The
new-representation branch reads the old buffer before unlinking it,
so forced growth is safe, and the input-iterator branch stages the
source in a temporary for exception safety, so it is safe by
accident. The failing kinds follow from where the write lands
relative to the read: `assign` fails only through reverse iterators,
`append` never, `insert` and `replace` at the front for every kind.
`insert` guards `const_pointer` with an address test and a copy
through a temporary string, but a `char*` argument is an exact match
for the member template and skips the guard; `replace` has no guard.
The tree tried twice in 2008 (bf077568 and 966011ea, both reverted):
each took the address of `*first` inside the template, which the
list discussion of the time showed is ill-formed for iterators that
return by value. Neither trunk nor 4.3.x fixed it afterwards, and
STDCXX-170 is open upstream.

Intended. Copy first on the generic path, as 21.3.5.6 specifies;
non-template overloads for the pointer and string-iterator types
that route to the count overload of `replace`, which already handles
an overlapping source. The `TODO` entry has the gate.

`21.string.stdcxx-162` was listed here. It is a probe of the string
atomics decision, not of this path, and belongs to the MT entry
(chapter 3.4).

### 3.8 `21.cwchar` aborts after passing

Fact. All 65 assertions pass and the process dies at exit with
"double free or corruption (top)". Check: a memory checker run.

### 3.9 Locale facets

Fact. `22.locale.num.get` fails 204 assertions, the `wchar_t`
overloads reading `unsigned long` consuming a different count than
expected. `22.locale.time.get` fails 33: `get_date ("01/01/2000")`
consumes 10 characters where 8 are expected, and `get_weekday
("torsdag")` is not recognized. `22.locale.money.get` fails 16,
`long double` reads with `showbase` ending in `failbit`.

New measurement, 2026-09-17. `22.locale.moneypunct`, previously 316
passing assertions, now aborts in all six configurations with stack
smashing detected. A backtrace places the abort at the return from
`Test<char>::check_moneypunct`, called for the native empty locale
name. That helper copies `setlocale`'s result into a 256-byte stack
array with `strcpy`; whether a long composite locale name overwrites
it is the next check. The identical test object linked against the
deleted fnmatch parser also aborts, as does the POSIX version. This
is a separately queued defect, not a regression attributed to the
matcher replacement. Valgrind reports no memory errors in either
control, which does not exclude an overwrite within a stack frame.

Repair, 2026-09-17. The suspected copy is confirmed: a 276-byte
composite name plus its terminator overwrites the 256-byte array and
the stack canary. The test now uses a stack buffer with a `malloc` fallback and
explicitly exercises mixed-category names. Targeted direct and harness
runs pass 540 assertions in all six configurations; with a C environment,
500 pass. `moneypunct-locale-name.md` records the debugger evidence and
the AddressSanitizer negative control. The full-suite tables refreshed later
that day record 540 passing assertions in all six configurations.

Intended. Per facet, reading each test against the library.

### 3.10 Numerics

Fact. `18.numeric.special.float` reports `has_denorm == 1` where the
test expects `-1` (`denorm_indeterminate`) for `float` and `long
double`, 3 assertions in 64-bit and 6 in 32-bit. `26.c.math` fails
the `modf` overloads for `float` and `long double` in 64-bit only.
`26.valarray.transcend` fails 3 in 32-bit only. `17.extensions`
fails 5 of 16, on `_RWSTD_NO_EXT_FAILURE` and its kin.

Hypothesis. Characterization questions: whether the config test or
the test's expectation is the one out of date.

## 4. Differences between configurations

| test | 11s | 11S | 15D |
|---|---|---|---|
| `18.numeric.special.float` | 6 failed | 3 failed | 3 failed |
| `26.c.math` | passes | 3 failed | 3 failed |
| `26.valarray.transcend` | 3 failed | passes | passes |
| `23.bitset.cons` | passes | varies | passes |
| `25.reverse` | passes | `SEGV` | passes |
| `21.string.stdcxx-162` | passes | passes | `ABRT` |
| `22.locale.*.mt` | single-threaded | single-threaded | chapter 3.4 |
| `20.temp.buffer`, `22.locale.num.put` | pass | pass | pass, with a few more assertions in 64-bit |

The single-threaded builds run the MT tests with one thread, which
is why they pass there and count nothing.

## 5. The same suite under Clang

The tables `x86_64-11S-clang.txt`, `i386-11s-clang.txt` and
`x86_64-15D-clang.txt` are the same three configurations built with
Clang (`CONFIG=gcc.config CXX=clang` on the `config` line), run the
same way and pinned in the same shape; `clang.md` records what it
took to get there. Row for row they are the GCC tables, with these
exceptions:

- `0.char` and `0.printf` passed every assertion under Clang before
  the alignment repair in chapter 3.6; their "misaligned address"
  failures were GCC's alone. Since the repair the rows are the same
  under both compilers: `0.printf` reports in its own format, so the
  harness says `FORMAT` with exit 0, as it does for the self-tests
  in chapter 3.6.2.
- `23.bitset.cons` (chapter 3.5) passed all 2347 assertions under the
  harness in the first Clang run, failed 680 in the next, and failed
  440 and 680 in bare runs of the same binary, the counts GCC shows.
  The count varies under the harness and outside it, with either
  compiler; the read depends on the process's memory, not on the
  compiler.
- `18.numeric.special.float` (chapter 3.10) fails 3 assertions in the
  Clang 32-bit build, the 64-bit count, where GCC's 32-bit build fails
  6.
- The MT rows of 15D vary under Clang as they do under GCC (chapter
  3.4) and are not a reference under either.
