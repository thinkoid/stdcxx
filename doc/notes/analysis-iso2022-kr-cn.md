# ISO-2022-KR and ISO-2022-CN: what a converter takes

As of 2026-11-08 (68dc8ce5). Probes ran in the 15D build under GCC
16.2.1, against glibc 2.44 on x86-64.

`codecvt_byname<wchar_t, char, mbstate_t>` knows four ISO-2022
names. It converts two of them, `ISO-2022-JP` and `-JP-2`. The other
two, `ISO-2022-KR` and `ISO-2022-CN`, are recognized and then
dropped: every member answers `error` or 0. The decision is whether
to give the two names converters or to stop recognizing them. This
note gathers what that decision needs: what the two encodings are,
what `src/iso2022.cpp` already has, which character tables exist,
how glibc's `iconv` behaves as an oracle, what an implementation
costs, and what dropping the names loses. Line numbers are those of
68dc8ce5. The names were dropped from the facet in the commit that
adds this note, since no working conversion is lost and the C library
serves neither.

## 0. tl;dr

- **The encodings.** ISO-2022-KR (RFC 1557) is ASCII plus KS C 5601,
  designated once into G1 and switched in and out with SO and SI.
  ISO-2022-CN (RFC 1922) is ASCII plus GB 2312 or CNS 11643 plane 1
  in G1 under SO, and CNS plane 2 in G2 under the single shift SS2.
  Every CN line carries its own designations and ends in ASCII.
  None of SO, SI, G1, a two-byte G2 or the per-line rule occurs in
  ISO-2022-JP or -JP-2, so the existing converters never needed them.
- **The state can hold both; the code around it cannot.** The struct
  in the caller's `mbstate_t` has G0-G3, the element invoked in GL,
  and both single-shift flags, with 7 spare bits. But the escape
  parser takes only G0 and the 96-character G2, `in` passes SO and
  SI through as characters, a single shift is assumed to carry one
  byte, and the reset to the initial state forgets G1. `ESC $ ) C`
  is an error today, by probe.
- **The tables exist.** KS C 5601 and GB 2312 come from the `EUC-KR`
  and `GB2312` databases the JP-2 converters already use. CNS planes
  1 and 2 come from the `EUC-TW` database: the 15D `localedef` builds
  it from an empty locale definition in 10 s, 2.5 MB, with all 55570
  characters of the charmap, plane 1 in two bytes and planes 2-7 and
  15 in four (`8E A0+plane`). Its lookups convert both ways by probe.
- **glibc departs from RFC 1922.** Its ISO-2022-CN encoder writes
  SS2 (and SS3) only before the first of a run of characters from
  the same single-shift set, so its own decoder reads the second one
  as ASCII or as plane 1: U+4E42 U+4E42 round-trips to U+4E42 `!!`.
  Its ISO-2022-KR output conforms. Its decoders are lenient. An
  oracle must keep runs of plane-2 characters out of the fixtures it
  reads from `iconv`.
- **The cost.** About 450 lines in `src/iso2022.cpp`, 12 case arms in
  `src/wcodecvt.cpp`, and 300 to 400 lines of test, in two commits
  of the size of adc23505. KR is the smaller half. The risks are the
  shared functions the JP paths also run, the glibc departure, and
  the KR designator after `unshift`.
- **Dropping the names** removes two enumerators, two string tests
  and twelve case labels. The facet's constructor then throws
  `runtime_error`, as it does for `ISO-2022-CN-EXT`. Nothing that
  works today is lost; the C library cannot serve either name, since
  glibc has no locale and no charmap for any ISO-2022 encoding.

## 1. The two encodings

### 1.1 ISO 2022 in brief

ISO 2022 builds a 7-bit or 8-bit code from several character sets.
It has four slots, G0 to G3, each holding one set. A *designation*
is an escape sequence that puts a set into a slot: `ESC ( B` puts
ASCII into G0, `ESC $ ) C` puts the two-byte KS C 5601 into G1. The
intermediate bytes say which slot and what kind of set: `(` `)` `*`
`+` name G0 to G3 for a 94-character set, a `$` before them makes it
a 94x94 two-byte set, and `.` names G2 for a 96-character set.

A designation does not make a set current; an *invocation* does. In
a 7-bit code the current set is the one invoked into GL, the byte
range 0x21-0x7E. SI (0x0F) invokes G0 and SO (0x0E) invokes G1. Both
lock, so each holds until the other. SS2 (`ESC N`) and SS3
(`ESC O`) invoke G2 or G3 for the next character alone. A two-byte
set takes two GL bytes per character.

### 1.2 ISO-2022-KR, RFC 1557

