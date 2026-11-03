# The test-suite baseline

As of 2026-11-03 (62777111), all six configurations.

The whole suite, run under its own harness on the current toolchain.
It is the reference for every change: which tests fail, by how much,
and why, where that is known. The numbers are pinned under
`doc/notes/baseline/`, one file per configuration. Each file is the
harness's own table without the timing columns.

## 0. The short version

- Every test builds. The harness runs 268 programs per configuration.
- No assertion fails, no program exits non-zero or dies of a signal,
  in any configuration, under either compiler.
- The tables come from a host with every glibc locale installed. A
  host with `en_US.UTF-8` alone differs from them in fourteen locale
  rows (chapter 2). A pin is a configuration's table, whatever the
  machine: the 15D table of an aarch64 host is the x86-64 one row for
  row.
- A failure in the tables is a fact to record, not a regression to
  chase, until its `TODO` entry says otherwise.

Toolchain: GCC 16.2.1, Clang 23.1.1, glibc 2.44, x86-64 Linux.

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

Of the 83 regression tests under `tests/regress`, 79 are plain
programs that assert and exit. They print nothing, so a pass is
`NOUT`. Two print without a summary, so a pass is `FORMAT`:
`27.basic_ios.copyfmt.stdcxx-1058` and
`27.stringbuf.members.stdcxx-427`. Two report through the driver:
`22.locale.cons.stdcxx-485` and `22.locale.messages.stdcxx-542`.

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
| 11S, debug, archive, 64-bit | `baseline/11S.txt` | 268 | 12,471,387 | 0 | 0 | 0 |
| 11s, debug, archive, 32-bit | `baseline/11s.txt` | 268 | 12,471,281 | 0 | 0 | 0 |
| 15D, debug, shared, threads, 64-bit | `baseline/15D.txt` | 268 | 12,475,059 | 0 | 0 | 0 |

Chapter 4 lists where the three differ.

The host the tables come from has every locale of glibc's
`SUPPORTED` list and the Shift_JIS one built by hand. The locale
tests run the sections gated on the C library knowing a named locale,
and fourteen rows depend on it: `22.locale.codecvt`, `collate`,
`cons.stdcxx-485`, the six `ctype` tests, `globals`, `moneypunct`,
`name`, `numpunct` and `time.get`. Each was read in its test: the
program asks the host for its locales with `rw_locale_query` or
`rw_locales` and runs sections only for the names it finds, or, in
`22.locale.name`, constructs locales by their C library names. The
thirteen other than `globals` also moved when the pins first came
from such a host (3c882e6a). The other programs that ask the host,
`22.locale.num.get`, `num.put`, `messages`, `codecvt.do_out`,
`codecvt.length` and the MT programs, use one name they find or run
a fixed number of rounds, and count the same on either host. A host
with `en_US.UTF-8` alone runs fewer assertions in each,
`22.locale.codecvt` 306 for 618 in 15D, and its tables differ from
the pins in those rows and the totals. The `run` target of
`etc/config/makefile.rules` clears `LANG`, `LANGUAGE` and the `LC_`
variables before it runs the tests:
`22.locale.moneypunct` exercises the native locale too, and counted
135020 assertions under none, 135050 under `LC_ALL=C` and 135100
under `LC_ALL=en_US.UTF-8` before the target cleared them. A table
depends on the host's locales alone.

