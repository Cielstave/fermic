# Signed decimal amounts

This example parses two integer amounts, subtracts them with checked arithmetic,
and formats the negative adjustment without allocation. Values represent integral
minor units, not floating-point currency. Arbitrary application arithmetic still
needs its own range policy; parsing alone does not guarantee subtraction fits.

```sh
./build/fermi run examples/features/signed_numbers/main.fe
./build/fermi test examples/features/signed_numbers/main.fe
sh tests/signed-decimal.sh
```

Expected output:

```text
-1250
```

The second native test formats and parses the minimum i64 value. The integration
suite checks integer boundaries, every i16 value, all 256 byte values within a
numeric input, 10,000 seeded signed values, decimal-power transitions, and output
capacities from zero through 24 bytes. Expected text comes from C `snprintf`.
Errors preserve the destination; neither parsing nor formatting allocates.
