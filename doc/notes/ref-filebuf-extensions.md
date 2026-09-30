# The file stream additions

## 0. tl;dr

The library adds to `basic_filebuf` and to the three file streams what
C++03 does not specify: construction on an open file descriptor or
stdio `FILE*`, `fd ()`, `attach ()`, `detach ()`, a permissions
argument to `open`, and an argument to `close`. The reference manual
marks them as extensions "for compatibility with Classic Iostreams"
(`doc/stdlibref/basic-filebuf.html`) and lists them in its Appendix B.
Every one is enabled in the six tracked configurations.

`_RWSTD_NO_EXT_FILEBUF` removes most of them, and the strict-ANSI
configuration defines it. Two stay under it, since no macro gates
them: the permissions argument and `close`'s argument. The descriptor
constructor also stays, private: `ios_base::Init` builds the standard
objects' buffers with it.

Inside the library only that constructor is used. Two tests
exercise the additions; no example does.

## 1. The gates

| macro | effect | set by |
|---|---|---|
| `_RWSTD_NO_EXT_FILEBUF` | removes the additions of chapters 2 and 3 marked "gated" | `_RWSTD_STRICT_ANSI` (`include/rw/_config.h`) |
| `_RWSTD_NO_NATIVE_IO` | file streams use stdio in place of POSIX descriptors, and the descriptor forms go | nothing: a manual override (original README, section 8.3) |
| `stdin` | the `FILE*` forms exist only where `<cstdio>` or `<stdio.h>` came before `<fstream>` | the translation unit's includes |

The tracked configurations define neither macro. Their file streams
work on POSIX descriptors.

## 2. `basic_filebuf`

All in `include/fstream`.

| addition | line | what it does | gated |
|---|---|---|---|
| `basic_filebuf (int fd, char_type* buf = 0, streamsize n = _RWSTD_DEFAULT_BUFSIZE)` | 135 | a buffer on an open descriptor, with an optional caller's buffer | native I/O; private under `_RWSTD_NO_EXT_FILEBUF` |
| `basic_filebuf (FILE* fp, char_type* buf = 0, streamsize n = …)` | 154 | a buffer on an open `FILE*` | yes, and `stdin` |
| `open (int fd, char_type* buf = 0, streamsize n = …)` | 178 | the descriptor form as a member | yes, and native I/O |
| `open (FILE* fp, char_type* buf = 0, streamsize n = …)` | 190 | the `FILE*` form as a member | yes, and `stdin` |
| `fd () const` | 200 | the descriptor of the associated file | yes, and native I/O |
| `attach (int fd)` | 205 | associates an open descriptor, keeping the current buffer | yes, and native I/O |
| `detach ()` | 210 | flushes and lets the descriptor go without closing it; returns it, or -1 when the flush fails | yes, and native I/O |
| `open (const char*, openmode, long protection = 0666)` | 168 | the third argument: permissions of a file the call creates | no |
| `close (bool close_file = true)` | 219 | the argument: `false` flushes and disassociates without closing, the path of `detach ()` | no |

A buffer passed to a constructor or `open` stays the caller's; the
file buffer does not free it.

## 3. `basic_ifstream`, `basic_ofstream`, `basic_fstream`

Each of the three has the same set.

| addition | lines (i / o / io) | gated |
|---|---|---|
| constructor `(int fd, char_type* buf = 0, streamsize n = …)` | 406 / 516 / 627 | yes, and native I/O |
| constructor `(FILE* fp, char_type* buf = 0, streamsize n = …)` | 417 / 527 / 638 | yes, and `stdin` |
| `open (int fd, char_type* buf = 0, streamsize n = …)` | 453 / 563 / 674 | yes, and native I/O |
| `open (FILE* fp, char_type* buf = 0, streamsize n = …)` | 463 / 573 / 684 | yes, and `stdin` |
| constructor from a name, `long protection = 0666` | 394 / 504 / 615 | no |
| `open` from a name, `long protection = 0666` | 442 / 552 / 663 | no |

The stream forms forward to the buffer's and set `failbit` when the
buffer fails.

## 4. Who uses them

- **The library.** `ios_base::Init::Init` (`src/iostream.cpp:253` and
  after) constructs the eight standard objects' buffers on
  `STDIN_FILENO`, `STDOUT_FILENO` and `STDERR_FILENO` with the
  descriptor constructor; under `_RWSTD_NO_NATIVE_IO`, on `stdin`,
  `stdout` and `stderr` with the `FILE*` one. Under
  `_RWSTD_NO_EXT_FILEBUF` the descriptor constructor is private and
  `ios_base::Init` its friend (`include/fstream:124`). No other
  library source calls an addition.
- **Tests.** `27.filebuf` exercises every buffer addition. The
  regression test `27.filebuf.members.stdcxx-308` calls `fd ()`. No
  other test uses an addition; `27.filebuf.codecvt` and
  `27.ios.members.static` work on files through stdio and POSIX calls
  of their own.
- **Examples.** None. The word "attach" in `ostream.cpp`,
  `stringbuf.cpp` and `wostream.cpp` names attaching a string buffer.

## 5. Beside the standard

C++03 through C++23 specify none of the additions. C++26 adds
`native_handle ()` to `basic_filebuf` and to the three streams
([P1759R6](https://www.open-std.org/jtc1/sc22/wg21/docs/papers/2023/p1759r6.html)):
the platform's handle of an open file, a descriptor on POSIX. It is
the standard's counterpart of `fd ()`. No standard, the current draft
included, gives a descriptor or `FILE*` constructor, `attach ()` or
`detach ()`, or a permissions argument to `open`. Other libraries
meet that need outside the standard class: libstdc++ with
`__gnu_cxx::stdio_filebuf`, a class of its own. Whether
the additions stay is not decided; the strict-ANSI entry in `TODO`
lists them among the additions its rule would retire.
