# The locale MT tests sized for ThreadSanitizer

A plain run of a locale MT test catches a race only when two threads
arrive in its window. Whether they do is set by the kernel's
thread-creation stagger, the same for every round. The same binaries
caught one fault in 20 runs of 20 under one kernel and in 0 of 100
under the next (`analysis-facet-first-use.md`, 2.4). ThreadSanitizer
reports a race from the accesses and the synchronization that ran,
not from their timing, so two threads and a few rounds can show what
thousands of iterations may miss. This note records how far each
program's threads and iterations come down with the sanitizer still
reporting the fault the program covers, and what the runs take.

## 0. tl;dr

All 18 programs are sized: chapter 2 has each program's size and
time, chapter 3 the runs. At its size every program but
`22.locale.cons.mt` catches a known fault in every run against a
library with the fault put back, and stays clean against the fixed
library. Most sizes are four threads and 256 iterations, room above a
floor of a few iterations. The 18 programs take about 253 s plain at
their defaults on the host below; at their sizes, about 2.5 s plain
and 4.7 s under the sanitizer. Two of the faults are leaks, not races:
the sanitizer does not see them, and only the programs' own leak
checks do. librwtest runs clean under the sanitizer: the soft timeout
and the replacement `operator delete` are fixed.

## 1. Method

No source changes in this phase. Each program is run through its own
options: `--nthreads`, `--nloops`, `--nrounds`, `--nlocales`,
`--ncatalogs`.

The build is the one in `analysis-locale-mt-race.md`, chapter 1: a 15D
build directory with `-fsanitize=thread` on the library, librwtest
and the tests. The configuration does not see the
option, so `config.h` is the plain 15D one. A 12D build made the same
way is the last pass, once the sizes are chosen: optimization takes
code paths the debug build does not.

The utilities stay uninstrumented: `localedef` is a separate,
single-threaded process, and under the sanitizer it takes 6.7 s to
build `de_DE.ISO-8859-1` against 0.04 s plain. The tests find it
through `BINDIR`, which points at the plain 15D build's `bin`.

A fault is put back in an export of the tree (`git archive`), built as
a build directory of its own: library, librwtest and the programs, so
that its programs find its library through their own `RUNPATH`.
`LD_LIBRARY_PATH` would also reach the plain `localedef`, which then
fails, and the tests skip what they cannot build.

The runs below are on an aarch64 host with 10 processors and four C
library locales: `C`, `C.utf8`, `en_US.utf8`, `POSIX`.

For each program:

- A plain 15D run at its defaults, timed. From 3.12 on, the defaults
  are not run under the sanitizer: such a run takes minutes, often
  ends at a soft timeout, and decides nothing the sizes do not.
- Runs under the sanitizer at decreasing sizes. At each size, one run
  against the fixed library and runs against a library with the
  program's fault put back. `analysis-locale-mt-race.md`, chapter 7,
  lists the faults; the commits of the sweep of one-time
  initializations are faults too. A commit is reverted where it still
  applies as a patch; where later commits changed its lines, chiefly
  the atomic sweep's renamed operations, it is put back by hand, and
  each section says how.
- The floor is the smallest size at which every run against the fault
  reports it and the run against the fixed library reports nothing.
  The chosen size leaves room above it, mostly four threads and 256
  iterations, and is confirmed by 25 runs against the fault and six
  or more against the fixed library.
- A program with no known fault is sized by what it runs: the smallest
  size at which each of its operations still runs from more than one
  thread on fresh state.

The sanitizer does not see a race whose accesses are all ordered but
whose outcome is wrong: two critical sections whose combination is
not atomic. The program's own assertions keep that case.

## 2. The programs

Defaults as the sources set them. Every program runs one thread per
processor, at least 2; `22.locale.facet.mt` and `22.locale.id.mt` stop
at 8. `--nthreads` on the command line is capped at the program's
`MAX_THREADS`, 16 or 32, but the default is not: a 32-processor host
runs `22.locale.ctype.mt` at 32 threads, a 10-processor one at 10.
"Covers" names the commit whose fault the program is known to catch;
a dash means not yet established.

| program | defaults | covers | plain 15D at defaults | command under the sanitizer | time | reports |
|---|---|---|---|---|---|---|
| `22.locale.codecvt.mt` | 5000 loops | 8c4282ee | 1.7 s | `./22.locale.codecvt.mt --nthreads=4 --nloops=256` | 1.1 s | none; 5 per run against the fault, 25 runs of 25 |
| `22.locale.cons.mt` | 20000 loops | – | 6.5 s | `./22.locale.cons.mt --nthreads=4 --nloops=256` | 0.13 s | none; none against dcb5dc92 or ae1fbe04 |
| `22.locale.ctype.mt` | 200000 loops | bf6dc721 | 25.6 s | `./22.locale.ctype.mt --nthreads=4 --nloops=256` | 0.4 s | none; 4 to 6 per run against the fault, 50 runs of 50 |
| `22.locale.facet.mt` | at most 8 threads, 20 rounds, 1000 loops | dcb5dc92 | 0.06 s | `./22.locale.facet.mt --nthreads=4 --nrounds=4 --nloops=256` | 0.02 s | none; 2 per run against the fault, 25 runs of 25 |
| `22.locale.globals.mt` | 20000 loops | ae1fbe04 | 6.0 s | `./22.locale.globals.mt --nthreads=4 --nloops=256` | 0.3 s | none; 332 to 396 per run against the fault, 25 runs of 25 |
| `22.locale.id.mt` | at most 8 threads, 500 rounds (fixed) | 46aa42c3 | 0.10 s | `./22.locale.id.mt --nthreads=4` | 0.2 s | none; 2 to 5 per run against the fault, 30 runs of 30 |
| `22.locale.messages.mt` | 10000 loops, 11 catalogs | ae1fbe04 | 6.2 s | `./22.locale.messages.mt --nthreads=4 --nloops=256` | 0.2 s | none; 9 to 31 per run against the fault, 25 runs of 25 |
| `22.locale.money.get.mt` | 100000 loops | 9a482a4f, ae1fbe04 | 17.7 s | `./22.locale.money.get.mt --nthreads=4 --nloops=256` | 0.1 s | none; 57 to 95 per run against 9a482a4f, 25 runs of 25 |
| `22.locale.money.put.mt` | 100000 loops | 9a482a4f, ae1fbe04 | 22.8 s | `./22.locale.money.put.mt --nthreads=4 --nloops=256` | 0.1 s | none; 14 to 21 per run against 9a482a4f, 25 runs of 25 |
| `22.locale.moneypunct.mt` | 10000 loops | 9a482a4f, ae1fbe04 | 2.5 s | `./22.locale.moneypunct.mt --nthreads=4 --nloops=256` | 0.1 s | none; 97 to 113 per run against 9a482a4f, 25 runs of 25 |
| `22.locale.num.get.mt` | 100000 loops | 9a482a4f, ae1fbe04 | 23.1 s | `./22.locale.num.get.mt --nthreads=4 --nloops=256` | 0.2 s | none; 28 to 37 per run against 9a482a4f, 25 runs of 25 |
| `22.locale.num.put.mt` | 100000 loops | 9a482a4f, ae1fbe04 | 26.1 s | `./22.locale.num.put.mt --nthreads=4 --nloops=256` | 0.2 s | none; 23 to 30 per run against 9a482a4f, 25 runs of 25 |
| `22.locale.numpunct.mt` | 200000 loops | 2e196ebb, 9a482a4f, ae1fbe04 | 34.6 s | `./22.locale.numpunct.mt --nthreads=4 --nloops=256` | 0.1 s | none; 16 to 93 per run against 2e196ebb, 25 runs of 25 |
| `22.locale.statics.mt` | 20000 loops | 5b68c2a7 | 0.68 s | `./22.locale.statics.mt --nthreads=4 --nloops=256` | < 0.1 s | none; 1 per run against the fault, 30 runs of 30 |
| `22.locale.time.get.mt` | 50000 loops | 9a482a4f, ae1fbe04 | 57.7 s | `./22.locale.time.get.mt --nthreads=4 --nloops=256` | 0.3 s | none; 38 to 48 per run against 9a482a4f, 25 runs of 25 |
| `22.locale.time.put.libc.mt` | 200 rounds | 81ef680c, 9a482a4f | 0.38 s | `./22.locale.time.put.libc.mt --nthreads=4` | 0.1 s | none; 81ef680c by the program's assertion, 9a482a4f by 7 or 8 reports, 25 runs of 25 each |
| `22.locale.time.put.mt` | 100000 loops | 9a482a4f, ae1fbe04 | 20.4 s | `./22.locale.time.put.mt --nthreads=4 --nloops=256` | 0.1 s | none; 13 to 19 per run against 9a482a4f, 25 runs of 25 |
| `22.locale.use_facet.mt` | 200 rounds | 2bbdf7c9, ae1fbe04 | 1.0 s | `./22.locale.use_facet.mt --nthreads=4` | 1.0 s | none; 2bbdf7c9 by the program's assertion, ae1fbe04 by 1 report, 25 runs of 25 each |

