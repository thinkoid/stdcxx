# Inside `std::locale`

## 0. tl;dr

A `locale` is a small handle to a **shared collection of facets**.
The facets supply character classification, conversion, collation,
formatting, parsing and message lookup. The library shares the
collection, and the facets of collections built from one another; it
postpones most of their data loading until first use.

That is the central design: **make copies cheap, reuse common work,
and keep repeated facet access short**. It also puts mutable machinery
behind an interface that looks read-only. Reference counting, the
locks of the global locale and of name expansion, and lazy
initialization are separate parts of the threading story.

Chapters 1 to 7 are the mechanics: handle, body, facets, lookup,
composition, data, sources, threads. Chapters 8 to 12 are the facets
against ISO/IEC 14882:2003, one family each: what the standard
requires, what the library does, where it departs and why, what the
tests pin, what is open. Chapter 13 indexes every departure with its
paragraph. The conformance record (`ref-conformance.md`) summarizes
the departures in one row each; this document is the normative text.
C++11 governs one place only, the format members of `time_get`
(chapter 12).

The tree as of 2026-11-04.

## 1. The handle, the body and the facets

`std::locale` holds one pointer, `_C_body`, to `__rw::__rw_locale`.
Copying a locale copies that pointer and increments the body's
reference count; it does not copy its facets.

The body contains the locale name, a reference count and two
collections of facet pointers:

- A fixed array of 26 standard facet slots, covering the narrow and
  wide specializations. An ordinary facet and its `byname` counterpart
  occupy the same slot.
- A separate array for installed facets with nonstandard ids.

The name is the locale's: one name, or six joined by `;` when the
categories come from different locales. A body has no name, a null
pointer, when it holds a facet installed by the template constructor
or was combined from a body with none; `name()` then returns `"*"`
(22.1.1.2, p13 and p15).

**Every standard slot is filled when the body is built.** The
constructor from a name obtains all 26 facets from their factories,
and a body built from others copies their slots. No slot changes while
the body lives. Every constructor builds a new body unless it copies
one; there is no repository of bodies or facets. Two locales of one
name built apart hold two bodies, and the byname facets of each are
its own and map their databases; a name of the C locale is the one
exception, and gives the classic body, built once and shared. A body
built from others shares their facets: `locale (de, en,
locale::numeric)` holds the `collate_byname<char>` of `de`. The
ordinary facets are one static per type, shared by every body that
holds them; the eight `{money,num}_{get,put}` facets are always
ordinary. Most facet data still loads on first use, but two
constructors map theirs: `ctype_byname<char>` and
`ctype_byname<wchar_t>` map the `LC_CTYPE` database, and the wide one
the codeset's `codecvt` database too.

Each facet has its own reference count. Thus two different bodies can
share a facet even when they cannot share the whole collection: a
body composed from another takes references to its facets.
`locale::facet` is the internal `__rw_facet` base, which also supplies
a mutex and the bookkeeping for its name and implementation data.

## 2. Facets, ids and lookup

A facet is one service, identified by its static `locale::id`.
The six categories group related services:

| category | facets |
|---|---|
| `collate` | string comparison, transformation and hashing |
| `ctype` | character classification and case conversion; `codecvt` for encoding conversion |
| `numeric` | `num_get`, `num_put`, `numpunct` |
| `monetary` | `money_get`, `money_put`, domestic and international `moneypunct` |
| `time` | `time_get`, `time_put` |
| `messages` | message catalogue access |

Standard ids select fixed slots: the numeric id is one greater than
the array index. User ids are assigned on demand; lookup searches the
user-facet array. A derived facet can inherit a standard facet's id
and replace that service, or declare its own id and add a service.
A user id is generated on first use, once per facet type: the first
thread to find it unset generates it under a static lock, and the
others take that value.

`use_facet<T>` resolves the type to a lookup path and returns a
reference. For a standard facet, the path is an indexed pointer
read. The body's constructor filled the slot: it selected the
category's name, called the type's factory and stored the pointer.
Ordinary standard facets use static storage, one per type; named ones
are allocated for the body. A factory sets the facet's type, its id
pointer and, once, the type's standard id.

**The locale selects the service; the facet implements it.** For
example, numeric parsing and output use punctuation supplied by other
facets through the stream's locale. `num_get` and `num_put` have no
`byname` class of their own.

## 3. Composition and the global locale

Constructing a locale by name builds a new body each time, the
classic body aside. Composing locales builds a new body too, but it
does not copy facet objects: it copies their pointers.

The category constructor takes selected categories from another
locale. `a.combine<T>(b)` takes one facet from `b`, retaining the rest
of `a`. Installing a user facet follows the same general route:
`_C_get_body` and `_C_combine` build a collection of shared facet
pointers, unless the result is a copy. A category mixture of two named
locales has a composite name; a locale built with a facet, or combined
from an unnamed one, reports `"*"`.

**Composition produces a new locale value; it does not edit the
source collection.** Nothing else edits a live body either: its
constructor fills every standard slot, and no slot changes until the
body is destroyed. Lookup, copying and comparison therefore read the
slots with no lock.

Default construction takes a reference to the current C++ global
body. `locale::global` replaces that body and returns the old locale;
existing handles retain their bodies. For a named locale it also
calls C `setlocale`; glibc refuses the library's six-category mixed
name, so a mixed global leaves the C locale as it was; the library
keeps its own name rather than compose one C library's (chapter 8.3).
`locale::classic()` supplies the permanent `"C"` locale through a
separate once-initialized handle.

## 4. Copying and the graph around the body

### 4.1 Avoiding work at three levels

**An ordinary copy shares the body.** The copy constructor in
`locale_core.cpp` copies `_C_body` and atomically increments the
body's reference count. Its work is independent of the number of
installed facets: no allocation, facet traversal, virtual call or
database mapping is needed. The atomic abstraction may itself use a
lock, depending on the configuration; the copy takes no lock of the
library's, but it is not universally lock-free.

Assignment acquires the incoming reference **before releasing the
old one**, then replaces the pointer. This order also handles
self-assignment without a special test. Release uses a temporary
handle whose destructor follows the normal disposal path. Acquiring
the new body is cheap; disposing of the old one can, on its last
reference, destroy the body and release its facets.

Composition tries progressively more work only as needed:

| opportunity | work avoided |
|---|---|
| Same source body, or category `none` | Category composition returns the original body with another reference. |
| Null facet, or the exact facet already installed | The facet constructor reduces to sharing the original body. |
| Anything else | `_C_construct` copies facet pointers and increments their counts, rather than cloning facet objects. |

The last case can allocate a body, a user-facet pointer array and name
storage. It still shares the expensive services and their data.
**Body sharing saves the whole collection; facet sharing saves its
contents when the collection must differ.**

Composition does not look for an existing body that would do. A
category taken from a locale of the same name, or two locales of one
name built apart, give different bodies; they compare equal by name
(chapter 8.2). Selecting `all` categories is not a copy of the donor:
the first argument's user facets stay, so `_C_combine` builds a body
for that case too.

### 4.2 Construction, the global locale and destruction

There is no repository of bodies or facets. A body holds one
reference to each of its facets, and a handle holds one reference to
its body.

Named construction comes through `_C_get_body` to
`_C_get_named_body`. A name of the C locale gives the classic body
with a reference. Any other name is expanded by `__rw_expand_name`
under a static guard and gives a new body with a count of 1. That
guard is the only lock of named construction: `__rw_locale_name`,
which the expansion calls, keeps the locale root in a static buffer
with no lock of its own (chapter 8.2). The body constructor runs after
it, with no lock of the library's, and calls the 26 factories.
Composition comes through `_C_get_body` and `_C_combine`, and builds
a body with `_C_construct` unless the result is a copy.

The classic body lives in static storage. `_C_classic` builds it
once, through `__rw_once`, with a count of 1 that the body keeps for
itself, and returns it with another reference. Its count never
reaches zero, so it is never destroyed. `classic()`'s once-initialized
public handle reaches it through `locale ("C")`.

The global locale is a pointer to a body, the classic body until the
first `locale::global`:

| call | meaning |
|---|---|
| `_C_get_global ()` | Acquire a reference to the current global body. |
| `_C_set_global (body)` | Retain `body` as the new global and return the previous global body. Its old global reference passes to the returned locale. |

Both take one mutex, the static mutex of a type of their own;
`_RWSTD_MT_STATIC_GUARD` would key a mutex on each call site's line.
The getter adds its reference under the mutex, so that a concurrent
`global` cannot release the body in between. The pointer's first
value is set through `__rw_once` (5b68c2a7) before either takes the
mutex, so the classic body is not built under it.

Destruction closes the loop. A handle decrements its body's count and
destroys the body at zero. The body destructor drops its reference to
each facet and deletes one whose count reaches zero, unless it is an
ordinary standard facet: a library type and no name, a static of its
factory, which lives on at count zero until the next body takes it.
A program's facet and a byname facet are deleted. **Copying and
destruction take no lock of the library's**, and construction takes
only the guard of the name's expansion.

The price is memory. Two locales of one name built apart hold two
bodies and two sets of byname facets, and each facet maps its own
databases, as in libstdc++. A program that builds a named locale once
and copies it shares one body.

The graph explains the MT tradeoff: only the global locale and name
expansion converge on shared machinery. Facet lookup reads the slots
with no lock, since they are filled before the body is shared, but
**a reference count is not a lock around later reads and writes in
the facet**. Chapter 7 identifies those separate boundaries.

## 5. Where the locale data comes from

The library has its own locale databases. Its `localedef` utility
compiles locale descriptions and charmaps into category data and
conversion tables. Named facets locate those files through the locale
root and map them for reading; `include/loc/_localedef.h` describes the binary
layouts. On Linux the mapping uses read-only `mmap`.

The facet base caches the mapping pointer and size. A few facets cache
derived scalars as well: `ctype` remembers each character's `narrow`
and `widen` results in a table, and `codecvt<char, char>` its
`always_noconv` answer. `numpunct` once cached its decimal point,
grouping and boolean names; its accessors call the virtuals every
time, and 3b046e29 removed the cache members. **Mapped data and
mutable facet caches are different layers.**

The C library is another backend, the one a facet falls back on when
its locale has no database, and the one a byname facet the program
constructs and uses without installing it in a locale reads (chapter
8.2). Paths that temporarily change
the C locale use `__rw_setlocale` to lock, save, switch and restore it.
This process-wide state is distinct from the C++ collection of facets;
the library's lock coordinates its own participating calls.

## 6. The sources under `etc/nls`

The databases are compiled from glibc's locale sources.
`etc/nls/src` and `etc/nls/charmaps` hold the `localedata` of the
glibc package 2.44+r50+g1848099f063e-1, glibc commit 1848099f063e:
371 definitions and 233 charmaps. The files are glibc's, byte for
byte, without a header of the tree's, so a refresh is a copy and a
diff. They keep glibc's names; a variant with a modifier is a file
of its own, `de_DE@euro`, `sr_RS@latin`. The package ships the
charmaps gzipped; the tree holds them uncompressed, since
`localedef` reads plain files.

`etc/nls/SUPPORTED` is the list of the locales the build compiles,
the package's `/usr/share/i18n/SUPPORTED` as it is: 502 lines, each
a locale's name and its charmap. `etc/config/GNUmakefile.bin`
composes the locale's name from the two columns: the name less any
codeset, a dot, the charmap, then the modifier.

- `aa_DJ ISO-8859-1` is `aa_DJ.ISO-8859-1`.
- `aa_DJ.UTF-8 UTF-8` is `aa_DJ.UTF-8`.
- `de_DE@euro ISO-8859-15` is `de_DE.ISO-8859-15@euro`.

The composed name is the locale's database directory under the
build's `nls`. The rule for a locale takes the definition and the
charmap back from it: the definition is the name less its codeset,
`de_DE@euro`, a file in `etc/nls/src`. The first locale of each
codeset, its seed, is built alone and writes the codeset's shared
files; every other locale waits for all the seeds
(`etc/config/GNUmakefile.bin:116`). A locale whose `LC_CTYPE` copies
another definition than its seed's, the `am_ET` and `tr_TR` groups,
still writes its ctype file beside a sibling built at once; `TODO`
has the entry, "`localedef` writes the shared files in place".

`countries`, `languages` and `encodings` are the tree's own: the
test driver reads them to map the host's locale names to canonical
ones. `posix` is an older set of definitions that nothing reads;
`TODO` has the entry.

The refresh, from the top of the tree, after an upgrade of the
glibc package:

```sh
rm etc/nls/src/* etc/nls/charmaps/*
cp /usr/share/i18n/locales/* etc/nls/src/
for f in /usr/share/i18n/charmaps/*.gz; do
    gzip -dc "$f" > etc/nls/charmaps/"$(basename "$f" .gz)"
done
cp /usr/share/i18n/SUPPORTED etc/nls/
```

