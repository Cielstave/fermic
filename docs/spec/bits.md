# Unsigned integer flags

Status: implemented as ordinary Fermi functions in `std.bits`.

```fermi
use std.bits;

let initial: u16 = 1;
let enabled: u16 = bits.set<u16>(initial, 4);
let allowed: bool = bits.has_all<u16>(enabled, 5);
```

Every function uses one `T: unsigned_integer` parameter, accepts explicit type
arguments, and is checked for u8, u16, u32, u64 and usize. Inputs must have the
same concrete type; signed arguments and implicit integer mixing are rejected.
Operations return new values and do not mutate their inputs.

| Function | Result |
| --- | --- |
| `has_all<T>(value: T, mask: T) -> bool` | Every mask bit is present |
| `has_any<T>(value: T, mask: T) -> bool` | At least one mask bit is present |
| `set<T>(value: T, mask: T) -> T` | Set the mask bits |
| `clear<T>(value: T, mask: T) -> T` | Clear the mask bits |
| `toggle<T>(value: T, mask: T) -> T` | Invert the mask bits |

A zero mask satisfies has_all and never satisfies has_any. Complement and results
retain T's width, including u8/u16. The module performs no allocation or external
operations. Each specialization carries `@effects()` and a verified work budget
of 100 abstract units; this is not a CPU-time guarantee.

The library has five ordinary templates, each supporting five unsigned types.
Only requested concrete instances are created and counted toward the existing
project function limit. Unrequested bodies have no checked effect/work certificate. No C intrinsics,
operator overloading, flags type, macro, or new keyword is required.

The [integer-domain example](../../examples/features/integer_domains/main.fe)
combines signed helpers and unsigned flags. Run `sh tests/integer-domains.sh` for
all widths, every u8 value, zero masks, high bits, invalid arguments and O0/O2
native validation. Sanitized execution is included in `make sanitize`.
