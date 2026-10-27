# Dynamic nonresource data buffers

`Buffer<T>` now admits the same nonresource data types as `T: data`: integers,
bool, static str, records, nonresource payload enums and fixed-array aliases.
Resource-containing elements, Results and borrowed descriptors remain rejected.
The buffer itself is still a single owning resource with explicit moves, automatic
cleanup and call-scoped borrows. It is not copyable or part of the public C ABI.

## Construction and initialization

```fermi
use std.buffer;
struct Item { id: usize; name: str; }
let width: usize = buffer.element_size<Item>();
var items: Buffer<Item> = try(buffer.empty<Item>(width * 128));
let length: usize = try(buffer.push<Item>(edit items, Item(id = 7, name = "part")));
```

`empty<T>(limit)` creates a zero-length/capacity owner without payload allocation.
`element_size<T>()` reports native C representation size, including alignment
padding. It is effect-free and charged one abstract work unit in the callee plus
the existing call cost. It does not report string contents outside a descriptor,
allocator overhead or process RSS. Layout is target/toolchain dependent, not a
stable exported record ABI.

`resize_filled<T>(edit buffer, length, seed)` reserves storage, assigns the typed
seed to every newly exposed element, then publishes the new length. Existing live
elements are unchanged. Shrinking retains capacity; regrowth uses the new seed.
The seed is evaluated/passed even when shrinking or keeping the current length.
Private fields and enum tags are preserved by copying an existing valid value;
zeroed allocation bytes are never published as newly constructed records.

`allocate<T>` and zero-filling `resize<T>` remain restricted to integers. Calling
them with a record is rejected. `reserve<T>` supports data elements and changes
only capacity. A reserved slot beyond logical length remains inaccessible.

## Collection operations

Push, pop, extend and from_slice now support data. Aggregate items require normal
explicit copy/move at the call site. Push retains its geometric growth policy and
exact-length fallback for limit/size errors. Pop copies the last value and shrinks;
empty pop returns invalid_argument. Extend uses exact growth and preserves order;
empty extension does not read an element. From-slice creates independent storage.
Data elements need no individual destructor; resource-element storage requires a
separate ownership/cleanup design.

Lengths/capacity count elements; limits count allocated payload bytes using actual
sizeof(T), including padding. Size multiplication and PTRDIFF_MAX are checked
before allocation. Limits and allocator identity stay with the owner. Old and new
allocations can coexist during growth. Failed growth preserves contents, logical
length and capacity. It does not roll back effects used to evaluate arguments.

Read/edit data views work over buffers, and editing records requires mutable
storage. Calls cannot retain a view across reserve/growth. Passing an element of
the destination as a seed in the same edit call remains conservatively rejected;
copy it into a local first. Owners can be moved into resource structs/enums and
are released once on return, propagation, or checked faults. Shrinking does not
securely erase retained storage.

## Native boundary

The trusted std.buffer adapter has a closed checked signature set. Integer
allocate/resize retain eager finite validation (18 functions). Reserve, empty,
resize_filled, truncate and element_size are instantiated and validated for requested data
types. Ordinary collection functions also instantiate on request. Native generic
access remains restricted to this protected standard module; arbitrary C calls
and new native operations are not obtained by choosing a function/module name.

The raw C runtime accepts positive element widths and malloc-aligned allocation.
Raw zeroing is not typed construction. Native generated code starts records at
length zero and initializes them with typed stores before exposing new length.
Only resource-free data may use this protocol. Native work/heap bounds for growth
remain unknown; element_size and truncate are explicit constant-work exceptions. Other
native functions are never analyzed as empty pure AST bodies.

Run `sh tests/record-buffers.sh`. The
[example](../../examples/features/record_buffers/README.md) creates and consumes
2,048 records under an explicit byte budget.

## Reusing capacity

`buffer.truncate<T>(edit value, length)` shortens a buffer to an existing logical
length and returns that length. Requesting growth produces `invalid_argument`
without changing the owner. `buffer.clear<T>(edit value)` truncates to zero.
Empty buffers and requests retaining the same length succeed without indexing an
element. Both helpers preserve capacity and do not allocate; they support only
resource-free `data` elements. They return `Result<usize, Error>` consistently
with other fallible buffer operations. Subsequent growth still initializes each
newly exposed element; removed bytes are not securely erased.

`truncate` uses a checked native metadata operation: it neither reads/copies an
element nor calls an allocator. `clear` delegates to it, and `pop` uses it after
copying the result element. Truncation has no tracked external effects and costs
one native work unit in `fermi-work-1`, plus ordinary call/argument work. Borrowed
helpers using these operations can therefore satisfy `@effects()` and a checked
`@budget(work = N, heap = 0)`. Creating or dropping the owner still carries the
`memory` effect. Empty effects do not mean immutable inputs: `edit` remains explicit. The [batch example](../../examples/features/bufferReuse/README.md)
uses one reserved allocation across sixteen batches.