Then `make locales` in the build's `bin` directory and the suite
against its pin. glibc's own `localedata/SUPPORTED` is a makefile
fragment, one `name/charmap \` per line; the installed package
renders it to the two columns the build reads. A refresh from a
glibc source tree converts the list first.
`doc/notes/plan-locale-sources.md` records the first import and the
two test fixtures it moved.

## 7. The threading picture

The important boundaries in a reentrant build are:

| mechanism | purpose and boundary |
|---|---|
| Body and facet reference counts | Preserve shared ownership; every update of a facet's count uses the same lock-free operation. They do not synchronize arbitrary writes inside a facet. |
| Global locale mutex | Serializes reads and replacements of the global body pointer, the getter's reference included. |
| Name expansion guard | Serializes `__rw_expand_name` and the locale root buffer of `__rw_locale_name`; the body constructor and its facet factories run after it, under no lock of the library's. |
| Facet factories | Their ordinary facets and the standard ids are function-local statics, initialized once under the compiler's guard. |
| Standard facet slots | Filled by the body's constructor before the body is shared, and never changed; lookup, copying, combining and comparison read them with no lock and no atomic access. |
| Lazy facet data | `_C_get_data` takes the facet mutex and checks again before loading; the size, or the data pointer itself where the data is built from the C library later, is stored with release, and the unlocked reads are acquire loads. |
| Cached scalars | The `ctype` and `codecvt` caches are filled with relaxed atomic accesses: every thread stores the same value and nothing else is published through them. |
| C locale lock | Covers participating `setlocale` operations, a separate source of serialization. |

**Shared lifetime does not imply safe publication or safe cache
initialization.** The fast paths avoid repeated locking and
computation, and each needs its own publication. The ThreadSanitizer
investigation found races in punctuation caches, implementation-data
publication and body bookkeeping, including use-after-free during
cached string assignment; see [the investigation](analysis-locale-mt-race.md).
Those are repaired as the table above describes, and a run of the
locale MT tests under the sanitizer reports nothing from the facets
or bodies. The initialization of the global locale went to
`__rw_once` in 5b68c2a7; `analysis-init-flags.md` is the census of
the flag-based initializations and their decisions.

A lock inside initialization alone does not establish that another
thread can safely read the initialized state: the data-loading slow
path always took a mutex, and its unlocked checks were the problem.

## 8. Class `locale`

### 8.1 The standard's contract

14882:2003 clause 22.1 specifies the handle, the facet base, the
facet id, the two access templates and `<clocale>`. The obligations
and freedoms, by paragraph:

- **Category, 22.1.1.1.1.** `category` is `int`; `none`, `collate`,
  `ctype`, `monetary`, `numeric`, `time` and `messages` are bitmask
  elements and `all` is their union. p1: "Further, the result of
  applying operators | and & to any two valid values is valid, and
  results in the setwise union and intersection, respectively, of the
  argument categories." p2: a member taking a category accepts a
  valid value or one of the `LC_` constants of `<clocale>`. p3: every
  locale has every facet of Table 51. p4: Table 52's instantiations
  are required, the byname facets among them. The values in the
  synopsis are "for exposition only".
- **Facet, 22.1.1.1.2.** A protected `explicit facet(size_t refs = 0)`,
  a virtual destructor, copy and assignment private. p1: a facet is a
  class derived from `locale::facet` with a public static `id`. p2:
  `refs == 0` deletes the facet with the last locale that holds it;
  `refs == 1` never deletes it. p3: one-argument constructors are
  explicit. p4: a byname facet behaves as the facet of
  `locale(name)`.
- **Id, 22.1.1.1.3.** p1: the index for lookup. p2, a note: an id may
  be initialized when its facet is first installed, relying on static
  storage being zero.
- **Constructors, 22.1.1.2.** `locale()`, p1-2: a copy of the last
  argument of `global`, else the classic semantics; `throw()`. Copy,
  p3, and assignment, p4-5, `throw()`. `locale(const char*)`, p6-8:
  `runtime_error` for a null or invalid name; valid names are `"C"`,
  `""` and implementation-defined ones. `locale(other, std_name,
  cat)`, p9-11: other but for `cat`, `runtime_error` as p7, named iff
  `other` is named. `locale(other, Facet* f)`, p12-13: a null `f`
  copies; p13: "The resulting locale has no name." `locale(other,
  one, cats)`, p14-15: named iff both arguments are named; no Throws
  clause. Destructor, p16: non-virtual, `throw()`.
- **Members, 22.1.1.3.** `combine<Facet>`, p1-4: `*this` with
  `other`'s `Facet`; `runtime_error` if `!has_facet<Facet>(other)`;
  p4: "The resulting locale has no name." `name`, p5: `"*"` for an
  unnamed locale; `locale(name().c_str())` is equivalent to
  `*this`; the contents are otherwise implementation-defined.
- **Operators, 22.1.1.4.** p1: "Returns: true if both arguments are
  the same locale, or one is a copy of the other, or each has a name
  and the names are identical; false otherwise." p2: `!=`. p3-5:
  `operator()` is `use_facet<collate<charT> >(*this).compare(...) < 0`.
- **Statics, 22.1.1.5.** `global`, p1-3: later `locale()` calls copy
  the argument; for a named argument, `setlocale(LC_ALL, name)`; for
  an unnamed one the effect on the C locale is implementation-defined;
  returns the previous global. `classic`, p4-6: the `"C"` locale,
  equal to `locale("C")`, unchanging.
- **Class-wide, 22.1.1.** p6: a facet reference stays usable while
  some locale refers to the facet. p7: results within one inserter or
  extractor call are identical. p8: "Named locales may be compared
  for equality; an unnamed locale is equal only to (copies of)
  itself."
- **Globals, 22.1.2.** `use_facet`, p1-4: `Facet` has the static `id`;
  `bad_cast` when `has_facet` is false; the reference lives while a
  copy of the locale does. `has_facet`, p5, declared `throw()` in the
  22.1 synopsis and in 22.1.2.
- **Convenience, 22.1.3.** `isspace(c, loc)` and the rest are `is` and
  `toupper`/`tolower` of `use_facet<ctype<charT> >(loc)`; the ctype
  chapter covers them.
- **C library locales, 22.3.** p2: `<clocale>` has the contents of C's
  `<locale.h>`: the six `LC_` macros, `NULL`, `lconv`, `localeconv`,
  `setlocale`.

### 8.2 The implementation

The handle, body, slots, ids, composition, the global locale, data
sources and threading boundaries are chapters 1-5 and 7. This section
maps the contract onto them.

**Files.** `include/loc/_locale.h`: the class, `use_facet`,
`has_facet`, the template constructor, `combine`, `operator()`.
`include/loc/_facet.h`: `__rw_facet`, `__rw_facet_id`.
`src/locale_core.cpp`: default and copy constructors, destructor.
`src/locale_combine.cpp`: the named, category and facet constructors
through `_C_get_body`, `_C_combine`, `_C_construct` and
`__rw_combine_names`, and `_C_get_facet`. `src/locale_body.cpp`: the
body and the table of its 26 facet factories, the classic body, the
global locale, `_C_get_named_body`, `__rw_expand_name`, assignment.
`src/locale_eq.cpp`, `src/locale_name.cpp`, `src/locale_global.cpp`,
`src/locale_classic.cpp`, `src/locale_bits.cpp` (the category
constants' definitions). `src/facet.cpp`, `src/setlocale.cpp`,
`src/mman.cpp`, `src/use_facet.h` and `src/ti_*.cpp` (chapter 2).

**Categories.** `none` is 0x0100 and the six run from 0x0200
(`collate`) to 0x4000 (`messages`); `all` is 0x7e00. No value is an
`LC_` constant. `__rw_locale::_C_LC2category` maps `LC_ALL` and the
six `LC_` constants to categories and takes any other value as a
category; `_C_check_category` accepts `none` or bits within `all`.
On glibc `LC_CTYPE` is 0 (`_RWSTD_LC_CTYPE` in `config.h`). The
constructors read their argument through `_C_arg2category`: 0, the
intersection of disjoint categories, selects nothing (22.1.1.1.1,
p1), so `LC_CTYPE` passed as the extension selects nothing there
too; the name-combining code maps the `LC_` table through
`_C_LC2category`, where 0 is still `ctype`.

**Facet and id.** `locale::facet` is a typedef of `__rw::__rw_facet`,
`locale::id` of `__rw::__rw_facet_id`. `__rw_facet` derives from
`__rw_synchronized` (a mutex) and holds the locale name, its buffer,
the data pointer and size, the type, the reference count and the
pointer to the type's id. The count starts at `refs`; each body that
holds the facet adds one and `~__rw_locale` deletes the facet at zero,
so `refs == 0` dies with the last locale and `refs == 1` never does.
Standard ids are `(type + 1) / 2`, stored once by the type's factory
(`src/use_facet.h`); a user id is the preincrement
of `__rw_id_gen`, which starts at `_C_last_type + 1`, so the first is
54, taken under a static guard (`__rw_facet_id::_C_init`, chapter 2).
A library facet follows the same rule: its factory constructs it with
`refs` 0 and each body adds its reference. A library-made named facet
dies with the last locale that holds it, as decided in f90fff0f; an
ordinary one is a static and survives its count.

**Names.** `locale(const char*)` reaches `_C_get_named_body`, which
expands the name with `__rw_expand_name`. `__rw_locale_name`
(`src/setlocale.cpp`) accepts a name when `$RWSTD_LOCALE_ROOT/<name>`
is a directory, or when `setlocale` accepts it, and then keeps the C
library's spelling. `""` expands per category from `setlocale (cat,
0)` after `setlocale (LC_ALL, "")`. A name that starts with `LC_` is
read as `LC_X=value` pairs: unknown categories are skipped and absent
ones are `"C"`, so glibc's twelve-category string is accepted. A null
or rejected name throws `runtime_error` (`_RWSTD_ERROR_LOCALE_BAD_NAME`,
`src/exception.cpp`). The internal name of a mixed body is six names
joined by `;` in the order of `config.h`'s `_RWSTD_CAT_0` to
`_RWSTD_CAT_5`: `LC_CTYPE`, `LC_NUMERIC`, `LC_TIME`, `LC_COLLATE`,
`LC_MONETARY`, `LC_MESSAGES` on glibc. `name()` returns `"*"` for a
body with no name, one name when all six agree, and otherwise
`LC_CTYPE=...;LC_NUMERIC=...;...`. `locale(x.name().c_str()) == x`
holds for such a name, by probe. A body built from a name is named,
and so is one a category constructor builds from two named locales;
one built with a facet, or combined from an unnamed one, is not (p9,
p11, p13, p15). The category constructors compose the name with
`__rw_combine_names`, each category from the argument its facets come
from, and reduce six equal parts to one, the form `__rw_expand_name`
gives: `locale (de, "de_DE.UTF-8", locale::collate)` is named
`de_DE.UTF-8` and equals `de`, by probe.

**Composition.** The two category constructors go to `_C_combine
(other, cat)`: `none` or the same body is a copy. Anything else is a
new body from `_C_construct`: the standard facets of `cat` from
`other`, every other facet from `*this`, its user facets included
(p9, p14). `all` is not a shortcut to `other`, because the user
facets of `*this` stay. The name-and-category constructor builds the
locale of the name first and combines with it. The template
constructor and `combine` go to `_C_combine (facet)`: the template
constructor passes a null facet for the one the locale already holds
for the type, and a null facet gives a copy (8.3); any other facet
gives a new body, named `"*"`.

**Equality.** `src/locale_eq.cpp`, as 22.1.1.4, p1 has it: one body
is equal; two bodies are equal when both are named and the internal
names are identical, and unequal otherwise. Two locales of one name
built apart compare equal; two built apart with the same facet do
not.

**Access.** `__rw_get_facet` resolves the 26 standard specializations
to `__rw_get_std_facet`: a plain read of the slot, which the body's
constructor filled through the factories `__rw_ct_*` of
`src/use_facet.h` and `src/ctype.cpp`. Every
other type goes to `_C_get_facet`, which initializes the id first. The
result is `dynamic_cast` to `Facet`. `use_facet` throws `bad_cast` on
null; `has_facet` returns the test and has no exception specification.
`combine` throws `runtime_error` (`_RWSTD_ERROR_FACET_NOT_FOUND`)
before it constructs.

**Global and classic.** `locale::global` calls `setlocale (LC_ALL,
name)` under `__rw_setlocale_mutex` unless the name holds `*`, then
swaps the global body under the global locale's mutex
(`_C_set_global`). The two steps take two locks, so two `global`
calls that overlap can leave the C locale from one and the C++
global from the other. That is the caller's to serialize, as calls
to `setlocale` are: 14882:2003 22.1.1.5, p2 describes the calls that
follow one `global`, and the C locale is set for the program's own C
calls, which the C library leaves unsynchronized against `setlocale`
anyway; the library's facets that reach the C library set it for
the duration of their call under the same mutex and never read the
global setting. `classic()`
constructs a static `locale ("C")` through `__rw_once`; the classic
body is static and never destroyed (chapter 4.2).

**Facet data.** A facet maps its data through `_C_get_data` (chapter
5), which reads the database only when the facet's id pointer is set.
A byname facet the program constructs and uses without installing it
has no id pointer and takes the C library path: `collate_byname<char>
("de_DE.UTF-8")` used directly transforms with `strxfrm`, the one in
`locale ("de_DE.UTF-8")` with the database, by probe. The comment at
`src/facet.cpp:169-173` states this as the design.

**Threading.** Chapter 7. `__rw_locale_name` caches the locale root in
a static buffer with no lock of its own. Its one caller,
`__rw_expand_name`, runs under the static guard of
`_C_get_named_body` (`src/locale_body.cpp:568`), which serializes it,
as the body repository's guard did when `analysis-init-flags.md` 2.7
was written.

**`<clocale>`.** `include/rw/_config.h:97-106` defines
`_RWSTD_NO_PURE_C_HEADERS` and `_RWSTD_NO_DEPRECATED_C_HEADERS` unless
`_RWSTD_USE_PURE_C_HEADERS` is given, so `include/ansi/clocale`
includes the C library's `<locale.h>` and declares `std::lconv`,
`std::localeconv` and `std::setlocale` by `using`. The library's own
declarations in `include/ansi/_clocale.h` compile only under
`_RWSTD_USE_PURE_C_HEADERS`.

### 8.3 Departures and extensions

The conformance record's chapter 2 carries one row per departure below, pointing here.

#### Installing the facet a locale holds is a copy

22.1.1.2, p13 and 22.1.1.3, p4: "The resulting locale has no name."
The template constructor compares the facet with the one the locale
holds for its type and, when they are the same object, passes a null
facet, so the result is a copy: named, and equal to the source.
`combine` takes that path whenever both locales hold the same facet,
as every locale does for the eight ordinary `{money,num}_{get,put}`
facets: `de.combine<num_get<char> >(en)` is named `de_DE.UTF-8` and
equals `de`, by probe. Why: e6c7726b; the comment in the constructor
says it does so "to potentially avoid creating a whole new locale
body". Pins: nothing.

#### `has_facet` has no exception specification

The 22.1 synopsis and 22.1.2 declare `has_facet(const locale& loc)
throw()`. The library declares none. `has_facet` of a standard facet
reads a slot the body's constructor filled and throws nothing. The
first `has_facet` of a program's facet type initializes the type's id
under a static guard, and a lock that fails throws `runtime_error`
(`include/rw/_mutex.h`). Why: e6c7726b. Pins: nothing.

#### `use_facet` checks the dynamic type

22.1.2, p2: "Returns: a reference to the corresponding facet of loc,
if present." The library `dynamic_cast`s the facet to `Facet`:
`use_facet<collate_byname<char> >(locale::classic ())` throws
`bad_cast`, and on a named locale returns the locale's own `collate`
facet. Why: e6c7726b, the comment in `use_facet`. Pins:
`22.locale.globals`, `test_byname`, for every byname facet. An
extension.

#### An invalid category throws

22.1.1.2, p14 has no Throws clause. `locale (one, other, cat)` with a
category outside `all` throws `runtime_error`
(`_RWSTD_ERROR_LOCALE_BAD_CAT`); the comment calls it a "missing
requirement". The name-and-category constructor checks the category
first and throws the same, since a bad category and a malformed
combined name both come back from `_C_get_body` as a null body; it
threw the bad-name message for either until 2026-11-03. Why:
e6c7726b. Pins: nothing; by probe. An extension.

#### `locale::facet` and `locale::id` are the library's classes

The 22.1.1 synopsis declares `class facet;` and `class id;` as
members. The library's are typedefs of `__rw::__rw_facet` and
`__rw::__rw_facet_id`; the facet carries a mutex base and public
members with reserved names (`_C_opts`, `_C_data`, `_C_get_name`).
Why: e6c7726b. Pins: nothing. Visible only through reserved names.

#### A mixed name does not reach the C library

22.1.1.5, p2: "If the argument has a name, does
std::setlocale(LC_ALL, loc.name().c_str())". The library makes the
call. glibc accepts a composite name only when it lists all twelve of
its categories; the library's lists six, so `setlocale` fails and the
C locale stays as it was, by probe. A name only the database knows
fails the same way. Why: e6c7726b; the comment at the call "assumes
all locale names (i.e., including those of combined locales) are in a
format understandable by setlocale()", and `__rw_cats` names six
categories in `setlocale`'s order. Pins: nothing.

### 8.4 What the tests pin

| program | covers | 15D |
|---|---|---|
| `22.locale.globals` | `has_facet` and `use_facet` of every standard facet, narrow and wide, in the classic and every named locale; byname facets present in named locales, `bad_cast` in the classic one | 78320 |
| `22.locale.cons.mt` | one named locale constructed by four threads at once; the name and two-locale combinations and their equalities | 800 |
| `22.locale.facet.mt` | facet reference counts | 8 |
| `22.locale.id.mt` | the first use of a user facet id | 999 |
| `22.locale.statics.mt` | racing `locale::global`, a multiset check of the globals | 200 |
| `22.locale.use_facet.mt` | `use_facet` of each of the 26 standard facets by four threads at once on a new named locale; the facets die with it | 200 |
| `22.locale.cons.stdcxx-485` (regress) | constructors from several threads, `has_facet<collate<char> >` | 128 |

No single-threaded test exercises the constructors' names,
`combine`, `operator==` of composed locales, the category values or
`global`'s effect on the C locale; every departure in 8.3 but
`use_facet`'s is unpinned. No row fails on the 15D pin. `TODO` "Tests
that GCC and Clang let through" names `22.locale.id.mt.cpp:98`, a
template instantiation 435 deep, for EDG.

### 8.5 Open

Nothing open.

## 9. ctype and codecvt

The `ctype` category holds classification and case mapping
(`ctype`) and encoding conversion (`codecvt`). Chapters 2, 5 and 7
give their slots, data sources and cache publication.

### 9.1 The standard's contract

ISO/IEC 14882:2003, clauses 22.1.3 and 22.2.1.

#### ctype_base and the convenience functions

- 22.2.1 p1: "The type mask is a bitmask type." The synopsis gives
  `alnum` as `alpha|digit` and `graph` as `alnum|punct`, its values
  for exposition only. C++03 has no `blank`.
- 22.1.3.1 p1: each `isF (c, loc)` returns
  `use_facet< ctype<charT> >(loc).is (ctype_base::F, c)`.
- 22.1.3.2 p1-p2: `toupper (c, loc)` and `tolower (c, loc)` return
  the facet's `toupper (c)` and `tolower (c)`.

#### ctype<charT> and ctype_byname<charT>

22.2.1.1 p2 requires `ctype<char>` and `ctype<wchar_t>`, classing "the
implementation's native character set". 22.1.1.1.1 p4 requires the
byname forms (Table 52). The public members return their virtual
(22.2.1.1.1 p1-p7). The virtuals (22.2.1.1.2):

- `do_is (m, c)` returns `(M & m) != 0` for the character's mask M;
  the vector form stores each M in `vec[p-low]` and returns `high`
  (p1-p2).
- `do_scan_is` and `do_scan_not` return the first `p` in
  `[low, high)` for which `is (m, *p)` is true, or false, else `high`
  (p3-p6).
- `do_toupper` and `do_tolower` return the counterpart "if it is
  known to exist, or its argument if not"; the vector forms replace
  each character that has one and return `high` (p7-p10).
- `do_widen` applies "the simplest reasonable transformation";
  unique results are required only for the basic source character
  set, and a widened character joins no class the narrow one lacks
  (p11-p12).
- `do_narrow` returns the transformed value "or dfault if no mapping
  is readily available"; for the basic source set
  `do_widen (do_narrow (c, 0)) == c`, and a digit narrows to its
  value plus `'0'` (p13-p14).

`ctype_byname` (22.2.1.2) adds a constructor and the same virtuals,
without text.

#### ctype<char> and ctype_byname<char>

- 22.2.1.3 p1: the specialization exists so the members can be
  inline; "The implementation-defined value of member table_size is
  at least 256."
- 22.2.1.3.1 p1: the destructor does `delete [] table()` if the
  constructor's table was nonzero and `del` was true.
- 22.2.1.3.2 p1-p7: `is`, `scan_is` and `scan_not` are not virtual;
  they read `table()[(unsigned char)c]`, and the vector `is` assigns
  that value into `vec[p-low]` (p4). p8-p11: `toupper`, `tolower`,
  `widen` and `narrow` return the virtual's result.
- p12: `table()` returns the constructor's table if nonzero, else
  `classic_table()`; 22.2.1.3.3 p1: `classic_table()` is the "C"
  locale's classifications.
- 22.2.1.3.4: the eight virtuals "are described identically" to the
  primary template's. 22.2.1.4 gives `ctype_byname<char>` the same
  eight and a constructor.

#### codecvt and codecvt_byname

- 22.2.1.5 p3: `codecvt<char, char, mbstate_t>` does not convert;
  `codecvt<wchar_t, char, mbstate_t>` converts between the native
  narrow and wide sets; "Instantiations on mbstate_t perform
  conversion between encodings known to the library implementor."
- 22.2.1.5.1 p1-p7: each public member returns its virtual.
  22.2.1.5.2 p1: the precondition is a valid range and a state that
  is initial or the result of the preceding conversion.
- p2: `do_in` and `do_out` stop at a character they cannot convert
  and leave `from_next` and `to_next` one past the last converted
  element; on `noconv`, `to_next` is `to` and the state is unchanged.
- p3: "Its operations on state are unspecified."
- p4, Table 53: `ok`, `partial` (not all source converted, or more
  source needed), `error`, `noconv`.
- p5-p6, Table 54: `do_unshift` stores the sequence that returns the
  state to initial; the two required instantiations store nothing,
  and `codecvt<char, char, mbstate_t>` returns `noconv`.
- p7-p8: `do_encoding` returns -1 for a state-dependent encoding,
  the constant count of external elements per character, or 0;
  `do_always_noconv` is true if `do_in` and `do_out` always return
  `noconv`, as `codecvt<char, char, mbstate_t>`'s does.
- p9-p10: `do_length` returns `from_next - from` for the longest
  prefix of at most `max` complete internal characters; "The
  instantiations required in Table 51 (22.1.1.1.1), namely
  codecvt<wchar_t, char, mbstate_t> and codecvt<char, char,
  mbstate_t>, return the lesser of max and (from_end-from)."
- p11: `do_max_length` is the largest value `do_length (..., 1)` can
  return; 1 for `codecvt<char, char, mbstate_t>`.

`codecvt_byname` (22.2.1.6) is a constructor and the seven virtuals,
without text.

### 9.2 The implementation

Headers: `include/loc/_ctype.h`, `_ctype.cc`, `_codecvt.h`,
`_codecvt.cc`, `_convenience.h`. Sources: `src/ctype.cpp`,
`src/ctype_bits.cpp`, `src/wctype.cpp`, `src/codecvt.cpp`,
`src/wcodecvt.cpp`, `src/iso2022.cpp`. There are no `src/ti_*` files
for the family: every required type is an explicit specialization
compiled in its source file, and the primary templates are defined
in the headers.

#### Masks, ids and factories

- `ctype_base::mask` is `__rw::__rw_ctype_mask`, an enumeration with
  bitmask operators: ten bits, `space` 0x001 to `graph` 0x200, and
  `alnum` as `alpha|digit`. `graph` has a bit of its own.
  `ctype_bits.cpp` defines the static members and `table_size`.
- `ctype<char>` is made by `__rw_ct_ctype` in `src/ctype.cpp`: the
  classic facet in static storage over `__rw_classic_tab`, a byname
  facet with `new`. The other three come from
  `_RWSTD_DEFINE_FACET_FACTORY`.
- Since dd8f4349 a facet takes its database when one exists and the
  C library otherwise, through `__rw_setlocale`.

#### ctype<char>

- `_C_mask_tab`, `_C_upper_tab`, `_C_lower_tab` point at
  `__rw_classic_tab`, `__rw_upper_tab`, `__rw_lower_tab` (ASCII; bytes
  0x80-0xff have mask 0 and map to themselves). `table_size` is 256.
  `is`, `scan_is`, `scan_not` index the mask table; the vector `is`
  copies the entry as it is.
- `do_toupper` and `do_tolower` index the case tables. `do_widen`
  and `do_narrow` are the identity, ignore `dfault`, and copy ranges
  with `memcpy`.
- `narrow` and `widen` are inline and cache the virtual's result in
  `_C_narrow_tab [256]` and `_C_wide_tab [256]`, 0 meaning empty;
  `narrow` stores only a result unequal to `dfault`. The fills are
  relaxed atomics (bf6dc721).
- The destructor deletes the mask table when `del` was set and a
  table was given, and the case tables when they are not the classic
  ones.

#### ctype_byname<char>

The constructor returns at once for "C". Otherwise it maps the
locale's `LC_CTYPE` record with `__rw_get_facet_data` and points the
three tables into it: `mask_tab`, `toupper_tab`, `tolower_tab` of
`__rw_ctype_t`. Without a database it sets the C locale through
`__rw_setlocale`, which throws for a name the C library does not
know, and builds the three tables from `isspace` ... `isgraph`,
`toupper` and `tolower`, once. It overrides no virtual.

#### ctype<wchar_t>

- The same six members as `ctype<char>`, over `__rw_classic_tab`.
  Under `_RWSTD_NO_EQUAL_CTYPE_MASK`, which glibc does not set, the
  mask table is built once from `iswspace` ... in "C", through
  `__rw_once`.
- `do_is`, `do_scan_is`, `do_scan_not`, `do_toupper`, `do_tolower`
  read the 256-entry tables for characters below 256. A character
  at or above 256 is in no class and maps to itself.
- `do_widen` widens a byte to the same value, 0x80-0xff included.
  `do_narrow` narrows 0-127 and returns `dfault` above. So
  `narrow (widen ('\xe9'), '?')` is `'?'` in the classic facet.
- `narrow` and `widen` cache as `ctype<char>`'s do.

#### ctype_byname<wchar_t>

The constructor maps `LC_CTYPE` as the narrow one does and also the
codeset's codecvt database (`_C_cvtimpl`, released in the
destructor). Every virtual is overridden:

- `do_is`, `do_scan_is`, `do_scan_not` call `__rw_get_mask`: a
  binary search of the `(ch, mask)` array `wmask_tab`. A character
  missing from the table is in no class: a mismatch scan stops at it,
  a match scan passes it. The vector `do_is` masks each entry with
  `__rw_all`.
- `do_toupper` and `do_tolower` call `__rw_toupper` and
  `__rw_tolower`: binary searches of the `(lower, upper)` and
  `(upper, lower)` arrays.
- `do_narrow` returns ASCII 1-126 as is; otherwise it encodes the
  character as UTF-8 with `__rw_itoutf8`, walks the `w_to_n` trie and
  returns the byte when the external string is one byte long, else
  `dfault`.
- `do_widen` returns 0-127 as is; otherwise it reads `n_to_w` at the
  byte and returns `WEOF` cast to `wchar_t` for a lead byte or an
  unmapped one.
- Without a database each member sets the C locale and calls
  `iswspace` ... `towupper`, `wctob` or `btowc`, per call, under the
  process-wide lock. The constructor only checks the name through
  `__rw_setlocale` then; the base class's three tables stay the
  classic ones, which no override reads.

#### codecvt<char, char, mbstate_t> and codecvt_byname<char, char, mbstate_t>

- `do_out` sets the next pointers to the start, returns `error` on a
  state `mbsinit` rejects, then `noconv` while `always_noconv()` is
  true. A derived `do_always_noconv` that returns false makes it copy
  with `memmove` and return `partial` when the destination is short
  (LWG 382). `do_in` calls `do_out` qualified.
- `do_unshift` returns `noconv`, `do_encoding` 1, `do_always_noconv`
  true, `do_max_length` 1, `do_length` the lesser of `max` and the
  range.
- `always_noconv` caches the virtual's answer in `_C_always_noconv`,
  -1 meaning unknown, relaxed (bf6dc721).
- The byname form only records its name.

#### codecvt<wchar_t, char, mbstate_t>

The classic wide facet is one byte per character. `do_in` widens each
byte as `unsigned char`; `do_out` stores the low eight bits of each
`wchar_t`; both return `partial` when the destination fills first.
`do_unshift` returns `noconv`, `do_encoding` and `do_max_length` 1,
`do_always_noconv` false, `do_length` the lesser of `max` and the
range. Every member checks the state with `mbsinit`.

#### codecvt_byname<wchar_t, char, mbstate_t>

The constructor reads the name into `_C_flags`:

- `__rw_encoding_from_name` recognizes `ISO-2022-JP`, `-JP-2`, `-KR`
  and `-CN`.
- `@UCS`, `@UCS-4`, `@UCS-2` and the byte-order forms set a UCS
  internal encoding and are stripped from the name.
- `UTF-8`, `UTF-16`, `UTF-16-BE`, `UTF-16-LE` and their lower-case
  spellings set a UTF type and empty the name; the upper-case
  spelling adds `__rw_strict`.

Each virtual switches on the ISO-2022 type. For a stateless name it
takes `_C_data ()`:

- `__rw_facet::_C_get_data` maps the locale's `LC_CTYPE` record,
  takes its `codeset_name ()` and maps the codecvt database of that
  name at the locale root; a name that is itself a codeset maps
  directly. A facet constructed without a locale has no `_C_pid` and
  gets no data.
- With data, or with a UTF or UCS flag, the converters are the
  library's (`USE_LIBC` false). `__rw_libstd_do_in` walks `n_to_w`
  with `__rw_mbtowco` and takes the pair table's internal or UCS
  value. `__rw_libstd_do_out` encodes the `wchar_t` as UTF-8, walks
  `utf8_to_ext` under `@UCS` and `w_to_n` otherwise, and copies the
  NUL-terminated external string. With no data the algorithm is
  UTF-8 (`__rw_utf8toucs4`, `__rw_itoutf8`); strict mode adds
  `__rw_utf8validate` and rejects surrogates and U+FFFE-U+FFFF on
  output; relaxed mode does not check continuation bytes.
- Without data or flags, the C library: `mbrtowc`, `wcrtomb`,
  `mbrlen`, `mbtowc (0, 0, 0)` and `MB_CUR_MAX` under
  `__rw_setlocale`.
- `do_unshift` returns `noconv` for a stateless encoding.
  `do_encoding` is 1 for a database whose `mb_cur_max` is 1, else 0.
  `do_max_length` is the database's `mb_cur_max`, or `MB_CUR_MAX`;
  6 for the UTF-8 names.
  `do_length` walks the same tries as `do_in` and counts the bytes
  of at most `max` complete characters.
- The library-database paths check the state with `mbsinit`, assert
  in a debug build and return `error` (0 for `length`). The C-library
  `in` and `length` take the state as it comes: a stateless
  encoding's state carries the bytes of a character split across
  calls, which `mbrtowc` processes with a -2 (C99 7.24.6.3.2, p4),
  and `in` then consumes them and returns `partial`; `out` still
  checks. A corrupt state on the C-library path is the caller's, as
  C leaves it (`analysis-codecvt-corrupt-state.md`).
- `ISO-2022-JP` and `-JP-2` go to `src/iso2022.cpp`. The
  `mbstate_t` holds an index into a static array of 511 states,
  allocated on first use and freed on a return to the initial shift
  state; the component codesets' databases (`EUC-JP`, `GB2312`,
  `EUC-KR`, `ISO-8859-1`, `ISO-8859-7`) are mapped once. `-KR` and
  `-CN` return `error` and 0 from every member.

#### The primary templates

`ctype<charT>`: `do_is` false, the vector form all zero masks,
`do_toupper` and `do_tolower` the identity, `do_widen` `charT ()`,
`do_narrow` `dfault`; the scans and range forms loop over the single
forms (`_ctype.cc`). `codecvt<I, E, S>`: `do_in`, `do_out` and
`do_unshift` return `noconv` when the two types have one size and
`error` otherwise, with the next pointers at `from` and `to` (p2, p5);
`do_always_noconv` follows the sizes; `do_encoding` -1; `do_length`
the lesser of `max` and the range for one size, 0 otherwise (p10);
`do_max_length` 1 or 0 (p11).
`ctype_byname<charT>` and `codecvt_byname<I, E, S>` only record the
name.

#### The database records

`include/loc/_localedef.h`; offsets count from the end of the struct.

- `__rw_ctype_t` (one `LC_CTYPE` file, shared as
  `<name>.ctype.<charmap>`): `codeset_off`, `charmap_off`, `wmask_s`,
  `wtoupper_off`, `wtolower_off`, `wmask_off`, `ctype_ext_off`; the
  narrow `toupper_tab [256]`, `tolower_tab [256]`, `mask_tab [256]`
  (32-bit words); `mb_cur_max`. After it the `__rw_upper_elm`,
  `__rw_lower_elm` and `__rw_mask_elm` arrays, ascending by
  character, then the two names.
- `__rw_codecvt_t` (one file per codeset at the locale root): eight
  offsets, `codeset_off`, `charmap_off`, `n_to_w_tab_off`,
  `w_to_n_tab_off`, `utf8_to_ext_tab_off`, `wchar_off`,
  `xliteration_off`, `codecvt_ext_off`, and `mb_cur_max`. A trie is
  pages of 256 words: `UINT_MAX` for none, bit 31 and a page number
  to continue, or a leaf.

The writers are `write_ctype` (`util/localedef/wctype.cpp`) and
`util/localedef/wcodecvt.cpp`:

- `LC_CTYPE` (b9e33044) takes POSIX's automatic inclusions (XBD
  7.3.1, `plan-localedef-backend.md` 3.2) and does not write `blank`
  (3.1).
- The wide arrays cover the characters of the charmap, not every
  code point the source names (decided in
  `plan-localedef-backend.md` 3.3; `TODO`, "The wide `LC_CTYPE`
  table covers the charmap").
- A narrow entry is written for a character the charmap encodes in
  one byte, by its first definition.
- The codecvt writer (171a8670) writes `n_to_w` and `w_to_n`, sets
  `utf8_to_ext_tab_off` equal to `w_to_n_tab_off` (decided in
  `plan-localedef-backend.md` 2.1), and writes one empty
  transliteration page that the runtime never reads (`__rw_xlit`'s
  only caller is compiled under `if (0)`; `TODO`, "The
  transliteration page and its dead reader").

#### The state type

`_RWSTD_MBSTATE_T` comes from `include/rw/_mbstate.h`; on glibc it is
`__mbstate_t` from `<bits/types/__mbstate_t.h>`, without `<wchar.h>`
(`analysis-glibc-mbstate_t.md` ch. 5).

#### The convenience interfaces

`include/loc/_convenience.h` defines the thirteen templates exactly
as 22.1.3 writes them, names parenthesized against the C macros.

### 9.3 Departures and extensions

#### Primary template definitions

22.1.1.1.1 p4 requires "those instantiations for facet templates
identified as members of a category". The library also defines
`ctype<charT>` and `codecvt<I, E, S>` for any type, with the
degenerate behaviour of 9.2, so that streams on `signed char` or
`unsigned char` instantiate (the comment in `_codecvt.h`). Why: the
original design, kept when 831519bf removed the gates. Pinned by: no
test. Conformance record: "Primary template definitions".

#### Encodings known to the library

22.2.1.5 p3: "Instantiations on mbstate_t perform conversion between
encodings known to the library implementor." Beyond the locale
names, `codecvt_byname<wchar_t, char, mbstate_t>` takes `UTF-8`
(strict) and `utf-8` (relaxed), the `@UCS` modifiers, and
`ISO-2022-JP` and `-JP-2`. The UTF-16 names and `ISO-2022-KR` and
`-CN` are recognized but not implemented. Why: the original design
(e6c7726b). Pinned by: `22.locale.codecvt` (the UTF-8 rows, the
`@UCS` `in` rows) and `22.locale.codecvt.length` (`UTF-8@UCS`); the
ISO-2022 converters by no test.

#### Cached narrow, widen and always_noconv

22.2.1.3.2 p10-p11 and 22.2.1.1.1 p6-p7 make `widen` and `narrow`
return their virtual; 22.2.1.5.1 p5: "Returns: do_always_noconv()".
The library calls each virtual once per character and facet and
returns the stored result after; a `do_narrow` result equal to
`dfault` is not stored. A derived virtual whose answer changes, or
that counts its calls, sees the difference. Why: the original design
(e6c7726b); the fills became relaxed atomics in bf6dc721. Pinned by:
`22.locale.ctype.mt` (800) for the narrow and widen caches; no test
for `always_noconv`'s.

#### length counts complete characters

22.2.1.5.2 p10 says the required instantiations, `codecvt<wchar_t,
char, mbstate_t>` among them, "return the lesser of max and
(from_end-from)". The byname facet obtained as that type from a named
locale returns the bytes of at most `max` complete characters; the
classic facet, one byte per character, gives the same number either
way. Why: the original design; the comment over
`__rw_libstd_do_length` quotes the resolution of LWG 305, which
narrows the sentence to `codecvt<char, char, mbstate_t>`. Pinned by:
`22.locale.codecvt.length` (484) and the `length` column of
`22.locale.codecvt` (`analysis-test-fixtures.md` 4.2).

#### An invalid state is an error

22.2.1.5.2 p1 and p9 make an initialized state a precondition; the
standard has no invalid `mbstate_t`. Every path of the library's own
asserts `mbsinit` in a debug build and returns `error`, or 0 from
`length`, otherwise. The C-library `in` and `length` of the wide
byname facet take the state as the C library takes it, since under
glibc a carried half character and a corrupt state answer `mbsinit`
alike; a corrupt state there is the caller's, as C99 7.24.6 p3
leaves it, and under glibc's UTF-8 `mbrtowc` does not return from
one. Why: a decision of 2026-09-16 (2a4b5c5f), completing the
original idiom (`analysis-codecvt-invalid-state.md` ch. 3), narrowed
on 2026-11-04 after 10ad3c6c carried the half character
(`analysis-codecvt-corrupt-state.md`). Pinned by:
`test_invalid_args` in `22.locale.codecvt`, which does not run the
section against the two libc-backed facets.

#### The classic wide facet narrows by truncation

22.2.1.5.2 p2: "Stops if it encounters a character it cannot
convert." `codecvt<wchar_t, char, mbstate_t>::do_out` stores the low
eight bits of every `wchar_t` and returns `ok`: L'\x100' becomes
`'\0'` and U+20AC becomes `'\xac'`, by probe. Why: the original
design (e6c7726b); no recorded decision. Pinned by: no test.

### 9.4 What the tests pin

| test | covers | 15D |
|---|---|---|
| `22.locale.ctype` | classification, case, narrow, widen; classic, byname over generated and libc locales | 91101 |
| `22.locale.ctype.is` | `is`, both forms, both widths | 25438 |
| `22.locale.ctype.scan` | `scan_is`, `scan_not` | 17132 |
| `22.locale.ctype.tolower` | `tolower`, both forms | 19396 |
| `22.locale.ctype.toupper` | `toupper`, both forms | 19396 |
| `22.locale.ctype.narrow` | `narrow`, `widen` against the C library | 9768, 2 warnings |
| `22.locale.ctype.mt` | first fill of the narrow and widen caches, gated | 800 |
| `22.locale.codecvt` | the virtuals of the four facets, `in`/`out` tables, the UTF-8 names, five generated locales against `tests/etc` fixtures and glibc, `@UCS`, invalid state | 618 |
| `22.locale.codecvt.do_out` | `out` over algorithmic, table and libc encodings | 1494 |
| `22.locale.codecvt.length` | `length` over the same three, and the byname narrow facet | 484 |
| `22.locale.codecvt.mt` | first conversion through the wide facet of `rwtest_de_DE.ISO-8859-1`, gated | 1600 |
| `22.locale.codecvt.stdcxx-435` | `in` of one character in a multibyte locale | NOUT |
| `27.filebuf.codecvt` | a derived `codecvt<char, char, mbstate_t>` under `basic_filebuf` | 205514 |

Rows from `doc/notes/baseline/15D.txt`; NOUT is a regression program
that writes no table. The two warnings of `ctype.narrow` are its
`test_libstd`, which says the library's own `ctype` is "not
exercised" there.

Known gaps and their `TODO` entries:

- "Two ctype test defects": `ctype.is` line 822 tests `'\xa0'` where
  `'\xa2'` is meant; `tolower` and `toupper` have an unbraced `TEST`.
- "`22.locale.codecvt` counts 634 assertions in 11S and 638 in 11s":
  the 16 between 15D and 11S are the skipped libc facets; the other
  4 are unexplained.
- "A codecvt oracle": none exists.
- Not covered by any row: `ctype<wchar_t>` at or above 256 beyond
  the classic case maps' one row each, the
  classic wide `out` above 0xff, the ISO-2022 converters.

### 9.5 Open

- The wide table covers the charmap, glibc's every named code point
  (`TODO`, "The wide `LC_CTYPE` table covers the charmap"). Check:
  the decision recorded.
- A `codecvt_byname<wchar_t, char, mbstate_t>` constructed and used
  outside a locale has no `_C_pid`, so `_C_get_data` gives it no
  database (the comment in `src/facet.cpp`): it converts through the
  C library, and for a name only the database knows it throws
  `runtime_error` from `__rw_setlocale` on the first call, by probe.
  `ctype_byname<wchar_t>` maps its database in the constructor and
  has no such limit. Under `@UCS` the same facet converts as UTF-8.
  Check: a decision whether a byname facet's data depends on its
  installation; the probe is a facet on a database-only name used
  directly and through `locale (classic (), &f)`.
- `ctype_byname<wchar_t> ("C")` takes the C library's answers where
  `ctype<wchar_t>` takes the tables: `widen ('\x80')` is `WEOF` in
  the first and U+0080 in the second, by probe. Check: whether the
  "C" byname facet should share the classic tables.
- The classic wide facet widens 0x80-0xff and narrows only 0-127, so
  `narrow (widen (c))` loses the upper half; its `codecvt` keeps it.
  22.2.1.1.2 p13 asks the round trip only for the basic source set.
  Check: a decision with the truncation of 9.3.
- `ctype_byname<wchar_t>::do_widen` and `do_narrow` treat bytes and
  characters below 0x80 as ASCII without the table. A charmap that
  is not ASCII there, glibc's `SHIFT_JIS` with U+00A5 at 0x5C, would
  disagree with its own `codecvt`. Check: `widen ('\\')` against
  `in` under `ja_JP.Shift_JIS` as the codecvt test builds it.
- `_RWSTD_CTYPE_*`, the C class masks the configuration emits, have
  no reader under `include` or `src`. Check: `git grep`, and a
  decision whether the configuration keeps them.

## 10. The numeric and monetary facets

`num_get`, `num_put`, `numpunct` and `numpunct_byname` are 22.2.2
and 22.2.3 of ISO/IEC 14882:2003; `money_get`, `money_put`,
`moneypunct` and `moneypunct_byname` are 22.2.6. The get and put
facets hold no data and have no byname form (chapter 2); they read
the punctuation facets of the stream's locale on every call. "By
probe" marks a fact run against a 15D build of the library.

### 10.1 The standard's contract

#### num_get

- **Integral and `void*` `do_get`** (`long`, `unsigned short`,
  `unsigned int`, `unsigned long`, `void*`), 22.2.2.1.2. Stage 1 takes
  the `scanf` specifier from `basefield` (Table 55: `%o`, `%X`, `%i`
  for an empty field, `%d` or `%u`; p6: `%p`). Stage 2 matches the
  widened atoms `"0123456789abcdefABCDEF+-"`, maps `decimal_point()`
  to `.` and discards `thousands_sep()` when `grouping()` is not empty
  (p8). Stage 3 converts by `scanf`'s rules: `goodbit` and the value,
  or `failbit` (p11); misplaced separators assign `failbit` (p12); a
  stage 2 that met `end` adds `eofbit` (p13). p1: "If an error occurs,
  val is unchanged; otherwise it is set to the resulting value."
- **Floating `do_get`**: the same with `%g` (p5).
- **`do_get (..., bool&)`**: without `boolalpha` a `long` is read and
  0 or 1 stored, anything else `failbit` and no store (p14); with it,
  `falsename()` and `truename()` are matched only as far as a unique
  match needs (p15), and `eofbit` joins the result that met `end`;
  empty names fail every input (p16).
- 22.2.2.1.1 lists `short&` where the synopsis has `float&`, a slip of
  the text; the class has no `short` or `int` members.

#### num_put

- **`do_put`** for `long`, `unsigned long`, `double`, `long double`,
  `const void*`, 22.2.2.2.2. Stage 1 is `printf` in the "C" locale with
  the specifier of Tables 57 to 60. p11: "if (flags & fixed) != 0 or
  if str.precision() > 0, then str.precision() is specified in the
  conversion specification." Stage 2 widens, inserts separators "For
  integral types" (p16) and maps `.` to `decimal_point()` (p17).
  Stage 3 pads by Table 61 and calls `width(0)` (p19 to p21).
- **`do_put (..., bool)`**: `do_put(out, str, fill, (int)val)` without
  `boolalpha`, a call no 2003 virtual matches uniquely; the characters
  of `truename()` or `falsename()` with it (p23).

#### numpunct and numpunct_byname

- The required instantiations hold the "C" data (22.2.3.1 p1): `.`,
  `,`, empty grouping, `"true"`, `"false"` (22.2.3.1.2 p1 to p4).
- `do_grouping`, p3: group `i` from the right has `vec[i]` digits, a
  missing element repeats the last, and "if (i<0 || vec[i]<=0 ||
  vec[i]==CHAR_MAX), the size of the digit group is unlimited."
- `numpunct_byname` (22.2.3.2) gives the named locale's values; the
  standard fixes none.

#### money_get

- **Both `do_get`** (`units`, `digits`), 22.2.6.1.2 p1: the pattern is
  `neg_format()` for all values; a valid sequence leaves `err` alone,
  an invalid one adds `failbit` (and `eofbit` at the end) and leaves
  `units` and `digits` alone; separators are not read without grouping
  and are checked last with it.
- p2: white space is optional where `space` or `none` appears, except
  at the end; the symbol is optional and read only when needed unless
  `showbase` makes it required. p3: the first character of a sign is
  matched at `sign`, the rest after everything else; an empty sign
  makes the component optional. p4: `units` is `sscanf ("%Lf")` of the
  digits.
- 22.2.6.3 p3: "The number of digits required after the decimal point
  (if any) is exactly the value returned by frac_digits()."

#### money_put

- **Both `do_put`**, 22.2.6.2.2 p1: `units` becomes characters as if by
  `sprintf (buf1, "%.01f", units)`, which C++11's 22.4.6.2.2 p1 reads
  `"%.0Lf"`; a leading `-` selects `neg_format()`; of `digits`, only a
  leading `-` and the digits after it are used; `width(0)` is called.
- p2: the symbol is written if and only if `showbase` is set; fill
  goes at `none` or `space` under `internal`, after under `left`,
  before otherwise.

#### moneypunct and moneypunct_byname

- 22.2.6.3 p1: `symbol`, `sign`, `value` and one of `space` or `none`
  appear once each; `none` is not first, `space` neither first nor
  last. p2: `space` requires a space.
- 22.2.6.3.2 p1 to p6: the nine virtuals; footnote 242 makes the
  international symbol four characters, "USD ". p7: the required
  instantiations return `{ symbol, sign, none, value }`. No other "C"
  value is fixed. `moneypunct_byname` (22.2.6.4) gives the named ones.

### 10.2 The implementation

#### num_get

`include/loc/_num_get.h` declares the nine 2003 virtuals and two
`long long` ones (10.3.1). Each non-`bool` virtual calls the private
`_C_get` (`include/loc/_num_get.cc`) with a type tag of `__rw_facet`.
`_C_get` is stages 1 and 2. The base is 16 for `void*`, 0 for an
empty `basefield`, 8 for `oct`, 16 for `hex`, 10 otherwise; base 0
becomes 8 after a leading `0` and a digit below 8, 16 after `0x`. A
character is classified by `ctype::narrow` and `__rw_digit_map`, not
by a search of the widened atoms. The digits go, behind a `+` that a
sign overwrites, into a 130-byte stack array, the group lengths into
a second. When either fills, both move to one heap block of twice the
size, freed on every exit by `__rw_num_get_buffer` (5d218165); a size
that would wrap throws `bad_alloc` (e2bc861d). A separator after the
decimal point or the exponent ends the field.

Stage 3 is `__rw_get_num` (`src/num_get.cpp`). Integers go through
the library's `__rw_strtol`, `__rw_strtoul`, `__rw_strtoll` and
`__rw_strtoull` (`src/strtol.cpp`), which negate a negative input
modulo 2^N for an unsigned type, then a clamp per tag (10.3.6).
Floating values go through `strtof`, `strtod` or `strtold` in the
global C locale; where its radix stopped the parse at `.`, the value
is reparsed under `__rw_setlocale ("C", LC_NUMERIC)`, which takes the
C-locale lock and switches the process's category. `__rw_validate`
maps `ERANGE` to infinity with `failbit` on overflow and to the
minimum normal value with `goodbit` on underflow.
`__rw_check_grouping` compares the groups with `grouping ()`.

`do_get (..., bool&)` reads a `long` through a qualified call to
`do_get (..., long&)`, not a virtual one; under `boolalpha` it matches
the names with `__rw_match_name` (`include/loc/_punct.cc`),
case-sensitively. `__rw_check_overflow_short` and `_int` in the
header serve the `short` and `int` extractors of `basic_istream`
(444888d6).

#### num_put

Each of the eight virtuals calls `_C_put` (`include/loc/_num_put.cc`)
with a tag; `bool` without `boolalpha` goes as `_C_int`, through no
virtual. `_C_put` formats into a 308-byte stack array
(`_RWSTD_DBL_MAX_10_EXP`) through `__rw_put_num` (`src/num_put.cpp`),
a heap block when the output outgrows it, freed on every exit by
`__rw_put_buffer` (`include/loc/_punct.h`) as `num_get`'s is, resets
`width` before output
(69ad31e6), pads, and maps the
placeholders `.` and `,` to `decimal_point()` and `;` to
`thousands_sep()`. `__rw_put_num` writes integers itself
(`__rw_dtoa`, `__rw_itoa`; no `showbase` prefix for zero), floating
values through `snprintf` with the specifier of
`__rw_get_stdio_fmat` (`src/punct.cpp`), retried in a heap block of
exactly the needed size, and infinity and NaN through
`__rw_fmat_infinite`. A null `void*` is `0`, not glibc's `(nil)`; a
pointer is not grouped. `__rw_put_groups` inserts the `;`
placeholders in place, in the integral part of every arithmetic type,
and moves to a heap block of its own when the separators do not fit,
freeing one it grew before; it reports the new size back through its
size argument, and `money_put`'s string path, which calls it for the
group sizes, guards its buffer the same way.

#### numpunct and moneypunct

The virtuals of `include/loc/_numpunct.h` call
`__rw_get_punct (this, which, char_type ())`; those of
`include/loc/_moneypunct.h` call it through `_C_get`, which adds
`__rw_mon` and, for `International`, `__rw_intl`. The overloads for
`char` and `wchar_t` are in `src/punct.cpp`; the primary template
asserts "must not be called" (10.3.12). The members call the virtuals
every time; nothing is cached (3b046e29). The byname classes only
store their name in a 32-byte buffer and override nothing: the base
virtuals read the data of the name the facet carries.

`__rw_get_numpunct` and `__rw_get_moneypunct` have three sources:

- the database, mapped by `_C_data ()` (chapter 5);
- for an unnamed facet, the "C" tables: `.`, `,`, `""`, `"true"`,
  `"false"`; for money, empty symbol and signs, `frac_digits` 0 and
  the pattern `"\2\3\0\4"`, p7's;
- for a named facet without a database, `localeconv ()` under
  `__rw_setlocale` for `LC_ALL`, copied into a heap record of the
  database's layout (wide strings by `mbstowcs`, names `"true"` and
  `"false"`) and published with release stores (chapter 7). The money
  patterns come from a 30-entry table indexed `cs_precedes * 15 +
  sep_by_space * 5 + sign_posn`; an index past it gives four `none`.

A narrow separator is the first byte of the stored string. A
`frac_digits` is the unsigned value of the stored `char`, so
`CHAR_MAX` reads as 127.

#### money_get

`_C_get` (`include/loc/_money_get.cc`) serves both virtuals and uses
the international facet through a `reinterpret_cast` to the domestic
type. It walks `neg_format ()`:

- `none` and `space` read white space where later fields can use it;
  white space beyond what `space` requires is a credit to a symbol or
  sign remainder that begins with white space (6c3f871a);
- `symbol` is read under `showbase`, in the first two fields, third
  when the last field is not `none`, or when a multicharacter sign is
  pending;
- `sign` takes the first character of either sign;
- `value` reads digits, one decimal point when `frac_digits () > 0`,
  and separators only when `grouping ()` starts with a positive element
  other than `CHAR_MAX`.

The buffers grow as `num_get`'s do (ffcde29f), with `bad_alloc` on a
wrapping size (edca50a1). The rest of a sign is matched with
`__rw_match_name`. `units` comes from `__rw_get_num`, which checks
the grouping; `digits` is widened from the buffer as it was read,
leading zeros included (10.3.9).

#### money_put

`do_put (..., long double)` (`include/loc/_money_put.cc`) formats
through `__rw_put_num` under `fixed` and the internal `_RWSTD_IOS_SCALED`
flag at precision `-frac_digits ()` (10.3.2), into a 304-byte stack
array or the heap block `__rw_put_num` moves to, and widens that into
a string of its length. `do_put (..., string_type)` narrows the
leading digits into a 304-byte array, or a heap block when they do
not fit, and computes the group sizes
with `__rw_put_groups`, one byte per group: a group of 256 digits or
more wraps its count, a limit on record (`ref-conformance.md` ch. 2).
Both end in `_C_put`: the pattern and sign by
the first character, the symbol only under `showbase`, the first
character of the sign at `sign` and the rest at the end, one
`widen (' ')` at `space`, the fill at the first `none` or `space`
under `internal`. A pattern with `none` first, or `space` first or
last, throws `runtime_error` through `_RWSTD_REQUIRES`.

#### The data and its writers

`include/loc/_localedef.h`: `__rw_punct_t` holds the offsets of the
narrow and wide `decimal_point` and `thousands_sep`, the `grouping`
bytes and an extension: `__rw_num_t` (narrow and wide `truename`,
`falsename`) for `LC_NUMERIC`, `__rw_mon_t` for `LC_MONETARY` (the
symbols `[intl][wide]`, one `positive_sign` and one `negative_sign`
per width for both formats, the patterns and the nine small integers
per format).

`util/localedef/wnumeric.cpp` writes the narrow strings through
`narrow` (`util/localedef/pool.cpp`: the charmap, then
transliteration), an unset grouping as `CHAR_MAX`, and both bool
names empty: no glibc source sets them, the old compiler wrote them
empty, and the 2005 sources set none. `util/localedef/wmonetary.cpp`
stores an unset or -1 small integer as `CHAR_MAX`, gives an unset
international one its domestic value (cbf75267), builds the patterns
in `format_of`, the old compiler's `create_format`, and makes a sign
`"()"` for both formats and widths when either format's `sign_posn`
is 0 (plan-localedef-backend.md 3.7).

The twelve `src/ti_*.cpp` of the family instantiate the `char` and
`wchar_t` facets and their factories; the get and put factories,
built with `_RWSTD_NO_BYNAME_FACET`, hold one static facet each.

### 10.3 Departures and extensions

#### 10.3.1 `long long` virtuals

- **Paragraph.** 22.2.2.1 and 22.2.2.2 list no `long long` virtual.
- **What.** `num_get` and `num_put` add `do_get` for `long long&` and
  `unsigned long long&`, and `do_put` for both types, where the
  compiler has the type.
- **Why.** Decided 2026-10-14: the library follows the compiler's type
  set; the `LONG_LONG` characterization decides
  (analysis-post-strict-ansi.md 4.1). Two vtables are wider.
- **Pins.** `22.locale.num.get` (`test_llong`, `test_ullong`);
  `22.locale.num.put` (`llong_test`; `ullong_test` exercises nothing,
  10.4). Row: "Compiler types beyond 2003".

#### 10.3.2 Fixed with a negative precision

- **Paragraph.** 22.2.2.2.2 p11 puts the precision in the conversion
  specification under `fixed`; C99 7.19.6.1 p5 takes a negative one
  given by asterisk as if it were omitted, and `__rw_get_stdio_fmat`
  omits it the same way, so `fixed` with a negative precision prints
  six digits after the point, and an integer is not affected at all.
- **What.** The 2005 import (e6c7726b) placed a decimal point
  `-precision ()` digits from the right instead, for integers and for
  floating values rounded to an integer, documented in
  `doc/stdlibref/num-put.html`; that left on 2026-11-04. The
  mechanism stays in `__rw_put_num` for `money_put`, which formats
  its `units` through it under the internal `_RWSTD_IOS_SCALED` flag
  the streams never set (10.3.10).
- **Pins.** `22.locale.num.put` from line 924 (integers) and the π
  rows under `fixed` at -2, -8 and -1000.

#### 10.3.3 Floating values are grouped

- **Paragraph.** 22.2.2.2.2 p16: "For integral types,
  punct.thousands_sep() characters are inserted".
- **What.** `__rw_put_groups` groups the integral part of floating
  output too: `1,234,567.500000` under `"\3"` (by probe).
- **Why.** The 2005 import. C++11's 22.4.2.2.2 stage 2 reads "For
  arithmetic types".
- **Pins.** `22.locale.num.put` `dbl_test`, line 1595:
  `12/34/56*78900` under `"\002"`.

#### 10.3.4 A zero precision is specified

- **Paragraph.** 22.2.2.2.2 p11 (quoted in 10.1).
- **What.** `__rw_get_stdio_fmat` writes `.0` for a zero precision
  without `fixed`: 1234.5 at precision 0 is `1e+03`, not `%g`'s
  `1234.5` (by probe).
- **Why.** The comment cites LWG 231; C++11 specifies the precision
  unless `floatfield` is `fixed | scientific`.
- **Pins.** `22.locale.num.put` `dbl_test`, line 1534 on, against
  `"%.0g"`.

#### 10.3.5 The `0x` prefix in stage 2

- **Paragraph.** 22.2.2.1.2 p8's atoms hold no `x` or `X`.
- **What.** `_C_get` accepts `0x` and `0X` before hex digits under
  `hex`, an empty `basefield`, and for `void*`.
- **Why.** The 2005 import; `scanf`'s `%X`, `%i` and `%p` take the
  prefix, and C++11's atoms add `x` and `X`.
- **Pins.** `22.locale.num.get` `test_long`, lines 843 to 859, and the
  `bool` rows at line 612.

#### 10.3.6 Overflow stores a clamped value

- **Paragraph.** 22.2.2.1.2 p1: on error "val is unchanged"; p11
  leaves an out-of-range field to the rules of `scanf`.
- **What.** An integer out of range stores the nearest limit and
  `failbit`. A negative input to an unsigned type wraps without
  `failbit` where the wrapped value fits, as C++11's `strtoull` rule
  does; `"-1"` is `ULONG_MAX` for `unsigned long`, 65535 with
  `failbit` for `unsigned short`, and `UINT_MAX` for `unsigned int`,
  with `failbit` where `long` is wider than `int`. Floating overflow
  stores ±infinity and `failbit`; underflow stores ±`FLT_MIN`,
  `DBL_MIN` or `LDBL_MIN` and `goodbit`, within C99's `strtod` rule
  that the test quotes.
- **Why.** LWG 23 (comments in `include/loc/_num_get.h`); C++11
  22.4.2.1.2 p3 stores the most positive or negative value. Underflow
  is the 2005 import.
- **Pins.** `22.locale.num.get` `test_uint`, `test_long`,
  `test_ulong`, `test_llong`, `test_ullong`, the three `_uflow`
  tests. Row: ch. 3, LWG 23.

#### 10.3.7 A grouping mismatch stores the value

- **Paragraph.** 22.2.2.1.2 p1 and p12; 22.2.6.1.2 p1, "does not
  change units or digits".
- **What.** `__rw_get_num` stores the value and returns `failbit` when
  the separators disagree with `grouping ()`; `money_get` does the
  same for `units`, and widens `digits` before the check. `12,34`
  under `"\3"` reads 1234 with `failbit` (by probe).
- **Why.** The 2005 import (the comment before the check). C++11
  22.4.2.1.2 p3-4 stores first and checks grouping after.
- **Pins.** `22.locale.num.get`; `22.locale.money.get` compares
  `digits` with its initial value only when grouping is empty.

#### 10.3.8 `money_get` sets `eofbit` on success

- **Paragraph.** 22.2.6.1.2 p1: "If a valid sequence is recognized,
  does not change err".
- **What.** `_C_get` adds `eofbit` whenever the input ended, valid or
  not: `1,234.56` under en_US reads 123456 with `eofbit` (by probe).
- **Why.** The 2005 import (`git blame` of the `__it == __end` test).
- **Pins.** `22.locale.money.get`, the rows from line 598 on, and
  `22.locale.money.get.stdcxx-62`.

#### 10.3.9 Fraction digits are exact

- **Paragraph.** 22.2.6.3 p3, quoted in 10.1; 22.2.6.1.2 p4.
- **What.** A decimal point, if present, is followed by exactly
  `frac_digits` digits, else `failbit`; without one the value is the
  bare digit string, so `100` under two fraction digits is 100 units
  and `007` is 7. `digits` holds every digit as it appeared, leading
  zeros included: `0.07` gives `"007"`. The 2005 import padded a short
  fraction with zeros and dropped the digits past `frac_digits`, so
  `100` read 10000 and `1.2` read 120, and stripped the leading zeros
  of `digits` down to one.
- **Why.** The grammar in p3 and the "in the order in which they
  appear" of p4; the padding had no paragraph behind it and broke the
  round trip with `money_put`, which writes 100 units as `1.00`.
- **Pins.** `22.locale.money.get`, the zero and non-zero rows under
  non-zero `frac_digits`, the short and long fraction rows with
  `failbit`, `digits_test` for the text.

#### 10.3.10 `money_put` reads `units` as `"%.0Lf"`

- **Paragraph.** 22.2.6.2.2 p1's `"%.01f"`.
- **What.** `units` is rounded to an integer by `snprintf`'s `%.0Lf`
  rule: 0.5 is `$0.00`, 1.5 and 2.5 are `$0.02` (by probe).
- **Why.** The 2005 import; C++11 22.4.6.2.2 p1 reads `"%.0Lf"`.
- **Pins.** `22.locale.money.put` `ldbl_test`.

#### 10.3.11 Readings within the letter

- The international symbol and a `space` both print: `-USD  1,234.56`
  under en_US (8cf34a6f, 22.2.6.3 p2, footnote 242). `money_get`
  reads that text and fails on one space, `-USD 1.00` (by probe).
- A `sign_posn` of 0 is the sign `"()"`: `(HK$1,234.56)` under en_HK
  in both widths (plan-localedef-backend.md 3.7).
- The "C" `moneypunct` holds `.`, `,`, empty strings and
  `frac_digits` 0 beside p7's pattern.

#### 10.3.12 Primary templates of the punctuation facets

- **Paragraph.** 22.1.1.1.1 Table 51 requires `char` and `wchar_t`
  only.
- **What.** `numpunct<C>` and `moneypunct<C, I>` compile for any `C`,
  but every virtual reaches the `__rw_get_punct` stub, which asserts
  in a debug build and returns a null pointer that the virtual
  dereferences: `numpunct<unsigned short>::decimal_point ()` aborts in
  15D (by probe).
- **Why.** The definitions stay as the only code since the gates left,
  a decision of 2026-10-14 (831519bf); `numpunct`'s gate had been
  commented out since the 2005 import. The stub has a body only to
  work around a VAC++ 7.0 bug (its comment).
- **Pins.** Nothing. Row: "Primary template definitions", which this
  qualifies; `moneypunct` is not in the row.

### 10.4 What the tests pin

| program | covers | 15D |
|---|---|---|
| `22.locale.num.get` | every virtual, both widths, three iterator types: bases, signs, grouping, overflow, underflow, long input, `bad_alloc` on wrap | 51990, 0 failed |
| `22.locale.num.put` | every virtual but `unsigned long long`, inf and NaN, grouping, widening, a negative precision | 1465, 2 warnings |
| `22.locale.numpunct` | "C" values, a facet used alone, every host locale against `localeconv` | 5366 |
| `22.locale.numpunct.mt` | first use of a fresh facet from many threads (7c93badb) | 1200 |
| `22.locale.money.get` | patterns, signs, symbols, white-space credit, long input and separators, `frac_digits` 305 and 4000, the digits as they appear | 4160 |
| `22.locale.money.put` | `long double` and string, patterns, the two spaces, grouping | 1346 |
| `22.locale.moneypunct` | every host locale against `localeconv`, all four facets, composite names | 135020 |
| `22.locale.moneypunct.mt` | first use of a fresh facet (79709615) | 1200 |
| `22.locale.num.put.stdcxx-2` | `showpoint` with a zero value | NOUT |
| `22.locale.money.get.stdcxx-62` | empty input sets `eofbit | failbit` | NOUT |

The two warnings of `22.locale.num.put` are `ullong_test`, a
placeholder since 2006. `22.locale.numpunct` never checks the names
of a named locale ("FIXME", line 223). Its narrow
separator is compared with the first byte of `localeconv`'s string,
which pins the first-byte reading. No row checks `unsigned short`
overflow (`22.locale.num.get` has `test_uint`): `test_ushrt` and
`test_uint` of `27.istream.fmat.arith` gate their rows on `_RWSTD_USHRT_SIZE` and
`_RWSTD_UINT_SIZE`, which nothing defines. The four `get` and `put` MT
programs were retired on 2026-10-19: the facets hold no state
(5c1368e6, a701ec25, 72d94f0b, 70070c4b). No row of the family fails
in any pin.

`TODO` entries: the money pattern of an unavailable `sign_posn`; the
unbounded monetary small integers of `number_value`; `src/punct.cpp`
under EDG's export model (WAITING); glibc's `signbit` (WAITING), under
which `num_put`'s classification of floating values is stubbed.

### 10.5 Open

- **Names of true and false.** Every database locale has empty
  names, so `boolalpha` writes nothing and reads nothing under them;
  the C-library path gives `"true"` and `"false"`. Decision: what a
  named locale's names are. Check: `boolalpha` round trip under
  en_US.UTF-8 from the database.
- **Multibyte separators.** fr_FR.UTF-8's narrow `thousands_sep` is
  0xE2, the lead byte of U+202F, and `num_put<char>` writes
  `1\xE2234\xE2567` (by probe); fr_CA's money separator is 0xC2.
  Decision: what a narrow facet returns for a separator wider than a
  byte. Check: the narrow output of every UTF-8 locale is valid UTF-8.
- **C.UTF-8's money data.** Beside the `TODO` entry's four `CHAR_MAX`
  parts, under which `money_put` writes nothing, its `frac_digits` is
  127, and on the C-library path its four `none` make `money_put`
  throw `runtime_error` (`RWSTD_LOCALE_ROOT=/nonexistent`, by probe).
  Check: the entry's, with `frac_digits` and both paths.
- **`digits` with leading zeros.** `"0.07"` yields `"07"` and `"007"`
  yields `"0700"` (by probe): neither every digit as read (p4) nor no
  leading zero, which the comment intends. Check: the string
  overload's result compared by text, not by `sscanf`, in
  `22.locale.money.get`.

## 11. Collation and message retrieval

### 11.1 The standard's contract

#### `collate`, 22.2.4

- 22.2.4.1, p1: `collate<char>` and `collate<wchar_t>` apply
  lexicographic ordering (25.3.8). p2: each function reads
  `[low, high)`.
- 22.2.4.1.1, p1-3: `compare`, `transform` and `hash` return the
  virtuals' results.
- `do_compare`, 22.2.4.1.2, p1: 1, -1 or 0; "The instantiations
  required in the Table 51 (22.1.1.1.1), namely collate<char> and
  collate<wchar_t>, implement a lexicographical comparison (25.3.8)."
- `do_transform`, p2: a string whose lexicographic comparison with
  another transform gives `do_compare`'s result.
- `do_hash`, p3: equal for strings `do_compare` calls equal; the note
  asks for a small collision probability.
- `collate_byname`, 22.2.4.2: the synopsis declares the three
  overrides. Its order is the named locale's (22.1.1.1.2, p4), which
  the standard leaves to the implementation, as it does the treatment
  of a sequence the codeset does not encode.

#### `messages`, 22.2.7

- 22.2.7.1, p1: a `catalog` comes only from `open`.
- 22.2.7.1.1, p1-3: `open`, `get` and `close` call the virtuals.
- `do_open`, 22.2.7.1.2, p1-2: a value below 0 when no catalog opens;
  the name maps to a catalog in an implementation-defined way; `loc`
  serves code conversion.
- `do_get`, p3-4: requires an open catalog; the message of `set` and
  `msgid` by an implementation-defined mapping, `dfault` when none.
- `do_close`, p5-7: requires an open catalog; releases its resources;
  their limit is implementation-defined.
- `messages_byname`, 22.2.7.2: the synopsis declares the three
  overrides.

### 11.2 The implementation

#### `collate<char>` and `collate<wchar_t>`

`include/loc/_collate.h` declares the primary template's three
virtuals and defines none ("protected virtual members are not
defined"); `include/loc/_collate.cc` defines its `id` and destructor.
The `char` and `wchar_t` specializations are exported classes defined
in `src/collate.cpp`, with their ids. `src/ti_collate.cpp` and
`src/ti_wcollate.cpp` define the factories: the ordinary facet in
static storage, the byname facet on the heap (chapter 2).

- `do_compare`: `memcmp` (narrow) or `wmemcmp` (wide) over the shorter
  length, then the lengths; -1, 0 or 1. The narrow facet compares as
  `unsigned char` (11.3).
- `do_transform`: a copy of the range.
- `do_hash`: `__rw_hash`, a Weinberger hash, over the result of the
  virtual `do_transform`, so a derived facet's transform is what
  hashes and p3 holds for it.

#### `collate_byname<char>` and `collate_byname<wchar_t>`

Both override `do_compare` and `do_transform` and inherit `do_hash`,
which therefore hashes the byname transform. `do_compare` transforms
both strings and compares the transforms; the wide facet does so since
8abda9b3, which removed its `wcscoll` path. `do_transform` takes the
facet's `LC_COLLATE` image from `_C_data` (chapter 5) and has three
paths:

- No image: under `__rw_setlocale (_C_name, LC_COLLATE)`,
  `__rw_strnxfrm` or `__rw_wcsnxfrm`, `strxfrm` or `wcsxfrm` segment by
  segment, embedded NULs kept as NULs. The C locale is switched for the
  call (chapter 5).
- `codepoint_collation` set: the base facet's transform, the string
  itself (f56f7adb).
- The table. At each position the longest collating element
  (`__rw_get_n_ce_offset`, `__rw_get_w_ce_offset`, only when
  `largest_ce > 1`), else the character (`__rw_get_char_offset`,
  `__rw_get_wchar_offset`), else, when `undefined_optimization` is set,
  the shared row `undefined_weight_idx` after a validity check against
  the codeset's codecvt database, mapped once per call and released
  with it (`__rw_cvt_mapping`). The narrow walk moves its pointer
  into the sequence as it descends the trie, so the check decodes
  the character from its first byte, bounded by the range, and steps
  past it with the decoder's own advance (`__rw_mbtowco`); the wide
  walk leaves its pointer on the character. The wide keys are the
  UTF-8 form of the `wchar_t` (`__rw_itoutf8`). A sequence the check
  rejects makes the whole transform empty; so does a wide character
  outside the charmap (5bbe3e4b). The rows go to
  `__rw_process_offsets`.

`__rw_process_offsets` walks once per level (`num_weights`), grouping
consecutive rows by the level's two direction bits in each row's
first word: forward, backward, and the two `position` forms. A
`position` level writes `CHAR_MAX` or `WCHAR_MAX` for an element
ignored there. Each level ends with the value 1. A wide weight is one
`wchar_t`. A narrow weight is spelled with run bytes of `CHAR_MAX - 1`,
each worth `CHAR_MAX - 2`, and a final byte from 1 to `CHAR_MAX - 2`:
prefix-free and below the mark (92546b5c,
`analysis-collate-transform.md`). A zero or `UINT_MAX` weight writes
nothing. Byte 1 both ends a level and spells weight 1, an overlap left
as is.

The element tries (56c7006e) and their leaf table:

- A trie page is indexed by byte value and written from its first
  used byte to its last. An entry holds one word: the leaf of a key
  that ends at that byte, or the next page's number with the high bit
  set, never both.
- The per-page tables are indexed by page number: the offset table,
  the first-byte and last-byte tables, and the leaf table.
- A key that is a proper prefix of another, `dz` before `dzs` in
  `hu_HU`, ends on a page whose entry had to be the pointer. That
  page's word in the leaf table is the shorter key's leaf; it is
  `UINT_MAX` on every page whose path is no element, and always on the
  root. One path leads to each page, so no two keys collide.
- The lookup matches the longest prefix. Entering a page with a leaf,
  it remembers the leaf and the cursor; on a miss it returns the
  remembered leaf with the cursor rewound; an entry leaf ends the walk.
- The character tries have no leaf table: a character ends a walk. A
  second word per entry was rejected: it doubles every element page
  for 26 keys in the corpus.

The image is `__rw_collate_t` (`include/loc/_localedef.h:204`), 364
bytes by `sizeof`: 26 words of offsets and counts, `longest_weight`,
`num_weights`, `largest_ce` and `weight_type[256]`. A row is
`elm_size` bytes, `(levels * longest_weight + 1)` words, the first
holding the direction bits; the runtime reads the directions there and
never reads `weight_type`. Under `codepoint_collation` the image is the
header and the two names, 376 bytes for `C.UTF-8`; `de_DE.UTF-8`'s is
13514560 bytes in 15D. The writers are `util/localedef/lcollate.cpp`
(the reader, glibc's `ld-collate.c` semantics), `collnum.cpp` (the
numbering, two passes), `trie.cpp` (the shared trie builder) and
`wcollate.cpp` (the image). A definition that leaves characters out
gets one shared row after every defined element, POSIX XBD 7.3.2.4,
decided in `plan-localedef-backend.md` 2.3.

The facets cache nothing; the image is read-only mapped memory.

#### `messages<char>` and `messages<wchar_t>`

`include/loc/_messages.h` derives `messages` from `__rw_facet` and
`messages_base` (`catalog` is `int`); the ids are specialized in
`src/ti_messages.cpp` and `src/ti_wmessages.cpp`. `messages_byname`
sets its name and overrides nothing; the name has no effect on the
catalogs.

- `do_open`: `__rw_cat_open` (`src/messages.cpp`), under
  `_RWSTD_MT_CLASS_GUARD (__rw_open_cat_data)`, calls `catopen (name,
  NL_CAT_LOCALE)`: the mapping is the C library's, `NLSPATH` and the
  `LC_MESSAGES` of the C library's current locale. Failure returns -1.
  Success stores the `nl_catd` and a copy of `loc` in the first free
  slot of a paged repository, `__rw_manage_cat_data`: eight static
  slots, then pages of eight (e8a04718). The slot index is the
  catalog.
- `do_get` (`include/loc/_messages.cc`): `__rw_get_message` calls
  `catgets (catd, set, msgid, "")` under the guard; no message, or an
  invalid catalog, returns `dfault`. The text converts through the
  `codecvt<charT, char, mbstate_t>` of the catalog's locale
  (`__rw_get_locale`), p2: `char` under `always_noconv` returns the
  bytes; otherwise `in` fills a buffer of one `charT` per byte; `ok`
  returns it, `noconv` for `wchar_t` widens byte by byte, `partial` and
  `error` return `dfault`.
- `do_close`: `__rw_cat_close` calls `catclose`, destroys the locale
  copy and frees the slot; an invalid or closed catalog throws
  `invalid_argument` once the guard is released (7443c91e).

`src/messages.cpp` includes `<nl_types.h>` unconditionally; the
library has no `catopen` of its own. The `LC_MESSAGES` image,
`__rw_messages_t` (`include/loc/_localedef.h:525`, 28 bytes), holds
`yesexpr` and `noexpr`, wide and narrow, written by
`util/localedef/wmessages.cpp`; no facet reads it, and
`util/locale.cpp` prints it.

The repository's class guard serializes `open`, `get` and `close`.
`__rw_get_locale` returns a reference read after the guard is
released, valid while the catalog is open, which p3 requires.

### 11.3 Departures and extensions

#### `collate<char>` orders bytes as `unsigned char`

22.2.4.1.2, p1: "namely collate<char> and collate<wchar_t>,
implement a lexicographical comparison (25.3.8)". 25.3.8 compares
with `operator<`, which on a signed `char` puts `'\x80'` before
`'a'`. The library compares with `memcmp`: `'\x80'` after `'a'`, by
probe. Why: the 2005 import (e6c7726b); the order agrees with
`char_traits<char>::lt` and `compare`, which follow LWG 467
(conformance record, chapter 3), so `collate<char>::compare`,
`basic_string<char>::compare` and the order of `transform`'s results
agree. Pins: nothing. The wide facet's `wmemcmp` compares as
`wchar_t` and agrees with `operator<`.

#### The primary template declares its virtuals and defines none

22.1.1.1.1, p4 requires `collate<char>` and `collate<wchar_t>` only,
so this is no departure. `collate<unsigned char>` compiles and fails
to link on its three virtuals, by probe. Why: e6c7726b ("protected
virtual members are not defined"); 831519bf removed the gate and kept
the declarations. The conformance record's row "Primary template
definitions" lists `collate` among the primary templates with
definitions; for `collate` it holds for the class, not its virtuals.

#### An invalid catalog is diagnosed

22.2.7.1.2, p5: "Requires: A catalog cat obtained from open() and not
yet closed." `close` of such a value throws `invalid_argument`; `get`
of one returns `dfault`. Why: e6c7726b. Pins: `22.locale.messages`,
`test_open_close`, which closes a catalog twice and expects the
exception. An extension.

### 11.4 What the tests pin

| program | covers | 15D |
|---|---|---|
| `22.locale.collate` | the test locale's table order, five pairs, narrow and wide, and their agreement; a `codepoint_collation` locale; the byname facets over sorted fixtures of six definitions built with `-w --no_position`; `transform` and `compare` against `strxfrm` and `strcoll` in the C library's single-byte locales; embedded NULs; `hash`; a wide character outside the charmap | 145873 |
| `22.locale.collate.stdcxx-1010` (regress) | sorting with a French locale, falling back on `"C"` | NOUT |
| `22.locale.messages` | `has_facet`, `open` and `close` with a double close, descriptor leaks, `get` through the catalog locale's `codecvt`, `NLSPATH`, a stress loop | 366 |
| `22.locale.messages.mt` | racing `open` and `close` | 600 |
| `22.locale.messages.stdcxx-542` (regress) | four catalogs opened and closed in reverse order | 0 |

No row fails on the 15D pin. The fixtures build with `--no_position`,
so the `position` levels of `TODO`'s ignored-element entry are not in
the suite. `check_libc` skips multibyte C library locales (a `FIXME`
in `22.locale.collate.cpp`). `TODO` "Calls EDG matches to no function"
names `22.locale.collate.cpp:1159`; "Tests that GCC and Clang let
through" names `22.locale.messages.cpp:616` (`getcwd`) and
`22.locale.collate.stdcxx-1010.cpp:71` (a UTF-8 literal).

### 11.5 Open

- `TODO` "Characters outside a collation table sort last, glibc's
  first" (Locales): the narrow lookup steps into a multibyte character
  before it misses, the validity check starts on a continuation byte,
  and the transform is empty. Check: the collation oracle over those
  ranges finds no disagreement over a character outside the table.
- `TODO` "A character ignored at a level sorts after a weight there"
  (Locales): the `position` mark puts a string with a trailing ignored
  element after the same string without it, against XBD 7.3.2.3;
  the fix is a count spelling in `__rw_process_offsets`. Check: the
  oracle over `en_US`, `sv_SE`, `fa_IR` and `ml_IN`.
- `TODO` "The collate image's element tries and their leaf table"
  (Documentation): 11.2 states its facts; the entry can leave with
  this chapter.
- Every `do_transform` that meets a character outside the table maps
  the codecvt database again and never unmaps it (11.2's validity
  check). Check: 100 wide transforms of U+743B under `zh_CN.UTF-8` add
  no mapping of the file; today they add 100, by probe, and so do 100
  narrow ones.
- The narrow spelling is linear in the weight: `"ab"` under
  `de_DE.UTF-8` transforms to 1132 bytes narrow and 12 `wchar_t` wide,
  by probe; glibc's weights run to five digits. Whether the length
  matters is a decision; no entry exists.

## 12. The time category: `time_get` and `time_put`

`time_get` parses a date or a time into a `struct tm`; `time_put`
formats one. Both read `LC_TIME` through `__rw_get_timepunct`.
`time_put` writes what `strftime` writes, and `time_get` reads what
`time_put` wrote. The members that parse an arbitrary directive are
C++11's, the one recorded exception to the 2003 baseline.

### 12.1 The standard's contract

14882:2003 unless N3242 is named. The facets use their `ios_base&`,
`iostate&` and fill as 22.2 describes, and `ctype<>` for formatting
details (22.2.5, p1). 22.2, p1: "Formatting facets use the character
argument fill to fill out the specified width where necessary." 22.2,
p2: the `get()` members take an `iostate&` "whose value they ignore,
but set to ios_base::failbit in case of a parse error." `time_get`
and `num_get` do: every member starts from `goodbit` and returns the
parse's own state (C++11 22.4.5.1.1, p8 and 22.4.5.1.2, p12 say so
outright for the format members). `money_get` ORs, by its own
22.2.6.1.2, p1: "sets err to (err|str.failbit)", unchanged in every
edition since 2003 against the clause-wide sentence.

#### `time_get`, `time_get_byname`

- 22.2.5.1, p1: "Each get member parses a format as produced by a
  corresponding format specifier to time_put<>::put." A mismatch
  reports an error or assigns unspecified values. Each public member
  returns its virtual (22.2.5.1.1, p1 to p5).
- `do_date_order` (22.2.5.1.2, p1): the order of day, month and year in
  the `'x'` format; `no_order` if it "contains other variable
  components". Footnote 233 allows `no_order` in a valid locale.
- `do_get_time` and `do_get_date` (p2 to p5) read what `put` writes for
  `'X'` and `'x'`. `do_get_weekday`, `do_get_monthname` (p6, p7) read a
  full or abbreviated name. p6: "If it finds an abbreviation that is
  followed by characters that could match a full name, it continues
  reading until it matches the full name or fails."
- `do_get_year` (p8, p9) reads an unambiguous year. p8: "It is
  implementation-defined whether two-digit year numbers are accepted,
  and (if so) what century they are assumed to lie in."
- `time_get_byname` (22.2.5.2) takes a name and redeclares the six
  virtuals; it states no behaviour of its own.

#### The C++11 members of `time_get`

- `get (s, end, f, err, t, format, modifier)` returns `do_get` with
  those arguments (N3242 22.4.5.1.1, p6).
- `get` over `[fmt, fmtend)` (p7 to p10) starts at `err = goodbit` and
  loops until the pattern ends, `err != goodbit`, or `s == end`, which
  sets `eofbit | failbit`. A specification valid for `strptime`
  evaluates `do_get` on `t`; a pattern too short to tell sets
  `failbit`. White space skips white space. Another character must
  match "in a case-insensitive comparison" (p8), by means p9 leaves
  unspecified.
- `do_get` (22.4.5.1.2, p11 to p15) needs a dereferenceable `t`. p12:
  it assigns the members "corresponding to a conversion directive
  appropriate for the ISO/IEC 9945 function strptime"; an invalid
  directive leaves `*t` unchanged with `failbit`; reaching `end` sets
  `eofbit`. p13: an `E`, `O`, `%c`, `%x` or `%X` directive that cannot
  determine its members sets `eofbit`, the members unspecified. p14
  leaves unspecified whether calls on one `tm` update or overwrite.

#### `time_put`, `time_put_byname`

- `put` over a pattern (22.2.5.3.1, p1) narrows each character by
  `ct.narrow (c, 0)`, writes text as it goes, and calls `do_put (s,
  str, fill, t, spec, mod)` for each `'%'`, optional modifier and
  specifier "as defined for the function strftime". The other `put`
  calls `do_put` (p2). p2a: the fill "may be used in the
  implementation-defined formats, or by derivations."
- `do_put` (22.2.5.3.2, p1) formats as `strftime` does, the output of
  the C-locale-dependent specifiers being implementation-defined.
  Footnote 235: the modifier "should follow POSIX conventions".
  `time_put_byname` (22.2.5.4) redeclares `do_put`, nothing more.

### 12.2 The implementation

#### Files

- `include/loc/_time_get.h`: the classes. The 2003 virtuals are inline
  `do_get` calls of `'X'`, `'x'`, `'a'`, `'b'` and `'Y'`.
- `include/loc/_time_get.cc`: `do_date_order`, `get` over a range,
  `do_get`, and the era readers `__rw_read_era`, `__rw_read_era_year`.
- `include/loc/_time_put.h`, `_time_put.cc`: `put` and `do_put`.
- `src/time_put.cpp`: the data of both facets: the three
  `__rw_get_timepunct` overloads, the writer's table
  `__rw_get_time_put_data`, `__rw_put_time` and `__rw_fmt_time`.
- `src/era.cpp`, `era.h`: the writer's `__rw_get_era`,
  `__rw_get_era_year`; the reader's `__rw_set_era`,
  `__rw_set_era_year`.
- `src/ti_time_get.cpp` and its three siblings: instantiations, ids and
  static factories (chapter 2). `src/time_get.cpp` holds an empty
  `namespace __rw`, as in the 2005 import. The byname facets add a name
  buffer and override nothing.

#### The data

`__rw_time_t` (`include/loc/_localedef.h`) is 101 32-bit words: the
codeset and charmap names, the counts and offsets of the eras and the
alternative digits, and narrow and wide offsets of the 7 `abday` and
`day`, 12 `abmon` and `mon`, 2 `am_pm`, and the seven formats `d_t_fmt`
to `era_t_fmt`. An `era_t` is 32 bytes: narrow and wide name and year
format, an offset whose sign is the direction, and two bounds. A
bound's year is astronomical; `INT_MIN` or `INT_MAX`, with `CHAR_MIN`
or `CHAR_MAX` month and day, is an end of time.
`util/localedef/wtime.cpp` (`write_time`) writes it, wide strings
before narrow, a source's era year -543 as -542. The source reader caps
`alt_digits` and the era records at 100 each, the facet's arrays, and
rejects an era without a format (`util/localedef/scalar.cpp`).

The record is the mapped database where the locale has one (chapter 5).
Otherwise `__rw_get_timepunct (facet, flags, inx)` builds it from the C
library under `__rw_setlocale`: `nl_langinfo`, `mbstowcs` for the wide
copies, `"%a %b %e %H:%M:%S %Y"` for `%c` in "C". That record has no
eras and no alternative digits, so `E` and `O` read and write as the
plain directives. It is published by release stores (9a482a4f; chapter
7). Neither facet caches anything else.

#### `time_get`: the walk

`get` over a range works on a copy of `*t`, or of a zeroed `tm` for a
null `t`. A pattern character is narrowed only to find `'%'`, the
modifier and the conversion; digits and `'.'` between them are skipped,
so `"%4Y"` is `%Y` (by probe). A conversion calls the virtual `do_get`
on the copy. White space skips white space. A literal matches by
`char_traits::eq` through the stream's `ctype::toupper` (90a98455,
50087549), or sets `failbit`; the input ending where a literal needs a
character sets `eofbit | failbit`. A call that leaves any bit set ends
the walk, which adds nothing (p8 tests `err` before `s == end`), so a
directive that reaches the end of the input before the pattern ends
leaves `eofbit` alone. The copy is stored only when no bit but
`eofbit` is set. The walk keeps no state (e438cc11).

#### `time_get`: `do_get`

`do_get` copies `*t` and asks `__rw_get_timepunct (facet, data, tm,
pmem, names, sizes)`: `data` goes in as conversion, modifier and width
of `char_type`, and comes out as adjustment, factor, bounds and digit
count; `pmem` names the member.

- One string is a pattern: the locale's `%c`, `%x`, `%X`; `%r` from
  `t_fmt_ampm`, or `"%I:%M:%S %p"` where that is empty (9d3e7b3d);
  `%D`, `%F`, `%R`, `%T`; `%Op` as `"%p"`. It goes to `get` over that range
  with the caller's `t`, so each nested directive is a virtual call
  again.
- Several strings are names. `__rw_match_name`
  (`include/loc/_punct.cc`) matches them in parallel through
  `ctype::toupper` and keeps the longest the input completes; `%a`,
  `%A`, `%b`, `%B`, `%h` take full or abbreviated. Under `O` the names
  are the alternative digits.
- Otherwise a number. White space is skipped; `%C`, `%y`, `%Y` take a
  `'+'` or `'-'` (503ae88c). At most 2 characters, 3 for `%j`, 4 for
  `%Y`, the sign included; a digit that would pass the bound is left
  unread, so `%d` on `"45"` reads 4 (e39b96fb, b910448a). Out of bounds
  is `failbit`.
- `%n`, `%t` skip white space. `%Z` consumes non-space characters. `%z`
  reads `Z`, `±hh`, `±hhmm`, `±hh:mm` into `tm_gmtoff` where `std::tm`
  has it (892e6c86).
- The rest is invalid, `failbit` with `*t` unchanged: `E` before
  anything but `c C x X y Y`, a modifier on `%Z` or `%z`, a conversion
  the table lacks.

Like the walk, `do_get` stores its copy when no bit but `eofbit` is
set, and updates it first (p14; e438cc11). `%C` sets the century and
keeps the last two digits of the year it finds. `%I`, `%l`, `%OI` store
the hour modulo 12 in the half of the day `tm_hour` holds. `%p` moves
the hour it finds into its half; an hour outside 0 to 23 is `failbit`.
`%y` pivots 69..99 to 19xx and 00..68 to 20xx, but appends to a
negative year it finds, the trace of a parsed century. `%U`, `%W`,
`%OU`, `%OW` check 00 to 53, `%V` and `%OV` 01 to 53, `%G` four digits
and `%g` two, and store nothing; `std::tm` has no week and no
week-based year. `%u` and `%Ou` read 1 to 7, Monday first, and store
7, Sunday, as 0.

#### `time_get`: the era conversions

`%EC`, `%Ey`, `%EY` ask for the era names. With none, the modifier is
dropped (ffea5ffe). With some, `__rw_read_era` works on a copy,
stored only when the directive's whole format matched; a valid `E`
directive that fails sets `eofbit`, never `failbit`, and leaves the
`tm` as it was.

- An era is every record of its name (`__rw_get_era_span`). Its span
  runs from the earliest bound to the latest, its first year is the
  start of the record with the smallest offset. `__rw_era_holds`
  compares whole dates only in the span's first and last years, a day
  below 1 taken as the first.
- `%EC` matches a name and calls `__rw_set_era`. A year inside the era
  stays. Otherwise it keeps its number in its own era, or takes the
  era's smallest offset, and moves into the era where that lands inside
  it; failing that it becomes the first year, or the year next to it
  inside the span.
- `%Ey` reads up to four digits, no sign, after white space, and
  `__rw_set_era_year` places them in the era the `tm` lies in. No
  digits, no era, or a year past the span is `eofbit`.
- `%EY` cuts each record's format at `%EC` and appends the name to the
  text before it, in a 512-element buffer; a format without `%EC`, or
  past the buffer, is left out. The strings are matched in parallel.
  Where records share a name, white space that begins every rest is
  skipped, then one character decides: a digit takes the rest that
  begins with `%Ey`, another character the rest that begins with it.
  The rest is walked in that record: `%Ey` reads in the record's era, a
  literal matches exactly, another directive is `eofbit`. A format
  without `%Ey` sets the record's start year.

`%Ec`, `%Ex`, `%EX` read the era format, or the plain one where it is
empty or missing.

#### `time_get`: `do_date_order`

It walks the facet's `%x` format (6aecb262, 08295a74), past the flags
`- _ 0 ^ #`, a width and a modifier (a width or precision in a format
is bounded at 64, `ref-conformance.md` ch. 2). `%d %e` are the day, `%b %B %h %m`
the month, `%C %y %Y` the year (`%y` right after `%C` completes it),
`%D` and `%F` their sequences; `%a %A %n %t %%` are skipped. Three
components, each once, give the order; else `no_order`.

