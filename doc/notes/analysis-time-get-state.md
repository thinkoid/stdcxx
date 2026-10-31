# The state a `time_get` parse carries across `do_get`

`time_get` parses a format one conversion directive at a time, each
through the virtual `do_get`, and a `do_get` call can hand back only
a `struct tm`. Some directives have no member in that structure, and
some mean something only together with others. This note records
the conflict, why the interface stays, what the C library and
libstdc++ do with the same problem, what this library does, and the
alternatives that were weighed for the week number in October 2026.

## The directives at issue

| directive | what it parses, per `strptime` | `tm` member | the problem |
|---|---|---|---|
| `%C` | the century, two digits | none | a year only with `%y`; alone, nothing to assign |
| `%y` | the year within the century, two digits, pivoted 69..99 to 19xx and 00..68 to 20xx | `tm_year` | the pivot is wrong when a `%C` came or comes later; the two combine at the end |
| `%Y` | the full year | `tm_year` | fine alone; it drops a `%y` parsed before it |
| `%U` | the week of the year, Sunday the first day, 00..53 | none | a day of the year only with a year and a weekday; alone, nothing to assign |
| `%W` | the week of the year, Monday the first day, 00..53 | none | as `%U`; a Sunday is the last day of the previous Monday-week |
| `%w`, `%a`, `%A` | the weekday, a number or a name | `tm_wday` | fine alone; also an input to the week derivation |
| `%j` | the day of the year, 001..366 | `tm_yday` | fine alone; an explicit `%j` must win over a derived one |
| `%m`, `%d`, `%e` | the month and the day of the month | `tm_mon`, `tm_mday` | fine alone; explicit values must win over the week derivation, and together with the year they imply `tm_wday` and `tm_yday`, which nothing names |
| `%I`, `%l` | the hour on the 12-hour clock, 1..12 | `tm_hour` | the hour of the day only with `%p`; stored modulo 12 within the half of the day `tm_hour` already holds |
| `%H`, `%k` | the hour on the 24-hour clock | `tm_hour` | fine alone; a `%p` after it sets the half of the day, where glibc ignores it |
| `%p` | the half of the day, the locale's AM or PM names | none | sets the half of the day of the hour it finds, before or after a `%I` |
| `%Z` | the time zone name | none | consumed, nothing to assign |
| `%z` | the offset from UTC | `tm_gmtoff` where the C library has it | nothing to assign elsewhere |
| `%x`, `%X`, `%c`, `%D`, `%r`, `%R`, `%T`, `%F` | a locale's or a fixed format, a whole pattern | several | a nested walk: what it parsed is settled inside it and does not reach the pattern around it |
| `%E` with `C`, `y`, `Y`, `x`, `X`, `c` | the era forms: an era name, a year within the era, the era's formats | `tm_year` and others | without era data they must fall back to the plain directive; with it, an era name is a start year and an offset, not a member |
| `%O` with a numeric directive | the locale's alternative digits for the value | the plain directive's | a value with no alternative symbol is read as digits; the first alternative symbol may be empty |

## 0. The short version

- A `%U` or `%W` week number has no `tm` member. `%C` alone has
  none, nor has `%p`. `%C` with `%y`, `%I` with `%p`, and a week with
  a year and a weekday are each worth a member only in combination.
- The standard splits `strptime`'s one call into one virtual call
  per directive and gives the virtual nothing but the `tm` to write.
  The caller that saw all the directives is the only place the
  combining can happen, and it has to read the pieces back out of
  the `tm`.
- glibc keeps a private state structure for the one call and
  combines at the end. libstdc++ keeps the same structure in its
  format walk, and bypasses the standard's `do_get` dispatch to keep
  it, by a compiler-specific test that the virtual is not
  overridden. The standard's design cannot carry strptime's
  semantics through the interface it mandates; libstdc++ worked
  around that defect for the one compiler that lets it.
- This library has the same defect in its own form, since
  2026-10-29: `get` keeps the century, the half of the day and the
  week number in locals, reads them back from the `tm` after each
  `do_get`, and rewrites the `tm` at the end. A `get` that is
  required to call the virtual and then overrules what the virtual
  stored breaks the virtual's contract; an override of `do_get` is
  clobbered, and the library cannot know one exists.
- To be clear about how that happened: the revival got carried away.
  A round trip of `time_put` through `time_get` under glibc's locale
  sources was the oracle, glibc's `strptime` was the reference for
  every failing row, and three changes in two days made the rows
  pass by giving `get` the state `strptime` has. Nobody asked
  whether the interface allowed it. It does not, and the rows should
  have been accepted as limits of the design, as the 2005
  implementation and the standard's own remark had accepted them.
