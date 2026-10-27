# Bitwise operations and binary integers

Status: implemented for the native 64-bit pilot. These operations extend the
[pilot grammar](pilot.md) without changing checked arithmetic or ownership rules.

## Integer operations

`&`, `|`, `^` and unary `~` operate on unsigned integers: `u8`, `u16`, `u32`, `u64`,
and `usize`. Binary operands must have the same type; the result retains that type.
Signed integers, booleans and enums are rejected with E_TYPE. Literals use their
normal contextual type; an unconstrained literal still defaults to i64.

`<<` and `>>` take an unsigned left operand and a usize shift count. The result
has the left operand's type. A count must be below its bit width: 8, 16, 32 or 64.
A directly invalid literal is rejected with E_CONST. Other invalid counts raise
fault code 4, FERMI_FAULT_SHIFT, before any C shift executes. Counts are never
masked or reduced modulo the width, including when the left operand is zero.

Left shift discards bits above the result width; right shift inserts zero bits.
Complement affects exactly the operand width. This is explicit bit manipulation;
`+`, `-`, and `*` retain overflow faults. Both operands are evaluated once, left
to right. Bitwise operators evaluate both operands, unlike logical && and ||.

Precedence from lowest to highest is:

| Operators | Meaning |
| --- | --- |
| `||` | Logical OR |
| `&&` | Logical AND |
| `\|` | Bitwise OR |
| `^` | Bitwise XOR |
| `&` | Bitwise AND |
| `== !=` | Equality |
| `< > <= >=` | Ordering |
| `<< >>` | Shifts |
| `+ -` | Addition and subtraction |
| `* / %` | Multiplication, division and remainder |

Binary operators are left associative. Unary operators bind more tightly, with
postfix operations strongest. Parenthesize masks before comparisons:

```fermi
func has_flag(flags: u32, bit: usize) -> bool {
    let one: u32 = 1;
    return (flags & (one << bit)) != 0;
}
```

Bitwise compound assignments and signed bitwise operations are not available.
An invalid count follows the existing fault epilogue and releases active owners;
earlier writes to edit views remain visible. No implicit rollback is introduced.
Each operation costs one abstract work unit plus its operands under fermi-work-1.
It introduces no heap allocation or external effect.

## Standard binary library

`use std.binary;` loads the compiler-provided Fermi implementation. Project files
cannot replace this module. Its public API is:

```fermi
pub enum Error { input_too_short, output_too_small, }
pub func read_u32_be(input: read u8[]) -> Result<u32, Error>;
pub func read_u32_le(input: read u8[]) -> Result<u32, Error>;
pub func write_u32_be(value: u32, output: edit u8[]) -> Result<usize, Error>;
pub func write_u32_le(value: u32, output: edit u8[]) -> Result<usize, Error>;
```

Reads inspect the first four bytes. A shorter input returns input_too_short;
extra bytes are ignored. Writes check capacity first, then replace exactly the
first four bytes and return 4. A shorter output returns output_too_small without
changing any byte. Empty views are valid and produce the corresponding domain
error. No terminator, alignment requirement, pointer reinterpretation or native
struct layout enters the encoding. Use slices to select a field at an offset.

The public functions have finite @budget certificates, zero heap use and no
external effects. Private low-byte extraction masks before a checked narrowing;
the narrowing failure arm is unreachable for every u32 value. These properties
apply to the library functions, not to arbitrary parsing workloads or CPU time.
The API currently covers unsigned 32-bit fields only, not whole-object serialization.

See [binary_frame.fe](../../examples/cli/binary_frame.fe) for executable usage.
Run `sh tests/bitwise.sh` for operator and encoding tests at O0/O2. The full
sanitizer suite also verifies owner release on shift faults.
