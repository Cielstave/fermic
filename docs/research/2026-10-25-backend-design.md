# Backend language design review

Reviewed 25 October 2026. Primary references were read alongside the local compiler,
runtime, specification and tests. Upstream main/master documentation is mutable;
these links identify the reviewed material, not a pinned implementation dependency.

## Sources and limited conclusions

| Primary source | Observation | Fermi design implication |
| --- | --- | --- |
| [Rust borrow checker](https://rustc-dev-guide.rust-lang.org/borrow-check.html) | Checking tracks initialization, moves and conflicting access; MIR reduces source-level complexity | Keep access categories explicit; evaluate a control-flow representation before escaping borrows |
| [Rust drop elaboration](https://rustc-dev-guide.rust-lang.org/mir/drop-elaboration.html) | Cleanup depends on initialization and transfer state, including conditional paths | Preserve owner/drop obligations across normal and fault exits |
| [Zig Allocator interface](https://raw.githubusercontent.com/ziglang/zig/master/lib/std/mem/Allocator.zig) | Allocation, in-place resize and relocation have distinct contracts | Specify relocation, failure preservation and allocator identity separately |
| [Go pipelines and cancellation](https://go.dev/blog/pipelines) | Early receiver exit can strand senders; fixed workers bound parallel work | Define cancellation and producer unblocking together with queue limits |
| [LLVM language reference](https://llvm.org/docs/LangRef.html#well-formedness) | Parseable IR can still violate semantic invariants; verification checks those invariants | A future Fermi IR needs a verifier, not merely a parser and printer |

These implications are engineering judgments for Fermi. They are not claims that
Fermi implements Rust lifetimes, Zig allocators, Go scheduling or LLVM IR.
No implementation code was copied from these sources.

## Local findings

The driver loads modules, checks semantics, then runs effect/work/range analysis.
The current intermediate representation is a typed structured tree. Owner planning
reserves stable temporary slots; C lowering sequences evaluation and cleanup.
There is no separate CFG/SSA or general borrow-region solver.

The C runtime implements fallible reserve/resize, while source programs expose
fixed-length allocation and from_bytes copying only. Scalar read/edit views cannot
update a descriptor. Exposing growth by treating a slice as an owner would cross
an unresolved lifetime boundary. The next proposal therefore introduces a distinct
exclusive descriptor borrow and retains call-scoped views.

Foundry is useful for identifying future requirements, but its broad generic and
backend APIs are speculative. First acceptance should instead exercise one small
end-to-end native service. A durable multi-tenant queue would add storage and
security questions before the resource/concurrency foundations are validated.

## Priorities and unknowns

Finish descriptor growth before file/socket resources. Establish resource lifecycle
before task scheduling. Use bounded in-memory service tests before persistence.
Measure safety-preserving generated code before deciding on another backend.

Research does not establish demand or unique language advantage. User feedback,
Linux execution, service performance, native-stack bounds, source-mapped runtime
faults and a relocatable installation remain missing evidence or capabilities.
Further design work must target those gaps rather than expand the syntax inventory.

## Implementation follow-up

The local findings above record the pre-growth review. A later implementation on
the same development date added exclusive byte-buffer descriptor borrows and
source reserve/resize. See the [current buffer specification](../spec/buffers.md)
and [integration design](../design/buffer-borrows.md). General escaping borrows,
resource nesting and a control-flow IR are still outside that implementation.
