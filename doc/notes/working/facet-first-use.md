# Two defects in the first use of a facet

Two threads that use a freshly constructed named locale at the same
time can each run into a defect in the library's lazy initialization
of its facets. One leaks references to the facet, so the facet
outlives every locale that holds it; the other hands out a facet that
reports itself initialized before its data exists, and a conversion
through it throws. They were found together on an aarch64 machine,
but neither depends on memory ordering: both fail on any hardware.
Each hides the other, which is why the second one looked rare.

## 0. tl;dr

- **The slot race leaks facet references.**
  `locale::_C_get_std_facet` (`src/locale_core.cpp`) fills an empty
  slot of the locale body with a facet obtained from
  `__rw_facet::_C_manage`, which adds a reference per call, and the
  slot is filled with no lock. When k threads find the slot empty at
  once, all k take a reference and store the same pointer; the body's
  destructor releases the slot once. k - 1 references leak: the named
  facet is never destroyed, and every later locale of the same name
  shares it.
- **The facet data is published before it exists.**
  `__rw_facet::_C_get_data` (`src/facet.cpp`) maps the codeset
  database of a wide `codecvt_byname` with
  `_C_impdata = __rw_get_facet_data (cat, _C_impsize, 0, codeset)`.
  The size is an output parameter, written by `__rw_mmap`
  (`src/mman.cpp`) right after `stat` and before `open` and `mmap`;
  the data pointer is stored only when the call returns. Meanwhile
  `__rw_facet::_C_data` (`include/loc/_facet.h`) takes a non-zero
  size to mean "initialized" and returns the still null pointer. The
  conversion then falls back to the C library, which does not know a
  locale that exists only in the library's database, and `in ()`
  throws `bad locale name`.
- **Measured**, 12 threads, a fresh `de_DE.ISO-8859-1` locale per
  round, 2000 rounds: with the slot filled by one thread first, 2685,
  2674 and 2698 of 24000 first conversions threw (about 11%); with
  the data stored before the size, none did. The data is now stored
  first, and `22.locale.codecvt.mt` carries the case. Left to race, the slot
  leak keeps the facet alive after the first round, the data is
  never mapped again, and the second defect shows only in the first
  round of a process.
- Both sites date from the initial import of the library (2005). The
  second defect was pointed out on the dev list in 2012
  (`locale-mt-2012.md`, chapter 2.2, item 3) and could not be shown
  to fail then; the first was not known.

Machine: aarch64 Linux (Snapdragon X Elite, 12 cores), GCC 16.2.1,
glibc 2.44, build type 15D, 2026-09-26.

## 1. The slot race

### 1.1. The code

`std::use_facet` for a standard facet reads the facet pointer from a
slot of the locale body, with no lock, and calls
`locale::_C_get_std_facet` when the slot is empty
(`include/loc/_locale.h`, `__rw_get_std_facet`). That function works
out the facet's name and type and ends:

```
    facet* const pfacet =
        _RW::__rw_facet::_C_manage (0, FacetType (type), nm, ctor);

    ...

#ifndef _RWSTD_REENTRANT

    // verify that initialization happens exactly once per thread
    // i.e., multiple threads may safely assign the same `pfacet'
    // to the same slot
    _RWSTD_ASSERT (!_C_body->_C_std_facets [inx]);

#endif   // _RWSTD_REENTRANT

    _C_body->_C_std_facets [inx] = pfacet;
```

`_C_manage (0, type, name, ctor)` looks the facet up by type and name
in the process-wide repository under a static lock, constructs it if
it is not there, and in either case adds a reference to it. The
comment is right that every thread stores the same pointer; the
reference each of them took is not accounted for. The body's
destructor (`src/locale_body.cpp`) calls `_C_manage` once per filled
slot to drop one reference, and the facet is destroyed only when its
count reaches zero.

### 1.2. The evidence

The reproducer below opens the codeset database once per construction
of the facet, so counting those opens counts facets. Per 2000 rounds,
each round a new `std::locale ("de_DE.ISO-8859-1")` that is destroyed
at the end of the round:

| threads using the locale | facets constructed |
|---|---|
| 1 | 2000 |
| 2, 3, 4, 12 | 1 to 4 |
| 2, 12, slot filled by one thread first | 2000 |

One thread gets a new facet every round; two racing threads keep the
first facet alive for the rest of the process. Filling the slot from
one thread before the others start restores one facet per round,
which isolates the cause to the slot.

### 1.3. Consequences

- A memory leak per named facet per race, and a facet (with its
  mapped database) that is never destroyed.
- Every later locale of the same name shares the leaked facet, with
  whatever state it has. The locale's lifetime rules no longer hold.

### 1.4. Fix, to decide

The body has a mutex (`__rw_locale::_C_mutex`, `src/locale_body.h`),
used today for its reference count. Two shapes:

1. Obtain the facet from `_C_manage` outside the body's lock, then
   under it store the pointer if the slot is still empty, and
   otherwise return the extra reference through `_C_manage`.
2. Do the whole fill under the body's lock, re-checking the slot
   after acquiring it.

The first keeps `_C_manage`'s static lock and the body's lock from
nesting. Either way the unlocked read in `use_facet` stays, and the
slot store must then publish the facet with release ordering (chapter
3).

## 2. The facet data published early

### 2.1. The code

`__rw_facet::_C_get_data`, for a wide `codecvt_byname` whose locale
is in the library's database, first maps the locale's `LC_CTYPE`
database to find the name of its codeset, then:

```
    // map the codecvt database
    _C_impdata = __rw_get_facet_data (cat, _C_impsize, 0, codeset);