#### `time_put`

`put` over a pattern takes `E` before `c C x X y Y` and `O` before `C d
e H I m M p S u U V w W y` (8820446f). Another letter after `E` or `O`
calls `do_put` with a null conversion and is then copied: `"%OY"`,
`"%Ed"` and `"%Q"` write themselves (by probe). `do_put` formats with
`__rw_put_time` into a 256-element array, or a heap block when the
output outgrows it, freed on every exit by `__rw_put_buffer`
(`include/loc/_punct.h`) as the put facets' are. It writes those
characters and nothing else: 22.2.5.3.2, p1 has it format as
`strftime` does, which has no field width, so `str.width ()` pads
nothing and stays as it is. `"%H:%M"` at width 5 is `"09:05"`.

`__rw_get_time_put_data` decides each directive.

- Names from the record; numbers by `snprintf` or `swprintf`; `%g`,
  `%G`, `%V` from `__rw_iso_week`; `%C` and `%y` as in 12.3.
- Patterns (`%c %x %X %r %D %F %R %T`, era formats) through
  `__rw_fmt_time`, which also takes HP-UX `%[-|0][w][.p]` inside a
  locale's format. An empty `t_fmt_ampm` makes `%r` `"%I:%M:%S %p"`
  (decb2382); an empty era format is the plain one.
