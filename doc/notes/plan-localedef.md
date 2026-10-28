# A new localedef: the plan

As of 2026-10-26 (4de50039).

## 0. tl;dr

The locale compiler under `util` is rewritten. The new compiler reads
the locale sources that GNU libc ships today, writes the database
format the facets read now, and is accepted against the old compiler
where the old one is right and against the C library where it is
wrong. The front end comes first: a line reader, one small function
per statement of the POSIX grammar, and a model of plain structs.
The back end, the tables and the seven writers, follows as its own
plan. The language is C++03 throughout, the tree's own. The old
compiler stays in the tree until the new one is accepted, and the
patches that taught it the current sources sit on the branch
`localedef-patches`, never merged.

## 1. Why a rewrite

The old compiler is one class. `Def`, in `util/def.h`, parses the
definition, computes the collation and conversion tables and writes
the seven images, with the parse state, the table state and the
output structs side by side as members. Its ownership is raw
pointers; its weights are arrays freed by hand in a destructor. It
had about a year of development, 2001 to 2002, and none after. The
library it serves is older: its copyright line starts in 1994, and
the history page of the user's guide calls it a code base shipping
for over a decade at the 2005 contribution. The locale subsystem the
compiler writes databases for dates from 2001, a rewrite of the
library's earlier locale implementation; the compiler and the
`codecvt` facet were the last pieces of that rewrite to be written.

In October 2026 it was patched until it read all 502 definitions of
glibc's `SUPPORTED`: the collation templates, the conditionals, the
one sequence of symbols and definitions, raw characters, escapes,
the keyword table. The patches work, and they showed what the code
is. The census found a heap corruption in about four runs of a
thousand, in the destructor and in `write_codecvt`, with a stray word
nobody could trace to a writer. Reading the compiler to find it cost
more than the reading was worth. The patches are the specification
of what a compiler for these sources must do; the code that carries
them is not worth keeping.

The alternatives were weighed and set aside. A thin layer over
`newlocale` and the `_l` functions needs POSIX 2008 names a strict
C++03 build does not see. A compiler without a parser, reading the C
library's compiled locales, needs the C library at build time and
ties the databases to its version. Retiring the compiler and reading
only the C library's locales gives up the library's own locales,
which are the only ones the suite controls. The library keeps its
own compiler and carries the glibc sources, the only free ones and
the most complete.

## 2. What stays fixed

Three things do not move while the compiler is rewritten.

The database format. `include/loc/_localedef.h` describes the seven
images the facets map. The new compiler writes the same bytes. A
better format, smaller ctype tables, a UTF-8 codecvt that is not a
table of every character, a collation table for `C.UTF-8` that is
not 7 MB, is a separate step that touches the readers too, and it
comes after acceptance, not with it.

The sources. `etc/nls` holds the 2.2-era definitions and charmaps
until the new compiler reads the current ones. Then the 371
definitions and 233 charmaps of glibc 2.44 replace them, `SUPPORTED`
replaces `gen_list`, and the four legacy names map explicitly. The
import waits for the compiler, since only the parked one reads the
current sources.

The oracles. `bin/oracle` compares a database with the C library
under `setlocale` for the same locale: every classification and both
case maps of every code point, and the collation of a word list, of
random strings and of each printable character against the next.
They measured the old compiler and will measure the new one.

## 3. The language

A locale definition is line-structured and flat. One statement per
line, the keyword first, operands separated by semicolons,
continuation by the escape character, comments by the comment
character. Nothing recurses. The nesting there is, sections between
a category keyword and `END`, `order_start` to `order_end`,
`reorder-after` to `reorder-end`, `translit_start` to `translit_end`,
`ifdef` to `endif`, is a run of lines, not a tree. The first token of
a line decides the statement, so no production backtracks, and the
line is the unit of recovery: report, skip the line, count the
error, read on. That is how glibc's `localedef` reads these files
and why its sources are clean.

The hard part of the compiler is not the grammar. It is the
collation semantics, and that lives in the model and the back end.

