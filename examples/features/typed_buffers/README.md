# Dynamic integer storage

This executable takes a runtime element count, appends signed samples to an owned
`Buffer<i64>`, and pops them in reverse order to compute their sum. The buffer starts empty and grows as samples
are produced. The caller supplies a 4096-byte payload limit; admitted counts are
1–128. Counts describe elements, while allocation limits describe bytes.

```sh
./build/fermi run examples/features/typed_buffers/main.fe -- 8
sh tests/typed-buffers.sh
```

Expected output is `-4`, the sum of samples -4 through 3. A count of 9 prints `0`.
Invalid counts return status 65 with a diagnostic. Argument and storage failures
remain explicit and no binary data is manually packed into a byte buffer.

```fermi
var values: Buffer<i32> = try(buffer.allocate<i32>(0, 4096));
let length: usize = try(buffer.push<i32>(edit values, -17));
```

Import `std.buffer` for allocate/reserve/resize/push/pop. All nine existing integer
types are supported. Growth failures preserve stored values, new elements are
zero-initialized, and cleanup follows existing ownership rules. `push` uses
geometric reservation with an exact-size fallback at byte/object limits.
`std.mem` keeps its existing byte-only interfaces.

The suite checks every integer width and signed extrema, reserve/resize behavior,
shrink/regrow zeroing, exact byte limits, overflow before allocation, allocator
failure, bounds-fault cleanup, push capacity reuse/fallback, module shadowing,
readonly storage, cross-type rejection, LIFO order, and empty-pop handling. The CLI output is checked against
an independent integer-series formula at counts 1, 8, 9, and 128 at O0/O2.

This does not provide general generics, record buffers, or resource-element lists.
See the [typed buffer contract](../../../docs/spec/typed-buffers.md).
