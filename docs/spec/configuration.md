# Startup configuration and decimal input

Implemented subset, 25 October 2026. These libraries support regular service
startup; they do not implement a task runtime or configuration-file format.

## Environment bytes

```fermi
use std.env;
// env.read_into(read name, edit output) -> Result<usize, env.Error>
```

Names must match `[A-Za-z_][A-Za-z_0-9]*` and contain 1–128 ASCII bytes.
This is Fermi's portable-name policy, not an operating-system size limit.
Values are copied as bytes, without UTF-8 decoding or a trailing NUL. Success
returns the copied length; bytes after that length remain unchanged. Present but
empty values succeed with zero. Absent values return `Error.not_found`.

| env.Error | Meaning |
| --- | --- |
| not_found | The variable is absent |
| invalid_name | The name violates the portable-name policy |
| output_too_small | The entire value does not fit |

Every domain error preserves the destination. The adapter validates name and value
length before copying; it never silently truncates or returns a pointer into libc.
An empty destination can accept an empty value but cannot accept a nonempty one.
The value scan inspects at most output length plus one bytes. Environment lookup
itself is libc work and has no certified time/heap bound. The adapter uses a
129-byte key array and performs no explicit heap allocation.

Environment data is externally supplied. Copying it does not make it trustworthy
or safe to publish. Native hosts must serialize environment access/mutation while
these calls run and must not use environment backing storage as output. The
current Fermi API has no environment setter or concurrency facility. Prefer reading
configuration once during startup and passing validated values onward.

## Decimal usize parsing

`std.num.parse_usize(bytes: read u8[])` returns `Result<usize, num.Error>`.
It accepts 1–20 decimal ASCII digits on the 64-bit pilot, including leading zeros.
Signs, whitespace, radix prefixes, separators, NUL and non-ASCII bytes are rejected.
More than 20 bytes is rejected even if the value consists only of leading zeros.
Overflow is a domain error checked before multiplication/addition, not saturation.

`std.num.usize_range(bytes, minimum, maximum)` additionally checks an inclusive
interval. An inverted interval is rejected before parsing. Error precedence is
invalid interval, empty input, excessive length, invalid digit/overflow as encountered,
then out-of-range value.

| num.Error | Meaning |
| --- | --- |
| empty | No digits |
| too_long | More than 20 bytes |
| invalid_digit | A byte outside ASCII 0–9 |
| overflow | The decimal value exceeds usize |
| invalid_range | minimum exceeds maximum |
| out_of_range | A valid usize falls outside the interval |

Both functions are ordinary Fermi code with `@budget` certificates and zero heap.
Current computed work is 1181 and 1219 units of `fermi-work-1`; declared ceilings
are 1500 and 2000. These figures describe the current abstract cost model, not CPU
time, native-stack bounds, or real-time deadlines. Checked arithmetic remains on.

## Required numeric environment settings

```fermi
use std.config;

func workers() -> Result<usize, config.Error> {
    return config.usize_env(read "FERMI_WORKERS", 1, 32);
}
```

`usize_env` validates the interval, reads into a fixed 20-byte array, parses, then
checks the inclusive range. It neither grows storage nor installs a default.
Its errors are `missing`, `invalid_name`, `value_too_long`, `invalid_value`,
`invalid_range`, and `out_of_range`, in that order. Empty or overflowing values
are invalid_value. More than 20 bytes is value_too_long. Raw values never enter
library diagnostics; callers choose how to report errors.

The function carries transitive `env.read`. It cannot receive a static work/heap
certificate, even though its Fermi implementation allocates no heap. Pure parsing
of an already-copied value remains separately certifiable. This distinction is
visible in `fermi explain` and contract manifests.

## Startup example

`examples/cli/startup_config.fe` requires FERMI_WORKERS, FERMI_JOB_BYTES and
FERMI_ACTIVE_BYTES, then checks that active payloads fit the configured allowance.
It prints acceptance on stdout, or a fixed field/constraint diagnostic on stderr.
It exits 0 on acceptance, 78 on rejected configuration, and 74 on output failure.
It validates a profile only; it does not start worker threads or enforce process RSS.

```sh
./build/fermi build examples/cli/startup_config.fe -o build/startup-config
FERMI_WORKERS=4 FERMI_JOB_BYTES=1024 FERMI_ACTIVE_BYTES=4096 ./build/startup-config
```

The active-byte allowance excludes queue storage, metadata, allocator overhead,
transport buffers and stacks. Those need their own limits in a future service.

## Verification and limits

`tests/configuration.sh` runs generated code at O0/O2, covers destination guards,
missing/empty values, invalid and 128-byte names, short destinations, non-UTF-8
values, ABI rejection, 5000 differential decimal cases and CLI startup failures.
It also checks standard-library shadow isolation, effects and static-budget
rejection. `make sanitize` instruments compiler and generated programs.

OS access currently uses libc getenv. General argument access, environment mutation,
text strings, file configuration and structured logging are not implemented.
See [the research record](../research/2026-10-25-startup-configuration.md) for the
upstream contracts informing this API.

## Signed decimal input

`std.num.parse_i32(bytes)` and `std.num.parse_i64(bytes)` return the corresponding
signed integer in `Result<T, num.Error>`. Both consume the whole byte view. The
accepted grammar is an optional single ASCII minus followed by 1–20 ASCII
digits. Leading zeros and `-0` are accepted. A plus sign, whitespace, separators,
NUL, non-ASCII digits, radix prefixes, and trailing characters are rejected.
The 20-digit limit excludes the optional minus sign; exceeding it returns
`too_long`, even for zeros. No allocation or input mutation occurs.

Error precedence is empty input or a lone minus (`empty`), excessive digit
length (`too_long`), then `invalid_digit` or i64 `overflow` at the first offending
byte. The i32 function first parses as i64 and then checks the i32 range;
therefore a later invalid byte takes precedence over i32 range overflow when the
whole number is within i64. Range failure maps to the existing `overflow` case.
No raw input is logged by either function.

Negative accumulation checks the signed lower bound before multiplication and
subtraction, allowing `-9223372036854775808` without an overflowing intermediate.
Positive input uses the negative of the maximum positive value as its lower
bound. The current work bounds are 1459 for i64 and 1483 for i32, with declared
ceilings of 3000 and 4000 and zero heap use. These are abstract work certificates.
The unsigned parsing API and its error ordering remain unchanged.