- `O` writes the alternative digit below the table's length, ordinary
  digits from there. `do_put ('Y', 'O')` writes the plain year
  (268998d5).
- `%EC %Ey %EY` use `__rw_get_era`: the first record, in order, whose
  bounds hold the whole date. `%Ey` is zero-padded to two ordinary
  digits; no era is the plain directive.
- `%z` is `tm_gmtoff` where non-zero, else the `TZ` offset, nothing for
  a negative `tm_isdst`; `%Z` is the name from `TZ`, nothing when it is
  unset.
- Anything else goes to `strftime` or `wcsftime` under the process's C
  locale: `%s` writes the epoch seconds (by probe).

A directive that does not fit grows the buffer, to twice its size or
what the directive needs, the larger: a literal of a format, a name or
an alternative digit by its length, a number by the length `snprintf`
reports. `swprintf` reports only -1, and the buffer grows once to the
widest number of the width and precision. `strftime` and `wcsftime`
format the directive behind a space, so that 0 means only that it does
not fit; the buffer grows once, to twice its size. A format inside a
format writes into the same buffer.

### 12.3 Departures and extensions

"Row" names a row of `ref-conformance.md` chapter 2 by its title.

#### The format members are C++11's

- 22.2.5.1 has five parsing members and no `get` with a format.
- The library has `get (..., char, char)`, `get` over a range and
  `do_get`, signature for signature N3242's 22.4.5.1.1, p6-10 and
  22.4.5.1.2, p11-15. The 2003 members ride on `do_get` and read the
  locale's `%X` and `%x`, as 2003 asks, not N3242's `"%H:%M:%S"` and
  Table 92.
