# Call-scoped struct borrows

A function may inspect or update a caller's struct without copying or consuming it:

```fermi
struct Counter { value: u32; }

func value(counter: read Counter) -> u32 { return counter.value; }
func add(counter: edit Counter, amount: u32) -> void { counter.value += amount; }

func example() -> u32 {
    var counter: Counter = Counter(value = 2);
    add(edit counter, 3);
    return value(read counter);
}
```

## Access and lifetime

`read S` is a shared, immutable borrow of struct S. `edit S` is an exclusive borrow
that permits field updates. Both use ordinary field syntax. The caller must supply
explicit read/edit access and initialized, stable storage: a binding, struct field,
or fixed-array element. Borrowing a temporary constructor or function result is
rejected. An edit borrow requires mutable storage or an existing edit borrow.

An edit parameter can be reborrowed as read or edit; a read parameter can only be
reborrowed as read. Read access remains immutable through nested fields, arrays,
and resource projections. Borrowed parameter descriptors cannot be reassigned.
Non-resource fields may be explicitly copied, but a borrowed struct descriptor
cannot be copied or moved as an owning struct value.

Borrowed values remain parameter-only: local borrow bindings, return values,
struct fields, enum payloads, Result payloads, and arrays of borrowed values are
rejected. Type aliases preserve these restrictions. This subset introduces no
escaping references, lifetime annotations, reference cycles, or partial moves.

## Aliasing and evaluation

Repeated shared borrows of the same root binding are accepted. Combining an edit
borrow with another overlapping borrow is rejected. Disjoint struct fields and
array indices under one root are not yet proved independent, so these cases are
rejected conservatively. Distinct root parameters inherit the caller's access
obligations. Evaluate other uses of a borrowed root into separate locals before
passing them as additional arguments.

Index expressions cannot mutate or consume their storage root. An assignment
through a borrowed field cannot evaluate a right-hand expression that takes an
edit borrow of that root. These restrictions preserve storage selected before
later argument or assignment-value evaluation.

## Resources and representation

Borrowing never transfers cleanup responsibility. The caller retains its owner
when the function returns or faults. Resource fields can be borrowed for existing
buffer/file operations; edit struct borrows can replace resource fields using the
normal move and old-value cleanup rules. Ordinary field projections retain the
existing conservative resource-effect inference.

Internal generated C uses a pointer to S, with a const-qualified target for read
borrows. The compiler preserves that qualification in nested field/array addresses.
The pointer itself requires no heap allocation or resource cleanup. Struct borrows
are not exposed through the public C ABI. Existing bounds and arithmetic checks
remain enabled at O0/O2.

## Compatibility and validation

Aliases of borrowed types can no longer be wrapped in arrays. Such arrays could
previously bypass the parameter-only restriction; their rejection closes a lifetime
hole rather than establishing a supported container representation.

`tests/struct-borrows.sh` covers shared/edit reborrows, nested fields, array elements,
resource growth/replacement, and caller cleanup after a borrowed bounds fault at
O0/O2. Semantic cases reject alias conflicts, readonly writes, escaping references,
mutable upgrades, temporary storage, and borrowed arrays. The
[grouped-record example](../../examples/features/struct_borrows/main.fe) uses a
local module and mutable fixed-array elements to accumulate statistics.

## Slicing array fields

A slice of an array reached through a shared struct borrow is a read view.
The access remains shared through nested struct fields and fixed-array elements.
Passing that slice to a read-view parameter must compile without discarding C
const qualifiers. Passing it as an edit view remains rejected. A slice reached
through an edit struct borrow can still be edited under existing alias rules.
The inventory name validation and snapshot writer exercise this composition.
