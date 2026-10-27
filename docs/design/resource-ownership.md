# Resource ownership and buffer integration

Status: byte-buffer integration implemented, 25 October 2026. Allocation, growth,
exclusive descriptor borrowing, move, views, Result transfer and automatic cleanup
are integrated into the compiler. See [the implemented subset](../spec/buffers.md).
Wider scalar buffers remain runtime-only; resource nesting is still restricted.
This document specifies the integration gate for stage 1 of [the backend plan](../PLAN.md).
Descriptor access and its compiler representation are detailed in the
[buffer-borrow design](buffer-borrows.md).

## Runtime contract implemented now

`runtime/include/fermi/buffer.h` defines an internal native interface, not a stable
extension ABI. `runtime/memory/buffer.c` implements creation, exact reservation,
resize, move, and drop for scalar widths 1/2/4/8. These widths cover current scalar
representations; the runtime does not implement user generics or nested resources.

Each slot starts as `{0}`. A vacant slot has no cleanup obligation. A successfully
created buffer owns its allocator, logical length, capacity, scalar width, and
capacity-byte limit. Zero-length creation is valid and does not call the allocator.
It still establishes an owner that can later grow. Move transfers the complete
descriptor into a vacant destination and vacates the source without allocation.
Drop frees live storage and vacates the slot; dropping a vacant slot is harmless.
Native clients must never shallow-copy owners or modify descriptor fields.

Creation checks element width, multiplication overflow, `PTRDIFF_MAX`, and the
configured capacity limit before allocating. Live elements start with zero bits.
Reserve grows capacity to the requested value without changing length. Resize
zeroes newly exposed elements, including values hidden by an earlier shrink.
Shrink retains the allocation; it neither securely erases data nor shrinks capacity.

Growth follows allocate → copy live prefix → commit descriptor → release old
allocation. Failure before commit leaves the pointer, length, capacity, allocator,
limit, and live data unchanged. Spare capacity is not readable or initialized.
Successful growth invalidates every view into the previous allocation.

The limit applies to requested payload capacity, not total process memory. During
growth both old and new allocations are live; transient payload memory can approach
twice the per-buffer limit. Allocator overhead, concurrent owners, and OS accounting
are additional costs. There is no implicit doubling or zero-size allocator call.
A future service-level allocator may enforce aggregate quotas; none is claimed here.

Allocator hooks are stored by value with their context. They must supply distinct
malloc-aligned storage or NULL; release must not fail. The context must outlive
its owners. Hooks are trusted native code and must not reenter an owner operation.
There are no global hooks, thread-safety guarantees, or application-specific callbacks.

## Ownership lowering model

The current typed tree cannot safely acquire heap owners merely by adding a C
`free` at the end of a function. Cleanup must cover nested scopes, partial argument
evaluation, Result payloads, branches, loops, returns, and the common fault exit.
The initial resource IR should represent the following operations explicitly:

| Operation | Required transition |
| --- | --- |
| Vacate slot | Establish an uninitialized owner slot with no drop obligation |
| Initialize owner | Successful creation installs an owner; failed allocation leaves the slot vacant |
| Move owner | Transfer to a vacant slot; clear the source obligation after transfer |
| Drop owner | Release only an initialized owner and clear its obligation |
| Begin/end view | Track read or exclusive edit access to the owner for a call |
| Exit scope | Drop initialized locals in reverse declaration order and temporaries in reverse creation order |
| Return owner | Move into the caller result slot before cleaning up callee slots |
| Fault edge | Clean up active temporaries and locals before propagating the original fault |

These names describe lowering operations, not source-language keywords. The current
implementation annotates the typed tree with function-scope owner temporaries and
emits explicit transfers, source invalidation, and cleanup; it does not add a CFG.
Use stable owner slots and explicit control-flow edges; do not retain cleanup
pointers to C temporaries whose lexical scope has ended. Where initialization
status differs across branches, runtime drop flags are required unless analysis
proves a fixed state. Static move checking still rejects invalid source operations;
runtime flags are not permission to read a maybe-initialized value.

