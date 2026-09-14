# Non-x86 platforms and compilers in stdcxx

Survey date: 14 September 2026.

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
[vacpp]: ../../etc/config/vacpp.config
[atomic-xlc]: ../../include/rw/_atomic-xlc.h
[sunpro]: ../../etc/config/sunpro.config
[atomic-sparc]: ../../include/rw/_atomic-sparc.h
[acc]: ../../etc/config/acc.config
[config-acc]: ../../include/rw/_config-acc.h
[parisc-asm]: ../../src/parisc/atomic.s
[ia64-asm]: ../../src/ia64/atomic.s
[icc]: ../../etc/config/icc.config
[osf]: ../../etc/config/osf_cxx.config
[atomic-dec]: ../../include/rw/_atomic-deccxx.h
[mipspro]: ../../etc/config/mipspro.config
[atomic-mips]: ../../include/rw/_atomic-mipspro.h
[reliant]: ../../etc/config/reliant_cds.config
[como]: ../../etc/config/como.config
[eccp]: ../../etc/config/eccp.config
[gcc-config]: ../../etc/config/gcc.config
[mbstate]: ../../include/rw/_mbstate.h
[float-test]: ../../tests/support/18.numeric.special.float.cpp
