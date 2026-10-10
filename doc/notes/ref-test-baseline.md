# The test-suite baseline

As of 2026-10-10: 176773dd for GCC and Clang.

The whole suite, run under its own harness on the current toolchain.
It is the reference for every change: which tests fail, by how much,
and why, where that is known. The numbers are pinned under
`doc/notes/baseline/`, one file per configuration. Each file is the
harness's own table without the timing columns.

## 0. The short version

- Every test builds. The harness runs 278 programs per configuration.
- No assertion fails, in any configuration, under either compiler.
- What fails, by cause:
  - `21.string.stdcxx-162` in 15D. Its fix breaks the 4.2.x binary
    interface and moves the minor version (chapter 3.1).
  - Five locale MT rows of 15D time out. They measure the harness's
    timeout, not the library (chapter 3.2).
- A failure in the tables is a fact to record, not a regression to
  chase, until its `TODO` entry says otherwise.

Toolchain: GCC 16.2.1, Clang 22.1.8, glibc 2.44, x86-64 Linux.

## 1. Running the suite

Run `make run` from a build directory's `tests` subdirectory. It runs
every built test through `bin/exec`, the harness, and prints one row
per program and a summary. The timeout comes from the build's
`makefile.in`: `RUNFLAGS = -t 60`. The slowest single-threaded test,
`22.locale.codecvt`, takes 24 seconds in the 32-bit debug build.

The rule exports `TOPDIR`, `TMPDIR`, `TZ` and `LD_LIBRARY_PATH`. To
run a test by hand, set `TOPDIR` to the source tree and work from
the build's `tests` directory. The locale tests exec
`../bin/localedef`.

The tables are measured with `LANG` and `LC_ALL` set to
`en_US.UTF-8`, as a login shell sets them. One row reads the
environment. Without the variables `22.locale.moneypunct` counts 500
assertions, not 540.
A command run through `ssh` without a login shell has neither
variable.

Set only the timeout, and set it in `makefile.in`. `RUNFLAGS` on the
`make` command line replaces the whole variable. It drops the
`--compat` and `--ulimit` options the rules append, and the tables
come out different.

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

### 1.1 Rows that pass without a summary

The 83 regression tests under `tests/regress` are plain programs that
assert and exit. They print nothing, so a pass is `NOUT`.

Five driver self-tests report on their own. They test the driver's
support functions, not the harness's output parser, and none calls
`rw_test` or reports through `rw_assert`:

| test | successful output | failure contract | successful runner row |
|---|---|---|---|
| `0.cmdopts` | diagnostics from deliberately rejected arguments | unexpected parser or callback results set exit status 1 | `FORMAT` |
| `0.strncmp` | overload banners | a mismatch prints a diagnostic and sets exit status 2 | `FORMAT` |
| `0.valcmp` | specialization banners | a mismatch prints a diagnostic and sets exit status 1 | `FORMAT` |
| `0.braceexp` | none | a mismatch increments `nerrors`; main returns 1 if any occurred | `NOUT` |
| `0.printf` | progress and its own assertion summary | failed checks print a summary and return 1 | `FORMAT` |

Banners and silent success have been these programs' convention since
their first commits. Reporting through libc also keeps the observation
apart from what is observed: `0.cmdopts` replaces the option registry
the driver uses, and `0.printf` tests the formatter the driver's
diagnostics use.

The harness keeps these contracts. `run_target` in `util/runall.cpp`
calls `parse_output` only after a zero, unsignalled exit. `check_test`
and `check_compat_test` in `util/output.cpp` then classify empty
output as `ST_NO_OUTPUT` and a missing summary as `ST_FORMAT`. A
test that uses driver assertions still needs its summary; exit zero
alone does not show success.

## 2. The reference

Three configurations, named as the build system names them. The
number is the debugging, optimization and thread-safety level. `s`
is an archive, `d` a shared library. Lowercase is 32-bit code,
uppercase 64-bit.

| configuration | file | programs | assertions | failed | non-zero exits | signalled |
|---|---|---|---|---|---|---|
| 11S, debug, archive, 64-bit | `baseline/x86_64-11S.txt` | 278 | 10,104,244 | 0 | 0 | 0 |
| 11s, debug, archive, 32-bit | `baseline/i386-11s.txt` | 278 | 10,104,134 | 0 | 0 | 0 |
| 15D, debug, shared, threads, 64-bit | `baseline/x86_64-15D.txt` | 278 | 10,125,302 | 0 | 0 | 6 |

Chapter 4 lists where the three differ.