A charmap is simpler still: a header of `<code_set_name>`,
`<mb_cur_max>`, `<mb_cur_min>`, `<comment_char>` and `<escape_char>`
lines, then `CHARMAP` to `END CHARMAP` with one character per line,
or a run `<A>..<B>` with the encoding of its first member, then an
optional `WIDTH` section.

The grammar of record is POSIX, XBD chapters 6 and 7, with glibc's
extensions as its `ld-*.c` files read them. The lexical rules come
from glibc's `linereader.c`, since the compiler reads what glibc
writes.

## 4. The shape

Three layers and a seam the old code never had.

### 4.1 The reader

A file stack for `copy` and `include`, the current comment and
escape characters, continuation joined. It yields one logical line:
file, line number, and a vector of tokens. A token is its kind, its
text, its value and its column. The kinds are keyword, symbolic name
(`<U00E9>`), string, number, the three ellipses, raw character,
semicolon, parenthesis, comma. A keyword resolves at lex time through
one static table, the table of section 6, so a statement led by a
word the compiler does not read is skipped by name and a word it
does not know draws a warning.

The reader carries the state a stateless scanner cannot: the escape
character changes between files, and a token's position names the
file it came from after that file is closed.

### 4.2 The productions

One small function per statement, over the tokens of one line. A
production takes an iterator pair over the line's tokens, the
attribute it fills by reference, and the diagnostic sink, and returns
whether it matched:

    bool grouping(tok_iter &it, tok_iter last, std::string &attr, diag_t &diag);

A category parser is a loop over lines until `END`, one `switch` on
the leading keyword, one call per case. The uniform signature is what
makes each production testable alone and lets the category parsers
read as the grammar does. Nothing backtracks, so there is no iterator
guard; a production that fails has reported at the token where it
stopped, with the file, the line, the column and what it expected.

The attribute by reference and a `bool` result is an out-parameter,
and the tree's design rules prefer a value. In C++03, without
`expected` or `optional`, it is the practical form for a production,
and it is kept with eyes open.

### 4.3 The model

The model is the tree. One struct per category, plain fields,
vectors and maps, strings as sequences of UCS-4 code points. The
productions write into it directly; there is no intermediate
syntax tree, because no category's data is recursive. The one
section that keeps a syntactic shape is collate: its entries stay in
one sequence, each an element name and its weights as names, with
the `reorder-after` blocks already placed. Resolving names to
numbers is the back end's.

Characters are UCS-4 code points everywhere above the reader. The
charmap supplies the bytes of each one. The old compiler's round
trip through `iconv` and UTF-8, its "C library locale with the same
encoding" argument and its `--ucs` switch have no place: on every
platform the library builds for, `wchar_t` is UCS-4.

### 4.4 The back end

The tables and the seven writers, from the model alone. Its own plan,
written when the front end reads the 502 definitions and the model's
values pass the checks of section 5.

## 5. Order of work

Each step is one piece with one check. The check is the one
measurement that settles the step; nothing more is run for it.

1. The reader. Check: the 165 definitions under `etc/nls` and the
   502 of glibc's `SUPPORTED`, with their charmaps, tokenize with no
   unknown token.
2. The charmap. `CHARMAP` and `WIDTH`, the `<A>..<B>` runs, into one
   table of name, code point and encoding. The code point comes from
   the name, not from an incremented byte, which is where the old
   compiler lost 135 CJK code points. Check: the 233 charmaps of
   glibc 2.44 read, and in the UTF-8 charmap every entry's bytes
   decode to its name's code point.
3. The five scalar categories, and `copy`: `LC_NUMERIC`,
   `LC_MONETARY`, `LC_TIME` with its eras, `LC_MESSAGES`, and the
   keyword table. Strings resolve to code points against the charmap
   at the boundary. Check: for the 502 installed locales of the C
   library, the model's values equal the output of `locale -k`,
   category by category. That is a real oracle that needs no back
   end.
4. `LC_CTYPE`. The classes, `toupper` and `tolower`, the ellipses;
   `class` and `map` beyond the twelve standard classes skipped.
   Check: the class sets against `iswctype` over every code point,
   the ctype oracle's measurement, on the model directly.
