# Dynamic integer buffers

This document describes integer behavior within the broader
[data buffer contract](record-buffers.md). All nine integer types remain supported.

```fermi
pub native func allocate<T: integer>(length: usize, limit: usize) -> Result<Buffer<T>, Error>;
pub native func reserve<T: data>(value: edit Buffer<T>, capacity: usize) -> Result<usize, Error>;
pub native func resize<T: integer>(value: edit Buffer<T>, length: usize) -> Result<usize, Error>;
```

Lengths and capacity count elements; the payload limit counts bytes. Creation
and newly exposed elements initialize to zero. Growth allocates before releasing
old storage; failure preserves length, capacity, and contents. Shrink retains
capacity; regrowth zeroes newly exposed elements. Reserve returns capacity and
does not change length. Resize returns length. Growth is exact, without implicit
doubling. Limits remain attached to the owner and are not process RSS bounds.

Integer buffers retain moves, cleanup, checked bounds and scalar views. Owners
cannot be copied, exported through the C ABI or stored in fixed arrays. Resource
structs and payload enums may own a buffer. std.mem remains byte-only.

Allocate and zero-filling resize are integer-only. Reserve supports nonresource
data. The two integer-native declarations expand to 18 functions; other data
adapters and ordinary collection functions instantiate on request. The trusted
adapter uses a closed checked signature set, described in
[record buffers](record-buffers.md). Memory effects remain explicit.

## Appending individual integers

```fermi
pub func push<T: data>(value: edit Buffer<T>, item: T) -> Result<usize, Error>;
```

`push` appends one scalar and returns the resulting length. It is ordinary Fermi
library code, not a native intrinsic. Existing capacity is reused. At capacity,
it requests double the previous element capacity, with a minimum of four; if
that request exceeds a byte/object-size bound, it retries the exact new length.
Allocator refusal is returned without retrying a smaller allocation. Domain
errors before growth preserve the owner. After successful reserve, resize within
capacity and scalar assignment require no further allocation. Checked index and
arithmetic rules remain enabled.

The existing primitives still allocate/reserve exact sizes. Their caller chooses
capacity policy, and `push` supplies one reusable policy. Geometric growth reduces
repeated copying during individual appends; it does not impose a latency guarantee
or a process-memory cap. Old and new allocations can coexist during growth.
The 18 allocate/resize native instances remain eager. Ordinary push/pop bodies are now created
only for requested types, and count toward the concrete function limit.

`buffer.Error` uses the same variant names as `mem.Error`, but retains its own type
identity. `try` does not implicitly convert between them. See the
[executable example](../../examples/features/typed_buffers/README.md) and run
`sh tests/typed-buffers.sh` for width, failure, ownership, effect, and CLI checks.

When the appended item comes from the same buffer, first read it into a scalar
binding, then pass that binding to push. Existing alias rules reject additional
uses of an exclusively borrowed owner in the same call. The new storage API does
not weaken those rules or keep a view alive across growth.

## Taking the last integer

```fermi
pub func pop<T: data>(value: edit Buffer<T>) -> Result<T, Error>;
```

Pop copies the last scalar, shrinks the logical length, and returns that scalar.
Capacity and the original byte limit are retained. Empty input returns
`Error.invalid_argument` without changing the owner. Shrinking a valid owner
requires no allocation; any resize error is propagated before publishing the
scalar. A later push can reuse retained capacity. This supplies stack operations
without adding a separate collection type or implicit underflow fault.

## Copying and appending slices

```fermi
pub func extend<T: data>(value: edit Buffer<T>, items: read T[]) -> Result<usize, Error>;
pub func from_slice<T: data>(items: read T[], limit: usize) -> Result<Buffer<T>, Error>;
```

`extend` appends all input elements in order and returns the total resulting
length. Existing elements retain their values. Length overflow is checked before
addition, and element-width multiplication and byte limits are checked by resize.
Insufficient limits or allocation failure preserve contents, length, and capacity.
Existing capacity is reused; growth requests exactly the required length, without
push's geometric spare-capacity policy. No allocation occurs for empty input or
when capacity suffices. Repeated small extends may copy repeatedly; callers can
reserve capacity first when the final size is known.

`from_slice` copies into independent owned storage with the given payload-byte
limit. Mutating the source afterwards does not change the buffer. Nonempty input
requires one payload allocation; empty input creates an allocation-free owner.
Construction failure publishes no owner and follows ordinary cleanup rules.

Arrays, slices, and another buffer's read view may supply input. Borrow checking
rejects a view of the destination itself, including an empty view, because growth
could invalidate that view. Copy to independent storage first when duplicating
contents from the same owner. Different integer element types require explicit
conversion; neither operation reinterprets bytes or silently narrows integers.

Both operations are ordinary Fermi templates supporting all nine integer types.
Importing std.buffer creates 18 eagerly validated integer-native functions. Each requested
extend/from_slice/push/pop instance uses one additional concrete function slot
under the existing 128-function limit. Unrequested data instances are absent. Nonresource data generics and record
storage are specified in [data generics](data-generics.md) and
[record buffers](record-buffers.md). See the executable
[batch example](../../examples/features/buffer_batches/README.md).
