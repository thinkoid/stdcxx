# A corrupt `mbstate_t` on the C-library codecvt path, revisited

`analysis-codecvt-invalid-state.md` (2026-09-16, 2a4b5c5f) made
every codecvt member answer a corrupt `mbstate_t` with `error`, or 0
from `length`, in an optimized build and with an assertion in a
debug build. 10ad3c6c (2026-11-03) removed that check from two
branches of the wide byname facet so that a character split across
two calls converts through the C library. The nightly of 2026-11-04
showed one failed assertion in `22.locale.codecvt` in the four
single-threaded configurations, and a probe showed the hang of
September back behind it. This note records what each change did,
what a corrupt state does now on every path, why the test saw only
part of it, where the two contracts conflict, and the ways out. The
decision is queued in `TODO` as "The C-library wide `codecvt_byname`
no longer detects a corrupt state".

## 0. tl;dr

- The September fix completed one idiom across `src/codecvt.cpp`
  and `src/wcodecvt.cpp`: check `mbsinit` on entry, assert in a
  debug build, return the error result otherwise. On the C-library
  branches of the wide byname facet the check was qualified, since
  a stateful encoding may be mid-sequence between calls: there it
  read "the encoding is stateful, or the state is initial".
- That qualification is wrong for glibc's UTF-8. `mbtowc (0, 0, 0)`
  reports UTF-8 as stateless, yet glibc keeps the bytes of an
  incomplete character in the `mbstate_t` between calls. A caller
  that offers half a character, then the rest, hands the second
  call a non-initial state in a "stateless" encoding, and the check
  rejected it. 10ad3c6c removed the check from the C-library `do_in`
  and `do_length` and made `do_in` consume the half character into
  the state with `partial`, which 22.2.1.5.2 p1 and p9 call the
  precondition of the next call, not an invalid state.
- Without the check, a state of all 0xff bytes reaches glibc. Under
  a UTF-8 locale `mbrtowc` does not return: the probe is killed at
  10 seconds. Under a single-byte locale glibc's `mbrtowc` asserts
  `mbsinit` itself when the converted character is NUL, and aborts.
  `mbrlen` on the same state returns, and `length` returns 0.
- The test's C-library locale is `aa_DJ`, single-byte, with a NUL
  byte as input. Its `in` row passes in the debug builds because
  glibc aborts; its `length` row fails because nothing aborts. The
  thread-safe debug builds skip the C-library facets in that
  section, so 15D never measured it, and the row was re-pinned "+4
  in each configuration" from 15D alone on 2026-11-03. The first
  full measurement, the nightly of 2026-11-04, pinned the failure.
- The conflict is confined to the two C-library branches, and only
  because the C library offers no way to tell a carried state from
  a corrupt one. C defines the meaning of a zero-valued object and
  of one its own functions altered; it gives no meaning to any other
  contents. The ways out are in chapter 7.

## 1. The September fix

`22.locale.codecvt` ends with `test_mbstate_t`, which fills an
`mbstate_t` with 0xff bytes and calls `in`, `out`, `unshift` and
`length` on six facets: the two base facets in the C locale, the
two byname facets over a C-library locale, and the same two over a
locale of the library's own database. In a debug build it expects
each call to abort, and catches the abort with a `SIGABRT` handler
and `siglongjmp`; in an optimized build it expects `error`, or 0
from `length`. The section came with the test in 2008.

Before 2a4b5c5f half of the library's paths computed the check and
discarded the answer, and the C-library branch of the wide byname
`do_in` handed the corrupt state to `mbrtowc`, which never returned.
The fix made every path act on its check. The check itself was left
as the file had it: `mbsinit` on the library's own paths, and on
the C-library branches

```c++
const bool mbstate_valid =
    mbtowc (0, 0, 0) || __rw::__rw_mbsinit (&state);
```

"a stateful encoding may legitimately be mid-sequence, so the check
there is: stateless encoding, therefore initial state". The note of
that day recorded the limit it knew: a corrupt state in a stateful
encoding stays undetectable. It did not record the other limit,
that a stateless encoding's state is not empty between calls under
glibc.

The same fix noted that a thread-safe debug build cannot run the
section against the C-library facets: those take the process-wide
`setlocale` lock before the check, and a jump out of the assertion's
`abort ()` leaves the lock held. Under `_RWSTDDEBUG &&
_RWSTD_REENTRANT` the test skips the two C-library facets and says
so. That is why 15D and 15D-clang count 668 assertions where the
four single-threaded tables count 684 and 688.

## 2. The November change