- Why: in the 2005 import; N2070 and N2211 took them into the standard
  from this facet. c40c5d16 brought them back as a ruled exception the
  day b7443c95 hid them. Pins: 22.locale.time.get throughout. Row
  "`time_get` with a format".

#### Each `do_get` updates the `tm`

- N3242 22.4.5.1.2, p14 leaves update or overwrite unspecified; p12
  asks for the `strptime` directive's members.
- The library updates, and the `tm` is the only carrier. `"%C %y"` on
  `"19 26"` is 2026, since `%y` cannot tell a century 19 from a zeroed
  `tm`; `"%Y %C"` on `"2026 19"` is 1926; `"%H %p"` on `"09 PM"` is 21,
  where glibc ignores the `%p` (by probe). No date is derived from a
  week, and stale members of the caller's `tm` feed the update.
- Why: e438cc11; the combining that 59f3a87e and 2829b153 put in `get`
  overruled the virtual it calls. Pins: 22.locale.time.get, "%C with
  %y", "%p with the hour", "%U and %W with %Y and %w: nothing derived".

#### Years before 0

- C90 has no `%C`; C99 leaves `%C` and `%y` outside 00-99 unspecified.
- `time_put` writes POSIX.1-2024's pair: `"-5"` and `"43"` for -543,
  `"-0"` for -1 to -99. `time_get` reads a sign before `%C`, `%y`,
  `%Y`, counted in the width, and appends: `"%C%y"` on `"-543"` and
  `"%y %C"` on `"43 -5"` are -543. `"%C%y"` of -1 to -99 does not read
  back: `"-043"` reads 2043 (by probe). glibc writes `"-6"` and `"57"`.
