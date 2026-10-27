# Decimal formatting

Status: std.fmt provides allocation-free decimal ASCII formatting of usize, i32, and i64.
It is ordinary Fermi code, with no formatting syntax or native adapter.

```fermi
pub enum Error { output_too_small, }
@budget(work = 3000, heap = 0)
pub func usize_into(value: usize, output: edit u8[]) -> Result<usize, Error>;
```

Success writes the canonical decimal representation at the start of output and
returns its byte count. Zero is one digit, 0. There is no sign, grouping, locale,
padding or NUL terminator. The current 64-bit usize requires at most 20 bytes.
Bytes after the returned count remain unchanged. The view remains owned by the
caller and is not retained.

The implementation determines the required length before touching the output.
If capacity is insufficient, it returns output_too_small and preserves the whole
destination. Empty output always produces this domain error. A private digit
helper uses remainder by ten before checked narrowing, making narrowing failure
unreachable. No unsigned truncating cast is added to the language.

The function has no external effects and zero heap use. Its current inferred
work bound is 1322 under fermi-work-1; the requested 3000 is a conservative ceiling.
These are abstract model units, not timing or native-stack guarantees. The two
fixed counting loops cover all 64-bit values without an unbounded loop or scratch
allocation.

```fermi
use std.fmt;

func length_text(value: usize, output: edit u8[]) -> Result<usize, fmt.Error> {
    return fmt.usize_into(value, edit output);
}
```

Callers can pass the written prefix to std.io, a protocol encoder or another
byte-processing function. Formatting has no I/O effect itself. Floats, hexadecimal
formatting, templates and arbitrary object serialization are not yet provided.

The generated-program integration test compares boundary values, decimal powers
and 5000 repeatable values against C snprintf at both O0 and O2. Each case varies
output capacity and checks surrounding sentinel bytes. Run sh tests/arguments.sh.

## Signed integers

```fermi
pub func i64_into(value: i64, output: edit u8[]) -> Result<usize, Error>;
pub func i32_into(value: i32, output: edit u8[]) -> Result<usize, Error>;
```

Signed formatting uses a leading ASCII `-` only for negative values. Zero formats
as `0`. Maximum lengths are 20 bytes for i64 and 11 for i32, including the sign.
No NUL terminator is written. Like usize formatting, insufficient capacity
preserves every destination byte; success preserves the unused suffix.

The implementation keeps the magnitude nonpositive, including while extracting
digits. It never negates the minimum signed value. Two fixed loops determine
capacity before any write and emit decimal digits without heap allocation.
The i32 entry point widens losslessly and delegates to the i64 implementation.
Declared work ceilings are 4000 and 5000; computed bounds on the current model
are 1339 and 1347, respectively. These certificates do not promise wall-clock
latency or native stack bounds.

Run `sh tests/signed-decimal.sh` for boundary, exhaustive small-integer parsing,
seeded differential formatting against C `snprintf`, and output sentinel checks
at both optimization levels. The [signed-number example](../../examples/features/signed_numbers/main.fe)
prints a negative adjustment and tests the minimum i64 round trip.
