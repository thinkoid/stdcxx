# The 2012 investigation of the numpunct race

In 2012 the project spent two months on the thread safety of
`numpunct` and `moneypunct`, on the dev list and in two JIRA issues,
and closed nothing. This note is the primer for picking the work up:
what was reported, what was established, what was disputed, what the
test programs were and where they live, and why no fix landed. The
current evidence is in `analysis-locale-mt-race.md`; this note is its
prehistory.

## 0. tl;dr

- February 2012, STDCXX-1056: a report that `numpunct` and
  `moneypunct` are not thread safe, with a patch that serialized every
  accessor on a lock per specialization. The reviewers asked for a
  failing test and a performance measure before any change to the
  inline accessors.
- September 2012 on the dev list, the case was argued from the
  output of four thread analyzers (Solaris Studio's Thread Analyzer
  among them) on Solaris/SPARC and Linux/x86. The reviewers accepted
  a defect in the lazy cache, rejected analyzer output as proof on
  its own, and rejected the parts of the patch that changed the
  locale and facet caches, as unrelated to the race. The exchange
  broke down; the reporter closed the issue as "will not be fixed".
- Established, and still true in the tree today:
  1. **The numpunct cache is broken.** The accessors in
     `include/loc/_numpunct.h` store the result of each virtual in a
     member of the shared facet and set a flag bit, with no lock and
     no ordering. Reduced to a standalone program that crashed
     on every run on a 16-core x86-64 box unless one call filled the
     cache before the threads started. STDCXX-1071 was opened for
     it, with two patches that remove the cache.
  2. **The facet data is published by double-checked locking without
     barriers.** `__rw_facet::_C_get_data` (`src/facet.cpp`) writes
     `_C_impdata` and then `_C_impsize` under the facet mutex;
     `_C_data` (`include/loc/_facet.h`) reads `_C_impsize` and then
     `_C_impdata` with no lock. Nothing orders either pair. Agreed to
     be a defect, never shown to fail: the only hardware at hand was
     x86-64, which does not reorder those stores or those loads.
- Not settled: whether the cache is worth keeping. One set of
  measurements showed the uncached accessors faster under
  contention, another showed the cache 2x to 8x faster. The
  difference was traced as far as a mutex per string on Linux/x86-64
  and no further.
- Why nothing landed: removing the cache members breaks the 4.x
  binary interface; publishing the facet data correctly needs
  barriers the library had no portable way to express before C++11;
  and the discussion stalled at the end of October 2012. No commit on
  any branch touches the cache after 2008.

## 1. Sources

### 1.1. JIRA

