# Non-x86 platforms and compilers in stdcxx

As of 2026-09-14 (ec67fbd8).

## 0. tl;dr

POWER remains an actively developed enterprise platform. SPARC servers
remain available, but with a published retirement schedule from Fujitsu.
PA-RISC and Alpha are discontinued hardware families. MIPS and PowerPC
survive in embedded products, which does not preserve the workstation
environments historically associated with their names. Itanium is also
discontinued, although compiler and operating-system lifetimes differ
from the hardware's lifetime.

The compiler picture is similarly mixed: IBM has an active successor
in Open XL, Oracle still offers Developer Studio, and GCC can outlive
the hardware it targets. Most other vendor toolchains in this tree
belong to legacy environments. A surviving processor name or compiler
brand does not establish a usable stdcxx platform: the architecture,
operating system, compiler and runtime must still work together.

The subsequent source assessment supports retiring the old platform
integrations in favor of GCC, Clang and Visual Studio on x86/x86-64,
targeting Linux and Windows. This is a project scope decision, not a
claim that every retired platform is dead. GCC has a measured build
baseline; Clang has a reproduced build failure, and current Visual
Studio support remains unverified. No source trimming was performed.
Chapters 8–10 give the removal scope, evidence and recommended sequence.

## 1. Scope and terminology

The scope is the checked-out stdcxx tree: documented ports, configuration
files, architecture-specific implementation code, and weaker references
that need to be distinguished from ports. x86 and x86-64 are excluded.

Three different claims need to be kept separate:

- **New processor development:** a vendor is introducing new generations.
- **New hardware sales:** a vendor still supplies an existing design.
- **Legacy availability:** used machines, refurbished equipment, spares,
  and support for installed systems remain available.

A product listing establishes an offering, not whether a foundry is
currently fabricating more wafers. This survey concerns commercial
platform availability; it does not audit semiconductor production.

## 2. Platform overview

