# The std::locale race under ThreadSanitizer

The multithreaded locale tests were the reason for the revival: they
failed intermittently in 2008 and nobody found out why. On the
current toolchain they fail on every run, differently each time, and
a ThreadSanitizer build says why. This note records what the tool
reports and what it means; the fixes wait until after the platform
retirement, so that they land on the surviving code. The 2012
investigation of the same defect, its test programs and why it
stalled are in `analysis-locale-mt-2012.md`.

## 0. tl;dr

- One run of `22.locale.numpunct.mt` in a ThreadSanitizer build of
  the 15D configuration produced 40 reports: 31 data races, 5
  heap-use-after-free, 4 uses of a destroyed mutex. The run ended in
  an uncaught `std::runtime_error`, one of the endings the plain
  build also shows.
- Root cause A: `numpunct` caches the results of its virtuals lazily,
  in members of the facet, with no synchronization. Two threads
  initialize the same cache at once. For the `char` members that is a
  lost flag bit; for the `std::string` members it is two threads
  assigning the same string, one freeing the representation the other
  is copying. Every use-after-free and destroyed-mutex report is this.
- Root cause B, same species: the facet's implementation data and the
  locale body's facet bookkeeping are initialized lazily and read
  without synchronization.
- A configuration fact that shapes the reports: on Linux/x86-64 the
  string reference count goes through a mutex per string, not the
  atomic builtins, because a 2007 clause keeps binary compatibility
  with stdcxx 4.1.x. The revival is not bound by that promise.
- A characterization nobody reads: `_RWSTD_NO_THREAD_SAFE_LOCALE` is
  answered by a config test and consumed nowhere.

Toolchain: GCC 16.2.1, glibc 2.44, x86-64 Linux, 2026-09-15.

## 1. The build

A fresh 15D build directory, configured as usual; the configuration
does not see compiler options and needs none. Then the library, the
test driver, the utilities and the test with the sanitizer on every
compile and link:

```sh
make BUILDDIR=$HOME/build/stdcxx-build/15D-tsan BUILDTYPE=15D config
cd $HOME/build/stdcxx-build/15D-tsan
make -j16 lib    CXXOPTS=-fsanitize=thread LDOPTS=-fsanitize=thread
make -j16 rwtest CXXOPTS=-fsanitize=thread LDOPTS=-fsanitize=thread
make -j16 util   CXXOPTS=-fsanitize=thread LDOPTS=-fsanitize=thread
cd tests && make 22.locale.numpunct.mt CXXOPTS=-fsanitize=thread LDOPTS=-fsanitize=thread
TOPDIR=<source tree> TSAN_OPTIONS="log_path=tsan halt_on_error=0" ./22.locale.numpunct.mt
```

The resulting `config.h` is identical to the plain 15D one, so the
instrumented library is the same library.

The later runs take every `22.locale.*.mt` test the same way, bare,
with `--nthreads=8`, and `--nloops=2000` where the test has the
option. Two of them cannot run under the sanitizer:
`22.locale.use_facet.mt` and `22.locale.time.put.libc.mt` count
blocks through the test driver's replacement `operator new`, whose
guard check aborts ("trailing guard corruption") against the
sanitizer's allocator in every build, before and after the fixes, as
it does under AddressSanitizer; without the sanitizer both pass.

## 2. The reports

Each data race is a pair of accesses to the same memory from two
threads with no ordering between them. Grouped by the two innermost
frames:

| count | one access | the other |
|---|---|---|
| 6 | `numpunct<char>::decimal_point`, `_numpunct.h:142-149` | `decimal_point`, `thousands_sep`, `truename`, `falsename`, same header |
| 5 | `pthread_mutex_lock` on a string's mutex | `string::_C_get_rep`, `string.cc:112`, creating a representation |
| 4 | `__string_ref::_C_get_ref`, `_strref.h:161` | `__rw_atomic_preincrement`/`predecrement` through a mutex, `_atomic-mutex.h:53,63` |
| 3 | `__string_ref::_C_get_ref` | `string::_C_get_rep` |
| 4 | `string::_C_unlink`, `string:902`, and `_C_pref`, `string:846` | `string::_C_unlink`, `string:926`, freeing |
| 1 | `locale::_C_get_std_facet` | `__rw_locale::_C_is_managed`, `locale_body.cpp:1136` |
| 1 | `__rw_facet::_C_manage` | `__rw_locale::_C_get_facet_type`, `locale_body.h:254` |
| 2 | `__rw_facet::_C_data`, `_facet.h:193-194` | `__rw_facet::_C_get_data`, `facet.cpp:248`; `__rw_get_numpunct`, `punct.cpp:228` |
| 3 | `__rw_strlen`, `__rw_punct_t::decimal_point`, `capacity` | `operator new`, `delete`, `pthread_mutex_destroy` |

The five heap-use-after-free reports are all a thread incrementing
the reference count of a string representation, through
`_strref.h:187`, that another thread freed in `string::operator=`
(`string.cc:236`, `string:920`). The four destroyed-mutex reports are
the same representation's mutex.

The full report is kept beside the instrumented build, outside the
tree.

## 3. Cause A: the lazy caches of `numpunct`

`include/loc/_numpunct.h`, from line 142: each accessor tests a bit
in `_C_flags`, and when it is clear calls the virtual, stores the
result in a member of the facet, and sets the bit:

```
    if (!(_C_flags & _RW::__rw_fn)) {
        numpunct* const __self = _RWSTD_CONST_CAST (numpunct*, this);
        __self->_C_falsename  = do_falsename ();
        __self->_C_flags     |= _RW::__rw_fn;
    }
    return _C_falsename;
```

The facet is shared by every thread that uses the locale, which is
the point of the test. Two threads that find the bit clear both
assign `_C_falsename`. `std::string` assignment is not an atomic
operation on the object: it unlinks the old representation, and when
the count drops to zero frees it, while the other thread is still
copying from it or has just taken a reference to it. That is the
use-after-free, and the destroyed mutex is the mutex inside that
representation. The `char` members are less dramatic: the
read-modify-write on `_C_flags` can lose another accessor's bit, so a
virtual is called again, which the comment in the header calls
avoiding "future initializations" and nothing worse.

The facet has a mutex. `__rw_facet` derives from `__rw_synchronized`
(`_facet.h:53`), and the lazy initialization does not take it. No
other facet caches a string. `moneypunct` has no cache: its
accessors forward to the virtuals. `ctype<char>` and
`ctype<wchar_t>` cache `narrow` and `widen` one character per table
slot, and `codecvt<char, char, mbstate_t>` caches `always_noconv` in
an `int`: scalars that every thread writes with the same value and
through which nothing else is published. Those are data races in the
language and harmless on the hardware, and the accesses are now
relaxed atomics, which state them correctly at no cost: the same
plain loads and stores on x86-64, and under ThreadSanitizer no cache
access pairs with another.

Hypothesis for the fix: guard the lazy initialization with the
facet's own mutex, once per accessor, the double-checked shape the
flags already suggest, with the flag written last; or initialize the
cache when the facet is constructed, which the standard's `do_`
virtuals make awkward. The check is a ThreadSanitizer run of the
locale MT tests with no report from the facet headers.

Fixed otherwise: the cache is gone. The accessors call the virtuals,
the forwarding of `moneypunct` and the binary-compatible patch of
2012 (`analysis-locale-mt-2012.md`, chapter 4.3); the members stay, unused,
until the next minor version. `22.locale.numpunct.mt`, which aborted
on every run, passes, and so does `22.locale.num.put.mt`, which reads
`grouping ()` through `num_put`. The cost the cache existed to save
is one virtual call per accessor; chapter 5 of the primer has the
2012 measurements on both sides.

## 4. Cause B: the facet's data and the locale body

`__rw_facet::_C_data` (`_facet.h:193`) tests `_C_impsize` and calls
`_C_get_data` to fill `_C_impdata` and `_C_impsize`
(`facet.cpp:248`) with no synchronization; two more races are in the
locale body's bookkeeping of managed facets
(`locale_body.cpp:1136`, `locale_body.h:254`) against
`_C_get_std_facet` and `_C_manage`. Same species as A, one level
down. Same intended fix; the body has a mutex too.