RFC 1557 numbers no sections, so its headings are cited instead.

"Description":

> It is assumed that the starting code of the message is ASCII.
> ASCII and Korean characters can be distinguished by use of the
> shift function. For example, the code SO will alert us that the
> upcoming bytes will be a Korean character as defined in KSC 5601.
> To return to ASCII the SI code is used.

```
        SO           KSC 5601
        SI           ASCII
        ESC $ ) C    Appears once in the beginning of a line
                         before any appearance of SO characters.
```

"Formal Syntax", where "priority should be given to the formal
syntax" over the description:

```
   body            = *e-line *1( designator *( e-line / h-line ))
   designator      = ESC "$" ")" "C"
   e-line          = *text CRLF
   h-line          = *text 1*( segment *text ) CRLF
   segment         = SO 1*(one-of-94 one-of-94 SI
```

The parenthesis of `segment` is unclosed in the RFC. The rules that
bind a converter:

- One set beyond ASCII, KS C 5601, designated into G1 by
  `ESC $ ) C` and invoked by SO; SI returns to ASCII in G0.
- The designator occurs at most once in a body (`*1`), at the start
  of a line, before the first line that has SO. Lines of pure ASCII
  may precede it, and a body without Korean may omit it.
- Every segment closes with SI, so each line ends in ASCII. The
  designation is not repeated per line; it holds for the body.
- `text` excludes ESC, SI and SO, and the Korean bytes are 0x21-0x7E.

### 1.3 ISO-2022-CN and -CN-EXT, RFC 1922

Section 1.2 sets up three kinds of designation:

> There are three kinds of designations: SOdesignation,
> SS2designation and SS3designation.
>
> The SOdesignation is in the form ESC $ ) <F>, [...] The
> SS2designation is in the form ESC $ * <F>, and the SS3designation
> is in the form ESC $ + <F>. A designation overrides any previous
> designation for subsequent bytes in the text.

and the single shift:

> The shift SS2 (two bytes with hexadecimal values 1B 4E) declares
> that the subsequent TWO bytes are interpreted in the character set
> defined by SS2designation, after which the previous interpretation
> (from SI or SO) is restored.

Its table of sets for ISO-2022-CN:

```
     ESC $ ) A         Indicates the bytes following SO are Chinese
                       characters as defined in GB 2312-80, until
                       another SOdesignation appears

     ESC $ ) G         Indicates the bytes following SO are as defined
                       in CNS 11643-plane-1, until another
                       SOdesignation appears

     ESC $ * H         Indicates the two bytes immediately following
                       SS2 is a Chinese character as defined in CNS
                       11643-plane-2, until another SS2designation
                       appears
```

And the line rule:

> If there are any GB or CNS characters on a line, a designation for
> the corresponding character set must be used so that each line has
> its own character set information and the text can be displayed
> correctly when scroll back in a window. Also, there must be a shift
> to ASCII (SI) before the end of the line (i.e., before the CRLF).
> In other words, each line starts in ASCII, and ends in ASCII.

Section 7.1 makes the rule formal:

```
   c_line ::= *char 1*(1*designation 1*(*char 1*c_text *char)) CRLF
   SO-SI-segment ::= SO 1*c_char *designation *c_segment SI
   SS2segment  ::= SS2 c_char
```

Section 1.3 adds, for ISO-2022-CN-EXT, ISO-IR-165 under SO
(`ESC $ ) E`) and CNS planes 3 to 7 in G3 under SS3 (`ESC $ + I` to
`ESC $ + M`). It reserves finals for GB 7589, 7590, 12345, 13131 and
13132, sets that "shall only be used once these final characters are
assigned". It repeats the line rule: "each line starts in ASCII, and
ends in ASCII, and has its own designation information before any
Chinese characters appear."

The rules that bind a converter:

- G1 holds GB 2312 or CNS plane 1 and may change within a line, as
  the example of section 1.2 does (`ESC $ ) A SO ... ESC $ ) G ...
  SI`). G2 holds CNS plane 2. Under -CN-EXT, G3 holds one of CNS
  planes 3 to 7.
- SS2 and SS3 cover one two-byte character each (`SS2segment ::= SS2
  c_char`); the shift state before them resumes after it.
- Every line with Chinese designates its sets before their first
  character on that line, and every line ends in SI. The RFC does
  not say that a reader forgets designations at a line end; it says
  that a writer must not rely on the previous line's. A writer thus
  forgets them at each line end, and a reader may keep them.

### 1.4 What the JP converters never needed

