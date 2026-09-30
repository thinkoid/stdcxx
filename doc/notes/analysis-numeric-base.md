# The numeric base bits: `setbase (10)` and a `basefield` that will not clear

## 0. tl;dr

The library keeps a stream's numeric base in the top six bits of its
format flags, beside `basefield`. Two defects follow from how the
flags setter reconciles the two. Both are library defects, in every
configuration, under both compilers.

- **`setbase (10)` selects autodetection.** After
  `in >> std::setbase (10)` a stream reads `"010"` as 8. The standard
  makes `setbase (10)` equal to `setf (dec, basefield)`, which reads
  10. `27.std.manip` fails eight assertions on it.
- **Clearing `basefield` does not clear `hex` or `oct`.** After
  `std::hex`, `unsetf (basefield)`, `setf (0, basefield)` and
  `resetiosflags (basefield)` all leave `hex` set: `"010"` reads as
  16, where the standard reads 8. No test fails on it.

The code predates the 2005 import. The check that catches the first
defect came with the test from the vendor's repository in 2008. No
issue or list thread records either defect.

The resolution removes the extension. `setbase` becomes the
standard's function, `bin` goes with the extended bases, and
`basefield` alone carries the base. A patch to the encoding was
designed and checked on a model of the setter first, and rejected:
chapter 8 has both.

## 1. The failing rows

`27.std.manip` runs `do_test` for four instantiations: `char` and
`wchar_t`, each with `std::char_traits` and the test's own
`CharTraits`. Each run applies `setbase (10)` twice: in the block of
the standard's bases (`tests/iostream/27.std.manip.cpp:570`) and in
the block of the extended bases (591). Four instantiations times two
sites give the eight failures.

The check is in the `setbase` case of `test<>` (line 227). It applies
the manipulator to one stream and the standard's definition to
another, and compares the flags:

```cpp
out1 << std::setbase (iarg);

out2.setf (   8 == iarg ? std::ios_base::oct
           :  2 == iarg ? std::ios_base::bin
           : 10 == iarg ? std::ios_base::dec
           : 16 == iarg ? std::ios_base::hex
           : Fmtflags (0),
             std::ios_base::basefield);

if (out1.flags () != out2.flags ())
    rw_assert (0, __FILE__, lineno, "std::setbase (%d)", iarg);
```

Bases 0, 2, 8 and 16 pass. Base 10 fails. The message names the
manipulator, not the flags, so it does not show what differs.

## 2. Where the base lives

`include/rw/_defs.h` defines the format flags. The standard's flags
and the library's additions (`bin`, `nolock`, `nolockbuf`,
`sync_stdio`) take the low 19 bits. Two more definitions reserve the
top of the word:

```cpp
#define _RWSTD_IOS_BASEMASK      63
#define _RWSTD_IOS_BASEOFF       26
```

Bits 26 to 31 hold the numeric base, 0 to 63. They serve the
`setbase` extension: bases 3 to 36, and 1 for Roman numerals, which
`basefield` cannot express. The facets read the base from these bits,
not from `basefield` (`include/loc/_num_get.cc:245`,
`src/num_put.cpp`).

`ios_base::flags (fmtflags)` (`src/ios.cpp:96`) reconciles the two.
It switches on `basefield`:

| `basefield` | base bits after the call |
|---|---|
| `oct` | 8 |
| `dec` | 10 |
| `hex` | 16 |
| `bin` | 2 |
| more than one bit | 10 ORed in |
| empty | 0 becomes 10. 2, 8 and 16 stay, and their `basefield` bit is set: `bin`, `oct`, `hex`. 10 stays, with `dec` clear. Any other base stays. |

The comment at the last row says that with base bits 10, "dec is left
alone (necessary for autodected parsing to work correctly)". The
facet shows why (`include/loc/_num_get.cc:245`):

```cpp
int __base = unsigned (__fl) >> _RWSTD_IOS_BASEOFF;

if (_RW::__rw_facet::_C_pvoid == __type)
    __base = 16;
else if (10 == __base && !(__fl & _RWSTD_IOS_BASEFIELD))
    __base = 0;
```

Base 0 is autodetection: a leading `0x` reads hex, a leading `0`
reads octal. So the library spells the standard's two decimal-looking
states apart:

| state | base bits | `dec` | input |
|---|---|---|---|
| `basefield == 0` | 10 | clear | autodetection, the `%i` of the standard's stage 1 |
| `basefield == dec` | 10 | set | decimal, `%d` |

The empty-`basefield` row does two jobs. It completes a base that
`setbase` stored without its `basefield` bit: 8 becomes `oct`. And it
turns base bits 0 into the autodetection state. Each job is right for
some callers and wrong for others. Chapters 3 and 4 are the two ways
it goes wrong.

