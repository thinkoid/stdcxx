# Inside `basic_string`: the replace family

## 0. tl;dr

`basic_string` in this library is a one-pointer handle to a
reference-counted body. Every operation that changes the characters,
from the range constructor through `operator=`, `assign`, `insert`,
`append` and `erase`, is a call to one of five `replace` overloads,
and four of those five are thin wrappers around the fifth: `replace
(size_type pos, size_type n, const_pointer s, size_type n2)`, the
workhorse that knows how to grow, share, and write in place.

The only path that is not a wrapper is the member template that takes
a pair of iterators of any type. It has to answer three questions the
caller's types leave open: is this really a range or an integral
count-and-character pair, is the source single-pass or multi-pass, and
does the source live inside the string being modified. The first two
are answered by dispatch on the argument type and the iterator
category. The third cannot be answered inside a template without an
expression that only some iterator types support, so it is answered
by overload resolution instead: non-template overloads for the
string's own pointer and iterator types catch those arguments before
the template sees them and route them to the workhorse, which tests
the address; every other type is copied into a temporary first, which
is what the standard specifies the operation to mean.

This note walks that machinery for a reader meeting the class for
the first time. Line numbers are as of the repair in a1d757e3 and the
cleanup in 937a2394 and will drift; the names will not.

## 1. The handle and the body

`include/string` declares the class; `include/string.cc` holds the
out-of-line members and is included at the end of the header.
`include/rw/_strref.h` holds the body.

### 1.1 One pointer

A `basic_string` object is its allocator (a private base, so an empty
allocator costs nothing) and one member, `_C_data`, a `pointer` to the
first character. The characters are always NUL-terminated, so
`c_str ()` and `data ()` return `_C_data` as it is.

The bookkeeping lives immediately *before* the characters. `_C_pref
()` steps back one `__string_ref` from `_C_data` and returns it:

    _C_string_ref_type* _C_pref () const {
        return static_cast<_C_string_ref_type*> (static_cast<void*> (_C_data)) - 1;
    }

`size ()` and `capacity ()` are `_C_pref ()->size ()` and `_C_pref
()->capacity ()`. There is no size field in the handle.

### 1.2 The body

`__rw::__string_ref` is the header of an allocation that continues
with the characters:

| field | meaning |
|---|---|
| `_C_mutex` | present in a thread-safe build (`_RWSTD_REENTRANT`) unless atomic operations are used for the count; on Linux/x86-64 string atomics are off for binary compatibility with 4.1.x (`include/rw/_config.h`), so every thread-safe build carries it and a single-threaded build does not |
| `_C_refs` | the reference count, stored offset by `_RWSTD_STRING_REF_OFFSET` (1): a body owned by one handle holds 0, and a body with counting disabled holds -1 |
| `_C_cap` | capacity in characters |
| `_C_size` | size in characters, wrapped in a struct for an old compiler's sake |

`_C_get_rep (cap, len)` allocates `cap + sizeof (ref) / sizeof
(value_type) + 2` characters' worth through the allocator, placement-
constructs the header, sets count, capacity and size, and writes the
terminator at `len`. It returns the header; callers take `->data ()`
for the character pointer. `_C_unlink (ptr)` is the release: it
decrements the count of the current body, frees the body if this was
the last owner (or counting was disabled), and then points `_C_data`
at `ptr`. Every operation that builds a new body ends with `_C_unlink
(new_body->data ())`, which is how the swap from old to new happens in
one step after the new body is complete.

The empty string has no allocation. `_C_nullref ()` returns a static
body with capacity 0, size 0 and a single terminator, shared by every
empty string of that character type; `_C_get_rep (0, 0)` returns it,
and `_C_unlink` never frees it because its count is never decremented
below the threshold.

### 1.3 Sharing, and when it stops

The copy constructor shares: if counting is enabled on the source's
body it copies `_C_data` and increments the count, and `operator=
(const basic_string&)` does the same. Any modifying operation
first checks `size_type (1) < _C_pref ()->_C_get_ref ()`, meaning "two
or more handles", and if so builds a new body rather than writing into
the shared one. That is the copy-on-write half.

The other half is the non-const `begin ()`. Handing out a mutable
iterator means the caller may write through it later, after some
other handle has been copied from this one; the library cannot
intercept that write. So `begin ()` clones the body if it is shared,
and then *disables* counting on the body (`_C_unref` sets the count
to -1). From then on the body belongs to this handle alone and every
copy constructor called on the string will copy the characters. The
comment at the site says the rest: this is not thread-safe, and the
caller owns synchronization from that point.

### 1.4 Growth

