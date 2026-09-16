# Signal handling: System V, BSD, POSIX, and glibc

*A primer prompted by repeated escapes from `abort()`. September 2026.*

## 0. tl;dr

A signal has a **disposition** (what to do), a **mask bit** in each
thread (whether delivery is blocked), and possibly **pending state**
(an occurrence waiting to be handled). These are separate facts.
Installing a handler does not unblock a signal. Blocking a signal does
not change its disposition to ignore.

With an ordinary POSIX handler, delivery temporarily adds the signal
to the receiving thread's blocked set. A normal return restores the
previous mask. A jump out of the handler bypasses that return path;
the jump must restore the mask if execution is to continue with the
earlier blocking state. On glibc, `sigsetjmp(env, 1)` followed by
`siglongjmp` does that; `sigsetjmp(env, 0)` does not.

“BSD semantics” and “System V semantics” describe particular interface
choices, not a single operating mode for the whole program. Modern
glibc commonly supplies a BSD-style `signal()` and a System V-style
`setjmp()` together. POSIX avoids that ambiguity with `sigaction()` and
the explicit mask-saving argument to `sigsetjmp()`.

For new POSIX code, choose the disposition with `sigaction`, manage
thread masks with `pthread_sigmask`, and keep asynchronous handlers
small. Where practical, accept control signals in ordinary execution
with `sigwait`. Restoring a mask does not repair an interrupted library
operation, release a lock, or unwind C++ objects.

## 1. Scope and how to read the evidence

This document separates four authorities:

| Layer | What it establishes |
|---|---|
| ISO C | The language-level interfaces and restrictions on signal handlers and nonlocal jumps |
| POSIX | Process/thread signal behavior, dispositions, masks, waiting, and additional safe interfaces |
| glibc | Header selection, compatibility wrappers, and concrete library algorithms |
| Linux | Delivery, pending sets, signal frames, and system-call behavior |

The standards baseline here is **C11**, using the public N1570 draft,
and **POSIX.1-2017**, using the installed authorized POSIX manual-page
reproductions. Issue 8 (POSIX.1-2024) is identified explicitly where
consulted; this is not a complete account of its changes. Some online
Open Group pages rejected retrieval during preparation, so links to
the 2017 specifications accompany the locally read text.

The glibc source baseline is commit `fa91c51bb7`, with historical
comparisons at release tags `glibc-2.18`, `glibc-2.19`, `glibc-2.40`,
and `glibc-2.41`. Runtime experiments use installed **glibc 2.44 on
Linux/x86-64**. Linux mechanics below are grounded in the installed
Linux man-pages 6.19 and the glibc Linux wrappers, not a new kernel
source audit. The experiments establish the narrow behaviors they
exercise; they are not tests of all the guarantees described here.

In standards prose, *undefined* means there is no behavioral guarantee;
*unspecified* leaves a choice without requiring it to be documented;
*implementation-defined* requires the implementation to document its
choice. A result seen on one libc does not turn an unspecified choice
into a portable contract.

## 2. The historical split

### 2.1 Traditional System V `signal()`

The traditional interface installs a handler that is reset to the
default disposition when invoked. It does not automatically block
another occurrence of the same signal during the handler. To continue
catching the signal, the handler has to reinstall itself.

That leaves a window: another occurrence can arrive after the reset
and before reinstallation. After reinstallation, another occurrence
can enter the handler recursively. The problem is the installation
and delivery protocol, not merely careless handler code.

