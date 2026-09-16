# The input files under tests/etc

Two locale tests read fixtures from `tests/etc`, a directory that no
branch of the repository ever had: the tests were migrated from the
vendor's repository in 2008 and the files stayed behind. This note
records what the tests expect of the files, how the eleven files now
in the tree were made, and what running them found.

## 0. tl;dr

- `22.locale.codecvt` reads five files whose byte and character
  counts its table fixes; `22.locale.collate` reads six word lists
  and asserts that the library sorts each list into the order it is
  in. The files are the assertion.
- The two ISO-8859-1 codecvt files are prose written for the
  purpose, in German and French, sized to the byte. The three
  Japanese files list the JIS X 0208 repertoire of the tree's own
  charmaps, in Shift_JIS, EUC-JP (with JIS X 0212) and UTF-8, laid
  out to meet the counts exactly.
- The six collate lists are words chosen to exercise each locale's
  rules, ordered by glibc's `localedef` and `sort` run over the
  tree's own locale definitions: the same definition, an independent
  implementation.
- Running them found three defects in the codecvt test, repaired in
  the same change, and one in the library: the narrow collate
  transform cannot spell a weight that is a multiple of 127, so the
  letter "l" in Croatian and the letter ko kai in Thai sort last.
  That is a `TODO` entry. The other four lists sort as glibc sorts
  them, including Czech "ch" and Danish "aa".

## 1. What the tests read

### 1.1 `22.locale.codecvt`

The table in the test names five locales, each built on the fly from
`etc/nls/src` and `etc/nls/charmaps` by `../bin/localedef`, and for
each the file `tests/etc/codecvt.<locale>.in`, its size in bytes,
its size in characters, and the values `encoding ()` and
`max_length ()` must return:

| locale | bytes | characters | encoding | max_length |
|---|---|---|---|---|
| `de_DE.ISO-8859-1` | 6826 | 6826 | 1 | 1 |
| `fr_FR.ISO-8859-1` | 3920 | 3920 | 1 | 1 |
| `ja_JP.Shift_JIS` | 25115 | 13001 | 0 | 2 |
| `ja_JP.EUC-JP` | 20801 | 14299 | 0 | 3 |
| `ja_JP.UTF-8` | 25056 | 12000 | 0 | 6 |

The file is read whole, in binary. Four facets convert it: the
narrow `codecvt<char, char>` and its `_byname` form must answer
`noconv`; the classic `codecvt<wchar_t, char>` must widen every byte
to one character, `ok` with the byte count; the `codecvt_byname
<wchar_t, char>` of the locale must produce the character count,
convert back to the byte count, and round-trip byte for byte. Then
`unshift`, `always_noconv`, `max_length`, `length` and `encoding`
are checked against the table. The same file is converted once more
through the facet constructed with the `@UCS-4` and `@UCS`
modifiers. Where the C library has a locale of the same name, every
conversion is also compared against `mbstowcs`, `wcstombs` and
`iconv`; on a machine with only a UTF-8 locale installed, which is
the common case now, those comparisons are skipped without a
diagnostic.

The counts are the old files' only surviving trace. Keeping them
costs nothing and leaves the test as it was.

### 1.2 `22.locale.collate`

The table names six locales, their charmaps and the files
`tests/etc/collate.<locale>.in`. Each locale is built with
`localedef -w --no_position`. The test reads the file line by line
(at most 1000 lines of under 256 bytes), converts each line through
the locale's `codecvt` for the `wchar_t` run, bubble-sorts the lines
with `collate::compare` and asserts that the result equals the file.
The comment in the test speaks of randomizing the words first; the
code does not, and does not need to: a sorted file passes only if
`compare` agrees with every adjacent pair, and one disagreement
moves a word and fails every line after it.

## 2. The codecvt files

### 2.1 ISO-8859-1

`codecvt.de_DE.ISO-8859-1.in` and `codecvt.fr_FR.ISO-8859-1.in` are
texts about the test that reads them, in the language of the locale,
written for this purpose and quoting nothing. Each uses the upper
half of the table where the language does: umlauts and the sharp s,
accents and cedillas, guillemets, the degree sign, the fraction and
the multiplication sign, which are the bytes a second implementation
could disagree about. Each is sized to its byte count; the closing
sentence is the adjustment. An edit must keep the size or change the
table.