`--nlocales` defaults to `MAX_THREADS` wherever a program has it.

## 3. Per program

### 3.1 `22.locale.codecvt.mt`

Four sections. Three run the steady state: each thread constructs a
named locale per iteration and calls one of the seven members of
`codecvt`, the member and the locale chosen by the iteration number,
in `char`, in `wchar_t`, then in both. The locales are the C library's,
so what they cover depends on the host. The fourth races the first
conversion through `de_DE.ISO-8859-1` built from the tree's sources,
200 rounds, a constant with no option. Only the fourth can see
8c4282ee.

With four locales and seven members, 28 iterations run every member
in every locale, the floor for the steady-state sections. The thread
count does not change detection: two threads report the fault in
every run. The 200 rounds set the floor of about a second. The size
was first two threads and 28 iterations, the floor; it is four
threads and 256 iterations like the others' since the sizes became
the defaults, and still reports the fault in every run.

| command | library | time | reports |
|---|---|---|---|
| `./22.locale.codecvt.mt` | plain 15D | 1.7 s | – |
| `./22.locale.codecvt.mt --nthreads=2 --nloops=28` | plain 15D | 0.96 s | – |
| `./22.locale.codecvt.mt` | fixed | 4.0 s | 0 |
| `./22.locale.codecvt.mt` | 8c4282ee | 4.1 s | 5, 5 runs of 5 |
| `./22.locale.codecvt.mt --nthreads=4` | fixed | 2.5 s | 0 |
| `./22.locale.codecvt.mt --nthreads=4` | 8c4282ee | 2.7 s | 5, 5 runs of 5 |
| `./22.locale.codecvt.mt --nthreads=2` | fixed | 1.6 s | 0 |
| `./22.locale.codecvt.mt --nthreads=2` | 8c4282ee | 1.7 s | 5, 5 runs of 5 |
| `./22.locale.codecvt.mt --nthreads=4 --nloops=28` | fixed | 1.1 s | 0 |
| `./22.locale.codecvt.mt --nthreads=4 --nloops=28` | 8c4282ee | 1.2 s | 5, 5 runs of 5 |
| `./22.locale.codecvt.mt --nthreads=2 --nloops=28` | fixed | 1.1 s | 0 |
| `./22.locale.codecvt.mt --nthreads=2 --nloops=28` | 8c4282ee | 1.1 s | 5, 5 runs of 5 |
| `./22.locale.codecvt.mt --nthreads=4 --nloops=256` | plain 15D | 0.98 s | – |
| `./22.locale.codecvt.mt --nthreads=4 --nloops=256` | fixed | 1.1 s | 0 |
| `./22.locale.codecvt.mt --nthreads=4 --nloops=256` | 8c4282ee | 1.1 s | 5, 25 runs of 25 |

The five reports against the fault are on the facet data's path: the
acquire load of `_C_impsize` against the plain store, and the reads of
the codecvt tables (`n_to_w_tab`, `get_internal_at_offset`,
`__rw_mbtowco`).

### 3.2 `22.locale.cons.mt`

One section. Each thread constructs two locales from one name, copies
one, constructs the next locale by name, and combines a locale with a
name and with another locale for one of eight categories: `all`, the
six, `none`. The locale is the iteration number modulo the number of
locales, the category modulo eight. With four locales, 8 iterations
run every category and every locale; the category fixes the locale, so
not every pairing runs. The locales are the C library's. The library
keeps a named locale's body after its first construction, so only the
first iterations race its initialization. Its checks are `RW_ASSERT`,
which aborts; a clean exit is the evidence.

No fault of chapter 7 is on its path. It never calls `use_facet` and
never adds a facet to a locale, so every facet slot of the bodies it
builds stays empty, and `__rw_locale::_C_construct` copies and counts
the facets of filled slots only. The two faults nearest to it were put
back, and it reports neither, at the chosen size or at its defaults:
dcb5dc92, the reference counts that `_C_construct` takes under each
facet's mutex, and ae1fbe04, the slot filled with a plain store and
read with plain loads. Both fault libraries report through the
programs that cover them: `22.locale.facet.mt` gives two reports
against dcb5dc92, in `_C_construct` with a facet added, and
`22.locale.use_facet.mt` one against ae1fbe04, the slot's store in
`locale::_C_get_std_facet`. The atomic sweep renamed the operations
ae1fbe04 changed, so its commit no longer reverts as a patch; the
store and the two loads were made plain by hand, and the facet id is
again stored on every construction.

The floor is two threads and 8 iterations. The size is four threads
and 256 iterations like the others', since the sizes became the
defaults: each of the eight pairings of category and locale it runs
comes 32 times. The sanitizer reports nothing at any size, against
either fault or the fixed library.

| command | library | time | reports |
|---|---|---|---|
| `./22.locale.cons.mt` | plain 15D | 6.5 s | – |
| `./22.locale.cons.mt --nthreads=2 --nloops=8` | plain 15D | < 0.01 s | – |
| `./22.locale.cons.mt` | fixed | 32.7 s | 0 |
| `./22.locale.cons.mt` | dcb5dc92 | 32.1 s | 0, 2 runs of 2 |
| `./22.locale.cons.mt` | ae1fbe04 | 31.1 s | 0, 2 runs of 2 |
| `./22.locale.cons.mt --nthreads=4` | fixed | 14.7 s | 0 |
| `./22.locale.cons.mt --nthreads=2` | fixed | 6.4 s | 0 |
| `./22.locale.cons.mt --nthreads=4 --nloops=8` | fixed | 0.02 s | 0 |
| `./22.locale.cons.mt --nthreads=2 --nloops=8` | fixed | 0.02 s | 0 |
| `./22.locale.cons.mt --nthreads=2 --nloops=8` | dcb5dc92 | 0.02 s | 0, 5 runs of 5 |
| `./22.locale.cons.mt --nthreads=2 --nloops=8` | ae1fbe04 | 0.02 s | 0, 5 runs of 5 |
| `./22.locale.cons.mt --nthreads=4 --nloops=256` | plain 15D | 0.02 s | – |
| `./22.locale.cons.mt --nthreads=4 --nloops=256` | fixed | 0.13 s | 0 |

### 3.3 `22.locale.ctype.mt`

Three sections: `ctype<char>`, `ctype<wchar_t>`, then both. Each thread
constructs a named locale per iteration, takes its `ctype` facet and
calls `widen` and `narrow`, the locale and the character chosen by the
iteration number. The main thread computes the expected results
through the same members first, in locales that are gone before the
threads start, so the threads' facets are new: the sanitizer places
every racing facet in a block allocated by a worker thread. The locales
are the C library's.

It covers bf6dc721, the `narrow` and `widen` caches of both
specializations. The atomic sweep renamed the operations the commit
changed, so it no longer reverts as a patch; the put-back makes its
thirteen relaxed loads and stores plain, `codecvt<char,
char>::always_noconv` among them, each cache still read once per call.
Against it the sanitizer names the four members, a read or a store in
one thread against a store in another.

A facet lives while some thread's locale holds it, so whether two
threads meet on one is still a matter of timing. With two threads the
first section missed in 6 runs of 40 at 8 loops, and in 2 or 3 of 25
at every loop count up to 256: two reports, the wide members only.
Four threads met in every run, down to 4 loops, one per locale, the
floor. The size keeps four threads and takes 256 loops for room above
it. At the defaults under the sanitizer the second and third sections
end at their 60-second soft timeout against the fault.

