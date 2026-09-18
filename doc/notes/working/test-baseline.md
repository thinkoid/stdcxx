# The test-suite baseline

The whole suite, run under its own harness on the current toolchain,
recorded once as the reference for everything after it: which tests
fail, by how much, and why where that is known. The numbers are
pinned under `doc/notes/working/baseline/`, one file per configuration, in
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
- The one library defect the string tests exposed, a range source
  inside the string being modified, is repaired (chapter 3.7): 450
  failed assertions and three aborting regressions per table gone.
- The three locale facet rows are repaired (chapter 3.9): a test
  macro misuse, a driver buffer that never reset, a date length the
  platform decides, and two facet limits, `num_get`'s 130-byte
  buffer and `money_get`'s whitespace ahead of a required symbol or
  sign tail.
- The four Numerics rows are repaired (chapter 3.10): two test
  defects from the 32-bit era, a pinned expectation made a
  measurement, a signaling NaN answer the test now takes from the
  characterization, and a driver comparison that had compared long
  doubles by sign since 2005 and now counts the values between them.

Toolchain: GCC 16.2.1, glibc 2.44, x86-64 Linux. Tables refreshed
2026-09-18, afternoon, including the three corresponding Clang
configurations.

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
| 11S, debug, archive, 64-bit | `baseline/x86_64-11S.txt` | 268 | 10,104,209 | 17 | 0 | 1 |
| 11s, debug, archive, 32-bit | `baseline/i386-11s.txt` | 268 | 10,104,091 | 17 | 0 | 1 |
| 15D, debug, shared, threads, 64-bit | `baseline/x86_64-15D.txt` | 268 | 10,104,275 | 17 | 7 | 9 |

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
morning tables still lacked, and the final one the string repair in
chapter 3.7, which also raises the assertion totals: the tests
iterate every case once per allocation the operation makes, throwing
at each, and the copying path makes one more. The 2026-09-18
refresh records the three locale facet rows and
`22.locale.ctype.tolower` (chapter 3.9); its summary lines also
catch up with the `0.char` and `21.cwchar` rows that had been
re-pinned by hand without them.
The afternoon refresh of the same day records the four Numerics rows
and `22.locale.num.get` (chapter 3.10); `17.extensions` asks five
questions fewer, which is the drop in the assertion totals.

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
reported as returning a null element where the value was expected.
The row had been clean in every earlier table and passed 22 further
runs, bare, through the harness and under its address-space limit,
with randomization on and off. The September 16 table kept the 6 as
measured; the September 17 rerun passed.

Cause, found 2026-09-18 with an address-sanitized build of the test
and of the driver's `char.cpp`, and a test defect again. The test
handed `rw_match` a single `char` on the stack as the expected value,
and the directive parser behind `rw_match` looks past every
character for a `@` repeat count, so it read the two bytes after the
local. They were the low bytes of a stale pointer sharing the stack
slot, randomized with the address space; a run in which they spelled
`@` and a digit parsed a repeat count out of the neighbour and
compared against what followed it. The "null element" was a second
artifact: `%{#c}` reads an `int` from the argument list, and the
`UserChar` result is a 32-byte struct passed in memory, so the
directive printed the first bytes of its `long double` member. The
same one-character idiom had been repaired in `21.string.access` and
`21.string.copy` in 2007 and missed in `21.string.iterators`,
`21.string.capacity` and `21.string.io`; the narrow overload parses
its second argument too, which caught `21.string.access` reading one
element past the accessed character and `21.string.io` one past the
test streambuf's exactly sized output buffer.

Fix, applied. Every buffer the string tests hand to `rw_match` is
terminated: a two-element array for a single expected or observed
character, a sentinel element after the test streambuf's output
buffer, as its input buffer already had. Check: the sanitized builds
of the three tests with stack locals report nothing; every string
test and the two istream tests that share the streambuf run under
valgrind with no error in 11S; the rows are unchanged in all six
tables. The `%{#c}` artifact is closed too: the string tests hand
the directive `char_value ()` from `rw_char.h`, the value of the
character as an `int` for each of the three character types, and
`0.char` pins the helper against the directive.

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

