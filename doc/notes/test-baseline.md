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
- The 64-bit archive build has two failures of its own, a
  nondeterministic assertion count and a segmentation fault, that
  the shared and 32-bit builds do not show.

Toolchain: GCC 16.2.1, glibc 2.44, x86-64 Linux, 2026-09-15.

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

The status column is the exit code when the program produced the
driver's output, otherwise one of the harness's words: `ABRT`,
`SEGV` and the like for signals; `HUP` for a program the harness
killed at the timeout; `NOUT` for no output; `FORMAT` for
output not in the driver's format; `EXEC` for a file that could not
be executed; `COMP` for a test that did not build.

Two kinds of row are not failures. The 82 regression tests under
`tests/regress` are plain programs that assert and exit; they print
nothing, so the harness reports `NOUT`, and their exit status is 0
where they pass (verified for a sample by hand). Three of the
driver's self-tests, `0.cmdopts`, `0.strncmp` and `0.valcmp`, print
their own report and exit 0; the harness reports `FORMAT`.

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
| 11S, debug, archive, 64-bit | `baseline/x86_64-11S.txt` | 268 | 10,068,455 | 735 | 2 | 9 |
| 11s, debug, archive, 32-bit | `baseline/i386-11s.txt` | 268 | 10,069,093 | 735 | 2 | 8 |
| 15D, debug, shared, threads, 64-bit | `baseline/x86_64-15D.txt` | 268 | 10,069,037 | 975 | 9 | 16 |

The 64-bit counts include `23.bitset.cons`, which varies from run
to run (chapter 3.5); the number pinned is the one measured with the
rest of the table, 0 failed in the 11S run pinned here and 240 in
the 15D one. The 15D counts include the MT locale tests, which are
not stable either (chapter 3.4), and that is the whole of the
difference between the 15D row and the other two. The tables were
pinned first with the two locale tests failing to read their input
files, re-pinned with the files in place (chapter 3.2), and re-pinned
again with the narrow transform repaired; each time the assertion
totals moved by those rows and by the two unstable ones.

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

Fact. In 11S, `23.bitset.cons` fails 240, 440 and 680 assertions in
three consecutive runs of one binary, and `25.reverse` dies of a
segmentation fault. Both pass in 11s and in 15D, so the variable is
the archive 64-bit library, not the word width.

Hypothesis. An uninitialized or out-of-bounds read that the shared
build's layout happens to tolerate. Check: the two tests in 11S
under a memory checker.

### 3.6 The driver's self-tests

Fact. `0.inputiter`, `0.outputiter` and `0.new` abort without a
message. `0.braceexp` exits 0 and prints nothing. `0.printf` fails 4
of 1876 assertions and `0.char` 1 of 479, all "misaligned address"
from the `%{#ls}`, `%{Ac}` and `%{/*Gs}` directives of the driver's
formatter. `0.fnmatch` fails one of its cases: `rw_fnmatch ("[a",
"[a", 0)` returns 1 where the native `fnmatch` returns 0.

Intended. One at a time, the silent aborts first.

### 3.7 Strings: ranges into the string itself

Fact. `21.string.assign`, `21.string.insert` and `21.string.replace`
fail 60, 150 and 240 assertions, every one with a source range that
is a reverse or bidirectional iterator into the string being
modified: `string ("abc").assign (rbegin-style range)` expected
"ba", got "bb". The regression tests `stdcxx-629`, `stdcxx-632` and
`stdcxx-170` abort on the same cases. `21.string.stdcxx-162` aborts
in 15D only, on its assumption `sizeof (strref) <= sizeof (SmallRef)`,
which does not hold when the reference count is atomic.

Hypothesis. The self-aliasing path of the iterator-range overloads
copies from storage it has already overwritten. The regression tests
are the gate.

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

- `0.char` and `0.printf` pass every assertion under Clang. Their
  "misaligned address" failures (chapter 3.6) are GCC's alone.
  `0.printf` reports in its own format, so the harness says `FORMAT`
  with exit 0, as it does for the three self-tests in chapter 1.
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

