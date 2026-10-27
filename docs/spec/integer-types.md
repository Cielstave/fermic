# Integer types

The native 64-bit pilot provides signed and unsigned integers at each fixed
width. Types are explicit and nominally distinct, including `u64` and `usize`.
This extends the [pilot](pilot.md); it does not introduce floating-point types,
implicit numeric promotions, or integer suffixes.

| Type | Range | C representation |
| --- | --- | --- |
| `i8` | -128 to 127 | `int8_t` |
| `u8` | 0 to 255 | `uint8_t` |
| `i16` | -32768 to 32767 | `int16_t` |
| `u16` | 0 to 65535 | `uint16_t` |
| `i32` | -2147483648 to 2147483647 | `int32_t` |
| `u32` | 0 to 4294967295 | `uint32_t` |
| `i64` | -9223372036854775808 to 9223372036854775807 | `int64_t` |
| `u64` | 0 to 18446744073709551615 | `uint64_t` |
| `usize` | 0 to 18446744073709551615 on the pilot target | `size_t` |

## Operations and conversions

Arithmetic operands have the same type. Addition, subtraction, multiplication,
and their compound assignments fault on overflow. Division truncates toward
zero; division or remainder by zero faults. Both division and remainder of a
signed minimum by -1 fault on overflow, including for `i8`. Negating a signed
minimum also faults. These checks remain enabled at `-O0` and `-O2`.

Unsigned bitwise operations work at all four widths and on `usize`. A shift
count is always `usize`, even when the left operand is `u64`. Counts must be
below the operand width. Left shifts intentionally discard high bits; see
[binary operations](binary.md).

`widen<T>(value)` requires the entire source range to fit in T. For example,
`i8` to `i16`, `u16` to `i32`, and `u32` to `u64` are lossless. `i8` to `u16`
and `u64` to `i64` are not. `u64` and `usize` can explicitly widen to each other
on the current 64-bit target but cannot be assigned implicitly.

`narrow<T>(value)` returns `Result<T, CastError>`, with `out_of_range` when the
actual value does not fit. Failed conversions do not reinterpret negative values
as large unsigned numbers. Literal typing and range checks follow the contextual
rules in the pilot specification; unconstrained integer literals default to `i64`.

```fermi
func sequence(value: u32) -> u64 {
    return widen<u64>(value);
}

func byte_delta(value: i64) -> Result<i8, CastError> {
    return narrow<i8>(value);
}
```

## Storage and analysis

All integer types are valid scalar function parameters, return values, struct
fields, fixed array elements, and `read`/`edit` view elements. Generated C headers
use the representations above. Existing view alignment, size, and alias checks
apply at the ABI boundary. Source `Buffer` storage remains limited to `Buffer<u8>`.

The work analyzer recognizes the unsigned `u16` upper bound. An unconstrained
`u64` upper bound equals the analyzer's unknown sentinel, as for `usize`;
explicitly bounded values can still receive finite work certificates. No change
to signed range proof capabilities or native stack guarantees is implied.

## Compatibility and validation

`i8`, `u16`, and `u64` are now prelude names. Earlier experimental sources using
these as user-defined type/function names or module aliases must rename them.
Regenerate compiler outputs; internal type identifiers are not stable ABI names.

`tests/cases/integer_types.fe` and its C integration test exercise the new types
at `-O0` and `-O2`. Tests cover every i8 operand pair for checked arithmetic,
unsigned boundaries, conversions, shifts, aggregates, and mutable u16 views.