### 2.2 Japanese

A Japanese text of twelve thousand characters was not going to be
written for a fixture, and the three counts do not describe one
text: they are three different documents. The files instead list
the repertoire of the charmaps that build the locales, which makes
their provenance self-evident and every character certain to be in
the table.

The repertoire is JIS X 0208 rows 4 to 8 and 16 to 84 in the order
of `etc/nls/charmaps/Shift_JIS`: hiragana, katakana, Greek,
Cyrillic, box drawing and the two kanji levels, 6670 characters.
Rows 1 to 3 and the vendor rows are left out: the symbol rows are
where Shift_JIS tables differ (the wave dash, the minus sign, the
currency signs), and a comparison against a C library's conversion,
where one is installed, should compare characters both tables
agree on.

Each file is a header line naming the construction, then lines of
the form `U+XXXX ` followed by a run of characters and a newline,
the label being the code point of the first character on the line.
The single-byte count fixes the number of lines, eight bytes per
line plus the header; the multibyte count is spread over the lines
as evenly as possible; the repertoire restarts when exhausted.

The arithmetic, with x the single-byte characters:

- Shift_JIS, every repertoire character two bytes: x + y = 13001
  and x + 2y = 25115 give y = 12114 and x = 887.
- EUC-JP, JIS X 0208 two bytes and JIS X 0212 three (SS3): x + y2 +
  y3 = 14299 and x + 2 y2 + 3 y3 = 20801 leave one degree of
  freedom, y2 + 2 y3 = 6502. The file takes y3 = 500 of JIS X 0212
  rows 16 to 77, so that the three-byte form is exercised, hence y2
  = 5502 and x = 8297.
- UTF-8, the same repertoire restricted to the characters that are
  three bytes in UTF-8 (Greek and Cyrillic are two): x + 3z = 25056
  and x + z = 12000 give z = 6528 and x = 5472.

The header is worded so that the single-byte count less its length
is a multiple of eight.

## 3. The collate files

### 3.1 The order is glibc's

A list sorted by the library itself would test nothing. The order in
each file comes from glibc's `localedef` compiling the tree's own
definition and charmap, and glibc's `sort` under the result:

```sh
I18NPATH=etc/nls/src localedef -c -i etc/nls/src/da_DK \
    -f etc/nls/charmaps/ISO-8859-1 <dir>/da_DK.ISO-8859-1
LOCPATH=<dir> LC_ALL=da_DK.ISO-8859-1 sort < words > collate.da_DK.in
```

Same `LC_COLLATE` source, a second implementation of it; where the
two disagree, one of them is wrong about the definition.

Two things to know when repeating this. glibc 2.44 refuses the
tree's `LC_IDENTIFICATION` sections and one line of the tree's 2005
`iso14651_t1` (a syntax error at its line 566, in the list of
ignorable punctuation) and needs `-c` to write the output anyway;
the collation it writes sorts as expected for letters, and the lists
contain nothing but letters. And the test builds the tree's locales
with `--no_position`, which turns a `forward,position` level into
`forward`; that changes the order only for strings that differ in
ignorable characters, which the lists do not contain.

### 3.2 The lists

Each list has between 55 and 73 words, in the charmap of its
locale, one per line, chosen for the rules the locale states:

- `cs_CZ`, ISO-8859-2: the letters with caron sort after their base
  letter (c < č, r < ř, s < š, z < ž), "ch" is a letter between h and
  i, accents are secondary.
- `da_DK`, ISO-8859-1: æ, ø and å follow z, and "aa" sorts as å.
- `en_US`, ISO-8859-1: case is secondary, lowercase first; a few
  accented loanwords.
- `hr_HR`, ISO-8859-2: č, ć, đ, š and ž after their base letters,
  and the digraphs dž, lj and nj are letters after d, l and n.
