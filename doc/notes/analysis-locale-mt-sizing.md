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
| `22.locale.ctype.mt` | 200000 loops | bf6dc721 | 25.6 s | `./22.locale.ctype.mt --nthreads=4 --nloops=256` | 0.4 s | none; 4 to 6 per run against the fault, 50 runs of 50 |
| `22.locale.facet.mt` | at most 8 threads, 20 rounds, 1000 loops | dcb5dc92 | 0.06 s | `./22.locale.facet.mt --nthreads=4 --nrounds=4 --nloops=256` | 0.02 s | none; 2 per run against the fault, 25 runs of 25 |
| `22.locale.globals.mt` | 20000 loops | ae1fbe04 | 6.0 s | `./22.locale.globals.mt --nthreads=4 --nloops=256` | 0.3 s | none; 332 to 396 per run against the fault, 25 runs of 25 |
| `22.locale.id.mt` | at most 8 threads, 500 rounds (fixed) | 46aa42c3 | 0.10 s | `./22.locale.id.mt --nthreads=4` | 0.2 s | none; 2 to 5 per run against the fault, 30 runs of 30 |
| `22.locale.messages.mt` | 10000 loops, 11 catalogs | ae1fbe04 | 6.2 s | `./22.locale.messages.mt --nthreads=4 --nloops=256` | 0.2 s | none; 9 to 31 per run against the fault, 25 runs of 25 |
| `22.locale.money.get.mt` | 100000 loops | 9a482a4f, ae1fbe04 | 17.7 s | `./22.locale.money.get.mt --nthreads=4 --nloops=256` | 0.1 s | none; 57 to 95 per run against 9a482a4f, 25 runs of 25 |
| `22.locale.money.put.mt` | 100000 loops | 9a482a4f, ae1fbe04 | 22.8 s | `./22.locale.money.put.mt --nthreads=4 --nloops=256` | 0.1 s | none; 14 to 21 per run against 9a482a4f, 25 runs of 25 |
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