- The way back is the model the standard's remark allows: `do_get`
  updates the `tm` it is given, so the `tm` carries the century and
  the half of the day between calls; `get` dispatches and nothing
  more; the week number, which nothing carries, is consumed and
  stored nowhere. Chapter 6 has the steps.
- The interface stays. The C++11 members came into the standard
  from this facet, and the 2003 members are specified by what
  `time_put` writes, which needs the same engine.

## 1. The interface

14882:2003's `time_get` has six virtuals: `do_date_order` and
`do_get_time`, `do_get_date`, `do_get_weekday`, `do_get_monthname`,
`do_get_year` (22.2.5.1.2). Each of the five parsing members is
specified by the format `time_put` produces for a specifier: `%X`,
`%x`, `%a`, `%b`, `%Y` (22.2.5.1, p1). There is no `do_get` with a
format in 2003, and no public way to parse an arbitrary directive.

C++11 added three members, signature for signature this library's
(22.4.5.1.1, p6 to p10; 22.4.5.1.2, p11 to p15 in N3242): `get` with
a format character and a modifier, `get` over a format range, and
the virtual `do_get` behind both. They entered through N2070 and
N2211, which name this library as the reference implementation. The
text is unchanged through C++23 (30.4.6.2 in N4950). The revival
keeps them as its one ruled exception to the 2003 baseline, for the
reasons `ref-conformance.md` chapter 2 and
`analysis-post-strict-ansi.md` 4.4 record: the committee took the
text from this facet, and the five 2003 members ride on the same
engine, `do_get_time` being a walk over the locale's `%X` and
`do_get_year` being `do_get('Y')`. Removing the public pair would
leave the engine in place, private, with the suite's rows
unreachable. They left in b7443c95 and came back the same day.

Three sentences of the C++11 text govern everything below.

- `get` over a range (22.4.5.1.1, p8): for each conversion
  specification, "the function evaluates s = do_get(s, end, f, err,
  t, format, modifier)". The walk must call the virtual, once per
  directive.
- `do_get` (22.4.5.1.2, p12): it "reads characters starting at s
  until it encounters an error, or until it has extracted and
  assigned those struct tm members, and any remaining format
  characters, corresponding to a conversion directive appropriate
  for the ISO/IEC 9945 function strptime". An invalid directive
  "leaves the object pointed to by t unchanged" with `failbit`.
- The remark (p14): "It is unspecified whether multiple calls to
  do_get() with the address of the same struct tm object will update
  the current contents of the object or simply overwrite its
  members."

So `strptime` is the reference for what each directive means, the
`tm` is the only output, and the standard knows that a directive
may not fit in it: p13 says that for `%c`, `%x`, `%X` and any `E` or
`O` directive, "when the function is unable to unambiguously
determine some or all struct tm members from the input sequence",
it sets `eofbit` and "the values of those struct tm members are
unspecified and may be outside their valid range".

## 2. The conflict

`strptime` is one call over the whole format. It can keep what it
saw in the first directive until it has seen the last. The standard
turned that one call into one virtual call per directive and kept
the C structure as the only thing a call can hand back. The
mismatch has four shapes in this facet.

- Directives with no member. `%U` and `%W`, the week numbers; `%C`,
  the century alone; `%p`, the half of the day; `%Z`, and `%z` where
  the C library's `tm` has no `tm_gmtoff`. A lone `do_get` has
  nothing to assign, and p12's "those struct tm members
  corresponding to" the directive is the empty set.
- Directives that mean something only in combination. `%C` with
  `%y` make a year; `%I` with `%p` make an hour; `%U` or `%W` with
  a year and a weekday make a day of the year. Each `do_get` sees
  one of them. Only the caller that saw all of them can combine,
  and it needs the raw values, not the members they were stored in.
- Directives that imply members they do not name. `%Y %m %d` fixes
  the weekday and the day of the year without a directive for
  either. glibc fills both at the end; the library leaves them as
  the caller had them.
- Members no directive sets. `tm_isdst`.

Order compounds the first two. "%p %I" and "%I %p" must give the
same hour, and "%U %w %Y" the same date as "%Y %U %w", so the
combining cannot happen in the directive that comes last; it has to
wait for the end of the format.

### 2.1 The design against the intention