`bin/driver` runs the certification. It exports one revision per
configuration, builds each through `bin/buildntest`, runs its suite,
writes a page per build, and compares each table with its pin. The
two are normalized by `bin/norm`, which keeps the table from its
heading to the failed assertions line, drops the timing columns and
squeezes the blanks. The verdict counts the tables identical to their
pins; one that differs fails the run, with the difference beside the
log. `bin/derive-pin` writes a pin from a run log.

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
`__rw_once` came in with its 600 assertions. No other row moved. The
GCC tables were measured again in full on
2026-10-13, at bf5fbd46, after the locale MT tests took sizes of
their own (`analysis-locale-mt-sizing.md`). Only their rows moved:
the seven timeouts of 15D pass, `22.locale.codecvt.mt` counts 1,600
assertions in 15D, not 12,800, and `22.locale.facet.mt` counts 8, not
40, in every configuration. All six were measured again on
2026-10-14, at c40c5d16, after the strict ANSI removals: `17.extensions`
is gone, `18.exception` counts 1,097, `21.cwchar` 118, `21.cwctype`
294, `22.locale.num.get` 51,894 and `23.bitset.cons` 1,677, in every
configuration; the Clang tables took the locale MT rows of 2026-10-13
in the same run. All six were measured again on 2026-10-19, at
d354aafd, on the host with every glibc locale, after the oracles of
`22.locale.codecvt` were repaired (`analysis-test-fixtures.md`,
chapter 4.5): thirteen of the locale rows above and `money.put.mt`
grew, and nothing else moved.

Since then the pins have moved with the commits below. From
df2646ee on, each commit moved the same rows by the same count in
all six tables:

- ed33d6d2, 2026-10-19, measured at 708f731d: the locale MT census
  of chapter 3.2. Seven rows went, `22.locale.globals` came, and the
  rewritten locale MT rows count; 268 programs.
- df2646ee, 2026-10-26: `22.locale.ctype.tolower` and `toupper`,
  19,388 to 19,390.
- 5bbe3e4b, 2026-10-26: `22.locale.collate`, 145,854 to 145,856.
- 592e858e, 2026-10-28: `22.locale.time.put`, 2,156 to 2,232; the
  totals were left 76 short.
- 5a8c2912, 2026-10-29: no row; the totals brought to their rows'
  sum.
- fa0fe473, 2026-10-29: `22.locale.ctype.is`, 25,432 to 25,434.
- e33e3773, 2026-10-29: `22.locale.ctype.scan`, 17,124 to 17,126.
- f56f7adb, 2026-10-29: `22.locale.collate`, 145,856 to 145,862.
- e4dedf65, 2026-10-29: `25.merge`, 23,364 to 23,472, and
  `25.sort`, 5,076 to 6,204.
- 11087a6a, 2026-10-29: `22.locale.time.put`, 2,232 to 2,272.
- 8820446f, 2026-10-29: `22.locale.time.put`, 2,272 to 2,304.
- 8cf34a6f, 2026-10-29: `22.locale.money.put`, 1,328 to 1,346.
- 9d3e7b3d, 2026-10-29: `22.locale.time.get`, 2,125 to 2,225, and
  its runtime warnings, 24 to 22.
- 967a487d, 2026-10-29: `22.locale.time.get`, 2,225 to 2,249.
- 4a1b1087, 2026-10-29: `22.locale.time.get`, 2,249 to 2,261.
- 59f3a87e, 2026-10-29: `22.locale.time.get`, 2,261 to 2,337, and
  its runtime warnings, 22 to 20.
- 2829b153, 2026-10-29: `22.locale.time.get`, 2,337 to 2,389.
- 892e6c86, 2026-10-29: `22.locale.time.get`, 2,389 to 2,493.
- e39b96fb, 2026-10-30: `22.locale.time.get`, 2,493 to 2,529.
- d85d3f98, 2026-10-30: `22.locale.time.get`, 2,529 to 2,561.
- 8abda9b3, 2026-10-30: `22.locale.collate`, 145,862 to 145,873.
- dd8f4349, 2026-10-30: `22.locale.ctype.is`, 25,434 to 25,438;
  `ctype.scan`, 17,126 to 17,132; `ctype.tolower` and `toupper`,
  19,390 to 19,395.
- b910448a, 2026-10-30: `22.locale.time.get`, 2,561 to 2,597.
- c964ca59, 2026-10-30: `22.locale.time.get`, 2,597 to 2,605, the
  rows d15125bb added without a pin.
- e438cc11, 2026-10-31: `22.locale.time.get`, 2,605 to 2,729, and
  its runtime warnings, 20 to 12.
- 503ae88c, 2026-10-31: `22.locale.time.get`, 2,729 to 2,819, and
  `22.locale.time.put`, 2,304 to 2,402.
