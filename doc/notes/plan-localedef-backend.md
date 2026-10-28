# A new localedef: the back end

As of 2026-10-27 (e544ecd1). The companion of `plan-localedef.md`,
whose section 4.4 this document is.

## 0. tl;dr

The front end is in: the reader, the charmap, the model, and the
productions for all six categories. The model reads every one of the
502 definitions of glibc 2.44 and the 165 under `etc/nls`, and its
values pass the checks of the plan's section 5. What remains is the
back end: the tables computed from the model and the seven writers
that put them on disk in the format `include/loc/_localedef.h`
describes.

The back end is a smaller piece of code than the front end and a
larger piece of decision. The format was designed for a world where
the external encoding, the internal `wchar_t` and UCS were three
different things, and for a transliteration the runtime never read.
Reproducing it byte for byte is the acceptance test, and most of it
should be reproduced without comment. Four places should not be
reproduced without a decision, because the old bytes are either
waste or wrong:

- the three identical tries of a UTF-8 codecvt database;
- the transliteration table;
- the weights of a character the definition leaves out;
- the mark a position-level `IGNORE` leaves in a transform.

Section 2 states each as background, question and directions, with
a recommendation, and the decision where one is taken. Section 3
does the same for the smaller questions, one of which, the eras, is
already decided: the library follows POSIX and glibc, and the HP-UX
convention the `time_put` test cites is not a source.

The order of work is one writer per step with one check per step,
scalar categories first, then `LC_CTYPE`, the codecvt database,
`LC_COLLATE`, and the acceptance run. Byte identity with the parked
compiler is the check wherever the parked compiler is right; where
it is known wrong, the C library decides through `bin/oracle`.

## 1. What the back end does

### 1.1 From the model to the images

The model is one struct per category: strings as sequences of UCS-4
code points, classes as runs of code points, case maps as pairs, the
collation as one sequence of symbolic entries with their weights as
names. The charmap is a table of name, code point and bytes. Nothing
in the model is numbered, encoded or sized.

The back end turns that into the seven images:

- the narrow form of every string, through the charmap, and through
  the `LC_CTYPE` transliteration table where the charmap lacks a code
  point, as glibc forms it;
- the `LC_CTYPE` tables: the narrow mask, `toupper` and `tolower`
  arrays of 256 entries, and the three wide arrays;
- the codecvt database of the codeset: the tries that take the
  external bytes to a character and back, the pairs of internal and
  UCS values, the multibyte strings;
- the collation numbering: every character, element and symbol gets
  a collating value, every character and element a row of weights
  per level, the undefined characters their shared row, and the
  tries that take a byte string or a wide string to its row;
- the era records of `LC_TIME`, the format arrays of `LC_MONETARY`;
- the offsets: every image is a header of offsets followed by the
  data, and the offsets are a function of the sizes.

### 1.2 The reference

The reference for byte identity is the parked compiler, the tip of
the branch `localedef-patches`, not `util/` on master. The branch
changed only the parse side: raw characters, escapes, one sequence
of symbols and definitions, `codepoint_collation`, the skip tables.
Every writer, the codecvt generators and the collation table
generators are identical on the two. So the writers on master
describe the bytes exactly, and the branch's parser decides what
goes in. Both compilers must build in one tree for the comparison,
which the plan's section 7 already requires.

The parked compiler writes in a fixed order: `LC_CTYPE`, the codecvt
database, `LC_MONETARY`, `LC_NUMERIC`, `LC_TIME`, `LC_COLLATE`,
`LC_MESSAGES`. The order does not matter to the bytes of any one
file, except that the codecvt database is written only if its file
does not exist, so the first locale built with a codeset writes it.
The build runs one locale per process, and the old table generators
keep static counters that are never reset; the new back end has no
statics and does not depend on the process boundary.

### 1.3 The format in one page

Every image starts with a struct from `_localedef.h`. Every offset
in the struct counts bytes from the end of the struct, so the facets
locate data as `this + 1 + offset`. The strings of a category are
written wide first, each a NUL-terminated array of `wchar_t`, so
they stay four-byte aligned, then narrow, then the codeset name and
the charmap file name, NUL-terminated. Nothing is sized by a count
except the arrays whose length the header gives; a string ends at
its NUL. Padding words and the `*_ext_off` fields are zero.

`LC_CTYPE` is a header of 1568 bytes that holds the narrow tables
inline: `toupper` and `tolower` as 256 bytes each, the mask as 256
words of ten bits. After it come three flat arrays sorted by
character: the (lower, upper) pairs, the (upper, lower) pairs, and
the (character, mask) elements. A locale that copies another's
`LC_CTYPE` shares the file: it is written once as
`<name>.ctype.<charmap>` in the output directory and each locale's
`LC_CTYPE` is a symbolic link to it.

The codecvt database is one file per codeset, named after it, next
to the locale directories. After a header of eight offsets come four
tries:

- `n_to_w`, keyed by the external bytes, whose leaf is the ordinal
  of the character;
- `w_to_n`, keyed by the UTF-8 form of the internal `wchar_t`, whose
  leaf is the offset of the multibyte string;
- `utf8_to_ext`, keyed by the UTF-8 form of the UCS value, read only
  under the `@UCS` modifier;
- the transliteration trie.

A trie is a
sequence of pages of 256 words, one page per byte of a key prefix,
numbered in depth-first order with page 0 the root. A word is
`UINT_MAX` for no character, the high bit and a page number to
continue, or a leaf. After the tries come the (internal, UCS) pairs
in the order of the external strings, the two names, the multibyte
strings NUL-terminated, and the transliteration data.

