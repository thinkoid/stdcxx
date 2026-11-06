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
| `%C` | the century, two digits | `tm_year` | in `strptime` a year only with `%y`; the library sets the century in `tm_year` and keeps the two low digits it finds there, so "%y %C" combines (6.1) |
| `%y` | the year within the century, two digits, pivoted 69..99 to 19xx and 00..68 to 20xx | `tm_year` | the library pivots and sets the whole year, so a `%C` before it holds only inside the pivot's window; a negative year in the `tm` it keeps (8.4) |
| `%Y` | the full year | `tm_year` | fine alone; it drops a `%y` parsed before it |
| `%G`, `%g` | the ISO 8601 week-based year, four digits or its last two | none | read, validated and discarded; `tm` has no week-based year |
| `%U` | the week of the year, Sunday the first day, 00..53 | none | a day of the year only with a year and a weekday; alone, nothing to assign |
| `%W` | the week of the year, Monday the first day, 00..53 | none | as `%U`; a Sunday is the last day of the previous Monday-week |
| `%V` | the ISO 8601 week of the year, 01..53 | none | as `%U`: read, validated and discarded |
| `%w`, `%u`, `%a`, `%A` | the weekday, a number (Sunday 0, or Monday 1 to Sunday 7 for `%u`) or a name | `tm_wday` | fine alone; in `strptime` also an input to the week derivation, which the library does not do |
| `%j` | the day of the year, 001..366 | `tm_yday` | fine alone; the library derives no day of the year to compete with it |
| `%m`, `%d`, `%e` | the month and the day of the month | `tm_mon`, `tm_mday` | fine alone; together with the year they imply `tm_wday` and `tm_yday`, which nothing names and the library leaves as the caller had them (7) |
| `%I`, `%l` | the hour on the 12-hour clock, 1..12 | `tm_hour` | the hour of the day only with `%p`; stored modulo 12 within the half of the day `tm_hour` already holds |
| `%H`, `%k` | the hour on the 24-hour clock | `tm_hour` | fine alone; a `%p` after it sets the half of the day, where glibc ignores it |
| `%p` | the half of the day, the locale's AM or PM names | none | sets the half of the day of the hour it finds, before or after a `%I` |
| `%Z` | the time zone name | none | consumed, nothing to assign |
| `%z` | the offset from UTC | `tm_gmtoff` where the C library has it | nothing to assign elsewhere |
| `%x`, `%X`, `%c`, `%D`, `%F`, `%r`, `%R`, `%T` | a locale's or a fixed format, a whole pattern | several | a nested walk: what it parsed is settled inside it and does not reach the pattern around it |
| `%E` with `C`, `y`, `Y`, `x`, `X`, `c` | the era forms: an era name, a year within the era, the era's formats | `tm_year` and others | without era data the plain directive; with it, an era has no member, so `%EC` moves the year into the era and `%Ey` reads in the era of the year, and `%EY`'s era is unknown until its name is read; chapter 9 |
| `%O` with a numeric directive | the locale's alternative digits for the value | the plain directive's | a value with no alternative symbol is read as digits; the first alternative symbol may be empty |

## 0. The short version

- A `%U`, `%W` or `%V` week number has no `tm` member, nor has the
  week-based year of `%G` and `%g`. `%C` alone has
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
- This library had the same defect in its own form, from
  2026-10-29 to e438cc11 (2026-10-31): `get` kept the century and
  the half of the day in locals, read them back from the `tm` after
  each `do_get`, and rewrote the `tm` at the end; an uncommitted
  change added the week number. A `get` that is
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
- The way back, taken in e438cc11 on 2026-10-31, is the model the
  standard's remark allows: `do_get` updates the `tm` it is given,
  so the `tm` carries the century and the half of the day between
  calls; `get` dispatches and nothing more; the week number, which
  nothing carries, is consumed and stored nowhere. Chapter 6 has the
  steps and the four limits that came with them.
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

