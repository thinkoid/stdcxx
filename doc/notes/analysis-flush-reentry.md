# `flush` as a protocol primitive: how the tie and `unitbuf` re-enter the sentry

As of 2026-11-09 (3a930842). Every quotation was checked against
ISO/IEC 14882:2003 and N3242; every claim about the code before the
change against the tree at the commit above, and chapter 8 against
the change, built as 15D with GCC 16.2.1.

Output streams have two synchronization points of their own: the tie
before an operation and `unitbuf` after one. Both need a buffer
operation, "write what this buffer holds", and both were specified as
a stream operation, `flush`. While `flush` was a null check and a
`pubsync`, the difference did not matter. Once `flush` became an
operation with a sentry, every internal use of it re-entered the
protocol that called it. The standard has patched the consequences
three times and the library once; this note follows the patches, shows
what each one left, and states the rule none of them states.

## 0. tl;dr

- **The defect.** The protocol calls itself through its own public
  surface. 27.6.2.3, p2 has the sentry call `os.tie()->flush()` and
  27.6.2.3, p4 has its destructor call `os.flush()` under `unitbuf`.
  With a sentry inside `flush`, the first is a cycle through two tied
  streams and the second a cycle through one.
- **The patches.** LWG 60 stripped `flush` of its sentry. LWG 581 put
  it back and had the sentry destructor call `pubsync` directly
  (N3242 27.7.3.4, p4). Footnote 292 lets the tie flush not happen
  "if the function can determine that no synchronization is
  necessary". The library adopted 581 in January 2008 (STDCXX-626,
  ce319daa), met the tie cycle, and in March 2008 made `_C_opfx`
  detect a cycle in the tie chain and skip the flush (STDCXX-804,
  222c045b).
- **What the skip cost.** Two streams on distinct buffers tied to
  each other synchronized neither, measured in chapter 6. The
  standard footnote covers a shared buffer, where the sync would be
  redundant; it does not cover distinct buffers, where it is owed. By
  comparison libstdc++ has no guard and recurses until the stack is
  gone.
- **The rule.** Synchronization the protocol does on its own behalf
  is a buffer operation, never a stream operation. N3242 applied it to
  `unitbuf`. The tie is the one internal synchronization point that
  still goes through `flush` in the standard.
- **The resolution** (chapter 8) applies the rule to the tie: the
  sentry walks the chain of ties once, the farthest stream first, and
  calls `pubsync` on each stream's buffer under the gate that
  stream's own `flush` applies. Only the caller's buffer is skipped.
  The sentry takes its own buffer's mutex after the walk.

## 1. Terms

- **tie.** `basic_ios::tie ()` is a pointer to an output stream that
  is flushed before every operation on this stream. `cin` is tied to
  `cout` so a prompt shows before a read. Any stream may be tied to
  any stream; nothing in the standard restricts the shape of the
  chain.
- **`unitbuf`.** A format flag: the stream flushes after every output
  operation.
- **sentry.** The object every formatted and unformatted operation
  constructs first and destroys last. Its constructor is the prefix:
  flush the tie, check the state. Its destructor is the suffix: the
  `unitbuf` flush. In this library it is also the lock: `sentry`
  derives from `__rw::__rw_guard` and takes the buffer's mutex in its
  constructor, after the prefix (`include/ostream` 75-84).
- **the gate.** This note's name for what the sentry does for every
  operation: refuse to touch the buffer when the stream is not
  `good ()`, turn an exception from the buffer into `badbit` and
  rethrow it only when the stream's exception mask asks, and hold the
  lock. 27.6.2.6, p1 states the first two.
- **`flush`.** `basic_ostream::flush ()`, a public operation.
- **`pubsync` and `sync`.** `basic_streambuf::pubsync ()` calls the
  virtual `sync ()`, which a derived buffer overrides to write what it
  holds. This is the buffer operation.

## 2. The text of 14882:2003

27.6.2.3, p2, the sentry constructor:

> If os.good() is nonzero, prepares for formatted or unformatted
> output. If os.tie() is not a null pointer, calls os.tie()->flush().

Footnote 292, on that sentence:

> The call os.tie()->flush() does not necessarily occur if the
> function can determine that no synchronization is necessary.

27.6.2.3, p4, the sentry destructor:

> If ((os.flags() & ios_base::unitbuf) && !uncaught_exception()) is
> true, calls os.flush().

