# One-time initialization and its flags

Some one-time initializations in the library elected their
initializer with an atomic increment and announced completion with a
plain write to a `volatile` counter. `volatile` orders neither the
counter against the data it guards nor the waiting loop against the
reads after it. On x86 the hardware keeps stores in order, so the
shape worked there by accident; on aarch64 a waiter could see the
counter before the data. This note is the census of every site that
synchronized on such a flag, or read a flag outside the lock that
guards its writes, with the decision taken for each. Line numbers
are those of ee454d5a, before the changes.

## 0. tl;dr

- **Six sites, four reachable on the matrix.** The matrix is GCC and
  Clang on x86, x86-64 and aarch64 Linux, with POSIX threads and the
  atomic backend over the `__atomic` built-ins.
- **One protocol for all of them.** `__rw_once` took the shape of
  Boost's atomic `call_once`: a zero-initialized flag, one mutex and
  one condition variable for every flag, waiters that block, and an
  initializer that throws leaves the flag ready for the next caller.
  The global locale, the standard iostream objects, the message
  catalog and the static mutexes initialize through it.
  `ref-threading.md` chapter 5 describes it.
- **One site needed no protocol.** The unlocked comparison with the
  classic locale body cannot give a wrong answer; it takes relaxed
  accesses.
- **`pthread_once` is gone.** `__rw_once` called it on every
  configuration, and POSIX does not say what an exception thrown
  through it does. The two fallbacks for systems without it went
  with it.

## 1. Method

The census reads every use of `volatile` in `include/` and `src/`,
every call of the atomic macros and functions, every lock site, and
every mutable static with static storage duration, function-local or
not. A site counts when a thread decides, without the lock that
guards the writes, that data written by another thread is ready.

The configuration facts come from the six standing build directories
(`11s`, `11S`, `15D` and their Clang twins) and `15D-tsan`:
`_RWSTD_NO_ATOMIC_OPS`, `_RWSTD_NO_INT_ATOMIC_OPS`,
`_RWSTD_NO_STATIC_MUTEX_INIT` and `_RWSTD_NO_TLS` are all undefined.
`_RWSTD_REENTRANT` implies `_RWSTD_POSIX_THREADS`
(`include/rw/_config.h:147`). The backend is
`include/rw/_atomic-builtins.h`: sequentially consistent
read-modify-writes, and loads and stores in the order their names
give.

## 2. The sites

### 2.1 The global locale, `src/locale_body.cpp:807-830`

`__rw_locale::_C_manage (0, 0)` returns the global locale. The first
caller built it:

- 807 tested `global` with no lock. 841 writes it under the
  `__rw_locale` static guard, in `locale::global`. ThreadSanitizer
  reported this pair in `22.locale.statics.mt`.
- 815 elected the builder: the first thread to increment `ginit` from
  0 to 1.
- 817 stored `global` with no lock and no release.
- 818 added 1000 to `ginit` with a plain read-modify-write.
- 823 was where the other threads waited for `ginit` to reach 1000,
  with no acquire.
- 830 reads `global` under the guard. The builder never took the guard
  after its store, so the lock ordered nothing here. On aarch64 a
  waiter could read 0, and 832 asserts.

The election exists because the builder's call, `_C_manage (0, "C")`,
takes the same static guard, and the mutex is not recursive. An
exception from the builder left `ginit` at 1, and every later caller
spun forever.

Decision: `__rw_once`, before the guard as the election was
(5b68c2a7). The initializer is a static member of a local class,
which may name the function's static `global` and the private
`_C_manage`.

### 2.2 The standard streams, `src/iostream.cpp:247`

`ios_base::Init::Init` incremented `__rw_ios_initcnt`. The thread
that got 1 constructed the eight standard streams; every other thread
returned at once, without waiting. A second thread that constructed
an `Init` while the first was still at work could use `cout` before
it existed. The destructor's decrement at 323 is a count, and its
last caller flushes; that part is sound.