A successful `Result<Buffer<T>, E>` owns its payload. Matching the success variant
transfers the payload to the arm binding and clears the Result's obligation. The
error variant has no buffer payload to drop. Existing mandatory Result handling
continues to apply. The first integration should reject resource-containing structs,
arrays, partial moves, and nested resource containers until their cleanup paths
have explicit tests; generic derivation must not accidentally enable `copy`.

## Evaluation and failure rules

Arguments retain left-to-right evaluation. A moved owner waiting in an argument
slot must be dropped if a later argument faults before the call begins. Once the
callee accepts the slot, the caller must no longer clean it up. Return values are
transferred before local cleanup so an escaping owner is not freed by its callee.

For owner replacement, evaluate the new value first, then drop the previous owner,
then commit the new owner. No new owner is installed on an allocation error. A
fault still takes the function cleanup edge. Native buffer release cannot itself
raise a Fermi fault, allocate, or replace the fault already being propagated.
Future file-resource close errors need a separate explicit-close contract.

## Borrow and effect rules

Read views may coexist; an edit view is exclusive. Moving, dropping, replacing,
reserving, or resizing an owner requires no outstanding views. Initially views
remain call-scoped: they cannot be stored, returned, or retained by native code.
A resize operation needs exclusive access to the owner descriptor, not merely an
`edit u8[]` view. That distinction is represented by TY_BUFFER_EDIT and resolved access metadata;
the parameter carries a pointer to the caller descriptor.

Allocation, growth, and resource release contribute memory effects transitively.
No allocating function may keep a `heap: 0` certificate from the existing pilot.
Budgeted functions reaching unmodeled memory effects are rejected; ordinary
service functions may have unknown bounds. Runtime callbacks and allocator timing
are not bounded by the scalar work model. Concurrency/cancellation must later use
the same cleanup edges; they are not part of the current runtime implementation.

## Integration acceptance gate

Full stage-1 acceptance requires generated-program tests for all of:

1. Scope exit, early return, nested block exit, loop iteration exit, and faults.
2. Successful/failed allocation, failed growth preserving old data, and empty owners.
3. Move into/out of helpers, Result matching/forwarding, and failure in later arguments.
4. Branch-dependent initialization/move, owner replacement, and exact-once release.
5. Rejection of copy, use-after-move, stale views, and resizing during a borrow.
6. Memory-effect propagation and rejection of unsupported budget claims.

The byte-buffer subset implements the scope, allocation, transfer, borrow, growth
preservation and effect cases above. tests/cases/buffers.fe and its guarded native
harness exercise descriptor reborrows, failure paths and old-plus-new peak bytes.

Every allocation site must support deterministic fault injection. Run generated
programs at O0/O2 with sanitizer coverage and compare live-allocation counters at
all exits. The runtime tests are prerequisites; they do not satisfy this compiler
gate on their own.

## Design reference

The separation between initialization analysis and cleanup insertion is informed
by [rustc drop elaboration](https://rustc-dev-guide.rust-lang.org/mir/drop-elaboration.html):
initialization creates cleanup obligations, moves remove them, and control-flow
joins can require runtime flags. Fermi's first integration deliberately limits
resource nesting and borrow lifetimes; this is not an implementation of Rust's
full ownership model or syntax.

## Compiler integration progress

The C backend now routes normal returns, fault exits, void fallthrough, and
native I/O returns through a shared function epilogue. Function storage is
initialized before the entry fault check, and return values use a function-scope
slot. See [function exit lowering](function-exits.md) for the implemented contract
and generated-program tests. A subsequent integration added stable temporary slots, source invalidation, and
resource cleanup for fixed-length byte buffers and their Results. Generated-program
tests cover scope order, replacement, call handoff, fault cleanup, and allocation
failures. Descriptor borrowing and native reserve/resize now extend this subset;
borrowed descriptors are excluded from owner planning/drop obligations.
