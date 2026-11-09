# `flush` as a protocol primitive: how the tie and `unitbuf` re-enter the sentry

As of 2026-11-09 (3a930842). Every quotation was checked against
ISO/IEC 14882:2003 and N3242; every claim about the code against the
tree at the commit above or at the commit named beside it, built as
15D with GCC 16.2.1.

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
  it back, and with it opened both cycles. LWG 835 closed them for
  C++11: the sentry destructor calls `pubsync` directly (N3242
  27.7.3.4, p4), and a ring of ties is a precondition violation of
  `tie` (N3242 27.5.5.3, p2). Footnote 292 lets the tie flush not happen
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
  is a buffer operation, never a stream operation. LWG 835 applied it
  to `unitbuf`, and for the tie chose a precondition instead. The tie
  is the one internal synchronization point that still goes through
  `flush` in the standard.
- **The resolution** (chapter 8) is LWG 835's: a ring of ties is a
  precondition violation, so the detector of 2008 stays and a ring is
  left alone, while a chain is flushed as p2 says. A walk that
  synchronized every stream of a ring once was built, measured and
  withdrawn the same day (a9dc953a); chapter 8 keeps it as the worked
  alternative.

## 1. Terms

- **tie.** `basic_ios::tie ()` is a pointer to an output stream that
  is flushed before every operation on this stream. `cin` is tied to
  `cout` so a prompt shows before a read. Any stream may be tied to
  any stream; nothing in 14882:2003 restricts the shape of the chain,
  and C++11 forbids a ring (chapter 4.3).
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

### 4.2 LWG 581: restore `flush`

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

The resolution put `flush` inside the gate. N3242 27.7.3.7, p7:

> Effects: Behaves as an unformatted output function (as described in
> 27.7.3.6.1, paragraph 1). If rdbuf() is not a null pointer,
> constructs a sentry object. If this object returns true when
> converted to a value of type bool the function calls
> rdbuf()->pubsync(). If that function returns -1 calls
> setstate(badbit) (which may throw ios_base::failure (27.5.5.4)).
> Otherwise, if the sentry object returns false, does nothing.

With a sentry in `flush`, both cycles of chapter 3 were open: the
destructor of 2003 still called `os.flush()` under `unitbuf`, and the
tie sentence still called `os.tie()->flush()`.

### 4.3 LWG 835: close one cycle, forbid the other

LWG 835, "Tying two streams together (correction to DR 581)", opened
2008-05-17, names both. Its text observes that two streams tied to
each other, directly or through a third, now flush each other without
end, with `cout` and `cerr` tied to each other as the example, and
that a stream with `unitbuf`, `cerr`, recurses on `cerr << flush`
because the sentry destructor calls `flush` again. The wording took
until 2010 and went into C++11 in two parts.

The destructor, N3242 27.7.3.4, p4:

> If ((os.flags() & ios_base::unitbuf) && !uncaught_exception() &&
> os.good()) is true, calls os.rdbuf()->pubsync(). If that function
> returns -1, sets badbit in os.rdstate() without propagating an
> exception.

It calls the buffer, not the stream, and carries its own gate:
`os.good()` and "without propagating an exception". That is the rule
of this note, applied once: the `unitbuf` cycle is closed at the
destructor.

The tie, N3242 27.5.5.3, p2, a new precondition:

> Requires: If tiestr is not null, tiestr must not be reachable by
> traversing the linked list of tied stream objects starting from
> tiestr->tie().

The tie cycle is not closed; it is forbidden. The tie sentence, N3242
27.7.3.4, p2, still reads "calls os.tie()->flush()", and a program
that builds a ring has broken a precondition, so its behavior is
undefined and no implementation has to detect it. For the tie the
committee chose the invariant over the primitive. 14882:2003, which
this library targets, has neither the precondition nor the
destructor's form: there a ring is well-formed and the text recurses.

### 4.4 The library adopts 581 and meets the tie cycle