- Directives with no member. `%U`, `%W` and `%V`, the week numbers;
  `%G` and `%g`, the week-based year; `%C`,
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
so that the way back in chapter 6, taken in e438cc11, can be read
against it. `get` is now the code before 59f3a87e but for the
literal match of 6.1. The century and the half of the day were
committed in 59f3a87e and 2829b153. The week locals, the derivation
and the handshake below never were: they describe an uncommitted
working tree of 2026-10-30, which e438cc11 replaced.

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
nothing from the week and leaves the `tm` as it was, which that
tree's test pinned. The convention lives in the walk and in the
`%U` branch of `do_get`, twenty lines apart, with a comment on each
side.

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
follows the remark's update model, in five steps, done in e438cc11:

1. `%U` and `%W`, with their `%O` forms, keep the bound of 53 and the
   two-digit count, consume and validate, and store nothing. The
   derivation and the handshake go.
2. The century and the half of the day move into `do_get`'s update
   of the `tm` it is given. `%C` sets the century and keeps the two
   low digits it finds in `tm_year`, so "%y %C" combines. `%y`
   cannot do the converse. XSH's rule is a property of the whole
   format: the pivot applies "when format contains neither a C
   conversion specifier nor a Y conversion specifier", and a
   `do_get` sees one directive, while nothing in a `tm` says whether
   a century was parsed, since a zeroed `tm` reads as 1900. So `%y`
   keeps the pivot and sets the whole year, except a negative
   century, which `%y` keeps (8.4), and a `%C` before a `%y` holds
   only inside the pivot's window, 1969 to 2068. That is the
   one place the update model departs from XSH's letter, and it is
   p14's limit, not a choice. `%I`, `%l` and `%OI`
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
knowing what it had parsed: the four in step 4, and the nesting rule,
which becomes moot since nothing is combined across levels. What it
restores is the virtual's contract, and a `get` that can be read
against p8 line by line.

A literal of the pattern matches the input's character by
`char_traits<char_type>::eq` of the two characters through the
facet's `ctype::toupper`: C++11 22.4.5.1.1 p8 has the literal match
"in a case-insensitive comparison", and p9 leaves the means
unspecified. The walk of an era's format in 9.8 matches the literals
after `%EC` exactly, and the text before `%EC` case-insensitively,
together with the era name; the era format is the locale's text, not
a caller's pattern, and p8 does not govern it. The pattern's
character is narrowed only to find the `%`, the modifier and the
conversion. A narrowed literal does not serve under
`wchar_t`: a character beyond ASCII narrows to `'\0'` in "C", as
does one with no single byte in a multibyte locale, and any two of
them compared equal. "%Y年" read "2026月" as 2026. The narrow
facet compared bytes and was exact.

## 7. What stays open

- glibc's `%C` with a `%Y`. "%C %Y" on "20 1926" gives 1926 here,
  `%Y` setting the whole year, and 2000 in glibc, whose end-of-call
  block takes a century without a two-digit year as the year itself.
  A quirk of the reference, not followed; recorded.
- `%C` on a negative `tm_year`. The kept digits came from `(tm_year
  + 1900) % 100`, negative for a year before 0. Closed in 503ae88c:
  `%C` keeps the digits without their sign, and a direct
  `get (..., 'C')` on "19" with `tm_year` -2443 pins 43.
- The derivation of a date from a week. Dropped with the way back;
  a program that parses "%Y %U %w" gets the year and the weekday
  and computes the rest, as it would after `strptime` on a C library
  that does not derive. Recorded, not queued.
- The implied members. "%Y %m %d" leaves `tm_wday` and `tm_yday` as
  the caller had them, where glibc completes them. C++11 22.4.5.1.2,
  p12 has `do_get` assign the members "corresponding to a conversion
  directive", and a weekday or day of the year that a date implies
  corresponds to no directive of the pattern. So `get` sets only the
  members its directives name, and the library does not fill the two.
  Decided; the reference page of `get` says so.
- The `E` family. Read since 2026-11-01 by the rules of chapter 9.
- The nesting rule. Moot under the update model: a nested `%x` is a
  `do_get` that updates the same `tm`, and nothing is combined
  across levels except through the members themselves.

## 8. The year before 0: `%C` and `%y`

