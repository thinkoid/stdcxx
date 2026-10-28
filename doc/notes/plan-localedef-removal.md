# Plan: the old locale compiler goes

The new `localedef` under `util/localedef/` is complete and accepted,
`doc/notes/plan-localedef-backend.md` chapter 7. This plan removes
the old compiler, the sources under `util/` that the `localedef`
target links, and makes the new one the tree's `localedef`.

## 0. tl;dr

Three things hold the old compiler in the tree, none of them its
compiling:

- the `locale` utility is a stub that executes `localedef
  --locale-mode`, and the mode lives in the old driver;
- two test fixtures are written in a syntax only the old scanner
  read, and four rows of the locale tests sit off their pins with
  the new compiler in place;
- the build, the test harness and two documentation pages name the
  old driver's options.

The order of work is the two fixtures, then a `locale` of its own,
then the cutover in one commit, then the documentation. Each step
has one check. The reference for byte identity, the parked branch
`localedef-patches`, stays as a tag.

## 1. What holds the old compiler

### 1.1 The `locale` utility

`util/locale_stub.cpp` is the whole of the `locale` program: it
replaces the base name of its own path with `localedef`, prepends
`--locale-mode` to its arguments and executes that. The old driver's
`main` in `util/localedef.cpp` strips the option and calls
`locale_main`, which is `util/locale.cpp`: 2915 lines that map a
locale's database files and print them, `locale -a`, `-m`, `-k`,
`-c`, `-h`, `--charmap`, `--help`. It reads the databases through
`_localedef.h`, not through the compiler, but `-h` prints characters
by their names in the charmap the locale was built from, and it
reads that charmap with the old compiler's `Charmap`, hence its
`Scanner`. Its diagnostics go to the old sink, `issue_diag` of
`diagnostic.cpp`, whose header includes `scanner.h`; an error there
throws `loc_exception`. `aliases.cpp` serves only the C library
locale `Charmap` wants, and `memchk.cpp` the checks of the mapped
files. The sanity test of the utilities,
`run_utils -s`, checks `locale --help`, `-a`, `-m` and `-k LC_ALL`
and fails 39 of 45 assertions with the new compiler in
`localedef`'s place, because the new driver has no such mode.

### 1.2 The two fixtures

`tests/localization/22.locale.ctype.cpp` writes a definition whose
`LC_CTYPE` holds two lines that are neither comments nor statements,
a list of names under a `#` comment that lost its `#`, and the old
scanner skipped them. The new reader, glibc's grammar, reports a
syntax error at each, the locale is not built, and the sections that
need it skip: `22.locale.ctype`, `22.locale.ctype.is` and
`22.locale.ctype.scan` lose 162, 124 and 24 assertions. Made
comments, the three rows return to their pins exactly.

`tests/localization/22.locale.collate.cpp` writes its weights
space-separated, `<a> <a> <LETTER> IGNORE`, and gives three of them
as raw byte codes, `\d139`, `\36` and `\x3A`, meant as "the
character with this code". glibc's `localedef` rejects the same
lines with the same messages. With the new compiler the locale is
not created and the test fails two assertions and loses 36.

### 1.3 The build, the harness and the parked branch

`etc/config/GNUmakefile.bin` links `localedef` from sixteen objects
of `util/`, `localedef2` from the eighteen of `util/localedef/`,
and `locale` from the stub; `locales` builds the tree's definitions
with `./localedef -w -c -f CHARMAP -i SOURCE DIR/NAME`, and
`install` copies `locale` and `localedef`. `run_utils` invokes
`localedef -w -c -f -i` and greps `localedef --help` for its first
line; the tests' `rw_localedef` adds `-w701 -w702 -w706` and
`--no_position`. The new driver has every one of those since the
acceptance. The old driver also documents `-g`, `-m`, `-r`, `-s`,
`--ucs` and `--notes`, which nothing in the tree uses.

The per-locale functionality scripts, `bin/NAME.sh`, exit 4 on a
`rm` of a directory in `run_locale_utils.sh` with either compiler;
that is a defect of the scripts, found at the acceptance, and this
plan's checks count them once it is fixed.

The branch `localedef-patches`, tip 1ee2f4d9, is the old compiler
with its parser patched to read glibc 2.44's sources: the reference
every step was measured against. Once the old sources leave master
the branch is the only place the reference can be built from.

### 1.4 The documentation

`doc/stdlibref/localedefutility.html` and `localeutility.html`
describe the two utilities with the old driver's options. The
`TODO` entry "The codecvt database and its three tries" describes
the old file; the new compiler writes two.

## 2. The questions

### 2.1 Where the `locale` utility lives

**Background.** The mode exists because the old build wanted one
binary; the stub costs a process per invocation and ties `locale`
to a driver that is going. `locale.cpp` is a reader of databases
with four small dependencies, none of them the compiler.

**Directions.**

- (a) Port the mode into the new driver: `localedef --locale-mode`
  as before, the stub kept. Cost: the new driver carries 2915 lines
  that have nothing to do with compiling, and the stub stays.
