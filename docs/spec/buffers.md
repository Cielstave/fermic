# Owned byte buffers

The [integer-buffer extension](typed-buffers.md) additionally supports all nine
integer element types through std.buffer. This document retains the byte-oriented
std.mem contract; its existing signatures and errors are unchanged.

Implemented subset, 25 October 2026. `Buffer<u8>` owns a heap allocation with a
mutable logical length and capacity. It is an intrinsic type, not support for
user-defined generic types. Nonresource record elements are supported by
[typed-seed buffers](record-buffers.md). Custom source-level allocators and
resource-containing collections are not available yet.

## Allocation

```fermi
use std.mem;

func length_of_buffer(length: usize) -> Result<usize, mem.Error> {
    match (mem.bytes(length, 65536)) {
        case Result.ok(let bytes: Buffer<u8>) {
            return Result.ok(bytes.len);
        }
        case Result.err(let error: mem.Error) {
            return Result.err(error);
        }
    }
}
```

`mem.bytes(length: usize, limit: usize)` returns
`Result<Buffer<u8>, mem.Error>`. It zero-initializes every element. The capacity
limit is in bytes; creation rejects a length above that limit before allocating.
Lengths above `PTRDIFF_MAX` are rejected as a size overflow. Empty creation succeeds
without calling the allocator, including when the limit is zero.

`mem.Error` has these variants, in declaration order:

| Variant | Meaning |
| --- | --- |
| `no_memory` | The allocator refused the request. |
| `size_overflow` | The requested size exceeds the runtime's addressable-size limit. |
| `limit` | The request exceeds the supplied capacity-byte limit. |
| `invalid_argument` | Reserved runtime validation failure; valid generated calls cannot produce it. |
| `invalid_state` | Reserved owner-state failure; valid generated calls cannot produce it. |

Allocation errors remain domain errors and must be handled or forwarded. The
limit is per buffer, not a process-wide quota. It excludes allocator overhead,
compiler-generated stack storage, and other live allocations.

## Copying borrowed bytes

`mem.from_bytes(source: read u8[], limit: usize)` returns
`Result<Buffer<u8>, mem.Error>`. It allocates an independent buffer with the
source length and copies every byte, including zero bytes. Strings, arrays,
buffer projections and slices can supply the read view. The source remains
borrowed only for the call; the returned owner may outlive it.

The limit and errors are the same as for `mem.bytes`. An empty source requires
no allocation. Limit rejection and allocation failure leave the source unchanged.
Successful copying uses one allocation; both source and destination may be live
at once. The per-buffer limit does not bound that combined memory use.

This function is implemented in Fermi using allocation, checked indexing and
explicit ownership transfer. It does not add a compiler intrinsic or change the
prohibition on implicit buffer copies. It carries a transitive memory effect.
See `examples/cli/owned_bytes.fe` for an executable example.

## Capacity and growth

`mem.reserve(buffer: edit Buffer<u8>, capacity: usize)` returns
`Result<usize, mem.Error>` with the resulting capacity. It does not change length.
Requests within current capacity succeed without allocation and return that existing
capacity. When allocation is required, growth is exact; there is no hidden doubling.

`mem.resize(buffer: edit Buffer<u8>, length: usize)` returns
`Result<usize, mem.Error>` with the resulting logical length. Newly exposed bytes
are zero, including after shrink/regrow. Shrinking retains capacity and does not
securely erase data. Only indices below length are accessible.

Both operations retain the owner, allocator, configured limit and live data on
failure. Growth allocates before releasing old storage: peak payload bytes can
reach old capacity plus new capacity. The buffer limit bounds each final capacity,
not this transient sum or process RSS. Overflow and capacity-limit checks precede
allocation. Handle or forward the Result before leaving scope.

## Exclusive descriptor borrows

A parameter `buffer: edit Buffer<u8>` borrows the owning descriptor exclusively;
it does not take ownership. Calls require explicit `edit buffer` and a stable
mutable buffer binding, a buffer field in a mutable struct, or an existing descriptor borrow. Nested reborrows return
access to the caller when the call finishes. A borrowed parameter may read len
and capacity, access elements and lend scalar views. It cannot copy, move, replace,
return or release the owner. Only the original owner carries cleanup obligations.

~~~fermi
func extend(buffer: edit Buffer<u8>) -> Result<usize, mem.Error> {
    let length: usize = buffer.len;
    return mem.resize(edit buffer, length + 16);
}
~~~