| command | library | time | reports |
|---|---|---|---|
| `./22.locale.ctype.mt` | plain 15D | 25.6 s | – |
| `./22.locale.ctype.mt --nthreads=4 --nloops=256` | plain 15D | 0.11 s | – |
| `./22.locale.ctype.mt` | fixed | 118.3 s | 0 |
| `./22.locale.ctype.mt` | bf6dc721 | 176.6 s | 8 and 11, 2 runs of 2 |
| `./22.locale.ctype.mt --nthreads=2 --nloops=4096` | fixed | 1.1 s | 0 |
| `./22.locale.ctype.mt --nthreads=2 --nloops=4096` | bf6dc721 | 2.3 to 3.3 s | 4 to 6, 3 runs of 3 |
| `./22.locale.ctype.mt --nthreads=2 --nloops=256` | fixed | 0.2 s | 0 |
| `./22.locale.ctype.mt --nthreads=2 --nloops=256` | bf6dc721 | 0.2 to 1.0 s | 4 in 23 runs of 25, 2 in the others |
| `./22.locale.ctype.mt --nthreads=2 --nloops=64` | fixed | 0.1 s | 0 |
| `./22.locale.ctype.mt --nthreads=2 --nloops=64` | bf6dc721 | 0.2 s | 4 in 22 runs of 25, 2 in the others |
| `./22.locale.ctype.mt --nthreads=2 --nloops=16` | fixed | 0.1 s | 0 |
| `./22.locale.ctype.mt --nthreads=2 --nloops=16` | bf6dc721 | 0.2 s | 4 in 23 runs of 25, 2 in the others |
| `./22.locale.ctype.mt --nthreads=2 --nloops=8` | fixed | 0.1 s | 0 |
| `./22.locale.ctype.mt --nthreads=2 --nloops=8` | bf6dc721 | 0.2 s | 4 in 23 runs of 25, 2 in the others |
| `./22.locale.ctype.mt --nthreads=2 --nloops=4` | fixed | 0.1 s | 0 |
| `./22.locale.ctype.mt --nthreads=2 --nloops=4` | bf6dc721 | 0.2 s | 2 in 4 runs of 5, 4 in the other |
| `./22.locale.ctype.mt --nthreads=2 --nloops=1` | fixed | 0.1 s | 0 |
| `./22.locale.ctype.mt --nthreads=2 --nloops=1` | bf6dc721 | 0.1 s | 0, 5 runs of 5 |
| `./22.locale.ctype.mt --nthreads=4 --nloops=4` | fixed | 0.1 s | 0 |
| `./22.locale.ctype.mt --nthreads=4 --nloops=4` | bf6dc721 | 0.2 s | 4 to 6, 25 runs of 25 |
| `./22.locale.ctype.mt --nthreads=4 --nloops=8` | fixed | 0.1 s | 0 |
| `./22.locale.ctype.mt --nthreads=4 --nloops=8` | bf6dc721 | 0.2 s | 4 or 5, 25 runs of 25 |
| `./22.locale.ctype.mt --nthreads=4 --nloops=256` | fixed | 0.2 s | 0, 6 runs of 6 |
| `./22.locale.ctype.mt --nthreads=4 --nloops=256` | bf6dc721 | 0.3 to 0.9 s | 4 to 6 in 50 runs of 50; the four members in each of the 25 checked by name |

### 3.4 `22.locale.facet.mt`

One section, gated. Each round the main thread builds a base locale
holding a user-defined facet that counts its destructions; the threads
wait for each other at a barrier, then each builds and destroys
locales from the base with a facet added. After the round the facet
must have been destroyed once, and not before the base went. Its
checks are `rw_assert`: a lost update is a failed assertion.

It covers dcb5dc92. Against the fault the sanitizer names the shared
facet's reference count: the increment `_C_construct` takes under the
facet's mutex, read and write, against the lock-free decrement in
`~__rw_locale`. The program's own assertion fails as well, in each
of 25 runs against the fault at the chosen size: the facet is never
destroyed.

One iteration in one round of two threads reports it in every run,
one of the two reports; 16 iterations give both in about two runs of
three. Four threads, four rounds and 256 iterations gave both in every
run. The plain program is already fast at its defaults, so the size is
for the sanitizer and leaves room above the floor.

| command | library | time | reports |
|---|---|---|---|
| `./22.locale.facet.mt` | plain 15D | 0.06 s | – |
| `./22.locale.facet.mt --nthreads=4 --nrounds=4 --nloops=256` | plain 15D | < 0.01 s | – |
| `./22.locale.facet.mt` | fixed | 0.5 s | 0 |
| `./22.locale.facet.mt` | dcb5dc92 | 0.5 to 1.1 s | 2, 5 runs of 5 |
| `./22.locale.facet.mt --nthreads=2` | fixed | 0.2 s | 0 |
| `./22.locale.facet.mt --nthreads=2` | dcb5dc92 | 0.2 to 1.0 s | 2, 5 runs of 5 |
| `./22.locale.facet.mt --nthreads=2 --nrounds=1` | fixed | < 0.1 s | 0 |
| `./22.locale.facet.mt --nthreads=2 --nrounds=1` | dcb5dc92 | 0.7 to 1.3 s | 2, 5 runs of 5 |
| `./22.locale.facet.mt --nthreads=2 --nrounds=1 --nloops=16` | fixed | < 0.1 s | 0 |
| `./22.locale.facet.mt --nthreads=2 --nrounds=1 --nloops=16` | dcb5dc92 | < 0.1 s | 2 in 17 runs of 25, 1 in the others |
| `./22.locale.facet.mt --nthreads=2 --nrounds=1 --nloops=4` | fixed | < 0.1 s | 0 |
| `./22.locale.facet.mt --nthreads=2 --nrounds=1 --nloops=4` | dcb5dc92 | < 0.1 s | 2 in 12 runs of 25, 1 in the others |
| `./22.locale.facet.mt --nthreads=2 --nrounds=1 --nloops=1` | fixed | < 0.1 s | 0 |
| `./22.locale.facet.mt --nthreads=2 --nrounds=1 --nloops=1` | dcb5dc92 | < 0.1 s | 1, 5 runs of 5 |
| `./22.locale.facet.mt --nthreads=4 --nrounds=4 --nloops=256` | fixed | 0.02 s | 0, 6 runs of 6 |
| `./22.locale.facet.mt --nthreads=4 --nrounds=4 --nloops=256` | dcb5dc92 | 0.1 to 0.4 s | 2, 25 runs of 25; the assertion fails in each of 25 more |

### 3.5 `22.locale.globals.mt`

Two sections, `has_facet`, then `use_facet`. Each thread constructs a
named locale per iteration, the locale chosen by the iteration number,
and asks it for every standard facet of both character types. In a
named locale each `_byname` facet must be the base facet; in the
classic one, asking for a `_byname` facet must throw. The locales are
the C library's, `C` among them.

It covers ae1fbe04. Every report against the fault is on the facet
slot: the load in `__rw_get_std_facet` against the store in
`locale::_C_get_std_facet`. One iteration of two threads, in `C`
only, gave 17 to 26 reports in every run.

The size is that of `22.locale.ctype.mt`, four threads and 256
iterations, with room above that floor. Against the fault the reports
slow a run to about 10 s, against 0.3 s on the fixed library. At the
defaults under the sanitizer both sections end at their 60-second soft
timeout against the fault.

| command | library | time | reports |
|---|---|---|---|
| `./22.locale.globals.mt` | plain 15D | 6.0 s | – |
| `./22.locale.globals.mt --nthreads=4 --nloops=256` | plain 15D | 0.05 s | – |
| `./22.locale.globals.mt` | fixed | 36.5 s | 0 |
| `./22.locale.globals.mt` | ae1fbe04 | 120.1 s | 438 |
| `./22.locale.globals.mt --nthreads=2 --nloops=1` | fixed | < 0.1 s | 0 |
| `./22.locale.globals.mt --nthreads=2 --nloops=1` | ae1fbe04 | 0.2 to 0.4 s | 17 to 26, 5 runs of 5 |
| `./22.locale.globals.mt --nthreads=2 --nloops=5` | fixed | < 0.1 s | 0 |
| `./22.locale.globals.mt --nthreads=2 --nloops=5` | ae1fbe04 | 0.6 to 1.1 s | 56 to 95, 5 runs of 5 |
| `./22.locale.globals.mt --nthreads=4 --nloops=256` | fixed | 0.3 to 0.4 s | 0, 6 runs of 6 |
| `./22.locale.globals.mt --nthreads=4 --nloops=256` | ae1fbe04 | 8.8 to 12.3 s | 332 to 396, 25 runs of 25 |