| feature | ISO-2022-JP, -JP-2 | ISO-2022-KR | ISO-2022-CN |
|---|---|---|---|
| sets in G0 | every set | ASCII | ASCII |
| G1, SO and SI | no | KS C 5601 | GB 2312, CNS plane 1 |
| G2 | -JP-2: ISO-8859-1 and -7, one byte, `ESC . F` | no | CNS plane 2, two bytes, `ESC $ * H` |
| G3 and SS3 | no | no | -CN-EXT only: CNS planes 3-7 |
| designation per line | no | no, once per body | yes |
| text ends in | `ESC ( B` | SI | SI on every line |

## 2. What `src/iso2022.cpp` has

### 2.1 The state

`__rw_iso2022_state_t` (139-153) lives in the caller's `mbstate_t`
since 68dc8ce5. It is six bytes of `unsigned char`: `gl_map:3`,
`gr_map:3`, `shift_state:1` and `sshift2:1` fill the first byte,
`sshift3:1` starts the second, and `g_map [4]` takes the last four.
glibc's `mbstate_t` is eight bytes, so 7 bits and 2 bytes are spare.

| field | meaning | written by the JP code |
|---|---|---|
| `gl_map` | element invoked in GL, 1-4 for G0-G3 | always 1 |
| `gr_map` | element invoked in GR | never |
| `shift_state` | width of the invoked set less one | yes |
| `sshift2` | SS2 pending | yes, for a one-byte G2 |
| `sshift3` | SS3 pending | cleared, never set |
| `g_map [0..3]` | registration of the set in G0-G3 | `[0]` and `[2]` only |

The struct thus already expresses SO (`gl_map` 2) and SI (`gl_map`
1), a set in G1 and in G3, and SS3. The per-line rule needs no field
either: `out` clears `g_map [1..3]` at a newline, so the next line
designates anew. A KR "designator written" mark is `g_map [1]`
itself, provided the reset of 2.5 keeps it.

### 2.2 The tables

- `__rw_chset_map` (181-338) is indexed by the ISO-IR registration
  number of a set, not by the final byte of its designation. A row
  holds the database name, the database index, the width less one,
  and the designating sequence `out` writes. Rows exist for 6
  (ASCII), 13 and 14 (JIS X 0201), 42 and 87 (JIS X 0208), 58
  (GB 2312), 100 (ISO-8859-1), 126 (ISO-8859-7), 149 (KS C 5601)
  and 159 (JIS X 0212).
- Four functions map a final byte to a registration, one per kind of
  set (466-535). `__rw_mb94_encoding_reg` knows `@` to `D`: JIS X
  0208-1978, GB 2312, JIS X 0208-1983, KS C 5601 and JIS X 0212.
- `__rw_db_map` (342-359) names 12 databases in 32 slots; slot 13 is
  free. `__rw_get_encoding_database` (591-609) maps one from the
  locale root on first use, under a static lock, and publishes its
  pointer with a release store.

KS C 5601 and GB 2312 thus have registrations, rows and databases.
CNS 11643 has none. Its registrations are ISO-IR 171 and 172 for
planes 1 and 2 (finals `G` and `H`) and 183 to 187 for planes 3 to 7
(`I` to `M`); ISO-IR-165 is 165 (`E`). All fit the 256-row table
and the `unsigned char` of `g_map`.

### 2.3 The input path

- `__rw_iso2022_escape` (751-826) reads `ESC N`, `ESC ( F` into G0,
  `ESC . F` into G2, and `ESC $ F` or `ESC $ ( F` into G0 (784-794).
  `ESC $ )`, `ESC $ *`, `ESC $ +` and `ESC O` are errors. After
  `ESC $` the function skips a `(` only, so for `ESC $ ) C` it looks
  `)` up as a final byte and finds nothing. Only ISO-2022-JP filters
  the sets (809-814); -JP-2 takes any set the lookup knows.
- `__rw_iso2022_to_interm` (1353-1436) leaves control bytes alone
  (1383-1385, a FIXME), so SO and SI convert as U+000E and U+000F.
  It takes the set from `g_map [0]`, or from `g_map [2]` under SS2
  (1389-1397).
- `__rw_iso2022_to_ucs4` (1575-1673) takes a single-shift character
  as one byte (1590). -JP-2's 96-character G2 needs that; CN's
  two-byte plane 2 does not.
- `__rw_iso2022_to_gb2312` (1179-1196) and `__rw_iso2022_to_ksc5601`
  (1206-1223) set the high bit of two bytes, which makes them EUC.
  They do not depend on the slot, so they serve KR and CN as they
  are.

The probe of 2.6 confirms the escape: `in` under `ISO-2022-JP-2` of
`ESC $ ) C SO 30 21 SI` answers `error`.