`LC_COLLATE` is a header of 352 bytes: 23 offsets and counts, three
bytes for the longest weight, the number of levels and the longest
element, and a byte per level of direction. After it:

- the tries of the collating elements, narrow and wide;
- the weight table, one row per character and element in the order
  their rows were assigned, each row a word of direction bits and
  then, per level, as many words as the longest weight, unused words
  `UINT_MAX`;
- the tries of the characters, narrow and wide;
- four tables of page offsets;
- the first and last byte of each page;
- the names.

The undefined characters, if the definition leaves any, share one
row whose index the header gives.

`LC_NUMERIC` and `LC_MONETARY` each begin with a punctuation struct,
decimal point, thousands separator and grouping, narrow and wide, and
continue with their own struct: true and false names for the numeric
facet, which no source sets; the currency symbols, signs, format
arrays and the nine small integers for the monetary one. `LC_TIME`
is a header of 101 words of offsets, then the era records of 32
bytes each, then the alternate-digit offsets, then the strings.
`LC_MESSAGES` is the two expressions.

The reader of all of this is the library's `src/` under `loc/`, and
`_localedef.h` is the contract. The back end does not touch the
readers. Where a question below ends in a format change, the change
is a separate step after acceptance, with the readers.

## 2. The format questions

Each question is stated the same way: what the format does and why,
the question in one sentence, the directions with their cost and
their gain, and a recommendation. The recommendation is the writer's
reading; the decision is the owner's.

### 2.1 The three tries for UTF-8

**Background.** The codecvt database was designed for three distinct
encodings: the external one, the charmap's bytes; the internal one,
the platform's `wchar_t`, which on some platforms of 2002 was not
UCS; and UCS itself, reached through the `@UCS` modifier on a locale
name. UTF-8 served as the pivot between them. Hence three tries: the
external bytes to the character, the UTF-8 of the internal value to
the external string, and the UTF-8 of the UCS value to the external
string. The runtime for a locale built by `localedef` reads the
tables and not an algorithm: `do_in` walks `n_to_w`, `do_out`
encodes the `wchar_t` as UTF-8 and walks `w_to_n`. Only a locale
with no database, the bare names `UTF-8` and `utf-8`, converts
algorithmically.

On every platform the library builds for, `wchar_t` is UCS-4: the
targets are Linux, where the library's configuration declares the
10646 `wchar_t`, and nothing else. The platforms the internal half
of the pair was written for, the Unix systems of 2002 with their
own wide encodings, and Windows with its 16-bit `wchar_t`, are not
targets and the plan does not provide for them. The internal value
of every character is its UCS value, and the pair carries the same
number twice. For a UTF-8 charmap the three keys of a character are
the same byte string and the three tries are the same shape. glibc
2.44's UTF-8 charmap expands to 297334 characters; each trie is 4814
pages of 1 KB, and the three together are 14.4 MB of the 18.8 MB
file. glibc keeps no table for UTF-8 at all; its converter is the
algorithm. The file is written once per codeset, so the cost is paid
once per build, not once per locale, and the file is mapped, not
read, so the cost at runtime is address space and page faults on the
pages a program touches.

One trie is not a copy of another, though. The leaves differ:
`n_to_w` leaves an ordinal into the pair table, `w_to_n` a byte
offset into the multibyte strings. `w_to_n` and `utf8_to_ext` have
the same key and are meant to have the same leaf whenever the
internal value is the UCS value, which it is here for every
character.

**Question.** Does the first back end write the three tries as they
are, or does it take the UTF-8 case as the occasion to change the
database?

**Directions.**

- (a) Reproduce. The writer expands the charmap and writes three
  tries. Gain: acceptance stays byte identity for every codeset file,
  and the readers are untouched. Cost: 14.4 MB per UTF-8 database and
  about three seconds to write it, once per build. The waste is real
  and bounded.
- (b) Alias `utf8_to_ext` to `w_to_n`. When every character's internal
  value is its UCS value, the writer sets `utf8_to_ext_tab_off` equal
  to `w_to_n_tab_off` and writes one trie for the two. The header
  already allows it, since the reader follows offsets, and no reader
  changes. Gain: a third of the tries, 4.8 MB, and the `utf8_to_ext`
  leaves become right (section 3.6). Cost: byte identity is waived for
  every UTF-8 codecvt file, and the structural comparison of section
  7 has to take the aliasing into account. `n_to_w` cannot join the
  alias: its leaves are ordinals.
- (c) No table for UTF-8. The writer leaves the tries empty and sets
  a flag, and the runtime converts a UTF-8 locale algorithmically, as
  it does for the bare names. Gain: the database shrinks to the pairs
  and strings, or to nothing. Cost: a reader change in `wcodecvt.cpp`,
  and a change of meaning: today a localedef UTF-8 locale converts
  only the sequences its charmap lists, so a code point assigned after
  the charmap's Unicode version fails to convert, where the algorithm
  and glibc convert any scalar value. That is the stricter behaviour
  and it is tied to the charmap's age. Whether the library wants
  table validation for UTF-8 is a question about the readers and the
  test suite, after acceptance.
