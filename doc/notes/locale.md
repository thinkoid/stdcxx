# Inside `std::locale`

## 0. tl;dr

A `locale` is a small handle to a **shared collection of facets**.
The facets supply character classification, conversion, collation,
formatting, parsing and message lookup. The library shares both the
collection and its individual facets, and postpones much of their
construction and data loading until first use.

That is the central design: **make copies cheap, reuse common work,
and keep repeated facet access short**. It also puts mutable machinery
behind an interface that looks read-only. Reference counting,
repository locking and lazy initialization are separate parts of the
threading story.

This is an architectural overview of the tree as of 2026-09-27.

## 1. The handle, the body and the facets

`std::locale` holds one pointer, `_C_body`, to `__rw::__rw_locale`.
Copying a locale copies that pointer and increments the body's
reference count; it does not copy its facets.

The body contains the locale name, a reference count, a mutex and two
collections of facet pointers:

- A fixed array of 26 standard facet slots, covering the narrow and
  wide specializations. An ordinary facet and its `byname` counterpart
  occupy the same slot.
- A separate array for installed facets with nonstandard ids.

Two bitmaps describe which standard slots contain library-managed
facets and which contain managed `byname` facets. They let the body
reason about whole categories without constructing every facet.
**A null standard slot can mean “not constructed yet”, not “absent”.**

Each facet has its own reference count. Thus two different bodies can
share a facet even when they cannot share the whole collection.
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
reference. For a standard facet, the common path is an indexed pointer
read. On a miss, `_C_get_std_facet` selects the category's name, asks
the facet repository for an instance and stores the pointer in the
body. The repository shares library-created facets by type and name.
Ordinary standard facets use static storage; named ones are allocated.

**The locale selects the service; the facet implements it.** For
example, numeric parsing and output use punctuation supplied by other
facets through the stream's locale. `num_get` and `num_put` have no
`byname` class of their own.

## 3. Composition and the global locale

The locale-body repository reuses named collections. Constructing a
locale by name need not create a new body, and composing locales need
not copy facet objects.

The category constructor takes selected categories from another
locale. `a.combine<T>(b)` takes one facet from `b`, retaining the rest
of `a`. Installing a user facet follows the same general route:
`_C_get_body` and `_C_combine` reuse an existing body where possible,
otherwise build a collection of shared facet pointers. Category
mixtures can still have a composite name; combinations that cannot
be represented as a managed named locale report `"*"`.

**Composition produces a new locale value; it does not edit the
source collection.** Internally, however, both collections may still
materialize shared facets on demand. That is the one change a live
body sees: a standard slot goes from null to its facet, once, under
the body's lock, and never changes again until the body is destroyed.
Lookup can therefore read a slot without the lock, and a copy of a
collection can take its slots one at a time: each read sees either
null, which the new body fills on its own first use with the same
managed facet, or the final pointer.

Default construction takes a reference to the current C++ global
body. `locale::global` replaces that body and returns the old locale;
existing handles retain their bodies. For a named locale it also
calls C `setlocale`. `locale::classic()` supplies the permanent `"C"`
locale through a separate once-initialized handle.

## 4. Copying and the graph around `_C_manage`

### 4.1 Avoiding work at three levels

**An ordinary copy does not visit the repository.** The copy
constructor in `locale_core.cpp` copies `_C_body` and atomically
increments the body's reference count. Its work is independent of
the number of installed facets: no allocation, facet traversal,
virtual call or database mapping is needed. The atomic abstraction
may itself use a lock, depending on the configuration; bypassing the
repository does not mean the operation is universally lock-free.

Assignment acquires the incoming reference **before releasing the
old one**, then replaces the pointer. This order also handles
self-assignment without a special test. Release uses a temporary
handle whose destructor follows the normal disposal path. Acquiring
the new body is cheap; disposing of the old one can involve the
repository and, on its last reference, destruction of the body and
release of its facets.

Composition tries progressively more work only as needed:

| opportunity | work avoided |
|---|---|
| Same source body, or category `none` | Category composition returns the original body with another reference. |
| Null facet, or the exact facet already installed | The facet constructor reduces to sharing the original body. |
| Result fully described by category names | `_C_combine` composes a name and asks `_C_manage` for the corresponding body. A repository hit avoids constructing a collection. |
| A distinct collection is necessary | `_C_construct` copies facet pointers and increments their counts, rather than cloning facet objects. Null standard slots remain lazy. |

The last case can allocate a body, a user-facet pointer array and name
storage. It still shares the expensive services and their data.
**Body sharing saves the whole collection; facet sharing saves its
contents when the collection must differ.**

Managed composition depends on the facet bitmaps and category names,
not just on the spelling returned by `name()`. Replacing one facet
can make a category a mixture that has no usable name; replacing its
remaining facets can make the collection managed again. The code's
example replaces the narrow and wide `messages` facets in succession:
after the first replacement it needs a private body, after the second
it can return to the named repository. Conversely, selecting `all`
categories is not an unconditional copy of the donor: user facets
outside those categories must be accounted for. That shortcut is
explicitly disabled in `_C_combine`.

### 4.2 The body manager as a junction

There are **two different `_C_manage` functions**. At the centre of
the body-management graph is `__rw_locale::_C_manage`, which owns the
named-body repository and the current global-body pointer. The facet
manager is a separate repository one level below it.

