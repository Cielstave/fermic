# Bitwise lowering and binary data foundation

The immediate requirement is reusable byte processing for application protocols,
file formats and flags. It does not require a networking runtime or a new protocol
keyword. Operations belong to the integer language subset; byte order belongs to
a library implemented in Fermi.

## Primary reference

The [WG14 C11 draft N1570](https://www.open-std.org/jtc1/sc22/wg14/www/docs/n1570.pdf),
sections 6.3.1.1, 6.3.1.3, 6.5.3.3 and 6.5.7, specifies integer promotions,
unsigned conversion, complement and shifts. In particular, a C shift count at
least the promoted operand width is undefined. Small unsigned operands can be
promoted to signed int; directly emitting their shifts would not express a
consistent fixed-width Fermi operation. Signed shifts also carry representability
and implementation-defined behavior that the pilot need not expose.

## Implemented decisions

Fermi accepts unsigned bitwise operands only. Counts are usize and must be less
than the Fermi operand width, independently of C promotion. Lowering evaluates
operands into temporaries, checks the count, converts operands to uint64_t before
shifting and converts the result back to its Fermi unsigned type. Complement
likewise executes in uint64_t before conversion. The unsigned conversions retain
the low bits. Left shift is deliberately a bit operation with discarded high
bits; ordinary arithmetic remains checked.

The native ABI gains fault code 4 without changing existing codes. Faults use the
existing shared epilogue. Budget analysis conservatively retains ordinary type
bounds for these expressions; it does not treat a shift as checked multiplication
or introduce a new range-proof exemption.

std.binary implements explicit big/little-endian u32 fields using indexed byte
access and Fermi operations. Capacity is checked before writing. No native
pointer cast, unaligned load or allocation is needed. This is a useful composition
test for the language and provides building blocks for libraries beyond backend
services. It is not evidence that networking or general serialization exists.

## Abuse cases and validation

Test width-equal and enormous counts, zero operands with invalid counts, high-bit
left shifts, complement width, signed/bool/enum misuse and mixed integer types.
Verify eager left-to-right evaluation, short-circuit separation, existing operator
precedence and nested type delimiters. A shift fault with a live allocation must
release it once; allocation failure must still precede the shift.

Binary tests use independent expected byte vectors, every byte in each position,
short and oversized views, empty/null ABI inputs, sentinel preservation and
repeatable mixed values. Both optimization levels and sanitizers are required.
Native stack bounds, CPU timing and untested deployment targets remain outside
these results.