| issue | title | state |
|---|---|---|
| [STDCXX-839](https://issues.apache.org/jira/browse/STDCXX-839) | SIGHUP in 22.locale.numpunct.mt | Open since 2008-04-08 |
| [STDCXX-1056](https://issues.apache.org/jira/browse/STDCXX-1056) | std::moneypunct and std::numpunct implementations are not thread-safe | Closed (Duplicate), 2012-09-28 |
| [STDCXX-1071](https://issues.apache.org/jira/browse/STDCXX-1071) | numpunct facet cache initialization is not thread-safe | Open since 2012-09-26 |

STDCXX-839 is the symptom as the nightly builds saw it in 2008: the
test timed out on many platforms and was read as slow, not as
broken. STDCXX-1056 was closed in favour of STDCXX-1071, which
narrows the scope to `numpunct` because `moneypunct` never failed in
the reduced tests.

### 1.2. Dev list threads

All messages are in the list archive at
<https://lists.apache.org/list.html?dev@stdcxx.apache.org>, months
2012-08 to 2012-10. Each link below opens the message in its
thread; dates are UTC, as the archive files them.

| date | message | what it carries |
|---|---|---|
| 2012-08-31 | [STDCXX forks](https://lists.apache.org/thread/s10ckh1zoptzf2sympqsrjo5yrysjltm) | the thread the discussion grew out of |
| 2012-09-03 | [STDCXX-1056](https://lists.apache.org/thread/cc2tx4vymnv0fx44ljpjg0dbchvdltkc) | the reopening: moneypunct.mt does not reproduce on a 16-core Opteron |
| 2012-09-05 | [reply](https://lists.apache.org/thread/h8cw67ls6ln5qgqd7hfo9hvccx718rnv) | the cache is not thread safe; the per-object lock proposal |
| 2012-09-05 | [reply](https://lists.apache.org/thread/yt42f736ttzcpjw12hzdtt050c9cbp8z) | "the race is benign": same value, same bytes |
| 2012-09-05 | [reply](https://lists.apache.org/thread/b6b44x2qh8fo7smz7x55lh9wvj9r68qq) | double-checked locking named; barriers needed on both paths |
| 2012-09-05 | [reply](https://lists.apache.org/thread/48c5cvyzmtl42h12pc7xnjq096oy1vnn) | why the locale is lazy: no static initialization, iostreams in low memory |
| 2012-09-06 | [reply](https://lists.apache.org/thread/ycdpfo75kr6t7w2l0v8zxwwrlhn43p2c) | first reproduction (SunPro 12.3, x86-64) and the constructor-initialization patch |
| 2012-09-10 | [reply](https://lists.apache.org/thread/26fmzdqzfhbk0p3zfwyfvn7fq3x6p4p7) | four timed variants of `grouping()`, and the program |
| 2012-09-12 | [reply](https://lists.apache.org/thread/o2r9n6st0n90tv51bbh74pcr90mtzt9l) | ABI: data members cannot change before 5.0; the `access.h` route |
| 2012-09-17 | [reply](https://lists.apache.org/thread/5dtx4bkv9pkgdjjvm5ox43z7g7rs5ypz) | why the facet data initialization is locked: the two stack traces |
| 2012-09-17 | [reply](https://lists.apache.org/thread/0xx2k2kyjbb4sc0foo3bxrjj01zvxqqc) | the `_C_impsize`/`_C_impdata` ordering problem, first statement |
| 2012-09-18 | [reply](https://lists.apache.org/thread/0c6htltglbtgrbnmrbd6vvto4h7b6m1g) | a correct program the Thread Analyzer flags (exhibit E) |
| 2012-09-18 | [reply](https://lists.apache.org/thread/zh45b7o3bcsqv58znlbfppvtgpjb05pj) | an eight-point summary of the state |
| 2012-09-19 | [STDCXX-1056 : numpunct fix](https://lists.apache.org/thread/rnq1vzfhvj3sz7qdhcx8q6r0xgrz8j4k) | the analyzer-driven patch: locking plus cache changes |
| 2012-09-20 | [reply](https://lists.apache.org/thread/fp1bnbnf33mogoxwnfyft95wk49o05yf) | the objection: out of scope, no failing test for the cache changes |
| 2012-09-20 | [reply](https://lists.apache.org/thread/n2d13f8jzssy285b50fy6o4t3fw4wjqo) | two analyzer reports that look like false positives |
| 2012-09-20 | [reply](https://lists.apache.org/thread/48t192yf07o6jv9p567lnhhc0jy1sf96) | "the only gold currency ... are failing test cases" |
| 2012-09-20 | [reply](https://lists.apache.org/thread/w7slvhzszrmdys4tw22w0twvx1jgk3s3) | what counts as a failing test case |
| 2012-09-22 | [STDCXX-1056 DCII](https://lists.apache.org/thread/p2d8fbd5ywp71mql79xcxxnywcgtgxwj) | the facet data double-checked locking in detail, and `facet.cpp:288` |
| 2012-09-25 | [reply](https://lists.apache.org/thread/b2okmtfxmglvbv825nb7r7196djq8hdz) | benign races by design; the rest must be fixed |
| 2012-09-26 | [reply](https://lists.apache.org/thread/7b175q68tyzmn99nyoso1q8p4r0rk0rw) | the three patches timed: forwarding, analyzer patch, lock-everything |
| 2012-09-26 | [reply](https://lists.apache.org/thread/c0zst2k4rdolp1dk1mp4ps2bqsnstt0b) | the review position on the patch: the smallest change that fixes the bug |
| 2012-09-27 | [STDCXX-1071 numpunct facet defect](https://lists.apache.org/thread/34x4hp3llo2fvq0071bkfr15c6q2wxkx) | the narrowed issue and its reduction |
| 2012-09-30 | [Fwd](https://lists.apache.org/thread/41rgt7ms8wvczxhy7bcoxv2hhbbb43md) | SPARC and Linux timings of the reduction: the cache is faster |
| 2012-09-30 | [reply](https://lists.apache.org/thread/7vsxoyn2s9smm5qphqk3sko4nr794gsh) | x86 timings of the same program: the cache is slower |
| 2012-10-02 | [reply](https://lists.apache.org/thread/qg8dd5wznzqcswk2x40s3mgzlyjwxh11) | the per-string mutex on Linux/x86-64 found in the disassembly |
| 2012-10-21 | [reply](https://lists.apache.org/thread/5ww1bj4n18f9z0r2vgvvxz6vvxdm3s5t) | `perf` profiles: kernel futex contention in the cached version |
| 2012-10-26 | [reply](https://lists.apache.org/thread/g50xmfj98zh69f2odzp00lrf174qnndm) | "removing the facet data cache is a priority"; the barriers the facet data needs |
| 2012-10-26 | [reply](https://lists.apache.org/thread/fr5v42hfq1xg81fh8v3fm83d7g2h08v9) | agreed; move the locking out of line into the library |
| 2012-10-27 | [reply](https://lists.apache.org/thread/3c0zro1ly58jqm4xv0bwh5y79cnyln05) | the last message: only x86 hardware to test the ordering on |

The list's `mbox` interface returns a month at a time:
`https://lists.apache.org/api/mbox.lua?list=dev&domain=stdcxx.apache.org&d=2012-09`.

## 2. The code under discussion

Both defects are in the tree unchanged: no line mentioning
`_C_flags` in `_numpunct.h` has changed since 2005 (`git log -S`).

### 2.1. The numpunct cache

`include/loc/_numpunct.h`, five accessors of the same shape:

```
template <class _CharT>
inline string numpunct<_CharT>::grouping () const
{
    if (!(_C_flags & _RW::__rw_gr)) {

        numpunct* const __self = _RWSTD_CONST_CAST (numpunct*, this);

        // [try to] get the grouping first (may throw)
        // then set a flag to avoid future initializations
        __self->_C_grouping  = do_grouping ();
        __self->_C_flags    |= _RW::__rw_gr;
    }

    return _C_grouping;
}
```

Every thread that finds the bit clear assigns `_C_grouping`. The
2012 premise was that this is harmless because every thread assigns
the same value, byte for byte. That premise does not hold for a
reference-counted string: assignment releases the old body and may
free it while another thread copies from it or takes a reference to
it. The flag update is a read-modify-write that can also lose another
accessor's bit, and nothing stops a reader from seeing the bit before
the string.

`moneypunct` has the same pattern and never failed in the reduced
tests; the 2012 scope was cut to `numpunct` for that reason.

### 2.2. The facet data

`__rw_facet::_C_data` (`include/loc/_facet.h`) is the fast path:

```
    const void* _C_data () const {
        return _C_impsize ? _C_impdata
            : _RWSTD_CONST_CAST (__rw_facet*, this)->_C_get_data ();
    }
```

`__rw_facet::_C_get_data` (`src/facet.cpp`) checks again under
`_C_mutex` and then stores `_C_impdata` followed by `_C_impsize`.
Three problems were identified:

1. The two stores have no store-store ordering. The unlock that
   follows releases them together, but the fast path does not
   acquire: it reads `_C_impsize` with no lock.
2. The two loads in `_C_data` have no load-load ordering. A thread
   may see `_C_impsize` set and a stale `_C_impdata`.
3. The `codecvt_byname` path ended with
   `_C_impdata = __rw_get_facet_data (cat, _C_impsize, 0, codeset);`.
   `_C_impsize` is an output parameter written inside the call,
   before `_C_impdata` is assigned the return value, so the flag was
   published before the data even in program order. This one fails
   on any hardware; it is fixed, with a test, and
   `analysis-facet-first-use.md` has the account.

The 2012 conclusion: a store-store barrier between the writes and a
load-load barrier between the reads are needed, and the library has
no API for either. The alternative, taking the lock on every read,
was measured and dismissed as "a whooping performance hit".

The slow path that fills in the `numpunct` data, `__rw_get_numpunct`
(`src/punct.cpp`), was shown by stack traces to run either under the
facet mutex (the library's own locale database) or under the
`__rw_setlocale` lock (the C library locales). The analyzers
reported ~3400 racy accesses in it; the 2012 reading was that the
analyzers do not follow a lock held across the recursive call.

## 3. The positions

**The report.** Four analyzers, on three systems, in 32 and 64 bits,
agree on the same sites; that agreement is the evidence. The
proposed patch removes every reported race. It also gave the facet
and locale caches (`facet.cpp`, `locale_body.cpp`) a larger initial
size and made them never shrink or evict, on the argument that resizing was
overhead and that it was involved in the races.

**The review.** A race reported by a tool is a potential defect. A
defect is shown by a program that fails: one that crashes, or that
observes a wrong value. The cache changes are unrelated to the race
and belong in their own issue with their own evidence. Two of the
analyzer's reports look like false positives: one in
`__rw_facet::_C_manage`, on data only ever touched under that
function's lock, one on a `memset` of memory `__rw_allocate` has
just obtained; and exhibit E is correct under the hardware models of the day yet
flagged. The fix must be the smallest change that removes the
failure, binary compatible for 4.2.x, and must not cost the common
case (the facet already initialized) a lock.

**Where they agreed.** The cache is broken. The facet data
initialization is subject to reordering. Something must be fixed.

## 4. The exhibits

The programs are attached to the JIRA issues and to list messages.
They are kept in the local archive of the list, not in this tree.

### 4.1. Failing programs

| exhibit | source | what it shows |
|---|---|---|
| A. `library-reduction.cpp` | [STDCXX-1071](https://issues.apache.org/jira/secure/attachment/12546780/library-reduction.cpp) | the facet, its data and the numpunct cache reduced to 180 lines of standalone code, no stdcxx needed. `-DNO_USE_NUMPUNCT_CACHE` forwards instead of caching; `-DNO_USE_STDCXX_LOCALES` takes the path that leaves `_C_impdata` null. With the cache, 16 threads and 10 million loops: "double free or corruption" under SunPro 5.12 `-O -mt` |
| B. `punct-mt.cpp` | [STDCXX-1071](https://issues.apache.org/jira/secure/attachment/12546781/punct-mt.cpp) | the same against the library: one `numpunct<char>` (or with `-DTEST_MONEYPUNCT` one `moneypunct<char>`) shared by N threads calling `grouping ()` (`negative_sign ()`) in a loop. Fails with 4.2.x for numpunct under SunPro 5.12 and GCC 4.7.1; the moneypunct variant did not fail |
| C. `t.cpp`, list version | [2012-09-06](https://lists.apache.org/thread/ycdpfo75kr6t7w2l0v8zxwwrlhn43p2c) | the first reproduction: 16 threads, 1000 calls each, on a facet whose cache no thread has filled. Passed once the cache was removed. The later timing programs call `grouping ()` once in `main` before starting the threads, because otherwise they crashed; that one call is the whole defect in one line |
| D. `t.cpp`, modified reduction | [2012-09-30](https://lists.apache.org/thread/41rgt7ms8wvczxhy7bcoxv2hhbbb43md) | exhibit A reshaped closer to the library, with a virtual `do_grouping` called from the inline accessor; the program behind the SPARC and CEL 4 timings |

Exhibit A initializes neither `_C_impsize` nor `_C_impdata`, and
`main` constructs its facet on the stack. On current toolchains the
program exits with status 2 (a thread saw an empty `grouping`) in
the cached and the uncached build, one thread included, on aarch64
with GCC 16.2;
it also needs `-fpermissive` for a string literal assigned to
`void*`. Before it can reproduce the race again the members have to
start at zero.

### 4.2. The program an analyzer flags

Exhibit E, `mt.cpp`
([2012-09-18](https://lists.apache.org/thread/0c6htltglbtgrbnmrbd6vvto4h7b6m1g)):
N threads each run a loop of

```
        if (counter == 0) {
            pthread_mutex_lock (&lock);
            if (counter == 0)
                ++counter;
            pthread_mutex_unlock (&lock);
        }
```

`counter` goes from 0 to 1 once, under the lock, and a reader either
sees 0 and takes the lock or sees 1 and uses nothing else. The
Thread Analyzer reported a race. The program was the argument that
analyzer output needs interpretation.

### 4.3. Timings and patches

| item | source |
|---|---|
| non-caching `grouping ()`, `t.cpp` (MT) and `s.cpp` (ST), results | [stdcxx-1056-timings.tgz](https://issues.apache.org/jira/secure/attachment/12544750/stdcxx-1056-timings.tgz) |
| a second round, with method | [STDCXX-1056-additional-timings.tgz](https://issues.apache.org/jira/secure/attachment/12545330/STDCXX-1056-additional-timings.tgz) |
| the timings re-verified, with the patch | [patch-timings.tgz](https://issues.apache.org/jira/secure/attachment/12546851/patch-timings.tgz) |
| remove the cache, binary compatible (members kept, unused) | [patch-4.2.x.diff](https://issues.apache.org/jira/secure/attachment/12546782/patch-4.2.x.diff) |
| remove the cache and its members | [patch-5.0.x.diff](https://issues.apache.org/jira/secure/attachment/12547066/patch-5.0.x.diff) |
| `x.cpp`, `u.cpp`: inline, virtual and locked loops; the string reduced | [2012-10-05](https://lists.apache.org/thread/c8c5mlt6516b40947bsc1m17vccdqdso) |
| `perf stat` and `perf report`, cached and uncached | [2012-10-21](https://lists.apache.org/thread/5ww1bj4n18f9z0r2vgvvxz6vvxdm3s5t) |
| the February patch: a lock per specialization and deep copies | [stdcxx-1056.patch](https://issues.apache.org/jira/secure/attachment/12513468/stdcxx-1056.patch) |
| the September analyzer-driven patch | [facet.cpp.diff](https://issues.apache.org/jira/secure/attachment/12546215/facet.cpp.diff), [locale_body.cpp.diff](https://issues.apache.org/jira/secure/attachment/12546216/locale_body.cpp.diff), [punct.cpp.diff](https://issues.apache.org/jira/secure/attachment/12546217/punct.cpp.diff) |
| a crash of `22.locale.numpunct.mt` under gdb, 15d, 32-bit Ubuntu | [22.locale.numpunct.mt.out](https://issues.apache.org/jira/secure/attachment/12513548/22.locale.numpunct.mt.out) |

## 5. The performance dispute

The cache exists to spare the public accessors a virtual call and a
trip into `__rw_get_numpunct`. Measured with exhibit D:

| system | cached | uncached |
|---|---|---|
| Solaris 10, 4 SPARCv9, GCC 4.5.3, 8 threads x 1M | 3.3 s | 26.3 s |
| CEL 4, 16 Xeon, GCC 3.4.6 | 1.1 s | 5.7 s |
| x86-64, 4 cores, 12S, 16 threads x 10M | 9.3 s | 5.2 s |
| Slackware, 16 Opteron, 12S, 16 threads x 10M | 29.8 s | 3.3 s |

The first two rows are the expected result; the last two are the
opposite. The trail: on Linux/x86-64 the string reference count goes
through a mutex in each string body, not the atomic builtins, to stay
binary compatible with 4.1.x (`include/rw/_config.h`, the AMD64
section). Copying the cached string takes and releases that mutex,
so 16 threads copying one string contend on one mutex, and `perf`
put half of the cached run in the kernel's spin lock and futex code.
The uncached version builds a new string each time and shares
nothing. Rebuilding with `_RWSTD_USE_STRING_ATOMIC_OPS` did not
change the x86 ordering, and the question was left there, with a
note that 16 threads hammering one facet may not be a representative
load.

## 6. Why nothing landed

- **Binary compatibility.** 4.2.x and 4.3.x could not change data
  members; the cache could only be bypassed (its members kept and
  unused), and removed in 5.0.
- **Barriers.** Publishing the facet data correctly without taking a
  lock on the fast path needs ordering the library could not spell
  portably in 2012. Moving the slow path out of line, where "ugly,
  bloated locking" could change without affecting the ABI, was agreed
  as the way and never worked out.
- **Hardware.** The ordering defect could only be demonstrated on a
  weakly ordered machine, and the one available was x86-64.
- **The discussion stopped.** The last message is 2012-10-27. STDCXX-1071
  is still open.

## 7. What has changed since

- The tree reproduces the cache defect without help: the 15D
  `22.locale.numpunct.mt` fails on every run on x86-64, and a
  ThreadSanitizer build names the cache (`analysis-locale-mt-race.md`).
- ThreadSanitizer implements the C++11 memory model rather than a
  lock-set heuristic, so its reports are races in the language's
  sense. Under that model exhibit E is a data race (an unsynchronized
  read of `counter` concurrent with its write), and so undefined
  behaviour, although it works on the hardware of 2012 and of today.
  The 2012 argument about exhibit E was a sound argument about
  hardware and a wrong one about the language; the facet data case,
  which publishes a second variable through the flag, was never
  covered by it, and was conceded as a defect.
- The revival keeps the 4.2.x binary interface until the next minor
  version. `TODO` lists the changes that wait for it, the string
  clause among them.
- `std::atomic` with acquire and release orderings expresses the
  facet data publication directly, once the C++17 floor lands; before
  it, the GCC and Clang `__atomic` builtins do.
- Weakly ordered hardware is at hand: an aarch64 machine now builds
  and runs the suite.
