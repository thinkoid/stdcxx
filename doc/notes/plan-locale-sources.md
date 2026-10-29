# Plan: glibc's current locale sources replace the 2.2 copies

The definitions and charmaps under `etc/nls` are copies from GNU libc
2.2, last changed in 2007. `doc/notes/analysis-locale-sources.md`
measured the distance to glibc's current `localedata`. The new
`localedef` under `util/localedef/` reads the current sources, the
acceptance in `doc/notes/plan-localedef-backend.md` chapter 7 built
all of them. This plan brings them into the tree.

## 0. tl;dr

The 371 definitions and the 233 charmaps of the installed glibc
package replace the 179 and 196 under `etc/nls`, verbatim, under
glibc's own file names. Its `SUPPORTED` replaces `gen_list` as the
list of the locales the build compiles; the build reads its two
columns. The four names glibc no longer has go. The build compiles
502 locales where it compiled 165. Four steps, one
check each: the import and a fresh build, the suite and a second
build, a scalar oracle against the C library, the documentation.

## 1. What is there

`etc/nls/src` holds 179 definitions: 165 named in `gen_list`, 14
shared files (`i18n`, `translit_*`, `iso14651_t1`, `POSIX`).
`etc/nls/charmaps` holds 196 charmaps, 29 of them named in
`gen_list`. The euro variants are files named `de_DE.euro`; glibc
names them `de_DE@euro`.

`gen_list` has two columns, the definition's name and the charmap's.
`etc/config/GNUmakefile.bin` composes the locale's name as
`<definition>.<charmap>` and moves a `.euro` or `.cyrillic` suffix of
the definition to the end as `@euro` or `@cyrillic`: `de_AT.euro
ISO-8859-15` builds `de_AT.ISO-8859-15@euro` from the definition
`de_AT.euro`. The rule for a locale derives the definition and the
charmap back from the name. The install target derives the codeset
names from the locale names. `run_locale_utils.sh` is given each
name. Nothing else reads `gen_list`.

`tests/src/locale.cpp` reads `etc/nls/countries`, `languages` and
`encodings`, maps between the host's locale names and canonical
ones. The tests compile a handful of definitions by name: `de_DE`,
`fr_FR`, `ja_JP`, `sv_SE`, `th_TH`, `hr_HR`, with their charmaps; all
exist in glibc's set. `etc/nls/posix` is a second, smaller set that
nothing references since the 2005 import.

