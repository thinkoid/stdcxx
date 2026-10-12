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

Nothing measured yet. librwtest runs clean under the sanitizer:
the soft timeout and the replacement `operator delete` are fixed.

## 1. Method

No source changes in this phase. Each program is run through its own
options: `--nthreads`, `--nloops`, `--nrounds`, `--nlocales`,
`--ncatalogs`.

The build is the one in `analysis-locale-mt-race.md`, chapter 1: a 15D
build directory with `-fsanitize=thread` on the library, librwtest,
the utilities and the tests. The configuration does not see the
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

- A plain 15D run at its defaults, timed.
- Runs under the sanitizer at decreasing sizes. At each size, one run
  against the fixed library and runs against a library with the
  program's fault put back. A fault is put back by reverting its commit
  in a scratch build; `analysis-locale-mt-race.md`, chapter 7, lists
  them.
- The chosen size is the smallest at which every run against the fault
  reports it and the run against the fixed library reports nothing.
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
| `22.locale.codecvt.mt` | 5000 loops | 8c4282ee | 1.7 s | `./22.locale.codecvt.mt --nthreads=2 --nloops=28` | 1.1 s | none; 5 per run against the fault, 25 runs of 25 |
| `22.locale.cons.mt` | 20000 loops | – | 6.5 s | `./22.locale.cons.mt --nthreads=2 --nloops=8` | 0.02 s | none; none against dcb5dc92 or ae1fbe04 |
| `22.locale.ctype.mt` | 200000 loops | – | | | | |
| `22.locale.facet.mt` | at most 8 threads, 20 rounds, 1000 loops | dcb5dc92 | | | | |
| `22.locale.globals.mt` | 20000 loops | – | | | | |
| `22.locale.id.mt` | at most 8 threads, 500 rounds (fixed) | 46aa42c3 | | | | |
| `22.locale.messages.mt` | 10000 loops, 11 catalogs | – | | | | |
| `22.locale.money.get.mt` | 100000 loops | – | | | | |
| `22.locale.money.put.mt` | 100000 loops | – | | | | |
| `22.locale.moneypunct.mt` | 10000 loops | – | | | | |
| `22.locale.num.get.mt` | 100000 loops | – | | | | |
| `22.locale.num.put.mt` | 100000 loops | – | | | | |
| `22.locale.numpunct.mt` | 200000 loops | 2e196ebb | | | | |
| `22.locale.statics.mt` | 20000 loops | – | | | | |
| `22.locale.time.get.mt` | 50000 loops | – | | | | |
| `22.locale.time.put.libc.mt` | 200 rounds | 81ef680c | | | | |
| `22.locale.time.put.mt` | 100000 loops | – | | | | |
| `22.locale.use_facet.mt` | 200 rounds | 2bbdf7c9 | | | | |

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
every run. The 200 rounds set the floor of about a second.

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

The size is the coverage floor. The sanitizer reports nothing at any
size, against either fault or the fixed library.

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