N2211's title states the intention, "Enhancing the time_get facet
for POSIX compatibility": parse what `strptime` parses. `strptime`'s
semantics are per call, not per directive; the combining above is
what glibc's state structure and libstdc++'s `__time_get_state`
exist for. The standardized design has one virtual per directive and
a `tm` as its only memory. It delivers `strptime`'s directive set
and cannot deliver `strptime`'s semantics, except by `get` overruling
the virtual it is required to call, which breaks the one thing a
virtual is for. libstdc++'s comment, chapter 4, is that contradiction
written down by people who met it: "without an ABI change for the
facets we can't keep state across the different do_get calls". The
design matches the paper's directive list and not its purpose.

What the text does permit is the remark's update model: a `do_get`
that reads the `tm` it is given and updates it. The `tm` then carries
between calls whatever has a member, and `get` has nothing to decide.

### 2.2 The same defect in this library

Between 2026-10-29 and 2026-10-30 three changes put the combining
into `get`: 59f3a87e for the century and the two-digit year,
2829b153 for the hour and the half of the day, and an uncommitted
change for the week number with a sentinel handshake across the
virtual. Each reads a value back from the `tm` after the `do_get`
that produced it, assuming the base `do_get`'s storage, and rewrites
the `tm` at the end of the format. That is `get` deciding over
`do_get`. An override that stores differently is clobbered, an
override for `%U` sees a scratch `tm` and has its work discarded, and
no portable test tells the library that an override exists. The rows
those changes pinned are right against glibc; the place they were
made right is wrong.

This was the revival getting carried away, and it should not have.
The oracle measured parity with glibc, and parity became the goal:
each failing row was a bug, each bug got a fix, and the fixes went
where the rows were easiest to make pass. The question that belonged
before the first of them, whether the interface the standard
mandates can carry the state the fix needs, was asked after the
third. The rule the tree states for itself, that a fix which looks
like an oversight is presumed design until the history says
otherwise, applied here to the standard's design as much as to the
library's, and was not applied. Chapter 5 describes the mechanism as
it stands, chapter 6 the way back.

## 3. What glibc does

`strptime_l.c` declares a `struct __strptime_state` for the call:
`have_I`, `is_pm`, `have_century`, `century`, `want_century`,
`have_wday`, `have_yday`, `have_mon`, `have_mday`, `have_uweek`,
`have_wweek`, `week_no`, `want_xday`, `decided`. Each directive
reads its number and sets its flag; `%C` stores the century in the
state, `%U` and `%W` store `week_no`, `%p` stores `is_pm`. The
members that exist are written as they are parsed. At the end of
the format one block settles everything:

- the century and the two-digit year combine into `tm_year`;
- `is_pm` adds twelve hours to an `%I` hour;
- with `have_wday` and a week number, `tm_yday` is derived from the
  weekday of January 1, which it computes from `tm_year` by
  arithmetic, and `tm_mon` and `tm_mday` follow from `tm_yday` where
  `%m` and `%d` were not parsed;
- `want_xday` fills `tm_wday` and `tm_yday` from a complete date
  where they were not parsed.

Three facts from probing glibc 2.44 on 2026-10-30, with the probe
programs under the certification directory of that day:

- It derives against whatever `tm_year` holds. "%U %w" without a
  year computes against 1900; "%Y %U" without a weekday stores
  nothing from the week.
- A week and weekday outside the year give garbage: `tm_mon` of -1
  for "2026 00 0" under `%U`, 25 for week 53 with a late weekday.
  The table it indexes has no bounds check.
- The `E` modifier's state is weaker than the `O` modifier's. `%EC`
  without era data falls back to `%C`; `%EY` to `%Y`; `%Ey` reads
  four digits before it looks at eras and does not restore the
  position when it falls back, so a lone "26" fails and "26 26" is
  read twice.

The `decided` field latches a choice of representation once per
call: after an alternative-digit match has fallen back to plain
digits, every later `%O` directive in the same call must use plain
digits too, so "%Od %Od" on "3 3" fails.

## 4. What libstdc++ does

libstdc++ has the same state structure, `__time_get_state` in
`bits/locale_facets_nonio.h`: `_M_have_I`, `_M_have_wday`,
`_M_have_yday`, `_M_have_mon`, `_M_have_mday`, `_M_have_uweek`,
`_M_have_wweek`, `_M_have_century`, `_M_is_pm`, `_M_want_century`,
`_M_want_xday`, `_M_week_no`, `_M_century`. Its format walk,
`_M_extract_via_format`, takes the state by reference, each
directive sets its flags, and `_M_finalize_state` settles the `tm`
at the end of the walk, in the compiled library rather than the
header. The five 2003 members each run a walk with a fresh state
and finalize.