The suite cannot race the first `Init`: the test driver includes
`<iostream>`, so every test program constructs the streams before
`main`. A probe program without the driver, linked as the suite
links, races the first `Init` from eight threads. At ee454d5a seven
of the eight return before the streams exist, in each of 20 runs.

Decision: count every `Init` as before, and construct the streams
through `__rw_once` in a private static member, `Init::_C_init`
(b1fd85f3). 27.4.2.1.6 gives `Init` the same effect: the first one
constructs the objects, and `init_cnt` never falls back to zero to
construct them again. The probe reports no early return in 200 runs.

### 2.3 The error-message catalog, `src/exception.cpp:519-555`

`__rw_vfmtwhat` opened the library's message catalog once. 519 tested
`__fname` with no lock; 524 tested it again under the guard. 550
stored `__cat`, then 551 stored `__fname`, both under the guard.
Nothing ordered the two stores for a thread that read `__fname` at
519 without the lock. That thread then read `__cat` at 555, also
without the lock, and could see the initial -1. The message then fell
back to the built-in text.

No suite test formats a catalog message on two threads at once. A
probe program throws `out_of_range` from eight threads; under
ThreadSanitizer at ad06d0e4 it reports the race between 551 and 519
in each of five runs.

Decision: `__rw_once`, through a local class that keeps the
function's statics (7a0fee0e). The guard at 524 and `__fname` go.

### 2.4 The classic locale's body, `src/locale_body.cpp:856`

`_C_manage` compares `plocale` with `classic` before it takes the
guard at 874. 1025 writes `classic` once, under the guard. A thread
holding the classic body got it through the guard and sees the
pointer. A thread holding another body may race the one write, but
reads either 0 or the classic pointer, and neither equals its own.
The answer cannot be wrong.

Decision: a relaxed load at 856 and a relaxed store at 1025
(4f38276b). This is the cache rule of `ref-threading.md` chapter 4:
no data is published through the pointer.

### 2.5 The static mutex, `include/rw/_mutex.h:286`

`__rw_get_static_mutex` built a mutex in place in the shape of 2.1:
an atomic election at 286, the placement construction, a plain
`__cntr += 1000` at 299, and a spin at 303 with no acquire. Without
integer atomics the election itself was a plain `++__cntr`.

The branch is compiled when `_RWSTD_NO_STATIC_MUTEX_INIT` is defined.
`include/rw/_config.h:170` defines it when the configuration finds
`_RWSTD_NO_COLLAPSE_TEMPLATE_STATICS` or
`_RWSTD_NO_STATIC_TEMPLATE_MEMBER_INIT`; GCC and Clang have neither.
On the matrix the static mutexes are PODs initialized at static
initialization.

Decision: `__rw_once`, through a local class (74bbd8f9). Where `int`
is not lock-free, the ordered accesses of `__rw_once` to its flag
would lock the static mutex of `int`, which this branch constructs
through `__rw_once`: an unbounded recursion. There `__rw_once` has no
unlocked fast path, and every access to a flag holds its own mutex, a
POSIX mutex initialized statically.

### 2.6 The `__rw_once` fallbacks, `src/once.cpp:74` and `:125`

`src/once.h` mapped `__rw_once` to `pthread_once` unless
`_RWSTD_NO_PTHREAD_ONCE` was defined. No characterization defined it;
only a hand define did. With it, two fallbacks compiled:

- Without atomics, 74 tested `_C_init` with no lock, and 93 wrote it
  under the mutex. This was plain double-checked locking.
- With atomics, 125 elected through an increment, 140 announced with a
  plain `init = 1000` through a `volatile` reference, and the waiters
  polled at 147 with no acquire. 148 restarted the election when the
  initializer threw.

