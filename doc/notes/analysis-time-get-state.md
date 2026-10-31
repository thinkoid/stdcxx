# The state a `time_get` parse carries across `do_get`

`time_get` parses a format one conversion directive at a time, each
through the virtual `do_get`, and a `do_get` call can hand back only
a `struct tm`. Some directives have no member in that structure, and
some mean something only together with others. This note records
the conflict, why the interface stays, what the C library and
libstdc++ do with the same problem, what this library does, and the
alternatives that were weighed for the week number in October 2026.

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
  overridden. This library keeps the state in locals of the walk,
  reads each piece back from the `tm` after the `do_get` that
  produced it, and settles at the end of the format.
- The week number is the one piece with no member to read it back
  from. It crosses the virtual by a convention private to the walk:
  a scratch `tm` whose `tm_yday` is `INT_MIN` on the way in and the
  week number on the way out.
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

## 5. What this library does

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

## 6. The alternatives for the week

Five were weighed before the handshake. Each fails a rule the tree
holds, named with it.

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
outside any use the standard describes.

## 7. What stays open

- The derivation and explicit fields. glibc keeps an explicit `%m`,
  `%d` or `%j` over what the week implies; the walk has to do the
  same, by remembering what it parsed as it does for the century and
  the hour. Queued.
- The implied members. "%Y %m %d" leaves `tm_wday` and `tm_yday` as
  the caller had them; glibc completes them. The remark in p14 makes
  either reading conforming. A decision.
- The `E` family. `%EC` without era data reads one digit against a
  bound of zero and fails; `%Ey` and `%EY` are unimplemented; `%EC`
  with eras stores the era's index where a year belongs. p13 permits
  `eofbit` and unspecified members where an era directive cannot be
  resolved, which bounds what a fix owes. Queued.
- The nesting rule. A year or weekday inside a nested `%x` does not
  reach an outer week. Recorded above as the rule; glibc would
  derive. A decision if a format in the sources turns out to need
  it.

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