A side question of the way back, with a sweep of the standards, their
defect trackers and the implementations on 2026-10-31. The library
writes `%C` and `%y` of a year before 0 where an era format falls
back to them, the day before the Buddhist era of th_TH and lo_LA in
the era oracle, and reads them back through `time_get`. 11087a6a made
`time_put` write glibc's pair; the sweep says the current standard
defines a different one.

### 8.1 The texts

| source | `%C` | `%y` | sign | domain |
|---|---|---|---|---|
| C90 7.12.3.5 | none | "the year without century as a decimal number (00-99)" | none | none |
| C99, C23 7.27.3.5 | "the year divided by 100 and truncated to an integer, as a decimal number (00-99)" | "the last 2 digits of the year as a decimal number (00-99)" | none | "If any of the specified values is outside the normal range, the characters stored are unspecified"; `tm_year` has no range |
| POSIX.1-2017 strftime | as C | as C, "[00,99]" | the `+` flag | RATIONALE: "Using negative years results in unspecified behavior" |
| POSIX.1-2024 strftime | C's text, and for the width: "If the year is less than 0 or greater than 9999, the number of characters ... shall be the number of digits and leading sign characters (if any) in the result of dividing the year by 100 and truncating, or two, whichever is greater" | unchanged, "[00,99]" | "a leading <hyphen-minus> character ('-') shall be included if the year is less than zero" under the `+` flag | RATIONALE unchanged from 2017 |
| POSIX.1-2017 strptime | "All but the last two digits of the year {2}"; "A leading '+' or '-' character shall be permitted before any leading zeros" | "The last two digits of the year"; the pivot "When format contains neither a C conversion specifier nor a Y conversion specifier"; the same sign clause | `%C`, `%y`, `%Y` | none |
| POSIX.1-2024 strptime | as 2017, then: "The tm_year member ... shall be set to the number formed by appending the last two digits of the year to these digits, minus 1900. If a y conversion is also performed, the last two digits of the year shall be those processed by the y conversion; otherwise, they shall be 00." | the pivot "If a C conversion is not also performed"; "If a C conversion is also performed, the tm_year member ... shall be set as specified for the C conversion; otherwise, the tm_year member shall be set to the calculated year minus 1900." | as 2017 | none |
| POSIX.1-2024 `date` RATIONALE | "The century number is calculated as the year divided by 100 and truncated to an integer"; "%Ey and %y can then be viewed as the offset from %EC and %C, respectively." | | | |
| C++20 chrono, [time.format] | "The year divided by 100 using floored division" | "The last two decimal digits of the year" | | signed `year` type |
| C++26 after LWG 3831 and 4022 | floored, "0 is inserted before the single digit (after the sign, if present)" | "The last two decimal digits of the year, regardless of the sign of the year"; `format("{:%C %y}", -1976y)` is "-20 76" | on `%C` only | parse of "-20 76" with "%3C %y" gives -1976 |

Two readings of the 2024 strftime text coexist: the normative width
sentence speaks of years below 0 and of truncating, while the
rationale still calls such years unspecified. The strptime text
composes by appending digits: "-5" and "43" make "-543"; the floored
pair "-6" and "57" would make "-657".

### 8.2 The defect reports

- Austin Group 472, "strftime %C does not account for sign character",
  accepted, applied in Issue 8; its response: "The standard is unclear
  on this issue, and no conformance distinction can be made between
  alternative implementations based on this." Its text is the width
  sentence above. 1184 is a duplicate; 466 is `date`'s `%C` past 9999.
- Austin Group 1727, "strptime() spec needs updates to deal with other
  changes", accepted, applied in Issue 8: the source of the `tm_year`
  sentences. Its discussion confirms years before 0 are in scope:
  "Doesn't the spec for strftime() allow for years beyond 9999 (and
  even before 0)?" "Yes it can".
- Austin Group 1969, "strptime() %y with a 1 digit field", accepted
  for TC1 on 2026-04-16: `%y` becomes "The last two digits of the
  year, for years greater than 9, or a value to be added to the year
  obtained from a C conversion (if any)", with "[-9,68]" added to
  2000. Additive composition gives -457 for "-5" and "43" where Issue
  8's appending gives -543. The two texts are not reconciled.
