# Runtime and library architecture

Status: current boundary plus planned service contracts, 25 October 2026.

## Implemented boundary

The compiler emits C with checked fault propagation and cleanup. Its buffer runtime
is embedded with internal linkage, so generated objects do not require a separately
linked Fermi runtime. Compiler execution still needs configured source-tree library
and runtime paths; installation is not relocatable yet.

runtime/memory/buffer.c owns allocation mechanics. library/std/mem.fe defines the
source API: bytes, from_bytes, reserve and resize. Native growth updates the caller
descriptor through a non-owning exclusive pointer. Borrowed descriptors never
enter cleanup lists. from_bytes is ordinary Fermi code. Scalar-width support in C does not imply
support for Buffer types other than u8 in Fermi.

library/std/io.fe provides synchronous stdin/stdout/stderr operations.
library/std/env.fe copies environment bytes into caller storage. library/std/num.fe
and library/std/config.fe implement bounded decimal/range validation and startup
composition in Fermi. Environment lookup remains an external env.read effect.
runtime/process/arguments.c manages explicit startup contexts. Generated executable
wiring initializes a private context from C main; generated libraries return
unavailable until an embedding contract exists. compiler/backend/c/runtime.c
embeds both argument and buffer snapshots through the same checked file copier.
library/std/fmt.fe supplies bounded decimal output in Fermi. std.time reads the
POSIX monotonic clock through a clock.read effect; deadline/remaining arithmetic
is ordinary Fermi. No blocking wait or I/O cancellation is implemented.

runtime/io/file.c supplies the read-only Linux owner exposed through std.fs.File.
Its snapshot is embedded into generated C. The checker enforces move and exclusive
borrows; cleanup and effects dispatch by resource kind, including owning Result
payloads. Explicit close consumes even on error. See [files](../spec/files.md).

Result<void, E> is now supported for fallible operations with no success payload.
std.fs.close now uses this shape and mandatory Result handling.

No scheduler, task runtime, network stack or process-wide
resource accountant is available.
Cleanup handles language exits and propagated faults, not SIGKILL, hardware faults,
process crashes, or arbitrary native memory corruption.

## Ownership and failure policy

An owner carries its cleanup obligation. Fallible operations return domain errors;
invalid program operations propagate language faults. Cleanup must not overwrite
the original fault. Explicit resource close will need a fallible API when external
flush/close failures matter; a destructor alone cannot report those failures safely.

Per-buffer capacity limits exclude old-plus-new transient growth memory. Service
limits must account for concurrent owners, active tasks and transport buffers.
Runtime budgets and static work certificates are different contracts. Neither
currently bounds native stack or process RSS.

## Library boundaries and gates

| Layer | Responsibility | Required gate |
| --- | --- | --- |
| Memory | Byte descriptor borrowing and growth implemented; broader resource forms remain restricted | Byte-buffer tests; ownership stage in PLAN.md |
| Bytes/text | Validation, parsing and explicit encoding | Malformed UTF-8 and bounded-input tests |
| Process/file/time | Configuration, file ownership, monotonic deadlines | Partial I/O, close errors and cleanup |
| Network | TCP ownership, partial reads/writes, timeouts | Slow/disconnected peer tests |
| Tasks | Bounded admission, sharing, cancellation and joins | Race/stress and shutdown tests |
| Protocols | HTTP, JSON and TLS integration | Interoperability, parser fuzzing and overload |

These are responsibilities, not frozen module names or package dependencies.
Generic collections and resource fields require separate language proposals;
implement only the abstractions demonstrated necessary by the next application.

## Planned concurrency contract

Task creation must identify ownership transfer or synchronized sharing. A shared
read view alone does not certify thread safety. Detached work is excluded from
initial acceptance; supervisors must join children before their borrowed resources
can be destroyed. A fatal child error stops sibling admission and initiates cleanup.

Cancellation needs a path through queue waits, I/O waits and computation checkpoints.
Every bounded queue must define full/closed behavior and who closes it. A consumer
exiting early must unblock producers. No async keyword or executor is selected yet;
a bounded blocking prototype can establish lifecycle behavior before that decision.

Native adapters remain trusted code and require explicit lifetime, alignment,
thread-safety and reentrancy contracts. TLS must use an evaluated maintained library;
no custom cryptography is planned. See [research](../research/2026-10-25-backend-design.md)
for the sources informing these requirements.