- (b) A `locale` program of its own: the `locale` target links
  `locale.cpp` with `aliases.cpp`, `diagnostic.cpp` and
  `memchk.cpp`, `locale_main` becomes `main`, the stub goes. Those
  four files move under `util/locale/` or stay in `util/` beside
  the other utilities. Cost: one makefile rule and a rename.
- (c) Rewrite `locale` on the new compiler's model. Cost: a
  rewrite for no gain; the utility prints databases, which the
  model does not hold.

**Recommendation.** (b). The utility becomes what POSIX names, a
program, and the old compiler's remainder shrinks to the files it
shares with it. The `diagnostic.cpp` it keeps is the old diagnostic
sink; the new compiler has `diag.cpp`. The two can coexist in
`util/`, and folding `locale.cpp` onto `diag_t` is a later cleanup,
not this plan's.

### 2.2 The collate fixture

**Background.** The fixture tests the facet, not the compiler: a
position level, a many-to-one weight, an element, and three weights
given by value. The value weights are the old scanner's extension;
POSIX gives a weight as a collating symbol, a collating element, a
character or a string of them.

**Directions.**

- (a) Rewrite the lines in the standard's form, semicolons between
  levels and each value weight as the character it names, `\d139`
  as the charmap's name for 0x8B. The intent of each case is kept,
  the syntax changes.
- (b) Teach the new reader the old extension. Cost: a syntax glibc
  rejects, kept for one fixture.

**Recommendation.** (a). The test's comments say what each case
tries; the rewrite keeps them and says so.

### 2.3 The old driver's options

**Background.** `-g` with `-m` generates every locale of a creation
list, with `-r` and `-s` naming the charmap and source directories;
`--ucs` chose UCS as the internal `wchar_t` encoding, which is the
only encoding now; `--notes` enabled informational messages. The
tree's build invokes the compiler once per locale, the harness
likewise, and the documentation page is the only other reader.

**Directions.**

- (a) Drop them. The new driver keeps `-c`, `-w`, `-w###`, `-f`,
  `-i`, `-d`, `-I`, `--no_position`, `--help` and its dumps.
- (b) Add `-g`/`-m` to the new driver for the documentation's sake.

**Recommendation.** (a), with the documentation page rewritten to
the options that exist.

### 2.4 The reference

**Background.** The parked branch is the only buildable copy of the
old compiler after this plan. Every explained difference in the
acceptance names the old behaviour it departs from, and someone
reading 3.7 of the back-end plan may want to run the old binary.

**Directions.**

- (a) Tag the branch tip, `localedef-old`, and delete the branch.
- (b) Keep the branch as it is.

**Recommendation.** (a). A tag is the record; a branch invites
commits.

## 3. Order of work

Each step is one commit with one check.

1. The two fixtures. `22.locale.ctype.cpp`: the two lines become
   comments. `22.locale.collate.cpp`: the weights in the standard's
   form, 2.2 (a). Check: with the new compiler standing in for
   `bin/localedef` as the acceptance ran it, the four rows return
   to their pins, 15D under GCC; no pin moves.
2. The `locale` program, 2.1 (b). Check: the sanity test,
   `run_utils -s`, passes every assertion with the new compiler in
   place, and `locale -k LC_ALL` on a database the new compiler
   wrote prints what it printed on the old one's for `de_DE.UTF-8`.
3. The cutover, one commit: the `localedef` rule links the objects
   of `util/localedef/`, the `localedef2` rule goes, the old
   compiler's sources go, `util/localedef.cpp`, `charmap.cpp`,
   `codecvt.cpp`, `collate.cpp`, `ctype.cpp`, `def.cpp`,
   `messages.cpp`, `monetary.cpp`, `numeric.cpp`,
   `time.cpp`, `scanner.cpp`, `path.cpp` and their headers, and
   the stub; the
   usage line says `localedef`; the branch becomes the tag. Check:
   a clean 15D build, `make locales` building the 165 definitions
   of `etc/nls`, and the locale tests on their pins.
4. The documentation: the two pages to the options and behaviour
   that exist, the `TODO` entry on the three tries closed with the
   file it described gone from the writer, the two `TODO` entries
   the plan opened closed. Check: a read.
5. Acceptance: the nightly's six configurations on their pins.

## 4. Decisions

Each of the four questions is decided as recommended, 2026-10-28.

- The `locale` utility is a program of its own, 2.1 (b).
- The collate fixture is rewritten in the standard's syntax, 2.2
  (a); the ctype fixture's two lines become comments.
- The old driver's generation options and `--ucs` go, 2.3 (a).
- The parked branch becomes the tag `localedef-old`, 2.4 (a).
- `locale.cpp` is ported onto `cmap_t` and `diag_t` instead of
  keeping the old charmap reader, 2026-10-28: the reader would
  otherwise hold `charmap.cpp` and `scanner.cpp` past the cutover.
- After this plan the import of glibc's current sources into
  `etc/nls` is the next piece, its own plan.

## 5. Acceptance

A clean build of 15D with no `localedef2` target builds the
library, `localedef`, `locale`, `gencat`, `exec` and the 165
definitions of `etc/nls`; the sanity test and the functionality
scripts pass; the locale tests sit on their pins in the six
configurations of the nightly; `make install` installs the two
utilities.
