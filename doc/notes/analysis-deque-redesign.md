# The deque redesign: findings, model, protocol, verification

`deque` was reviewed at 7655ec93, the tip of the three-step storage
rework of 2026-10-16, and rewritten the next day on the review's
findings. This note is the record of both: what was wrong, the model
that replaced it and how that model was decided, the mutation protocol,
how the result was verified, and the costs and open items it leaves.
The reviewed tip carries the tag `freeze-2026`. The rewrite landed as
49cbe97f over the inert groom 9c282871; the review's findings were
queued in ddc0a0b3 and closed by the rewrite.

## 0. tl;dr

- **Six findings closed.** Inserting an element of the deque into its
  own middle wrote the wrong value; erasing everything returned the old
  beginning; `end() + 0` asserted at a buffer boundary; `1 + it` did not
  compile on a 64-bit target without the debug wrapper; a throwing
  source increment lost a constructed object; source exceptions during
  assignment left changed values. Each has a regression in the suite.
- **The model changed.** The node vector of 7655ec93 owned a buffer in
  every slot. The deque now keeps a pointer map with null slack around
  one contiguous span of owned buffers, the live range inside it. The
  map grows geometrically; buffers are allocated by need and never freed
  before destruction or swap. The 2026-10-16 decision for two ranges was
  given up because every increase in map capacity forced buffer
  allocations; chapter 3 says how each of that decision's objections is
  met.
- **One boundary representation.** An iterator at a buffer boundary
  names the following node, and a null element pointer if that node
  owns no buffer. Equality compares fields. The map's terminal null slot
  makes stepping off the last buffer ordinary.
- **Reservation before mutation.** An operation-local record reserves
  room, constructs the new elements, shifts the shorter side, and
  commits; on any failure its destructor destroys what it constructed
  and puts the storage back, old map included, so iterators held before
  a failing insert stay valid.
- **Verified** by the full GCC 15D suite (274 programs, 11912687
  assertions, none failed), a new storage test that fails in every
  section on the old headers, an independent randomized oracle with a
  `bad_alloc` at every allocation ordinal and a throw at every
  source-iterator operation, ASan and UBSan, and differential runs
  against another library.
- **Open:** every non-pointer source range is staged in a temporary,
  a second copy per element, to meet 23.2.1.3, p2 for throwing source
  iterators; that trade stands until decided otherwise. The ownership
  scan of the first rewrite is gone: the deque keeps the span's bounds
  (6.1).

## 1. The findings at 7655ec93

The review ran the reviewed headers, the headers before the storage
rework (d7a980b3) and another library's `deque` over the same probes.
Findings 1.1 to 1.4 reproduced on both revisions of this library;
1.5 is new in d425d47d.

### 1.1 An aliased scalar insert wrote the wrong value

`insert(begin() + 2, d[0])` on `[0..9]` produced `[0, 1, 1, 2, ...]`.
The prefix shift assigned over the slot the argument referred to before
the fill read it. The count overload shared the path. Now a middle
insert saves the value before any shift; an end insert needs no copy.
Pinned by the alias matrix of `23.deque.storage`: every size to 33,
every position, every alias, counts 1 to 4.

### 1.2 Erase-all returned the old beginning

`erase(begin(), end())` on 100 elements returned an iterator unequal to
`end()`: the result was the saved `first` after the last pop had
recentred the stored endpoints. Erase now saves an offset and rebuilds
the result after mutation. The old modifiers loop had discarded both
erase results; it checks them now.

### 1.3 Two representations of a boundary

After 16 default pushes `end() + 0` asserted. The deque stored a
boundary as the previous buffer's one-past pointer; ordinary increment
produced the following buffer's first pointer; equality subtracted
positions to reconcile them. See chapter 2 for the one representation
that replaced both.

### 1.4 Literal-left addition deduced two difference types

`1 + it` deduced the offset template parameter as `int` from the
literal and `long` from the iterator. The debug wrapper hid it. The
non-member `operator+` now takes the iterator's `difference_type`, as
`reverse_iterator` does.

### 1.5 Source exceptions escaped the accounting

The range-insert loop of d425d47d advanced the source before it advanced
the constructed endpoint. A source increment that threw between the two
left a constructed object outside the cleanup range; its destructor never
ran. Other source exceptions came after old values had been overwritten,
and restoring storage and size did not restore those values. 23.2.1.3,
p2 exempts only `T`'s copy constructor and assignment, so for a source
whose operations may throw, no effects means the source must be consumed
before any old element is touched. The rewrite records each construction
before the next source operation and stages arbitrary sources (chapter
4). Pinned by the source sweeps of `23.deque.storage` over input, forward
and bidirectional categories, each ordinal of copy, assignment,
comparison, dereference and increment, checking contents, endpoints and
live-object counts.