### 3.6 `22.locale.id.mt`

One section, gated, 500 rounds, a constant: each round is a facet type
of its own, a template instantiation, so the rounds cannot be an
option. Each round the threads wait at a barrier, then each installs a
facet of the round's type in a copy of the classic locale and asks for
it back. The facet types used in order must take consecutive ids; a
gap is an id generated by one thread and overwritten by another's.
The gap check is `rw_assert`, and the first gap ends the run.

It covers 46aa42c3. Against the fault the sanitizer names the id's
store in `__rw_facet_id::_C_init`, the unlocked store of one thread
against another's, and the gap check fails in every run: the fault
runs end in the first rounds and are quicker than the fixed ones.

The thread count is the only size. Two threads report the fault in
every run; the size takes four, for room.

| command | library | time | reports |
|---|---|---|---|
| `./22.locale.id.mt` | plain 15D | 0.10 s | – |
| `./22.locale.id.mt --nthreads=4` | plain 15D | 0.03 s | – |
| `./22.locale.id.mt` | fixed | 0.3 s | 0 |
| `./22.locale.id.mt` | 46aa42c3 | 0.1 s | 3 to 5, 5 runs of 5 |
| `./22.locale.id.mt --nthreads=2` | fixed | 0.1 s | 0 |
| `./22.locale.id.mt --nthreads=2` | 46aa42c3 | < 0.1 s | 2, 5 runs of 5 |
| `./22.locale.id.mt --nthreads=4` | fixed | 0.1 to 0.2 s | 0, 6 runs of 6 |
| `./22.locale.id.mt --nthreads=4` | 46aa42c3 | 0.1 s | 2 to 5, 30 runs of 30; the gap check fails in each of the 25 counted |

### 3.7 `22.locale.messages.mt`

Three sections: `messages<char>`, `messages<wchar_t>`, then both. The
main thread writes 11 catalogs first. Each thread constructs a named
locale per iteration, takes its `messages` facet, closes the catalog
it opened in the same slot iterations before, opens the next by name,
and reads a message from it every other iteration. A thread keeps four
narrow and five wide catalog slots. The catalog, the slot and the
locale are the iteration number modulo 11, the slot count and the
number of locales, so with four locales 220 iterations run every
combination in every section. The locales are the C library's.

No fault of chapter 7 is in the catalog code. Each iteration's
`use_facet` on a fresh locale crosses the facet slot, and the program
reports ae1fbe04: every report follows from a facet published without
ordering, its readers in `_C_get_facet_type`, `_C_is_managed`,
`messages::open`, and the `codecvt` facet the catalog converts with.
One iteration of two threads reported it in one run of five; four
threads and 256 iterations in every run.

The size is four threads and 256 iterations, above the 220 that run
every combination.

| command | library | time | reports |
|---|---|---|---|
| `./22.locale.messages.mt` | plain 15D | 6.2 s | – |
| `./22.locale.messages.mt --nthreads=4 --nloops=256` | plain 15D | 0.08 s | – |
| `./22.locale.messages.mt` | fixed | 18.5 s | 0 |
| `./22.locale.messages.mt` | ae1fbe04 | 17.3 s | 7 |
| `./22.locale.messages.mt --nthreads=2 --nloops=1` | fixed | < 0.1 s | 0 |
| `./22.locale.messages.mt --nthreads=2 --nloops=1` | ae1fbe04 | < 0.1 s | 2 in 1 run of 5, 0 in the others |
| `./22.locale.messages.mt --nthreads=4 --nloops=256` | fixed | 0.2 s | 0, 6 runs of 6 |
| `./22.locale.messages.mt --nthreads=4 --nloops=256` | ae1fbe04 | 0.3 to 0.7 s | 9 to 31 in the first 5; reported in 25 runs of 25 |

### 3.8 `22.locale.money.get.mt`