- (d) A vector of hybrid records in place of the tries. The charmap
  is already a list of singles and runs, and a run is contiguous on
  both sides by definition: in code points, and in bytes under the
  rule that produced it, the first member's encoding incremented
  with carry at 256. So one sorted vector of records, first and last
  code point and the bytes of the first, holds the whole mapping.
  Both directions come from the one vector: `w_to_n` is a binary
  search on the code point, and `n_to_w` is a second ordering, a
  permutation of record indexes sorted by the first byte string. The
  ordinal indirection and the pair table go, since the record is the
  pair, and the three tries become one vector and one index for
  every codeset, not only UTF-8. For UTF-8 the bytes are a function
  of the code point, so a record is two words and the table is a
  membership set, the on-disk form of the charmap-as-ranges item.
  Space: about 16 bytes per charmap line, under 1 MB for glibc's
  UTF-8 charmap against 4.8 MB per trie. Time: a binary search over
  56K records is sixteen probes whose top is hot across the calls of
  a conversion loop, and a 256-entry first-level index by the high
  bits of the code point makes it a search over a few hundred; a
  trie lookup is one cold 1 KB page per byte of the key. The one
  rule the vector must keep: the charmap's own partition into runs
  and singles, never neighbours merged into a longer range, because
  on the bytes side the increment rule holds only inside a run as
  the charmap wrote it, and a legacy codeset's byte regions are not
  contiguous across runs. The collate character tries take the same
  shape, first, last and first row, with singles where the order
  interleaves, and the wide collate trie's UTF-8 spelling goes with
  them. Four runtime files read the codecvt database and change
  with it: `wcodecvt.cpp`, the converter; `iso2022.cpp`, the
  ISO-2022 converter, which walks `utf8_to_ext` and `n_to_w`;
  `wctype.cpp`, whose `widen` and `narrow` go through the tables;
  and `collate.cpp`, which narrows through them for the narrow
  transform. A redesign of those readers after acceptance, with its
  own plan.

**Recommendation.** (a) for the writer and the acceptance run, so that
the step is pure. Then (b) as the first commit after acceptance: it is
a two-line change in the writer, no reader moves, and it carries the
fix of 3.6. (d) is the design to take when the readers are opened,
after acceptance, and it answers (c) for UTF-8 as well, since a
membership set is what the algorithmic path lacks.

**Decided.** (b) from the start, since `wchar_t` is UCS-4 on every
target: the writer produces `n_to_w` and `w_to_n` and sets
`utf8_to_ext_tab_off` equal to `w_to_n_tab_off`, two tries and no
third. No reader changes. The acceptance check for the codecvt file
is structural rather than `cmp`: the header equal to the reference
except that one offset, the two tries equal, the pairs, names and
strings equal, and no third trie on the new side. (d), the vector,
comes once the two tries have been worked through, in increments,
with its own plan and its readers.

### 2.2 The transliteration table

**Background.** A transliteration statement gives a code point and
a list of alternatives: `<U20AC> "<U0045><U0055><U0052>"` says the
euro sign may be written `EUR`. glibc uses the table in two places.
At build time, `localedef` forms the narrow form of every string of
a locale through it, so that under ISO-8859-1 the currency symbol of
a euro locale is `EUR` and under ISO-8859-15 the euro sign itself.
At run time, `iconv` opened with `//TRANSLIT` reads the table from
`LC_CTYPE`; `wcrtomb` and its family do not, they return `EILSEQ`.

The library's compiler has the first use since step 3 of the front
end: the model keeps the true code point, and the narrow form is the
back end's, formed through the table as glibc forms it. The step 3
check settled that reading: 145 of the 20080 values of `locale -k`
differ from the model only by transliteration, and all are explained
by it. So the back end needs the table in memory whatever it writes.

The second use the library has never had. The old compiler writes
the table into the per-codeset codecvt database as a fourth trie
and a block of alternatives. The one reader, `__rw_xlit`, is called
from a block of `__rw_libstd_do_out` guarded by a literal zero, and
the guard was there in the 2005 import. The bytes it writes are also
wrong by glibc's reading:

- the database is per codeset and the first locale built writes it,
  so every ISO-8859-1 locale gets the table of whichever came first;
- a duplicate key keeps the first statement, so an included file
  overrides the locale's own lines, and `de_DE` gets `Ä` to `A`
  rather than `AE`;
- a symbol token is its own alternative where glibc joins the
  operands of one alternative;
- a string of two characters or fewer is dropped.

The C++03 `codecvt` has no notion of
a substitute: `do_out` returns `error` for a character it cannot
convert, `do_max_length` bounds the bytes of one character, and both
libstdc++ and libc++ return `error`. A transliterating `codecvt`
would break `do_in` as the inverse of `do_out` and understate
`do_max_length`.

**Question.** What does the back end write for the transliteration
section of the codecvt database: the old bytes, corrected bytes, or
an empty table?

**Directions.**

- (a) Reproduce the old bytes. Gain: byte identity. Cost: a page of
  code to reproduce the old selection rules exactly, defects included,
  and the content of the file keeps depending on which locale of the
  codeset was built first, so a census in a different order gives a
  different file.
- (b) Write an empty table: the one root page the old compiler always
  creates, every word `UINT_MAX`, no data; `xliteration_off` and the
  other offsets as before. Gain: the codecvt database becomes a pure
  function of the charmap, no wrong data on disk, and no reader
  changes, since none reads it. Cost: byte identity waived for the
  transliteration section, which the plan's acceptance already lists
  among the known-wrong areas.
- (c) Write glibc's table, per locale, into `LC_CTYPE` through the
  `ctype_ext_off` extension the format reserves. Gain: the right data
  in the right place for a transliterating API in the `iconv` mould,
  if one is ever wanted. Cost: a format change and a new reader for
  data nothing consumes; speculative.