Default construction and `locale::global` reach the body manager
directly. Named construction comes through `_C_get_body`; composition
comes through `_C_get_body` and `_C_combine`, reaching the manager
when the result has a managed name. Otherwise composition branches
off to construct a private body. `classic()` joins the named-construction
path when its once initializer constructs the public `"C"` handle.

Destruction closes the loop. A managed handle returns its reference
to the body manager, which may remove and destroy the body. An
unmanaged handle decrements the count and destroys the body directly
at zero. In either case, the body destructor releases its facets,
passing library-managed ones to `__rw_facet::_C_manage`. That second
manager also receives requests from lazy standard-facet lookup.
**Ordinary copying branches around both managers**, acquiring only
another reference to the existing body.

The body manager's two arguments encode four requests:

| call | meaning |
|---|---|
| `_C_manage (0, 0)` | Acquire a reference to the current global body. |
| `_C_manage (body, 0)` | Retain `body` as the new global and return the previous global body. Its old global reference passes to the returned locale. |
| `_C_manage (0, name)` | Find or construct the named body and return an acquired reference. |
| `_C_manage (body, name)` | Release a managed body; remove and destroy it if its count reaches zero, except for the permanent classic body. |

The named repository is a sorted array of pointers, searched with
`bsearch` and sorted after insertion. It starts with space for eight
pointers in static storage and grows when necessary. **It reuses live
bodies; it is not a permanent cache of every locale ever requested.**
The classic body is special: it lives in static storage, and its
release path decrements the count without taking the repository lock.

The repository lock protects lookup, insertion and removal, and the
global-body branch uses it when acquiring or replacing the global
reference. Initial global setup has its own atomic-counter and
`volatile`-spin protocol, which recursively requests the `"C"` body;
this is distinct from `classic()`'s once-initialized public handle.
These paths deserve separate examination when studying publication.

The graph explains the MT tradeoff: copying an existing handle avoids
central repository contention, while constructing by name and changing
the global locale converge on shared machinery. Lazy facet lookup
avoids that machinery after a slot is populated, but **the repository
lock is not a lock around subsequent reads and writes in the body or
facet**. Chapter 6 identifies those separate boundaries.

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
grouping and boolean names; its accessors now call the virtuals every
time, the members kept unused until the next minor version for binary
compatibility. **Mapped data and mutable facet caches are different
layers.**

The C library is another backend, with selection and fallback varying
by facet and implementation options. Paths that temporarily change
the C locale use `__rw_setlocale` to lock, save, switch and restore it.
This process-wide state is distinct from the C++ collection of facets;
the library's lock coordinates its own participating calls.

## 6. The threading picture

The important boundaries in a reentrant build are:

| mechanism | purpose and boundary |
|---|---|
| Body and facet reference counts | Preserve shared ownership; every update of a facet's count uses the same lock-free operation. They do not synchronize arbitrary writes inside a facet. |
| Locale and facet repository locks | Serialize repository operations and reuse. |
| Standard facet slots | Filled once, under the body's lock, with a release store; lookup, copying, combining and comparison read them unlocked, with acquire loads where the facet is then used. |
| Lazy facet data | `_C_get_data` takes the facet mutex and checks again before loading; the size, or the data pointer itself where the data is built from the C library later, is stored with release, and the unlocked reads are acquire loads. |
| Cached scalars | The `ctype` and `codecvt` caches are filled with relaxed atomic accesses: every thread stores the same value and nothing else is published through them. |
| C locale lock | Covers participating `setlocale` operations, a separate source of serialization. |

**Shared lifetime does not imply safe publication or safe cache
initialization.** The fast paths avoid repeated locking and
computation, and each needs its own publication. The ThreadSanitizer
investigation found races in punctuation caches, implementation-data
publication and body bookkeeping, including use-after-free during
cached string assignment; see [the investigation](working/locale-mt-race.md).
Those are repaired as the table above describes, and a run of the
locale MT tests under the sanitizer reports nothing from the facets
or bodies. The initialization of the global locale, a counter and a
`volatile` spin, is the remaining candidate, queued with the other
flag-based initializations in `TODO`.

A lock inside initialization alone does not establish that another
thread can safely read the initialized state: the data-loading slow
path always took a mutex, and its unlocked checks were the problem.
On aarch64 the library's atomic operations are still mutex operations
until the builtin backend is ported (`TODO`).

## 7. Where to look

| subject | source |
|---|---|
| Public handle, `use_facet`, standard lookup | `include/loc/_locale.h`, `src/locale_core.cpp` |
| Body layout, ids, named-body repository | `src/locale_body.h`, `src/locale_body.cpp` |
| Composition | `src/locale_combine.cpp` |
| Facet base, repository, data loading | `include/loc/_facet.h`, `src/facet.cpp` |
| Factories for standard facets | `src/use_facet.h`, `src/ti_*.cpp` |
| A concrete lazy cache | `include/loc/_numpunct.h`, `src/punct.cpp` |
| Global and classic locales | `src/locale_global.cpp`, `src/locale_classic.cpp` |
| C locale coordination and file mapping | `src/setlocale.h`, `src/setlocale.cpp`, `src/mman.cpp` |
| Database production and layouts | `util/localedef.cpp`, `include/loc/_localedef.h` |

Written by OpenAI LeChuck.
Brought up to date with the tree of 2026-09-27 by Claude.