- The same width sentence covers a year past 9999, so `time_put`
  does not take the century modulo 100: `"100"` for 10000, `"-100"`
  for -10000, and `%OC` the same value, in ordinary digits in "C".
- Why: b54acca9 and 503ae88c, replacing 11087a6a's glibc pair. Pins:
  22.locale.time.put's rows before 0 and past 9999;
  22.locale.time.get, "%C, %y and %Y with a sign";
  `bin/oracle/time.cpp`'s one known difference.

#### Narrow names matched by element

- Against 22.2.5.1.2, p6 (12.1), `time_get<char>` matches a name byte by
  byte. A name that is a prefix of longer names sharing its next byte
  cannot be read: lzh_TW's 廿 before 年. The wide facet reads it.
- Why: an input iterator cannot give back the bytes of a longer name.
  Pins: no test row. Row "Narrow names matched by element".

#### Empty `am_pm` names

- Against 22.2.5.1, p1 (12.1), `%p` matches an empty AM name at once, as
  glibc does. hy_AM, sm_WS and su_ID have empty names; their `%r`, in
  `t_fmt` or `d_t_fmt`, writes 21:05:09 as `"09:05:09 "`, which reads
  09:05:09.
- Why: d85d3f98. Pins: 22.locale.time.get, `test_empty_ampm`. Row
  "Empty `am_pm` names".

