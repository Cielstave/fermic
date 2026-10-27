# Dynamic record storage

```sh
./build/fermi run examples/features/record_buffers/main.fe
```

Expected output: `2096128` followed by a newline.

The program pushes 2,048 records, then pops them and sums their identifiers.
Each record contains an integer and a static str. Its payload limit is calculated
with buffer.element_size<Part>(), including native padding. Growth errors are
explicit; the remaining buffer owner is cleaned up on failure.

Records containing owned text, buffers or files are not admitted as elements yet.
This example demonstrates nonresource elements; it does not claim resource-element
cleanup, unlimited storage, stable record ABI or a process-memory quota.
