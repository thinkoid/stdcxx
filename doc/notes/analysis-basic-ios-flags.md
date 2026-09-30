# `basic_ios::flags ()` after construction carries the library's own bits

## 0. tl;dr

`27.basic.ios` fails one assertion in every configuration. A
`basic_ios` constructed with a null buffer must have
`flags () == skipws | dec`. The library returns more:

- in every build, the numeric base, 10, in bits 26 to 31;
- in the thread-safe builds, also `nolock | nolockbuf`, the locking
  extension's default for streams other than the eight standard ones.

`flags ()` returns the stored word whole, so a caller sees both. The
postcondition is an equality, and a program can observe the
difference; libstdc++ returns exactly `skipws | dec`.

STDCXX-950 reported the failure in June 2008 and proposed to mask the
extra bits in the test. The patch never landed. Masking would declare
the library right. The fix that makes the postcondition true removes
what sets the bits, the extended `setbase` bases, `bin` and the
locking extension, which the strict-ANSI entry in `TODO` already
lists. The failure is a symptom of that entry, not a separate defect.

## 1. The failing row

`test_ctors` (`tests/iostream/27.basic.ios.cpp`) constructs
`std::basic_ios<char> io0 (0)` and checks the postconditions of
`init`. The failing check, at line 251:

```cpp
rw_assert ((io0.skipws | io0.dec) == io0.flags (),
           __FILE__, __LINE__,
           "basic_ios<%s>::flags () == %{If}, got %{If}",
           cname, io0.skipws | io0.dec, io0.flags ());
```

In 11S the message reads:

```
basic_ios<char>::flags () == dec | skipws, got dec | skipws
```

Expected and observed print the same. The driver's `%{If}` formatter,
`_rw_fmtflags` (`tests/src/fmt_bits.cpp:150`), zeroes the base bits
before it names the flags, and names a base only when it is not 8, 10
or 16. Base 10 prints as nothing. In 15D the message names the other
difference:

```
basic_ios<char>::flags () == dec | skipws, got dec | skipws | nolock | nolockbuf
```

The check above it has a defect of its own. It asserts
`io0.badbit == io0.rdstate ()` and prints `io0.flags ()` as the
observed state (line 247). The assertion passes, so the message has
never shown.

## 2. The probe

A program built with the flags of the build's tests prints
`flags ()` of a fresh `basic_ios<char>`:

| build | `flags ()` | beyond `skipws \| dec` |
|---|---|---|
| 11S | `0x28001002` | `0x28000000` |
| 11S-clang | `0x28001002` | `0x28000000` |
| 15D | `0x28031002` | `0x28030000` |
| libstdc++ | `0x1002` | none |

`0x28000000` is `10 << 26`: base 10 in the field `_RWSTD_IOS_BASEOFF`
and `_RWSTD_IOS_BASEMASK` define (`include/rw/_defs.h:718`).
`0x30000` is `_RWSTD_IOS_NOLOCK | _RWSTD_IOS_NOLOCKBUF` (lines 698
and 699).

## 3. Where the bits come from

`ios_base::_C_init` (`src/ios.cpp:165`) initializes every stream:

```cpp
const unsigned fmtfl = (10U << _RWSTD_IOS_BASEOFF) | skipws | dec;
...
_C_fmtfl  = fmtfl;
...
#if     defined (_RWSTD_REENTRANT)             \
    && !defined (_RWSTD_NO_EXT_REENTRANT_IO)   \
    && !defined (_RWSTD_NO_REENTRANT_IO_DEFAULT)

    // disable locking of iostream objects and their associated buffers
    // standard iostream objects will override in ios_base::Init::Init()
    _C_fmtfl  |= _RW::__rw_nolock | _RW::__rw_nolockbuf;

#endif
```

`ios_base::Init` clears `nolock` and `nolockbuf` on the eight standard
objects (`src/iostream.cpp:282`, 321). The getter returns the word
unmasked (`include/rw/_iosbase.h:228`):

```cpp
fmtflags flags () const {
    return fmtflags (_C_fmtfl);
}
```

## 4. What the bits are for

**The base bits** hold the stream's numeric base for the `setbase`
extension: bases 3 to 36, and 1 for Roman numerals, which `basefield`
cannot express. `num_get` and `num_put` read the base from them.
`analysis-numeric-base.md` describes the encoding and two defects in
it: `setbase (10)` selects autodetection, and clearing `basefield`
does not clear `hex` or `oct`.