The installed package is glibc 2.44+r50+g1848099f063e-1: `localedata`
as of glibc commit 1848099f063e. `/usr/share/i18n/SUPPORTED` lists
502 locales in two columns, the locale's name and the charmap's:
`aa_DJ.UTF-8 UTF-8`, `aa_DJ ISO-8859-1`, `de_DE@euro ISO-8859-15`.
Where the name carries a codeset it is the charmap's name; a
modifier follows the definition's name, and the definition is the
file of that name with the modifier (`de_DE@euro` is a file). The
502 name 350 definitions and 30 charmaps. glibc's own `SUPPORTED` is
a makefile fragment, one `name/charmap \` per line; the package
renders it to the two columns. The charmaps are shipped gzipped; the
compiler reads plain files.

## 2. The questions

### 2.1 Verbatim, or under the tree's header

The 2005 import put the Apache header and a Rogue Wave copyright
line on each file above glibc's own comment. glibc's files open with
the FSF's statement that it claims no copyright in the locale data.

Decision: verbatim. The files are data, the statement in them says
whose; a byte-identical copy makes the next refresh a copy and a
diff. The three files without the statement (`ab_GE`, `hrx_BR`,
`translit_emojis`) come with the rest.

### 2.2 File names

glibc names the euro variants `de_DE@euro`; the tree renamed them
`de_DE.euro` and the build maps the suffix back.

Decision: glibc's names. The build's rule derives the definition
from the locale name without the `@` to `.` substitution.

### 2.3 The list

`gen_list` names a definition and a charmap; `SUPPORTED` names a
locale and a charmap.

Decision: `etc/nls/SUPPORTED`, verbatim from the package, replaces
`gen_list`. The build composes the locale's name from the two
columns: `aa_DJ ISO-8859-1` is `aa_DJ.ISO-8859-1`, `aa_DJ.UTF-8
UTF-8` is `aa_DJ.UTF-8`, `de_DE@euro ISO-8859-15` is
`de_DE.ISO-8859-15@euro`, the tree's form, so that the database
directories keep their names and the harness its name patterns. The
rule derives definition and charmap from the name as before, less
the substitution of 2.2.

### 2.4 The four legacy names

`iw_IL`, `no_NO`, `sr_YU` and `sr_YU.cyrillic` are 2.2 names glibc
renamed: `he_IL`, `nb_NO`, `sr_RS@latin` and `sr_RS`. Nothing in the
tree names them but the characterization's list of host locale
names, which is not about these sources. The queue entry, in
7fc0bf66, keeps them through an explicit mapping.

Decision: dropped. Yugoslavia is gone, and the other three are
2.2's spellings of locales glibc has since corrected; a stub that
copies the successor keeps a wrong name alive. The queue entry's
mapping is withdrawn with this plan.

### 2.5 The build under `-j`

`localedef` writes the shared `LC_CTYPE` file of a charmap and the
charmap's codecvt database beside the locale directories, the first
locale to need them writing them after a `stat`. Two locales of one
charmap built at once both write. Every writer writes the same
bytes, so the files are whole once every writer is done, and the
tests read them after `make locales` returns; the write is not
atomic all the same. The debug compiler takes 12 s on a UTF-8
definition and 3 s on an eight-bit one; the 502 take about 70
minutes in one process.

Decision: the build seeds. The first locale of each codeset is
built alone and writes the codeset's codecvt database and the
shared ctype file; the other locales of the codeset are order-only
dependents of it, so under `-j` they start when it is done. A
locale whose `LC_CTYPE` copies another definition than the seed's
(`am_ET`, `tr_TR`) still shares its ctype file with its siblings
under `-j`; the queue keeps that residue. Step 2 builds the set in
one process and compares.

### 2.6 What stays

`etc/nls/posix`, `countries`, `languages`, `encodings`: untouched.
`posix` is a queue entry (nothing reads it).

## 3. Order of work

1. The import. `etc/nls/src` and `etc/nls/charmaps` emptied and
   refilled from the package, the charmaps uncompressed; `SUPPORTED`
   in, `gen_list` out; the build reads it. Check: a fresh 15D build,
   `make -j16 locales` builds the 502 locales and the 30 codecvt
   databases, every
   `localedef` exits 0, the time recorded.
2. The suite and a second build. Check: the 15D suite against its
   pin; every row that moves is explained by the source difference
   (`analysis-locale-sources.md` chapter 3 and 4 name the values)
   and re-pinned; `make locales` in one process into a second
   directory is byte-identical to the parallel one.
3. A scalar oracle. `bin/oracle` has collate, ctype and time; the
   values the survey found different, currency, separators,
   grouping, formats, month and day names, have none. A fourth
   program reads `numpunct`, `moneypunct` and the names of
   `time_put` through the library's facets and compares with
   `nl_langinfo` and `localeconv` under `setlocale` for the same
   name. Check: run over the 502 of `SUPPORTED`, zero disagreements,
   or each one explained in its output.
4. Documentation. `README.md`, `ref-locale.md` (the provenance and
   the refresh recipe), the closing chapter of
   `analysis-locale-sources.md`, the queue entry leaves.

Issues found on the way are queue entries at once, in the chapter
of the part of the tree they belong to.

## 4. Acceptance

The queue entry's check: `localedef` builds every definition in the
refreshed `etc/nls/src` with its charmap, two builds byte-identical;
the locale tests pass against their pins, re-pinned where the data
moved; the decoded currency, numeric and date values of the shared
definitions match the glibc sources they come from.
