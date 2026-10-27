# Data algorithms

```sh
./build/fermi run examples/features/data_generics/main.fe
./build/fermi test examples/features/data_generics/main.fe
```

Expected output: `data-generics: inventory and token checks passed`.

The example sorts fixed inventory records by ID and reorders parser-token records.
Both workloads share std.slice copy/fill/swap with explicit data type arguments.
Record copies are independent; source arrays are retained. The source uses no
allocation, callbacks or generic structs.