### 2.4 The output path

- `__rw_ucs4_to_interm` (1496-1571) tries six databases in a fixed
  order, JP-2's: ASCII, EUC-JP, EUC-KR, GB2312, ISO-8859-7 and
  ISO-8859-1 (1502-1512). KR needs ASCII and EUC-KR; CN needs ASCII,
  GB2312 and EUC-TW, in glibc's order (chapter 4).
- `__rw_ksc5601_to_iso2022` (891-937) and `__rw_gb2312_to_iso2022`
  (946-992) test `g_map [0]` and designate through
  `__rw_iso2022jp_designate` (616-744), which writes the G0 forms
  `ESC $ ( C` and `ESC $ A`. KR and CN designate into G1 and invoke
  with SO, so neither function serves them; their byte transform,
  `& 0x7f`, is one line.
- `__rw_iso2022jp_do_unshift` (1808-1835) writes `ESC ( B`. KR and
  CN write SI.

### 2.5 The reset

`__rw_reset_iso2022_state` (392-414) zeroes the state on a return to
the initial shift state: ASCII invoked, nothing in G2, no single
shift pending. It does not look at `g_map [1]` or `g_map [3]`, which
the JP code never sets. Run as it is, it would zero a KR or CN state
in SI with a set in G1 at the end of every call. `in` would then
lose the designation, so a later SO on the same line would fail, and
KR's `out` would write its designator again in every call. The
function's check must thus count G1 and G3 among the designations
that keep a state non-zero, as adc23505 did for G2. `__rw_get_iso2022_state` (419-460)
initializes a zero state for the two JP types only (449-457) and
needs the two new cases.

### 2.6 The facet today

`__rw_encoding_from_name` (363-387) returns `iso2022_kr` for
`ISO-2022-KR` and `iso2022_cn` for `ISO-2022-CN` (380-385; the
enumerators are in `src/iso2022.h` 47-48). The six switches of
`codecvt_byname<wchar_t, char, mbstate_t>` in `src/wcodecvt.cpp`
list both before `default: break;` (1214, 1296, 1364, 1426, 1499,
1546). Probe, a facet installed in the classic locale: `out` of
`{a, U+AC00}`, then `in` and `length` of `ESC $ ) C SO 30 21 SI`:

```
ISO-2022-KR      encoding 0 max_length 0 out error consumed 0 wrote 0 in error length 0
ISO-2022-CN      encoding 0 max_length 0 out error consumed 0 wrote 0 in error length 0
ISO-2022-CN-EXT  threw .../src/setlocale.cpp:117: ... bad locale name: "ISO-2022-CN-EXT"
no-such-encoding threw .../src/setlocale.cpp:117: ... bad locale name: "no-such-encoding"
```

`encoding` 0 claims a variable width and no state. 22.2.1.5.2 p7
asks for -1 "if the encoding of the externT sequence is
state-dependent", so the answer is wrong as well as useless. The
shipped reference, `doc/stdlibref/codecvt-byname.html`, lists
neither name in its table of codesets.

## 3. The character sets and their databases

### 3.1 KS C 5601 and GB 2312

ISO-2022-KR's two bytes, and ISO-2022-CN's under `ESC $ ) A`, are
the EUC bytes less the high bit. The converters of 2.3 add it back
and look the pair up in the `EUC-KR` or `GB2312` database; `out`
does the reverse. Both databases build from `etc/nls/charmaps` in
under a second, and the iso2022 test builds them already.

### 3.2 CNS 11643 through EUC-TW

`etc/nls/charmaps/EUC-TW` is glibc's charmap, byte for byte (`cmp`
against `/usr/share/i18n/charmaps/EUC-TW.gz`). Its census, by the
form of the bytes:

| form | plane | characters |
|---|---|---|
| one byte | ASCII | 128 |
| `A1-FE A1-FE` | 1 | 5867 |
| `8E A2 A1-FE A1-FE` | 2 | 7650 |
| `8E A3` to `8E A7`, then two bytes | 3 to 7 | 6394, 7286, 8601, 6386, 6537 |
| `8E AF`, then two bytes | 15 | 6721 |

The charmap comments out the four-byte form of plane 1, `8E A1`,
with `%IRREVERSIBLE%`, so the database reads plane 1 in two bytes
only. One code point has two encodings: U+5344 is plane 1 `A4 BF`
and plane 3 `8E A3 A1 B8`.

The database builds the way `22.locale.codecvt.iso2022` builds its
six, through `rw_localedef` in `tests/src/locale.cpp` (`localedef -w
-c -f <charmap> -i <empty source> <root>/<name>`):

