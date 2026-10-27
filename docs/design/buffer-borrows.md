# Exclusive buffer descriptor borrowing

Status: implemented byte-buffer subset, 25 October 2026.
The [buffer specification](../spec/buffers.md) defines the supported source contract.
This document records implementation decisions and the integration acceptance matrix.

## Source contract

Implemented declarations in std.mem:

~~~fermi
pub native func reserve(buffer: edit Buffer<u8>, capacity: usize)
    -> Result<usize, Error>;
pub native func resize(buffer: edit Buffer<u8>, length: usize)
    -> Result<usize, Error>;
~~~

reserve returns the resulting capacity, which may exceed the request if already
allocated. It does not change length. resize returns the resulting logical length.
Both preserve pointer, length, capacity, allocator, limit and live bytes on failure.
Growing length initializes new bytes to zero; shrinking retains capacity and does
not securely erase data. Growth remains exact when allocation is necessary, with
no hidden doubling. Capacity and length are distinct quantities.

The syntax edit Buffer<u8> borrows a descriptor, not its elements. Initially it is
allowed only on parameters; read descriptor borrows, local borrow bindings, returns,
fields, arrays of descriptors and exported C parameters are excluded. Generic
object borrowing is not implied. No new keyword is required.

Call sites require edit and stable mutable storage. Passing a descriptor onward
is a reborrow: the outer function retains no cleanup obligation for the owner.
It may access the descriptor again after the nested call returns. A callee cannot
move, copy, replace or drop the borrowed owner. Reading len/capacity, indexing,
slicing and lending element views are permitted under the same exclusive access.
The capacity property is read-only and available on owners and descriptor borrows.

## Call-scoped exclusivity

A descriptor borrow conflicts with every other access to the same owner in the
call, including read slices, edit slices, moves and nested argument evaluation.
Disjoint element slices do not make descriptor mutation safe. Retain the pilot's
conservative rule requiring other owner uses to be evaluated before the call.

~~~fermi
// Compute the scalar before lending the descriptor.
let length: usize = buffer.len;
let resized: Result<usize, mem.Error> = mem.resize(edit buffer, length + 16);
// Handle resized before continuing.
~~~

Reject resize(edit buffer, buffer.len + 16); compute length first. Reject any call
that borrows both the descriptor and one of its views. Once the call returns, a
fresh view sees the new allocation. Persisted views remain forbidden, so a stale
view cannot be represented in safe source. Nested borrow conflicts must be checked
before accepting the enclosing call, regardless of argument order.

## Compiler representation and lowering

Descriptor access uses TY_BUFFER_EDIT for both type identity and resolved argument
access; TY_EDIT retains its scalar-view interpretation. Use stable binding identity
for alias comparison. Preserve the distinction through type formatting, checking,
effect analysis, ownership planning, lowering and ABI rejection.

The generated parameter carries a pointer to the caller's FermiBuffer descriptor.
It is not an owner, is not copied into an owning temporary, and never enters a drop
list. Reborrow forwards that pointer. Element projection reads current data/length
from the descriptor when the view is created. Growth cannot occur during that view's
call lifetime. Caller storage remains the sole cleanup owner on success or fault.

User helpers containing a descriptor borrow need not allocate, but actual growth
calls carry transitive memory effects and cannot satisfy heap: 0. Audit work/range
analysis so unknown mutated length is never treated as an invariant across calls.
Do not attach C alias/nonnull assumptions without evidence valid on every path.

Native validation accepts only the exact trusted std.mem signatures and error enum.
Result storage is ordinary scalar-result storage; never reserve an owning Buffer
slot for a growth result. Runtime failure status maps to the existing mem.Error
variants. No borrowed descriptor escapes through the public C ABI.

## Implementation mapping

| Area | Implementation |
| --- | --- |
| Type identity and restrictions | frontend/parse.c, frontend/types.c |
| Mutable access, aliases and relocation hazards | analysis/check.c |
| Pointer/reborrow and fresh view projection | ir/lower.c |
| Non-owning borrow temporaries and cleanup | ir/ownership.c |
| Trusted signatures and native calls | analysis/check.c, backend/c/memory.c |
| Conservative descriptor bounds | analysis/budget.c; ranges.c excludes these owners |
| Source API and example | library/std/mem.fe, examples/cli/buffer_growth.fe |

Borrow arguments do not reserve owning temporary slots. Scalar growth Results use
ordinary value temporaries. Existing owners still carry all cleanup obligations.
The implementation keeps the typed structured tree; no CFG/SSA is introduced.

## Acceptance matrix

| Area | Required evidence |
| --- | --- |
| Access | Reject immutable owner, temporary owner, move/copy/drop through borrow |
| Aliases | Reject owner plus view in both orders, nested growth and disjoint-slice bypass |
| Lifetimes | Reject return/local/field/export borrow; accept nested reborrow |
| Success | Empty growth, reserve without length change, resize within/spanning capacity |
| Initialization | Shrink/regrow exposes zero bytes, not stale hidden data |
| Failure | Limit, size overflow and every allocator failure preserve old descriptor/data |
| Cleanup | Early return/fault after growth frees final allocation exactly once |
| Evaluation | Fault in later argument never invokes growth or frees caller ownership |
| Analysis | Transitive memory rejection and no stale length/range proofs |
| ABI | Independently generated objects link without exporting runtime internals |

Run generated cases at O0/O2 with ASan/UBSan, allocator guards and release counters.
Compare old and new descriptors on failed growth. Include tests for a no-op reserve
while failure injection is armed: it must not allocate. Check peak transient bytes
against old plus requested capacity, not just the final buffer limit. The supported byte-buffer contract
is now in spec/buffers.md. Broader resource forms remain separately scoped.