The C-library `do_in` converts with `mbrtowc`. When the source ends
inside a multibyte character, `mbrtowc` returns -2 and, by C99
7.24.6.3.2 p4, has stored the bytes it consumed in the state. The
library returned `ok` with nothing consumed. The caller offered the
same bytes again, with the state that already held them, and the
check above found a non-initial state in an encoding `mbtowc (0, 0,
0)` calls stateless: an assertion in a debug build, `error` in the
others. A character split across two buffers never converted through
the C library. `wifstream` over a small buffer hit it.

10ad3c6c made `__rw_libc_do_in` consume the bytes on -2 and return
`partial`, the result 22.2.1.5.2 p4 gives when "additional source
elements are needed before another destination element can be
produced". For the next call to accept the carried state, the check
had to go from the C-library `do_in`; and from the C-library
`do_length`, whose p9 precondition is the same as p1: "state
initialized, if at the beginning of a sequence, or else equal to the
result of converting the preceding characters in the sequence". The
other branches kept their checks. The test gained `test_libc_split`:
the half character, its completion, `length` from a carried state,
and a `wifstream` through a one-element buffer, under a C-library
UTF-8 locale. The row grew by four assertions.

The six pins were re-pinned by adding four to each, from the 15D
measurement. In 15D the invalid-state section does not reach the
C-library facets, so the re-pin carried an assumption about the
other four tables that the nightly of 2026-11-04 disproved.

## 3. Where the checks stand now

The state check, `mbsinit` with an assertion and the error result,
by member and path after 10ad3c6c:

| facet, path | `do_in` | `do_out` | `do_unshift` | `do_length` |
|---|---|---|---|---|
| `codecvt<char, char>` | through `do_out` | yes | yes | yes |
| `codecvt<wchar_t, char>` | yes | yes | yes | yes |
| `codecvt_byname<wchar_t, char>`, library database | yes | yes | yes | yes |
| `codecvt_byname<wchar_t, char>`, C library | none | qualified | yes | none |
| `codecvt_byname<wchar_t, char>`, ISO-2022 | elements | elements | elements | elements |

"Qualified" is the September form, stateful or initial. The
ISO-2022 state is the library's own struct, stored in the
`mbstate_t`, so the branches have no `mbsinit` to call; "elements"
is their check: the zero object is the initial state, and any other
must invoke elements in range, asserted. The narrow byname facet is
the narrow base facet's code under another name and checks as the
base does.

## 4. What a corrupt state does now, by probe

The probe constructs the wide byname facet directly, fills the
state with 0xff bytes before each call, and calls `in` and `length`
on a one-byte source holding NUL, as the test does. Built against
the debug 11S library of the nightly:

```c++
#include <locale>
#include <cstring>
#include <cstdio>
#include <cwchar>

int main (int argc, char**)
{
    typedef std::codecvt_byname<wchar_t, char, mbstate_t> Cvt;

    const Cvt cvt (argc > 1 ? "C" : "en_US.UTF-8");

    mbstate_t state;
    std::memset (&state, -1, sizeof state);

    char ext = '\0'; const char* enext = 0;
    wchar_t in = L'\0'; wchar_t* inext = 0;

    std::printf ("in: %d\n",
                 (int) cvt.in (state, &ext, &ext + 1, enext,
                               &in, &in + 1, inext));

    std::memset (&state, -1, sizeof state);
    std::printf ("length: %d\n", cvt.length (state, &ext, &ext + 1, 1));

    return 0;
}
```

| locale | `in` | `length` |
|---|---|---|
| `en_US.UTF-8` | does not return; killed at 10 seconds | not reached |
| `C` | `Fatal glibc error: mbrtowc.c:102 (__mbrtowc): assertion failed: __mbsinit (data.__statep)`, `SIGABRT` | 0, no abort |

Both outcomes are glibc's, not the library's. The UTF-8 converter
unpacks a carried character from the state as a total length taken
from the high bits of the count and a pending-byte count from the
low eight (`iconv/gconv_simple.c`, `UNPACK_BYTES`); 0xffffffff
gives a total of sixteen million and 255 pending bytes, beyond any
UTF-8 sequence, and the conversion never returns. The exact loop
was not traced. The single-byte abort is narrower than it looks:
`mbrtowc` asserts `mbsinit` only when the character it converted
is NUL (`wcsmbs/mbrtowc.c`), and the probe's input byte is NUL. A
non-NUL byte under a single-byte locale would return normally with
the state untouched; that case was not probed. `mbrlen` runs the
same converter without the NUL assertion, which is why `length`
returns 0 where `in` aborts.

The test's C-library locale is the first non-C name the C library
knows, `aa_DJ` on the host of the tables, a single-byte locale, and
its input is the same NUL byte. So the test's `in` row passes in a
debug build by glibc's abort, caught by the test's handler like the
library's own, and its `length` row fails for want of one. In an
optimized build the `in` row would fail too: glibc's assertion is
compiled in regardless, so `in` aborts where the test expects
`error`. No optimized configuration is pinned.