## 3. First defect: `setbase (10)`

The manipulator's body is `__rw_setbase` (`include/iomanip:63`). It
validates the base, replaces the base bits and clears `basefield`:

```cpp
const unsigned __ifl =
    (  __strm.flags () & ~_STD::ios_base::basefield
     & ~(   _RWSTD_STATIC_CAST (unsigned, _RWSTD_IOS_BASEMASK)
         << _RWSTD_IOS_BASEOFF))
    | __base << _RWSTD_IOS_BASEOFF;

__strm.flags (_STD::ios_base::fmtflags (__ifl));
```

It never sets a `basefield` bit; it relies on the setter's empty row.
From a fresh stream, `0x28001002` (base bits 10, `skipws`, `dec`):

- `setbase (8)`: base bits 8. The row sets `oct`. Correct.
- `setbase (16)` and `setbase (2)`: the row sets `hex` and `bin`.
  Correct.
- `setbase (0)`: base bits 0. The row makes them 10 with `dec` clear,
  `0x28001000`: autodetection. Correct.
- `setbase (10)`: base bits 10. The row leaves them, with `dec` clear,
  `0x28001000`: autodetection. **Wrong.**

Base 10 is the one base the row cannot complete, because base bits 10
without `dec` are how the row spells autodetection.

## 4. Second defect: `basefield` will not clear

The standard's ways to clear `basefield` all reach the setter with
`basefield` empty and the old base bits in place. `setf (f, mask)`
and `unsetf (f)` are inline in `include/rw/_iosbase.h` and call
`flags (fmtflags)` with the old flags masked; `resetiosflags` calls
`setf`. After `std::hex` the base bits are 16. The empty row reads
16 as a base `setbase` left incomplete, and sets `hex` again.

The row cannot tell the two apart. Base bits 16 with `basefield` empty
arrive the same way from `setbase (16)` and from
`hex` followed by `unsetf (basefield)`. The first wants `hex`; the
second wants autodetection.

`27.std.manip` exercises `resetiosflags (basefield)`, but compares
the manipulator with `setf (0, basefield)` on another stream. Both go
through the same row, so they agree. It also starts from a fresh
stream, whose base bits are 10: that case is correct.

## 5. The probes

Two programs built against the library read `"010"` from an
`istringstream` after each way of setting or clearing the base. They
were built with the flags of the build's tests, against three builds,
and against libstdc++ with the host compiler.

```cpp
std::istringstream in ("010");
in >> std::hex;
in.unsetf (std::ios_base::basefield);
int i = -1;
in >> i;   // 8 expected: autodetection reads a leading 0 as octal
```

| setting | 11S | 15D | 11S-clang | libstdc++ |
|---|---|---|---|---|
| none | 10 | 10 | 10 | 10 |
| `setbase (10)` | **8** | **8** | **8** | 10 |
| `setf (dec, basefield)` | 10 | 10 | 10 | 10 |
| `std::dec` | 10 | 10 | 10 | 10 |
| `setbase (0)` | 8 | 8 | 8 | 8 |
| `unsetf (basefield)` | 8 | 8 | 8 | 8 |
| `hex`, `unsetf (basefield)` | **16** | **16** | **16** | 8 |
| `hex`, `setf (0, basefield)` | **16** | **16** | **16** | 8 |
| `hex`, `resetiosflags (basefield)` | **16** | **16** | **16** | 8 |

After `oct` and `unsetf (basefield)` all four read 8, since octal and
autodetection agree on `"010"`. The flags tell them apart: the
library keeps `oct` set (`0x20001040` in 11S); libstdc++ has none
(`0x1000`).

The flags after `setbase (10)` and after `setbase (0)` are the same
in each build: `0x28001000` in 11S and 11S-clang, `0x28031000` in 15D,
whose streams also carry `nolock` and `nolockbuf`.

Output is unaffected by the first defect: `num_put` writes base 10
in either state. The second affects output too. After `std::hex` and
`unsetf (basefield)`, an `ostringstream` writes 255 as `ff` in 11S;
libstdc++ writes `255`, the `%d` of an empty `basefield`.

## 6. The standard

C++03, 27.6.3, p5, defines `setbase` through a function `f`. The text
is unchanged in later drafts; N4296, [std.manip], reads:

```cpp
void f(ios_base& str, int base) {
  // set basefield
  str.setf(base == 8 ? ios_base::oct :
    base == 10 ? ios_base::dec :
    base == 16 ? ios_base::hex :
    ios_base::fmtflags(0), ios_base::basefield);
}
```

