# The narrow collate transform

`collate<char>::transform` turns a string into a byte string that
sorts, under `char_traits<char>::compare`, in the order the locale's
collation rules put the original. The byte string is built by
spelling each of the string's collation weights as bytes, and for
twenty-one years the spelling could not tell where one weight ended
and the next began, nor a large weight from an IGNOREd element. This
note records the encoding, the two faces of the defect, the fix and
why it reserves two byte values rather than one.

## 0. tl;dr

- The weights are unsigned integers of arbitrary size and the
  transform is a string of `char`. The library spelled a weight as a
  run of `CHAR_MAX` bytes followed by a remainder byte, subtracting
  `CHAR_MAX` per run byte, so the remainder of a weight that is a
  multiple of `CHAR_MAX` was `CHAR_MAX` itself: the spelling of the
  smaller weight was a **prefix** of the spelling of the larger, and
  the next weight merged into it.
- `collate_byname<char>::do_compare` transforms both operands and
  compares the results whole, so the merge was a wrong answer from
  `compare`, not only from `transform`. The letter "l" in the tree's
  `hr_HR` has primary weight 1016, eight times 127, and ko kai in
  `th_TH` has 127; both sorted after every other letter, 101 lines of
  the two word lists under `tests/etc`.
- The same byte, `CHAR_MAX`, is also the mark the position orderings
  write for an IGNOREd element, so a weight of `CHAR_MAX` or more
  opened exactly like an IGNOREd element and the comparison fell
  through to bytes that had nothing to do with the answer.
- The fix reserves two values. `CHAR_MAX - 1` is the run byte, worth
  `CHAR_MAX - 2` each, so the final byte is in 1 to `CHAR_MAX - 2`
  and no run byte can be taken for it; `CHAR_MAX` is the IGNORE mark
  and nothing else, so every weight's spelling opens below it. On a
  signed `char`: run byte 126 worth 125, final byte in 1 to 125,
  mark 127. The encoding is prefix-free, monotone, and below the
  mark. Transform strings are not persistent, so nothing depended on
  the old spelling.
- Reserving one value is not enough: with the mark still equal to
  the run byte, a character of weight 128 against an IGNOREd
  character followed by one of weight 2 spells `7f 02` on both sides
  and ties where the first must sort first, a case the old spelling
  happened to get right. The test now builds that case.
- The wide facet stores one `wchar_t` per weight and its mark is
  `WCHAR_MAX`; it was never affected, which is why only the `char`
  half of `22.locale.collate` failed.
- Chapter 3 walks one letter through both spellings and through the
  comparison each of them produces.
- Unchanged since the 2005 import and present on every branch of the
  repository.

## 1. What the transform is made of

A locale's collation definition gives every collating element a
weight per pass -- primary, secondary, and so on -- and states for
each pass whether it is read forward, backward, or by position. The
database `localedef` writes holds the weights as `unsigned int`.

`__rw_process_offsets` (`src/collate.cpp`) walks the string once per
pass, appends each element's weight for that pass through
`__rw_append_weight`, and ends the pass with a byte 1. In the two
position orderings (`forward,position` and `backward,position`) an
IGNOREd element is not skipped but marked, with the largest value of
the character type, so that the string whose first non-IGNOREd
element comes after fewer IGNOREd ones sorts first. The wide
overload of `__rw_append_weight` appends the weight as one
`wchar_t`. The narrow one has to spell a value that does not fit a
byte across several bytes, and that spelling is this note's subject.

Two facets consume the result. `do_transform` returns it, and
`do_compare` transforms both operands and returns the sign of
`string::compare` over the two. Nothing decodes a transform string:
the encoding's only contract is that the byte order agrees with the
weight order.

## 2. The defect

The spelling was

    while (CHAR_MAX < wt) {
        out += char (CHAR_MAX);
        wt  -= CHAR_MAX;
    }
    out += char (wt);

which writes `floor ((wt - 1) / CHAR_MAX)` run bytes and a remainder
in 1 to `CHAR_MAX`. The remainder is `CHAR_MAX` exactly when `wt` is
a multiple of `CHAR_MAX`, and then the spelling is a run of run
bytes and nothing else. A run of *k* bytes is the spelling of
127*k*, and it is also the opening of every larger weight, so

    spelling (127) = 7f          is a prefix of
    spelling (254) = 7f 7f       is a prefix of
    spelling (381) = 7f 7f 7f

and the weights that follow decide a comparison they should have
had no part in. A prefix-free code is what the concatenation needs
and this one is not.

### 2.1 The probe