Fixed otherwise: not with the mutexes but with the order of the stores
and the loads. `_C_impsize` and `_C_impdata` are published with
release stores and read with acquire loads, the data built from the C
library included, and so is the facet pointer in the locale body's
slot; the standard facet's type id, which `_C_manage` wrote on every
construction, is written only when it changes (`analysis-facet-first-use.md`,
chapter 3). With the caches of chapter 3 and the scalar caches of
`ctype` and `codecvt` done as well, one round of the locale MT tests
under ThreadSanitizer gives 6 reports where it gave 1,490 before the
ordering fixes of chapter 7: four in the test driver, one in a
test, and one in the double-checked initialization of the global
locale (`locale_body.cpp:807`), which the sweep of atomic integer
flags in `TODO` covers.

## 5. The configuration facts

`include/rw/_config.h:148-158`: on Linux/x86-64, unless
`_RWSTD_USE_STRING_ATOMIC_OPS` is defined, `_RWSTD_NO_STRING_ATOMIC_OPS`
is, "for binary compatibility with stdcxx 4.1.x" (2007-10-18). The
effect is that `std::string` counts references under a mutex per
string through `_atomic-mutex.h`, although `_RWSTD_NO_ATOMIC_OPS` is
not set and the `__sync` builtins are available and used everywhere
else. It is not a race, but it is why mutexes appear inside string
representations in the reports, and it costs every string a mutex.
The revival does not ship binaries compatible with 4.1.x; the clause
is a decision to make, after the retirement, with the string tests as
the gate.

`etc/config/src/THREAD_SAFE_LOCALE.cpp` determines whether each
thread has its own C locale environment or the process shares one;
`config.h` records the answer as `_RWSTD_NO_THREAD_SAFE_LOCALE`, and
no file in the tree reads the macro. Either something was meant to
consume it and never did, or the test outlived its consumer; the
history will say which.

## 6. Where this leaves the MT tests

The plain 15D results for `22.locale.*.mt` are symptoms of A and B
and stay out of the reference until both are fixed. The
ThreadSanitizer build is the instrument from here: rebuild it after
the retirement, run the locale MT tests, and expect the report to
name exactly the sites in chapters 3 and 4. Weakly ordered hardware
is still wanted afterwards, for the ordering questions a fix on x86
cannot answer, but it is no longer needed to see the defect.

## 7. What was fixed

2026-09-26 and 27. Each row is one cause, its commit and its evidence.

| cause | fix | commit | evidence |
|---|---|---|---|
| A: `numpunct` caches | call the virtuals; the members stay, unused, until the next minor version | 2e196ebb | `22.locale.numpunct.mt` |
| B: wide `codecvt` data | store the data before its size | 8c4282ee | `22.locale.codecvt.mt` |
| racing first `use_facet` | return the extra references | 2bbdf7c9 | `22.locale.use_facet.mt` |
| facet data publication | release store of `_C_impsize`, acquire loads | 4e559c9b | memory model; no reordering in an aarch64 litmus |
| `time_put` data built twice | re-check under `__rw_setlocale` | 81ef680c | `22.locale.time.put.libc.mt` |
| `ctype` and `codecvt` scalar caches | relaxed atomic loads and stores | bf6dc721 | none possible |
| data built from the C library | release stores | 9a482a4f | ThreadSanitizer |
| facet slot in the locale body | release store; acquire reads | ae1fbe04, c1f10248, afda987c | ThreadSanitizer |
| user-defined facet id | generated once, under a lock | 46aa42c3 | `22.locale.id.mt` |
| facet reference count | one atomic operation everywhere | dcb5dc92 | `22.locale.facet.mt` |

One round of the locale MT tests under ThreadSanitizer gives six
reports: four in the test driver, one in a test, and the global
locale's `ginit` flag. The 15D rows of the older locale MT tests now
measure the harness only (`ref-test-baseline.md`, chapter 3.4).