27.6.2.6, p1, the protocol every unformatted output function follows:

> Each unformatted output function begins execution by constructing
> an object of class sentry. If this object returns true, while
> converting to a value of type bool, the function endeavors to
> generate the requested output. If an exception is thrown during
> output, then ios::badbit is turned on in *this's error state. If
> (exceptions() & badbit) != 0 then the exception is rethrown. In any
> case, the unformatted output function ends by destroying the sentry
> object, then, if no exception was thrown, returning the value
> specified for the unformatted output function.

27.6.2.6, p7, `flush`, as LWG 60 left it:

> If rdbuf() is not a null pointer, calls rdbuf()->pubsync(). If that
> function returns -1 calls setstate(badbit) (which may throw
> ios_base::failure (27.4.4.3)). Does not behave as an unformatted
> output function (as described in 27.6.2.6, paragraph 1).

The clause is written as code: which function calls which, in what
order, under what condition. 17.3.1.2, p6 says how to read that:

> In some cases the semantic requirements are presented as C++ code.
> Such code is intended as a specification of equivalence of a
> construct to another construct, not necessarily as the way the
> construct must be implemented.

The permission reaches as far as the as-if rule of 1.9 does, to
whatever a program cannot observe. In iostreams a program observes a
great deal: `sync` is virtual, so which buffer is synchronized, how
many times and in what order is visible to any program that derives
from `streambuf`. For the virtual seams the code is the mandate. For
the rest it is one implementation written down.

## 3. The defect

The protocol has two synchronization points of its own, and both are
specified through `flush`:

- before an operation, the tie: `os.tie()->flush()` (27.6.2.3, p2);
- after an operation, `unitbuf`: `os.flush()` (27.6.2.3, p4).

Both want the buffer written. Neither wants the stream operation
around it: the state check of a stream whose state the caller already
checked, a second tie flush, a second lock. The text reached for
`flush` because the implementation it was written from had `flush`
and no separate primitive, and in that implementation `flush` was a
null check and a `pubsync`, so nothing distinguished the two.

The sentry changed that. Once `flush` is an operation with a sentry
inside, each internal call to `flush` re-enters the protocol:

- `unitbuf`: `flush` constructs a sentry; the sentry's destructor
  sees `unitbuf` and calls `flush`; which constructs a sentry. One
  stream, no end.
- the tie: an operation on `a` constructs a sentry; it calls
  `b.flush ()`; `flush` constructs a sentry on `b`; it calls
  `a.flush ()`; which constructs a sentry on `a`. Two streams tied to
  each other, no end.

Both cycles close through the sentry. Both open at the same place: an
internal synchronization point that calls the public operation
instead of the buffer.

## 4. The patches

### 4.1 LWG 60: strip `flush`

The 2003 text above is the resolution of LWG 60. It broke the
`unitbuf` cycle at `flush`: with no sentry in `flush`, the destructor
may call it. The cost is in the last sentence of 27.6.2.6, p7:
`flush` "does not behave as an unformatted output function". It is
the one output operation outside the gate.

### 4.2 LWG 581: restore `flush`, move the destructor

LWG 581 objected to that cost. Its text:

> The resolution of issue 60 changed basic_ostream::flush() so as not
> to require it to behave as an unformatted output function. That has
> at least two in my opinion problematic consequences:
>
> First, flush() now calls rdbuf()->pubsync() unconditionally, without
> regard to the state of the stream. I can't think of any reason why
> flush() should behave differently from the vast majority of stream
> functions in this respect.
>
> Second, flush() is not required to catch exceptions from pubsync()
> or set badbit in response to such events. That doesn't seem right
> either, as most other stream functions do so.

The two consequences are the first two duties of the gate, seen by a
user:

- **State.** The users' model is that a failed stream does nothing
  until it is cleared; that model is what makes `while (cin >> x)`
  an idiom. A `flush` outside the gate goes back to a buffer whose
  last write failed and tries again: the full disk, the broken pipe,
  a derived buffer mid-failure. The sentry already forbids this for
  every inserter and for `put` and `write`.
- **Exceptions.** 27.6.2.6, p1 is the whole `exceptions ()` design:
  the library swallows what the buffer throws, the user opts in to
  seeing it. A `flush` outside the gate lets a throwing `sync` out of
  `cout << endl` in a program that never asked for exceptions. The
  -1 path sets `badbit`; the throwing path was left out.