The tables were pinned on 2026-09-30 from full rebuilds, after the
address-space limit was scaled to the processor count (chapter 3.2)
and the `setbase` extension and `bin` were removed
(`analysis-numeric-base.md`). After the removal of the locking
extension the same day, 15D was measured again in full and the other
five in the rows that named the extension; only `27.basic.ios` moved,
in the two thread-safe tables (`analysis-basic-ios-flags.md`). After
the removal of the file stream additions, also that day, five
configurations were measured again in full and 15D-clang in its
`27.*` rows; only `27.filebuf` moved. The GCC tables were measured
again in full on 2026-10-07, at c755a926. Three rows moved, each with
the commit that changed its test: `19.cerrno` runs 4 assertions, not 5
(d006f308); `21.char.traits.compare` is new (6bb238b6);
`27.ostream.inserters.stdcxx-51` passes (415860cc). All six were
measured again in full on 2026-10-09, when the `once` test of
`__rw_once` came in with its 600 assertions. No other row moved.
15D was measured again in full on 2026-10-09, after
the codecvt MT program became seven and `22.locale.numpunct.mt` was
rebuilt on fresh-locale rounds; only those rows and the summary moved.
It was measured once more the same day, when the five codecvt programs
took the first-use shape and the first-use and mixed programs left:
only their rows and the summary moved. 11S and 11s were measured in
full that evening, at 64e1a829: the same rows and the summary moved,
and nothing else; their MT rows report no assertions, as before. The
three Clang tables were measured in full the same night, at 91c923f5,
with the same result; in 15D-clang `22.locale.numpunct.mt` passes as
in 15D. `22.locale.money.get.mt` timed out there, and the pin kept
its pass. 15D was measured again in full the same night, when
`22.locale.money.get.mt` moved to the round driver: only its row and
the assertion total moved. All six were measured in full at
176773dd the next night. Only 15D-clang moved, in the same row and
the total; the MT rows of the archive builds report no assertions.
15D was measured again in full on 2026-10-10, when
`22.locale.money.put.mt` moved to the round driver: its row, the
signalled count and the total moved, nothing else. The other five
tables pin its older row until a run measures them.

## 3. What fails

### 3.1 `21.string.stdcxx-162` in 15D

The test requires a string body of at most 24 bytes. In 15D every
string body carries its own mutex, 64 bytes: the layout of every
4.2.x release on Linux/x86-64. Atomic reference counts satisfy the
test and break that binary interface. The revival does not keep it;
the `TODO` entry on the next minor version makes the change. The row
is `ABRT` in 15D and `NOUT`, a pass, elsewhere.

### 3.2 The MT locale rows of 15D

Five older locale MT tests end in 15D with `HUP`:
`22.locale.ctype.mt`, `num.get.mt`, `num.put.mt`, `time.get.mt` and
`time.put.mt`. A test runs one thread per
processor, 32 on the machine the tables come from. It runs each
threaded section to its own 60-second soft timeout, and has up to
three sections. The harness's 60 seconds cut it short. Run bare at 16
threads, all sixteen locale MT tests of ae1fbe04 pass, these six
among them, in 2 to 143 seconds.

The other older tests pass. `cons.mt`, `globals.mt`, `messages.mt`,
`moneypunct.mt` and `statics.mt` count none: their pass means every
thread ran to the end without an error, a crash or a hang.

`22.locale.numpunct.mt` runs 64 rounds of four threads on fresh
locales built from the tree's sources and counts 2560 assertions in
about a second. The `TODO` entry "MT tests that provoke contention
instead of waiting for it" describes the shape.

The codecvt MT test is five programs in the same shape, one per
operation of the facet, each on `de_DE.ISO-8859-1` built from the
tree's sources, four threads, 64 rounds, no gates (the gate hides the
fault they cover, `analysis-facet-first-use.md` 2.4). Each thread
calls the wide and the narrow facet once per round.

| Program | Operation | Assertions |
|---|---|---|
| `22.locale.codecvt.in.mt` | `in ()`: result, counts, characters | 1024 |
| `22.locale.codecvt.out.mt` | `out ()`: result, counts, bytes | 1024 |
| `22.locale.codecvt.unshift.mt` | `unshift ()`: result, count | 512 |
| `22.locale.codecvt.length.mt` | `length ()` | 512 |
| `22.locale.codecvt.properties.mt` | `encoding ()`, `max_length ()`, `always_noconv ()` | 1024 |

`22.locale.money.get.mt` is one program in the same shape, since
`get ()` is one operation with two overloads. It runs on
`de_DE.ISO-8859-1` and `en_US.ISO-8859-1` built from the tree's
sources, four threads, 64 rounds, no gates, and counts 8192
assertions in about a second. Each thread calls both overloads for
both character types and both formats, on a positive and a negative
input. The two sources leave the international formats unspecified,
so the international parses fail on the first character; the test
compares that result too.