#### An era mismatch is not a fallback

- N3242 22.4.5.1.2, p13; XSH takes the plain conversion "If the
  alternative format or specification does not exist in the current
  locale".
- With era records the alternative exists: `"19"` under `%EC` in ja_JP
  is `eofbit`, nothing read (by probe), where glibc reads a century.
- Why: ffea5ffe. Pins: 22.locale.time.get, "Taisho" and "19" under
  `EC`. Row "An era mismatch is not a fallback".

#### A zeroed `tm` lies in an era

- Under N3242 22.4.5.1.2, p13, `%Ey` reads in the `tm`'s era; 1900 is
  明治 33, พ.ศ. 2443, 民前 12. A lone `"8"` under `%Ey` in ja_JP is 1875
  (by probe).
- Why: ffea5ffe; no latch outlives a call. Pins: row "A zeroed `tm`
  lies in an era".

#### `%Ey` reads four digits

- Under N3242 22.4.5.1.2, p12: 0 to 9999, no sign, as glibc. `"%EC
  %Ey"` on `"平成 40"` keeps 1990 from `%EC`, with `eofbit` (by probe).
- Why: ffea5ffe. Pins: row "`%Ey` reads four digits".

#### An era is every record of its name

- Under N3242 22.4.5.1.2, p13, `"%EC %Ey"` reads `"令和 01"`,
  `time_put`'s 2019-06-01, as 2019; glibc bounds by one record and
  cannot. `%EY` reads `"令和元年"` as 2019 and `"令和8年"` as 2026 (by probe).
