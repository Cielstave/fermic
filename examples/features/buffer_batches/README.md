# Integer buffer batches

```sh
./build/fermi run examples/features/buffer_batches/main.fe
```

Expected output: `25` followed by a newline.

`buffer.from_slice<i64>` copies an initial array into independent owned storage.
`buffer.extend<i64>` appends a second array in one fallible growth operation.
The 40-byte payload limit permits exactly five i64 elements. Errors propagate
through Result, and resource cleanup follows the normal ownership rules.