- (d) Drop the section and the dead reader: `xliteration_off`,
  `__rw_xlit`, the guarded block. Cost: a format change in the header
  and the readers, after acceptance, in the commit that removes the
  old compiler.

**Recommendation.** (b) now, (d) with the removal of the old
compiler. The step 3 use is kept in full; the model's table is the
one glibc's `localedef` builds, precedence included, and that is the
one the narrow strings need.

### 2.3 Characters outside the collation table

**Background.** A collation definition names the characters it
orders. A charmap has characters the definition does not name, and
the definition may say where they go with the keyword `UNDEFINED`, a
line in the order like any other that stands for all of them. POSIX
(XBD 7.3.2.4) says what happens without it: the utility shall issue
a warning and place such characters at the end of the collation
order, with the same primary weight. The library's compiler has done
exactly that since the 2005 import: one shared row of weights, its
own collating value at every level, after every defined element, and
the warning, which blocks output unless `-c` is given.

glibc means to do the same. Its `ld-collate.c` says "don't emit an
error, simply append UNDEFINED at the end", and appends it. But by
reading, its `output_weight` returns before writing the weights of
`UNDEFINED`, so a missing character takes weight index 0, which is
the lowest defined character's row. In `iso14651_t1_common` that row
is `IGNORE;IGNORE;IGNORE;<self>`: a missing character is ignored at
three levels and sorts before every letter. Five sources define
`UNDEFINED`, `ja_JP`, `ko_KR`, `sl_SI`, `km_KH` and `lo_LA`, eight of
the 502 entries of `SUPPORTED`; the other 494 are in the case the
paragraph describes, and every one of them draws the library's
warning.

The `wcsxfrm` check has been run, on glibc 2.44, for U+743B, an
ideograph the pinyin table does not list, against U+0001, U+743C
and `a`. Under `zh_CN.UTF-8` the reading holds: U+743B transforms
to three empty levels and a fourth-level value, as U+0001 does, and
`wcscoll` puts it before U+0001 and before every letter. Under
`de_DE.UTF-8` and `en_US.UTF-8` it does not: U+743B and U+743C get
distinct primary weights, ascending with the code point, after
every letter. The common table gives the Han block computed
weights, which `localedef` constructs, and its characters are not
missing there; they become missing under `zh_CN` because the pinyin
table reorders its own list and leaves the rest of the block to the
undefined element. So glibc's index 0 is the fate of a missing
character where one exists, and the example is `zh_CN`, not
`de_DE`. Which characters a definition leaves out is a property of
the definition, and the library's rule applies to whatever is left.

The oracle measures against glibc, so the difference shows: 1584 of
50000 random strings over U+4E00 to U+9FFF under `zh_CN`, all over
characters outside the pinyin table, and a handful in `fa_IR`,
`ml_IN` and `en_US`. The narrow facet agrees with glibc by accident:
its lookup advances into a multibyte character before it misses,
the validity test then starts on a continuation byte, and
`do_transform` returns an empty string, which sorts before
everything. The wide lookup does not advance and sorts last, as the
compiler intends. That is a runtime bug whichever order is chosen,
and it is queued separately.

The reading of glibc is a reading. One command settles it: under
`zh_CN.UTF-8`, `wcsxfrm` of U+743B, a Han character outside the
pinyin table, equals `wcsxfrm` of U+0001 if the reading is right.

**Question.** What weights does the back end give the shared row of
the characters a definition leaves out: POSIX's, the library's for
twenty-one years, or glibc's?

**Directions.**

- (a) POSIX, as now. The shared row takes its own value at every
  level, after every defined element. Gain: the standard's order, byte
  identity, and nothing to explain in the code. Cost: the oracle's
  residue over missing characters stays, as explained residue; the
  oracle can skip characters outside the table, or the record can
  carry the count and the reason.
- (b) glibc's order exactly. The shared row takes `IGNORE` at every
  level but the last, and at the last the weight of the lowest defined
  row. Gain: the oracle's residue over missing characters goes, by
  estimate, since nothing was run. Cost: it copies an accident, not a
  rule; it departs from a "shall" in POSIX; and a variant that gives
  the character its own value at the last level sorts the missing
  characters first but parts from glibc among themselves.
- (c) A switch, `--undefined=posix|glibc`, default POSIX, and the
  oracle builds with the other. Cost: two behaviours to test and to
  document, for one oracle's benefit.
- (d) Measure first. Run the `wcsxfrm` check; if glibc's order is the
  accident the reading says, (a) follows and the glibc defect can be
  reported with the command as the reproducer.

**Recommendation.** (d) as the precondition of the collate step,
then (a). The warning stays, since the standard asks for it, but it
should not block output: 494 of 502 entries trip it, and a compiler
that refuses glibc's own sources without a flag is not reading them
as glibc does. A warning counts, an error exits 4, and `-c` keeps
its meaning for errors.

**Decided.** (a): POSIX's order and the library's historic rule,
one shared row after every defined element, the warning counting
and not blocking. The measurement of (d) is done and recorded
above.

### 2.4 The IGNORE mark at a position level

**Background.** A collation level is read `forward`, `backward` or
with `position`. An element `IGNORE`d at a level has no weight
there. In a forward or backward level it is skipped as if absent. In
a position level it is not: POSIX says the position of the ignored
elements is significant, so two strings that differ only in where
their ignored elements lie compare unequal. The fourth level of
`iso14651_t1` is `forward,position`.