Decision: `__rw_once` rebuilt in the shape of Boost's atomic
`call_once`, without `pthread_once`; `_RWSTD_NO_PTHREAD_ONCE` and both
fallbacks gone (ad06d0e4). Its header moved to
`include/rw/_once.h`, where the static mutex and the tests reach it.
`tests/support/once.cpp` races eight threads at a fresh flag in each
of 200 rounds; in every other round the initializer throws once
(0fa1c07d).

### 2.7 Not a site: the locale root, `src/setlocale.cpp:143`

`__rw_locale_name` caches the `RWSTD_LOCALE_ROOT` environment
variable in a static buffer. 147 tests the first byte, 156 copies it
in, and there is no flag and no lock. All three-argument calls come
from `__rw_expand_name` and the `__rw_locale (const char*)`
constructor, and both run only inside `_C_manage`, under the
`__rw_locale` static guard. The calls at 408 and 476 are inside
`#if 0`. The guard serializes the cache, but nothing at the site says
so.

## 3. Uses that are not sites

These synchronize nothing beyond the integer itself, or were repaired
before this census:

- Reference counts: facets (`src/locale_body.h:161-173`), locale bodies
  (`src/locale_core.cpp`, `src/locale_combine.cpp`,
  `src/locale_body.cpp`), strings (`include/rw/_strref.h:187`, `:201`),
  the TR1 `shared_ptr` (`include/tr1/_smartptr.h`). The increments and
  decrements are sequentially consistent.
- `ios_base::xalloc` (`src/iostore.cpp:151`): the count is the
  result.
- `precision`, `flags` and `sync_with_stdio` (`src/ios.cpp:92`,
  `:101`, `:122`): the exchanged value is the data.
- The temporary buffer and the `what` buffer (`src/tmpbuf.cpp`,
  `src/exception.cpp:376-416`): thread-local with `_RWSTD_NO_TLS`
  undefined, so their increments are plain and per thread.
- `__rw_facet::_C_opts` (`src/facet.cpp:89`): written only at static
  initialization.
- The facet data, the facet slots, the facet ids and the scalar
  caches: release, acquire and relaxed since the 2026-09-27 repairs
  (`ref-threading.md` chapter 4).

## 4. The check

ThreadSanitizer, 15D built with `-fsanitize=thread`, three rounds of
`once` and the locale MT tests with eight threads (`use_facet.mt` and
`time.put.libc.mt` abort under the sanitizer's allocator in every
build and are left out):

| tree | reports from the library | reports from the harness |
|---|---|---|
| ad06d0e4, `__rw_once` rebuilt | `statics.mt`, 2.1, once per round | `time.get.mt`, its timeout flag |
| 74bbd8f9, every site on `__rw_once` | none | `time.get.mt`, its timeout flag |

The harness reports are the soft-timeout flag of `rw_thread_pool`,
read by the threads and written by the timer
(`tests/src/thread.cpp:63` and `:73`). The catalog probe of 2.3
reports nothing in 20 runs at 74bbd8f9. The `ios_base::Init` probe of
2.2 cannot run under the sanitizer: its runtime loads the system's
C++ library, whose own `Init` constructs the streams before `main`.

The static mutexes were exercised by a test-only edit of `config.h`
in a scratch 15D build, defining `_RWSTD_NO_STATIC_MUTEX_INIT`, and
in a second one also `_RWSTD_NO_INT_ATOMIC_OPS`. `once`, the locale
MT tests, `27.objects` and `19.exceptions.mt` pass in both. Before
`__rw_once` tested `_RWSTD_NO_ATOMIC_OPS` instead of
`_RWSTD_NO_INT_ATOMIC_OPS`, which `<rw/_mutex.h>` undefines, every
program of the second build overflowed its stack in the recursion
2.5 describes. The first build passes the same three rounds under
ThreadSanitizer with no report from the library.

11S, 11s and 15D under GCC match their pins at 74bbd8f9. At ad06d0e4,
with the `once` test, all six matched them but for one timeout of
`22.locale.money.get.mt` in 15D-clang on a loaded machine
(`ref-test-baseline.md` chapter 2).
