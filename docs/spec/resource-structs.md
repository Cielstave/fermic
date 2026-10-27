# Resource-owning structs

Status: implemented for whole-value ownership, field replacement and call-scoped
buffer/file borrowing. [Call-scoped struct borrows](struct-borrows.md) now support
shared read and exclusive field updates. Partial moves and escaping references
remain unavailable.

## Declaration and construction

```fermi
use std.mem;

struct Packet {
    sequence: u32;
    payload: Buffer<u8>;
}

func create(sequence: u32) -> Result<Packet, mem.Error> {
    return Result.ok(Packet(
        sequence = sequence,
        payload = try(mem.bytes(4, 64))
    ));
}
```

A struct may contain buffers, files, earlier resource structs and payload enums.
Views, descriptor borrows, void and Result fields remain rejected. Nominal types
must be complete before use; recursive owning declarations are unavailable.
Constructors require every field, named in declaration order. Arguments evaluate
left to right. Existing owners require explicit `move`; resource values cannot
be copied. The constructor takes ownership of each field immediately after its
argument succeeds. If a later argument propagates an error or faults, initialized
fields are cleaned automatically.

Structs retain the limits of 128 fields and 65536 abstract cells (the sum of field
cells). Field type nesting is limited to 32 levels, including intervening aggregate
types. This bounds type traversal; it does not prove native stack usage.

## Access, mutation and borrowing

Reading scalar fields or copying resource-free fields preserves the enclosing
owner. Buffer indexing, length/capacity queries and byte views can use field paths,
including nested structs. Fields of a `let` owner remain immutable.

```fermi
var packet: Packet = try(create(1));
packet.payload[0] = 7;
let length: usize = try(mem.resize(edit packet.payload, 8));
```

A mutable struct's buffer/file field may be passed to an `edit Buffer<u8>` or
`edit fs.File` parameter. The field must belong to stable caller-owned storage;
a field of a temporary constructor is not borrowable. Existing descriptor
reborrows remain supported. Aliasing is checked conservatively using the whole
root binding: simultaneous exclusive borrows of different fields in one struct
are currently rejected. Use separate owners or an independent I/O buffer where
an operation needs multiple resources. Bounds and other arguments cannot consume
or reborrow the root while its storage is borrowed.

Replacing a field evaluates the replacement before releasing its previous value.
A fault or propagated error during evaluation leaves the old value owned until
normal exit cleanup. Whole-struct assignment and `move(packet)` follow the same
rules. `move(packet.payload)` is rejected; a field cannot be extracted independently.
A struct declared without an initializer must receive a complete value before any
field can be accessed or assigned. Field-by-field initialization is unavailable.

## Cleanup, effects and ABI

Resource fields are released in reverse declaration order at scope exit, return,
loop exit, replacement, propagation and language fault. Nested structs follow the
same order; enum fields clean only their active variant. Vacant temporary fields
need no release. Each resource aggregate has one generated cleanup helper, reused
by nested owners. User destructors are not introduced.

File cleanup remains best effort. Explicit consuming `fs.close` still requires a
whole File binding, so a File retained in a struct currently uses fallback cleanup.
Applications that must handle close errors should keep that File as a separate owner.

Memory and file-close effects include all owned fields, even if a particular path
does not allocate or open them. Owning structs can cross local Fermi module boundaries
and travel in Result success payloads or enum variants. Their C layout remains
internal; exported C interfaces reject aggregate owners. Arrays of resources and
nested resource Results remain unsupported.

Run `sh tests/resource-structs.sh` for O0/O2 cleanup-order, allocation-failure,
partial-construction, field-growth, replacement, loop-exit and fault tests. The
[owned packet example](../../examples/cli/owned_packet/main.fe) transfers a public
resource struct across modules and prints its payload.