The library's transform marks each ignored element in a position
level with the largest value of the character type, `CHAR_MAX`
narrow and `WCHAR_MAX` wide, in place, so the string whose first
weighted element comes after fewer ignored ones sorts first. The
narrow spelling of a weight was repaired in 2026 so that no weight
opens with the mark; `analysis-collate-transform.md` is the record.
glibc does it the other way about: in a position level it writes,
before the weights of each weighted element, a count of the ignored
elements skipped since the last weighted one, plus one, and it
writes nothing for ignored elements at the end of the string.

The two spellings are nothing alike, the bytes of a transform are
not comparable between the library and glibc, and agreement between
them on any set of strings proves nothing about either. The judge
is the standard. POSIX, XBD 7.3.2.3, defines `position`: "The
string containing an element not subject to IGNORE after the fewest
collating elements subject to IGNORE from the start of the compare
shall collate first. If both strings contain a character not
subject to IGNORE in the same relative position, the collating
values assigned to the elements shall determine the ordering. In
case of equality, subsequent characters not subject to IGNORE shall
be considered in the same manner." And 7.3.2.4, for every other
level: "Collation shall behave as if, for each weight level,
elements subject to IGNORE are removed".

That text decides the two queued pairs. Under `sv_SE`, `ü`, ignored
at the fourth level, against `ÿ`, which has its code point there:
equal at the first three levels; at the fourth, `ÿ` has an element
not subject to IGNORE and `ü` has none left to consider, so `ü`
collates first, as a shorter string does. Under `en_US`, `é`
against `é>`, where `>` is ignored at the first three levels: those
levels behave as if `>` were removed, so they are equal, and at the
fourth `é` is exhausted first. glibc puts the shorter string first
in both, which is the standard's order; the library puts it second
in both. The reason is the mark: a value that sorts after every
weight, written in place of an ignored element, makes a string with
a trailing ignored element collate after the same string without
it, where the standard says before. glibc's count realizes the
rule: fewest ignored elements before a weighted one gives the
smaller count, and nothing is written for ignored elements after
the last weighted one. Whether the `<SPECIAL>` section's backward
second level adds a second cause is unknown; the transform of each
pair against `wcsxfrm`, level by level, would show it.

What the compiler writes is simpler than the runtime. An `IGNORE`
is a weight of zero in the row; the direction bits of each level are
two bits in the row's order word. The format has no mark; the mark
is the runtime's reading of the zero.

**Question.** Is the position-level disagreement the compiler's to
fix in what it writes, or the runtime's to fix in how it spells the
transform?

**Directions.**

- (a) The runtime's. The writer keeps the zero and the direction bits.
  `__rw_process_offsets` in `src/collate.cpp` spells a position level
  as glibc does: a count before each weighted element, nothing after
  the last. Gain: no format change, and the library's transform
  agrees with `wcsxfrm` on the strings the oracle generates. Cost: a
  runtime change with its test, and the count needs the same
  prefix-free spelling the weights got in 2026.
- (b) The compiler's. Give an element ignored at a position level a
  real weight, so that the runtime never sees the zero. Cost: it
  changes the meaning of the table, so the two compilers part on every
  locale with a position level, which is all of glibc's; and no
  weight reproduces glibc's counting, so the residue moves rather than
  goes.
- (c) Settle first. Transform the two pairs under both and read the
  bytes; then (a) or nothing.

**Recommendation.** The writer reproduces the zero and the bits, and
the question leaves this plan: it is a runtime entry, fixed by (a)
with the standard's text as its specification and (c) as its first
step, in `TODO` under Library where the oracle's residue is
explained.

**Decided.** As recommended. For the runtime entry the standard is
the specification and glibc, whose spelling realizes it clause by
clause, a usable oracle for position levels.

## 3. The smaller questions

### 3.1 `blank`

**Background.** The mask word of `LC_CTYPE` has ten bits: `space`,
`print`, `cntrl`, `upper`, `lower`, `alpha`, `digit`, `punct`,
`xdigit`, `graph`. `alnum` is `alpha|digit` and has no bit. C++03's
`ctype_base` has no `blank`; C++11 added it. The old compiler skips
the `blank` statement by name. The model keeps the class, since
glibc sets it in every definition.

**Question.** Does the writer drop `blank`, or carry it in a spare
bit?

**Directions.**

- (a) Drop it at the writer, as the old compiler did. Byte identity
  holds; nothing reads it.
- (b) Write it as bit 0x400 in the mask word and the wide elements.
  The readers test `mask & m` and no mask value of theirs includes the
  bit, so they are unaffected, but every `LC_CTYPE` file changes and
  the comparison must mask the bit. Gain: the data is there the day
  the library grows a C++11 `ctype_base::blank`.

**Recommendation.** (a). The bit is a one-line change the day it is
wanted, and the model already has the class.

**Decided.** (b): `blank` is kept, as bit 0x400 of the mask word, in
the narrow table and the wide elements. The mask fields are 32 bits,
no reader's mask value includes the bit, and the readers test
`mask & m`, so the bit is invisible until a `ctype_base::blank` names
it. The comparison of section 7 masks the bit out of `LC_CTYPE`.

### 3.2 Class defaults

**Background.** POSIX, XBD 7.3.1 `LC_CTYPE`, defines the term
"automatically included": it is not an error to include or omit the
referenced characters, "the implementation provides them if missing
(even if the entire keyword is missing)". The keyword descriptions
then say what each class includes automatically:

- `alpha`, the characters of `upper` and `lower`;
- `graph`, the characters of `upper`, `lower`, `alpha`, `digit`,
  `xdigit` and `punct`;
