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
synchronization of facet counts (chapters 4 and 5). Initialization
counters remain. **Making every increment
stronger would not have repaired these protocols.**

This note describes the source model and guides the remaining MT work.
The [locale investigation](working/locale-mt-race.md) records the
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
- Streams with locking enabled: the library supplies additional
  operation-level locking, subject to its configuration and flags.

The historical [Level 2 contract](../stdlibug/44-1.html) describes
optional internal protection for iostreams and locales. Its flag
description contradicts itself: **the source treats `nolock` and
`nolockbuf` as disabling their respective locks when set**. Ordinary
streams start with locking disabled on the extension-enabled path;
standard-stream initialization clears those bits when the configured
defaults permit it. See [ios.cpp](../../src/ios.cpp),
[iostream.cpp](../../src/iostream.cpp) and
[_basic_ios.h](../../include/rw/_basic_ios.h).

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
such as a cache every thread fills with the same value. They expand
to the GNU `__atomic` built-ins where `ATOMIC_BUILTINS.cpp` finds them
and to ordinary accesses otherwise or in a nonreentrant build
([_defs.h](../../include/rw/_defs.h)). There is still no
compare-and-exchange.

### 3.1 Backend selection and the meaning of `false`

[_atomic.h](../../include/rw/_atomic.h) selects the GNU `__sync`
backend where the configuration found the built-ins working on `int`,
x86 assembly helpers on other matching branches, and mutex fallback
otherwise. Width adapters and missing-width fallbacks add another
dispatch layer: each width follows its own characterization,
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
| `_RWSTD_THREAD_*`, with TLS available | Ordinary operation on thread-local storage. |
| `_RWSTD_THREAD_*`, without TLS | Corresponding atomic wrapper on shared storage. |
| `_RWSTD_ATOMIC_IO_SWAP` | With the locking extension enabled, ordinary exchange if `nolock` is set; otherwise the selected atomic wrapper with its supplied mutex. Without that extension, the atomic wrapper is unconditional. |

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

[_atomic-sync.h](../../include/rw/_atomic-sync.h) implements arithmetic
with `__sync_add_and_fetch` and `__sync_sub_and_fetch`, but exchange
with `__sync_lock_test_and_set`. GCC documents the arithmetic
operations as full barriers and that exchange as an **acquire**
barrier. Its interface therefore does not promise release publication
of earlier writes merely because the macro is called `ATOMIC_SWAP`.
See [GCC's legacy atomic builtins](https://gcc.gnu.org/onlinedocs/gcc/_005f_005fsync-Builtins.html).

The assembly backend has its own instruction and compiler-boundary
contract; for example, [x86/atomic.s](../../src/x86/atomic.s) uses
locked `xadd` and memory `xchg`. Mutex fallback supplies ordering
through participating lock/unlock operations. These implementations
must be evaluated separately rather than assigned one undocumented
universal barrier guarantee.

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

The [locale tour](locale.md) describes the ownership graph around
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

The [locale MT record](working/locale-mt-race.md) identified
concurrent lazy initialization of `numpunct` members: accessors in
[_numpunct.h](../../include/loc/_numpunct.h) tested `_C_flags`,
obtained a value, assigned the cache and updated the flag. Two threads
could both enter; for cached strings, both assigned the same string
object, and the use-after-free and destroyed-mutex reports followed.
The accessors now call the virtuals every time; the members stay,
unused, until the next minor version.

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

Several mechanisms use a counter to choose a first caller, then ask
that counter to stand for the readiness of unrelated state.

| site | source protocol and audit boundary |
|---|---|
| Global locale, `__rw_locale::_C_manage` | Atomic increment elects the initializer; a pointer and a volatile completion counter are then updated ordinarily. Other callers inspect the pointer or spin on the counter before the later global-locale guard. That later guard does not cover these earlier accesses. |
| Fallback `__rw_once`, [once.cpp](../../src/once.cpp) | Atomic increment elects a caller, ordinary volatile `init = 1000` announces completion, and waiters poll. The separate mutex fallback also has an ordinary unlocked first check. Neither pattern gains publication merely from its initial increment. |
| Fallback static mutex construction, [_mutex.h](../../include/rw/_mutex.h) | Placement construction follows a counter election; an ordinary volatile update signals completion. With no usable integer atomic operation, even election uses ordinary increment. This branch is conditional on missing static mutex initialization. |
| `ios_base::Init`, [iostream.cpp](../../src/iostream.cpp) | First increment initializes streams; later increments return immediately. Counting initializer objects does not itself wait for stream initialization. Establish startup reachability and ordering before claiming concurrent construction is covered. |
| `__rw_facet_id::_C_init`, [facet.cpp](../../src/facet.cpp) | Repaired: threads racing the first use of a facet type each generated an id and the last store won. The id is now generated once, under a static lock with a recheck, stored with release and read with acquire; `22.locale.id.mt` counts the ids 500 types take. |

The normal POSIX `__rw_once` route delegates to `pthread_once`, as
selected in [once.h](../../src/once.h). Do not attribute fallback
behavior to a configuration that uses the native primitive. Equally,
that native route does not repair independent hand-written
initializers such as the global-locale counter.

For the fallback protocols, audit exception recovery and waiter
progress as well as ordering. The static-mutex bootstrap cannot be
fixed by recursively asking the same uninitialized mutex machinery
to protect itself.

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
4. **Give initialization a completion protocol.** Prefer a supported
   native once mechanism or another fully specified mechanism. Cover
   readers, exceptions and bootstrap dependencies. Removing `volatile`
   or strengthening the election instruction alone is insufficient.
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
