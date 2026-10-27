# Current feature examples

These programs use implemented Fermi syntax and compile to native executables.
They demonstrate explicit effect ceilings, Result handling, resource ownership,
local modules and native tests. They do not depend on an application-specific C
host or proposed language features.

| Example | Features | Expected result |
| --- | --- | --- |
| [Reusable batches](bufferReuse/README.md) | Typed record buffers, capacity reuse, explicit Result handling | 128 records in sixteen batches |
| [Frame check](frame_check/main.fe) | `@effects()`, read views, endian decoding, typed structs, explicit error conversion | Four native tests pass |
| [Owned jobs](owned_jobs/main.fe) | `try`, resource struct inside a payload enum, whole-value moves, exhaustive matching, bounded allocation | Three native tests pass |
| [Integer domains](integer_domains/main.fe) | Signed negation, constrained generics, std.bits flags and high-bit handling | Three native tests pass |
| [Integer generics](integer_generics/main.fe) | One integer parameter, explicit specialization, local library and typed views | Three native tests pass |
| [File report](file_report/main.fe) | CLI arguments, buffer allocation, exclusive file creation, write-all, sync, consuming close | A new status file is written |

Run commands from the repository root after `make`. Use `./build/fermi` to select
the current checkout compiler.

## Frame check

```sh
./build/fermi run examples/features/frame_check/main.fe
./build/fermi test examples/features/frame_check/main.fe
./build/fermi explain examples/features/frame_check/main.fe
```

[header.fe](frame_check/header.fe) decodes a five-byte header: one version byte
and a big-endian u32 payload length. It accepts version 1 and payload lengths up
to 65536 bytes. Tests cover a valid header, truncated input, unsupported versions,
and an excessive length. This example validates header metadata only; it does
not receive network traffic or verify an accompanying payload.

The decoder permits no tracked external effects. Its `edit`-free interface is
read-only, while `@effects()` alone does not prove termination or absence of
faults. The caller permits `io.write` to print the final status.

Expected output: `frame-check: all header checks passed`.

## Owned jobs

```sh
./build/fermi run examples/features/owned_jobs/main.fe
./build/fermi test examples/features/owned_jobs/main.fe
```

[jobs.fe](owned_jobs/jobs.fe) copies a payload into a buffer capped at 64 bytes,
constructs an owning Job, and transfers it through Event.received. Matching moves
the active job into the branch; automatic cleanup releases its payload on exit.
The summary retains only scalar values. An idle variant and allocation-limit
failure are tested as well.

`@effects(memory)` permits allocation and cleanup without permitting file access
or standard I/O. The conservative effect set includes resource cleanup even for
an invocation using the idle variant. The program is sequential and creates no
threads, task runtime, or persistent queue.

Expected output: `owned-jobs: ownership checks passed`.

## File report

```sh
./build/fermi run examples/features/file_report/main.fe -- build/example-status.txt
```

Choose an output path that does not exist. [storage.fe](file_report/storage.fe)
creates an owner-only file, writes the report, synchronizes it and explicitly
handles consuming close through `try`/return. The storage helper permits only the
required filesystem effects; CLI, allocation and diagnostic effects stay in the
entry module.

Expected standard output: `file-report: report created`.

Expected file contents:

```text
status=ready
queued=0
```

The path buffer is limited to 4096 bytes. Existing destinations are refused and
retain their contents. Invalid arguments exit with 64; file failures exit with 74.
Diagnostics use standard error. A failed save may leave an incomplete new file.
File synchronization does not provide a transaction or persist the parent
directory. The application does not remove paths automatically.

## Expected compiler rejections

These sources are intentionally invalid:

```sh
./build/fermi check examples/features/rejected/hidden_output.fe
./build/fermi check examples/features/rejected/implicit_cleanup.fe
```

[hidden_output.fe](rejected/hidden_output.fe) demonstrates `E_EFFECT` for a helper
that introduces `io.write` below an empty ceiling.
[implicit_cleanup.fe](rejected/implicit_cleanup.fe) demonstrates `E_EFFECT` for
buffer ownership cleanup when `memory` is not permitted. Neither is a runnable
program. They must continue to fail checking.

## Verification

`sh tests/feature-examples.sh` runs frame check, owned jobs and file report at O0/O2,
including their native tests. It compares file contents and output, checks
overwrite refusal and invalid arguments,
and verifies both expected rejections. It is included in `make test` and
`make sanitize`.

## Integer function generics

```sh
./build/fermi run examples/features/integer_generics/main.fe
./build/fermi test examples/features/integer_generics/main.fe
```

The numeric module shares scalar ordering and slice search across integer widths.
Type arguments are explicit, for example `numeric.larger<i32>(-7, 4)` and
`numeric.contains<i16>(read values, 8)`. Each declaration is checked for all nine
integer types; this subset does not include generic structs or resource type
parameters. Run `sh tests/integer-generics.sh` for its O0/O2 integration suite.

## Signed and unsigned integer domains

```sh
./build/fermi run examples/features/integer_domains/main.fe
./build/fermi test examples/features/integer_domains/main.fe
```

The signed module uses signed_integer for checked negation and sign classification.
std.bits uses unsigned_integer for width-preserving flag changes. Tests cover the
signed helpers, u16 flags and the high u64 bit. Negating a signed minimum remains
a checked overflow fault. The example allocates no memory or file resources.

## UTF-8 strings

`strings/main.fe` demonstrates immutable UTF-8 `str`, byte lengths, scalar counting, content equality, and output through `std.io`. Its local `routes.fe` module classifies paths using prefix and suffix queries. `std.text.compare` provides typed lexicographic ordering. Run `sh tests/strings.sh` for O0/O2 coverage.

## Scalar match

`scalar_match/main.fe` demonstrates exhaustive integer and boolean matching with a final wildcard for integer domains. Run `sh tests/scalar-match.sh` for O0/O2 coverage.

## Type aliases

`type_aliases/main.fe` imports public aliases for a count, a string, and a record. Aliases preserve the original type identity, ownership, and mutability rules. Run `sh tests/type-aliases.sh` for native and visibility checks.

## Struct borrows

`struct_borrows/main.fe` records statistics into mutable array elements through a local module. `read Summary` inspects data, and `edit Summary` updates caller storage without whole-struct copies. Run `sh tests/struct-borrows.sh` for O0/O2 checks including resource fields and fault cleanup.