With the tree's `hr_HR` built from `etc/nls/src`, the letters and
two words transformed to (the first pass only, then the pass
terminators):

    a        7f 7f 7f 7f 7f 6f 01 ...
    l        7f 7f 7f 7f 7f 7f 7f 7f 01 ...
    z        7f 7f 7f 7f 7f 7f 7f 7f 7f 7f 3e 01 ...
    lav      7f 7f 7f 7f 7f 7f 7f 7f 7f 7f 7f 7f 7f 6f ...
    zub      7f 7f 7f 7f 7f 7f 7f 7f 7f 7f 3e ...

"l" alone is eight run bytes and stops, because the pass terminator
1 follows and terminates it by accident. In "lav" the five run bytes
of "a" continue the eight of "l" into thirteen, and thirteen run
bytes beat the ten of "z": `compare ("z", "lav")` answered -1, and
`compare ("lav", "zub")` answered 1. The letter sorts correctly
alone and wrongly in every word.

The same probe on `th_TH` shows ko kai, primary weight 127, as the
single byte `7f`.

### 2.2 What it cost

`22.locale.collate` sorts six word lists with `compare` and asserts
that the result is the file. The `char` half failed 36 lines of the
Croatian list and 65 of the Thai one; the `wchar_t` half sorted all
six as the definitions say. Every other locale in the test has no
letter whose weight is a multiple of 127, which is why the defect
survived: it needs a weight of exactly the wrong size to show.

### 2.3 The other face: the IGNORE mark

The mark the position orderings write for an IGNOREd element is
`numeric_limits<char>::max ()`, the same `7f` as the run byte. A
weight of 127 spelled as the mark, byte for byte, and any larger
weight opened with it. Take a position pass in which a character
`a` has weight 129, a character `b` weight 2, and `x` is IGNOREd,
and compare the strings "a" and "xb":

    a     7f 02          (129 = 127 + 2)
    xb    7f 02          (the mark, then 2)

A tie, where "a" must sort first: its first non-IGNOREd element is
at position 0 and "xb"'s at position 1. With `a` of weight 130 the
answer comes out the wrong way round. The tree's own definitions do
not reach this -- their position passes weight a character by its
ordinal, so the weights just past 127 have tiny final bytes and the
smallest weight in `hr_HR`'s position pass is 79 -- and no line of
the six word lists depends on it. It is one definition away.

## 3. A weight spelled both ways

The spelling is a loop, and the loop reads more easily as arithmetic.
Both versions write `k` run bytes and one final byte; they differ
in which byte is the run byte and what it is worth.

| | run byte | worth | run bytes `k` | final byte |
|---|---|---|---|---|
| before | 127 | 127 | `(wt - 1) / 127` | `(wt - 1) % 127 + 1`, in 1 to 127 |
| after | 126 | 125 | `(wt - 1) / 125` | `(wt - 1) % 125 + 1`, in 1 to 125 |

The division is integer division. The `- 1` and `+ 1` are there
because a weight of 0 is IGNORE and never reaches this code, so the
final byte counts from 1 rather than from 0. The residue is taken
over 125: the final byte can be neither 126, which is what reserves
126 for the run, nor 127, which is what reserves 127 for the mark.

### 3.1 A weight that is a multiple of 127

The letter "l" in the tree's `hr_HR` has primary weight **1016**,
which is 8 x 127. Before:

    k     = (1016 - 1) / 127 = 1015 / 127 = 7
    final = (1016 - 1) % 127 + 1 = 126 + 1 = 127

    7f 7f 7f 7f 7f 7f 7f  7f
    \__ 7 run bytes ___/  \_ final byte, 127 as well

which is eight bytes of 7f and no way to tell the seventh run byte
from the eighth. After:

    k     = (1016 - 1) / 125 = 1015 / 125 = 8
    final = (1016 - 1) % 125 + 1 = 15 + 1 = 16

    7e 7e 7e 7e 7e 7e 7e 7e  10
    \___ 8 run bytes _____/  \_ final byte

Eight run bytes and a final `10`: `8 x 125 + 16 = 1016`, and the
byte after the run says where the weight stops.

### 3.2 A weight that is not

The letter "a" has primary weight **746**. Before:

    k     = 745 / 127 = 5     final = 745 % 127 + 1 = 110 + 1 = 111 = 6f
    7f 7f 7f 7f 7f 6f         5 x 127 + 111 = 746

After:

    k     = 745 / 125 = 5     final = 745 % 125 + 1 = 120 + 1 = 121 = 79
    7e 7e 7e 7e 7e 79         5 x 125 + 121 = 746