- WG14: no defect report or paper on `%C`, `%y` or a negative
  `tm_year`. N715 (1997) brought `%C` into C99 from the POSIX.2 `date`
  utility with its "(00-99)". The C99 Rationale says the specifiers
  were "chosen according to existing practice to cover long-standing
  POSIX practice". DR 326's discussion treats `tm_year = -999 - 1900`
  as well-defined for the C99 text.
- WG21 LWG 3831, "Two-digit formatting of negative year is ambiguous"
  (2022, in the working paper 2026), and 4022 (2023, ready 2026).
  Before them MSVC printed "76 24" and libc++ "76 76" for -1976. The
  case for the floor in that discussion: "%C can't be truncated
  division, otherwise the years [-99, -1] would map to the same
  century as the years [0, 99]"; against the floored remainder: "24
  just doesn't cut it"; and "The negative sign need only be in one
  place, and it has to be in %C to prevent ambiguity." The author
  called the matter one where "POSIX strftime / strptime doesn't
  handle negative years in this department, so this is an opportunity
  for an extension."

### 8.3 The implementations, for -543

| implementation | `%C` | `%y` | reads a signed `%C` | source |
|---|---|---|---|---|
| glibc 2.44 strftime | "-6" | "57" | strptime rejects a sign on `%C`, `%y` and `%Y` | `year / 100 - (year % 100 < 0)`; `(tm_year % 100 + 100) % 100` |
| gnulib nstrftime, coreutils `date` | "-5" | "43" | | ChangeLog 2005-03-09: "Make %Y always equivalent to %C%y, and similarly for %G and %g." |
| musl | "-5" | "43" | yes, additive: "-6 57" and "-5 -43" give -543, its own "-5 43" gives -457 | `(1900LL+tm->tm_year) / 100`; `val = -val` for `%y` |
| FreeBSD libc, tzcode | "-5" | "43"; "-0" for `%C` of -1..-99 | no sign | `_yconv` |
| libstdc++ 16 chrono format | "-06" | "43" | the sign is dropped in parse: "-06 43" gives 643 | `_M_C_y_Y`, "3831" cited |
| libc++ chrono format | "-6" | "43" | no parse | "floored division" in the source |
| MSVC STL chrono | "-06" | "43" | composes "-06 43" to -543 | `_Decompose_year`, LWG-3831 cited, 2026-04 |
| Ruby, Python | "-6" | "57" | | through glibc |
| Rust chrono | floored | floored | | documented: "100 BCE (year number -99) will print -1 and 99" |
| Java `uu` | | "43" | | "the rightmost two digits" |

History: glibc's floor dates from 1996, "%C, %y: Handle negative years
portably", after a 1995 "Use year modulo 100 for %y". On the tz list
in 2004 the same maintainer argued "it's least surprising if %C is
consistent with %y, i.e., if %C is (year - (year mod 100)) / 100",
while a WG14 member replied that a system writing `%C` "-2" and `%y`
"34" for -234 "would, I think, conform"; tzcode then shipped
truncation with "-0" for the years -1 to -99, and in 2005 gnulib
left glibc's convention for truncation with the note above. No glibc
bug concerns `%C` or `%y` of a negative year.

### 8.4 What follows

- Correct, by the current standard, is Issue 8's pair: `%C` is the
  truncated century with the sign, `%y` the last two digits without
  one, and a reader composes them by appending digits. 11087a6a wrote
  glibc's floored pair for `time_put`; what it replaced, "-05" and
  "-43", was a signed `%y`, wrong too. b54acca9 makes `time_put`
  write Issue 8's pair and the era oracle record glibc's as a known
  difference; 503ae88c makes `time_get` read a sign before `%C`, `%y`
  and `%Y` and compose by appending, under the update model: `%C`
  keeps the digits of the year it finds, `%y` keeps a negative
  century it finds, since a negative year in the `tm` is proof that a
  century was parsed, and pivots after any other.