```
$ : > root/empty.src
$ localedef -w -c -f etc/nls/charmaps/EUC-TW -i root/empty.src root/empty.EUC-TW
rc=0, 10.15 s                  (EUC-KR 0.57 s, GB2312 0.36 s)
$ ls -l root/EUC-TW
2522822 bytes                  (EUC-KR 756723, GB2312 531737)
```

The build's `locale` utility dumps it (`RWSTD_LOCALE_ROOT=root
LC_ALL=empty.EUC-TW locale --charmap`). The dump has the census of
the charmap, 55570 characters, plane 1 in two bytes and the others
behind `8E A0+plane`:

```
<code_set_name> EUC-TW
<mb_cur_max>    4
<U0000>..<U007F>            \x00   # L'\x00' (128 characters)
<U4E42>                     \x8e\xa2\xa1\xa1   # L'\x4e42'
<U6C3C>                     \x8e\xa3\xa4\xa1   # L'\x6c3c'
<U00020086>                 \x8e\xa4\xa1\xa1   # L'\x20086'
<U5344>                     \xa4\xbf   # L'\x5344'
```

`codecvt_byname ("EUC-TW")` over that database, `in` and then `out`
of what `in` produced:

```
e9 c7          -> U+6F22,  out e9 c7          plane 1
a4 bf          -> U+5344,  out a4 bf          plane 1
8e a3 a1 b8    -> U+5344,  out a4 bf          plane 3, written as plane 1
8e a1 e9 c7    -> error                       plane 1, four-byte form
8e a2 a1 a1    -> U+4E42,  out 8e a2 a1 a1    plane 2
8e a3 a4 a1    -> U+6C3C,  out 8e a3 a4 a1    plane 3
8e a4 a1 a1    -> U+20086, out 8e a4 a1 a1    plane 4
8e a7 a1 a1    -> U+20055, out 8e a7 a1 a1    plane 7
8e af a1 a1    -> U+20002, out 8e af a1 a1    plane 15
```

A converter thus maps ISO-2022-CN onto the database without a table
of its own. A plane-1 character, two GL bytes `b1 b2` under SO after
`ESC $ ) G`, becomes `b1|80 b2|80`. A plane-2 character, after
`ESC $ * H` and SS2, becomes `8E A2 b1|80 b2|80`. `out` reads the
plane from the lead bytes of the database's answer and rejects any
plane ISO-2022-CN does not carry. The database writes U+5344 as
plane 1, as glibc does.

### 3.3 What -CN-EXT would add

Planes 3 to 7 are in the same database, behind `8E A3` to `8E A7`,
so SS3 and G3 cost no new table. RFC 1922 designates no plane 15, so
`out` would reject it. ISO-IR-165 has no charmap in
`etc/nls/charmaps` or in `/usr/share/i18n/charmaps`; glibc carries
its table inside the ISO-2022-CN-EXT module only, and `iconv -t
ISO-IR-165` answers "not supported". An `ESC $ ) E` would thus stay
an error. `ISO-2022-CN-EXT` is not even a recognized name today: the
name test compares the rest after `ISO-2022-` with `CN` exactly, so
the longer name takes the unknown-name path of 2.6.

## 4. glibc's `iconv`

`iconv -l` lists `ISO-2022-KR`, `ISO-2022-CN`, `ISO-2022-CN-EXT`
and `EUC-TW`. The characters below come from `iconv -t EUC-TW` and
`-t GB2312`: U+4E1C is in GB 2312 alone, U+6F22 in CNS plane 1 alone
(`E9 C7`), U+4E42 in CNS plane 2 (`8E A2 A1 A1`) and in no GB 2312
row, and U+6C3C in CNS plane 3.

### 4.1 ISO-2022-KR

```
a U+AC00 b LF U+AC01 c LF
1b 24 29 43 61 0e 30 21 0f 62 0a 0e 30 22 0f 63 0a
ESC $  )  C  a  SO 0  !  SI b  LF SO 0  "  SI c  LF

"ab"                1b 24 29 43 61 62
""                  1b 24 29 43
U+AC00              1b 24 29 43 0e 30 21 0f
U+AC00 LF           1b 24 29 43 0e 30 21 0f 0a
U+AC00 SP U+AC01    1b 24 29 43 0e 30 21 0f 20 0e 30 22 0f
```

glibc writes the designator first in every conversion, before any
text and even for no text. It writes SI before any ASCII byte, so SI
precedes every LF, and a space between two Korean characters leaves
SO. The output conforms to RFC 1557's syntax.

### 4.2 ISO-2022-CN

```
a U+4E1C LF U+6F22 U+4E42 b LF
61 1b 24 29 41 0e 36 2b 0f 0a 1b 24 29 47 0e 69 47 1b 24 2a 48 1b 4e 21 21 0f 62 0a
a  ESC $  )  A  SO 6  +  SI LF ESC $  )  G  SO i  G  ESC $  *  H  ESC N  !  !  SI b  LF