## 5. The red row

| table | `22.locale.codecvt` |
|---|---|
| 11S, 11s, 11S-clang, 11s-clang | 684 or 688 assertions, 1 failed, line 1232 |
| 15D, 15D-clang | 668, 0 failed; the C-library facets skipped |

The failing assertion:

```
std::codecvt_byname<wchar_t, char, std::mbstate_t>::length
(state_type&, ...) failed to detect an invalid state argument
```

It is pinned as measured since 006ec1b2, with the entry in `TODO`.

## 6. The two contracts

The September contract: a corrupt state is a precondition
violation, asserted in a debug build like the range preconditions
beside it, and answered with the error result where assertions are
compiled out. The November contract: on the C-library path the
state between calls may hold the bytes of an incomplete character,
and 22.2.1.5.2 p1 and p9 make that the precondition of the next
call. Both hold on every path but two. On the C-library `do_in` and
`do_length` they meet, because the only question the C library
answers about a state is `mbsinit`, and a carried state and a
corrupt state both answer no.

What C says, 9899:1999 7.24.6 p3:

> The initial conversion state corresponds, for a conversion in
> either direction, to the beginning of a new multibyte character
> in the initial shift state. A zero-valued `mbstate_t` object is
> (at least) one way to describe an initial conversion state. A
> zero-valued `mbstate_t` object can be used to initiate conversion
> involving any multibyte character sequence, in any `LC_CTYPE`
> category setting. If an `mbstate_t` object has been altered by
> any of the functions described in this subclause, and is then
> used with a different multibyte character sequence, or in the
> other conversion direction, or with a different `LC_CTYPE`
> category setting than on earlier function calls, the behavior is
> undefined.

The standard gives a meaning to a zero-valued object and to one its
own functions altered, and to nothing else. An object of other
contents is not a conversion state, and the standard says nothing
about passing one. 14882:2003 22.2.1.5.2 p1 and p9 require the
state "initialized, if at the beginning of a sequence, or else
equal to the result of converting the preceding characters": a
corrupt state meets neither, and a call with one is outside the
library's contract too. The September note said as much: both the
assertion and the error result are quality-of-implementation
choices.

## 7. The ways out

1. **The check returns to the two branches**, in its September form.
   A corrupt state is detected on every path again, the hang is
   gone, and the row is green. The cost is the carried character:
   the call after a `partial` finds a non-initial state in an
   encoding `mbtowc (0, 0, 0)` calls stateless and returns `error`,
   which is where 10ad3c6c started. `test_libc_split` would have to
   go with it, and `wifstream` over a C-library UTF-8 locale would
   fail again on a character that straddles a buffer.
2. **The C-library path is left undefined on a corrupt state.** The
   library's own paths keep their checks; the two C-library branches
   take the state as the C library takes it. The test's C-library
   section stops passing a corrupt state in every configuration, as
   it already does in the thread-safe debug builds, and the row
   returns to 0 failed with sixteen assertions fewer in the four
   tables. `ref-conformance.md` and the September note record the
   limit: on the C-library path a corrupt state is the caller's, and
   under glibc's UTF-8 it is a hang. This is the position the C
   standard takes and the one the library already held, unknowingly,
   for a corrupt state in a stateful encoding.
3. **A check of the state's shape**, later, under a characterization.
   glibc keeps the pending bytes of a UTF-8 character in the state
   with their count in the low eight bits of `__count` and the
   character's total length above them; a carried state has a count
   below its total and a total of two to six. A probe could learn
   that layout at configuration time and a check could reject what
   does not fit it, on glibc only, with the carried character kept.
   The layout is glibc's private business and may move; the
   characterization is what would notice.

The library takes the second way, since the first trades a
conforming feature for the detection of an input that no standard
defines. The third was queued and then declined: the check would
read a layout glibc may move, hold for UTF-8 alone, and turn an
input no standard defines into `error` for the caller who passed
it.

## 8. Instruments

- The probe above: `~/build/stdcxx-build/handover-20261104/probe-codecvt-state.cpp`
  on the host of the tables; build with the 11S line of the nightly's
  test log and link against its `libstd11S.a`.
- The nightly of 2026-11-04: `~/build/stdcxx-build/nightly-20261104/`,
  `11S/tests/22.locale.codecvt.out` for the failing run.
- `git show 2a4b5c5f -- src/wcodecvt.cpp` and `git show 10ad3c6c --
  src/wcodecvt.cpp` for the two changes side by side.
