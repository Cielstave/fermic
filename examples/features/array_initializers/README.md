# Repeated fixed-array initialization

This executable counts distinct byte values using a fixed lookup table:

```fermi
var seen: bool[256] = [false; 256];
var output: u8[20] = [0; 20];
```

Run from the repository root:

```sh
./build/fermi run examples/features/array_initializers/main.fe
./build/fermi test examples/features/array_initializers/main.fe
sh tests/array-repeat.sh
```

Expected output is `10`, the number of distinct byte values in `fermi compiler`.
The helper intentionally accepts at most 32 input bytes for its work certificate;
longer input returns `Error.input_too_long`. It counts bytes, not Unicode characters.

The [library system](../../library_system/README.md) also uses this feature to
initialize its catalog, members, loans, reservations, and audit slots. Each empty
record constructor runs once and its result is copied into independent slots.
See the [language contract](../../../docs/spec/array-initializers.md) for ownership,
evaluation order, fixed limits, and rejected forms.
