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

Nothing measured yet. librwtest's replacement `operator new`, at the
top of `TODO`, comes first.

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

Until librwtest's replacement `operator new` is fixed,
`22.locale.use_facet.mt` and `22.locale.time.put.libc.mt` abort in it
under the sanitizer.

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
| `22.locale.codecvt.mt` | 5000 loops | 8c4282ee | | | | |
| `22.locale.cons.mt` | 20000 loops | – | | | | |
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