`22.locale.money.put.mt` is its counterpart: both `put ()` overloads
for `char`, the `long double` overload for `wchar_t`, both formats, a
positive and a negative value, the same two locales, 6144 assertions.
The wide string overload is left out until `ctype<wchar_t>::is ()`
answers from the database in these locales (`TODO`).

The round driver they and `22.locale.numpunct.mt` share is
`rw_first_use_rounds` in `librwtest` (`tests/include/rw_rounds.h`);
`--nthreads`, `--nrounds` and `--soft-timeout` override its counts.

The harness limits each test's address space to 1 GiB
(`etc/config/GNUmakefile.tst`), STDCXX-440's guard against a test
that takes the machine's memory. In thread-safe builds the limit
grows by 128 MiB per processor, since each thread maps an 8 MiB stack
and a 64 MiB `malloc` arena. On 32 processors 15D runs under 5 GiB.

The four gated MT tests, `22.locale.use_facet.mt`,
`22.locale.time.put.libc.mt`, `22.locale.id.mt` and
`22.locale.facet.mt`, pass in every configuration.

The `TODO` entry "MT tests that provoke contention instead of waiting
for it" covers the timeouts. Until it is done the six rows record
the harness, not a reference.

## 4. Differences between configurations

| row | 11S | 11s | 15D |
|---|---|---|---|
| `18.numeric.special.float` | 134 | 119 | 134 |
| `20.temp.buffer` | 10891 | 10792 | 10891 |
| `22.locale.num.put` | 1485 | 1481 | 1485 |
| `22.locale.money.get` | 3952 | 3960 | 3952 |
| `22.locale.codecvt` | 322 | 322 | 306 |
| `atomic_add`, `atomic_xchg` | 0 | 0 | 66, 22 |
| `21.string.stdcxx-162` | `NOUT` | `NOUT` | `ABRT` |
| five `22.locale.codecvt.*.mt` | 0 | 0 | 1024, 1024, 512, 512, 1024 |
| `22.locale.numpunct.mt` | 0 | 0 | 2560 |
| six `22.locale.*.mt` | pass | pass | `HUP`, chapter 3.2 |

Reasons, in order:

- 32-bit GCC skips the signaling NaN section of
  `18.numeric.special.float`. GCC's x87 code generation quiets a
  signaling NaN copied through the floating-point unit, so the
  library's characterization reports none, and the test checks
  `has_signaling_NaN` against the characterization.
- `20.temp.buffer` and `22.locale.num.put` run a few more cases in
  64-bit. Both pass everywhere.
- `22.locale.money.get` runs 8 more cases with a 32-bit `size_t`: a
  `frac_digits` of 2^30 would wrap the facet's buffer size, and the
  facet throws `bad_alloc`. With a 64-bit `size_t` nothing wraps and
  the cases are skipped with a note. The harness counts warnings, not
  notes.
- 15D skips the two libc-backed facets of `22.locale.codecvt` on a
  corrupt `mbstate_t`. Their state check sits under the locale lock.
- The atomic operation tests count only in the thread-safe build.
- `21.string.stdcxx-162`: chapter 3.1.
- The five codecvt MT programs and `22.locale.numpunct.mt` count
  only in the thread-safe build.
- Six older locale MT tests time out in 15D: chapter 3.2.

The single-threaded builds run each MT test with one thread. Most
count nothing there. The newer ones count in every build:
`22.locale.use_facet.mt` and `22.locale.time.put.libc.mt` 200 each,
`22.locale.id.mt` 999, `22.locale.facet.mt` 40,
`21.string.cons.mt` 16, `21.string.push_back.mt` 2. These pass in
15D too. `22.locale.money.put.mt` counted 6 in the single-threaded
builds; on the round driver it reports none there, which their
tables show once a run measures them.

## 5. The same suite under Clang

`x86_64-11S-clang.txt`, `i386-11s-clang.txt` and
`x86_64-15D-clang.txt` are the same three configurations built with
Clang (`CONFIG=gcc.config CXX=clang` on the `config` line), run and
pinned the same way. `analysis-clang.md` records what it took.

| configuration | assertions | failed | non-zero exits | signalled |
|---|---|---|---|---|
| 11S-clang | 10,104,244 | 0 | 0 | 0 |
| 11s-clang | 10,104,149 | 0 | 0 | 0 |
| 15D-clang | 10,119,158 | 0 | 0 | 7 |

They match the GCC tables row for row, with one exception.
`18.numeric.special.float` runs 134 assertions in the 32-bit Clang
build, where GCC's runs 119. Clang generates SSE code for i386, which
carries a signaling NaN. The two 15D tables were identical when both
were last measured; the timeouts measure the harness under either
compiler (chapter 3.2).
