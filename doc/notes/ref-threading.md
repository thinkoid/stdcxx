# The threading model and its atomic operations

## 0. tl;dr

stdcxx uses atomic operations to make shared ownership inexpensive:
copies retain a representation, releases count down to destruction,
and callers avoid taking a repository lock for every copy. That is a
useful design. The trouble starts **where an atomic counter is being
asked to protect a larger object**.

An indivisible increment does not protect the pointer used to reach
the counter, serialize writes to the object, or announce that its
initialization has finished. Those are separate obligations. The
library's small atomic interface obscures the distinction: the same
macro can select a hardware operation, an ordinary operation under a
mutex, or an ordinary operation with no synchronization at all.

The clearest failure was the locale cache: two threads assigned the
same cached string, although its representation's reference count was
protected. It is repaired, together with the publication of facet data
and of the facets themselves, the facet id race and a mixed
synchronization of facet counts (chapters 4 and 5). One-time
initialization goes through one protocol, `__rw_once` (chapter 5).
**Making every increment stronger would not have repaired these
protocols.**

This note describes the source model and guides the remaining MT work.
The [locale investigation](analysis-locale-mt-race.md) records the
measurements; the `22.locale.*.mt` tests pin the repairs.

## 1. What reentrant mode means

`_RWSTD_REENTRANT` enables the library's synchronization machinery.
In [the macro layer](../../include/rw/_defs.h), guards acquire
mutexes and atomic wrappers dispatch to their implementations. In a
nonreentrant build, guards disappear and the corresponding operations
become ordinary increments, decrements and exchanges. Library and
client headers must agree on these configuration choices; some also
affect object representation.

**Reentrant mode is an implementation mode, not a blanket guarantee
that any two operations on any object can overlap.** Distinguish:

- Separate handles sharing an internal representation: reference
  management is intended to support this without copying all the data.
- Concurrent mutation of the same handle: the representation's count
  cannot protect the handle's pointer or the whole assignment.
- Shared library infrastructure: repositories, lazy caches and global
  state need their own synchronization, even when reached through
  different user objects or through `const` functions.
- The standard stream objects: the library locks each operation on
  them. A user's stream is not locked.

**A user's stream and its buffer are not locked.** Concurrent access
to one stream object may race, as C++11 permits
([iostreams.threadsafety]). `ios_base::_C_init`, which
`basic_ios<>::init` calls, sets `_C_nolock` for every stream.
`ios_base::Init` clears it on the eight standard objects, whose
sentries and state changes then take the stream's and the buffer's
mutex. C++11 asks that of a standard object synchronized with stdio
([iostream.objects]). See [ios.cpp](../../src/ios.cpp),
[iostream.cpp](../../src/iostream.cpp) and
[_basic_ios.h](../../include/rw/_basic_ios.h).

The historical [Level 2 contract](../stdlibug/44-1.html) describes the
flags `nolock` and `nolockbuf` that once selected the locking of each
stream. They are gone; a user who shares a stream between threads
synchronizes it.

Even with stream locking, a sequence of insertions is not one
indivisible transaction. The [stream locking description](../stdlibug/44-2.html)
also distinguishes the stream-state mutex from the buffer mutex:
sentries supply the latter protection for stream operations; direct
buffer access does not automatically acquire it.

The C locale remains a separate process resource. The save/switch/restore
guard in [setlocale.h](../../src/setlocale.h) coordinates participating
library calls; it cannot serialize external calls that bypass its lock.
`_RWSTD_REENTRANT` does not turn that process state into private state
for each C++ locale.

## 2. What an atomic operation actually supplies

Five questions must be answered separately for each shared object:

| obligation | question |
|---|---|
| Indivisibility | Can two updates to this scalar lose one another? |
| Publication | How does a reader acquire visibility of the completed data? |
| Lifetime | What keeps the object alive before the reader can increment its count? |
| Exclusion | What prevents concurrent changes to the object's mutable fields? |
| Compound invariant | What keeps its pointer, count, flags and repository membership consistent? |

An atomic reference count answers the first question. Together with a
correct ownership protocol and ordering, it can help answer the third.
It does not answer the remaining questions by proximity.