The third duty, the lock, the issue does not mention; a library whose
sentry locks feels it as a second copy of the protocol to keep in
step with the first.

The resolution put `flush` inside the gate and broke the `unitbuf`
cycle at the other end. N3242 27.7.3.7, p7:

> Effects: Behaves as an unformatted output function (as described in
> 27.7.3.6.1, paragraph 1). If rdbuf() is not a null pointer,
> constructs a sentry object. If this object returns true when
> converted to a value of type bool the function calls
> rdbuf()->pubsync(). If that function returns -1 calls
> setstate(badbit) (which may throw ios_base::failure (27.5.5.4)).
> Otherwise, if the sentry object returns false, does nothing.

N3242 27.7.3.4, p4, the sentry destructor:

> If ((os.flags() & ios_base::unitbuf) && !uncaught_exception() &&
> os.good()) is true, calls os.rdbuf()->pubsync(). If that function
> returns -1, sets badbit in os.rdstate() without propagating an
> exception.

The destructor now calls the buffer, not the stream, and carries its
own gate: `os.good()` and "without propagating an exception". That is
the rule of this note, applied once. The tie sentence, N3242
27.7.3.4, p2, still reads "calls os.tie()->flush()".

### 4.3 The library adopts 581 and meets the tie cycle

The library implemented the proposed resolution of 581 in January
2008, before C++11 adopted it (STDCXX-626; ce319daa, whose ChangeLog
entry reads "Implemented proposed resolution of LWG issue 581 and
reverted the resolution of issue 60 wrt ostream::flush()").
`flush` in `include/ostream.cc` is that code: it constructs a sentry
and, when the sentry is true, calls `pubsync` inside a `try`, sets
`badbit` on -1, and on an exception sets `badbit` with the flag that
rethrows when the mask asks. The comment at line 39 points at the
issue. Since chapter 8 the body after the sentry is
`_C_sync_rdbuf`, which the tie's walk calls too.

Until then `_C_opfx`, the sentry constructor's body, read:

```cpp
    // flush tied stream only if its buffer is distinct from that
    // of *this for greater efficiency and to to prevent a deadlock
    if (__tie && this->good () && __tie->rdbuf () != this->rdbuf ())
        __tie->flush ();
```

One skip, for a shared buffer, inside footnote 292. With a sentry in
`flush` the tie cycle of chapter 3 opened. Two months later the
library closed it (STDCXX-804; 222c045b, ChangeLog entry of
2008-03-31: "Detect and avoid flushing streams that are tied together
or that share the same buffer to prevent deadlock or infinite
recursion"). The regression test it added,
`tests/regress/27.basic.ios.tie.stdcxx-804.cpp`, ties up to 32
`ostringstream`s in a ring, inserts into each and flushes each, and
asserts that every stream is still `good ()`. It checks termination.
It does not check that any buffer other than the stream's own was
synchronized, so the skip it pins is invisible to it.

## 5. The code before the change

`include/ostream` 279-306 at 3a930842, `_C_opfx`:

```cpp
template<class _CharT, class _Traits>
inline basic_ostream<_CharT, _Traits>&
basic_ostream<_CharT, _Traits>::_C_opfx ()
{
    basic_ostream* __tie = this->tie ();

    if (__tie && this->good ()) {

        // detect and avoid flushing streams that are tied together
        // or that share the same buffer to prevent deadlock or
        // infinite recursion
        const basic_ostream *__p0, *__p1;

        // traverse the list of tied streams looking for a cycle
        for (__p0 = this, __p1 = this->tie (); __p0 && __p1;
             __p0 = __p0->tie (),
                 __p1 = __p1->tie () ? __p1->tie ()->tie () : 0)
            if (__p0 == __p1 || __p0->rdbuf () == __p1->rdbuf ()) {
                __tie = 0;
                break;
            }

        if (__tie)
            __tie->flush ();
    }

    if (_C_is_sync ())
        __rw::__rw_fflush (static_cast< ios_base* >(this), 0);

    return *this;
}
```

The walk was a tortoise and a hare over the tie chain. `__p0`
advanced one stream per step and `__p1` two. On a chain that ended in
a null the hare fell off and the loop exited with `__tie` intact. On a
chain that looped the two met, or two streams on one buffer were
compared, and `__tie` was zeroed. The flush was then either the whole
chain, through `__tie->flush ()` and its own sentry and its own tie,
or nothing. The sentry's guard took this stream's buffer mutex before
`_C_opfx` ran, so the whole prefix ran under it.

The destructor, then at `include/ostream` 83-93 and unchanged by
chapter 8, already followed the N3242 form: `pubsync` directly,
`badbit` without an exception on -1.

## 6. The probe

Two `ostream`s on two counting buffers, tied to each other; two on one
buffer, tied to each other; and a chain of two without a loop.
`sync` counts its calls and exits if it ever nests fifty deep.

```cpp
#include <iostream>
#include <streambuf>
#include <cstdio>
#include <cstdlib>

struct CountBuf : std::streambuf {
    const char* name; int syncs; int depth;
    CountBuf (const char* n) : name (n), syncs (0), depth (0) { }
    int sync () {
        ++syncs;
        if (++depth > 50) { std::printf ("%s: recursion\n", name); std::exit (3); }
        --depth;
        return 0;
    }
    int_type overflow (int_type c) { return c; }
};

int main ()
{
    CountBuf ba ("a"), bb ("b");
    std::ostream a (&ba), b (&bb);
    a.tie (&b);
    b.tie (&a);
    a << 'x';
    std::printf ("after a << x: a.syncs %d b.syncs %d\n", ba.syncs, bb.syncs);
    b << 'y';
    std::printf ("after b << y: a.syncs %d b.syncs %d\n", ba.syncs, bb.syncs);

    std::ostream c (&ba), d (&ba);
    c.tie (&d); d.tie (&c);
    ba.syncs = 0;
    c << 'z';
    std::printf ("shared buffer, c << z: syncs %d\n", ba.syncs);

    CountBuf be ("e"); std::ostream e (&be);
    std::ostream f (&bb); f.tie (&e);
    f << 'w';
    std::printf ("f -> e, f << w: e.syncs %d\n", be.syncs);
    return 0;
}
```

Against the 15D build, linked the tree's way (`gcc`, `-lstd15D
-lsupc++`), and against libstdc++ 6.0.36 with `g++`:

| case | owed by 27.6.2.3, p2 | this library at 3a930842 | libstdc++ |
|---|---|---|---|
| `a << 'x'`, `a` and `b` tied to each other, distinct buffers | `b` synchronized | `a.syncs 0 b.syncs 0` | SIGSEGV |
| `c << 'z'`, `c` and `d` tied to each other, one buffer | nothing, footnote 292 | `syncs 0` | SIGSEGV |
| `f << 'w'`, `f` tied to `e`, `e` untied | `e` synchronized | `e.syncs 1` | `e.syncs 1` |

libstdc++ implements 581 with no guard. The recursion is sentry,
`flush`, sentry, and never reaches `sync`, so the probe's depth check
never fires and the process dies on the stack. The library at
3a930842 survived every ring, as STDCXX-804 intended, and synchronized
nothing in one. The owed sync on the first row was the departure;
chapter 8 has the probe against the change.

## 7. A second defect: the tie has no invariant

`tie` is a pointer with no rule about where it may point. The text was
written for a tree: input streams pointing at the stream that shows
their prompt, one edge from `cin` to `cout`. It permits a graph. A
ring is representable, an implementation has to survive one, and no
paragraph says what a ring means. Footnote 292 is where the committee
noticed, and it says only that the flush may be omitted, not what a
program may expect instead. STDCXX-804 is the same notice in code.

Nothing cures this but a sentence in the standard. An implementation
does the next thing: it defines the behavior of a ring, and the only
definition that satisfies p2 for every member is that every stream in
the ring is synchronized once.

## 8. The resolution

The rule, stated once: synchronization the protocol performs on its
own behalf is a buffer operation. N3242 applied it to `unitbuf`; this
change applies it to the tie. The sentry no longer calls `flush` on
the tied stream. `basic_ostream::_C_sync_chain` walks the chain of
ties once and synchronizes each stream's buffer itself
(`include/ostream.cc` 51-129):

```cpp
template<class _CharT, class _Traits>
void
basic_ostream<_CharT, _Traits>::
_C_sync_chain (const basic_streambuf<char_type, traits_type> *__buf)
{
    // count the streams on the chain, each once: the hare, two steps
    // to the tortoise's one, falls off a chain that ends in a null
    // and meets the tortoise on a ring
    const basic_ostream *__p0 = this;
    const basic_ostream *__p1 = this;

    bool __ring = false;

    while (__p1->tie () && __p1->tie ()->tie ()) {
        __p0 = __p0->tie ();
        __p1 = __p1->tie ()->tie ();

        if (__p0 == __p1) {
            __ring = true;
            break;
        }
    }

    _RWSTD_SIZE_T __n = 1;

    if (__ring) {
        // the tortoise restarted at *this and the hare, both one step
        // at a time, meet at the first stream of the ring
        for (__p0 = this; __p0 != __p1; ++__n) {
            __p0 = __p0->tie ();
            __p1 = __p1->tie ();
        }

        // one lap from there is the length of the ring; __n counts
        // the streams before it plus one, and the lap the rest
        for (__p1 = __p0->tie (); __p1 != __p0; ++__n)
            __p1 = __p1->tie ();
    }
    else {
        for (__p0 = this; __p0->tie (); ++__n)
            __p0 = __p0->tie ();
    }

    // the chain ends before the first stream that is not good (),
    // whose own sentry would flush neither its buffer nor its tie
    // (27.6.2.3, p2)
    _RWSTD_SIZE_T __ngood = 0;

    for (__p0 = this; __ngood < __n && __p0->good (); ++__ngood)
        __p0 = __p0->tie ();

    if (__ngood)
        _C_sync_tail (__ngood, __buf);
}


template<class _CharT, class _Traits>
void
basic_ostream<_CharT, _Traits>::
_C_sync_tail (_RWSTD_SIZE_T                                   __n,
              const basic_streambuf<char_type, traits_type> *__buf)
{
    // the farthest stream first, as its sentry is the innermost of
    // the nested flushes of 27.6.2.3, p2
    if (1 < __n)
        this->tie ()->_C_sync_tail (__n - 1, __buf);

    // what flush () does inside its sentry, under the same gate
    if (this->rdbuf () && this->rdbuf () != __buf) {
        _RWSTD_MT_GUARD (this->_C_bufmutex ());

        if (this->good ()) {
            if (_C_is_sync ())
                __rw::__rw_fflush (static_cast< ios_base* >(this), 0);

            _C_sync_rdbuf ();
        }
    }
}
```

The walk has six parts:

- **The count.** The walk counts the distinct streams reachable from
  the tie, the tie included. The tortoise and hare find a ring, as
  the 2008 code did. The tortoise restarted at the tie and the hare,
  both advanced one stream at a time, then meet at the ring's first
  stream, and one lap from there is the ring's length. Two streams
  tied to each other count 2, a stream tied to itself 1, and `a -> b
  -> c -> b` counts 3 from `a`. The walk allocates nothing and marks
  no stream, so the streams it reads stay unchanged.
- **The order.** `_C_sync_tail` recurses to the count's depth and
  synchronizes on the way back, so the farthest stream goes first.
  The nested flushes of p2 did the same, since the farthest stream's
  sentry was the innermost. They recursed to the same depth, so the
  walk adds no limit.
- **The cut.** The walk ends before the first stream that is not
  `good ()`, and nothing beyond it is synchronized. 27.6.2.3, p2 makes
  the tie flush part of the preparation that happens "If os.good() is
  nonzero", so that stream's own sentry would have flushed neither its
  buffer nor its tie: the cut is the letter of p2. A stream without a
  buffer is not `good ()`, so the chain ends there too. Footnote 292
  licenses fewer synchronizations, never more.
- **The gate.** For each stream the walk does what that stream's
  `flush` does inside its sentry, and nothing else. It skips a stream
  whose buffer is the caller's, takes the buffer's mutex, and checks
  `good ()` again under it, since the state of a locked stream can
  change under the walk. It then calls `__rw_fflush` for a standard
  stream synchronized with stdio, as `_C_opfx` does, and calls
  `_C_sync_rdbuf` (132-147): `pubsync` in a `try`, `badbit` on -1,
  and on an exception `badbit` with the flag that rethrows when the
  stream's mask asks. `_C_sync_rdbuf` is the body `flush` had after
  its sentry, and `flush` calls it there, so the protocol has one
  copy. A failed stream is thus left alone, and a throwing buffer
  reaches the program only when its stream asks: the two defects 581
  was filed over stay fixed. The destructor of the tie's own sentry
  has no counterpart: it called `pubsync` a second time on a standard
  stream synchronized with stdio, so `cin`'s tie to `cout` now
  synchronizes `cout`'s buffer once where it did twice.
- **The caller's buffer.** A good stream whose buffer is the caller's
  is stepped over: the walk goes on past it, but its buffer is not
  synchronized. The operation about to run writes that buffer, so no
  synchronization is necessary and footnote 292 lets the call not
  occur. It is the case the pre-2008 code skipped.
- **A shared buffer.** Any other buffer that two streams of the chain
  share is synchronized once per good stream that holds it. Each
  nested flush of p2 calls `pubsync`, so that is the letter of p2,
  and the walk does not deduplicate.

The lock moved. The sentry's guard took this stream's buffer mutex
before the prefix ran; it now takes it after. `_C_opfx` walks the
chain from the tie, flushes stdio for a synchronized standard stream,
and returns the mutex, which the guard then takes
(`include/ostream` 75-84). The walk takes one buffer's mutex at a
time, each after the recursion beneath it has returned, and the
sentry holds none of its own across the walk. Two threads inserting
into two standard streams tied to each other, `cout` and `cerr`, thus
cannot deadlock. With the caller's mutex held across the walk they
would: each thread would hold its own buffer's mutex and wait for the
other's. Nothing in `_C_opfx` needs this stream's buffer mutex, since
it reads this stream's state and touches other buffers only under
their own mutexes.

The input sentry follows the same order. 27.6.1.1.2, p2:

> If is.good() is true, prepares for formatted or unformatted input.
> First, if is.tie() is not a null pointer, the function calls
> is.tie()->flush() to synchronize the output sequence with any
> associated external C stream.

`basic_istream::_C_sync_tie` (`include/istream.cc` 72-86) calls
`tie ()->_C_sync_chain (rdbuf ())` when the stream is `good ()` and
has a tie, inside a `try` whose handler sets `badbit` with the
rethrow flag, as the `try` of `_C_ipfx` did around the whole prefix.
It returns the mutex the sentry's guard takes. `_C_ipfx` no longer
synchronizes the tie; its state check and its whitespace skip run
under the lock as before. Two consequences show:

- `ws`, which calls `_C_ipfx` again after its sentry, synchronizes
  the tie once where it did twice.
- A tie whose exception reaches the input stream, because the tie's
  mask asks and the input stream's does not, leaves the input stream
  with `badbit`, and `_C_ipfx` then adds `failbit` for a stream not
  `good ()`. 27.6.1.1.2, p5 permits that: "During preparation, the
  constructor may call setstate(failbit)".

The probe of chapter 6, against the change:

| case | this library |
|---|---|
| `a << 'x'`, `a` and `b` tied to each other, distinct buffers | `a.syncs 0 b.syncs 1` |
| `b << 'y'`, after the row above | `a.syncs 1 b.syncs 1` |
| `c << 'z'`, `c` and `d` tied to each other, one buffer | `syncs 0` |
| `f << 'w'`, `f` tied to `e`, `e` untied | `e.syncs 1` |

Two threads inserting ten thousand characters each into `cout` and
`cerr`, tied to each other, finish in 0.025 s. `27.ostream`'s
`test_sentry` pins the walk: two streams tied to each other on
distinct buffers and on one, a chain of three and its order, a chain
that enters a ring, a chain cut at a failed stream, a tied buffer whose
`sync` returns -1, and one that throws under the tied stream's
`exceptions (badbit)`. `27.istream.sentry` pins an input stream tied
to a ring of two. The 2008 regression test keeps passing, since it
asks only that every stream in the ring stays `good ()`.

## 9. The lesson

Code-shaped mandates compose badly. "Calls os.tie()->flush()" was
correct about the flush that existed when it was written, and nobody
re-read it when 581 changed what `flush` does. A sentence about the
effect, "the tied stream's pending output is written before the
operation", would have survived 581 untouched. Each patch since,
issue 60, issue 581's destructor, footnote 292, STDCXX-804, moves one
internal use off `flush` or excuses it, and none names the rule,
because the text has no place for a rule that is not a call.

The reading that falls out, and the one this tree applies: cite the
paragraph, then ask what a program can observe. If the difference
shows through a virtual or a public query (`gcount`, `width`,
`rdstate`, a buffer's `sync`), the call sequence is the mandate. If
nothing shows, the code is illustrative and 17.3.1.2, p6 says so. The
owed `sync` of chapter 6 is the first kind. The tortoise and hare, and
the nested sentries they stand in for, are the second.