So `setbase (10)` sets `dec`, and `setf (0, basefield)` leaves
`basefield` empty whatever was set before. Stage 1 of `num_get`'s
conversion reads `basefield == dec` as `%d` and `basefield == 0` as
`%i`.

## 7. History

- The base bits, the setter's table and `__rw_setbase` are in the 2005
  import (e6c7726b). Their history before that is the vendor's.
- The original README documents the extension as the bases beyond the
  standard's (`_RWSTD_NO_EXT_SETBASE`, section 8.3.10). It says
  nothing of base 10 or of clearing `basefield`.
- The test came from the vendor's Perforce repository in May 2008,
  STDCXX-885: a36e09d2 on the release branch, 2d8421f8 on trunk. Its
  `setbase` check has compared flags since.
- The issue tracker and the development list archives have no issue
  or thread on either defect. The project went quiet a few weeks
  after the test arrived.

## 8. The resolution

### 8.1 A patch to the encoding, rejected

The defects can be patched in place, one change per job of the empty
row:

1. `__rw_setbase` sets the `basefield` bit of the base beside the base
   bits, for 2, 8, 10 and 16. The manipulator no longer depends on the
   empty row to complete a base.
2. The empty row stops completing bases. Base bits 2, 8 and 16 with
   `basefield` empty become 10, autodetection, as 0 does. An extended
   base stays.

A model of the setter and of `__rw_setbase`, the two functions
transcribed over plain `unsigned`, checked it. Its current rules
reproduce the library's measured flags on every row the probes cover.
Under the patched rules:

| path | today | patched | the standard |
|---|---|---|---|
| `setbase (10)` | autodetect | decimal | decimal |
| `setbase (0)` | autodetect | autodetect | autodetect |
| `setbase (8)`, `(16)` | 8, 16 | 8, 16 | 8, 16 |
| `setbase (2)`, `(36)` | 2, 36 | 2, 36 | autodetect |
| `hex`, `setf (0, basefield)` | 16 | autodetect | autodetect |
| `oct`, `setf (0, basefield)` | 8 | autodetect | autodetect |
| `setbase (8)`, `setbase (10)` | autodetect | decimal | decimal |

The patch fixes both defects and keeps the extension: bases 2 and 36
stay non-standard, and an extended base still cannot be cleared
through `basefield`, which is already empty. It also keeps the bits
`27.basic.ios` fails on.

### 8.2 The extension goes

The revival's target is the C++03 library, and the strict-ANSI entry
in `TODO` already lists the extended bases and `bin` for removal. The
extension is the storage both defects live in, and the bits that
break the postcondition of `init`. Patching the machinery and then
deleting it would do the work twice. Decided 2026-09-30: the extension
goes, `bin` included, as the first slice of that entry.

`setbase` becomes the standard's `f` (chapter 6): 8, 10 and 16 set
`oct`, `dec` and `hex`; every other value clears `basefield`, for
decimal output and prefix-dependent input. `basefield` carries the
base alone, and with the base bits go:

- the setter's reconciliation table; `flags (fmtflags)` stores what
  it is given;
- `ios_base::_C_base`, and the facets' reading of the base bits: they
  take the base from `basefield`;
- the Roman-numeral and arbitrary-base conversions of `num_get` and
  `num_put`;
- `ios_base::bin`, the manipulator `std::bin`, `_RWSTD_IOS_BIN`, and
  the macros `_RWSTD_NO_EXT_SETBASE` and `_RWSTD_NO_EXT_BIN_IO`.

Check: `27.std.manip` pins the standard's rule before the removal. For
each base from -2 to 37 in a sample it checks `basefield`, the
insertion of 1234 and the extraction of `"010"` and `"19"`, which
tell decimal, octal, hex and prefix-dependent input apart. For `hex`
and `oct` followed by each of the three ways to clear `basefield` it
checks `basefield`, the same extractions and the insertion of 255.
The expectations were run first against libstdc++, which meets all of
them. Against the library before the removal, 100 of the 120
assertions fail, each as chapters 3 and 4 predict.

Measured after the removal, 2026-09-30, from full rebuilds of the six
configurations. `27.std.manip` passes all 48 of its assertions.
`27.basic.ios` passes in the single-threaded builds. `22.locale.num.get`
and `22.locale.num.put` lose the cases of the removed bases, and the
driver's `0.printf` the cases that printed them. No other row moves,
and the warnings of the tests' own sources are unchanged.

`27.basic.ios` fails on the same bits for another reason: `flags ()`
returns them, where the standard's postcondition allows
`skipws | dec` only. `analysis-basic-ios-flags.md` has it.
