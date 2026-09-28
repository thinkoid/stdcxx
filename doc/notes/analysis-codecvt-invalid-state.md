# The codecvt facets on an invalid `mbstate_t`

`22.locale.codecvt` ends with a section that passes a corrupt
`mbstate_t` to every codecvt facet and asks each member what it does
about it. In every debug build the section killed the test, and the
harness reported `ABRT` for the whole program. This note records what
the section expects, why the expectation stopped being met, what the
library did in the builds where it was met, and the fixes.

## 0. tl;dr

- The section is not a mismatch between the test and the library's
  assertion. The test was written to expect the assertion: in a debug
  build it installs a `SIGABRT` handler, jumps out of the `abort()`,
  and records a pass. glibc 2.41 (October 2024) changed `abort()` so
  that the second jump of the process no longer works, and the second
  assertion killed the test. The handler now saves and restores the
  signal mask, which is all the jump needed.
- In an optimized build the test expects every member to answer
  `error` (`length`, 0). The library did so on about half its paths;
  the two `do_out` members and the six branches of the wide byname
  facet checked the state, asserted, and then discarded the answer.
  The libc-backed wide facet went on to hand the corrupt state to
  glibc's `mbrtowc`, which never returns. Every path now returns the
  error result when its check fails. The assertion stays in debug
  builds.
- In a thread-safe debug build the section cannot be run against the
  libc-backed facets at all: they take the process-wide locale lock
  before the check, and the jump out of the assertion leaves it held.
  The test leaves those facets to the other builds there and says so.
- Two smaller repairs of the same test: a table row that expected
  `ok` from an `out` that cannot complete, and the test's own read of
  `TOPDIR`, which used a null pointer when the variable was unset. The
  driver now exposes the directory it derives, and the test takes it
  from there.

## 1. What the section expects

`test_mbstate_t` fills an `mbstate_t` with `0xff` bytes and calls
`in`, `out`, `unshift` and `length` on the facet under test, six
facets in all: the narrow and wide base facets in the `"C"` locale,
the narrow and wide byname facets over a libc locale, and the same
two over one of the library's own locale databases. The expectation
is set by the build:

```c++
#ifndef _RWSTDDEBUG
    // when debugging is disabled, expect member functions to
    // detect and indicate an invalid argument by setting the
    // return value to codecvt_base::error
    const bool expect_abort = false;
#else
    // when debugging is enabled, expect member functions to
    // detect and indicate an invalid argument by calling abort()
    const bool expect_abort = true;
#endif
```

Each call is wrapped in `setjmp`, with a `SIGABRT` handler that
`longjmp`s back; landing in the handler is the pass in a debug build
and a failure otherwise. The section came with the test in the 2008
migration from the vendor's repository and is unchanged since; trunk
has the same text. So the contract on record is: a corrupt state is a
precondition violation, asserted in debug builds like the range
preconditions three lines above it, and answered with `error` where
the assertions are compiled out.

## 2. The jump out of `abort()`

The 11S and 15D runs printed the library's assertion message twice
and then died of `SIGABRT`: the first `abort()` was caught, the
second was not. The handler is reinstalled before every call, so the
disposition was not the problem; the signal mask was.

When the kernel delivers a signal to a handler it blocks that signal
for the duration of the handler. A `longjmp` out of the handler never
returns from it, so the block stays. glibc's `setjmp` does not save
the signal mask (with `_GNU_SOURCE`, which g++ defines, `setjmp` has
System V semantics), so the `longjmp` does not restore it either.
After the first caught assertion, `SIGABRT` is blocked in the thread.

Up to glibc 2.40, `abort()` began by unblocking `SIGABRT`, in a stage
machine whose comment says why: "we must allow repeated calls of
abort when a user defined handler for SIGABRT is installed". glibc
2.41 rewrote `abort()` to be async-signal-safe (BZ 26275, 2024-10-03)
and dropped that stage. The new function raises `SIGABRT` directly,
and if it finds itself still running afterwards, which is what a
blocked signal looks like from the inside, it installs the default
disposition and raises again. That is the second, fatal abort.

The fix is the one the old `abort()` was doing on the test's behalf:
`sigsetjmp (env, 1)` saves the mask and `siglongjmp` restores it. The
`jmp_buf` becomes a `sigjmp_buf`, and `<csetjmp>` becomes
`<setjmp.h>`, since the POSIX names are not in the standard header.
The test already includes `<unistd.h>`, `<iconv.h>` and
`<langinfo.h>` unconditionally, so it asked for POSIX before this.

## 3. What an optimized build did

An 8S build, the first optimized build of the revival, ran the
section and showed the test's own verdict on the library:

| facet | `in` | `out` | `unshift` | `length` |
|---|---|---|---|---|
| `codecvt<char, char>` | not detected | not detected | `error` | 0 |
| `codecvt<wchar_t, char>` | `error` | not detected | `error` | 0 |
| `codecvt_byname<char, char>` | not detected | not detected | `error` | 0 |
| `codecvt_byname<wchar_t, char>`, libc locale | hangs | | | |
| `codecvt_byname<wchar_t, char>`, own database | not reached | | | |