Three sections: `money_get<char>`, `money_get<wchar_t>`, then both.
The main thread formats a value with `money_put` in each locale first.
Each thread constructs a named locale per iteration, takes its
`money_get` facet and parses the locale's string back, into a `long
double` on odd iterations and into a string on even ones. The locale
is the iteration number modulo the number of locales, so with four
locales the parity fixes the overload: not every pairing runs. The
locales are the C library's: the database has no data for them, and
`moneypunct` builds its data from the C library under
`__rw_setlocale`.

It covers 9a482a4f, the publication of that data. The atomic sweep
renamed the operations it changed; put back by hand, the builders'
stores and the loads of the data are plain. Against it the sanitizer names the
stores in `__rw_get_moneypunct` against the readers of the
`moneypunct` data, its strings and signs. Each iteration's `use_facet`
crosses the facet slot, and the program reports ae1fbe04 as well.

One iteration of two threads, in `C` only, reports neither fault. Four
iterations report 9a482a4f in every run, two threads or four, and 16
give 20 or more reports a run. The size is four threads and 256
iterations, for room.

| command | library | time | reports |
|---|---|---|---|
| `./22.locale.money.get.mt` | plain 15D | 17.7 s | – |
| `./22.locale.money.get.mt --nthreads=4 --nloops=256` | plain 15D | 0.03 s | – |
| `./22.locale.money.get.mt` | fixed | 108.2 s | 0 |
| `./22.locale.money.get.mt` | 9a482a4f | 115.7 s | 114 |
| `./22.locale.money.get.mt --nthreads=2 --nloops=1` | fixed | < 0.1 s | 0 |
| `./22.locale.money.get.mt --nthreads=2 --nloops=1` | 9a482a4f | < 0.1 s | 0, 5 runs of 5 |
| `./22.locale.money.get.mt --nthreads=2 --nloops=1` | ae1fbe04 | < 0.1 s | 0, 5 runs of 5 |
| `./22.locale.money.get.mt --nthreads=2 --nloops=4` | fixed | < 0.1 s | 0 |
| `./22.locale.money.get.mt --nthreads=2 --nloops=4` | 9a482a4f | 0.1 to 0.2 s | 2 to 12, 5 runs of 5 |
| `./22.locale.money.get.mt --nthreads=4 --nloops=4` | fixed | < 0.1 s | 0 |
| `./22.locale.money.get.mt --nthreads=4 --nloops=4` | 9a482a4f | 0.1 to 0.2 s | 2 to 11, 5 runs of 5 |
| `./22.locale.money.get.mt --nthreads=4 --nloops=16` | fixed | < 0.1 s | 0 |
| `./22.locale.money.get.mt --nthreads=4 --nloops=16` | 9a482a4f | 0.2 to 0.5 s | 14 to 29, 5 runs of 5 |
| `./22.locale.money.get.mt --nthreads=4 --nloops=256` | fixed | 0.1 s | 0, 6 runs of 6 |
| `./22.locale.money.get.mt --nthreads=4 --nloops=256` | 9a482a4f | 1.3 to 2.4 s | 57 to 95, 25 runs of 25 |
| `./22.locale.money.get.mt --nthreads=4 --nloops=256` | ae1fbe04 | 1.6 to 3.4 s | 70 to 96, 5 runs of 5 |

### 3.9 `22.locale.money.put.mt`

The mirror of `22.locale.money.get.mt`, three sections, `char`,
`wchar_t`, then both. The main thread formats a value in each locale
first, with an overload and an international flag of the locale's own;
each thread constructs a named locale per iteration, formats the same
value with `money_put` and compares. The locales are the C library's;
the program keeps three of the host's four, `C`, `en_US.utf8` and
`POSIX`.

It covers 9a482a4f, through the same put-back as 3.8: the sanitizer
names the stores in `__rw_get_moneypunct` against the readers of the
data. Each iteration's `use_facet` crosses the facet slot, and the
program reports ae1fbe04 as well.

One iteration of two threads reports neither fault. Four iterations
report 9a482a4f in most runs, but one run of five with four threads
reported nothing; 16 and more reported in every run. The size is four
threads and 256 iterations, for room.

| command | library | time | reports |
|---|---|---|---|
| `./22.locale.money.put.mt` | plain 15D | 22.8 s | – |
| `./22.locale.money.put.mt --nthreads=4 --nloops=256` | plain 15D | 0.03 s | – |
| `./22.locale.money.put.mt` | fixed | 116.5 s | 0 |
| `./22.locale.money.put.mt` | 9a482a4f | 127.4 s | 20 |
| `./22.locale.money.put.mt --nthreads=2 --nloops=1` | fixed | < 0.1 s | 0 |
| `./22.locale.money.put.mt --nthreads=2 --nloops=1` | 9a482a4f | < 0.1 s | 0, 5 runs of 5 |
| `./22.locale.money.put.mt --nthreads=2 --nloops=4` | fixed | < 0.1 s | 0 |
| `./22.locale.money.put.mt --nthreads=2 --nloops=4` | 9a482a4f | 0.1 to 0.2 s | 5 to 7, 5 runs of 5 |
| `./22.locale.money.put.mt --nthreads=4 --nloops=4` | fixed | < 0.1 s | 0 |
| `./22.locale.money.put.mt --nthreads=4 --nloops=4` | 9a482a4f | < 0.1 s | 2 to 7 in 4 runs of 5, 0 in the other |
| `./22.locale.money.put.mt --nthreads=4 --nloops=16` | fixed | < 0.1 s | 0 |
| `./22.locale.money.put.mt --nthreads=4 --nloops=16` | 9a482a4f | 0.1 to 0.2 s | 5 to 12, 5 runs of 5 |
| `./22.locale.money.put.mt --nthreads=4 --nloops=256` | fixed | 0.1 s | 0, 6 runs of 6 |
| `./22.locale.money.put.mt --nthreads=4 --nloops=256` | 9a482a4f | 0.6 to 1.0 s | 14 to 21, 25 runs of 25 |
| `./22.locale.money.put.mt --nthreads=4 --nloops=256` | ae1fbe04 | 1.0 to 1.3 s | 35 to 42, 5 runs of 5 |

### 3.10 `22.locale.moneypunct.mt`

Three sections, `char`, `wchar_t`, then both. The main thread records
every member of the four `moneypunct` facets in each locale first.
Each thread constructs a named locale per iteration, the locale
chosen by the iteration number, and calls every member of the
domestic and the international facet of the section's character
types, comparing each with the record. The locales are the C
library's.

It covers 9a482a4f, through the put-back of 3.8: every member reads
the data the builder publishes. Each iteration's `use_facet` crosses
the facet slot, and the program reports ae1fbe04 as well.

One iteration of two threads, in `C` only, reports neither. Four
iterations reported 9a482a4f in every run, two threads or four. The
size is four threads and 256 iterations, for room.

| command | library | time | reports |
|---|---|---|---|
| `./22.locale.moneypunct.mt` | plain 15D | 2.5 s | – |
| `./22.locale.moneypunct.mt --nthreads=4 --nloops=256` | plain 15D | 0.04 s | – |
| `./22.locale.moneypunct.mt` | fixed | 11.0 s | 0 |
| `./22.locale.moneypunct.mt` | 9a482a4f | 66.9 s | 140 |
| `./22.locale.moneypunct.mt --nthreads=2 --nloops=1` | fixed | < 0.1 s | 0 |
| `./22.locale.moneypunct.mt --nthreads=2 --nloops=1` | 9a482a4f | < 0.1 s | 0, 5 runs of 5 |
| `./22.locale.moneypunct.mt --nthreads=2 --nloops=4` | fixed | < 0.1 s | 0 |
| `./22.locale.moneypunct.mt --nthreads=2 --nloops=4` | 9a482a4f | 0.1 to 0.6 s | 4 to 47, 5 runs of 5 |
| `./22.locale.moneypunct.mt --nthreads=4 --nloops=4` | fixed | < 0.1 s | 0 |
| `./22.locale.moneypunct.mt --nthreads=4 --nloops=4` | 9a482a4f | 0.5 to 0.6 s | 41 to 48, 5 runs of 5 |
| `./22.locale.moneypunct.mt --nthreads=4 --nloops=16` | fixed | < 0.1 s | 0 |
| `./22.locale.moneypunct.mt --nthreads=4 --nloops=16` | 9a482a4f | 0.8 to 1.1 s | 57 to 75, 5 runs of 5 |
| `./22.locale.moneypunct.mt --nthreads=4 --nloops=256` | fixed | 0.1 s | 0, 6 runs of 6 |
| `./22.locale.moneypunct.mt --nthreads=4 --nloops=256` | 9a482a4f | 2.5 to 5.8 s | 97 to 113, 25 runs of 25 |
| `./22.locale.moneypunct.mt --nthreads=4 --nloops=256` | ae1fbe04 | 2.7 to 3.9 s | 45 to 55, 5 runs of 5 |

### 3.11 `22.locale.num.get.mt`

Three sections, `char`, `wchar_t`, then both. The main thread formats
a value in each locale first, its type fixed by the locale's place in
the list, one of eight from `bool` to `void*`; with four locales four
types run. Each thread constructs a named locale per iteration, takes
its `num_get` facet and parses the value back. The locales are the C
library's, and `numpunct` builds its data from the C library.

It covers 9a482a4f, through the put-back of 3.8: the sanitizer names
the stores in `__rw_get_numpunct` against the readers of the
`numpunct` data. Each iteration's `use_facet` crosses the facet slot,
and the program reports ae1fbe04 as well.

One iteration of two threads, in `C` only, reports neither. Four
iterations reported 9a482a4f in every run, two threads or four. The
size is four threads and 256 iterations, for room. At the defaults
under the sanitizer one section ends at its 60-second soft timeout,
with the fixed library and against the fault.

| command | library | time | reports |
|---|---|---|---|
| `./22.locale.num.get.mt` | plain 15D | 23.1 s | – |
| `./22.locale.num.get.mt --nthreads=4 --nloops=256` | plain 15D | 0.04 s | – |
| `./22.locale.num.get.mt` | fixed | 138.3 s | 0 |
| `./22.locale.num.get.mt` | 9a482a4f | 156.7 s | 44 |
| `./22.locale.num.get.mt --nthreads=2 --nloops=1` | fixed | < 0.1 s | 0 |
| `./22.locale.num.get.mt --nthreads=2 --nloops=1` | 9a482a4f | < 0.1 s | 0, 5 runs of 5 |
| `./22.locale.num.get.mt --nthreads=2 --nloops=4` | fixed | < 0.1 s | 0 |
| `./22.locale.num.get.mt --nthreads=2 --nloops=4` | 9a482a4f | 0.2 to 0.4 s | 13 to 21, 5 runs of 5 |
| `./22.locale.num.get.mt --nthreads=4 --nloops=4` | fixed | < 0.1 s | 0 |
| `./22.locale.num.get.mt --nthreads=4 --nloops=4` | 9a482a4f | 0.2 to 0.3 s | 14 to 17, 5 runs of 5 |
| `./22.locale.num.get.mt --nthreads=4 --nloops=16` | fixed | < 0.1 s | 0 |
| `./22.locale.num.get.mt --nthreads=4 --nloops=16` | 9a482a4f | 0.3 to 0.5 s | 12 to 24, 5 runs of 5 |
| `./22.locale.num.get.mt --nthreads=4 --nloops=256` | fixed | 0.2 s | 0, 6 runs of 6 |
| `./22.locale.num.get.mt --nthreads=4 --nloops=256` | 9a482a4f | 1.2 to 2.6 s | 28 to 37, 25 runs of 25 |
| `./22.locale.num.get.mt --nthreads=4 --nloops=256` | ae1fbe04 | 1.6 to 2.5 s | 49 to 58, 5 runs of 5 |

### 3.12 `22.locale.num.put.mt`

The mirror of `22.locale.num.get.mt`, three sections, `char`,
`wchar_t`, then both, the value type fixed by the locale's place in
the list. Each thread constructs a named locale per iteration, formats
the locale's value with `num_put` and compares it with the main
thread's. The locales are the C library's.

It covers 9a482a4f, through the put-back of 3.8: the sanitizer names
the stores in `__rw_get_numpunct` against the readers of the
`numpunct` data. Each iteration's `use_facet` crosses the facet slot,
and the program reports ae1fbe04 as well.

One iteration of two threads, in `C` only, reports neither. Four
iterations reported 9a482a4f in every run, two threads or four. The
size is four threads and 256 iterations, for room.

| command | library | time | reports |
|---|---|---|---|
| `./22.locale.num.put.mt` | plain 15D | 26.1 s | – |
| `./22.locale.num.put.mt --nthreads=4 --nloops=256` | plain 15D | 0.05 s | – |
| `./22.locale.num.put.mt --nthreads=2 --nloops=1` | fixed | < 0.1 s | 0 |
| `./22.locale.num.put.mt --nthreads=2 --nloops=1` | 9a482a4f | < 0.1 s | 0, 5 runs of 5 |
| `./22.locale.num.put.mt --nthreads=2 --nloops=4` | fixed | < 0.1 s | 0 |
| `./22.locale.num.put.mt --nthreads=2 --nloops=4` | 9a482a4f | 0.1 to 0.2 s | 6 to 12, 5 runs of 5 |
| `./22.locale.num.put.mt --nthreads=4 --nloops=4` | fixed | < 0.1 s | 0 |
| `./22.locale.num.put.mt --nthreads=4 --nloops=4` | 9a482a4f | 0.1 to 0.2 s | 8 to 12, 5 runs of 5 |
| `./22.locale.num.put.mt --nthreads=4 --nloops=16` | fixed | < 0.1 s | 0 |
| `./22.locale.num.put.mt --nthreads=4 --nloops=16` | 9a482a4f | 0.3 to 0.4 s | 11 to 18, 5 runs of 5 |
| `./22.locale.num.put.mt --nthreads=4 --nloops=256` | fixed | 0.2 s | 0, 6 runs of 6 |
| `./22.locale.num.put.mt --nthreads=4 --nloops=256` | 9a482a4f | 1.3 to 2.1 s | 23 to 30, 25 runs of 25 |
| `./22.locale.num.put.mt --nthreads=4 --nloops=256` | ae1fbe04 | 2.8 to 4.0 s | 79 to 97, 5 runs of 5 |

### 3.13 `22.locale.numpunct.mt`

Three sections, `char`, `wchar_t`, then both. The main thread records
the five members of `numpunct` in each locale first. Each thread
constructs a named locale per iteration, the locale chosen by the
iteration number, calls the five members and compares. Its checks are
`RW_ASSERT`, which aborts. The locales are the C library's.

It covers 2e196ebb. The commit reverts as a patch: `numpunct` caches
the results of its virtuals again, in members filled without a lock.
Against it the sanitizer names the cache and the strings it holds,
and the program's own assertion catches a wrong `grouping` in most
runs at the chosen size, aborting the run: this is the race the
revival started from. Its data comes from the C library, so the
program reports 9a482a4f as well, and ae1fbe04 through each
iteration's `use_facet`.

One iteration of two threads, in `C` only, reports no fault. Four
iterations reported 2e196ebb in every run, two threads or four;
9a482a4f missed in two runs of five with two threads and four
iterations, and never at 16 or more. The size is four threads and 256
iterations, for room.

| command | library | time | reports |
|---|---|---|---|
| `./22.locale.numpunct.mt` | plain 15D | 34.6 s | – |
| `./22.locale.numpunct.mt --nthreads=4 --nloops=256` | plain 15D | 0.03 s | – |
| `./22.locale.numpunct.mt --nthreads=2 --nloops=1` | fixed | < 0.1 s | 0 |
| `./22.locale.numpunct.mt --nthreads=2 --nloops=1` | 2e196ebb | < 0.1 s | 0, 5 runs of 5 |
| `./22.locale.numpunct.mt --nthreads=2 --nloops=1` | 9a482a4f | < 0.1 s | 0, 5 runs of 5 |
| `./22.locale.numpunct.mt --nthreads=2 --nloops=4` | fixed | < 0.1 s | 0 |
| `./22.locale.numpunct.mt --nthreads=2 --nloops=4` | 2e196ebb | 0.1 to 0.2 s | 13 to 18, 5 runs of 5 |
| `./22.locale.numpunct.mt --nthreads=2 --nloops=4` | 9a482a4f | < 0.1 to 0.2 s | 13 to 15 in 3 runs of 5, 0 in the others |
| `./22.locale.numpunct.mt --nthreads=4 --nloops=4` | fixed | < 0.1 s | 0 |
| `./22.locale.numpunct.mt --nthreads=4 --nloops=4` | 2e196ebb | 0.2 to 0.3 s | 17 to 28, 5 runs of 5 |
| `./22.locale.numpunct.mt --nthreads=4 --nloops=4` | 9a482a4f | 0.1 s | 2 to 13, 5 runs of 5 |
| `./22.locale.numpunct.mt --nthreads=4 --nloops=16` | fixed | < 0.1 s | 0 |
| `./22.locale.numpunct.mt --nthreads=4 --nloops=16` | 2e196ebb | 0.3 to 0.6 s | 24 to 47, 15 runs of 15; the assertion aborted 2 of the last 10 |
| `./22.locale.numpunct.mt --nthreads=4 --nloops=16` | 9a482a4f | 0.3 to 0.4 s | 21 to 28, 5 runs of 5 |
| `./22.locale.numpunct.mt --nthreads=4 --nloops=256` | fixed | 0.1 s | 0, 7 runs of 7 |
| `./22.locale.numpunct.mt --nthreads=4 --nloops=256` | 2e196ebb | 0.2 to 3.1 s | 16 to 93, 25 runs of 25; the assertion aborted 15 of the last 20 |
| `./22.locale.numpunct.mt --nthreads=4 --nloops=256` | 9a482a4f | 1.4 to 1.7 s | 32 to 41, 5 runs of 5 |
| `./22.locale.numpunct.mt --nthreads=4 --nloops=256` | ae1fbe04 | 0.9 to 1.5 s | 19 to 25, 5 runs of 5 |

### 3.14 `22.locale.statics.mt`

Two sections. In the first, gated, the threads wait for each other and
copy `locale::classic ()` once. In the second each thread makes each
of the locales global in turn, the locale chosen by the iteration
number. The locales are the C library's.

Chapter 7 predates the sweep of one-time initializations, whose
commits are faults too. The program covers 5b68c2a7, the global
locale initialized through `__rw_once`. The commit reverts as a
patch. Against it the sanitizer names the global locale's pointer:
the unlocked test in `__rw_locale::_C_manage` against the store under
its guard, both from `locale::global`. One iteration of two threads
reports it in every run.

It does not cover 4f38276b, the relaxed accesses to the classic body
in the same function, nor does any size reach it, the defaults
included: the pointer is stored once, when the classic locale is first
built, and the program's own array of locales, default-constructed at
namespace scope, builds it during static initialization.

The size is four threads and 256 iterations, for room; under the
sanitizer that is a tenth of a second.

| command | library | time | reports |
|---|---|---|---|
| `./22.locale.statics.mt` | plain 15D | 0.68 s | – |
| `./22.locale.statics.mt --nthreads=4 --nloops=256` | plain 15D | < 0.01 s | – |
| `./22.locale.statics.mt` | fixed | 3.2 s | 0 |
| `./22.locale.statics.mt` | 4f38276b | 3.2 s | 0, 2 runs of 2 |
| `./22.locale.statics.mt --nthreads=2 --nloops=1` | fixed | < 0.1 s | 0 |
| `./22.locale.statics.mt --nthreads=2 --nloops=1` | 5b68c2a7 | < 0.1 s | 1, 6 runs of 6 |
| `./22.locale.statics.mt --nthreads=2 --nloops=1` | 4f38276b | < 0.1 s | 0, 5 runs of 5 |
| `./22.locale.statics.mt --nthreads=2 --nloops=4` | 5b68c2a7 | < 0.1 s | 1, 5 runs of 5 |
| `./22.locale.statics.mt --nthreads=4 --nloops=4` | 5b68c2a7 | < 0.1 s | 1, 5 runs of 5 |
| `./22.locale.statics.mt --nthreads=4 --nloops=16` | 5b68c2a7 | < 0.1 s | 1, 5 runs of 5 |
| `./22.locale.statics.mt --nthreads=4 --nloops=256` | fixed | < 0.1 s | 0, 7 runs of 7 |
| `./22.locale.statics.mt --nthreads=4 --nloops=256` | 5b68c2a7 | 0.1 s | 1, 30 runs of 30 |
| `./22.locale.statics.mt --nthreads=4 --nloops=256` | 4f38276b | < 0.1 s | 0, 5 runs of 5 |

### 3.15 `22.locale.time.get.mt`

Three sections, `char`, `wchar_t`, then both. The main thread formats
a time in each locale first, its conversion fixed by the locale's
place in the list, from `%X` and `%x` to the day and month names. Each
thread constructs a named locale per iteration, takes its `time_get`
facet and parses the string back with the matching member. The
locales are the C library's, and the time data comes from
`__rw_get_timepunct` in `src/time_put.cpp`. The plain program takes
57.7 s at its defaults, three seconds under the harness's timeout.

It covers 9a482a4f, through the put-back of 3.8: the sanitizer names
the stores in `__rw_get_timepunct` against the readers of the time
data. Each iteration's `use_facet` crosses the facet slot, and the
program reports ae1fbe04 as well.

One iteration of two threads, in `C` only, reported 9a482a4f in four
runs of five; four iterations and more in every run. The size is four
threads and 256 iterations, for room.

| command | library | time | reports |
|---|---|---|---|
| `./22.locale.time.get.mt` | plain 15D | 57.7 s | – |
| `./22.locale.time.get.mt --nthreads=4 --nloops=256` | plain 15D | 0.12 s | – |
| `./22.locale.time.get.mt --nthreads=2 --nloops=1` | fixed | < 0.1 s | 0 |
| `./22.locale.time.get.mt --nthreads=2 --nloops=1` | 9a482a4f | < 0.1 s | 2 in 4 runs of 5, 0 in the other |
| `./22.locale.time.get.mt --nthreads=2 --nloops=4` | fixed | < 0.1 s | 0 |
| `./22.locale.time.get.mt --nthreads=2 --nloops=4` | 9a482a4f | 0.1 to 0.4 s | 8 to 24, 5 runs of 5 |
| `./22.locale.time.get.mt --nthreads=4 --nloops=4` | fixed | < 0.1 s | 0 |
| `./22.locale.time.get.mt --nthreads=4 --nloops=4` | 9a482a4f | 0.1 to 0.4 s | 7 to 28, 5 runs of 5 |
| `./22.locale.time.get.mt --nthreads=4 --nloops=16` | fixed | < 0.1 s | 0 |
| `./22.locale.time.get.mt --nthreads=4 --nloops=16` | 9a482a4f | 0.2 to 0.7 s | 10 to 31, 5 runs of 5 |
| `./22.locale.time.get.mt --nthreads=4 --nloops=256` | fixed | 0.3 to 0.4 s | 0, 7 runs of 7 |
| `./22.locale.time.get.mt --nthreads=4 --nloops=256` | 9a482a4f | 1.9 to 4.3 s | 38 to 48, 25 runs of 25 |
| `./22.locale.time.get.mt --nthreads=4 --nloops=256` | ae1fbe04 | 1.4 to 1.7 s | 35 to 43, 5 runs of 5 |

### 3.16 `22.locale.time.put.libc.mt`

One section, gated, 200 rounds. Each round the main thread constructs a
fresh locale that only the C library knows, `C.utf8` here, and takes
its `time_put` facet; the threads wait for each other and call `put`
on it at once, the first call, which builds the facet's data. After
the round the locale goes, and the driver's replacement `operator new`
counts the blocks that outlive it. The leak check is `rw_assert`, and
the first failure ends the run.

It covers 81ef680c. The commit reverts as a patch: every thread that
waited on `__rw_setlocale` builds the data again and overwrites the
pointer, so all builds but the last leak. That is a leak, not a race:
the builds are serialized by the lock, and the sanitizer reports
nothing. The program's own leak check fails in the first round of
every run, with two threads and one round already. The program also
covers 9a482a4f, through the put-back of 3.8: the sanitizer names the
stores in `__rw_get_timepunct` against the readers of the data, in
every run at every size.

The rounds are fresh locales and cheap under the sanitizer, so the
size keeps the default 200 and takes four threads.

| command | library | time | reports |
|---|---|---|---|
| `./22.locale.time.put.libc.mt` | plain 15D | 0.38 s | – |
| `./22.locale.time.put.libc.mt --nthreads=4` | plain 15D | 0.03 s | – |
| `./22.locale.time.put.libc.mt --nthreads=2 --nrounds=1` | fixed | < 0.1 s | 0 |
| `./22.locale.time.put.libc.mt --nthreads=2 --nrounds=1` | 81ef680c | < 0.1 s | 0; the leak check fails, 5 runs of 5 |
| `./22.locale.time.put.libc.mt --nthreads=2 --nrounds=1` | 9a482a4f | < 0.1 s | 2, 5 runs of 5 |
| `./22.locale.time.put.libc.mt --nthreads=4 --nrounds=4` | 81ef680c | < 0.1 s | 0; the leak check fails, 5 runs of 5 |
| `./22.locale.time.put.libc.mt --nthreads=4 --nrounds=4` | 9a482a4f | 0.1 s | 3 to 7, 5 runs of 5 |
| `./22.locale.time.put.libc.mt --nthreads=4 --nrounds=64` | 81ef680c | < 0.1 s | 0; the leak check fails, 5 runs of 5 |
| `./22.locale.time.put.libc.mt --nthreads=4 --nrounds=64` | 9a482a4f | 0.1 to 0.2 s | 4 to 8, 5 runs of 5 |
| `./22.locale.time.put.libc.mt --nthreads=4` | fixed | 0.1 s | 0, 11 runs of 11 |
| `./22.locale.time.put.libc.mt --nthreads=4` | 81ef680c | < 0.1 s | 0; the leak check fails, 30 runs of 30 |
| `./22.locale.time.put.libc.mt --nthreads=4` | 9a482a4f | 0.3 s | reported in 30 runs of 30, 7 or 8 in the 5 counted |

### 3.17 `22.locale.time.put.mt`

The mirror of `22.locale.time.get.mt`, three sections, `char`,
`wchar_t`, then both, the conversion fixed by the locale's place in
the list. Each thread constructs a named locale per iteration, formats
the locale's time with `time_put` and compares it with the main
thread's. The locales are the C library's.

It covers 9a482a4f, through the put-back of 3.8: the sanitizer names
the stores in `__rw_get_timepunct` against the readers of the time
data. Each iteration's `use_facet` crosses the facet slot, and the
program reports ae1fbe04 as well.

One iteration of two threads, in `C` only, reports neither. Four
iterations reported 9a482a4f in every run, two threads or four. The
size is four threads and 256 iterations, for room.

| command | library | time | reports |
|---|---|---|---|
| `./22.locale.time.put.mt` | plain 15D | 20.4 s | – |
| `./22.locale.time.put.mt --nthreads=4 --nloops=256` | plain 15D | 0.03 s | – |
| `./22.locale.time.put.mt --nthreads=2 --nloops=1` | fixed | < 0.1 s | 0 |
| `./22.locale.time.put.mt --nthreads=2 --nloops=1` | 9a482a4f | < 0.1 s | 0, 5 runs of 5 |
| `./22.locale.time.put.mt --nthreads=2 --nloops=4` | fixed | < 0.1 s | 0 |
| `./22.locale.time.put.mt --nthreads=2 --nloops=4` | 9a482a4f | 0.1 to 0.2 s | 5 to 9, 5 runs of 5 |
| `./22.locale.time.put.mt --nthreads=4 --nloops=4` | fixed | < 0.1 s | 0 |
| `./22.locale.time.put.mt --nthreads=4 --nloops=4` | 9a482a4f | 0.1 s | 4 to 7, 5 runs of 5 |
| `./22.locale.time.put.mt --nthreads=4 --nloops=16` | fixed | < 0.1 s | 0 |
| `./22.locale.time.put.mt --nthreads=4 --nloops=16` | 9a482a4f | 0.1 to 0.3 s | 7 to 15, 5 runs of 5 |
| `./22.locale.time.put.mt --nthreads=4 --nloops=256` | fixed | 0.1 to 0.2 s | 0, 7 runs of 7 |
| `./22.locale.time.put.mt --nthreads=4 --nloops=256` | 9a482a4f | 0.7 to 2.0 s | 13 to 19, 25 runs of 25 |
| `./22.locale.time.put.mt --nthreads=4 --nloops=256` | ae1fbe04 | 0.4 to 0.6 s | 10 to 16, 5 runs of 5 |

### 3.18 `22.locale.use_facet.mt`

One section, gated, 200 rounds. The main thread builds
`de_DE.ISO-8859-1` from the tree's sources first. Each round it
constructs a fresh locale of that name; the threads wait for each
other and call `use_facet` for its wide `codecvt` at once, the first
call, which fills the facet's slot. After the round the locale goes,
and the driver's replacement `operator new` counts the blocks that
outlive it. The leak check is `rw_assert`, and the first failure ends
the run. Building the locale takes most of the second the program
runs, plain or under the sanitizer.

It covers 2bbdf7c9: every thread that found the slot empty took a
reference to the facet, and the body releases the slot once, so the
facet outlived its locale. Since then ae1fbe04 changed the same lines,
so the put-back is by hand and keeps its release store: the slot is
filled without the lock and the extra references are kept. That is a
leak, not a race, and the sanitizer reports nothing; the program's own
leak check fails in the first round of every run, with two threads and
one round already. The program also covers ae1fbe04, one report in
every run at every size.

The rounds are cheap, so the size keeps the default 200 and takes four
threads.

| command | library | time | reports |
|---|---|---|---|
| `./22.locale.use_facet.mt` | plain 15D | 1.0 s | – |
| `./22.locale.use_facet.mt --nthreads=4` | plain 15D | 0.9 s | – |
| `./22.locale.use_facet.mt --nthreads=2 --nrounds=1` | fixed | | 0 |
| `./22.locale.use_facet.mt --nthreads=2 --nrounds=1` | 2bbdf7c9 | | 0; the leak check fails, 5 runs of 5 |
| `./22.locale.use_facet.mt --nthreads=2 --nrounds=1` | ae1fbe04 | | 1, 5 runs of 5 |
| `./22.locale.use_facet.mt --nthreads=4 --nrounds=16` | 2bbdf7c9 | | 0; the leak check fails, 5 runs of 5 |
| `./22.locale.use_facet.mt --nthreads=4 --nrounds=16` | ae1fbe04 | | 1, 5 runs of 5 |
| `./22.locale.use_facet.mt --nthreads=4` | fixed | 1.0 s | 0, 9 runs of 9 |
| `./22.locale.use_facet.mt --nthreads=4` | 2bbdf7c9 | 0.9 s | 0; the leak check fails, 28 runs of 28 |
| `./22.locale.use_facet.mt --nthreads=4` | ae1fbe04 | 1.0 s | 1, 28 runs of 28 |

## 4. Findings

### 4.1 Which program catches which fault

| fault | what it put back | caught by | how |
|---|---|---|---|
| 8c4282ee | wide `codecvt` data published before its size | `22.locale.codecvt.mt` | sanitizer |
| ae1fbe04 | facet slot filled and read with plain accesses | `22.locale.globals.mt`, `.messages.mt`, `.money.get.mt`, `.money.put.mt`, `.moneypunct.mt`, `.num.get.mt`, `.num.put.mt`, `.numpunct.mt`, `.time.get.mt`, `.time.put.mt`, `.use_facet.mt` | sanitizer |
| dcb5dc92 | facet reference counts under the facet's mutex | `22.locale.facet.mt` | sanitizer and the program's assertion |
| bf6dc721 | `ctype` caches with plain accesses | `22.locale.ctype.mt` | sanitizer |
| 46aa42c3 | facet id generated without the lock | `22.locale.id.mt` | sanitizer and the program's gap check |
| 9a482a4f | data built from the C library published with plain stores | `22.locale.money.get.mt`, `.money.put.mt`, `.moneypunct.mt`, `.num.get.mt`, `.num.put.mt`, `.numpunct.mt`, `.time.get.mt`, `.time.put.libc.mt`, `.time.put.mt` | sanitizer |
| 2e196ebb | `numpunct` caches | `22.locale.numpunct.mt` | sanitizer and the program's assertion |
| 81ef680c | `time_put` data built again by every waiting thread | `22.locale.time.put.libc.mt` | the program's leak check only |
| 2bbdf7c9 | extra references of racing first `use_facet` calls | `22.locale.use_facet.mt` | the program's leak check only |
| 5b68c2a7 | global locale initialized by a volatile counter | `22.locale.statics.mt` | sanitizer |
| 4f38276b | classic body compared with plain accesses | none | not reachable, 4.3 |

ae1fbe04 is caught by every program whose iterations call `use_facet`
on a fresh locale, and 9a482a4f by every program whose facets take
their data from the C library. Not put back in this phase, so whether
any program reaches them is open: 4e559c9b (the release store of
`_C_impsize`, the memory model's case with no litmus), c1f10248 and
afda987c (further readers of the facet slot), and three commits of the
sweep of one-time initializations, b1fd85f3 (the standard iostream
objects), 7a0fee0e (the message catalog) and 74bbd8f9 (the static
mutexes).

### 4.2 `22.locale.cons.mt` reaches no fault

The program constructs, copies, combines and compares locales, and
never calls `use_facet` or adds a facet. Every facet slot of the
bodies it builds stays empty, and `__rw_locale::_C_construct` copies
and counts the facets of filled slots only, so neither the reference
counts (dcb5dc92) nor the slot (ae1fbe04) is on its path; both were put
back and it reported neither, at its size or at its defaults. What it
does run is the bodies' construction by name, their cache, and
combination by category, which no fixed fault touched. Its size is
the coverage floor, unchecked against a fault.

### 4.3 `22.locale.statics.mt` races a classic locale already built

The first section was written to race the first construction of the
classic locale. The program defines its array of locales at namespace
scope, and their default construction during static initialization
builds the classic body before `main`. The threads of the first
section copy a classic locale that exists. That is why 4f38276b, whose
store happens once at that construction, is out of reach at any size
(a breakpoint on the store shows the static initialization of the
array). The second section, `locale::global`, catches 5b68c2a7 in
every run.

### 4.4 Two faults are leaks

81ef680c and 2bbdf7c9 leak: the extra builds and references happen in
order, under a lock or by one thread each, and the sanitizer has no
race to report. The only detectors are the programs' own checks, the
blocks that outlive a round counted through the driver's replacement
`operator new`. They fail in the first round of every run, under the
sanitizer as well since the replacement `operator delete` was fixed.
Any rework of these programs must keep the checks.

### 4.5 What the sanitizer does not make deterministic

The sanitizer reports a race between two accesses that ran; it cannot
report one between threads that never met. Where the shared object is
made by the workers themselves, meeting is still timing.
`22.locale.ctype.mt` with two threads missed its `char` section in
about one run in ten at any size; small sizes missed in other programs
too, `22.locale.money.put.mt` at four iterations in one run of five,
`22.locale.numpunct.mt` against 9a482a4f at four iterations of two
threads in two of five. Four threads and room above the floor removed
every miss in the confirmation runs. One iteration reaches only the
first locale, `C`, and for most programs nothing.

### 4.6 Other observations

- `22.locale.time.get.mt` takes 57.7 s plain at its defaults, three
  seconds under the harness's 60-second timeout.
- `22.locale.money.put.mt` drops `C.utf8` at setup: an exception in its
  setup's `try` block, reported by a warning that is inactive unless
  `--locales` is given. Which call throws is not established.
- `22.locale.codecvt.mt` races the first conversion in a fixed 200
  rounds and `22.locale.id.mt` runs a fixed 500 rounds, constants with
  no option; they set those programs' floors.
