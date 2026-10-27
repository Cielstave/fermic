# Nonresource data generics and views

`T: data` extends on-request function instantiation to integers, bool, static str,
nonresource structs/enums, and fixed arrays. Results, resource owners and borrowed
views/descriptors are excluded. There is one explicit type parameter; generic
structs, multiple parameters, traits and implicit inference remain unavailable.

```fermi
func duplicate<T: data>(value: T) -> T { return copy(value); }
func first<T: data>(items: read T[]) -> T { return copy(items[0]); }
```

Calls name a concrete visible type or transparent alias, such as
`duplicate<Item>(copy(item))`. Canonical type identity reuses instances; nominal
structs with identical fields in different modules remain different types.
`fermi explain` formats qualified argument names and array shapes.

Each requested body is checked in its defining module. Access to a private field
cannot be acquired by instantiating a helper with an imported type. Whole-value
copying of an existing nonresource opaque value is permitted. The constraint does
not assert arbitrary operators or members: those must exist on the requested
concrete type. Unrequested bodies have syntax/declaration checking, not semantic
or resource certification. Source, type, node, arena and function limits remain.

The parser uses an unnameable, nonresource record for the preliminary data-body
parse. It is not an instance, user-visible value, or zero constructor for T.
`read T[]` and `edit T[]` describe views; bare `read T`/`edit T` require struct
arguments when the instance is parsed. Intrinsic type constructors retain their
own restrictions. Results still require a known fieldless error enum; abstract
error constraints are not introduced.

## Copy and ownership

`copy(place)` now also accepts scalar data places. This lets one body explicitly
copy both scalar and aggregate values. Resource copies remain rejected. Move
still requires a whole aggregate binding; it is not scalar assignment. Copy takes
a binding/field/index place, not an arbitrary temporary expression. Existing
explicit aggregate transfer requirements remain active.

No resource-containing record becomes data because it is private, aliased, nested
or stored in an enum payload. Borrowed descriptors do not become owning values.
No escaping reference, dynamic string-to-str conversion or implicit allocation is
introduced.

## Data views and std.slice

Read/edit array views now admit data elements, including records, enum payloads,
strings and fixed-array aliases. They remain call-scoped and require initialized,
stable storage. Read access propagates through elements, fields and slices; edit
access requires mutable storage. Existing conservative alias rules still apply.

```fermi
use std.slice;
// All operations use an explicit concrete type argument.
slice.fill<Item>(edit items, copy(seed));
let count: usize = try(slice.copy_into<Item>(read input, edit output));
try(slice.swap<Item>(edit items, 0, 1));
```

`copy_into` returns the copied length and checks output length before mutation.
`Error.output_too_small` preserves all output; trailing output is unchanged.
`swap` checks both indices first; `Error.invalid_index` preserves all elements.
Swapping an index with itself succeeds. `fill` copies the seed into each element.
The library performs no allocation and declares empty effect ceilings. It is not
an overlapping-memory move API; callers must satisfy view alias rules.

Exported C views retain integer/bool elements. Aggregate/string views and buffer
owners are rejected at that boundary until an appropriate ABI and lifetime
contract exists. Concrete scalar wrappers may use data views internally.

Run `sh tests/data-generics.sh`. The executable
[example](../../examples/features/data_generics/README.md) sorts inventory records
and reorders parser-token records using the same ordinary generic library.