Its C++11 `get` over a range faces the standard's p8, which says
the walk evaluates `do_get` per directive. The header says what it
does about that, in a comment headed "Nasty hack": "The C++ standard
mandates that get invokes the do_get virtual method, but
unfortunately at least without an ABI change for the facets we
can't keep state across the different do_get calls. So e.g. if
__fmt is "%p %I:%M:%S", we can't handle it properly, because we
first handle the %p am/pm specifier and only later the 12-hour
format specifier." The code compares the address of `do_get` as
seen through `this` with the address of `time_get::do_get`, a
pointer-to-member-function conversion GCC allows under a diagnostic
it silences. When the two agree, the virtual is not overridden, and
`get` runs the stateful walk itself without calling `do_get`. When
they differ, it calls `do_get` per directive, as the standard says,
and the combining is lost.

Its C++11 `do_get` builds a two- or three-character format from the
directive, runs a walk over it with a fresh state and finalizes. A
direct `do_get` for `%U` therefore reads the week into a state that
is discarded: nothing is stored, and nothing can be, as in this
library.

The hack is not available to this library: it is a GCC extension,
and the tree builds with Clang too and stays C++98. Its presence in
the other library is the clearest statement that the standard's
interface has no portable channel for the state.

## 5. What this library does, as of 2026-10-30

This chapter describes the mechanism that 2.2 names as the defect,
so that the way back in chapter 6 can be read against it.

The walk is the public `get` over a format range in
`include/loc/_time_get.cc`. It keeps the state in locals: the
century and the two-digit year, the last hour and whether it was on
the 12-hour clock, the half of the day, and the week number with its
base. After each `do_get` it reads the piece that call produced
back out of the `tm`: the century from `tm_year` after `%C`, the two
digits after `%y`, the hour after `%I`, the half after `%p`. At the
end of the format it settles what it parsed, whatever the order:
century and year combine, the half of the day adds its twelve hours
to a 12-hour hour and is ignored after a 24-hour one, and a week
with a year and a weekday gives the day of the year by the same
arithmetic as glibc's, with the month and the day of the month from
the leap-adjusted month lengths.

Two rules of its own:

- Each walk settles only what it parsed itself. The walk of a `%x`
  inside a pattern combines its own `%OC%Oy`; a `%C` outside it does
  not reach a `%y` inside, and a year or weekday parsed inside a
  nested `%x` does not count for an outer `%U`. The alternative, one
  state across nested walks, is what glibc has, since it has no
  nesting; here a nested walk is a `do_get` like any other, and the
  rule keeps it so.
- A day outside the year derives nothing and the parse is still
  good. glibc stores a month of -1 or 25 there.

The week number is the one piece the walk cannot read back, since
no member holds it. It crosses the virtual by a handshake: the walk
hands `%U` and `%W` a copy of its working `tm` with `tm_yday` set to
`INT_MIN`, `do_get` stores the week number into that field only when
it finds the sentinel there, and the walk reads it back and restores
its own `tm_yday`. A direct call of `do_get` on a user's `tm` stores
nothing from the week and leaves the `tm` as it was, which the test
pins. The convention lives in the walk and in the `%U` branch of
`do_get`, twenty lines apart, with a comment on each side.

## 6. The alternatives, and the way back

Five channels for the week number were weighed before the handshake,
on the premise that the derivation was wanted. Each fails a rule the
tree holds, named with it.

- Store the week number in the caller's `tm_yday` as a raw value
  and let the walk convert it. A direct caller of `do_get('U')`
  gets a documented lie in a member the standard names "day of the
  year". The walk's locals exist so that no member carries a
  meaning the standard did not give it.
- Parse `%U` in the walk without calling `do_get`. p8 says the walk
  evaluates `do_get` for every conversion specification, and a user
  who overrides `do_get` to handle `%U` would never be called.
- Detect an override and bypass `do_get` when there is none,
  libstdc++'s way. The pointer-to-member comparison is a GCC
  extension outside the language; the tree builds with Clang and
  is C++98.
- Keep the state in the facet object. A facet is shared by every
  stream and thread that uses the locale; `time_get` is an MT-1
  object with no per-call storage, and a mutable member would be a
  data race between two parses on one facet.
- Keep the state in the stream, through `ios_base::iword`. The
  walk would write to the user's stream, and `iword` can allocate
  and throw where the parse itself does not.

The handshake's costs, stated so they are not forgotten: a user
`tm` whose `tm_yday` already holds `INT_MIN` would receive the week
number from a direct `do_get('U')`, and a user override of `do_get`
for `%U` sees the scratch copy rather than the walk's `tm`. Both are
outside any use the standard describes. They are also beside the
point once 2.2 is accepted: the handshake is the sixth channel for a
state `get` should not hold.

### 6.1 The way back