Same length, a different final byte, and both spell 746. A weight
that is not a multiple of 127 was never in trouble with the first
face of the defect: its final byte was already below 127. Only the
change of what a run byte is worth moves it, and a spelling gains
a byte only where the old `k` and the new one differ.

### 3.3 Where the first spelling failed

One letter sorted correctly on its own. Each pass of the transform
ends with a byte 1 (chapter 1), so "l" alone came out

    7f 7f 7f 7f 7f 7f 7f 7f  01 ...
                             \_ end of pass, and by luck the end of
                                the weight as well

and the pass terminator did the job the final byte should have done.
Put another letter after it and there is no terminator to borrow.
"la", before:

    l    7f 7f 7f 7f 7f 7f 7f 7f
    a                             7f 7f 7f 7f 7f 6f
    la   7f 7f 7f 7f 7f 7f 7f 7f  7f 7f 7f 7f 7f 6f

Thirteen run bytes and a `6f`. Nothing in that string says the
weight of "l" ended after the eighth byte, and `do_compare` does not
look: it hands both transformed strings to `string::compare`. Read
the way the comparison effectively reads it, "la" opens with one
weight of `13 x 127 + 111 = 1762`, not with 1016.

Against "z", weight **1332**, which is `10 x 127 + 62`:

    byte     1  2  3  4  5  6  7  8  9 10 11 12 13 14
    z       7f 7f 7f 7f 7f 7f 7f 7f 7f 7f 3e
    lav     7f 7f 7f 7f 7f 7f 7f 7f 7f 7f 7f 7f 7f 6f ...

The first ten bytes are equal. At the eleventh "z" has its final
byte `3e` and "lav" is still running, and `7f` beats `3e`, so "lav"
sorted after "z" -- and after every other word, since 1762 is larger
than any single letter's weight in the locale. `compare ("z", "lav")`
answered -1.

### 3.4 Where the second one does not

After the fix the same two words are

    byte     1  2  3  4  5  6  7  8  9 10 11 12 13 14 15
    z       7e 7e 7e 7e 7e 7e 7e 7e 7e 7e 52
    lav     7e 7e 7e 7e 7e 7e 7e 7e 10 7e 7e 7e 7e 7e 79 ...

"z" is ten run bytes and a `52`, which is `10 x 125 + 82 = 1332`;
"lav" is eight run bytes and a `10`, which is "l", then the five
run bytes and `79` of "a", then "v". The two differ at the ninth
byte, where "lav" has the final byte of "l" and "z" is still
running: `10` is below `7e`, so "lav" comes first, which is what
weight 1016 against weight 1332 says. `compare ("z", "lav")`
answers 1.

The general form of it: two spellings can agree only as long as both
are running. The first one to stop puts a byte below 126 where the
other has 126, so the one still running is the larger weight -- which
is exactly the order the weights are in, since a longer run means
more 125s. That is the whole argument of chapter 4, seen once.

### 3.5 The other letter in the test

Ko kai in `th_TH` has primary weight **127** itself, the smallest
multiple there is:

    before   k = 126 / 127 = 0   final = 126 % 127 + 1 = 127   ->  7f
    after    k = 126 / 125 = 1   final = 126 % 125 + 1 = 2     ->  7e 02

Before the fix the letter was a single run byte, indistinguishable
from the opening of every weight of 127 or more in the locale, and
from the IGNORE mark, and it too sorted last. After, it is `7e 02`,
and the next letter kho khai, weight 133, is `7e 08`: the same run,
a larger final byte, the right order.

### 3.6 A weight against the mark

The case of chapter 2.3, with the weights the test uses: a character
`m` of position weight **128**, a character `n` of weight **2**, and
an IGNOREd `l`; the strings "m" and "ln", position pass only.

    before   m    7f 01         (128 = 127 + 1)
             ln   7f 02         (the mark, then 2)

    after    m    7e 03         (128 = 125 + 3)
             ln   7f 02         (the mark, then 2)

Before, "m" sorted first by the accident that 1 is below 2: with
`m` of weight 129 the two tie, and from 130 up the order is wrong.
After, "m" opens with the run byte 126 and "ln" with the mark 127,
and "m" sorts first whatever the weights, because every spelling
opens below the mark: with a byte below 126 if the weight fits one
byte, with 126 if it does not.

Reserving only the run byte, and leaving the mark equal to it, would
have spelled `m` as `7f 02` against `7f 02`: a tie, on a case the
old spelling got right. That is why the fix reserves two values.

## 4. The fix

    const unsigned run_byte = CHAR_MAX - 1;

    while (run_byte <= wt) {
        out += char (run_byte);
        wt  -= run_byte - 1;
    }
    out += char (wt);