### 1.6 The exception loop skipped failures

`23.deque.modifiers` armed element failures at the current operation
count, which the operation incremented before testing, and armed several
sources at once. An insert with room succeeded on its first attempt
without injecting anything. The loop now sweeps allocation, copy and
assignment separately, each from ordinal one on a fresh container, and
asserts that the armed failure was reached. Its row grew from 201238 to
590558 assertions.

### 1.7 A dense queue rotated the whole map

With every slot owning a buffer and one buffer's worth of spare, a
fixed-size `pop_front`/`push_back` queue rotated one buffer every
bufferful of cycles and swapped nearly every map entry each time. This
is pointer work, not operations on `T`, so not a complexity violation;
it is what the sparse map removes.

## 2. The representation

The ordinary map slots have this shape:

    null slots | spare buffers | live buffers | spare buffers | null slots

The non-null slots form one contiguous owned span; the live range lies
within it; the other owned buffers are retained spares; boundary buffers
may be only partly live. One extra terminal slot is always null. The
deque stores six things: the two live iterators, the map pointer, the
ordinary slot count, and the span's first slot and length. The null and
non-null pattern encodes the same bounds; the debug check compares the
two (see 6.1).

The raw iterator keeps a node pointer and an element pointer. At a
boundary the node is the following one and the element pointer is that
node's buffer start, or null if the node owns no buffer. Offsets stay in
`[0, B)`. Equality compares both fields. Subtraction, zero arithmetic and
the empty deque never form a pointer from null. Copying is explicit;
a mutable iterator converts to a const one and not the reverse, in the
raw class and in the debug wrapper. An offset-only iterator, one
dependent load more per dereference, was built and measured slower on
traversal, so the cached element pointer stays.

`max_size()` is `(N - 1) * B + 1` for a map of at most `N` ordinary
slots, since the live range may start at offset `B - 1`; `N` is bounded
by the rebound allocator's `max_size()` less the sentinel and by
`difference_type`. Length checks run before any unsigned addition.

## 3. How the model was decided

On 2026-10-16 the queue decided two ranges (7929478f, 9a90f663,
f7ef45ae): every slot of the node vector owns a buffer, the vector's
extent is the storage, the live iterators mark the live part, the vector
regrows by the golden ratio, nothing is kept from a failed growth, and
pops never free (15e27446). That decision named the alternative and
declined it: a map with spare slots and blocks allocated by need, as
libc++ has, "a third range this design does not pay for". Its objections
were that a vector grown by need copies every node pointer every
bufferful, quadratic over n, and that a minimum of 32 slots held 4 KiB
for one `int`.

The rewrite session, after the review, agreed the third range. The
reason: with every slot owning a buffer, every increase in map capacity
forces buffer allocations, and a growth that cannot keep a partial
result (f7ef45ae) must allocate them all before installing any. Separating
map capacity from buffer ownership makes map growth a pointer-array
allocation and buffer growth one buffer at a time, each committed as it
comes. The objections are met rather than dropped: the map grows
geometrically, so the quadratic case does not arise; the by-need part is
the buffers, each serving a bufferful of pushes; a slot is a pointer, so
the 32-slot minimum of the growth hook costs bytes, not buffers, and
stays. Pops never free and the destructor frees every owned slot, as
decided. The growth hook `__rw_new_capacity<deque>` now takes and
returns a node count; in 7655ec93 its argument was an element count.

## 4. The mutation protocol

A reservation is a local object with one job: undo an uncommitted
insertion. It saves the old map, its size and both iterators, then
prepares room for `n` elements at one end:

1. If the partial live buffer at that end has the room, nothing else.
2. Otherwise scan outward from the live range to both ends of the owned
   span, across spares only.
3. Use the spares on the near side. Take what is still short from the
   far side, one pointer transfer each into the adjacent null slot.
4. If null slots on the near side run out: recentre the span when the
   null slack is at least the span and covers the request; otherwise
   allocate a larger map (geometric, bounded by the allocator and
   `difference_type`), copy the span into it already rotated, and keep
   the old map until commit.
5. Allocate the buffers still missing, one per slot.
6. Rebuild both iterators in canonical form.

Construction then goes into the reserved room, each successful
construction advancing the cleanup cursor at once. Commit moves the live
endpoint and frees a replaced old map. The destructor, when commit did
not run, destroys the constructed range, frees the new buffers, reverses
the transfers and the recentring or frees the new map, and restores the
four saved fields. The mechanism needs no exception support and works in
the no-exceptions configuration.

The inserts shift the shorter side. A middle scalar insert copies its
argument first (1.1). A range insert takes the direct path for pointers
and for this deque's own iterators, whose operations cannot throw; any
other source is consumed into a temporary deque first and that temporary
is inserted by the direct path (1.5). The input-iterator overload is the
staged path. `erase` moves the shorter side and pops, then rebuilds its
result from the surviving offset. Pops and `clear` keep every buffer;
`deque<T>().swap(d)` releases them. `swap` exchanges the four fields when
the allocators compare equal.