- ffea5ffe, 2026-11-01: `22.locale.time.get`, 2,819 to 3,007, and
  its runtime warnings, 12 to 6.
- 90a98455, 2026-11-01: `22.locale.time.get`, 3,007 to 3,015.
- 50087549, 2026-11-01: `22.locale.time.get`, 3,015 to 3,027.
- 34e12013, 2026-11-02: `22.locale.ctype.toupper` and
  `22.locale.ctype.tolower`, 19,395 to 19,396 each.
- 76b24202, 2026-11-02: `22.locale.num.put`, 1,485 to 1,495 (1,481 to
  1,491 on 32-bit), and `22.locale.num.get`, 51,894 to 51,990.
- 0a3b2cca, 2026-11-03: `22.locale.num.get`, 51,990 to 55,206.
- 5438f794, 2026-11-03: `22.locale.money.put`, 1,346 to 1,352.
- 60c683eb, 2026-11-03: `22.locale.money.get`, 3,952 (3,960 on 32-bit) to
  4,160.
- f58562cd, 2026-11-03: `22.locale.num.put`, 1,495 to 1,499 (1,491 to
  1,495 on 32-bit).
- 9c4860bd, 2026-11-03: `22.locale.time.get`, 3,027 to 3,071.
- c580b059, 2026-11-03: `22.locale.time.get`, 3,071 to 3,131.
- 62777111, 2026-11-03: `22.locale.time.get`, 3,131 to 3,283.

The runtime warnings of every table fell from 60 to 42 with those of
`22.locale.time.get`.

## 3. What fails

### 3.1 `21.string.stdcxx-162`

Nothing fails. The test requires a string body of at most 24 bytes,
and in 15D every string body of the 4.2.x releases carried its own
mutex, 64 bytes, the layout of every release on Linux/x86-64. The
mutex went with that binary interface; the row is `NOUT`, a pass, in
every configuration.

### 3.2 The MT locale rows of 15D

Until 2026-10-13 seven older locale MT tests ended in 15D with
`HUP`: each ran one thread per processor, 32 on the machine the
tables come from, and ran each threaded section to its own 60-second
soft timeout, so the rows recorded the harness, not the library.
They were then sized under ThreadSanitizer, and on 2026-10-19 a
census of what each covered (the commits from 5c1368e6 to ed33d6d2)
retired six of them, `money.get.mt`,
`money.put.mt`, `num.get.mt`, `num.put.mt`, `time.get.mt` and
`time.put.mt` (the `get` and `put` facets hold no state of their
own), turned `globals.mt` into the single-threaded
`22.locale.globals`, and rewrote the rest to the gated shape.

Every locale MT program now provokes the race it covers: four
threads, whatever the processor count, wait on a gate and start
together at the first use of a fresh object, round after round, 200
rounds, and a check judges each round. Each runs in about a second
or less in 15D, and each counts in every configuration; with one
thread, the single-threaded builds run the rounds without the race.
The eleven, and what each races:

| program | races | 15D |
|---|---|---|
| `22.locale.use_facet.mt` | the facet slot of a fresh body, every standard facet in turn; leak check | 200 |
| `22.locale.facet.mt` | the facet reference counts | 8 |
| `22.locale.id.mt` | a user facet's id | 999 |
| `22.locale.codecvt.mt` | the wide `codecvt`'s database, on the first conversion | 1,600 |
| `22.locale.ctype.mt` | the `widen` and `narrow` caches of a fresh `ctype` facet | 800 |
| `22.locale.numpunct.mt` | the `numpunct` data built from the C library; leak check | 1,200 |
| `22.locale.moneypunct.mt` | the `moneypunct` data, the four facets in turn; leak check | 1,200 |
| `22.locale.time.put.libc.mt` | the `time_put` data, both character types in turn; leak check | 200 |
| `22.locale.messages.mt` | the catalog repository, `open` of one catalog from every thread; leak check | 600 |
| `22.locale.statics.mt` | the swap of the global locale, with the returned locales checked as a multiset | 200 |
| `22.locale.cons.mt` | the named-body repository, one body per name constructed from every thread at once, and the combinations | 1,000 |