Repair, applied 2026-09-17. The generic path builds a temporary
from the source first, as 21.3.5.6 specifies; non-template overloads
for `pointer`, `const_pointer` and, under debug iterators, the
string's own iterator types route to the count overload of
`replace`, which already allocates a new representation when the
source lies in the buffer. No address of a dereferenced iterator is
formed in the template, so an iterator returning by value
instantiates. Check: the three tests at 100% and the three
regressions exit 0 in all six configurations; a 24-case probe that
fails 11 rows against the previous headers passes; every other row
of the six tables unchanged. The guarded `insert` overloads are now
redundant and queued for removal.

`21.string.stdcxx-162` was listed here. It is a probe of the string
atomics decision, not of this path, and belongs to the MT entry
(chapter 3.4).

### 3.8 `21.cwchar` aborts after passing

Fact. All 65 assertions passed and the process died at exit with
"double free or corruption (top)" in every configuration.

Cause, from valgrind: a test defect that glibc turns into a double
free. `test_file_functions` opened one stream on the null device for
writing and called every file function on it in the order of the
standard's synopsis, the input functions included. The last of them,
`ungetwc` after unflushed output, has no contract: a stream opened for
output takes no input, and on a stream opened for update 7.19.5.3, p6
of C99 requires a flush or a positioning call between output and
input. glibc's pushback on such a stream allocates a backup buffer and
swaps it with the get area, whose base is the stream's buffer; the
flush at exit writes the pending character and resets the get area
to the buffer without leaving backup mode, so the swap back at exit
leaves both pointers on the buffer, and the buffer is freed as the
backup area and again as the buffer. The narrow pushback has the
same shape and is benign: its second free is reached only under a
memory checker's `__libc_freeres`. The wide free at exit dates from
glibc 2.41 and the one on `fclose` from 2.44; a standalone C program
with `fopen ("w")`, `fputwc`, `ungetwc` reproduces the abort on 2.44.

Fix: the input functions get a second stream opened for reading, the
order of the calls unchanged; both streams are closed. Check: exit 0,
valgrind clean in 11S, the row at 100% in the three GCC
configurations. Under Clang the row now shows what the abort hid: the
const and non-const overloads of `wmemchr`, `wcspbrk`, `wcsrchr`,
`wcsstr` and `wcschr` fail, 5 assertions and 5 warnings, because
glibc's `<wchar.h>` declares its C++ overloads only for GCC 4.4 and
later and Clang presents itself as GCC 4.2, while `<string.h>` has a
Clang clause; the tree's characterization reports the overloads
present. That is a configuration question with its own `TODO` entry;
the Clang rows are pinned as measured.

### 3.9 Locale facets

Fact. `22.locale.num.get` fails 204 assertions, the `wchar_t`
overloads reading `unsigned long` consuming a different count than
expected. `22.locale.time.get` fails 33: `get_date ("01/01/2000")`
consumes 10 characters where 8 are expected, and `get_weekday
("torsdag")` is not recognized. `22.locale.money.get` fails 16,
`long double` reads with `showbase` ending in `failbit`.

Read against the library, 2026-09-18, none of the three is what the
fact says.

`22.locale.num.get`: two causes. 108 of the failures are the test's:
a 2008 commit silencing 64-bit conversion warnings rewrote `sizeof
lmin - 1` as `INTSIZE (lmin - 1)`, the size of a pointer, at 22
sites, the limit strings of `long`, `unsigned long` and `void*` and
the `long double` cases, so they expected 8 characters consumed in
64-bit and 4 in 32-bit for every type. The other 96 are the
facet's: `num_get` collected digits in a 130-byte array on the
stack, sized for the widest integer in base 2, and set `failbit`
where the input outgrew it, under a FIXME from the 2005 import, while
the test, from the same year, feeds it a `long double` of
`LDBL_MAX_10_EXP + 1` zeros and a grouped `long` of 257 characters
and more on purpose. The array and the array of group sizes beside it
now move to one heap block of twice the size when full, the pointers
into them carried over; the exit test is the tree's own, the pointer
compared with the array and freed if it moved, held in a guard so
that the early returns and an exception from a facet take it too. The row
is at 65508/0 everywhere; the 96 more assertions are the grouping
loop running all nine lengths instead of stopping at the first
failure. `valgrind` on the test in 11S is clean.

