# A composite locale name overwrites the moneypunct test's stack

## 0. tl;dr

`22.locale.moneypunct` copied the result of `setlocale (LC_ALL, ...)`
into a 256-byte array. A mixed-category locale produced a 276-byte name;
the copy, including its terminator, overwrote the stack canary. This is
a test defect, present since its original introduction in May 2007
(`3452176f`, merged into the release branch as `d88eb793`).

The test now uses a 256-byte stack buffer with a `malloc` fallback. Explicit
mixed-category cases exercise the repair without relying on the starting
environment. Targeted runs pass under GCC and Clang in all six measured
configurations. The library is unchanged.

## 1. Why the name grew

The original copy had a purpose: subsequent calls to `setlocale` can
invalidate its returned storage, and the test needs the name for its
comparisons and diagnostics. The error was imposing a fixed bound.

With all categories equal, libc returns a short name such as
`en_US.UTF-8`. With differing categories, glibc returns semicolon-separated
assignments covering all twelve categories, including its extensions.
The observed name began `LC_CTYPE=C.UTF-8;LC_NUMERIC=en_US.UTF-8;`
and ended `LC_IDENTIFICATION=en_US.UTF-8`; the other categories also
used `en_US.UTF-8`. Its length was 276 bytes, excluding the terminator.

The test initially had `LC_ALL` masking the category overrides. At the
end of each `runTest` invocation it removes `LC_ALL`; the next invocation
then encounters the mixed environment in its native-locale case. This
explains why the first empty-name call succeeded and a later one aborted.

## 2. Establishing the write

In the original GCC 11S binary, a debugger breakpoint immediately before
the copy measured `strlen (loc) == 276` and `sizeof (locnamebuf) == 256`.
Stepping over that single statement changed the bytes beyond the array:
at offset 256, `FICATION`; at offset 264, where the canary sat,
`=en_US.U`. Continuing reached `__stack_chk_fail` at the helper's return.

The earlier comparison with the former pattern matcher reproduced the
same abort. The matcher replacement exposed the failure during its suite
refresh but did not cause it. Valgrind's absence of invalid-access reports
did not exclude a write between objects within a live stack frame.

## 3. Repair and regression

The test retains its 256-byte stack buffer for short names and uses
`malloc (strlen (loc) + 1)` when the name and its terminator do not fit.
The copy is made before further locale changes. Only the allocated
buffer is freed, on normal completion, the early null-`lconv` return,
and the caught-exception path. Allocation failure is an assertion failure
and returns without attempting the copy.

The regression setup uses the same stack-buffer/fallback pattern to
preserve its composite name across calls. Neither change introduces a
Standard Library type: testing one library type should avoid depending
on another where practical. The test's pre-existing string uses remain.

For each accepted installed locale, the test now sets that locale,
changes only `LC_NUMERIC` to `C`, copies libc's resulting name and tests
that composite locale. Monetary data stays in the selected locale. The
existing four instantiations cover narrow and wide characters and national
and international punctuation. Failure to exercise the resulting locale
is an assertion failure rather than a silent skip.

The installed `en_US.utf8` produces a 259-byte composite name in this
case. That is already too large for the old array, but its four-byte
overflow lands in padding before the canary on this GCC 11S build.
Consequently a normal negative-control run can pass despite the overwrite.
With AddressSanitizer, the same control reports a 260-byte write past
`buf` from the new mixed-name call. Disabling only the helper's allocation fallback
in a scratch copy is sufficient to trigger it; the repaired copy passes.
Long-name coverage depends on the installed locales; a machine with only
`C` and `POSIX` cannot supply this particular long composite name.

## 4. Verification, 2026-09-17

Rebuilt this test and ran it directly in `11S`, `11s` and `15D`, under
GCC and Clang. Each configuration passed with the inherited environment,
a clean C environment, and an explicit mixed environment:

| environment | assertions | failed | exit |
|---|---|---|---|
| inherited | 540 | 0 | 0 |
| `LANG=C`, category overrides removed | 500 | 0 | 0 |
| `LANG=en_US.UTF-8`, `LC_CTYPE=C.UTF-8`, other overrides removed | 540 | 0 | 0 |

`make run RUN=22.locale.moneypunct` passed 540 assertions in all six
configurations with the existing harness options. The sandbox killed
32-bit execution with SIGSYS and its harness produced no test row;
those results were discarded and verification repeated outside it.

The GCC 11S AddressSanitizer check instruments the test translation unit
and links the existing library and driver. The stack-only control
exits 1 with `stack-buffer-overflow`; the repair exits 0 with 500 passing
assertions. Leak detection is disabled for this access check.

Valgrind reports no invalid accesses in the repaired test. A separate
full leak check reports 96 bytes in eight `rw_putenv` allocations, also
present in the original binary under the same environment. This repair
does not change that helper's environment-storage lifetime.

Scratch fault-injection checks track both new allocation sites. Normal
completion, failure of either allocation, a caught exception before
`lconvdup`, and the null-`lconv` early return all leave zero tracked
allocations. Injected failures produce the expected assertion diagnostics;
the callback's zero exit status alone is not used as their oracle.

The full suite was not rerun; its pinned tables retain the preceding
measurement, including the old moneypunct abort.

OpenAI LeChuck