- The `%y` field's "[00,99]" is not inconsistent after all: it is the
  unsigned half of a pair whose sign rides on `%C`, which is also the
  human reading, five centuries and forty-three years before 0.
- Three seams, settled in b54acca9 and 503ae88c. The years -1 to
  -99 truncate to the century 0, which has no sign: `%C` writes "-0"
  there, as tzcode does, within Issue 8's widths and readable by its
  appending sentence, "-043" for -43. Two variants were tried and
  rejected: the letter's "00", whose "0043" is the year 43 under any
  reading; and the sign on `%y`, "00-43", whose "-43" is three
  characters past `%y`'s {2}, so that no conforming reader takes it,
  and exempting the sign from the width would make the reader
  disagree with the standard on a defined input, "-43" alone being
  -4 by the width and by the premise of Austin Group bug 1969, to
  rescue an undefined one. This library's reader cannot take "-0"
  either: -0 is 0 in `tm_year`, so the `%y` after it pivots and
  "%C%y" of -1 to -99 does not read back here; `%Y` carries those
  years. The years below -999 need a field width the facet has no
  syntax for. TC1's additive `%y` contradicts Issue 8's appending for
  a negative century; the published text is followed. A signed `%C`
  with a signed `%y` is defined by nobody; the reader takes it by the
  C identity, the writer never emits it.
- The sign is one of the characters a field scans, Issue 8's "the
  maximum number of characters scanned" and its application usage on
  field widths, and the premise of bug 1969: `%C` and `%y` read two,
  `%Y` four, the sign included. "%C%y" reads "-543" as "-5" and
  "43"; "+19" under `%C` is the century 1 with the 9 left.
- glibc's floored pair is unreadable by a conforming reader: "-6"
  and "57" append to -657. The era oracle counts glibc's pair as a
  known difference on the day before the Buddhist era.
- C++26's chrono is a different convention for a different API, the
  floored century with the literal two digits and its own
  composition; it is not the model for `time_put`.

The sweep's sources and probe programs are kept with the day's
handover material outside the tree.

## 9. The era conversions: `%EC`, `%Ey` and `%EY`

An era record of LC_TIME is a direction, an offset, a start date, an
end date, a name and a year format. ja_JP's `+:2:2020/01/01:+*:令和:%EC%Ey年`
numbers 2020 as 令和 2 and runs to the end of time. The offset is
"the number of the year closest to start_date", and `+` means "the
years closer to start_date have lower numbers". The end date may
come before the start: zh_TW's 民前 starts on 1911-12-31 and counts
up into the past. Two records may share a name: 令和 has one for
2019 alone, whose format `%EC元年` writes the first year as 元年, and
one from 2020; the two are one era. Until 2026-11-01 the reader had
none of this. `%EC`
stored the matched era's index in `tm_year`, `%Ey` had no reader,
and `%EY` matched the year formats as if they were names. The rules
below were decided on 2026-11-01 and are what `do_get` does since.

### 9.1 The texts

- C++11 22.4.5.1.2, p12: an invalid directive "leaves the object
  pointed to by t unchanged" with `failbit`. `%Ed` and `%EM` are
  invalid: `E` modifies only `c`, `C`, `x`, `X`, `y` and `Y`.
- p13: for an `E` directive, "when the function is unable to
  unambiguously determine some or all struct tm members from the
  input sequence", it sets `eofbit`, and "the values of those struct
  tm members are unspecified and may be outside their valid range".
- p14: whether a call updates the `tm` or overwrites its members is
  unspecified. The library updates it (chapter 6).
- XSH strptime: `%EC` is "The name of the base year (period) in the
  locale's alternative representation", `%Ey` "The offset from %EC
  (year only)", `%EY` "The full alternative year representation";
  and "If the alternative format or specification does not exist in
  the current locale, the behavior shall be as if the unmodified
  conversion specification were used."

### 9.2 Two failure contracts