## 5. Verification

- The GCC 15D suite (debug, thread-safe, shared): 274 programs, 11912687
  assertions, 0 failed, 0 signalled, 90 runtime warnings unchanged;
  normalized output equal to the x86_64 15D pin. Rows moved: iterators
  4915404 to 4921862, modifiers 201238 to 590558, storage new at 1310326.
  Run twice, by the rewrite session and by the evaluation.
- `23.deque.storage` against the 7655ec93 headers: alias section 2600 of
  65450 fail, source section 464 of 14604 fail, boundary and allocator
  sections abort. The test bites.
- An independent oracle, outside the tree: `deque<int>` against an array
  model, pushes and pops at both ends, aliased single and count inserts,
  range inserts from pointers, a list, an input iterator and throwing
  input and bidirectional sources, erase with result checks, clear,
  assign, copy, swap, resize, release; after every operation size, both
  walks, subscripts, reverse walk, `end() + 0`, `begin() + size()`,
  differences and orderings on random pairs. Every operation with the
  no-effects guarantee retried with a `bad_alloc` at each allocation
  ordinal and every throwing source at each operation ordinal, the deque
  equal to the model after each caught exception, allocations balanced
  at exit. Passes with default and three-element buffers, three seeds
  each, and under ASan and UBSan; passes on another library without
  injection, which validates the harness; aborts on the 7655ec93 headers
  at the first comparison, on `end() + 0`.
- Three differential runs of 20000 mixed operations each match another
  library's `deque` on contents and erase results.
- Both header modes, forced `_RWSTD_NO_MEMBER_TEMPLATES` and the
  no-exceptions configuration compile a program that inserts, erases,
  copies and swaps. The no-member-templates path is syntax-checked only;
  no compiler here lacks member templates.
- A dense queue at four sizes performs a bounded number of map writes
  per bufferful after warm-up and never grows its map; the reviewed
  rotation moved nearly the whole map each time.
- The deprecated-copy warnings of the raw and wrapped deque iterators are
  gone; the one `-Wshadow` in `23.deque.special` predates the rewrite.

## 6. Costs and open items

### 6.1 Ownership bounds

The rewrite as landed found the owned span by scanning the map from
the live range outwards on every reservation at a null boundary, and
took a far spare from the span's far end, which only the scan located.
The work was proportional to the retained spare buffers: since pops
never free, a deque that grew to a peak and drained paid a peak-sized
scan on every bufferful of pushes at a null boundary until destroyed or
swapped away. The dense queue, with no spares, never saw it; the
drained queue saw nothing else. Measured with `deque<int>` grown to P
buffers and drained, a bufferful of pushes at the null boundary cost
0.4 ms at P = 1000 and 22 ms at P = 64000.

libc++ is the same model with the bounds cached, and so is this deque
now: two members hold the first owned slot and the owned count, the
reservation reads them and keeps them current through recentring, map
replacement, transfers and each buffer it allocates, and its rollback
restores the saved pair. The same bufferful costs 2 to 3 us at every P.
The scan survives in `_C_is_valid` as the debug check of the cache,
slot by slot. `push_front` at a buffer boundary constructs into the
spare of the slot before when the span says one is there, the fast
path `push_back` already had through the end iterator's element
pointer.

### 6.2 Staged sources

Every source that is not a pointer or this deque's iterator costs a
second copy per element and a temporary deque: a list, a set, a user
iterator, a `vector` iterator in a debug build. This is what p2's letter
requires for a source whose operations may throw; the other libraries
do not pay it and fail the throwing-source probes. The carve-out for
pointers and own iterators is sound, since their operations cannot throw.
The trade stands as implemented; relaxing it to forward iterators would
be a recorded deviation in the conformance note.

### 6.3 Edges of the geometry

At an allocator's hard node limit, growth cannot promise slack: a full
map with a far spare is reallocated at the same size to rotate the span.
Bounded maps of one to three slots run queues, fill to `max_size()` and
reject oversized inserts in the storage test. `max_size()` is
conservative by up to `B - 1` elements. The five pins other than
x86_64 15D take the moved rows from the next night.

## 7. Record

The reviewed tip is 7655ec93, tagged `freeze-2026`. The groom is
9c282871, the rewrite 49cbe97f, the findings' queue entries ddc0a0b3.
The regression tests are the probes: `23.deque.storage` one section per
finding, `23.deque.iterators` for the boundary and conversion cases,
`23.deque.modifiers` for the failure sweeps.

Written by OpenAI LeChuck, from the review and the rewrite's record.
Merged with the independent evaluation of the rewrite by Claude.