The harness limits each test's address space to 1 GiB
(`etc/config/GNUmakefile.tst`), STDCXX-440's guard against a test
that takes the machine's memory. In thread-safe builds the limit
grows by 128 MiB per processor, since each thread maps an 8 MiB stack
and a 64 MiB `malloc` arena. On 32 processors 15D runs under 5 GiB.

## 4. Differences between configurations

| row | 11S | 11s | 15D |
|---|---|---|---|
| `18.numeric.special.float` | 134 | 119 | 134 |
| `20.temp.buffer` | 10891 | 10792 | 10891 |
| `22.locale.num.put` | 1499 | 1495 | 1499 |
| `22.locale.codecvt` | 634 | 638 | 618 |
| `atomic_add`, `atomic_xchg` | 0 | 0 | 66, 22 |
| `22.locale.codecvt.mt` | 400 | 400 | 1600 |
| `22.locale.ctype.mt` | 200 | 200 | 800 |
| `22.locale.numpunct.mt` | 600 | 600 | 1200 |
| `22.locale.moneypunct.mt` | 600 | 600 | 1200 |
| `22.locale.cons.mt` | 400 | 400 | 1000 |

Reasons, in order:

- 32-bit GCC skips the signaling NaN section of
  `18.numeric.special.float`. GCC's x87 code generation quiets a
  signaling NaN copied through the floating-point unit, so the
  library's characterization reports none, and the test checks
  `has_signaling_NaN` against the characterization.
- `20.temp.buffer` and `22.locale.num.put` run a few more cases in
  64-bit. Both pass everywhere.
- 15D skips the two libc-backed facets of `22.locale.codecvt` on a
  corrupt `mbstate_t`, 16 assertions. Their state check sits under
  the locale lock. The 4 between 11S and 11s have no known reason;
  they date from the first pins of a host with every locale
  (3c882e6a). `TODO` holds the entry, "`22.locale.codecvt` counts
  634 assertions in 11S and 638 in 11s".
- The atomic operation tests count only in the thread-safe build.
- `22.locale.codecvt.mt` counts two checks per thread and round,
  `22.locale.ctype.mt` one, `22.locale.numpunct.mt` and
  `22.locale.moneypunct.mt` one per thread and two per round, and
  `22.locale.cons.mt` one per thread and one per round: 400, 200, 600
  and 400 with the one thread of a single-threaded build; 1,600, 800,
  1,200 and 1,000 with four.

The single-threaded builds run each MT test with one thread. Two
count nothing in any build, `19.exceptions.mt` and
`20.temp.buffer.mt`. Eight count the same in every build:
`22.locale.use_facet.mt`, `22.locale.time.put.libc.mt` and
`22.locale.statics.mt` 200 each, `22.locale.messages.mt` 600,
`22.locale.id.mt` 999, `22.locale.facet.mt` 8, `21.string.cons.mt` 16
and `21.string.push_back.mt` 2.

## 5. The same suite under Clang

`11S-clang.txt`, `11s-clang.txt` and `15D-clang.txt` are the same
three configurations built with Clang (`CONFIG=gcc.config CXX=clang` on the `config` line), run and
pinned the same way. `analysis-clang.md` records what it took.

| configuration | assertions | failed | non-zero exits | signalled |
|---|---|---|---|---|
| 11S-clang | 12,471,387 | 0 | 0 | 0 |
| 11s-clang | 12,471,296 | 0 | 0 | 0 |
| 15D-clang | 12,475,059 | 0 | 0 | 0 |

Measured at 44420f7a, they matched the GCC tables of that date row
for row, with one exception. `18.numeric.special.float` runs 134
assertions in the 32-bit Clang build, where GCC's runs 119. Clang
generates SSE code for i386, which carries a signaling NaN.

Measured again at c40c5d16 on 2026-10-14, they match the GCC tables
row for row, `21.string.stdcxx-162` included, but for the
`18.numeric.special.float` row of the 32-bit build. `bin/driver`
takes a Clang configuration as the build type with `-clang` appended
and runs it through the same `buildntest` call with `CXX=clang` on
the make line.