In glibc's System V compatibility implementation, the corresponding
`sigaction` choices are `SA_RESETHAND | SA_NODEFER`, without
`SA_RESTART`. The source uses the old names `SA_ONESHOT` and
`SA_NOMASK`, and also sets the historical `SA_INTERRUPT` flag.
See [glibc's System V wrapper][g-sysv].

This is not a claim that all interfaces on every System V descendant
were unreliable. The System V family also supplied `sigset`,
`sighold`, `sigrelse`, and related interfaces. Solaris documentation
distinguishes its native `signal(3C)`, `sigset(3C)`, and BSD
compatibility interfaces; the selected interface matters even within
one system. See [the Solaris signal guide][solaris].

### 2.2 BSD's persistent handlers

The BSD model keeps the handler installed and blocks its own signal
while it executes. It also restarts certain interrupted operations.
The 4.2BSD `sigvec` interface exposed a handler, an additional mask,
and flags; this is an important predecessor to the POSIX model.
“Reliable” here means control over handler persistence and delivery
races, not that every occurrence is counted or queued.

The [historical BSD manual][bsd-sigvec] also illustrates why a release
matters: `SV_INTERRUPT`, used to select interruption rather than
restart, was not available in 4.2BSD. “BSD behavior” is a useful
family resemblance, not a substitute for a version and interface.

### 2.3 POSIX makes the choices explicit

`sigaction()` installs the handler and its delivery policy together.
Applications select persistence, additional blocking, reentry, and
restart behavior through separate fields and flags. POSIX retains
`signal()` for compatibility but does not make handler installation
through that spelling uniform across implementations.

| Question | Traditional System V `signal` | BSD-style `signal` | POSIX `sigaction` |
|---|---|---|---|
| Handler remains installed? | Normally no | Yes | Yes unless reset requested |
| Own signal blocked in handler? | No automatic block | Yes | Normally yes; `SA_NODEFER` changes this |
| Interrupted calls restart? | Generally interruption semantics | Certain calls restart | `SA_RESTART` selects restart where supported |
| Additional signals blocked in handler? | Not expressed by this API | Not expressed by `signal` itself | `sa_mask` |
| Extra delivery information? | Not expressed by this API | Historical interfaces varied | `SA_SIGINFO` and `sa_sigaction` |

The table compares the familiar historical contracts, not every
exception in every Unix release. In particular, `SA_RESETHAND` and
`SA_NODEFER` should be considered separately. POSIX permits a reset
request to imply nondeferral; a program requiring both requests both
explicitly. Linux implements the two flags independently.

Sources: [POSIX `sigaction`][p-action], its application usage and
rationale, and the two glibc compatibility implementations.

## 3. A signal's journey

### 3.1 Generation, pending state, and delivery

*Generation* is the event that creates a signal occurrence. *Delivery*
is when its action is taken. In between, an occurrence can be
*pending*. A blocked signal ordinarily remains pending until it can
be delivered or is accepted by a waiting interface.

```text
event -> generation -> pending
                         |
              +----------+-----------+
              |                      |
      eligible for delivery    accepted by sigwait,
              |                sigwaitinfo, etc.
      disposition determines
      handler / default action

Ignoring is a disposition: occurrences can be discarded.
Blocking is a mask decision: it postpones ordinary delivery.
```

Possible sources include a terminal action, a timer, `kill`,
`pthread_kill`, `raise`, and a faulting instruction. Signal numbers
alone do not identify the cause: `kill(pid, SIGSEGV)` is not a memory
fault. With `SA_SIGINFO`, `si_code` helps distinguish causes; only
fields meaningful for that cause should be inspected.

In a threaded POSIX program, `raise` targets the calling thread.
`pthread_kill` names a particular thread; `kill` ordinarily names a
process or process group. The word “kill” does not imply termination:
it sends the requested signal, whose disposition determines the
action. A process-directed signal can be delivered to an eligible
thread; it is not broadcast to every thread. A fault attributable to
a particular thread is directed to that thread.

On Linux, standard signals coalesce: multiple occurrences of the same
signal while it is pending do not provide an occurrence count.
Real-time signals provide queuing, subject to resource limits, with
ordering rules distinct from standard signals. `SA_SIGINFO` alone
does not make a standard Linux signal a lossless event queue. Use
`SIGRTMIN` and `SIGRTMAX`, not hard-coded real-time signal numbers;
glibc's threading implementation reserves signals in this area.

### 3.2 Default action is not always termination

The default depends on the signal: termination, termination with core
dump processing, ignoring, stopping, or continuing. A core file is
not guaranteed; limits and system configuration affect its creation.
`SIGKILL` and `SIGSTOP` cannot be caught, ignored, or blocked.
`SIGCONT` has special continuation effects even when handler delivery
is blocked. Signal handling includes job control, not just errors.

Nor is ignoring a hardware fault a recovery strategy. For the relevant
fault-generated signals, the standards impose restrictions on ignoring
or returning from the handler. On a particular machine, merely
returning may repeat the faulting instruction. Do not generalize the
behavior of an explicitly raised signal to a hardware exception.

The Linux delivery and queuing details in this chapter are described
in [`signal(7)`][linux-signal].

## 4. Dispositions, masks, and ownership

| State | Scope | Main interfaces |
|---|---|---|
| Disposition, handler flags, handler's extra mask | Process-wide, per signal | `sigaction` |
| Current blocked set | Per thread | `pthread_sigmask`; `sigprocmask` for single-threaded programs |
| Pending process-directed signals | Process | Generation; acceptance/delivery removes occurrences |
| Pending thread-directed signals | Target thread | Generation; acceptance/delivery removes occurrences |
| Alternate handler stack | Per thread | `sigaltstack` |

`sigemptyset`, `sigfillset`, `sigaddset`, `sigdelset`, and
`sigismember` manipulate a `sigset_t` object. They do not themselves
change the running thread's mask. Passing that set to a masking
interface makes it effective. Do not construct signal sets with
integer shifts: the old BSD integer-mask interfaces are historical
compatibility APIs, not the POSIX representation.

Mask operations are `SIG_BLOCK` (add these signals), `SIG_UNBLOCK`
(remove these), and `SIG_SETMASK` (replace the mask). Unblocking a
pending signal can cause its handler to run before the masking call
returns. Treat unblocking as an execution boundary, not passive
bookkeeping.

POSIX does not specify `sigprocmask` for a multithreaded process; use
`pthread_sigmask` there. Its error convention also differs: it returns
an error number directly, whereas `sigprocmask` returns `-1` and sets
`errno`. A mask in one thread does not protect all the others.

### 4.1 Inheritance

A newly created thread inherits the creator's mask, but not its
pending signals. Dispositions are shared by the process. Blocking
control signals before creating worker threads therefore establishes
a useful initial policy for the whole thread group.

After `fork`, the child inherits dispositions and the calling thread's
mask; the child's pending set starts empty. Across `exec`, the mask
and pending signals survive, while caught dispositions reset. On
Linux, ignored dispositions remain ignored. POSIX allows a special
choice for an ignored `SIGCHLD`, so do not silently export Linux's
rule as universal.

This is why a launcher can accidentally start a new program with a
signal still blocked. Handler code is replaced by `exec`; a blocked
mask is not automatically cleared with it. See [POSIX masking][p-mask],
[thread creation][p-thread], and [`exec`][p-exec].

## 5. Entering and leaving a handler

For a persistent handler installed without `SA_NODEFER`, the mask at
entry is:

```text
handler mask = previous mask UNION sa_mask UNION {delivered signal}
```

The kernel applies this before running the first handler instruction.
`sigsetjmp` has no role in the automatic block. Other unblocked
signals can still interrupt this handler, so automatic blocking is
not global serialization.

`SA_NODEFER` omits the automatic addition of the delivered signal.
It does not remove that signal from `sa_mask` if the application
explicitly included it. With `SA_RESETHAND`, the handler disposition
also changes; a saved signal mask cannot later restore a disposition.

### 5.1 The Linux return path

Linux saves the interrupted machine context, mask, and relevant stack
state in a signal frame. It arranges execution of the handler with a
return address leading to a trampoline. A normal handler return
reaches that trampoline, which invokes `rt_sigreturn`; the kernel
restores the saved context and mask.

glibc supplies architecture-specific parts of that interface. On
x86-64, its `sigaction` wrapper arranges a `__restore_rt` trampoline;
the common Linux wrapper translates the libc action structure to the
kernel form and calls `rt_sigaction`. These are implementation
details, not functions applications should call to return from a
handler. See [the Linux wrapper][g-action] and
[the x86-64 trampoline][g-trampoline].

A normal return therefore restores the mask that preceded delivery,
even if the handler changed its current mask. A nonlocal jump avoids
this return path. It is then the jump mechanism, not `rt_sigreturn`,
that must restore whatever environment it promises.

### 5.2 Alternate stacks

`sigaltstack` supplies storage for a handler, and `SA_ONSTACK` requests
its use. This is particularly relevant when the normal stack is
exhausted, but does not make arbitrary handler operations safe or
repair the fault. Each thread needs its own setup. Nonlocal exits
from alternate stacks introduce additional state considerations;
the examples here use the ordinary stack and make no claim to test
alternate-stack recovery. See [`sigaltstack(2)`][linux-stack].

## 6. What a handler can safely do

Thread-safe and async-signal-safe answer different questions. A mutex
can make a function safe between threads while making it unusable
from a handler that interrupted the same function while holding that
mutex. The interrupted execution cannot release the lock until the
handler returns. Reentrant execution must also respect partially
updated data, even where no lock is involved.

ISO C provides a much smaller handler contract than POSIX. C11's
rules distinguish explicitly raised signals from asynchronous ones;
for asynchronous handlers, assignment to a `volatile sig_atomic_t`
flag is the classic permitted communication mechanism. C11 also
recognizes lock-free atomic objects, but this is not permission to
call arbitrary library code from a handler. See N1570 7.14.1.1.

```c
static volatile sig_atomic_t stop_requested;

static void request_stop(int sig)
{
    (void)sig;
    stop_requested = 1;
}
```

The flag is suitable for simple communication with interrupted
execution in the same thread. It is not a general inter-thread
synchronization mechanism or an atomic counter. `volatile` does not
provide mutual exclusion, compound-operation atomicity, or the memory
ordering needed to publish a data structure to other threads.

POSIX specifies a broader list of async-signal-safe functions,
including `write`, `_exit`, and signal-mask operations. Consult the
list for the chosen edition instead of inferring safety from a short
implementation. Formatted I/O, allocation, C++ streams, and ordinary
mutex locking do not belong in a general asynchronous handler.
Even a safe `write` can block or fail: async-signal-safe does not mean
nonblocking or infallible.

A handler that calls interfaces affecting `errno` should save it on
entry and restore it before normal return. For example, a self-pipe
notification handler needs to preserve `errno`, use a descriptor
prepared in advance, and account for a full pipe. The main loop must
treat notifications as a request to examine state, not a guaranteed
one-byte-per-event history.

POSIX's required safe operations and the C++ restrictions are distinct
contracts. Modern C++ has its own signal-safety rules; stdcxx's C++98
heritage does not acquire newer atomic facilities merely by using
POSIX. The complete examples below are C/POSIX programs.

Sources: [C11 draft][c11], [POSIX signal actions][p-signals], and
[Linux's signal-safety reference][linux-safe].

## 7. Signals and ordinary control flow

### 7.1 Interrupted calls and `SA_RESTART`

An interrupted operation can restart, fail with `EINTR`, or complete
partially. `SA_RESTART` is not a request to restart every system call.
On Linux, for example, some blocking reads restart, whereas interfaces
such as `poll`, `select`, and `nanosleep` have their own interruption
behavior and are not restarted by that flag. Socket timeouts affect
the rules too. Consult the operation's contract.

A positive byte count from `read` or `write` is progress, not an
`EINTR` failure. Preserve that progress. Retrying every failed call
blindly is also wrong: some operations have special retry rules, and
a shutdown signal may mean that retrying is no longer wanted.

BSD's restart tradition explains the flag's history, but neither
“BSD” nor `SA_RESTART` is a substitute for these per-operation rules.
See the interruption tables in [`signal(7)`][linux-signal].

### 7.2 Checking a flag and sleeping

This sketch has a race:

```text
while (!stop_requested)
    pause();
```

The handler can run after the condition is checked but before
`pause`. The flag is then set, yet the program goes to sleep waiting
for another signal. `volatile` does not close that window.

The POSIX solution is to block the signal while inspecting the flag,
then use `sigsuspend` to replace the mask and wait atomically. After
a handler returns, `sigsuspend` restores the pre-wait mask and returns
with `EINTR`; the loop can inspect the flag with the signal blocked
again. `pselect` provides the corresponding mask-and-wait operation
when waiting for file descriptors. See [POSIX `sigsuspend`][p-suspend].

Here is a complete single-threaded example. It deliberately generates
a pending `SIGUSR1` before waiting, making the mask transition visible
without needing a second process. Save it as `signal-wait.c`.

```c
#define _POSIX_C_SOURCE 200809L
#include <errno.h>
#include <signal.h>

static volatile sig_atomic_t notified;

static void notify(int sig)
{
    (void)sig;
    notified = 1;
}

int main(void)
{
    struct sigaction action = {0};
    sigset_t blocked, previous, waiting;

    if (sigemptyset(&blocked) || sigaddset(&blocked, SIGUSR1))
        return 1;
    if (sigprocmask(SIG_BLOCK, &blocked, &previous))
        return 2;

    action.sa_handler = notify;
    if (sigemptyset(&action.sa_mask) || sigaction(SIGUSR1, &action, 0))
        return 3;

    waiting = previous;
    if (sigdelset(&waiting, SIGUSR1) || raise(SIGUSR1))
        return 4;

    while (!notified) {
        if (sigsuspend(&waiting) != -1 || errno != EINTR)
            return 5;
    }

    return sigprocmask(SIG_SETMASK, &previous, 0) ? 6 : 0;
}
```

This is a standalone demonstration, not a helper that restores every
process attribute: it leaves its handler installed until exit. In a
threaded program, choose `pthread_sigmask` and establish which thread
will accept the signal.

### 7.3 Accepting signals without an asynchronous handler

For process control signals, a dedicated waiting thread is often
simpler. Block the chosen signals before creating workers, so the
workers inherit the mask. A designated thread calls `sigwait` or
`sigwaitinfo` to accept them. On return it is ordinary thread code,
able to use normal synchronization and library facilities. The chosen
signals remain blocked for asynchronous delivery.

This is appropriate for signals such as shutdown requests. It does
not redirect a worker's synchronous hardware fault to the waiting
thread. Nor can it consume a signal pending specifically for some
other thread. Avoid competing handler and waiter arrangements for
the same signal.

Linux `signalfd` exposes signal acceptance through a file descriptor,
which is useful in event loops but is not POSIX. The selected signals
still need a deliberate blocking policy. See [POSIX `sigwait`][p-wait]
and [`signalfd(2)`][linux-fd].

## 8. Nonlocal jumps: a separate BSD/System V split

### 8.1 What is saved?

`setjmp` saves a return environment. `longjmp` resumes it as though
the saving call returned a nonzero value. This restores control flow,
not the whole program: heap modifications, file positions, locks, and
external effects are not rolled back.

Historically, BSD `setjmp`/`longjmp` saved and restored the signal
mask, while `_setjmp`/`_longjmp` omitted it. System V Release 3 and
its SVID contract specified that `setjmp`/`longjmp` did not save and
restore the mask. POSIX deliberately did not choose between those
`setjmp` contracts; it added the explicit `sigsetjmp` interface.
This history is explained in the [POSIX `sigsetjmp` rationale][p-sj].

| Saving operation | Mask behavior on inspected glibc |
|---|---|
| `setjmp(env)` | Does not save the mask |
| `sigsetjmp(env, 0)` | Does not save the mask |
| `sigsetjmp(env, 1)` | Saves the current thread's mask |

`siglongjmp` restores the saved mask when requested by the matching
`sigsetjmp`. For portable code that requires restoration, use a
nonzero argument. POSIX.1-2017's `sigsetjmp` application usage warns
against relying on mask saving when the argument is zero; the zero
case in the table and experiments is specifically glibc behavior.

glibc records the decision in `__mask_was_saved`; its common jump
implementation tests that field before restoring the mask with
`SIG_SETMASK`. The `sig` prefix on `siglongjmp` does not manufacture
a mask that was never saved. See [mask saving][g-save] and
[mask restoration][g-jump].

### 8.2 glibc's feature-test macros do not select one global dialect

Before glibc 2.19, an explicit `_BSD_SOURCE` could select BSD-style
`setjmp` when competing feature selections were absent. Commit
`7011c2622f` removed `__FAVOR_BSD`; from glibc 2.19 onward the normal
header expansion of `setjmp` selects the version that does not save
the mask. Defining `_GNU_SOURCE` or `_DEFAULT_SOURCE` does not change
that into BSD mask-saving behavior.

`signal`, however, has its own selection. In the inspected headers,
the miscellaneous-extension interface exposes the BSD implementation.
Without it, the declaration redirects `signal` to `__sysv_signal`.
`_DEFAULT_SOURCE` enables the miscellaneous interfaces;
`_GNU_SOURCE` enables them too. A strict POSIX feature selection
alone can therefore produce System V-style `signal` on glibc even
though `sigaction` remains available.

The BSD wrapper uses a persistent disposition, includes the signal
in the handler's mask, and normally selects `SA_RESTART`. Its restart
choice can also be affected by `siginterrupt`, so even that wrapper
has state worth noticing. The System V wrapper makes the opposite
reset/nondeferral/restart choices described in chapter 2.

| Typical modern glibc source environment | `signal(handler)` selection | `setjmp` mask saving |
|---|---|---|
| `_DEFAULT_SOURCE` enabled | BSD-style wrapper | No |
| `_GNU_SOURCE` enabled | BSD-style wrapper | No |
| `_POSIX_C_SOURCE=200809L` alone, miscellaneous extensions absent | System V wrapper | No |
| Explicit `sigaction` and `sigsetjmp(env, 1)` | Policy from `sigaction` fields | Yes |

Feature-test macros must precede system headers. Compiler defaults and
predefined macros can affect the result, so inspect the actual
translation environment. Applications should not define glibc's
internal `__USE_*` switches.

The source evidence is [the public signal header][g-header],
[the BSD wrapper][g-bsd], and [the jump header][g-jheader]. This is
why “we compile in BSD mode” is insufficient to explain a jump.

### 8.3 The mask is only one recovery obligation

The jump target must still be alive, initialized, and belong to the
same thread. Automatic variables local to the saving function that
changed after the save and lack `volatile` qualification cannot be
relied upon after the jump. Calls to the saving macro also have
restricted syntactic contexts; the examples use it directly in an
`if` condition. See N1570 7.13 and [POSIX `longjmp`][p-lj].

POSIX.1-2008 TC2 added `longjmp` and `siglongjmp` to its safe-function
list. That does not make general escape from interrupted unsafe code
safe: if the handler interrupted a non-async-signal-safe operation
and jumps away, later calls to unsafe operations can have undefined
behavior. The interrupted operation may have been left inconsistent.
The normative `longjmp` description includes this restriction; older
informative prose in `sigaction` still describes the earlier list.

In C++, nonlocal jumps are not exception unwinding. The language's
`setjmp`/`longjmp` rule makes crossing a boundary where the equivalent
throw/catch would invoke nontrivial automatic-object destructors
undefined. POSIX jump functions provide no C++ destructor-unwinding
contract either. Do not use them to escape scopes holding RAII locks.
See the [C++ draft's nonlocal-jump rule][cpp-jump].

For stdcxx's assertion tests, this distinction matters directly:
restoring `SIGABRT`'s mask can repair the repeatability of the trap,
but cannot make a jump across a held locale lock safe. The separate
[codecvt investigation](codecvt-invalid-state.md) records that case.
An isolated child process can test expected fatal behavior without
resuming the potentially compromised execution of the child.

## 9. Case study: `abort()` and glibc 2.41

### 9.1 The required behavior

POSIX requires abnormal termination unless `SIGABRT` is caught and
the handler does not return. `abort` must override blocking or
ignoring `SIGABRT`; it does not promise that a blocked signal will
first be unblocked while the application's handler is installed.
Returning from a handler does not cancel the abort.

This distinction permits glibc to force termination by changing the
disposition before unblocking. `raise(SIGABRT)` by itself does not
provide `abort`'s stronger termination behavior.
See [POSIX `abort`][p-abort].

### 9.2 The implementation change

glibc 2.40's `abort` uses a staged algorithm. It unblocks `SIGABRT`
before the initial raise and allows a handler to escape, leaving
the algorithm ready for another call. A later `abort` thus unblocks
a signal left blocked by a previous jump.

Commit `d40ac01cbbc66e6d9dbd8e3485605c63b2178251`, committed on
**2024-10-08** and included in **glibc 2.41**, replaces that algorithm
as part of making `abort` async-signal-safe and coordinating it with
signal-disposition changes and process creation. The author date is
2024-10-03; neither date is the release date of glibc 2.41.

The 2.41 sequence is:

1. Call `raise(SIGABRT)` without an initial unblock.
2. If execution continues, block signals and acquire the internal
   abort write lock.
3. Set the disposition of `SIGABRT` to `SIG_DFL`.
4. Raise it internally and unblock it, forcing termination.
5. Retain a machine-instruction and `_exit` fallback.

An escape from the first handler never reaches step 2. A blocked
initial signal does: the raise cannot run the handler, so `abort`
continues into the termination sequence. The commit message explicitly
identifies mask-losing `setjmp`/`longjmp` users and recommends
`sigsetjmp` instead.

Sources: [`abort.c` in 2.40][g-abort40],
[`abort.c` in 2.41][g-abort41], and [the change][g-abort-change].
The inspected development checkout later substitutes `__raise_direct`
for the first `raise`; the mask distinction remains.

### 9.3 Trace the two calls

Assume a persistent `sigaction` handler, no `SA_NODEFER`, no concurrent
disposition changes, and `SIGABRT` unblocked when the environment is
saved.

| Event | `sigsetjmp(env, 0)` on glibc | `sigsetjmp(env, 1)` |
|---|---|---|
| Save environment | No saved mask | Saves unblocked mask |
| First `abort`: handler entry | Kernel blocks `SIGABRT` | Kernel blocks `SIGABRT` |
| Handler calls `siglongjmp` | Block remains | Saved unblocked mask restored |
| Second `abort`: initial raise | Signal becomes pending | Handler runs again |
| Outcome | Default disposition installed; unblocking terminates | A second jump can escape |

Thus “glibc 2.41 makes the second jump fatal” is imprecise. In the
failing case, the second handler and second jump never occur. The
second **abort** terminates because the first jump left the signal
blocked.

Saving a mask that already blocks `SIGABRT` will not help. Nor will
mask restoration reinstall a handler reset by System V semantics or
`SA_RESETHAND`. Explicit unblocking and `SA_NODEFER` can change the
mask outcome, but introduce their own delivery and reentry conditions;
they are not substitutes for understanding the saved environment.

### 9.4 A reproducible experiment

Save this as `abort-jump.c`. It takes `0` or `1` as the mask-saving
argument. The handler does only a POSIX nonlocal jump; observations
and output happen after the jump. There are no C++ objects or
application locks, and `abort` is called directly rather than through
an arbitrary interrupted library operation.

```c
#define _POSIX_C_SOURCE 200809L
#include <setjmp.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>

static sigjmp_buf env;
static int caught;

static void escape_abort(int sig)
{
    (void)sig;
    siglongjmp(env, 1);
}

int main(int argc, char **argv)
{
    struct sigaction action = {0};
    sigset_t mask;

    if (argc != 2 || (argv[1][0] != '0' && argv[1][0] != '1')
        || argv[1][1] != '\0')
        return 2;

    action.sa_handler = escape_abort;
    if (sigemptyset(&action.sa_mask) || sigemptyset(&mask)
        || sigaddset(&mask, SIGABRT))
        return 3;
    if (sigprocmask(SIG_UNBLOCK, &mask, 0)
        || sigaction(SIGABRT, &action, 0))
        return 4;

    if (sigsetjmp(env, argv[1][0] == '1')) {
        ++caught;
        if (sigprocmask(SIG_BLOCK, 0, &mask))
            return 5;
        printf("caught=%d SIGABRT_blocked=%d\n",
               caught, sigismember(&mask, SIGABRT));
        if (fflush(stdout))
            return 6;
        if (caught == 2)
            return 0;
    }

    abort();
}
```

Compile and run in a disposable directory:

```sh
cc -std=c11 -O2 -Wall -Wextra abort-jump.c -o abort-jump
ulimit -c 0
./abort-jump 0
./abort-jump 1
```

The language flag here makes the standalone C example reproducible;
it is not a change to stdcxx's build configuration.

Observed on glibc 2.44:

```text
argument 0:
caught=1 SIGABRT_blocked=1
[terminated by SIGABRT]

argument 1:
caught=1 SIGABRT_blocked=0
caught=2 SIGABRT_blocked=0
[normal exit, status 0]
```

The brackets describe the parent process's observation, not output
from the program. Shells may print a diagnostic and encode signal
termination as an exit status; a parent should inspect `waitpid`
status with `WIFSIGNALED` and `WTERMSIG` rather than assuming a
particular shell encoding.

## 10. Practical use and diagnosis

For shutdown, let a handler set a flag or wake ordinary code; close
resources there. If that code sleeps, use an atomic mask-and-wait
operation or an appropriately designed notification channel. In a
threaded service, consider a dedicated signal-waiting thread.

For child reaping, treat `SIGCHLD` as “child state may have changed,”
not “exactly one child exited.” Standard signals coalesce. Drain the
available statuses with the appropriate `waitpid` loop, handle
`EINTR`, and select stop/continue reporting deliberately. On Linux,
explicit `SIG_IGN` for `SIGCHLD` has child-reaping effects different
from simply leaving its default disposition; do not equate the two
because a signal table labels the default action “ignore.”

For failures involving a handler, inspect these independently:

1. Which interface installed it, under which feature-test macros?
2. Is its disposition persistent, reset, or already changed elsewhere?
3. Which thread owns the event, and what is that thread's current mask?
4. Is the signal pending for the thread or for the process?
5. Does the handler return, exit, or jump? Who restores the mask?
6. What operation was interrupted, and which locks or invariants remain?

On Linux, `/proc/PID/task/TID/status` distinguishes `SigBlk`,
`SigPnd`, `ShdPnd`, `SigIgn`, and `SigCgt`. A view of only the main
thread can miss the mask of the thread that matters. `strace` can
show `rt_sigaction`, `rt_sigprocmask`, signal generation, and
`rt_sigreturn`; a nonlocal exit may explain the absence of the last.
Debugger signal policies can stop, pass, or suppress a signal, so
record those policies when comparing a debug run to an ordinary run.

When reading old source, first reconstruct the intended protocol.
Reinstalling a handler inside itself, using integer signal masks,
or relying on `setjmp` to restore a mask can be evidence of a
particular Unix contract rather than a random coding mistake. Then
ask which part of that contract the current translation environment
actually preserves.

## 11. Evidence, reproduction, and limits

The source comparisons establish the glibc wrapper policies, the
2.19 `setjmp` header change, and the 2.41 `abort` change. No historical
glibc release was built or executed for this primer.

The two complete examples above were compiled with
`cc -std=c11 -O2 -Wall -Wextra` and run on Linux/x86-64 with glibc
2.44. `signal-wait` exited 0. `abort-jump 0` terminated by `SIGABRT`
after one observation; `abort-jump 1` printed both observations and
exited 0. Core dumps were disabled for the abort experiment.

A separate compile-only probe of `signal()` under
`_POSIX_C_SOURCE=200809L` alone and with `_DEFAULT_SOURCE` inspected
the resulting symbol references. To reproduce it, save this as
`signal-selection.c`:

```c
#include <signal.h>

static void handler(int sig)
{
    (void)sig;
}

int main(void)
{
    return signal(SIGUSR1, handler) == SIG_ERR;
}
```

```sh
cc -std=c11 -O2 -D_POSIX_C_SOURCE=200809L -c signal-selection.c -o posix.o
cc -std=c11 -O2 -D_POSIX_C_SOURCE=200809L -D_DEFAULT_SOURCE -c signal-selection.c -o default.o
nm -u posix.o default.o
```

The former referenced `__sysv_signal`; the latter referenced
`signal`. This checks header selection, not the entire behavioral
contract of either wrapper.

No stdcxx build or test suite was run. The experiments do not establish
safe recovery from an
arbitrary assertion, hardware fault, interrupted unsafe operation,
or alternate-stack handler.

### 11.1 Standards and historical documentation

- [ISO C11 draft N1570][c11]: 7.13 (nonlocal jumps), 7.14 (signals),
  and 7.22.4.1 (`abort`). It is a public committee draft, not a claim
  to reproduce the published standard verbatim.
- POSIX.1-2017: [signal actions][p-signals], [`sigaction`][p-action],
  [`sigprocmask` and thread masking][p-mask], [`sigsetjmp`][p-sj],
  [`siglongjmp`][p-slj], [`longjmp`][p-lj], and [`abort`][p-abort].
  The normative descriptions take priority over stale informative
  remarks about which functions are async-signal-safe.
- [POSIX.1-2024 `sigaction`][p-action24]: consulted for the continued
  distinction between reset and nondeferral.
- [Historical BSD `sigvec` documentation][bsd-sigvec] and
  [Solaris's signal-interface comparison][solaris]. These provide
  historical context, not universal current-system guarantees.
- [C++ working draft, nonlocal jumps][cpp-jump]: the destructor
  restriction. This moving draft is not used to redefine stdcxx's
  language target.

### 11.2 Implementation map

| Question | glibc source |
|---|---|
| Which `signal` does a source call name? | `signal/signal.h`, `include/features.h` |
| What does the BSD wrapper install? | `sysdeps/posix/signal.c` |
| What does the System V wrapper install? | `sysdeps/posix/sysv_signal.c` |
| How does Linux receive an action? | `sysdeps/unix/sysv/linux/libc_sigaction.c` |
| Where is the x86-64 return trampoline? | `sysdeps/unix/sysv/linux/x86_64/libc_sigaction.c` |
| Which jump variant does the header select? | `setjmp/setjmp.h` |
| Where is the mask saved/restored? | `setjmp/sigjmp.c`, `setjmp/longjmp.c` |
| Why does the second abort terminate? | `stdlib/abort.c`; compare 2.40 and 2.41 |

Implementation links below are pinned to release tags or commits.
Linux man-pages references should be read alongside the installed
edition: web copies can change independently of the target system.

[c11]: https://www.open-std.org/jtc1/sc22/wg14/www/docs/n1570.pdf
[p-signals]: https://pubs.opengroup.org/onlinepubs/9699919799/functions/V2_chap02.html#tag_15_04
[p-action]: https://pubs.opengroup.org/onlinepubs/9699919799/functions/sigaction.html
[p-action24]: https://pubs.opengroup.org/onlinepubs/9799919799/functions/sigaction.html
[p-mask]: https://pubs.opengroup.org/onlinepubs/9699919799/functions/pthread_sigmask.html
[p-thread]: https://pubs.opengroup.org/onlinepubs/9699919799/functions/pthread_create.html
[p-exec]: https://pubs.opengroup.org/onlinepubs/9699919799/functions/exec.html
[p-suspend]: https://pubs.opengroup.org/onlinepubs/9699919799/functions/sigsuspend.html
[p-wait]: https://pubs.opengroup.org/onlinepubs/9699919799/functions/sigwait.html
[p-sj]: https://pubs.opengroup.org/onlinepubs/9699919799/functions/sigsetjmp.html
[p-slj]: https://pubs.opengroup.org/onlinepubs/9699919799/functions/siglongjmp.html
[p-lj]: https://pubs.opengroup.org/onlinepubs/9699919799/functions/longjmp.html
[p-abort]: https://pubs.opengroup.org/onlinepubs/9699919799/functions/abort.html
[cpp-jump]: https://eel.is/c++draft/csetjmp.syn
[bsd-sigvec]: https://man.freebsd.org/cgi/man.cgi?apropos=0&manpath=2.11+BSD&query=sigvec&sektion=2
[solaris]: https://docs.oracle.com/cd/E19455-01/806-4750/6jdqdflt0/index.html
[linux-signal]: https://man7.org/linux/man-pages/man7/signal.7.html
[linux-safe]: https://man7.org/linux/man-pages/man7/signal-safety.7.html
[linux-stack]: https://man7.org/linux/man-pages/man2/sigaltstack.2.html
[linux-fd]: https://man7.org/linux/man-pages/man2/signalfd.2.html
[g-header]: https://github.com/bminor/glibc/blob/fa91c51bb7/signal/signal.h
[g-bsd]: https://github.com/bminor/glibc/blob/fa91c51bb7/sysdeps/posix/signal.c
[g-sysv]: https://github.com/bminor/glibc/blob/fa91c51bb7/sysdeps/posix/sysv_signal.c
[g-action]: https://github.com/bminor/glibc/blob/fa91c51bb7/sysdeps/unix/sysv/linux/libc_sigaction.c
[g-trampoline]: https://github.com/bminor/glibc/blob/fa91c51bb7/sysdeps/unix/sysv/linux/x86_64/libc_sigaction.c
[g-jheader]: https://github.com/bminor/glibc/blob/fa91c51bb7/setjmp/setjmp.h
[g-save]: https://github.com/bminor/glibc/blob/fa91c51bb7/setjmp/sigjmp.c
[g-jump]: https://github.com/bminor/glibc/blob/fa91c51bb7/setjmp/longjmp.c
[g-abort40]: https://github.com/bminor/glibc/blob/glibc-2.40/stdlib/abort.c
[g-abort41]: https://github.com/bminor/glibc/blob/glibc-2.41/stdlib/abort.c
[g-abort-change]: https://github.com/bminor/glibc/commit/d40ac01cbbc66e6d9dbd8e3485605c63b2178251