- Why: ffea5ffe. Pins: 22.locale.time.get, the Heisei and Gannen
  records. Row "An era is every record of its name".

#### A plain digit under `%O` only where no symbol exists

- XSH says only that `%O` uses the alternative numeric symbols. A value
  with a symbol reads only as the symbol; digits are taken past the
  table, or 0 under an empty first symbol. The inverse of `time_put`;
  glibc takes digits for any value.
- Why: the 2005 import (`__inx >= 2 * __cnt` in `do_get`). Pins:
  `test_user`'s `%O` sections. Row "A plain digit under `%O` only where
  no symbol exists".

#### Two-digit years in `do_get_year`

- Under 22.2.5.1.2, p8 (12.1), `do_get_year` is `%Y`: four characters,
  sign included, no pivot. `"26"` is the year 26; `"12345"` is 1234
  with the 5 unread (by probe).
- Why: the 2005 import; no recorded decision. Pins: 22.locale.time.get,
  `get_year` on `"0"`, `"00"` and `"001"` as the years 0 and 1.

#### Extensions of the interface

- A null `t` to `get` or `do_get` parses without storing (no row);
  N3242 p11 requires a dereferenceable `t`.
- A null `pat_end` to `put` ends the pattern at its null.
- `do_put` with a null `t` writes the locale's `%c`, `%r`, `%x`, `%X`
  format; 22.locale.time.put pins it in its `T (INT_MAX)` rows.
- Directives beyond C90: C99's and POSIX's, `%k`, `%l`, `%OC`, `%Op`,
  HP-UX widths in a locale's formats; `time_get` reads `%k`, `%l`,
  `%Z`, `%z`, `%OC`, `%Op`. Footnotes 235 and 236.

### 12.4 What the tests pin

| program | covers | 15D row |
|---|---|---|
| `22.locale.time.get` | "C", both widths: every directive, the walk, signs, the update model, invalid patterns, `E` without eras. A generated locale with eras (Heisei, its Gannen record, Showa, the backward bar) and 70 alternative digits, both widths, and its twin with an empty `am_pm`. `date_order` and names in the host's English, German and Danish locales, `char` only | status 0, 6 warnings, 3352 assertions |
| `22.locale.time.put` | "C", both widths, against `strftime` where the expected text is a format; years before 0 and past 9999; a stream width that pads nothing; a generated locale with eras and alternative digits | status 0, 2450 assertions |
| `22.locale.time.put.libc.mt` | the first `put` of a facet built from the C library, four threads, `char` and `wchar_t` alternating, 200 rounds | status 0, 200 assertions |

No row fails. The six warnings are three "not exercised" notes per
width: `%c` and the `O` modifier in "C", `%Ex` in the generated locale.
The named-locale sections follow the host; 08295a74 kept the pin to a
host with `en_US.UTF-8` alone. The `get` and `put` MT tests were
retired (7b8aede0, e6d382f5): the facets hold no state.

`TODO`: "The era format comments of the =time_get= test are swapped".
Outside the suite, `bin/oracle/time.cpp` (eras) and `bin/oracle/scalar.cpp`
(names, `%p`, `%x`, `%X`, `%c`) compare `time_put` with glibc.

### 12.5 Open

- The comparisons with glibc's `strptime` behind 12.3 ran outside the
  tree. Check: a `strptime` oracle under `bin/oracle`, beside the
  `strftime` one.

## 13. The departures

Every place the library departs from ISO/IEC 14882:2003, extends it,
or takes a reading the text leaves open, with the chapter that states
it and the paragraph it answers to. "Defect" marks a departure that
is a `TODO` entry rather than a decision.

| departure | paragraph | chapter | kind |
|---|---|---|---|
| Installing the facet a locale holds is a copy | 22.1.1.2 p13, 22.1.1.3 p4 | 8.3 | design |
| `has_facet` has no exception specification | 22.1 synopsis, 22.1.2 | 8.3 | design |
| `use_facet` checks the dynamic type | 22.1.2 p2 | 8.3 | extension |
| An invalid category throws | 22.1.1.2 p14 | 8.3 | extension |
| `locale::facet` and `locale::id` are the library's classes | 22.1.1 synopsis | 8.3 | design |
| A mixed name does not reach the C library | 22.1.1.5 p2 | 8.3 | decision pending |
| Primary template definitions (`ctype`, `codecvt`, `numpunct`, `moneypunct`; `collate` declares only) | 22.1.1.1.1 p4 | 9.3, 10.3.12, 11.3 | design |
| Encodings known to the library (UTF-8, `@UCS`, ISO-2022) | 22.2.1.5 p3 | 9.3 | design |
| Cached `narrow`, `widen` and `always_noconv` | 22.2.1.3.2 p10-11, 22.2.1.1.1 p6-7, 22.2.1.5.1 p5 | 9.3 | design |
| `length` counts complete characters | 22.2.1.5.2 p10, LWG 305 | 9.3 | design |
| An invalid state is an error; the C-library path takes it as it comes | 22.2.1.5.2 p1, p9 | 9.3 | decision 2026-09-16, narrowed 2026-11-04 |
| The classic wide facet narrows by truncation | 22.2.1.5.2 p2 | 9.3 | decision pending |
| `long long` virtuals | 22.2.2.1, 22.2.2.2 synopses | 10.3.1 | decision 2026-10-14 |
| Floating values are grouped | 22.2.2.2.2 p16 | 10.3.3 | C++11's text |
| A zero precision is specified | 22.2.2.2.2 p11, LWG 231 | 10.3.4 | C++11's text |
| The `0x` prefix in stage 2 | 22.2.2.1.2 p8 | 10.3.5 | C++11's text |
| Overflow stores a clamped value | 22.2.2.1.2 p1, p11, LWG 23 | 10.3.6 | design |
| A grouping mismatch stores the value | 22.2.2.1.2 p1, p12; 22.2.6.1.2 p1 | 10.3.7 | design |
| `money_get` sets `eofbit` on success | 22.2.6.1.2 p1 | 10.3.8 | design |
| Fraction digits are exact | 22.2.6.3 p3, 22.2.6.1.2 p4 | 10.3.9 | conformant since 2026-11-03 |
| `money_put` reads `units` as `"%.0Lf"` | 22.2.6.2.2 p1 | 10.3.10 | C++11's text |
| Two spaces after an international symbol; `sign_posn` 0 as `"()"` | 22.2.6.3 p2, p7 | 10.3.11 | reading |
| `collate<char>` orders bytes as `unsigned char` | 22.2.4.1.2 p1, LWG 467 | 11.3 | design |
| An invalid catalog is diagnosed | 22.2.7.1.2 p5 | 11.3 | extension |
| The format members are C++11's | 22.2.5.1; N3242 22.4.5.1.1 p6-10, 22.4.5.1.2 p11-15 | 12.3 | decision 2026-10-14 |
| Each `do_get` updates the `tm` | N3242 22.4.5.1.2 p12, p14 | 12.3 | decision 2026-10-31 |
| Years before 0 | C99 7.23.3.5; POSIX.1-2024 | 12.3 | decision 2026-10-31 |
| Narrow names matched by element | 22.2.5.1.2 p6 | 12.3 | limit |
| Empty `am_pm` names | 22.2.5.1 p1 | 12.3 | decision 2026-10-30 |
| An era mismatch is not a fallback | N3242 22.4.5.1.2 p13 | 12.3 | decision 2026-11-01 |
| A zeroed `tm` lies in an era | N3242 22.4.5.1.2 p13 | 12.3 | decision 2026-11-01 |
| `%Ey` reads four digits | N3242 22.4.5.1.2 p12 | 12.3 | decision 2026-11-01 |
| An era is every record of its name | N3242 22.4.5.1.2 p13 | 12.3 | decision 2026-11-01 |
| A plain digit under `%O` only where no symbol exists | XSH `strptime` | 12.3 | decision 2026-11-01 |
| Two-digit years in `do_get_year` | 22.2.5.1.2 p8 | 12.3 | design |
| Extensions of the `time_get` and `time_put` interface | N3242 22.4.5.1.2 p11; 22.2.5.3 | 12.3 | extension |

## 14. Where to look

| subject | source |
|---|---|
| Public handle, `use_facet`, standard lookup | `include/loc/_locale.h`, `src/locale_core.cpp` |
| Body layout, ids, the classic and global bodies | `src/locale_body.h`, `src/locale_body.cpp` |
| Composition | `src/locale_combine.cpp` |
| Facet base, data loading | `include/loc/_facet.h`, `src/facet.cpp` |
| Factories for standard facets | `src/use_facet.h`, `src/ti_*.cpp`, `src/ctype.cpp` |
| Lazily built C-library data | `src/punct.cpp` (`__rw_get_numpunct`, `__rw_get_moneypunct`) |
| Global and classic locales | `src/locale_global.cpp`, `src/locale_classic.cpp` |
| C locale coordination and file mapping | `src/setlocale.h`, `src/setlocale.cpp`, `src/mman.cpp` |
| Locale sources and the list the build compiles | `etc/nls`, `etc/config/GNUmakefile.bin` |
| Database production and layouts | `util/localedef/`, `include/loc/_localedef.h` |

| The facets, family by family | `include/loc/_<facet>.h`, `_<facet>.cc`, `src/<facet>.cpp`; chapters 8 to 12 |
| The departures | chapter 13, `ref-conformance.md` chapter 2 |

Written by OpenAI LeChuck. Brought up to date with the tree of
2026-11-04; chapters 8 to 12 written against ISO/IEC 14882:2003 on
2026-11-02.