Descriptor borrows are parameters only: no local bindings, fields, arrays, Result
payloads, return values or exported C parameters. General object borrows and read
descriptor parameters are not implemented. This is distinct from `edit u8[]`,
which borrows element storage and cannot update length/capacity.

A call cannot borrow a descriptor together with any other access to the same
owner, including disjoint slices or nested argument expressions. Compute length
and capacity arguments first; `mem.resize(edit buffer, buffer.len + 16)` is rejected.
Index/slice bounds and assignment values cannot reborrow their target's descriptor:
those expressions may otherwise relocate storage after its address was prepared.
Perform descriptor operations in a preceding statement and create fresh views later.

See `examples/cli/buffer_growth.fe` for a complete executable using reserve,
resize and a user-defined reborrow helper.

## Ownership and access

Named buffers require `move(value)` when passed, assigned, or returned. `copy` is
rejected for buffers and Results containing buffers. Matching a resource Result
consumes it and transfers a successful payload to the arm binding. Moving clears
the source cleanup obligation; the checker rejects subsequent reads.

`let` and owning parameters are immutable. To edit an owned payload, move it into a
`var bytes: Buffer<u8>` binding. `bytes.len` and `bytes.capacity` cannot be assigned; indexing and slicing
check their bounds. Index and slice-bound expressions must not consume the
storage owner. Zero-length slices do not perform arithmetic on a null pointer.

A buffer can be passed as `read bytes` or `edit bytes` to a scalar-view parameter.
Read views may coexist; edit access requires mutable, exclusive storage. Views
remain call-scoped and cannot escape through locals or return values. A call
cannot both borrow and move the same owner, including through another argument's
nested expression. This also includes bounds evaluated for a different view
argument: borrowing one owner does not permit consuming it while constructing
another owner's slice. Evaluate owner-dependent bounds before the call. Repeated
read views and statically disjoint edit slices remain supported when their bounds
do not perform other uses of a borrowed owner. Existing conservative alias checks
still apply.

## Cleanup and faults

Initialized locals are released in reverse declaration order at scope exit,
including loop iterations and match arms. Loop-initializer owners live until the
loop exits. A return transfers its value before cleanup. Faults release live
expression temporaries and locals before propagation. Vacant, moved, and empty
slots do not cause an allocator release.

Arguments evaluate left to right. If a later argument faults, the caller releases
owners already prepared for that call. After invocation, the callee owns resource
arguments, including on its fault paths. Owner replacement evaluates the new value
before releasing the old owner. Explicit `bytes = move(bytes)` preserves ownership.
A failed allocation handled before replacement leaves the original owner intact.

The current implementation supports buffers and `Result<Buffer<u8>, E>`, where
`E` is a fieldless enum. [Owning struct fields](resource-structs.md) and payload
enums may contain buffers. Resource arrays, nested resource Results, partial
moves, and resource types in the exported C ABI are rejected.

## Effects, artifacts, and tests

Allocation, growth and release contribute the transitive `memory` effect. Such functions
report unknown heap/work bounds and cannot satisfy a `@budget(..., heap = 0)`
annotation. Reading a borrowed view or descriptor alone does not acquire a memory
effect. Descriptor length/capacity are conservatively unknown to work-bound
refinement; no immutable-length range proof is attached to them.

The compiler embeds its trusted runtime sources in generated C with internal
linkage. Generated C and objects do not require a separately linked Fermi runtime.
The compiler itself still needs the configured standard-library and runtime source
directories. Neither this internal layout nor the new library API is a stable ABI.

`sh tests/buffers.sh` checks O0/O2 execution, exact release counts and order,
nested/loop scopes, branch-dependent owners, moves and Result forwarding,
replacement, bounds faults, and faults before/after argument handoff. Its guarded
allocator injects failures at every allocation in a four-buffer chain and checks
that no allocation remains live. `make sanitize` runs these paths with ASan/UBSan;
semantic tests reject invalid copies, moves, borrows, types, and budget claims.
A separate C ABI test links two independently generated resource objects to check
that embedded runtime symbols remain private.

Run the executable example with:

```sh
./build/fermi build examples/cli/buffer.fe -o build/buffer-example
./build/buffer-example
```

Expected stdout: `Fermi` followed by a newline. Generated-program growth tests cover
no-op reserve, empty growth, limit/overflow rejection, shrink/regrow zeroing,
allocator failure, nested reborrows, return transfer, faults and repeated growth.
They check live and peak payload bytes as well as release counts/order. Wider
resource forms remain outside this byte-buffer integration.