- `print`, those of `graph`, `graph` itself, and the space;
- `alnum`, the characters of `alpha` and `digit`;
- `space`, the six portable white-space characters and the
  characters of `blank`;
- `upper`, `lower`, `digit` and `xdigit`, the portable character
  set's letters A to Z and a to z, digits 0 to 9, and the hex
  letters, which no definition can leave out.

glibc's `set_class_defaults` implements that list after reading.
The old compiler's `auto_fill` does part of it: `print` and `graph`
take `upper`, `lower`, `alpha`, `digit`, `xdigit` and `punct`;
`alpha` takes `upper` and `lower`; the space is not added to
`print`, in a line that is commented out. The line, in `auto_fill`
of `util/def.cpp`, tests the class `space`, not the character
`<space>` the standard names: enabled, it would have made tab,
newline and the other white-space controls printable, which `print`
must exclude, so it was commented out rather than fixed, in the 2005
import. The rule the standard states is the single character, by
code point, and the class into nothing. The step 4 check found no
rule firing on any of the 502 glibc definitions: every one sets all
eleven classes and both maps, and `alnum` is the only derived class.
The 165 definitions under `etc/nls` have not been checked for it.

**Question.** Which defaults does the writer apply: the old
compiler's, glibc's, or POSIX's?

**Directions.**

- (a) The old compiler's, for byte identity.
- (b) glibc's.
- (c) POSIX's, which differ from (a) in the space and the portable
  set. The three agree wherever every class is set.

**Recommendation.** (a), with a census first: the definitions of
`etc/nls` and `SUPPORTED` on which any rule changes any bit. If the
count is zero, the three are the same code and the plainest, (c),
is free.

**Decided.** (c), POSIX's rules of XBD 7.3.1 as listed above, the
single `<space>` into `print` and the portable set into its five
classes included, over glibc's and the old compiler's. The census
is still run, as the measure of where the three part on the
corpus: a definition on which a rule changes a bit is a difference
from the reference to explain in the acceptance run, not a defect.

### 3.3 Members the charmap lacks

**Background.** The model keeps what the source says. A class run
of `i18n_ctype` covers code points an ISO-8859-1 charmap lacks; a
case pair may name one; a transliteration alternative may contain
one; a collation range may run through them. The old compiler is
inconsistent here. A symbolic range `<U0041>..<U005A>` keeps only
the members the charmap has, which is what `i18n_ctype` uses; an
absolute `...` range adds every value, charmap or not; a case pair
needs both sides in the charmap; a collation range through values
the charmap lacks inserts entries for them and then dereferences a
lookup that fails, which is undefined behaviour by reading. glibc
classifies every code point the source names, whatever the charmap,
so under `de_DE.ISO-8859-1` `iswalpha` of a Cyrillic letter is true.

**Question.** Does the wide `LC_CTYPE` table cover the code points
the source names or the ones the charmap has?

**Directions.**

- (a) The charmap's, as the old compiler does for symbolic ranges:
  byte identity, and small tables for the 8-bit locales.
- (b) The source's, as glibc does: the wide classification no longer
  depends on the codeset, at a cost in file size, since the wide table
  is a flat array of eight bytes per character, so an 8-bit locale's
  `LC_CTYPE` grows from kilobytes to a few megabytes, the size the
  UTF-8 locales already are.

**Recommendation.** (a) for acceptance. (b) is a real question for
the oracle under an 8-bit glibc locale, and it belongs to the import
step, when the 8-bit locales of `SUPPORTED` are built. Independent
of the choice, the collation writer skips range members the charmap
lacks and the transliteration lookup tries alternatives in order and
takes the first whose every character the charmap has, which is the
rule step 3 verified against `locale -k`; neither needs a decision.

**Decided.** (a).

### 3.4 Eras

**Decided.** The library follows POSIX and glibc. The `time_put`
test's fourth era cites HP-UX 11.11's `ja_JP` for the meaning of a
`-` direction, and that convention is not a source; the test is
rewritten.

**Background.** An era record is 32 bytes: two string offsets, the
offset, the start and end years as full years, and a byte each for
month and day. The old compiler stores the direction as the sign of
the offset, the start year as written, and the end of time only as
an end date. The runtime takes `-` to mean that the era's numbers
grow into the past from `year[0]`, which is the opposite of POSIX,
where the direction says which end of the era holds the lower
numbers and glibc derives the order in time from the two dates. Two
consequences, both found by reading: a `+` era that ends at `-*`,
the pre-republican era of `zh_TW` and its four siblings and the
pre-Meiji era of `ja_JP`, never matches, so the library falls back
to `%C` and `%y` where glibc writes the era's year; and a `-` era
counts the wrong way. The equal-year branch also tests the day
against both bounds at once, so a date in the first or last month of
an era can miss. The year is one off for an era that starts before
year 1: the compiler stores `-543` as written, glibc reads it as
543 BC, which has no year 0 and is astronomical `-542`, so the
Buddhist year of 2026 is 2570 in the library and 2569 in glibc.
Finally a date that does not parse writes stack bytes into the
record, which is the "unset words" of the acceptance list.

**What the decision means for the back end.** The model already
holds glibc's reading: the year less 1900 with no year 0, the month
from 0, the day from 1, the direction as a character, and both
infinities. The writer puts the full astronomical year in the
record, `-542` for the Buddhist era, keeps the sign of the offset as
the direction, and writes an infinity at either end. The record
format does not change. The runtime's lookup and `%Ey` take glibc's
form, in `src/time_put.cpp`: the order in time from the dates, the
direction from the sign, the day compared against the bound of its
own month. A date that does not parse is an error and no database is
written. The runtime change rides with the time writer's step, since
the records it reads change meaning in the same commit, and the
`time_put` test is re-pinned there.