The library implemented the proposed resolution of 581 in January
2008, before C++11 adopted it (STDCXX-626; ce319daa, whose ChangeLog
entry reads "Implemented proposed resolution of LWG issue 581 and
reverted the resolution of issue 60 wrt ostream::flush()").
`flush` in `include/ostream.cc` is that code: it constructs a sentry
and, when the sentry is true, calls `pubsync` inside a `try`, sets
`badbit` on -1, and on an exception sets `badbit` with the flag that
rethrows when the mask asks. The comment at line 39 points at the
issue.

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
recursion"). LWG 835 (4.3) followed seven weeks later. The regression
test the library added,
`tests/regress/27.basic.ios.tie.stdcxx-804.cpp`, ties up to 32
`ostringstream`s in a ring, inserts into each and flushes each, and
asserts that every stream is still `good ()`. It checks termination.
It does not check that any buffer other than the stream's own was
synchronized, so the skip it pins is invisible to it.

## 5. The code

`include/ostream` 279-306, `_C_opfx`:

```cpp
template<class _CharT, class _Traits>
inline basic_ostream<_CharT, _Traits>&
basic_ostream<_CharT, _Traits>::_C_opfx ()
{
    basic_ostream* __tie = this->tie ();

    if (__tie && this->good ()) {
        // 27.6.2.3, p2: flushes the tie through its own sentry, which
        // flushes its tie in turn; on a ring of ties, which N3242
        // 27.5.5.3, p2 forbids, or on two streams of the chain sharing
        // a buffer, the tie is not flushed (footnote 292; departure)
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
nothing. The sentry's guard takes this stream's buffer mutex before
`_C_opfx` runs, so the whole prefix runs under it. The code is
STDCXX-804's of 2008; the comment is new.

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
Under 2003's letter the first row is a departure; chapter 8 records
why it stands.

## 7. A second defect: the tie has no invariant

`tie` is a pointer with no rule about where it may point. The text was
written for a tree: input streams pointing at the stream that shows
their prompt, one edge from `cin` to `cout`. It permits a graph. In
14882:2003 a ring is representable, an implementation has to survive
one, and no paragraph says what a ring means. Footnote 292 is where
the committee first noticed, and it says only that the flush may be
omitted, not what a program may expect instead. STDCXX-804 is the
same notice in code.

The cure is a sentence in the standard, and C++11 has it: the
precondition of N3242 27.5.5.3, p2 (chapter 4.3) makes a ring the
program's fault and its behavior undefined. An implementation that
targets 14882:2003 has no such sentence, and does the next thing: it
defines the behavior of a ring. Two definitions are possible, every
stream in the ring synchronized once, which is what p2 would owe if
it could be followed, or none, which the committee's ruling makes
acceptable. Chapter 8 takes the second.

## 8. The resolution

The library takes the position of LWG 835 and keeps the detector of
STDCXX-804. A ring of ties is a precondition violation, N3242
27.5.5.3, p2, and the standard owes such a program nothing. On a
chain the sentry does what 27.6.2.3, p2 says: it calls `flush` on the
tied stream, whose sentry calls `flush` on its tie in turn, and the
chain ends at a null tie or at a stream that is not `good ()`, whose
sentry flushes nothing. On a ring the sentry does what the code of
chapter 5 has done since 2008: the tortoise and hare find the ring,
or two streams of the chain on one buffer, and the tie is not
flushed. A ring thus terminates, as `27.objects` has required since
2006 and the regression test of STDCXX-804 since 2008, and nothing in
it is synchronized.

Under 2003's letter that is the departure of chapter 6: 2003 has no
precondition, and footnote 292 does not reach a ring of distinct
buffers. Under C++11 it is the library's defined handling of a case
the standard leaves undefined, and the precondition, adopted from
C++11, is the departure from 2003, in the direction the standard
took. The comment on `_C_opfx` cites both paragraphs.

Two other resolutions were weighed, and one of them was in the tree
for a day.

- **The walk** (a9dc953a) counted the streams reachable from the tie,
  cut the chain at the first stream not `good ()`, and synchronized
  each stream's buffer once, the farthest first, under its own lock,
  through `pubsync` and the body `flush` has after its sentry; both
  sentries took their buffer mutex after the prefix, so two threads
  inserting into a ring could not deadlock. Every ring terminated and
  every member was synchronized once. It was the letter of p2 for
  2003 and a definition of an undefined case for C++11, at the cost
  of some forty-five lines, two members on `basic_ostream` and one on
  `basic_istream`, and a lock order changed after twenty years. It
  was withdrawn for the reason 835 gives: a ring is representable in
  the type, and the committee ruled it meaningless; code that defines
  its behavior answers a question the program is not allowed to ask,
  on a path every insertion runs.
- **835's letter with no detector**, the form the sentry had from the
  2005 import until 2008, was weighed and not taken. `cout` and
  `cerr` tied to each other, which `27.objects` has exercised since
  2006, would deadlock on the two buffer mutexes, a hang where
  libstdc++ crashes, and the twelve lines of the detector buy
  termination for a configuration the tree has kept working for
  twenty years.

The rule of this note stands as a reading of the text: the committee
applied it to `unitbuf`, declined to apply it to the tie, and chose
the invariant instead. The library follows the committee on the ring
and keeps its own guard against the program that breaks the
invariant.

`27.ostream`'s `test_sentry` pins it: two streams on distinct buffers
tied to each other synchronize neither, as do two on one buffer and a
chain that enters a ring; a chain of three synchronizes the farthest
stream first; a chain ends at a failed stream; a tied buffer whose
`sync` returns -1 sets the tied stream's `badbit` and leaves the
sentry true; one that throws under the tied stream's
`exceptions (badbit)` escapes the sentry's construction.
`27.istream.sentry` pins an input stream tied into a ring of two: the
tie is flushed, and its sentry finds the ring and leaves the other
stream alone. The probe of chapter 6 stands as its table shows.

## 9. The lesson

Code-shaped mandates compose badly. "Calls os.tie()->flush()" was
correct about the flush that existed when it was written, and nobody
re-read it when 581 changed what `flush` does. A sentence about the
effect, "the tied stream's pending output is written before the
operation", would have survived 581 untouched. Each patch since,
issue 60, issue 835's destructor, footnote 292, STDCXX-804, moves one
internal use off `flush` or excuses it, and none names the rule,
because the text has no place for a rule that is not a call. For the
tie, issue 835 left the call as written and forbade the program the
shape that exposes it; this library detects the shape and leaves it
alone.

The reading that falls out, and the one this tree applies: cite the
paragraph, then ask what a program can observe. If the difference
shows through a virtual or a public query (`gcount`, `width`,
`rdstate`, a buffer's `sync`), the call sequence is the mandate. If
nothing shows, the code is illustrative and 17.3.1.2, p6 says so. The
owed `sync` of chapter 6 is the first kind. The tortoise and hare, and
the nested sentries they stand in for, are the second.