| Family | State | Evidence |
|---|---|---|
| IBM POWER | Active processor and server development. IBM introduced Power11 servers in July 2025, supporting AIX, IBM i and Linux. | [IBM Power11 announcement][power11] |
| Embedded PowerPC | Existing products remain available, including NXP's PowerPC-based QorIQ T2080 and its development board. This is a different market from IBM's enterprise POWER systems. | [NXP T2080][t2080] |
| Sun / SPARC servers | New Fujitsu SPARC M12 systems remain on the published sales roadmap through August 2029. Final shipments are scheduled for November 2029 and support through November 2034. | [Fujitsu SPARC roadmap][fujitsu] |
| Embedded SPARC | Commercial parts remain available for space applications, including Gaisler's radiation-hardened GR740, with four LEON4 SPARC V8 cores. | [Gaisler GR740][gr740] |
| HP PA-RISC | Discontinued. The last order date for HP 9000 systems was December 2008. Existing machines and replacement equipment are legacy hardware. | [Perl's HP-UX platform documentation][parisc] |
| DEC Alpha | Discontinued. HP stopped accepting new AlphaServer orders on 27 April 2007; upgrades and add-ons continued through April 2008. | [Preserved HP historical account][alpha] |
| Intel Itanium / IA-64 / IPF | Discontinued. Intel's final-generation Itanium 9700 discontinuance notice set the last order date to January 2020 and the last shipment date to 29 July 2021. | [Intel PCN 116733-00][itanium] |
| MIPS instruction set | Survives in embedded products, including Microchip's MIPS32-based PIC32MZ-W1 microcontrollers. | [Microchip PIC32MZ-W1][pic32] |

## 3. What the tree actually touches

The architecture sections of [the configuration header][config] and
the hardware identification in [the test driver][driver] identify six
non-x86 families: Alpha, Itanium, MIPS, PA-RISC, POWER/PowerPC and SPARC.
The driver distinguishes PA-RISC 1.0/2.0, SPARC V8/V9, POWER3/4/5 and
several PowerPC implementations. These are variants within the families,
not additional independent platforms.

[The original README, section 10][readme], supplies the historical
compiler/OS pairings below. Its statements about tests describe that
release, not a verification performed in 2026. A configuration file or
preprocessor branch is weaker evidence than a recorded successful port.

| Architecture and OS | Compiler named by stdcxx | Historical evidence in this tree |
|---|---|---|
| POWER/PowerPC, AIX | IBM VisualAge C++, subsequently XL C/C++ (`xlC`); GCC also has AIX configuration | README 10.8 records VisualAge 5/6 and XL 7/8/9, with tests on AIX 5.2/5.3. [vacpp.config][vacpp] and [_atomic-xlc.h][atomic-xlc] preserve compiler-specific machinery. |
| POWER/PowerPC, Linux | IBM XL C/C++; GCC | [vacpp.config][vacpp] explicitly has a non-AIX Linux branch. This does not establish a current Linux/POWER test result. |
| SPARC, Solaris | Sun C++ / Sun Studio; GCC; Comeau C++; EDG eccp | README 10.15 records Sun C++ 5.3–5.9; 10.6 records a GCC/SPARC test. Sections 10.3 and 10.5 describe Solaris ports for Comeau and eccp but do not identify the CPU in every entry. [sunpro.config][sunpro] and [SPARC atomics][atomic-sparc] provide explicit SPARC evidence. |
| PA-RISC, HP-UX | HP aCC; GCC | README 10.7 records aCC 3.x and HP-UX 11.x. [acc.config][acc], [_config-acc.h][config-acc] and [PA-RISC assembly][parisc-asm] retain the port. |
| Itanium, HP-UX | HP aCC 5.x/6.x | README 10.7 calls this IPF and records aCC 5.57, 6.00 and 6.15. [IA-64 assembly][ia64-asm] is present. |
| Itanium, Linux | Intel C++ (`ecc`/`icc`); GCC | README 10.9 includes IA64 in the Intel port; 10.6 records GCC 3.4.4 on Linux/IA64. [icc.config][icc] explicitly handles the old IA-64 compiler executable name. |
| Itanium, Windows | Microsoft Visual C++; Intel C++ | README 10.12 and 10.9 include IA64 in their port descriptions. Their listed release-test combinations do not establish an IA64 test run. |
| Alpha, Tru64 UNIX / OSF1 | DEC / Compaq / HP C++ (`cxx`); GCC | README 10.4 records Compaq/HP C++ 6.5 and 7.1 tests, including an unresolved-symbol caveat. [osf_cxx.config][osf] and [_atomic-deccxx.h][atomic-dec] retain compiler-specific support. |
| Alpha, Linux | GCC | [The configuration header][config] explicitly says its Alpha treatment applies to both Tru64 UNIX and Linux. No Linux/Alpha release-test entry was found in README 10.6. |
| MIPS, SGI IRIX | SGI MIPSpro C++ (`CC`) | README 10.13 describes MIPSpro 7.3–7.5 on IRIX 6.5 and records a 7.41 test. [mipspro.config][mipspro] and [_atomic-mipspro.h][atomic-mips] retain the port. |
| MIPS, Siemens/Nixdorf Reliant UNIX (SINIX lineage) | Siemens CDS++ (`CC`) | [reliant_cds.config][reliant] explicitly names this environment; the configuration header has `SNI` branches. README 10.14 already says the port is not maintained. The MIPS/Reliant pairing is also documented in [Infor's installation guide][reliant-platform]. |

## 4. Compiler availability and succession

**IBM:** VisualAge and classic XL are the historical tools named in the
tree. IBM now offers Open XL C/C++ for AIX, based on LLVM and Clang.
This is an active vendor compiler line, but the changed implementation
means the old `xlC` integration cannot be assumed to work unchanged.
[IBM Open XL][openxl]

**Sun/Oracle:** Sun C++ became part of the Studio product line. Oracle
still offers Developer Studio 12.6 as its current downloadable release.
Continued availability of that release is not evidence of a new compiler
generation or of compatibility with the library's old Sun C++ branches.
[Oracle downloads][studio]

**HP aCC:** the PA-RISC and Itanium branches belong to HP-UX, not to
OpenVMS. They are legacy toolchains associated with retired hardware.
HPE ended standard support for HP-UX 11i v3 and Integrity i4/i6 on
31 December 2025. Its support matrix allows mature software support
without sustaining engineering on Integrity through at least the end
of 2028. These are platform support dates, not separately verified aCC
sales or compiler-maintenance deadlines.
[HPE notification][hpux-eos], [HPE support matrix][hpux-matrix]

**DEC/Compaq/HP C++:** the tree's documented port is the Tru64 UNIX
compiler, not every product ever sold under the HP C++ name. No current
Tru64 compiler offering was established by this survey. VSI separately
offers and documents C++ for OpenVMS, including Alpha and Itanium;
that is an OS-specific continuation, not a new Alpha processor or a
Tru64 revival. [VSI C++][vsi-cxx]

**SGI MIPSpro and Siemens CDS++:** treat the IRIX and Reliant UNIX
toolchains as legacy environments. No current new-system or compiler
offering for either environment was established here. Availability of
MIPS-based microcontrollers does not restore either compiler/OS pairing.
The Reliant port was already unmaintained in the library's README.

**Intel and Microsoft on Itanium:** these are historical IA-64 compiler
targets, distinct from those vendors' continuing products for other
architectures. Intel's porting guide identifies 11.1 as the retained
IA-64 compiler and says Compiler XE 2016 does not target IA-64. The
library's Microsoft entry is likewise a historical port declaration;
this survey establishes no current MSVC IA-64 offering.
[Intel porting guide][intel-guide]

**GCC:** compiler support can outlive hardware sales. GCC 15.2's manual
still documents Alpha, HPPA, IA-64, MIPS, RS/6000 and PowerPC, and SPARC
options. An architecture backend is not a promise that every historical
OS ABI, vendor assembler or C library remains supported. Check the
specific GCC release and target triple before selecting a build host.
[GCC 15.2 manual][gcc-manual]

### 4.1. Other compiler names in the README

These entries complete the compiler inventory without inventing
additional CPU ports:

| Compiler | What stdcxx records | Availability qualification |
|---|---|---|
| Comeau C++ | Solaris port, version 4.2.42 or later; this release not tested; [como.config][como] remains. | Historical integration; no current vendor offering verified here. |
| EDG eccp demo | Versions 2.45.2–3.10 on Linux and Solaris; [eccp.config][eccp] remains. | Distinguish the old demo driver from EDG's licensed front end. EDG published version 6.8 documentation in November 2025. A contemporaneous standards-meeting report records plans to wind down and open-source the front end within a year; completion of that transition was not verified here. [EDG manual][edg], [meeting report][edg-transition] |
| Apogee C++ | README 10.1: port not maintained; an `__APOGEE__` section remains in the configuration header. | Historical reference; no current offering verified here. |
| KAI C++ | README 10.10: port not maintained. | Historical reference; no current offering verified here. |
| Metrowerks CodeWarrior | README 10.11: not ported. | The CodeWarrior name survives in NXP's embedded tools, including Power Architecture. That is not evidence of an stdcxx port. [NXP CodeWarrior][codewarrior] |
| Borland C++ | README 10.2: not ported. | No non-x86 port established; excluded from the hardware inventory. |

## 5. Boundaries and incidental references

- **Darwin/Mac OS X:** real OS-specific configuration exists in
  [gcc.config][gcc-config] and [_mbstate.h][mbstate]. The README's
  explicit Mac test is on an excluded architecture. Its general Darwin
  port statement does not independently establish a PowerPC Mac test,
  much less a modern Arm Mac port.
- **OpenVMS:** [a floating-point test][float-test] mentions `__VMS` in
  an exclusion condition. This is not a documented successful OpenVMS
  port. VSI's Alpha/Itanium compiler availability is relevant context,
  not evidence that the library builds there.
- **VAX:** the DEC character-map files cite a VAX/VMS manual. These are
  character-encoding provenance, not VAX implementation support. There
  is no basis here for adding VAX as a library port.
- **IBM mainframes:** the earlier hardware overview included
  z/Architecture because IBM still makes it (z17 was announced in 2025).
  The surveyed tree supplies no explicit S/390 or z/Architecture port
  evidence. Retain it as background only, separate from POWER; do not
  infer a port from the shared IBM vendor name. [IBM z17][z17]
- **Arm/AArch64, RISC-V, m68k, SuperH and other architectures:** no
  explicit port evidence was found in the surveyed configuration,
  implementation or README. Generic GCC support or an OS name alone
  does not establish that stdcxx touched each architecture that compiler
  or OS supports. RISC-V appears in this survey only to explain MIPS
  the company's current direction.
- **OS/2 and x86-only environments:** excluded from the platform
  inventory as requested, even where the tree retains OS-specific code.

## 6. Names that can mislead

**Sun and SPARC are not interchangeable.** Sun was the system vendor;
SPARC is the processor architecture. Fujitsu's M12 sales schedule is
clear evidence of continuing new SPARC server availability. Its roadmap
is subject to change and should not be read as a promise of new CPU
generations. [Fujitsu][fujitsu]

Oracle still publishes technical material for its SPARC T8 and M8
servers. That documentation alone does not establish current orderability
or manufacturing status. This survey did not establish an exact current
Oracle sales cutoff. [Oracle SPARC technical resources][oracle]

**MIPS the company and MIPS the instruction set now have different
directions.** The company's current processor offerings use RISC-V.
Microchip's MIPS-based microcontrollers show that the older instruction
set still has commercial products. A newly announced processor carrying
the MIPS company name therefore need not execute MIPS instructions.
[MIPS product announcements][mips], [Microchip][pic32]

**An embedded survivor does not preserve a Unix workstation platform.**
Gaisler's SPARC processors and Microchip's MIPS microcontrollers are
useful current products, but their availability says nothing by itself
about running historical Solaris or IRIX applications.
[Gaisler][gr740], [Microchip][pic32]

## 7. Relevance to the library

Hardware availability is only one part of platform viability. A useful
build target also needs an accessible operating system, compiler, ABI,
headers and runtime libraries. Continuing sales of a processor family
do not demonstrate compatibility with an older toolchain bearing the
same platform name.

This survey makes no claim that the library has been built or tested on
these current products. It is background for platform decisions, not a
support matrix or a reason by itself to remove historical platform code.

## 8. Assessment of the proposed narrower scope

Assessment date: 14 September 2026. Source examined: `dfa9e3a1`; the
subsequent `ec67fbd8` commit adds this document without changing code.
The tests below used an isolated copy of the former revision. This
assessment changes documentation only.

**Recommendation: proceed with the narrower support policy, followed by
a staged cleanup.** The source contains enough dedicated implementation
and build machinery to make the reduction worthwhile. Keeping that
machinery solely for historical completeness is not necessary. The
portable configuration system, however, serves the retained targets and
future ports; it should not be discarded with the vendor integrations.

### 8.1. What can be retired

The inventory found **37 dedicated file candidates, totaling 3,817
lines**, including comments and license headers. This is a concrete
lower bound, not an estimate of the final net deletion. Their includes,
dispatch branches, generated-project inputs and documentation must be
updated together before deleting the files.

| Group | Files eligible for retirement under the proposed policy |
|---|---|
| Seven compiler override headers | `include/rw/_config-{acc,deccxx,eccp,icc,mipspro,sunpro,xlc}.h` |
| Five architecture/vendor atomic headers | `include/rw/_atomic-{deccxx,mipspro,parisc,sparc,xlc}.h` |
| Three retired threading backends | `include/rw/_mutex-{dce,os2,solaris}.h` |
| Nine Unix compiler configurations | `etc/config/{acc,como,eccp,icc,mipspro,osf_cxx,reliant_cds,sunpro,vacpp}.config` |
| Six assembly implementations | `atomic.s` and `atomic-64.s` in each of `src/ia64/`, `src/parisc/` and `src/sparc/` |
| Seven Intel compiler Windows configurations | `etc/config/windows/icc-9.0.config`, and the ordinary and `-x64` configurations for 9.1, 10.0 and 10.1 |

An identity search across `include`, `src`, `etc/config` and `tests`
found retired compiler/platform references in **140 files**. This count
includes comments and some dedicated files above: it is a review surface,
not 140 independently obsolete files. Important mixed-file work includes:

- Compiler and architecture dispatch in [_config.h][config], atomics,
  mutexes, exception/runtime integration and test-driver identification.
- OS ABI branches in [_mbstate.h][mbstate], file and locale operations,
  floating-point handling and configuration probes.
- Compiler discovery in [GNUmakefile][topmake], platform flags in
  [gcc.config][gcc-config], and OS-specific linker/archive rules.
- Template repositories and prelinking in [makefile.common][common],
  [GNUmakefile.lib][libmake] and [GNUmakefile.cfg][cfgmake]. Their named
  consumers include Sun, DEC and IBM; retiring those consumers enables
  removal of whole build concepts, not just a few compiler flags.
- Intel-specific Windows generation paths and retired-platform entries
  in `etc/config/xfail.txt`.

The historical README and investigation documents are records. Preserve
their historical meaning; update the current support statement and build
instructions rather than making past test results look like present ones.

### 8.2. What must remain

Keep GCC and Microsoft compiler/runtime integration, x86 and x86-64
atomics, POSIX and Windows mutex backends, generic mutex-based atomic
fallbacks, and the compiler/C-library characterization mechanism.
Remove retired sub-branches from these components where appropriate;
do not delete the whole component because its comments mention an old
platform.

Specific traps in this tree:

- [_atomic-x64.h][atomic-x64] describes both IA64 and X64. Its MSVC x64
  interlocked operations remain needed. It is not an Itanium-only file.
- [_atomic-sync.h][atomic-sync] has Intel/IA64 branches but also the
  GCC builtin implementation. [_atomic.h][atomic-dispatch] selects it
  for retained GNU targets. Keep the shared implementation.
- [_mutex-pthread.h][mutex-pthread] contains Tru64 and IRIX initializer
  workarounds alongside the Linux backend. Keep the backend and its
  `PTHREAD_MUTEX_INITIALIZER` path.
- An old compiler's bug can have exposed a real library defect. Keep
  regression tests that express required library behavior; retire only
  obsolete environment-specific expectations and workarounds.
- Feature tests for types, headers, functions, exceptions, TLS and
  initialization are not made redundant by limiting the compiler brands.
  The retained compilers still have different runtimes and modes.
- x86-64 Linux and Windows have different data models (LP64 and LLP64).
  Keep type-size and ABI characterization; pointer width is not a
  substitute for the size of `long`.

### 8.3. Preparing for ARM and RISC-V

Retire named legacy integrations without introducing assumptions that
all pointers, integers, floating-point representations or memory-ordering
rules match x86. Keep portable C++ and mutex fallbacks. New architectures
should need a small backend or capability probe, not restoration of old
vendor conditionals.

There is a concrete limitation to fix when that work begins:
[_atomic.h][atomic-dispatch] chooses GNU builtins using both compiler
version and an architecture list. The existing
[ATOMIC_OPS.cpp][atomic-probe] does **not** generally characterize builtin
atomic availability or ordering: it checks the argument type of Windows
`InterlockedIncrement` on 32-bit Windows and otherwise returns success.
A future builtin-based ARM/RISC-V backend needs an appropriate
characterization and concurrency verification. Removing platform names
alone does not establish correctness on weakly ordered processors.

Neither ARM nor RISC-V was built or tested in this assessment. They are
future targets, not part of the presently verified matrix.

## 9. Measured state of the retained targets

| Environment | Assessment result |
|---|---|
| Linux x86, GCC 16.2.1, `11s` | Configuration, static library and test-driver builds passed. A standalone smoke test passed. |
| Linux x86-64, GCC 16.2.1, `15D` | Configuration, shared library and test-driver builds passed. A standalone smoke test passed; its dynamic dependencies contain stdcxx and no libstdc++. |
| Linux x86-64, Clang 22.1.8, `15D` | Configuration completed using `CONFIG=gcc.config CXX=clang`. Library build failed; the failure was reproduced by building `ti_insert_int.o` alone. |
| Linux x86, Clang | Not tested. The 64-bit failure already establishes that Clang needs work before it can be declared supported. |
| Windows x86/x86-64, Visual Studio | Not tested: no native Windows/MSVC test environment was used. The retained build machinery needs a current-toolchain audit. |

The smoke test exercised vector/string operations, classic-locale stream
formatting and parsing, and an out-of-range exception. It is a limited
runtime check, not a complete conformance, ABI or thread-safety test. The
full suite was not run, and its previously recorded failures remain
unresolved.

The initial sandboxed 32-bit configuration was invalid: executables
were terminated with SIGSYS, leaving missing configuration facts such
as `_RWSTD_SIZE_T`. Repeating configuration outside the sandbox produced
the successful baseline above. Do not mistake the failed sandbox run
for a source regression.

### 9.1. Clang needs explicit verification

The reproduced diagnostic is:

```text
include/rw/_ioinsert.cc:42:21: error: implicit instantiation of undefined template 'std::basic_ostream<char>'
```

The include trace runs through `src/ti_insert_int.cpp`, `include/ostream`
and `include/rw/_ioinsert.h`. The latter requests inserter instantiations
while `basic_ostream` is still incomplete in this compilation. This is
an existing failure, before any retirement changes; its repair was not
attempted here.

Clang reports `__GNUC__=4`, `__GNUC_MINOR__=2` and `__GNUG__=4` in this
environment. Consequently it enters many GCC branches. GNU-compatible
macros are useful for shared facilities, but are not evidence that GCC
version workarounds or instantiation behavior apply to Clang. The
configuration and explicit-instantiation paths need to be checked with
the actual compiler, without pinning an older language dialect.

### 9.2. Visual Studio retention is not current Windows support

The configurations describe Visual Studio 2002–2008. The project
generator creates `VisualStudio.VCProjectEngine` COM objects and emits
`.vcproj` files. It also carries explicit `msvcprt`/`msvcprtd` default
library exclusions. See [projectdef.js][projectdef]. These are real
integration requirements to audit against the selected Visual Studio
release, including SDK headers, CRT linkage, DLL exports and exception
handling. A Linux `clang-cl` executable cannot verify that environment.

Compiler names alone do not define a complete support matrix. A useful
initial acceptance matrix is GCC and Clang on Linux, and MSVC on
Windows, each in both widths. GCC/MinGW and Clang/clang-cl on Windows
are additional combinations, not automatic consequences of that list.
Likewise, retaining compiler brands does not require retaining every
historical release: minimum versions should be stated when their
corresponding version workarounds are evaluated for removal.

## 10. Recommended implementation sequence

1. State the narrowed OS, architecture and compiler matrix in current
   documentation. Keep ARM/RISC-V as future work. Retain the historical
   platform survey as the rationale and record.
2. Establish reproducible retained-target baselines. GCC already has the
   limited results above; repair and verify Clang, obtain Windows/MSVC
   results, and record existing test-suite failures before broad cleanup.
3. Retire the 37 dedicated candidates with their dispatch and build
   references. Use small reviewable changes grouped by subsystem.
4. Simplify mixed compiler/OS branches, template repository/prelink
   machinery and retired thread backends. Preserve generic tests and
   fallbacks. Do not combine this with raising the implementation's C++
   language floor or redesigning containers/locales.
5. Compare retained-target configuration results, library exports and
   runtime results before and after. For changes intended to be inert,
   compare generated code as well. Account for known failures explicitly
   rather than treating a suite with failures as clean.
6. Revisit remaining version workarounds against the chosen compiler
   minimums. Add ARM/RISC-V later as explicit ports with suitable atomic
   capability tests and verification on real weakly ordered hardware.

The expected benefit is a smaller build system and fewer incompatible
vendor/ABI branches to reason about. The assessment does not establish
a runtime speedup. No source files were trimmed, no compiler fixes were
made, and no new platform support was claimed or implemented.

## 11. Resolution

Decided 14 September 2026, done 15 September 2026, on master from
`dd1128a5` to `3232faa6`. The matrix is GCC and Clang on x86 and
x86-64 Linux, with aarch64 Linux to follow; Windows and Visual Studio
were retired with the rest, against the recommendation in chapter 8,
because nobody builds there. Darwin and the BSDs were not part of the
decision and their branches stay. The links above to files that no
longer exist point at the last commit that had them.

One subsystem per commit, the dedicated files first, then the build
concepts, then the mixed branches area by area:

| commit | what went |
|---|---|
| `dd1128a5` | the nine vendor compiler configurations under `etc/config` and the VisualAge version script; the makefile defaults to `gcc.config` |
| `f860b01b` | `etc/config/windows`, the MASM sources, the resource file, the DLL export pragmas |
| `6513a431` | the nine compiler override headers and the dispatch sections of `_config.h`; the Cygwin, MinGW, Tru64, Solaris and AIX sections of the GCC header |
| `3760bce1` | the effect of the Visual C++ 6 operator new clause, kept explicitly: its test held on every other compiler too |
| `0aeadf90` | the Compaq, MIPSpro, PA-RISC, SPARC and VisualAge atomic backends with the IA-64, PA-RISC and SPARC assembly; the dispatch and the retained backends reduced to the matrix |
| `26a5ccc8` | the DCE, OS/2, Solaris and Win32 thread backends; POSIX threads is the one thread model |
| `67c1f7b6` | template repositories, prelinking, AIX shared archives, the per-system branches of the gcc configuration and the rules, the workarounds in the run scripts |
| `d743fa8d` | the mixed branches in 97 headers and sources, and the twelve `_RWSTD_` macros only the override headers had defined |
| `12475dfe` | the mixed branches in 37 characterizations; the characterizations themselves stay |
| `c2beb093` | the mixed branches in 18 utility sources |
| `3e2f7ec5` | the mixed branches in 98 tests and driver sources |
| `3232faa6` | the mixed branches in 9 examples |

Every conditional on a retired identity was evaluated with the
identity macros undefined and reduced to what remains: a branch that
became false was removed, one that became true kept without its
directive, a mixed condition rewritten. Two classes of identifier
counted as retired: the compiler, system and processor macros, and
the `_RWSTD_` macros that only the deleted override headers defined,
none of them a characterization.

The check, for every commit: a fresh configuration of 11S, 11s and
15D writes the same `config.h` as before, and every translation unit
of the library, the driver, the utilities, the tests and the
examples, preprocessed with the flags the build uses, comes out as
the same text up to the line numbers the assertion macros embed.
The generated `makefile.in` lost five variables that described the
retired build concepts and nothing else. At the end, fresh builds of
the three configurations went through every phase, and their suite
tables are the pinned baselines row for row, except the rows the
baseline itself marks as unstable: `23.bitset.cons`, whose count
varies from run to run, and in 15D the seven locale MT rows that
end differently on every run until the race is fixed.

Not addressed here, on purpose: the GCC builtin atomics are still
selected by an architecture list that names i486 and x86-64 only,
so aarch64 falls to the mutex fallback (the `std::locale` entry in
`TODO` carries the port); the string atomics clause for binary
compatibility with 4.1.x is a decision of its own; the version
workarounds for old GCC releases wait for a stated minimum, step 6
above; `bin/genxviews` and `etc/config/xfail.txt` name the 2008
platforms as data and are records, like the original README.

[power11]: https://newsroom.ibm.com/2025-07-08-ibm-power11-raises-the-bar-for-enterprise-it
[t2080]: https://www.nxp.com/products/T2080
[fujitsu]: https://docs.fujitsu/documents/002161/overview-of-sparc-servers-en.pdf
[gr740]: https://www.gaisler.com/products/gr740
[parisc]: https://sources.debian.org/src/perl/5.40.1-6/README.hpux
[alpha]: https://zx.net.nz/mirror/h71000.www7.hp.com/openvms/30th/t_past_text.html
[pic32]: https://www.microchip.com/en-us/products/microcontrollers/32-bit-mcus/pic32m/pic32mz-w1
[z17]: https://newsroom.ibm.com/z17
[oracle]: https://www.oracle.com/servers/technologies/enterprise-sparc-servers-resources.html
[mips]: https://mips.com/news/
[itanium]: https://cdrdv2-public.intel.com/806590/PCN116733-00.pdf
[openxl]: https://www.ibm.com/products/open-xl-cpp-aix-compiler-power
[studio]: https://www.oracle.com/tools/developerstudio/downloads/developer-studio-jsp.html
[hpux-eos]: https://support.hpe.com/hpesc/public/docDisplay?docId=a00143419en_us&docLocale=en_US
[hpux-matrix]: https://www.hpe.com/psnow/doc/4aa4-7673enw
[vsi-cxx]: https://products.vmssoftware.com/cxx
[intel-guide]: https://cdrdv2-public.intel.com/671228/intel-compiler-15-0-linux-proting-guide.pdf
[gcc-manual]: https://gcc.gnu.org/onlinedocs/gcc-15.2.0/gcc.pdf
[edg]: https://www.edg.com/docs/edg_cpp.pdf
[edg-transition]: https://herbsutter.com/2025/11/10/trip-report-november-2025-iso-c-standards-meeting-kona-usa/
[codewarrior]: https://www.nxp.com/design/design-center/software/development-software/codewarrior-development-tools/codewarrior-network-applications/codewarrior-development-suites-for-networked-applications:CW-DS-NETAPPS
[reliant-platform]: https://archive.docs.infor.com/int/6.2/en-us/intdoc/u9018cus.pdf
[readme]: ../../README
[config]: ../../include/rw/_config.h
[driver]: ../../tests/src/driver.cpp
[vacpp]: https://github.com/thinkoid/stdcxx/blob/16df3c69/etc/config/vacpp.config
[atomic-xlc]: https://github.com/thinkoid/stdcxx/blob/16df3c69/include/rw/_atomic-xlc.h
[sunpro]: https://github.com/thinkoid/stdcxx/blob/16df3c69/etc/config/sunpro.config
[atomic-sparc]: https://github.com/thinkoid/stdcxx/blob/16df3c69/include/rw/_atomic-sparc.h
[acc]: https://github.com/thinkoid/stdcxx/blob/16df3c69/etc/config/acc.config
[config-acc]: https://github.com/thinkoid/stdcxx/blob/16df3c69/include/rw/_config-acc.h
[parisc-asm]: https://github.com/thinkoid/stdcxx/blob/16df3c69/src/parisc/atomic.s
[ia64-asm]: https://github.com/thinkoid/stdcxx/blob/16df3c69/src/ia64/atomic.s
[icc]: https://github.com/thinkoid/stdcxx/blob/16df3c69/etc/config/icc.config
[osf]: https://github.com/thinkoid/stdcxx/blob/16df3c69/etc/config/osf_cxx.config
[atomic-dec]: https://github.com/thinkoid/stdcxx/blob/16df3c69/include/rw/_atomic-deccxx.h
[mipspro]: https://github.com/thinkoid/stdcxx/blob/16df3c69/etc/config/mipspro.config
[atomic-mips]: https://github.com/thinkoid/stdcxx/blob/16df3c69/include/rw/_atomic-mipspro.h
[reliant]: https://github.com/thinkoid/stdcxx/blob/16df3c69/etc/config/reliant_cds.config
[como]: https://github.com/thinkoid/stdcxx/blob/16df3c69/etc/config/como.config
[eccp]: https://github.com/thinkoid/stdcxx/blob/16df3c69/etc/config/eccp.config
[gcc-config]: ../../etc/config/gcc.config
[mbstate]: ../../include/rw/_mbstate.h
[float-test]: ../../tests/support/18.numeric.special.float.cpp
[topmake]: ../../GNUmakefile
[common]: ../../etc/config/makefile.common
[libmake]: ../../etc/config/GNUmakefile.lib
[cfgmake]: ../../etc/config/GNUmakefile.cfg
[atomic-x64]: ../../include/rw/_atomic-x64.h
[atomic-sync]: ../../include/rw/_atomic-sync.h
[atomic-dispatch]: ../../include/rw/_atomic.h
[mutex-pthread]: ../../include/rw/_mutex-pthread.h
[atomic-probe]: ../../etc/config/src/ATOMIC_OPS.cpp
[projectdef]: https://github.com/thinkoid/stdcxx/blob/16df3c69/etc/config/windows/projectdef.js
