# Function effect contracts

Status: implemented for the currently tracked external and resource effects.

## Declaration

An optional `@effects(...)` attribute sets the upper limit on a function's inferred
effects. The empty list prohibits every tracked effect. Without the attribute,
effect inference continues unchanged and no explicit ceiling is imposed.

```fermi
@effects()
func checksum(input: read u8[]) -> u32 {
    var total: u32 = 0;
    for (var index: usize = 0; index < input.len; index += 1) {
        total = (total + widen<u32>(input[index])) % 65536;
    }
    return total;
}
```

Supported names are `io.read`, `io.write`, `memory`, `env.read`, `process.args`,
`clock.read`, `fs.open`, `fs.read`, `fs.close`, `fs.write`, and `fs.sync`.
Unknown or repeated names and repeated attributes are parse errors. Trailing commas
are rejected. Attributes precede `pub`/`export`/`func`; `@effects` and `@budget`
may appear together in either order, each at most once. Native resource type
declarations cannot carry function attributes.

## Checking and resource ownership

After ordinary type/ownership checking, inference reaches a fixed point over all
reachable calls, including recursive calls and local imports. The inferred effect
set must be a subset of the declared ceiling. Unused permissions do not force an
effect or propagate to callers: actual inferred effects propagate. A helper change
that introduces a prohibited effect is rejected when the project is rechecked.
Every declared function is checked, including functions unused by the entrypoint.
Inference is conservative: both branches contribute, including a branch guarded
by a constant false condition. No path-sensitive effect elimination is promised.

Automatic cleanup counts. A function taking an owning buffer requires `memory`
even if its body is empty. Owning files require `fs.close`; nested resource structs,
enums, and Result payloads retain their cleanup effects. Borrowed descriptors carry
no ownership cleanup, but called read/write/growth operations still contribute.

```fermi
@effects(memory)
func release(buffer: Buffer<u8>) -> void {}
```

A violation produces `E_EFFECT` at the function declaration, naming a forbidden
effect. When a call in that body contributes the effect, a related location names
that call. The witness identifies one contributor, not a full call trace. Cleanup
alone can violate a ceiling without a contributing call expression.

## Relationship to budgets and safety

Effect ceilings do not require a statically bounded work count. A loop with an
unknown bound or a recursive function can satisfy `@effects()`. `@budget` retains
its stricter contract: no tracked effects, a proven work bound, and heap zero.
An unused `memory` permission cannot bypass those checks when memory is used.

These are closed-project compiler checks, not capability tokens, effect handlers,
a runtime sandbox, request quotas, or restrictions on foreign host code. There is
no effect polymorphism or stable separately compiled effect interface yet.

The empty set is not a general purity or determinism proof. Mutation through
`edit` views, checked arithmetic/bounds faults, divergence, native stack use, and
elapsed time are not represented by these effect names. No termination, race,
latency, or absence-of-fault guarantee follows from this attribute.

## Reports, compatibility and tests

`fermi explain` reports inferred effects and, for annotated functions, permitted
effects. Export contract JSON gains `allowed_effects`: an array for an explicit
ceiling, including `[]`, or `null` for inference without a declared ceiling.
`effects` continues to report actual inference. This is an additive schema field;
exported C signatures and runtime behavior are unchanged. Annotations do not
introduce runtime checks or disable existing checks.

Run `sh tests/effect-contracts.sh` for every tracked effect, recursive imported
helpers, diagnostic locations, JSON metadata and O0/O2 native tests. Semantic tests
cover malformed attributes, resource cleanup, unknown work and budget composition.
The [runnable example](../../examples/cli/effect_contracts/main.fe) includes an
unbounded checksum, an allocating wrapper and a statically budgeted function.