p12 and p13 are two contracts. A directive that is not one, `%Ed`,
fails with `failbit` and leaves the `tm` as it was. A valid `E`
directive that cannot settle its members sets `eofbit`, never
`failbit`: an unknown era name, a year outside its era, no digits
for `%Ey`, an era format the input does not follow. The members are
unspecified. `%EC`, a lone `%Ey` and `%EY` leave the `tm` as it was
on failure: the library stores what an era directive read only when
its whole format matched, so a `%EY` whose format fails after its
`%Ey` stores nothing. XSH's fallback applies where the alternative
"does not exist".
A locale with era records has the alternative, so a mismatch there
is not a fallback: "19" under `%EC` in ja_JP sets `eofbit` and reads
nothing, where glibc reads the century.

Under `get` over a format, `eofbit` from a directive before the last
ends the walk with `eofbit` alone. C++11 22.4.5.1.1, p8 ends the loop
at the first of its conditions, and `err != goodbit` comes before
`s == end`, so the walk adds no `failbit` and stores the members read
so far: "%Y-%m" and "%Y " on "2026" store the year with `eofbit`.
The walk sets `eofbit | failbit` itself only where the input ends
before a literal of the pattern.

### 9.3 No era data

XSH's sentence decides the locales without eras. "C", en_US and the
test's other locales read `%EC`, `%Ey` and `%EY` exactly as `%C`,
`%y` and `%Y`, sign and pivot included. `%Ec`, `%Ex` and `%EX` read
the plain `%c`, `%x` and `%X` where the locale's `era_d_t_fmt`,
`era_d_fmt` or `era_t_fmt` is empty or missing: ar_SA defines an
empty `era_d_fmt`, ja_JP no `era_t_fmt`, zh_TW none of the three.
That is also what `time_put` writes for them. Before, the empty
format parsed as nothing: good, no character read, no member set.

### 9.4 State

p8 has `get` evaluate `do_get` once per directive, and chapter 6 made
the `tm` the only carrier between two calls. The era state obeys the
same rule. A record found while reading may live only inside the one
`do_get` that found it, inside the walk of `%EY` over a year format.
Nothing outlives the call: no latch in `get`, no member of the facet,
no thread or static storage. `%EC` and `%Ey` written as two
directives combine through `tm_year` alone.

### 9.5 The era of a date, for the reader

An era is every record with its name. ja_JP's 令和 is its 元年
record, 2019-05-01 to 2019-12-31 with offset 1, and its record from
2020-01-01 with offset 2. The two make one span, and their offsets
agree: both make 2019 year 1 and 2020 year 2. The data must keep
that agreement, one era and one count, and then any record of the
name serves for the year arithmetic; the reader takes the first in
order. The era's span runs from the earliest bound of its records to
the latest; a bound at either end of time stays infinite. Its first
year is the start year of the record with the smallest offset: 2019
for 令和, 1989 for 平成.

`time_put` finds the era of a date by comparing it with each record's
bounds as a whole date, in the order of the records. The reader uses
a sibling of that lookup, over eras. A year strictly inside an era's
span lies in the era whatever `tm_mon` and `tm_mday` hold. Only a
date in the span's first or last year is compared whole, against the
bound of the record that holds that end, as the writer does, so that
2019-06-01 is 令和 and 2019-04-01 平成. A day below 1 is taken for
the first of the month there. A zeroed `tm` is January 0, which a
whole-date comparison puts before an era that begins on January 1;
the reader must not. Eras are tried in the order of their first
records, forward and backward alike.

### 9.6 `%EC`: the base-year rule

`%EC` names an era and nothing more; `tm` has no member for an era.
What it can do is move the year into the era it names, so that a
`%Ey` before or after it finds the era by the year.

- The name is matched against every record's name. The era named is
  every record with that name, by 9.5.
- When the year in the `tm` lies in that era, by the lookup of 9.5,
  the year stays.
- Otherwise the year keeps its number. Its number n is its year in
  the era it lies in, or the named era's smallest offset where it
  lies in none. The year becomes start + (n - offset) times the
  direction, in the era's first record, where the lookup puts that
  year in the era.
- Otherwise the year becomes the era's first year. Where the month
  and the day in the `tm` put that year outside the era, it becomes
  the year next to it inside the span: 平成 begins on 1989-01-08, so
  a zeroed `tm`'s January 0 of 1989 lies in 昭和 64, and 平成 takes
  1990.