The premise falls first. p8 makes "valid for strptime" a condition
on the format, and p12 obliges `do_get` to assign "those struct tm
members corresponding to" the directive, an empty set for `%U` and
`%W`. A `do_get` that reads the week against its bounds, consumes it
and assigns nothing conforms; the derivation is glibc's behavior and
libstdc++'s choice, not the standard's requirement. With the
derivation optional, the week needs no channel at all. The rest
follows the remark's update model, in five steps:

1. `%U` and `%W`, with their `%O` forms, keep the bound of 53 and the
   two-digit count, consume and validate, and store nothing. The
   derivation and the handshake go.
2. The century and the half of the day move into `do_get`'s update
   of the `tm` it is given. `%C` sets the century and keeps the two
   low digits it finds in `tm_year`, so "%y %C" combines. `%y`
   cannot do the converse: nothing in a `tm` says whether a century
   was parsed, since a zeroed `tm` reads as 1900, so `%y` keeps XSH's
   pivot and sets the whole year, and a `%C` before a `%y` holds only
   inside the pivot's window, 1969 to 2068. `%I`, `%l` and `%OI`
   store the hour modulo 12, as glibc does, within the half of the
   day `tm_hour` already holds; `%p` sets the half of the day of the
   hour it finds. "%p %I" then reads the same as "%I %p". The 2005
   storage, the hour less one with `%p` adding 1 or 13, could serve a
   `%p` after `%I` or after `%H` but not both, and went with it.
3. The walk's locals and its end-of-pattern settle go. `get`
   dispatches, matches literals and skips white space, as p8 says,
   and touches the `tm` for nothing else. An override composes: it
   sees the same `tm` the base would.
4. The rows whose answer changes are re-pinned against the probe:
   "%C %y" on "19 26", 2026 by the pivot where glibc gives 1926;
   "%H %p" with PM, 21 where glibc ignores the `%p` after a 24-hour
   hour; "%Y %C" on "2026 19", 1926 since `%C` keeps the digits of
   the full year, where glibc gives 1900; a caller who did not zero
   the `tm`, whose stale members feed the update. No locale source
   writes the first three shapes outside the pivot's window; the
   fourth is the remark's own warning. Done in e438cc11.
5. This note's chapter 5 becomes the record of the two days the
   combining lived in `get`, and chapter 7 loses the derivation
   items.

What the update model gives up is the cases the walk resolved by
knowing what it had parsed: the two in step 4, and the nesting rule,
which becomes moot since nothing is combined across levels. What it
restores is the virtual's contract, and a `get` that can be read
against p8 line by line.

## 7. What stays open

- glibc's `%C` with a `%Y`. "%C %Y" on "20 1926" gives 1926 here,
  `%Y` setting the whole year, and 2000 in glibc, whose end-of-call
  block takes a century without a two-digit year as the year itself.
  A quirk of the reference, not followed; recorded.
- `%C` on a negative `tm_year`. The kept digits come from `(tm_year
  + 1900) % 100`, negative for a year before 0; no row covers it.
  Queued.
- The derivation of a date from a week. Dropped with the way back;
  a program that parses "%Y %U %w" gets the year and the weekday
  and computes the rest, as it would after `strptime` on a C library
  that does not derive. Recorded, not queued.
- The implied members. "%Y %m %d" leaves `tm_wday` and `tm_yday` as
  the caller had them; glibc completes them. The remark in p14 makes
  either reading conforming. A decision.
- The `E` family. `%EC` without era data reads one digit against a
  bound of zero and fails; `%Ey` and `%EY` are unimplemented; `%EC`
  with eras stores the era's index where a year belongs. p13 permits
  `eofbit` and unspecified members where an era directive cannot be
  resolved, which bounds what a fix owes. Queued.
- The nesting rule. Moot under the update model: a nested `%x` is a
  `do_get` that updates the same `tm`, and nothing is combined
  across levels except through the members themselves.

## 8. Where to look

- `include/loc/_time_get.cc`: `get` over a format range is the
  walk; `do_get` is the per-directive virtual; the `%U` branch and
  the walk's week locals are the two ends of the handshake.
- `src/time_put.cpp`, `__rw_get_timepunct`: the per-directive table,
  members, bounds, digit counts and names.
- `tests/localization/22.locale.time.get.cpp`: the rows for every
  combination named here, against glibc's probed results.
- `doc/notes/ref-conformance.md` chapter 2 and
  `analysis-post-strict-ansi.md` 4.4: the ruled exception.
- glibc `time/strptime_l.c`; libstdc++ `bits/locale_facets_nonio.h`
  and `.tcc`, `__time_get_state` and `_M_extract_via_format`.