The narrow base facet's `do_out` (and `do_in`, which calls it) had
the state check inside `#ifdef _RWSTDDEBUG`, with `_RWSTD_UNUSED
(state)` in the other branch; the wide base facet's `do_out` the
same. The wide byname facet's `do_in`, `do_out` and `do_length`, in
both their libc and library-database branches, computed the check,
asserted on it, and then `_RWSTD_UNUSED (mbstate_valid)`. The
remaining members, the base facets' `do_in` (wide), `do_unshift` and
`do_length`, and the byname `do_unshift`, follow the check with
`if (!mbstate_valid) return error;` or its equivalent. The file's own
idiom, applied to half of the file.

The hang is the cost of the missing half. The libc branch of the wide
byname `do_in` passes the state to `mbrtowc`; with `0xff` in
`__count`, glibc's UTF-8 conversion loops without consuming anything.
The stack at the interrupt:

```
#0  0x00007ffff7a2ea3d in ?? () from /usr/lib/libc.so.6
#1  0x00007ffff7ac29dd in mbrtowc () from /usr/lib/libc.so.6
#2  std::codecvt_byname<wchar_t, char, __mbstate_t>::do_in(...) const
#3  test_mbstate_t<std::codecvt_byname<wchar_t, char, __mbstate_t>>(...)
```

The fix completes the idiom: the two `do_out` members check
unconditionally and return `error` when the check fails, as their
`do_in` neighbours do, and the six byname branches `break` out of the
switch with the result variable at its initial value, which is
`error` for the conversions and 0 for `length`. The check is
`mbsinit`, one integer compare on glibc. The libc branches keep their
qualification: a stateful encoding may legitimately be mid-sequence,
so the check there is "stateless encoding, therefore initial state",
and a corrupt state in a stateful encoding stays undetectable.

The standard has no notion of an invalid `mbstate_t`; the caller owns
the object and must initialize it, and passing garbage is undefined.
Both the assertion and the error result are quality-of-implementation
choices, and nothing conforming forbids either. The assertion stays
because it is how the library treats every precondition, and because
the test expects it.

## 4. The lock in the thread-safe debug build

With the mask fixed, 15D still hung, after the thirteenth caught
assertion: the libc branch of the wide byname `do_in`. That branch
constructs a `__rw_setlocale` before it checks the state, because
the check asks the locale whether the encoding is stateful
(`mbtowc (0, 0, 0)`), and `__rw_setlocale` acquires the process-wide
`__rw_setlocale_mutex` (`src/setlocale.cpp`). The assertion's
`abort()` fires with the mutex held, the jump out of the handler
skips the guard's destructor, and the next facet call that wants the
locale waits forever. In a build without threads the mutex is a
no-op, which is why 11S passes.

The library is not wrong to check under the lock; the answer needs
the locale. The test's device, abort and continue, is what cannot
work under a held lock, and only the libc-backed facets hold one. In
an optimized build the error return runs the destructor normally and
the facets are fully exercised there. So under `_RWSTDDEBUG &&
_RWSTD_REENTRANT` the test skips the two libc-backed facets, prints a
note saying so, and runs the other four. The 15D row of the suite
reports 301 assertions where 11S reports 317, the sixteen calls it
does not make.

## 5. Assertion 1582

Before the section, one assertion failed in every build:

```
codecvt<wchar_t, char, mbstate_t>::out(..., = LBE, ...)
    == std::codecvt_base::ok, got std::codecvt_base::partial
```

The `L` is the format's wide-string prefix; the source is `L"BE"`.
The row, in the `<mb_cur_max> 6` table, was

```c++
TEST_IN_OUT ("123456789",     9,  9, L"BE",  3, 2, ok);
```

where the fifth column is both the size of the destination buffer
for `in` and the number of source characters for `out`. Three source
characters are `B`, `E` and the terminating NUL; `B` and `E` take
nine bytes, the NUL a tenth, and the destination has nine. Table 53
and LWG 382, which the library's `do_out` cites, make that `partial`.
The library is right, and the row's `ok` could not be met with a
three-character source. Every other `ok` row in the tables has the
column equal to the number of characters converted, and this one now
does too, 2; the `in` half of the row is unchanged by it.

## 6. `TOPDIR`

The test read `TOPDIR` for the paths of its fixture files, recorded
an assertion when the variable was unset, and then built the paths
from the null pointer: a segmentation fault in a bare run. The driver
already derives the directory from its own `__FILE__` when the
variable is unset, in two places; that logic is now one function,
`rw_topdir ()` in `rw_locale.h`, and the test uses it. Without either
source the test records the assertion and skips the file conversions
instead of faulting. `22.locale.collate` reads the variable the same
way and exits without it; it is left for its own entry.

## 7. Verification

Bare runs from the build's `tests` directory, with and without
`TOPDIR`:

| build | result |
|---|---|
| 11S | exit 0, 317 assertions, none failed; 24 library assertions caught |
| 15D | exit 0, 301 assertions, none failed; 16 caught, libc facets skipped |
| 8S | exit 0, 317 assertions, none failed; no assertion, no hang |

The suite tables under `baseline/` are re-pinned; the codecvt row
moves from `ABRT` to a pass in every configuration, and no other row
moves beyond the known-unstable ones.