Each run byte stands for `CHAR_MAX - 2` and the loop runs while the
weight is `CHAR_MAX - 1` or more, so the final byte is in 1 to
`CHAR_MAX - 2` and a run byte is never final. A weight is spelled
`k` run bytes and a final byte `wt - (CHAR_MAX - 2) * k`, with `k =
floor ((wt - 1) / (CHAR_MAX - 2))`. `CHAR_MAX` itself is never
written by this code; it is the IGNORE mark, and only that.

**Prefix-free.** Two spellings that agree up to the end of the
shorter one agree at its final byte, which is below the run byte;
the longer spelling has a run byte there. So the shorter is not a
proper prefix of the longer.

**Monotone.** `k` is nondecreasing in `wt`. More run bytes win,
because at the position where the shorter spelling puts its final
byte the longer one has the run byte, which is larger. Equal run
counts compare by the final byte, which increases with `wt`.

**Below the mark.** A spelling opens with its final byte if it has
no run bytes, and with a run byte otherwise; both are below
`CHAR_MAX`. So at any position of a position pass a weight sorts
before an IGNOREd element, whatever the weight and whatever follows
either.

Together those make the concatenation of spellings order the same
way as the sequence of weights, with IGNOREd elements last at their
position, which is what `do_compare` asks of it.

**Cost.** A weight `w` is spelled in `floor ((w - 1) / 125) + 1`
bytes where it was spelled in `floor ((w - 1) / 127) + 1`, so the
increase is `floor ((w - 1) / 125) - floor ((w - 1) / 127)`: one
byte at most for the weights the tree's definitions produce, and in
general about `w / 7938` bytes. Transform strings are documented as
valid only for the locale and the program run that made them --
nothing in the tree or the standard persists one -- so a change of
spelling has no compatibility cost.

The test's `test_weight_val` builds its expectation by repeating the
library's spelling, so it repeats the new one.

## 5. Why not one reserved value

The first face of the defect is fixed by any spelling whose final
byte cannot be a run byte, and the natural one reserves a single
value: run byte `CHAR_MAX`, worth `CHAR_MAX - 1`, final byte in 1 to
`CHAR_MAX - 1`. That was the first shape of this change, and it
fixed the two word lists.

It also made the second face worse. The mark of the position
orderings stayed `CHAR_MAX`, still the run byte, so a weight of
`CHAR_MAX` or more still opened like an IGNOREd element; and where
the old spelling had put a final byte of `w - 127` after the run,
this one put `w - 126`, one larger. A weight of 128 against an
IGNOREd element followed by a weight of 2 went from `7f 01` against
`7f 02`, correct, to `7f 02` against `7f 02`, a tie. A defect that
had been latent in the tree acquired a new case, and the case the
old spelling happened to get right.

The mark has to sort above every spelling, and a spelling's first
byte is either a small weight or the run byte, so the mark must be
above the run byte: two values, not one. The byte has room for
them. A weight of 0 is IGNORE and never spelled, so 0 is unused;
1 ends a pass and is also the spelling of a weight of 1, an overlap
that predates this change and is not touched by it.

## 6. Verification

`22.locale.collate` goes from 103 failed assertions of 809 to 0 of
714 in every configuration, both halves; a bare run in `11S` gives
the same with `TOPDIR` set and unset. The probe of chapter 2.1 shows
"l" as eight run bytes and a final `10`, ko kai as `7e 02`, and
both words in their places. The weight-127*k* case the `TODO` entry
asked for is ko kai against kho khai: `7e 02` before `7e 08`.

The test's synthetic locale, which already had a
`forward;backward;forward,position` order and an IGNOREd letter
"l", gained the two characters of chapter 3.6: "m" and "n" tie on
the first two levels and part at the position level with weights
128 and 2, and `compare ("m", "ln")` must answer -1. The library
built with the one-value spelling of chapter 5 fails that assertion
with 0, the tie; the library built with the old spelling passes it,
and would fail it with a weight of 129.

Across the six pinned configurations the collate row is the only
row of the suite that moves, apart from the rows that vary from run
to run and varied again -- `23.bitset.cons` in the 64-bit builds and
the MT locale tests in the thread-safe ones, `test-baseline.md`
chapters 3.4 and 3.5. Where those held still the assertion totals
fall by exactly the 95 assertions the collate row loses.

The test's own `TOPDIR` handling changed in the same commit. It read
the environment variable directly and called `exit (1)` when it was
unset, which killed the run before the driver could report; it now
uses `rw_topdir ()`, the driver's accessor that falls back on the
location of its own source file, and asserts once instead of
exiting. That was the last of the three sites the baseline work
found.