A zeroed `tm` is 1900, 明治 33 in ja_JP. 平成 33 would be 2021, which
lies in 令和: re-based without the lookup, "%EC %Ey" on "平成 8"
would read 令和 8, 2026. Left at 1989, it would read 昭和 8, 1933,
since the `%Ey` after it finds 昭和 in January 1989. The rule reads
1996.

So "%EC %Ey" on "令和 8" is two calls: 令和 33 is 2051, inside 令和;
the 8 is then 2026. "%Ey %EC" on "8 令和" reads 明治 8, 1875, and
令和 keeps the 8: 2026. "%EC" alone on "令和" gives 2051, 令和 33:
a lone `%EC` names the era and leaves the number to the `tm`.

### 9.7 `%Ey`

`%Ey` reads up to four digits, 0 to 9999, with no sign, after white
space. glibc reads the same field. Alternative digits under `%OEy`
are out of scope. The era is the one the date in the `tm` lies in,
by 9.5, and the year is start + (n - offset) times the direction in
the era's first record, the inverse of `time_put`'s arithmetic. No
era sets `eofbit`. A year outside the era's span sets `eofbit`, as
32 is outside `%d`'s bounds: 平成 40 would be 2028, past the end of
平成. A bound at either end of time is no bound. The backward eras
follow from the direction: 民前 2 is 1910 under zh_TW, and the test
locale's bar, which counts down from 7 in 1926, reads 1 as 1920 and
8 as past its end.

The span is the era's, not one record's. `time_put` writes "%EC %Ey"
of 2019-06-01 as "令和 01", the 元年 record's year 1. The record from
2020 numbers 2019 as 1 too, and 2019 lies in the span of 令和, so the
reader reads 2019. p13 asks whether the input determines the members
unambiguously, and 令和 1 names one year. `%EY` reads the same date
as 令和元年.

### 9.8 `%EY`

`%EY` is a record's year format, and the record is unknown until its
name is read. glibc tries each record's format in turn and rewinds;
an input iterator cannot. Each record's format is cut at its `%EC`,
and its text before the `%EC` with the record's name appended makes
one string per record: the names in every locale source, "foobar"
for the test locale's "foo%EC %Ey". The strings are matched in
parallel by `__rw_match_name`. The rest of the format of the record
found is then walked inside the call, with that record as the walk's
state: a `%Ey` there resolves in that record's era, not by the
`tm`, and the literals must match character for character.
"Heisei 1 Nen" takes the format of the test locale's record from
1990 and reads 1989, inside the span of Heisei. A format without a
`%Ey` sets the record's first year.

Two records with one name leave two rests: "%Ey年" and "元年" in
ja_JP, " %Ey Nen" and " Gannen" in the test locale. White space that
begins both is skipped in each and in the input. Then one character
decides: a digit takes the rest that begins with `%Ey`, any other
character the rest that begins with it. "令和8年" is 2026 and "令和元年"
2019. Any other shape sets `eofbit`: a format with no `%EC`, an
empty format among them, a rest that one character does not decide,
a directive other than `%Ey` in a rest. The empty format is a shape
no locale can have: `localedef` rejects an era record without a
format, "missing era format", as glibc's does, so the reader meets
one only in a hand-built database. `time_put` writes nothing for
its `%EY`; the two sides agree and the case is not pinned.

### 9.9 The zeroed `tm`

A zeroed `tm` is 1900. Under ja_JP that is 明治 33, under th_TH พ.ศ.
2443, under zh_TW 民前 12, and under the test locale bar -19. So a
lone `%Ey` on a zeroed `tm` is determined, by the letter of 9.7: "8"
is 明治 8, 1875, under ja_JP, and p13 does not apply. The caller who
parses "%Ey" alone gets a year in whatever era the `tm` lies in; the
standard's interface carries no era, and glibc's latch is what
replaces it there.

### 9.10 The rows

The real locales, read by a probe against the build's locale
databases, narrow and wide alike, from a zeroed `tm`:

| format | input | locale | year | state |
|---|---|---|---|---|
| `%EC %Ey` | 令和 8 | ja_JP | 2026 | eof |
| `%Ey %EC` | 8 令和 | ja_JP | 2026 | good |
| `%EC` | 令和 | ja_JP | 2051 | good |
| `%EC` | 平成 | ja_JP | 1990 | good |
| `%EC %Ey` | 平成 8 | ja_JP | 1996 | eof |
| `%EC %Ey` | 令和 01 | ja_JP | 2019 | eof |
| `%EC%Ey年` | 平成1年 | ja_JP | 1989 | good |
| `%EC %Ey` | พ.ศ. 2569 | th_TH | 2026 | eof |
| `%EC %Ey` | 民前 2 | zh_TW | 1910 | eof |
| `%EY` | 令和8年 | ja_JP | 2026 | good |
| `%EY` | 令和元年 | ja_JP | 2019 | good |
| `%EC` | 19 | ja_JP | 1900, nothing read | eof |
| `%EC %Ey` | 平成 40 | ja_JP | 1990, left by `%EC` | eof |
| `%EC` | 19 | C, en_US | 1900, as `%C` | eof |
| `%Ed` | 1 | any | unchanged, nothing read | fail |

The round trip of `time_put` through `time_get` holds for `%Ec`,
`%Ex` and `%EY` under ja_JP, th_TH, lo_LA and zh_TW on 2026-10-28,
2019-06-01, 1989-03-01 and 1910-06-01, 96 rows, narrow and wide.
`22.locale.time.get` pins the same rules in its generated locale:
Heisei, its Gannen record, Showa and the backward bar, and "C" for
the locale without eras.

### 9.11 What glibc does differently

glibc 2.44 is the reference for what each directive reads, not for
these rules. The differences, as differences:

- A mismatch with era data falls back. glibc's `%EC` on "19" reads
  the century, its `%Ey` falls back to `%y`, its `%EY` to `%Y`. Here
  that is `eofbit`, by XSH's "does not exist".
- glibc latches. `decided` and `era_cnt` hold the era and the choice
  of representation for the rest of the call, and an end-of-call
  block combines the era with the year. The update model has no
  latch; the `tm` is the only carrier.
- glibc's `%EC` alone gives the first year of the first record with
  the name, 2020 for 令和. Here the year keeps its number where the
  era has it.
- glibc's `%Ey` with no era fixed takes the first record whose span
  holds the number: a lone "26" is 2044 under ja_JP. Here the era is
  the `tm`'s.
- glibc's span test multiplies by the calendar's direction and
  rejects every year past the offset of a backward era: it writes
  民前12年 and cannot read it. Here the direction is the record's.
- glibc bounds a year by each record alone. Its `%EC%Ey` cannot
  read "平成01", which it writes: 平成 1 lies outside the record from
  1990 that its `%Ey` takes. Its `%EY` reads 平成元年 by trying each
  format. Here the bounds are the era's span: `%EC %Ey` reads
  "平成 01" as 1989 and "令和 01" as 2019.

## 10. Where to look

- `include/loc/_time_get.cc`: `get` over a format range is the
  walk; `do_get` is the per-directive virtual.
- `src/time_put.cpp`, `__rw_get_timepunct`: the per-directive table,
  members, bounds, digit counts and names.
- `src/era.cpp`: the era lookups of the writer and the reader, the
  year arithmetic, and `__rw_set_era` and `__rw_set_era_year` behind
  `%EC` and `%Ey`; `__rw_read_era` in `include/loc/_time_get.cc` is
  the reader of the three directives and the walk of `%EY`.
- `tests/localization/22.locale.time.get.cpp`: the rows for every
  combination named here, against glibc's probed results.
- `doc/notes/ref-conformance.md` chapter 2 and
  `analysis-post-strict-ansi.md` 4.4: the ruled exception.
- glibc `time/strptime_l.c`; libstdc++ `bits/locale_facets_nonio.h`
  and `.tcc`, `__time_get_state` and `_M_extract_via_format`.