`_C_grow (from, to)` asks `__rw_new_capacity` for the next capacity
after `from`, a geometric step with ratio 1.618 and a floor of 128
characters (`_RWSTD_STRING_CAPACITY_RATIO`,
`_RWSTD_MINIMUM_STRING_CAPACITY` in `include/rw/_defs.h`), and returns
the larger of that and `to`. Any operation that must reallocate calls
it with the old size and the required size.

## 2. Iterators

In a build without `_RWSTDDEBUG`, `iterator` is `pointer` and
`const_iterator` is `const_pointer`; `_C_make_iter (p)` returns `p`.

In a debug build, both are `__rw::__rw_debug_iter` (`include/rw/
_iterbase.h`), a class wrapping the pointer together with a pointer
to the owning container. Dereference, increment, subscript and
comparison assert that the iterator is dereferenceable, or that both
operands belong to the same container. `base ()` returns the wrapped
pointer without asserting anything, which is what makes it usable on
an empty range; `_ITER_BASE (it)` in `_defs.h` spells the same thing
in either build.

Every measured configuration of the revival is a debug build, so in
the suite tables the string's iterators are debug iterators. Two
helpers convert them to offsets: `_C_off (first, last)` is `last -
first` after asserting the range, and `_C_off (it)` is the offset from
`begin ()`.

## 3. Everything is `replace`

The public surface is large; the implementation is not. The map, from
`include/string` and `include/string.cc`:

| public operation | becomes |
|---|---|
| `basic_string (first, last)` (range constructor) | `replace (begin, begin, first, last)` on the empty string |
| `operator= (const_pointer)`, `operator= (value_type)` | `replace (0, size, s, n)` |
| `assign (…)`, every overload | `replace (0, size, …)` |
| `insert (pos, …)`, `insert (it, …)`, every overload | `replace (pos, 0, …)` or `replace (it, it, …)` |
| `append (…)`, every overload | `replace (size, 0, …)` or `replace (end, end, …)` |
| `erase (pos, n)`, `erase (it)`, `erase (first, last)` | `replace (pos, n, const_pointer (), 0)` |
| `replace (it, it, …)` in its non-template forms | `replace (_C_off (…), _C_off (…), …)` |
| `resize`, `operator+=` | `erase`, `append` or `replace` as above |
| `push_back` | writes in place when the body is unshared with room to spare, otherwise `replace (size, 0, 1, c)` |

Five `replace` overloads have a body of their own:

1. **`replace (size_type pos, size_type n, const_pointer s, size_type n2)`**,
   `string.cc`, the workhorse. Chapter 4.
2. **`replace (size_type pos, size_type n, size_type count, value_type c)`**,
   `string.cc`, the fill form; the same three branches as the
   workhorse with `traits_type::assign (p, count, c)` in place of the
   copy. It has no aliasing question: the source is one character
   passed by value.
3. **`replace (size_type pos, size_type n, const basic_string& str, size_type pos2, size_type n2)`**,
   `string`, which range-checks and forwards to the workhorse with
   `str.data () + pos2`. When `str` is `*this`, that is a pointer into
   the buffer, and the workhorse handles it.
4. **`replace (iterator, iterator, InputIter, InputIter, void*)`**, the
   member template, `string.cc`. Chapter 5.
5. **`__replace_aux (iterator, iterator, InputIter, InputIter)`**, the
   private helper the template uses once it holds a multi-pass source
   known not to alias the buffer. Chapter 7.

The dispatch in chapter 5 adds four non-template `replace` overloads
that have one-line bodies forwarding to the workhorse. They are the
subject of chapter 6.

## 4. The workhorse

`replace (pos1, n1, s, n2)` replaces the `n1` characters at `pos1`
(clipped to the end) with the `n2` characters at `s`. After the range
checks it computes `xlen`, the count actually removed, `size1`, the
resulting size, and `rem`, the length of the tail after the replaced
range. Then one of three things happens.

