# Implemented language specification

Status: implementation index, 26 October 2026. This index consolidates existing
contracts; it does not introduce a second grammar or approve proposal syntax.

## Normative documents for the current subset

| Area | Contract |
| --- | --- |
| Native executable reuse and invalidation | [Run cache](run-cache.md) |
| Command help, build-and-run and argument forwarding | [CLI](cli.md) |
| Complete fixed-array initialization and repeated values | [Array initializers](array-initializers.md) |
| Loop exits, continuation and resource cleanup | [Loop control](loop-control.md) |
| Transparent names for existing types | [Type aliases](type-aliases.md) |
| Owned UTF-8 text and private struct fields | [Owned text](owned-text.md) |
| Immutable UTF-8 string values and text helpers | [Strings](strings.md) |
| Fixed-width integers, conversions and C representations | [Integer types](integer-types.md) |
| Width-preserving unsigned flags | [Bits](bits.md) |
| Nonresource records, algorithms and data views | [Data generics](data-generics.md) |
| Dynamic nonresource record storage and seeded initialization | [Record buffers](record-buffers.md) |
| Finite integer function specialization | [Integer generics](integer-generics.md) |
| Function effect ceilings and transitive checking | [Effect contracts](effect-contracts.md) |
| Native test execution | [Test runner](test-runner.md) |
| Shared/edit struct access without transfer | [Struct borrows](struct-borrows.md) |
| Struct ownership and field cleanup | [Resource structs](resource-structs.md) |
| Payload enums and resource variant cleanup | [Payload enums](payload-enums.md) |
| Exhaustive fieldless enum matches | [Enum matches](enum-matches.md) |
| Exhaustive integer and boolean matches | [Scalar matches](scalar-matches.md) |
| Explicit error propagation and cleanup | [Result propagation](error-propagation.md) |
| Unit-success Results and required handling | [Unit Results](unit-results.md) |
| Grammar, values, control flow, checking | [Pilot](pilot.md) |
| Modules, visibility, executable entry and native library mode | [Modules](modules.md) |
| Standard input/output and effect behavior | [I/O](io.md) |
| Unsigned bit operations and endian fields | [Binary](binary.md) |
| Monotonic timestamps and pure deadline arithmetic | [Time](time.md) |
| Process arguments and startup context | [Arguments](process-arguments.md) |
| Allocation-free decimal output | [Formatting](formatting.md) |
| Startup environment, decimal input and ranges | [Configuration](configuration.md) |
| Opaque file ownership, reading and consuming close | [Files](files.md) |
| Dynamic integer storage and fallible append | [Typed buffers](typed-buffers.md) |
| Owned bytes, fallible copying and growth | [Buffers](buffers.md) |
| Diagnostic format and source locations | [Diagnostics](diagnostics.md) |
| Abstract work estimates and unknown bounds | [Work model](work-model.md) |
| Supported proof-based check elimination | [Range proofs](range-proofs.md) |

The buffer and module documents extend the pilot where explicitly stated. The
telemetry document describes a particular application protocol, not general Fermi
syntax. Earlier v0.2 design sketches are not the current language reference.

## Current language boundary

Explicit integer/bool types, fixed arrays, structs with resource ownership,
fieldless and payload enums, restricted Result with `try(...)` propagation, explicit aggregate transfers and call-scoped
scalar views are supported. fs.File owns a regular file opened for reading or exclusively created for writing,
with call-scoped borrowing, explicit sync and consuming close. Buffer<T> owns
growable nonresource data storage; edit Buffer<T> parameters borrow its descriptor.
std.mem retains byte interfaces and std.buffer supplies finite integer operations. Allocation, copying and growth are fallible
and carry memory effects. Checks for arithmetic
and indexing remain enabled at O0 and O2 unless justified by implemented proofs.

Payload enums can own buffers or files, including through nested variants. Their
layout is internal; exported ABI parameters/results remain scalar or integer/bool views; aggregate
views and owners are excluded.
Result error types remain fieldless enums.

Owned UTF-8 `std.text.Text` is implemented as a resource struct with private
bytes and explicit fallible library operations. It does not convert to static `str`.

Integer and nonresource data function generics with explicit arguments are supported.
Generic structs, resource type parameters, partial field moves, escaping references, callbacks, concurrency,
networking and package resolution remain unavailable. Foundry uses several of
these as proposals and is intentionally outside compiler acceptance tests.

## Extension requirements

A language change must specify accepted grammar, type identity, initialization,
mutability, ownership/alias behavior, evaluation order, errors, effects, lowering,
ABI exposure and diagnostics. Include positive and negative examples and tests
at O0/O2. Resource changes also require deterministic failure and cleanup evidence.

Keep API proposals in design/ until implemented. Record compatible extensions
and breaking changes explicitly. An example under examples/design/ is not evidence
that the compiler accepts it. An internal runtime C function is not automatically
an available standard-library API.