**The lock bits** are the locking extension. In a thread-safe build
the guarded stream operations take the stream's mutex unless `nolock`
is set, and the buffer's unless `nolockbuf` is. `ios_base::imbue`
guards with
`_RWSTD_MT_GUARD (flags () & _RW::__rw_nolock ? 0 : &_C_mutex)`;
`flags`, `precision` and `width` exchange their value through
`_RWSTD_ATOMIC_IO_SWAP` (`include/rw/_defs.h`), an ordinary exchange
under `nolock` and a locked one otherwise. Without the extension the
macro is always the locked exchange. A user sets or clears the bits
through the flags, as with any format flag. The original README describes the two controlling macros
(section 8.3):

- `_RWSTD_NO_EXT_REENTRANT_IO` removes the extension and the symbols
  `ios_base::nolock` and `ios_base::nolockbuf`. `_RWSTD_STRICT_ANSI`
  defines it (`include/rw/_config.h:259`).
- `_RWSTD_NO_REENTRANT_IO_DEFAULT`, when defined, creates new streams
  locked. By default they are created unlocked, with the two bits set.

So in the tracked configurations a user's stream is unlocked by
default, and the standard objects are locked.

## 5. The standard

The postconditions of `basic_ios::init` are a table in C++03
27.4.4.1. N4296, [basic.ios.cons], has the same row:

| element | value |
|---|---|
| `flags()` | `skipws \| dec` |

The postcondition is an equality. The library's value differs from it
in bits the standard does not define. A program that compares
`flags ()` with a value it computed, or prints it, sees the
difference.

## 6. History

- The test came from the vendor's repository as `27_basic_ios.cpp`
  and was migrated to the driver in April 2008, STDCXX-868 (a72a2d70
  on trunk, 2008-04-16; ca1622dc on the release branch, 2008-05-07).
  The migration's announcement on the development list says the
  original disabled some assertions "by hard-coding", and that the
  migration enabled them behind a command-line option. The option is
  `--no-basic_ios-ctors`, and it skips `test_ctors`, where the flags
  check is. Why the original disabled them is not recorded.
- STDCXX-950, opened in June 2008 against trunk, reported the failure
  in a thread-safe build, with the message of chapter 1: the formatter
  named the lock bits and hid the base bits. A patch followed four
  minutes later, "Clear internal fmtflags bits when verifying
  basic_ios::flags() method". It was never applied. The issue carries fix version 4.2.2,
  never released, and no resolution.
- The base bits and the lock default are in the 2005 import
  (e6c7726b).

## 7. The options

**Mask the bits in the test**, as STDCXX-950 proposed. The row turns
green and the library stays as it is. That declares the library
right, and it is not: the difference is observable and the standard
states an equality. Under a C++03 target the test is right.

**Mask the bits in the getter.** `flags ()` would return the standard
bits only. That breaks the round trip `f = s.flags (); ...; s.flags
(f)`, which must restore the stream's state:

- an extended base is lost. `f` carries an empty `basefield` and no
  base bits, and the setter turns base bits 0 into autodetection;
- `nolock` is lost. Restoring `f` turns locking back on for a stream
  its user had unlocked.

A getter that hides state the setter needs is a new defect.

**Remove what sets the bits.** The strict-ANSI entry in `TODO`
removes the extended `setbase` bases, `bin` and the stream-locking
additions. With them gone:

- `basefield` carries the base alone. The facets can read it from
  there, and the base bits, the setter's reconciliation and both
  defects of `analysis-numeric-base.md` go with them.
- `_RWSTD_NO_EXT_REENTRANT_IO` is defined, and `_C_init` sets no lock
  bits.

Then `flags ()` is `skipws | dec` after `init` by construction, in
every build.

The last step carries a decision the entry does not make yet: the
locking of a user's stream. Without the extension, every stream in a
thread-safe build is locked, and the eight standard objects already
are. Today user streams are unlocked by default. C++11 asks for less
than the extension's absence gives: concurrent access to a stream
object "may result in a data race" ([iostreams.threadsafety]), and
only the synchronized standard objects are guaranteed free of races
([iostream.objects], p4). The library's default already has that
shape. Keeping it without the flag bits means a home for the lock
state outside `fmtflags`: a member of `ios_base`, which changes its
layout. The revival does not keep the 4.2.x binary interface, so that
is open to it.

## 8. Resolution

Decided 2026-09-30: the `setbase` extension and `bin` go now, as the
first slice of the strict-ANSI entry (`analysis-numeric-base.md`,
chapter 8). The base bits go with them. After that slice the row
passes in the single-threaded builds; in the thread-safe ones it still
fails on `nolock | nolockbuf`.

The locking extension waits for the decision of chapter 7: whether a
user's stream stays unlocked by default, and where the lock state
lives if it does. The `rdstate` message of chapter 1 is repaired with
the row.

Check, when the locking slice lands: the row at 100% in the six
configurations, and a copy of the test that sets a bit outside the
standard's after construction fails it.
