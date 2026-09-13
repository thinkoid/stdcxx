# Apache stdcxx

The Apache C++ Standard Library: Rogue Wave's implementation of the
C++ Standard Library, open-sourced in 2005, developed through 2008,
dormant after, and retired to the Apache attic in 2012. It is a
complete implementation of the library specified by ISO/IEC
14882:2003, with its own iostreams, locales, strings and containers,
built by a configuration system that characterizes the compiler and
the C library with a few hundred small test programs rather than
trusting version numbers.

This repository is a revival. It forks from the tip of the 4.2.x
maintenance line, the last released lineage, and builds it against a
current toolchain: GCC 16 and glibc 2.44 at the time of writing. The
tree had not been compiled in earnest since 2012, and the toolchain
had moved a great deal in the meantime. Everything that broke gets
the same treatment: find out what the platform now offers, write a
characterization for it, and let the tree branch on the answer.

The original `README` is kept untouched beside this file. It is the
complete manual for the library's layout, configuration macros, build
system and platform notes as of 4.2.2, and it remains accurate for
everything the revival has not changed.

## Status

The library builds clean. The test driver builds. The test suite
builds except for a small number of tests whose remaining failures
are listed in `TODO`, in priority order, with the fix intended for
each. The suite has not yet been run in full against the current
toolchain; restoring that baseline is the next milestone.

Investigations that earn a write-up live under `doc/notes`. The first
one traces the library's `__mbstate_t` clash with glibc through three
epochs of the C library's headers.

## Building

The build is out of tree. From the source directory:

```sh
make BUILDDIR=$HOME/build/stdcxx-build/15D BUILDTYPE=15D config
```

`config` runs the characterizations and writes `config.h` into the
build directory. It is serial by design: each characterization may
depend on the answers of others, and the walk resolves those
dependencies on demand. It takes about twenty seconds and cannot be
parallelized. Then, from the build directory:

```sh
cd $HOME/build/stdcxx-build/15D
make -j16 lib        # the library
make -j16 rwtest     # the test driver
make -j16 tests      # the tests
make -j16 examples   # the examples
```

The phases are serial with respect to one another and parallel
within. A bare `make -j` at the top level was never the idiom.

`BUILDTYPE` selects the build: the number is the level of debugging,
optimization and thread safety, the letter is archive or shared and
narrow or wide. `15D` is debug, shared, threads, 64-bit. The original
`README`, section 5, has the full table. Command-line `CXXOPTS` and
`CPPOPTS` reach the build but not the configuration, by design: a
build directory describes itself, and anything the configuration must
see goes in the compiler's config file under `etc/config`.

## Requirements

GNU make, a C++ compiler, and the C library the compiler targets.
Nothing else. There is no autoconf, no CMake, and there will not be.

## Rules of the tree

- Nothing here is easy pickings. Twenty years of curated changes,
  everything in the tree with a purpose. A fix that looks like an
  oversight is presumed to be a design until the history says
  otherwise. The full history since the 2005 import is in git, and
  the commit messages say why.
- Characterize, never pin. A missing or changed feature is detected
  by a configuration test under `etc/config/src` and gated by the
  `_RWSTD_NO_*` macro it yields. The compiler is never bent back with
  `-std` or `-D` to make the tree compile.
- The tree is C++98 and stays so until the floor is raised
  deliberately, as one decision. Null pointers are the literal `0`.
- Commit messages are ChangeLog entries, in the style already in the
  history. The `ChangeLog` file itself is empty: git holds the same
  information.

## Layout

| path | what |
|---|---|
| `include/` | the public headers; `rw/` and `loc/` hold the private ones |
| `src/` | the library sources |
| `etc/config/` | the configuration system: makefiles, compiler config files, and the characterizations under `src/` |
| `tests/` | the test suite; `include/` and `src/` hold the test driver |
| `examples/` | example and tutorial programs |
| `util/` | the locale utilities and the test harness's `exec` |
| `doc/` | the original HTML manuals, and `notes/` for the revival's investigations |
| `TODO` | the queue, in priority order |
| `README` | the original manual, untouched |

## License

Apache License 2.0; see `LICENSE`. `NOTICE` carries the copyright
notices that apply.