```

`__rw_get_facet_data` composes a path and calls `__rw_mmap`:

```
    struct stat sb;
    if (stat (fname, &sb) == -1)
        return 0;

    *size = sb.st_size;

    const int fd = open (fname, O_RDONLY);
    ...
    void *data = mmap (0, sb.st_size, PROT_READ, MAP_SHARED, fd, 0);
```

So `_C_impsize` becomes non-zero before `open`, and `_C_impdata`
stays null until `mmap` has returned and the call has unwound. The
work happens under the facet's mutex, but the reader does not take
it:

```
    const void* _C_data () const {
        return _C_impsize ? _C_impdata
            : _RWSTD_CONST_CAST (__rw_facet*, this)->_C_get_data ();
    }
```

A thread that calls `_C_data ()` in that window gets null.
`codecvt_byname<wchar_t, char, mbstate_t>::do_in`
(`src/wcodecvt.cpp`) reads null as "no database" and uses the C
library: it constructs `__rw_setlocale (_C_name, LC_CTYPE)`, which
throws for a name the C library does not know. Every locale that
exists only in the library's database is such a name.

For a UTF-8 locale the null pointer is harmless: the conversion does
not use the C library for UTF encodings and falls back to its own
algorithmic UTF-8 transformation, with the same result. The defect
shows on the single-byte and legacy multibyte encodings.

A second, smaller defect sits on the same line: when `open` or `mmap`
fails after `stat` succeeded, the facet is left with a non-zero size
and a null pointer for good, and every later call takes the C library
path. That is the "no database" behaviour, reached by a different
route; it is recorded here and not measured.

### 2.2. The evidence

The reproducer (chapter 4) with the slot filled first, 12 threads,
2000 rounds, a fresh locale each round:

| library | first conversions that threw, of 24000 |
|---|---|
| size stored first | 2685, 2674, 2698 |
| data mapped into locals, stored before the size | 0, 0, 0 |

The first exception of every run, the only one printed, carried the
same message, from `src/setlocale.cpp`:
`__rw_setlocale(const char*, int, int): bad locale name:
"de_DE.ISO-8859-1"`. With 2 threads, 0 and 1 threw in two runs of
2000 rounds: the window is a few microseconds and two threads rarely
meet in it.

To confirm that the throws come from this window and not from
somewhere else on the same path, the window was also widened on
purpose, by delaying `open` of the codeset database through a
preloaded shim, with the workers starting 100 microseconds apart: the
number of throws rose with the delay (about 9 of the 11 late threads
per construction at 1 ms), and with the fix it
stayed at zero over 162 constructions. That experiment established
the cause; the numbers above, without any shim, are the defect as
programs meet it.

### 2.3. The fix

`_C_get_data` maps into locals and stores the data before the size:

```
    size_t cvtsize = 0;
    const void* const cvtdata =
        __rw_get_facet_data (cat, cvtsize, 0, codeset);
    _C_impdata = cvtdata;
    _C_impsize = cvtsize;
```

This removes the program-order defect, which is what fails. The
stores are still plain, and the reader's loads are still unordered;
chapter 3 is about that.

### 2.4. The test

`22.locale.codecvt.mt` ends with a case that builds
`de_DE.ISO-8859-1` from the tree's sources into a locale root of its
own, skips when the C library knows the name, and then, 200 times,
constructs the locale, fills its slot from the main thread and has
the pool's threads, created anew each round, convert `"abc"` on
first use. Thread creation staggers the threads' arrival enough that
some land in the window. On the aarch64 machine, 12 threads: 81, 47,
75 and 55 failed assertions in four runs before the fix, 0 in four
runs after. The case runs only in builds with threads.

## 3. What these fixes do not cover

Both sites publish a pointer through an unlocked read: the slot in
`use_facet`, and `_C_impdata` through `_C_impsize` in `_C_data`. With
both defects fixed the stores happen in the right order in the
program, but the C++ memory model still calls the unlocked reads data
races: a release store and an acquire load are what make them
defined. On the aarch64 machine a litmus test of the `_C_data` shape
(`locale-mt-2012.md`, chapter 7, and the working notes) showed no
reordering in 60 million rounds, so this part has no failing test;
it is the third class of the fix list in `TODO`.

## 4. The reproducer

One process, N worker threads parked on a barrier. Each round the
main thread constructs `std::locale (name)`, optionally calls
`std::use_facet<std::codecvt<wchar_t, char, std::mbstate_t> >` once
itself (to fill the slot alone), releases the workers, waits for
them, and destroys the locale. Each worker calls `use_facet` and then
`in ()` on the three characters `"abc"`, counting exceptions, results
other than `ok`, and wrong characters. The locale database is the
build directory's `nls`, named through `RWSTD_LOCALE_ROOT`; the C
library must not know the locale's name.

The test in the suite follows this shape (chapter 2.4); the
reproducer itself is not part of the tree.