`22.locale.time.get`: 32 of the 33 were the driver's.
`rw_locale_query` keeps its result in a static buffer whose length
was never reset, so every call appended to the previous result and a
query matching nothing returned the previous query's answer. The
test asks for an English, a German and a Danish locale in turn; on a
machine with `en_US` alone the German and Danish queries came back
`en_US.utf8` and their sections ran under it, "Sonntag" consumed as
the S of Sunday. With the length reset at entry the two sections are
skipped where their locale is absent, as the test intends. The same
defect inflated `22.locale.ctype.tolower`, which queries at every
loop entry without caching the list: 1538 assertions to the 1028 of
`toupper`, now equal. The last failure expected the English "%x" to
be 8 characters, the length of "%m/%d/%y" on the vendor Unixes of
2008; glibc's `en_US` has formatted the date as "%m/%d/%Y" since
2000, as does the tree's own `en_US` source, and the length now
comes from `strftime`, as the same function already sizes "%X". With
German and Danish locales generated into a scratch directory
(`localedef`, `LOCPATH`, and a `locale` shim on `PATH` that lists
them, since glibc's `locale -a` never lists `LOCPATH`), the German
section passes and the Danish one showed its seven abbreviated
weekday rows counting three bytes, which "søn" and "lør" are not in
UTF-8; they are locale-decided now, as the German rows were. The
row is at 2010/0 everywhere, the 24 warnings unchanged.

`22.locale.money.get`: two cases from the 2005 import of the test,
whose comments describe the intended behaviour, against facet rules
unchanged since the same import. Where `money_base::none` or
`money_base::space` appears in the pattern the facet read all the
whitespace it found, and where the currency symbol required after
it, or the rest of a multicharacter sign, began with whitespace,
nothing was left for them: a symbol of " " with `showbase` failed at
the end of "103 ", a negative sign of "- " at the end of "-109  ".
The standard makes the symbol and the rest of the sign required
after the other components and the whitespace optional, and a
reader of input iterators cannot tell the optional space from the
symbol's until the run ends. The facet now credits the whitespace it
read beyond the one character `space` requires to what follows:
whitespace the symbol begins with, before any character of it is
read from the input, and whitespace the rest of either sign begins
with. A sign character, a value or a symbol character read from the
input cancels the credit. A credited character satisfies a
whitespace character of the symbol or sign by class rather than
identity; no real locale's symbol or sign begins with whitespace,
so outside the two cases the credit is never spent. The row is at
3888/0 everywhere; the 8 more assertions are the string overload's
value check, reached for the two cases now.

Two findings on the way, queued in `TODO`: `money_get`'s own
304-byte value buffer has no bound check at all, and a 20000-digit
value dies of SIGSEGV; and the driver's `rw_ldblcmp` returns a sign,
so the `long double` value assertions of `22.locale.money.get`
cannot fail.

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

### 3.10 Numerics

Fact. `18.numeric.special.float` reports `has_denorm == 1` where the
test expects `-1` (`denorm_indeterminate`) for `float` and `long
double`, 3 assertions in 64-bit and 6 in 32-bit. `26.c.math` fails
the `modf` overloads for `float` and `long double` in 64-bit only.
`26.valarray.transcend` fails 3 in 32-bit only. `17.extensions`
fails 5 of 16, on `_RWSTD_NO_EXT_FAILURE` and its kin.

Hypothesis. Characterization questions: whether the config test or
the test's expectation is the one out of date.

Repair, 2026-09-18. The fact above was partly misread: `has_denorm`
failed for all three types in every configuration, the 32-bit GCC
build adding three `has_signaling_NaN` failures; the valarray row
failed its 3 everywhere; the extension failures were the five
`_RWSTD_NO_EXT_*_PRIMARY` macros. Four causes, none in the
library's numerics:

- `26.c.math`: the test's `check_bits` counts a `size_t` down past
  zero and compares the exhausted counter with `unsigned (-1)`,
  equal only where the two types have the same width. On LP64 every
  call answered no, so the `modf` overloads failed for writing past
  their argument, which they never did. The sentinel is
  `std::size_t (-1)` now. The helper dates from 2005, before the
  64-bit builds.
- `18.numeric.special.float`: the test's expected `has_denorm` was
  never a measurement but a pinned answer, `denorm_present` for
  three retired platforms and `denorm_indeterminate` for all others,
  where the library's answer comes from `INFINITY.cpp` reading the
  value of the smallest denormal. The expectation is derived now
  the way the test derives its other values: half the smallest
  normal, stored through a volatile, is a denormal where they are
  represented and zero where they are absent or flushed, under the
  same `_RWSTD_NO_DBL_TRAPS` guard the characterization computes
  under. The three further failures of the 32-bit GCC build were
  `has_signaling_NaN`, asserted true under `is_iec559` by the test
  and false by the library, whose `NO_SIGNALING_NAN.cpp` found none
  there: under GCC's x87 code generation a signaling NaN copied
  through the floating-point unit comes back quiet, `0x7ff0` in the
  top bytes becoming `0x7ff8`, where SSE code generation, Clang's
  default for i386 and everyone's for x86-64, preserves it. The
  type has the representation and the compiler cannot carry it; the
  library's answer describes what its `signaling_NaN()` returns.
  The test verifies `has_signaling_NaN` against the characterization
  under `is_iec559` as it already did outside it, so the 32-bit GCC
  row runs 119 assertions with the signaling NaN section skipped
  and the others 134. `VERIFY_CONST`'s message printed the trait's
  value as the expected and the literal as the observed; it prints
  the literal and the member now.
- `26.valarray.transcend`: `acos` and `asin` over 0 to 9 are NaN
  from 2 on, and the test compares element by element with
  `rw_equal`. For float and double the driver compares
  representations, so two NaNs of the same bits are equal; for long
  double the sign-only `rw_ldblcmp` of the queue's own entry called
  any NaN unequal. `rw_ldblcmp` compares representations now: the
  exponent and significand read as one ordinal in two 64-bit parts,
  so that the result is the number of representable values between
  the arguments, as it is for float and double, for the x87
  extended format (the explicit integer bit dropped so that the
  significands of consecutive exponents adjoin) and the 128-bit
  interchange format; other formats keep the relative-error path
  and its FIXME. Verified against `nextafterl` through all four
  driver libraries and, the same text retargeted at `__float128`
  through libquadmath, for the 128-bit format. Two callers had been
  written against the tolerant version. `26.c.math` compares
  `pow (long double, int)`, the library's exact power followed by
  one rounded division, with `powl`, which glibc does not round
  correctly on x87: one unit in the last place apart for 24 of the
  400 pairs, now the stated tolerance. `22.locale.money.get`
  expected `-109.1` for the input `-109` at zero fraction digits, a
  2005 typo beside its `-109.0` sibling that the sign-only
  comparison had hidden, and its two checks were one-sided
  (`2 > dist`, `1 < dist`); the row reads `-109.0` and the checks
  accept one unit either way. With every expected value shifted two
  units up, then down, 1704 of the row's 3888 assertions go red
  each way. `22.locale.num.get` spelled the expected value of a
  row as the type under test cast from a double literal, so its
  long double rows expected a double's precision where the facet
  reads the string with `strtold`: 240 rows within the tolerance
  and not within one unit, the value itself exact in the facet
  through all three iterator kinds. The literal carries the `L`
  suffix now, its nearest long double being what the string
  denotes; the float and double rows are unmoved.
- `17.extensions`: the five failing assertions demanded
  `_RWSTD_NO_EXT_MONEYPUNCT_PRIMARY` and its four siblings be
  defined under strict ANSI. `rw/_config.h` has carried the five
  commented out as "not implemented yet" since 2005, and the strict
  ANSI entry in `TODO` keeps the primary templates and retires the
  macros. The test no longer asks for gates the library never grew;
  the bogus primary templates it defined behind the same macros
  were dead code. 11 assertions remain.

The four rows are at 100% in the six configurations.

## 4. Differences between configurations

| test | 11s | 11S | 15D |
|---|---|---|---|
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
- `18.numeric.special.float` (chapter 3.10) runs 134 assertions in
  the Clang 32-bit build, the 64-bit count, where GCC's 32-bit build
  runs 119: the signaling NaN section is skipped where the
  characterization found the value cannot be carried, which is
  GCC's x87 code generation and not Clang's SSE.
- The MT rows of 15D vary under Clang as they do under GCC (chapter
  3.4) and are not a reference under either.