**glibc against the text.** glibc 2.44 implements XBD 7.3.5 as
written, measured with `date` under the installed locales: the
direction relative to the dates, so that the pre-republican era of
`zh_TW`, `+` and ending at `-*`, makes 1911 民前01年 and the year
100 民前1812年; no year 0, so that the Buddhist era declared at
-543 makes 2026 พ.ศ. 2569; the whole date against the bounds, so
that 2019-04-30 is 平成31年 and 2019-05-01 令和01年; and the
unmodified conversion when no era matches, as XSH `strftime`
prescribes. Beyond the text and not against it: `%Ey` is zero-padded
to two digits, the start date accepts the infinities too, and
令和元年 for a first year is a second one-year era in the `ja_JP`
source, not a rule. The runtime takes the two-digit padding, the
one choice the text leaves.

**Still open.** Whether `time_get` should read the era records at
all: it parses the `E` modifier and drops it, and its test leaves
`%Ey` and `%EY` unexercised. That is a Library entry, not the
compiler's.

### 3.5 The codecvt database and the CJK extension blocks

**Background.** The old charmap reader expanded a run `<A>..<B>` by
incrementing the last byte of the encoding without carry. In the
UTF-8 charmap five CJK extension blocks have runs that start off a
64-code-point boundary, so the tail of each run produced invalid
byte strings, `f0 ab a0 c0` for a code point whose encoding is
`f0 ab a1 80`, and the valid strings were never produced. In
`LC_CTYPE` the shift shows as the 135 code points the acceptance
list names. In the codecvt database it shows as 13199 entries: the
`n_to_w` trie holds the invalid strings and lacks the valid ones, so
under the old database about 13000 characters of extensions E, F, H,
I and J fail to convert in `do_in` and `do_out`. No oracle measures
codecvt, so nobody has seen it fail. The new charmap takes the code
point from the name and computes the bytes from it, so the new
database is right by construction and byte identity fails in exactly
those pages.

**Question.** What is the check for the codecvt file, where byte
identity is known to fail?

**Directions.**

- (a) A codecvt oracle: for every code point, `codecvt_byname::do_in`
  and `do_out` under the library's locale against `mbrtowc` and
  `wcrtomb` under the C library's, in `bin/oracle` beside the ctype
  and collate arms.
- (b) A structural check without the C library: every leaf of `n_to_w`
  decoded to its pair and every pair encoded back to its key, read
  against the charmap.
- (c) A masked comparison: `cmp` with the differing pages listed and
  explained as the 13199.

**Recommendation.** (b) as the step's check, since it is exact and
needs nothing outside the tree, and (a) queued under Tests, since a
codecvt oracle is wanted whatever the compiler does.

**Decided.** As recommended: (b) as the step's check, (a) queued
under Tests.

### 3.6 The `utf8_to_ext` leaves

**Background.** By reading, the `utf8_to_ext` trie has written a
wrong leaf since 2006. Its leaf is meant to be the byte offset of the
external string, which is what `w_to_n` writes and what the runtime
reads under `@UCS`; it writes the character's ordinal plus the
offset of the string block instead, from the map `n_to_w` uses.
Before the 2006 change it used the byte-offset map. The trie is
reached only under the `@UCS` modifier on a locale name and in the
ISO-2022 converter, which is why nothing has noticed. The check is a
locale named with `@UCS` converting one non-ASCII character out.

**Question.** Does the writer reproduce the wrong leaf for identity
or write the right one?

**Directions.**

- (a) Reproduce, and queue the defect with the check.
- (b) Write the byte offset, and waive identity for that trie in every
  codecvt file, which is every codeset, not only UTF-8.

**Recommendation.** (a) for the acceptance run; 2.1 (b) then makes
the trie an alias of `w_to_n` for UTF-8, which is the fix for the
codesets that matter, and the 8-bit and legacy codesets get the byte
offset in the same commit.

**Decided.** Closed by 2.1 (b) in the first writer: the alias is
the fix, for every codeset.

### 3.7 Old behaviour the back end does not reproduce

These need no decision; they are listed so the comparison can expect
them and the code can say why.

- The old compiler writes the codecvt database even after a parse
  error, then aborts. The new one writes nothing after an error.
- The old table generators number pages from static counters that a
  second locale in one process would continue. The new ones start
  from zero per file.
- Two ellipsis lines in a row: in the old compiler's second pass the
  second range starts from the first's predecessor rather than its
  end, by reading. The front end already places ranges as glibc
  does. If any of the 502 sources has the shape, the collate step's
  comparison will show it, and the explanation is this line.
- The shared undefined row's direction word is never set by the old
  compiler when the source has no `UNDEFINED`; the new one sets it
  to the last section's.
- A collation range through values the charmap lacks: skipped
  members, no lookup of a missing character.
- The narrow `...` range in `LC_CTYPE` sets mask bits for bytes the
  charmap does not list. The new writer sets bits for characters.

## 4. The shape

One file per image under `util/localedef`, `wctype.cpp`,
`wcodecvt.cpp`, `wcollate.cpp`, `wnumeric.cpp`, `wmonetary.cpp`,
`wtime.cpp`, `wmessages.cpp`, each one function from the model and
the charmap to a byte vector, and a driver that names the file and
writes the vector. Three helpers are shared:

- A pool. The strings of a category go into a vector of bytes, wide
  then narrow, and the pool returns each string's offset from the
  end of the header. Every writer computes its offsets the same way,
  and the order of insertion is the order on disk.
- A trie builder. The four codecvt tries and the four collate tries
  have one page encoding and one depth-first numbering. The builder
  takes a sorted sequence of (key bytes, leaf) and yields the pages
  in number order, with the first and last byte of each for the
  collate writer. The two prefix rules, a key that is a prefix of
  another and a page allocated only for the prefixes of valid keys,
  are parameters, not copies.
- The narrow form of a string: through the charmap, then the
  transliteration table, as section 2.2 says.

The collation numbering is its own pass before the collate writer,
since it is the one place where the model's names become numbers:

- collating values from 2 in sequence order, symbols included;
- rows for characters and elements in the order the sequence
  assigns them;
- weights resolved by name to the value the name had at the time,
  as the old compiler does in its second pass;
- the shared undefined row;
- the longest weight and the longest element.

It is the part most likely to part from the old compiler by a count,
so it is written to be dumped, as `--collate` dumps the sequence.

The writers keep the model's `set` flags out of the bytes: an unset
keyword takes the old compiler's default, an empty string or
`CHAR_MAX`, in the writer, not in the model.

## 5. Order of work

Each step is one writer with one check. The check is the one
measurement that settles the step; nothing more is run for it. The
corpus of every check is the 502 definitions of `SUPPORTED` and the
165 of `etc/nls`, each with its charmap, built by both compilers into
two trees, and `cmp` on the file the step writes.

1. The charmap table as ranges, the queued item. Check: as queued,
   `--charmap` over glibc's UTF-8 charmap prints the same lines
   within twice the reader's time and 10 MB over the bare process.
   This comes first because every later step pays the table once
   per locale.
2. The pool and the four scalar writers: `LC_NUMERIC`,
   `LC_MESSAGES`, `LC_MONETARY`, `LC_TIME` without the era records.
   Check: byte identity of the four files for every definition, the
   era records masked.
3. The era records and the runtime's lookup, one commit. Check: the
   `time_put` test's era cases against `strftime` under the C
   library for the twelve definitions with eras, `%EC`, `%Ey` and
   `%EY` over a set of dates on both sides of each boundary; the
   test re-pinned.
4. `LC_CTYPE`. Check: byte identity for every definition except the
   135 code points, where the ctype oracle decides, and the bits
   the census of 3.2 names, where the standard's rule decides.
5. The codecvt database, two tries and the alias of 2.1 (b). Check:
   the structural check of 3.5 (b) for every codeset, and the
   comparison of 2.1 against the reference for every codeset whose
   charmap has no CJK run off a 64-boundary, the transliteration
   section empty on the new side.
6. The collation numbering and `LC_COLLATE`, after the `wcsxfrm`
   check of 2.3. Check: byte identity for every definition; each
   difference explained by a line of section 3.7 or it is a defect.
7. Acceptance: section 7.

## 6. Decisions

- The reference is the parked compiler at the tip of
  `localedef-patches`, built in the same tree.
- The internal `wchar_t` is UCS-4 and the targets are Linux only;
  the platforms with another wide encoding are not provided for.
- The codecvt database has one mapping each way, external bytes to
  the code point and back. `utf8_to_ext` is `w_to_n` under another
  name and the first writer aliases it, 2.1 (b): two tries, and a
  structural comparison in place of `cmp` for the codecvt file. In
  the vector of 2.1 (d) the `wchar_t` is the key and no UTF-8 step
  remains.
- The format does not change in this plan; the codecvt file uses
  an aliasing the header already allows. 2.2 (d) is the first
  format commit after acceptance, with its readers; 2.1 (d), the
  vector of hybrid records, is the shape of the redesign that
  follows, once the two tries have been worked through.
- The transliteration table is read and used for the narrow strings,
  and written empty.
- The undefined characters sort last, POSIX's order and the
  library's rule, one shared row; the `wcsxfrm` check is done and
  recorded in 2.3; the warning counts and does not block output.
- The position-level mark is the runtime's defect: POSIX 7.3.2.3
  fixes the order, glibc's count realizes it, the mark does not; a
  runtime entry, not the writer's.
- `blank` is written as bit 0x400 of the mask, unread until the
  library names it; the class defaults are POSIX's, XBD 7.3.1, with
  the census of 3.2 as the measure of where they part from the
  reference.
- The wide `LC_CTYPE` table covers the charmap's characters;
  glibc's rule is revisited at the import.
- Eras take POSIX's and glibc's meaning, in the compiler and the
  runtime, one commit.
- The `utf8_to_ext` defect is closed by the alias of 2.1 (b).
- C++03, the tree's style, no statics, as the front end.

## 7. Acceptance

The new compiler builds every definition of `etc/nls` and of glibc's
`SUPPORTED` with its charmap, two builds byte-identical. Each
database is byte-identical to the parked compiler's except:

- `LC_CTYPE` at the 135 CJK code points, where the ctype oracle
  decides, in bit 0x400 of every mask, which the comparison masks
  out, and in the bits the census of 3.2 names, where POSIX's rule
  decides;
- the codecvt database, compared structurally as 2.1 says, the
  `utf8_to_ext` trie aliased and absent, the pages of the five CJK
  extension blocks judged by the structural check of 3.5, and the
  transliteration section empty;
- the era records, where `strftime` under the C library decides;
- the lines of section 3.7, each named in the comparison's output.

The locale tests pass against their pins, re-pinned where the data
moved. Then the old compiler goes, and the import of the current
sources begins.