- `sv_SE`, ISO-8859-1: å, ä and ö follow z, and w sorts with v.
- `th_TH`, TIS-620: the leading vowels sort after the consonant they
  precede, which the definition spells as collating elements.

### 3.3 What the library says

Four lists sort under the library exactly as under glibc. Croatian
and Thai do not, in the narrow facet only, and for one reason,
chapter 4.4.

## 4. Findings

### 4.1 `length` compared against a C locale that was not set

`test_libstd_do_length` compared the facet's `length` against
`mbrlen` whenever the C-library comparison was not disabled, without
setting the C locale; the helpers for `in` and `out` set it and skip
the comparison when the locale does not exist. The comparison ran in
whatever locale the process was in and stopped at the first byte
above 0x7F: 19 for the German file, the header for the Japanese
ones. Repaired: the same guard as the other two helpers.

### 4.2 `length` counts bytes

The table's expectation for `length` on the `codecvt_byname<wchar_t,
char>` facet was the character count. The standard (22.2.1.5.2 p10,
the resolution of LWG issue 75 that the library's source cites)
makes it the number of external characters, that is bytes, consumed
in producing at most `max` internal ones, and with `max` the size of
the file that is the whole file: 25115 for the Shift_JIS one, which
is what the library returns and what the library's own comment says
it returns. Repaired: the expectation is the byte count, as it
already was for the classic facet.

### 4.3 A shared database named after a real codeset

Earlier in the test, a section builds a single-byte locale from the
C library's "interesting" locale, with a synthetic charmap named
after that locale's codeset; on this machine the codeset is `UTF-8`.
`localedef` writes the conversion database under the charmap's
`<code_set_name>` in the locale root and, finding one there, reuses
it ("exists, skipping", `util/codecvt.cpp`). The `ja_JP.UTF-8`
locale built later in the same root therefore converted through the
single-byte stub: `max_length` 1, `encoding` 1, `error` at the first
multibyte sequence, while the same locale built by hand answers 6, 0
and `ok`. In 2008 the interesting locale would have named ISO-8859-1
or similar and could have collided with the German and French rows
the same way. Repaired: the synthetic charmap takes the generated
name the test's other synthetic charmaps use.

### 4.4 The narrow collate transform and multiples of 127

`__rw_append_weight` in `src/collate.cpp` spells a weight into the
narrow transform string as a run of `CHAR_MAX` bytes, one per 127
subtracted, followed by the remainder as a byte. For a weight that
is an exact multiple of 127 the remainder is 127 itself, so the
string ends in one more `CHAR_MAX` and nothing marks where the
weight ends; the transform strings are then compared with
`string::compare`, and the next weight merges into it. The primary
weight of "l" in the tree's `hr_HR` is 1016, eight times 127, and
that of ko kai in `th_TH` is 127. A probe of `transform` shows the
two letters contributing eight and one `0x7f` bytes and nothing
else, so "lav" reads as thirteen `0x7f` and the weight of "a", and
sorts after "z". The wide facet stores one `wchar_t` per weight and
sorts both lists correctly, which is why only the `char` half of the
test fails: 36 lines of the Croatian list and 65 of the Thai one.
Unchanged since the 2005 import on every branch.

The fix is a `TODO` entry. The encoding wants to be prefix-free and
monotone, for instance a run of `0x7f` for each full 126 with a
final byte in 1 to 126; transform strings are not persistent, so
the change has no compatibility cost.

### 4.5 What remains in the codecvt row

With the files in place and the three repairs, `22.locale.codecvt`
fails one assertion in both widths, number 1582, `out` of the
three-character string "LBE" into a nine-byte buffer answering
`partial` where `ok` is expected, which predates the files; and the
process still aborts on the corrupt-state section, the `TODO` entry
for the test. The collate row records the 101 mismatches of chapter
4.4 until its entry is done.

## 5. Regenerating

The prose files are edited in place, keeping the byte count. The
Japanese files are the construction of chapter 2.2, a few lines of
script over the charmaps. The collate lists are re-sorted with the
commands of chapter 3.1 after any edit, and after any change to the
locale definitions under `etc/nls/src`. The script that produced the
present files is not kept; this note is its specification.
