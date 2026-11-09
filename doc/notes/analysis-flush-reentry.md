# `flush` as a protocol primitive: how the tie and `unitbuf` re-enter the sentry

As of 2026-11-09 (3a930842). Every quotation was checked against
ISO/IEC 14882:2003 and N3242; every claim about the code against the
tree at the commit above, built as 15D with GCC 16.2.1.

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
- **What the skip costs.** Two streams on distinct buffers tied to
  each other synchronize neither, measured in chapter 6. The
  standard footnote covers a shared buffer, where the sync would be
  redundant; it does not cover distinct buffers, where it is owed. By
  comparison libstdc++ has no guard and recurses until the stack is
  gone.
- **The rule.** Synchronization the protocol does on its own behalf
  is a buffer operation, never a stream operation. N3242 applied it to
  `unitbuf`. The tie is the one internal synchronization point that
  still goes through `flush`, in the standard and in this library.
- **The resolution** (chapter 8) applies the rule to the tie: walk the
  chain once, `pubsync` each distinct buffer under the gate the tied
  stream's own sentry would have applied. It is the open entry under
  Streams in `TODO`.

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
  constructor (`include/ostream` 75-80).
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
`include/ostream.cc` 35-55 is that code: `flush` constructs a sentry,
calls `pubsync` inside a `try`, sets `badbit` on -1, and on an
exception sets `badbit` with the flag that rethrows when the mask
asks. The comment at line 39 points at the issue.

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

## 5. The code today

`include/ostream` 279-306, `_C_opfx`:

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

The walk is a tortoise and a hare over the tie chain. `__p0` advances
one stream per step and `__p1` two. On a chain that ends in a null
the hare falls off and the loop exits with `__tie` intact. On a chain
that loops the two meet, or two streams on one buffer are compared,
and `__tie` is zeroed. The flush is then either the whole chain,
through `__tie->flush ()` and its own sentry and its own tie, or
nothing.

The destructor, `include/ostream` 83-93, already follows the N3242
form: `pubsync` directly, `badbit` without an exception on -1.

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

| case | owed by 27.6.2.3, p2 | this library | libstdc++ |
|---|---|---|---|
| `a << 'x'`, `a` and `b` tied to each other, distinct buffers | `b` synchronized | `a.syncs 0 b.syncs 0` | SIGSEGV |
| `c << 'z'`, `c` and `d` tied to each other, one buffer | nothing, footnote 292 | `syncs 0` | SIGSEGV |
| `f << 'w'`, `f` tied to `e`, `e` untied | `e` synchronized | `e.syncs 1` | `e.syncs 1` |

libstdc++ implements 581 with no guard. The recursion is sentry,
`flush`, sentry, and never reaches `sync`, so the probe's depth check
never fires and the process dies on the stack. This library survives
every ring, as STDCXX-804 intended, and synchronizes nothing in one.
The owed sync on the first row is the departure.

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
definition that satisfies p2 for every member is that every buffer in
the ring is synchronized once.

## 8. The resolution

The rule, stated once: synchronization the protocol performs on its
own behalf is a buffer operation. N3242 applied it to `unitbuf`; the
tie is the remaining case. Applied to `_C_opfx`:

- walk the chain from `tie ()` until a null or a stream already
  visited, so the walk terminates on a ring by construction;
- for each stream on the walk whose buffer has not been synchronized
  yet, do what that stream's own `flush` would do inside its sentry:
  skip it if it is not `good ()`, take its buffer's lock, call
  `pubsync`, set `badbit` on that stream on -1, and on an exception
  set `badbit` on that stream and rethrow only when its mask asks;
- synchronize a buffer once however many streams share it, which is
  the shared-buffer skip of footnote 292 kept.

The walk replaces the chain of nested sentries with one pass over the
same streams, and it synchronizes the same buffers the chain would
have, with the same per-stream gate. The observable difference from
`tie ()->flush ()` is the one footnote 292 licenses: a buffer reached
twice is synchronized once. The 2008 regression test keeps passing,
since it asks only that every stream in the ring stays `good ()`.

Two things the walk must not lose, because the chain had them:

- the per-stream gate, above. Dropping to a bare `pubsync` on each
  buffer would synchronize a failed stream and let a derived buffer's
  exception escape, the two defects 581 was filed over;
- the lock. The tied stream's sentry takes its buffer's mutex; the
  walk takes the same mutex around the same call.

The alternative is to leave the code and record the first row of the
table in chapter 2 of `ref-conformance.md` as a departure from
27.6.2.3, p2: a ring of distinct buffers is not synchronized. The
choice is the open entry under Streams in `TODO`.

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