**A new body** is built when any of three conditions holds:

    if (   size_type (1) < size_type (_C_pref ()->_C_get_ref ())   // shared
        || capacity () < __size1                                    // too small
        || __s >= data () && __s < data () + __size0) {             // source inside

The third condition is the aliasing test: if the source starts inside
the current characters, writing in place could overwrite it before it
is read. In all three cases the new body is filled from the old one
and from `s` with three `traits_type::copy` calls (prefix, source,
tail), and only then is the old body released with `_C_unlink`. If
anything throws before that point the string is untouched; the
operation has the strong exception guarantee on this branch.

**In place** otherwise: the tail is moved to its new position with
`traits_type::move`, which is overlap-safe, the source is copied into
the gap, the terminator is written and the size updated. The order
matters and is correct here because the source is known to be outside
the buffer.

**Cleared** when the result would be empty: `clear ()`, which either
unlinks a shared body or sets the size to 0.

Note what the aliasing test needs: an address. The workhorse takes a
`const_pointer`, so it has one. The template in the next chapter takes
iterators of an arbitrary type, and does not.

## 5. Dispatching the template

The standard requires, for every container, that a call such as
`s.assign (3, 'x')` reach the count-and-character overload even though
the member template `assign (InputIter, InputIter)` deduces
`InputIter = int` and is an exact match. The library resolves that
first, then decides how to walk the range.

### 5.1 Integral or iterator

`_RWSTD_DISPATCH (T)` in `include/rw/_select.h` expands to

    (typename __rw::__rw_select_int<T>::_SelectT (1L))

`__rw_select_int<T>::_SelectT` is `int` for every integral type, by
explicit specialization, and `void*` for everything else. So the
expression is `int (1)` for an integral `T` and `(void*) 1` otherwise.
The public two-iterator template does nothing but append that value:

    template <class _InputIter>
    basic_string& replace (iterator first1, iterator last1,
                           _InputIter first2, _InputIter last2) {
        return replace (first1, last1, first2, last2, _RWSTD_DISPATCH (_InputIter));
    }

and the two targets differ in their last parameter:

    replace (iterator, iterator, size_type n, value_type c, int);    // the count form
    replace (iterator, iterator, InputIter, InputIter, void*);       // the range form

`assign`, `insert` and `append` carry the same pair. The trailing
`int` or `void*` is never used inside; the comment "unnamed arg is
used for overload resolution" marks each one. The non-template
overloads of chapter 6 carry the trailing `void*` too, so that they
compete in the same overload set as the range template.

### 5.2 Single pass or multi-pass

Inside the range template (`string.cc`, the function is defined
either as the member `replace (…, void*)` or, for a compiler whose
member templates cannot be declared `extern`, which the
characterization `_RWSTD_NO_EXTERN_MEMBER_TEMPLATE` detects and
neither GCC nor Clang triggers, as the free function `__rw_replace`;
the body is shared) three cases follow:

1. **Empty source.** Handled first, as an erase through the fill form.
   Everything after this can assume at least one source element.
2. **Bidirectional or random-access source**, decided by
   `__is_bidirectional_iterator (iterator_category)`, a pair of
   overloaded functions on the tag types in `_iterbase.h`. The source
   is copied into a temporary string, and the temporary's characters
   are handed to `__replace_aux`:

       _C_string_type __s3;
       __s3.__replace_aux (__s3.begin (), __s3.begin (), __first2, __last2);
       return __s.__replace_aux (__first1, __last1, __s3.begin (), __s3.end ());

   The temporary starts empty, so its own `__replace_aux` call takes
   the new-body branch, which reads the source before it touches
   anything. The copy is made this way, and not with the range
   constructor, because the range constructor is itself a call to
   this template and would recurse into the same branch.
3. **Input or forward source.** The elements are pulled one at a time
   into a temporary that grows as needed, in a hand-written loop, and
   the temporary is then handed to `__replace_aux` as above. This
   branch predates the repair; it existed for exception safety, so
   that a source that throws mid-way leaves the string untouched, and
   for the single pass a pure input iterator allows. It was alias-safe
   by construction, which is why input-iterator sources never showed
   the defect.

The result of cases 2 and 3 is the same: `__replace_aux` only ever
sees a source in a buffer other than the destination's.

## 6. The pointer overloads and why they exist

### 6.1 The problem

`__replace_aux` writes in place when it can, and in-place writing has
an order: move the tail, then copy the source. With a source inside
the buffer, either step destroys source elements not yet read. The
regressions read: `s = "abc"; s.insert (s.begin (), s.begin () + 1,
s.begin () + 2)` gave "aabc" for "babc". Which calls failed depended
on geometry alone: `assign` only through reverse iterators, since a
forward source at offset 0 is always ahead of the write; `append`
never, since the write lands past the end; `insert` and `replace` at
the front through every iterator kind, since the tail moves first.

The workhorse has the address test. The template does not have an
address. Getting one out of an arbitrary iterator is the trap the
library fell into twice in 2008:

- `&*first` is fine for pointers and for the string's own iterators,
  but for an iterator whose `operator*` returns by value it is the
  address of a temporary, and the comparison against the buffer is
  meaningless. Worse, the expression is instantiated for every type
  the template is used with, so a runtime "is this a pointer" test
  around it does not help: the code must compile for all of them.
- Casting the iterator object itself to a pointer does not compile
  for class-type iterators at all.

The rule that falls out: **no expression in the template may assume
anything about the iterator beyond the iterator requirements.** The
question "is the source inside the buffer" has to be asked somewhere
that already knows the source is a pointer.

### 6.2 The technique

That somewhere is overload resolution. Four non-template members sit
beside the range template, each with the same trailing `void*` so
that the dispatcher of 5.1 finds them in the same set:

    replace (iterator, iterator, const_pointer, const_pointer, void*);
    replace (iterator, iterator, pointer,       pointer,       void*);
    replace (iterator, iterator, const_iterator, const_iterator, void*);   // debug builds
    replace (iterator, iterator, iterator,       iterator,       void*);   // debug builds

The **`const_pointer`** overload computes the offsets and calls the
workhorse with `s` and `last - first`. The workhorse's own address
test then decides between a new body and writing in place. No
temporary string is built, and an allocation happens only when the
source really overlaps.

The **`pointer`** overload exists because of how C++ ranks candidates.
For a `char*` argument the template deduces `InputIter = char*`, an
exact match. The `const_pointer` overload needs a qualification
conversion, `char*` to `const char*`, which ranks lower, so the
template wins and the guard is skipped. The tree carried exactly this
gap for years: `insert` had a guarded `const_pointer` overload since
2005 (the STDCXX-25 repair), and `insert (it, &s [1], &s [2])` never
reached it. With a non-template taking `pointer`, the argument is an
exact match for both candidates, and a non-template beats a template
on a tie. It forwards to the `const_pointer` overload.

The **debug-iterator** overloads exist because in a debug build the
string's own `iterator` and `const_iterator` are class types, so they
would go to the template. They unwrap with `base ()`, which yields the
pointer without dereferencing, and forward to the pointer overloads.
In a non-debug build `iterator` *is* `pointer`, these two would be
redeclarations of the first two, and the `#ifndef _RWSTD_NO_DEBUG_ITER`
guard keeps them out.

Everything else, `reverse_iterator`, `const_reverse_iterator`, user
iterators, iterators from other containers, goes to the template and
is copied first. That is the operation as 21.3.5.6 of the 1998 and
2003 standards describes it: `replace (i1, i2, basic_string (j1,
j2))`. Writing in place is the optimization, and it is taken only
where the type system has proved it safe.

### 6.3 Why not a trait

A compile-time `is_pointer<InputIter>` could select a specialization
instead of an overload; the effect would be the same, and the library
has `__rw_is_pointer`. The overloads were chosen because they are the
idiom the class already used, at `insert`, since 2005; because they
read as ordinary C++98 without a second mechanism; and because the
ranking rule that makes the `pointer` overload necessary is the same
rule that makes it sufficient. What must not be done is a *runtime*
test on the trait inside the template, which was the second 2008
attempt: the address expression is still instantiated for every type.

### 6.4 Cost

A reverse-iterator or user-iterator range now costs one temporary
allocation it did not before; a pointer range costs an allocation only
when it overlaps the buffer, where it previously produced the wrong
answer. The tests measure this: they run each case once per
allocation the operation makes, scheduling a `bad_alloc` at each, and
their assertion totals rose by one round per case on the copying path,
all passing.

## 7. `__replace_aux`

The helper takes a multi-pass source and assumes it does not overlap
the destination; every caller now guarantees that. It computes the
same `xlen`, `len`, `rem` as the workhorse and has the same shape:

- **New body**, when the body is shared or the capacity is short:
  prefix copied, source assigned element by element through the
  iterator, tail copied, `_C_unlink`. The source is read from the old
  buffer before the swap, which is why this branch was always safe
  and why a forced growth never showed the defect.
- **In place** otherwise: tail moved with `traits_type::move`, source
  assigned element by element, terminator and size written.
- **Empty result**: unlink to the null body.

It does not test for overlap because it has no address to test; that
is the division of labour this note describes. A future change that
gives it a pointer source should route through the workhorse instead.

## 8. Where to look

| what | where |
|---|---|
| the handle, typedefs, public overloads, private helpers | `include/string` |
| the body | `include/rw/_strref.h` |
| the workhorse, the fill form, the range template, `__replace_aux` | `include/string.cc` |
| debug iterators | `include/rw/_iterbase.h` |
| integral dispatch | `include/rw/_select.h` |
| growth policy | `include/rw/_defs.h` |
| the tests | `tests/strings/21.string.{assign,insert,replace,append,cons}.cpp` |
| the regressions | `tests/regress/21.string.*.stdcxx-{170,629,632,438}.cpp` |
| the repair and its measurement | `working/test-baseline.md`, chapter 3.7; commits a1d757e3 and 937a2394 |
| the history | `git log -S__replace_aux -- include/string.cc`, the 2008 entries |