5. `LC_COLLATE`. Collating symbols and elements, scripts, the order
   sections and their weights, `reorder-after`, the conditionals,
   `UNDEFINED`, `codepoint_collation`. Check: the 502 definitions
   parse, and a dump of the sequence for `es_ES`, `sv_SE`, `zh_CN`
   and `cmn_TW` reads against the sources.

Then the back end.

## 6. What the compiler reads, and what it skips

The old compiler's October patches settled these; the new one
starts from them.

Read and kept: every keyword of the six categories the facets read,
`copy`, `include`, `comment_char`, `escape_char`, the three
ellipses, `translit_start` to `translit_end` (parsed into the model;
whether the back end writes it is its decision: the runtime has read
no transliteration since the 2005 import).

Read and ignored, no facet reads the data:

- `LC_TIME`: `alt_mon`, `ab_alt_mon`, `date_fmt`, `week`,
  `first_weekday`, `first_workday`, `cal_direction`, `timezone`,
  `era_year`.
- `LC_MESSAGES`: `yesstr`, `nostr`.
- `LC_CTYPE`: `class` and `map` past the ones `std::ctype` reads,
  `charclass`, `charconv` and the names they declare, `outdigit`,
  `alnum`, `default_missing`, `translit_ignore`.
- `LC_COLLATE`: `symbol-equivalence`, `coll_weight_max`,
  `section-symbol`.
- `LC_MONETARY`: the second currency, `duo_*`, `uno_valid_*`,
  `conversion_rate`.

Skipped whole, from their keyword to their `END`: `LC_ADDRESS`,
`LC_IDENTIFICATION`, `LC_MEASUREMENT`, `LC_NAME`, `LC_PAPER`,
`LC_TELEPHONE`.

Read as their ISO/IEC TR 14652 spelling: `reorder-sections-after`,
`reorder-sections-end`.

Any other keyword draws a warning and its statement is skipped. None
of the 502 definitions draws one today.

Rules the patches established, which the new compiler keeps:

- A bare collating symbol and the definitions a `reorder-after`
  places after it live in one sequence, as glibc keeps them; a name
  nothing declares is a new symbol.
- A character written raw, in UTF-8, reads as its symbolic name. In
  a narrow string it is written in the charmap's encoding.
- An escaped character in a string stands for itself.
- `codepoint_collation` orders every character by its value.
- An era's start year is stored as glibc stores it: year -1 is 1 BC,
  so the Buddhist era of `th_TH` gives 2569 for 2026, not 2570.
- A date that does not parse is reported, and no byte of a record is
  left unset.

Two questions of runtime semantics, not of the compiler, stay open
in `TODO`: where a character outside a collation table sorts, POSIX
says last and glibc puts it first; and where the position-level
`IGNORE` mark sorts.

## 7. Decisions

- C++03 throughout, the tree's dialect, no exception for a build-time
  tool. Ownership by value in the standard containers, `auto_ptr`
  from `rw/_autoptr.h` or a hand-rolled scoped pointer where a value
  will not do.
- New code follows the tree's style for new files: `snake_case`
  names, `_t` types, a private prefix only on a member that guards an
  invariant.
- The new sources live in `util/localedef`, built as their own
  target beside the old `localedef` until acceptance. Byte
  comparison needs both compilers in one tree.
- Diagnostics: one sink with file, line, column, the error numbering
  the old compiler printed, a count, exit status 4. An error is a
  value up to the statement; a file that cannot be read is an
  exception out of the reader.
- The old files are not groomed. They go wholesale when the new
  compiler is accepted.

## 8. Acceptance

The new compiler builds every definition of `etc/nls` and of glibc's
`SUPPORTED` with its charmap, two builds byte-identical. Each
database is byte-identical to the one the parked compiler writes,
except where the parked one is known wrong: the 135 CJK code points,
the era records, the unset words, the transliteration table. There
the C library decides, through `bin/oracle`. The locale tests pass
against their pins, re-pinned where the data moved. Then the import.