In particular, retaining through an unprotected pointer is too late
if another thread can free the pointee between the pointer read and
the increment. The reader needs an existing owner or a protected
acquisition path. Likewise, a reader must not interpret “someone has
started initialization” as “all initialized fields are ready.”

Memory ordering concerns both compiler transformations and processor
visibility. A strong instruction on x86 is not a complete proof of a
source protocol. GCC explicitly says that volatile access does not
order unrelated nonvolatile memory; **`volatile` is not a publication
mechanism**. See [GCC's volatile semantics](https://gcc.gnu.org/onlinedocs/gcc/Volatiles.html).

## 3. The abstraction hides several different mechanisms

The read-modify-write entry points are `_RWSTD_ATOMIC_PREINCREMENT`,
`_RWSTD_ATOMIC_PREDECREMENT` and `_RWSTD_ATOMIC_SWAP`. Increment and
decrement return the new value; exchange returns the previous value.
They take no ordering argument.

Loads and stores with an explicit ordering came later:
`_RWSTD_ATOMIC_STORE_RELEASE` and `_RWSTD_ATOMIC_LOAD_ACQUIRE` publish
data through a flag or a pointer, and `_RWSTD_ATOMIC_STORE_RELAXED`
and `_RWSTD_ATOMIC_LOAD_RELAXED` state a race that orders nothing,
such as a cache every thread fills with the same value. They take
the mutex argument of the rest of the family and call the backend's
`__rw_atomic_load_acquire`, `__rw_atomic_load_relaxed`,
`__rw_atomic_store_release` and `__rw_atomic_store_relaxed`; in a
nonreentrant build they are ordinary accesses
([_defs.h](../../include/rw/_defs.h)). The value a store writes is
not deduced: it converts to the type of the object, which alone
selects the overload. There is still no compare-and-exchange.

### 3.1 Backend selection and the meaning of `false`

[_atomic.h](../../include/rw/_atomic.h) selects the backend over
the GNU `__atomic` built-ins where the configuration found them
working on `int`, and the mutex fallback otherwise. Width adapters
and missing-width fallbacks add another dispatch layer: each width follows its own characterization,
`CHAR_ATOMIC_OPS.cpp` through `LLONG_ATOMIC_OPS.cpp`, and a width
whose characterization fails is served by the mutex templates. On
aarch64 GCC and Clang compile the built-ins to calls to the compiler
runtime's outline helpers, which choose the LSE instructions or an
exclusive load and store loop at run time.

| call form in a reentrant build | mechanism |
|---|---|
| Supported scalar with `false` | Hardware/compiler overload when available. |
| Scalar with `false`, without a matching hardware overload | Generic operation under a static mutex associated with the scalar type. |
| Scalar with an explicit `__rw_mutex_base&` | Ordinary scalar operation under that mutex. |
| `volatile` scalar with `false` | No hardware overload takes a `volatile` object, so the call takes the mutex template for the `volatile` type. Callers cast `volatile` away first, as the increments of STDCXX-792 do. |
| `_RWSTD_THREAD_*`, with TLS available | Ordinary operation on thread-local storage. |
| `_RWSTD_THREAD_*`, without TLS | Corresponding atomic wrapper on shared storage. |
| `_RWSTD_ATOMIC_IO_SWAP` | Ordinary exchange on an unlocked stream (`_C_nolock` set, a user's stream); otherwise the selected atomic wrapper with its supplied mutex. |

**`false` is an overload tag, not a request for relaxed ordering or
for no locking.** Conversely, an explicit mutex does not merely
decorate a hardware operation: it selects the mutex implementation.
See [_atomic-mutex.h](../../include/rw/_atomic-mutex.h).

That implementation states its essential condition: callers must use
the same mutex for their operations to be mutually atomic. A hardware
RMW on a scalar does not respect a mutex around somebody else's
ordinary `++` on that scalar. Two different mutexes do not coordinate
those ordinary operations either.

Static locks also have different scopes. The generic scalar fallback
uses a lock associated with the value type, so unrelated counters can
contend. `_RWSTD_MT_STATIC_GUARD(T)` incorporates `__LINE__` into its
type: separate call sites using the same `T` need not share a lock.
Read the expansion before drawing a lock boundary around a function
or subsystem.

### 3.2 Ordering is not uniform

[_atomic-builtins.h](../../include/rw/_atomic-builtins.h) implements
the increment, the decrement and the exchange with
`__atomic_add_fetch`, `__atomic_sub_fetch` and `__atomic_exchange_n`,
all sequentially consistent, and the ordered loads and stores with the
order their names give, for every characterized width, for `wchar_t`
and for pointers of every type. See
[GCC's memory-model built-ins](https://gcc.gnu.org/onlinedocs/gcc/_005f_005fatomic-Builtins.html).

On x86 and x86-64 the read-modify-writes are locked `xadd` and
`xchg`. On aarch64 they are calls to the compiler runtime's
`acq_rel` outline helpers: `ldaddal` and `swpal` on a CPU with the
LSE atomics, an acquire-release exclusive pair otherwise. The `__sync`
built-ins the backend replaced added a trailing full barrier on the
exclusive-pair path. That barrier ordered later ordinary accesses
after the store, which C++ sequential consistency does not promise;
the library uses the increments for counts, ids and elections, none
of which needs more than acquire and release. The mutex fallback
supplies ordering through the lock and unlock of the mutex its
callers share.

The library still exports the x86 and x86-64 assembly routines, for
32-bit binaries built before the characterizations, which call them.
No header refers to them; the `TODO` entry on the next minor version
removes them with their symbols.

**Even a full barrier cannot make an unprotected compound operation
indivisible.** It also does not convert every ordinary access elsewhere
to the same counter into a synchronized access.

## 4. Reference counts: the larger object remains exposed

### 4.1 Strings

[_strref.h](../../include/rw/_strref.h) holds the count, capacity,
size and access to the character storage. Its increment/decrement
wrappers protect count updates through hardware operations, one
shared string mutex, or a mutex in the representation, according to
configuration. The immortal empty representation has special handling.

The mutex around a decrement is not a mutex around string assignment.
Assignment also reads and replaces the string object's representation
pointer, manages the old representation and possibly allocates a new
one. Two assignments to the same string can interleave outside the
counter's critical section. One can then attempt to retain storage
that the other has already released.

There are ordinary count accesses too: `_C_get_ref()` reads `_C_refs`
directly, and `_C_unref()` writes the unshareable sentinel with an
explicit comment that it is not thread-safe. Audit those callers and
their exclusivity assumptions; the presence of atomic increment and
decrement elsewhere does not settle their safety.

The Linux/x86-64 compatibility clause in
[_config.h](../../include/rw/_config.h) can retain per-representation
mutexes instead of string hardware atomics for pre-5 library versions.
Changing that policy can remove overhead and alter representation.
**It cannot repair concurrent assignment to the same string.**

### 4.2 Locale bodies and facets

The [locale tour](ref-locale.md) describes the ownership graph around
`__rw_locale::_C_manage`. Retaining an existing body avoids rebuilding
its facet collection. Repositories separately coordinate finding,
inserting and retiring shared bodies and facets. The count and the
repository lock serve different purposes.

The global-locale branch and named-body repository branch in
[locale_body.cpp](../../src/locale_body.cpp) have distinct static-guard
call sites. Their identical type argument does not make them one
mutex. Global replacement retains the incoming body before handing
off the previous global reference; ordinary copies retain an already
owned body. Those ownership transitions must survive any redesign.

Facet counts had **mixed synchronization**: the two constructors in
[locale_combine.cpp](../../src/locale_combine.cpp) that build a body
from another incremented them under the facet's mutex, while the
helpers in [locale_body.h](../../src/locale_body.h) and every other
site pass `false`. The two do not exclude each other, and a test that
builds and destroys locales from a shared one in several threads
(`22.locale.facet.mt`) lost updates in most runs. All facet-count
updates now go through `_C_add_ref` and `_C_remove_ref`. The body's
own mutex-taking count helpers have no callers; body counts pass
`false` everywhere.

### 4.3 Lazy caches are writes, including through `const`

The [locale MT record](analysis-locale-mt-race.md) identified
concurrent lazy initialization of `numpunct` members: accessors in
[_numpunct.h](../../include/loc/_numpunct.h) tested `_C_flags`,
obtained a value, assigned the cache and updated the flag. Two threads
could both enter; for cached strings, both assigned the same string
object, and the use-after-free and destroyed-mutex reports followed.
The accessors now call the virtuals every time; the members stay,
unused, until the `TODO` entry on the next minor version removes them.

An atomic flags word alone would still have permitted both
initializers to write the cache, and an election bit set before
initialization would have let readers arrive before completion. A
sound protocol distinguishes uninitialized, being initialized and
ready, or uses a lock covering both access and initialization, or,
as here, removes the cache.

`__rw_facet::_C_get_data()` in [facet.cpp](../../src/facet.cpp)
**acquires the facet mutex on its slow path**; its initial check and
the fast path in [_facet.h](../../include/loc/_facet.h) are outside
that mutex. The question was publication to those readers, not the
absence of a lock. `_C_impsize` is now stored with release after
`_C_impdata`, and read with acquire; where the data is built from the
C library after the size is already set, `_C_impdata` itself is the
published pointer, stored with release under `__rw_setlocale` and
read with acquire, including under the facet mutex, which does not
exclude those builders.

The body's standard-facet slots follow the same rule: filled once
under the body's lock with a release store, read unlocked with
acquire. The `ctype` `narrow` and `widen` tables and
`codecvt<char, char>::always_noconv` stay caches, because every thread
stores the same value and nothing else is published through them;
their accesses are relaxed atomics. A reference count keeps a facet
alive; it does not make its mutable cache immutable.

## 5. Initialization: election is not completion

Several mechanisms used a counter to choose a first caller, then
asked that counter to stand for the readiness of unrelated state. An
increment elects, but it announces nothing: the elected caller's
writes reach the others only through a release they acquire, and the
others must wait for it. Every one-time initialization in the
library now goes through one protocol.

[`__rw_once`](../../include/rw/_once.h) has the shape of Boost's
atomic `call_once`. Its flag is a zero-initialized `int` with three
states: ready, running and done. A caller that loads done with
acquire returns at once. Any other caller takes the one mutex every
flag shares. A ready flag elects it: it marks the flag running,
unlocks, and calls the initializer. A running flag makes it wait on
the one condition variable; a done flag sends it back. When the
initializer returns, its caller stores done with release under the
mutex and wakes every waiter. When the initializer throws, it stores
ready instead, and one of the woken waiters calls the initializer in
turn. The mutex is never held while an initializer runs, so an
initializer may itself initialize through another flag; through its
own flag it deadlocks ([once.cpp](../../src/once.cpp)).

| site | what it initializes |
|---|---|
| `locale::classic`, [locale_classic.cpp](../../src/locale_classic.cpp) | The classic locale object. |
| `ctype<wchar_t>`, [wctype.cpp](../../src/wctype.cpp) | The classic wide mask table, where `_RWSTD_NO_EQUAL_CTYPE_MASK` is defined. |
| Global locale, `__rw_locale::_C_manage` | The global locale, the classic body at first. The initializer runs before the `__rw_locale` guard, because building the body takes that guard. |
| `ios_base::Init`, [iostream.cpp](../../src/iostream.cpp) | The eight standard iostream objects. Every `Init` is counted, for the destructor's flush, and waits for the first to construct them. |
| `__rw_vfmtwhat`, [exception.cpp](../../src/exception.cpp) | The message catalog of the library's exceptions. |
| `__rw_get_static_mutex`, [_mutex.h](../../include/rw/_mutex.h) | Each static mutex, where the configuration cannot initialize mutexes statically. |

The static mutexes bound the protocol from below. Where `int` is not
lock-free, the ordered accesses to an `int` lock the static mutex of
`int`, which may be one of the mutexes `__rw_once` constructs. There
`__rw_once` has no unlocked fast path, and every access to a flag
holds the protocol's own mutex, which is a POSIX mutex initialized
statically.

`__rw_once` no longer calls `pthread_once`: POSIX leaves undefined
an exception thrown through it, and glibc's recovery is an
implementation detail. Its fallbacks for systems without
`pthread_once` went with it.

Two related sites need no protocol. `__rw_facet_id::_C_init`
([facet.cpp](../../src/facet.cpp)) generates a facet type's id once,
under a static lock with a recheck, stored with release and read with
acquire; `22.locale.id.mt` counts the ids 500 types take.
`_C_manage` compares a body with the classic body before it takes the
guard, with a relaxed load. Only a thread that holds the classic body
compares equal, and it got the body through the guard; nothing is
published through the pointer.

## 6. Other uses and their limits

These callers keep the survey wider than locale and string:

| site | role of the atomic operation | boundary to preserve or examine |
|---|---|---|
| [iostore.cpp](../../src/iostore.cpp), `xalloc()` | Allocates an index by incrementing a shared counter. | The counter's result is the resource; this is a direct scalar use, not protection for every stream's indexed storage. |
| [ios.cpp](../../src/ios.cpp), `precision()` and `flags()` | Exchanges a scalar under the stream mutex when locking is enabled. | Coordinate readers and larger stream operations with that same locking policy. |
| `sync_with_stdio()` in the same file | Exchanges the shared mode flag through the `false` overload. | An atomic flag change alone does not serialize all ongoing buffer operations or their transitions. |
| [tmpbuf.cpp](../../src/tmpbuf.cpp) | Increment returning one reserves the reusable buffer; losers decrement and allocate elsewhere; return releases it. | Here a counter can mediate access to larger storage because losers do not use that storage. Verify every use follows the reservation and release protocol. TLS makes the reservation local to a thread. |
| [exception.cpp](../../src/exception.cpp) | Reserves and retains the reusable exception-message buffer, using TLS when available. | Audit ownership transfer, release and thread affinity; a count does not make simultaneous formatting safe. |
| [tr1/_smartptr.h](../../include/tr1/_smartptr.h) | Manages strong and weak counts and deletion. `_C_get_ref()` obtains a value by incrementing and decrementing. | This directly included TR1 implementation and its tests are part of the survey. The two-counter retirement protocol needs its own proof; its “read” is actually two RMWs that temporarily change ownership accounting. |

The buffer reservation is an important counterexample to a blanket
rejection of counters. **A counter can protect access to a larger
object when a complete protocol gives exactly one participant access
and all others obey it.** A shared ownership count normally permits
multiple participants and therefore supplies no such exclusion.

## 7. Work order for the MT repair

1. **Name the protected state.** For each count or flag, record its
   storage, all access paths, selected overloads, lock identity,
   ownership preconditions and the invariant it is meant to preserve.
   Include ordinary accesses and destruction, not only macro calls.
2. **Repair the demonstrated cache protocol.** Done for the locale:
   the `numpunct` cache is gone, facet data, facets and ids are
   published with release and acquire, the scalar caches are relaxed
   atomics. Count optimization remains a separate decision.
3. **Unify synchronization for each counter.** Done for facet counts.
   An atomic field must not retain conflicting ordinary accesses; a
   mutex-based field needs the same mutex at every overlapping access.
   The string counts and their ordinary accesses (chapter 4.1) are
   still to audit.
4. **Give initialization a completion protocol.** Done: every
   one-time initialization goes through `__rw_once` (chapter 5),
   which covers readers, exceptions and the bootstrap of the static
   mutexes. Removing `volatile` or strengthening the election
   instruction alone would have been insufficient.
5. **Then choose memory orders and representations.** Retaining from
   an existing owner, publishing new state and performing the last
   release have different requirements. Derive them from the complete
   protocol. Adopting a newer implementation language does not itself
   supply this library with a usable public `<atomic>` implementation;
   select the internal primitive without accidentally depending on a
   host standard library.

The existing [atomic arithmetic test](../../tests/support/atomic_add.cpp)
and [exchange test](../../tests/support/atomic_xchg.cpp) exercise the
primitives. They cannot establish that a locale cache or an ownership
handoff is safe. Verification must also force concurrent first use,
copy/release and last-owner transitions, and initialization failure
where supported. Use the locale MT tests and ThreadSanitizer for the
known paths; separately exercise reachable fallback configurations.
Any ordering argument intended to survive beyond x86 needs evidence
beyond an x86 stress run. A clean run supports a protocol review; it
does not replace one. The locale tests that pin the repairs release a
few threads together on fresh state, round after round, rather than
repeat an operation on warm state: a first-use race shows in seconds
that way, and not at all once everything is cached.

Written by OpenAI LeChuck.
Brought up to date with the tree of 2026-09-27 by Claude.