U+4E1C LF U+4E1C        1b 24 29 41 0e 36 2b 0f 0a 1b 24 29 41 0e 36 2b 0f
U+4E1C CR LF U+4E1C     1b 24 29 41 0e 36 2b 0f 0d 0a 1b 24 29 41 0e 36 2b 0f
U+4E2D                  1b 24 29 41 0e 56 50 0f    (GB 2312 and CNS plane 1)
U+6F22 U+4E42 U+6F22    1b 24 29 47 0e 69 47 1b 24 2a 48 1b 4e 21 21 69 47 0f
```

glibc designates each set before its first character on a line,
forgets the designations at LF so that the next line designates
again, and closes every line with SI. It tries GB 2312 first, then
plane 1, then plane 2, so U+4E2D, in both GB 2312 and plane 1, is
written in GB 2312. After SS2 inside SO, the next plane-1 character is read in SO again.
All of this follows RFC 1922.

### 4.3 Where glibc departs

**A single shift that locks.** RFC 1922 section 7.1 makes SS2 cover
one character (`SS2segment ::= SS2 c_char`). glibc's encoder writes
SS2 only before the first character of a run from G2, and writes the
later ones as if G2 were invoked:

```
U+4E42 U+4E42                 1b 24 2a 48 1b 4e 21 21 21 21 0f
U+6F22 U+4E42 U+4E42          1b 24 29 47 0e 69 47 1b 24 2a 48 1b 4e 21 21 21 21 0f
U+4E42 U+6F22 U+4E42 U+4E42   1b 24 2a 48 1b 4e 21 21 1b 24 29 47 69 47 1b 4e 21 21 21 21 0f
```

Read by the RFC, the second `21 21` is `!!` in ASCII, or U+3000 of
plane 1 under SO. In the third line `69 47` follows no SO, so it is
`iG`. glibc's own decoder reads its output that way, so a round trip
through `iconv` alone corrupts the text:

```
U+4E42 U+4E42                -> ISO-2022-CN -> U+4E42 ! !
U+6F22 U+4E42 U+4E42         -> ISO-2022-CN -> U+6F22 U+4E42 U+3000
U+4E42 U+6F22 U+4E42 U+4E42  -> ISO-2022-CN -> U+4E42 i G U+4E42 ! !
```

SS3 under -CN-EXT behaves the same: U+6C3C U+6C3C gives `1b 24 2b 49
1b 4f 24 21 24 21 0f`, which reads back as U+6C3C `$!`.

**SI without SO.** After a plane-2 character in ASCII, glibc writes
SI although it wrote no SO: U+4E42 `a` U+4E42 gives `1b 24 2a 48 1b
4e 21 21 0f 61 1b 4e 21 21 0f`. SI in ASCII changes nothing, but the
grammar of section 7.1 has SI only at the end of an SO-SI-segment.

**Lenient decoders.** glibc reads text the RFCs exclude:

| input | glibc reads |
|---|---|
| CN, line 2 uses SO with no designation of its own | the designation of line 1 |
| CN, `SO 36 2b SI` with no SO designation | GB 2312, U+4E1C |
| CN, `ESC N 21 21` with no SS2 designation | CNS plane 2, U+4E42 |
| CN, `ESC $ + I ESC O 24 21` under plain -CN | the escape bytes as characters |
| KR, `SO 30 21 SI` with no designator | KS C 5601, U+AC00 |
| KR, the designator after text on its line | accepted |
| KR, `ESC $ ) A SO 30 21 SI` | the escape bytes, then U+AC00 |
| KR or CN, LF inside SO | error |
| KR, a space inside SO | error |

### 4.4 What this means for an oracle

The iso2022 test checks two directions against `iconv`: glibc reads
the facet's output back to the original, and the facet reads glibc's
output back to the original
(`tests/localization/22.locale.codecvt.iso2022.cpp` 358-375). It
compares no bytes. For KR both checks hold. For CN the first holds
for any output that follows the RFC. The second fails for any
fixture with two characters from G2 in a row, since glibc's bytes no
longer encode the fixture; such a fixture either skips that check or
pins the departure. The lenient rows of 4.3 are no guide for `in`:
the facet decides for itself whether to accept them.

## 5. The pieces of an implementation

### 5.1 The pieces

Shared by KR and CN, in `src/iso2022.cpp`:

| piece | function | size |
|---|---|---|
| designations into G1, into G2 as a 94x94 set and into G3; SS3; the sets each encoding admits, so that -JP-2 does not start taking CNS | `__rw_iso2022_escape` | 40 lines |
| SO and SI invoke G1 and G0; the set comes from `g_map [gl_map - 1]`; SS3 reads G3 | `__rw_iso2022_to_interm` | 25 lines |
| a single shift is as wide as the set in the shifted slot | `__rw_iso2022_to_ucs4` | 5 lines |
| zero-state initialization for both types | `__rw_get_iso2022_state` | 2 lines |
| G1 and G3 keep a state non-zero, as G2 does | `__rw_reset_iso2022_state` | 10 lines |
| designation into G1 with SO and SI, into G2 with SS2 per character, into G3 with SS3 per character | a new designate function beside `__rw_iso2022jp_designate` | 60 lines |
| the database list as a parameter: JP-2's six, KR's two, CN's three | `__rw_ucs4_to_interm` | 15 lines |
| `in` and `length` through the existing loops; `out` per encoding; `unshift` writing SI; `encoding` -1 | new `__rw_iso2022kr_do_*` and `__rw_iso2022cn_do_*`, declared in `src/iso2022.h` | 150 lines |
| twelve case arms | the six switches of `src/wcodecvt.cpp` | 30 lines |

KR alone:

- `out` writes `ESC $ ) C` before the first character of a
  conversion, as glibc does, and keeps KS C 5601 in G1 after it.
- `in` admits `ESC $ ) C` alone. It decides whether SO without the
  designator is an error, as RFC 1557 has it, or KS C 5601, as glibc
  reads it.
- `max_length` is 7: the designator, SO and two bytes, by the rule of
  `ref-locale.md` 9.2 ("a designation and the longest character
  after it").

CN alone:

- Rows for ISO-IR 171 and 172 in `__rw_chset_map`, finals `G` and
  `H` in `__rw_mb94_encoding_reg`, and `DBIDX_EUC_TW` in slot 13 of
  `__rw_db_map`: 20 lines.
- `__rw_iso2022_to_euctw` for `in` and `__rw_euctw_to_iso2022` for
  `out`, as 3.2 describes them: 60 lines.
- `out` writes SI and clears `g_map [1..3]` at LF: 10 lines.
- `in` decides whether a designation survives LF; glibc keeps it.
- `max_length` is 8: `ESC $ * H`, `ESC N` and two bytes.

-CN-EXT, if wanted: a fifth name in `__rw_encoding_from_name` and
the enum, rows 183-187 and finals `I` to `M`, and SS3 in `out`, 40
lines. ISO-IR-165 stays an error for want of a table.

Tests, in `22.locale.codecvt.iso2022` or a sibling of its shape:

- `test_in`, `test_out` and their splitting and windowing take any
  encoding. `test_out_error`, `test_unshift_length`,
  `test_copied_state` and `test_held_states` are written around JP
  characters and bytes (U+3042, `ESC ( B`, `ESC $ B`, `ESC . A`) and
  need per-encoding variants.
- The wide fixtures for U+4E1C and for U+AC00 U+AC01 serve as they
  are. RFC 1922's own example, `1b 24 29 41 0e 3d 3b 3b 3b 1b 24 29
  47 47 28 5f 50 0f`, is a byte fixture. A run of plane-2 characters
  pins 4.3, and lines ending in LF pin the per-line rule.
- `run_test` builds `EUC-TW` as well, 10 s, where the test now runs
  in 1.7 s. The pin row moves.
- 300 to 400 lines in all.

Documents: two rows in the reference page's table of codesets, and
its NOTE on the databases each encoding needs; `ref-locale.md` 9.2,
9.3 and 9.4; the `TODO` entry leaves.

The databases a user builds: `ANSI_X3.4-1968` and `EUC-KR` for KR;
`ANSI_X3.4-1968`, `GB2312` and `EUC-TW` for CN. Each comes from an
empty locale definition, as the reference page already explains.

### 5.2 The cost

About 450 lines in `src/iso2022.cpp`, 30 in `src/wcodecvt.cpp` and
`src/iso2022.h`, and 300 to 400 in the test. adc23505 changed 269
lines of the library and added 811 of test, so the work is two
commits of that order. KR comes first, since CN builds on its G1, SO
and SI.

### 5.3 The risks

- The escape parser, the reset and `__rw_ucs4_to_interm` are shared
  with the JP paths, so a change there can move JP behaviour. The
  1500 assertions of the iso2022 test guard it.
- The CN oracle cannot compare against glibc's output for runs of G2
  or G3 characters (4.3).
- KR's designator and `unshift`. Footnote 226 to 22.2.1.5.2 p5 says
  the unshift characters "typically" return "the state to stateT()".
  A zero state after `unshift` makes the next `out` write the
  designator again. `basic_filebuf` calls `unshift` at `close` and
  on a seek after output (27.8.1.3 p6, 27.8.1.4 p11-12), so a file
  written, sought and written again carries the designator twice.
  RFC 1557's syntax allows it once; glibc reads both. Keeping G1
  across `unshift` keeps one designator but leaves a non-zero state.
- The single-shift width at 1590 holds for -JP-2's one-byte G2 by
  assumption. Reading it from the set changes a line every -JP-2
  input runs.
- `EUC-TW` takes 10 s to build in 15D, so a CN test runs about seven
  times longer than the JP one.

## 6. The alternative: dropping the names

### 6.1 How an unknown name is handled

86fbc909 did this for the UTF-16 names. 22.1.1.1.2 p4 gives a byname
facet "the virtual function semantics equivalent to that facet of
the locale constructed by locale(const char*) with the same name",
and no locale has either name. The facet thus takes each as it
takes any name the library does not know (86fbc909). The path is the
stateless branch of `ref-locale.md` 9.2: the constructor finds no
database of the name at the locale root, so `USE_LIBC` holds and its
`__rw_setlocale` throws `runtime_error`. `ISO-2022-CN-EXT` and
`no-such-encoding` take that path (2.6).

### 6.2 What the change is

- `src/iso2022.h`: the enumerators `iso2022_kr` and `iso2022_cn`
  (47-48) leave.
- `src/iso2022.cpp`: the two `strcmp` arms (380-385) leave.
- `src/wcodecvt.cpp`: the twelve case labels (1214-1215, 1296-1297,
  1364-1365, 1426-1427, 1499-1500, 1546-1547) leave.
- `22.locale.codecvt`: the two names join the unknown names of the
  row 86fbc909 added.
- `ref-locale.md` 9.2 and 9.3 lose the two names, and the `TODO`
  entry leaves.

The facet's constructor then throws instead of the members answering
`error` and 0.

### 6.3 What a user loses

Nothing that works. Today the facet converts no character in either
direction and misstates `encoding`; after the change it says at once
that it does not know the name. What leaves is the prospect of
reading or writing RFC 1557 or RFC 1922 text through a wide stream.
A program that needs it can call `iconv`, which knows all three
encodings (chapter 4).

The C library cannot serve either name. The facet's C-library path
runs `mbrtowc` and `wcrtomb` under a locale of the name, and glibc
has none:

```
$ locale -a | grep -ic 2022
0                                    (of 889 locales)
$ ls /usr/share/i18n/charmaps | grep -i 2022
                                     (none of 233 charmaps)
setlocale (LC_CTYPE, "ko_KR.ISO-2022-KR") = null
setlocale (LC_CTYPE, "ISO-2022-KR")       = null
setlocale (LC_CTYPE, "zh_CN.ISO-2022-CN") = null
setlocale (LC_CTYPE, "ISO-2022-CN")       = null
setlocale (LC_CTYPE, "ko_KR.EUC-KR")      = non-null
```

Neither `localedef` can build such a locale. A charmap gives each
character a fixed byte sequence and has no notation for SO, SI or a
designation, so no charmap describes an ISO-2022 encoding. For the
same reason no database named `ISO-2022-KR` can exist for the
stateless path either.

## 7. Open choice

The decision is whether `codecvt_byname<wchar_t, char, mbstate_t>`
converts `ISO-2022-KR` and `ISO-2022-CN`, or stops recognizing the
two names.

**Implement.** The two names convert through `src/iso2022.cpp` over
the `EUC-KR`, `GB2312` and `EUC-TW` databases, which the 15D
`localedef` builds from the shipped charmaps. The work is two
commits of the order of adc23505, and it touches functions the JP
converters share. The oracle is glibc's `iconv`, except where its
ISO-2022-CN encoder departs from RFC 1922. The KR designator after
`unshift` and the reading of a line end on input are policies the
implementation fixes. `ISO-2022-CN-EXT` stays unknown unless it is
taken up as well, and its ISO-IR-165 has no table.

**Drop.** The two names leave the constructor's name test, the enum
and the switches, as the UTF-16 names left in 86fbc909. The facet's
constructor then throws, as it does for any name the library does
not know. No working conversion is lost, and the C library offers
none. ISO-2022-JP and -JP-2 remain the only ISO-2022 encodings the
library converts.
