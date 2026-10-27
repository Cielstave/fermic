# Payload enums

Status: implemented in the native compiler. This extends fieldless enum matching;
existing fieldless enums retain their scalar representation and behavior.

## Declaration and construction

```fermi
struct Job { id: u32; priority: u8; }
enum Command {
    stop,
    submit(Job),
    count(usize),
}

func next() -> Command {
    return Command.submit(Job(id = 42, priority = 1));
}
```

A variant has either no payload or one explicit owning type. Use a struct for
multiple fields. A payload can be an integer, bool, fieldless enum, array, struct,
an earlier payload enum, Buffer<u8>, or std.fs.File. Views, owner borrows, void,
and direct Result payloads are rejected. Recursive owning declarations remain
unavailable: nominal types must be complete before use.

Construct payload variants with exactly one positional argument: `Command.count(8)`.
Construct fieldless variants without parentheses: `Command.stop`. Existing explicit
numeric and aggregate transfer rules apply. Imported constructors use
`module.Command.count(8)` and require a public enum.

At most 128 variants are allowed. An enum has at most 65536 abstract cells, counting
one tag cell plus the largest payload. Payload type nesting is limited to 32
levels, including intervening aggregate types; this is a compiler complexity limit,
not a native stack guarantee.

## Exhaustive matching

```fermi
func work(command: Command) -> usize {
    match (command) {
        case Command.stop { return 0; }
        case Command.submit(let job: Job) { return widen<usize>(job.priority); }
        case Command.count(let count: usize) { return count; }
    }
}
```

Every variant must occur exactly once, in any order. A payload case requires
`(let name: Type)` with the exact payload type. A fieldless case has no parentheses.
Each case has its own lexical scope. The subject is evaluated once; one branch
executes. Returning branches and continuing branches use the existing initialization
and ownership joins. Integer patterns, wildcards, guards, and nested destructuring
remain unavailable.

Matching a payload enum consumes a named subject, including when its current
variant is fieldless. Matching `copy(value)` preserves a non-resource enum.
Assignments, argument passing, and returns use explicit move/copy when transferring
an existing aggregate. Payload enums do not support equality or ordering operators;
inspect variants with match. Fieldless enums retain scalar equality.

## Resource lifetime

An enum may own a buffer or file, directly or through nested enum payloads.
Moving the enum transfers that owner. Matching transfers only the active payload
to its binding; payload bindings are immutable and can be moved into a mutable
local when needed. Resource enums cannot be copied.

Cleanup releases only the active resource variant, once, on normal scope exit,
return, loop exit, replacement, propagation, or fault. Ignored enum owners are
cleaned automatically. The compiler emits one cleanup routine per resource enum
type; shared nested variants reuse that routine instead of duplicating code. Partial mutations before an error remain visible. File
cleanup retains its existing best-effort close policy; use explicit fs.close when
a close failure must be handled. User-defined destructors are not introduced.

[Resource struct fields](resource-structs.md) are supported; arrays of resource
enums remain unsupported. Non-resource enums can appear in arrays and structs. `copy(place)` now accepts aggregate fields
and indexed elements as well as bindings; resources still cannot be copied, and
`move` still requires a whole binding.

## Representation, effects, and errors

The internal C representation uses a uint32_t tag and a union of payload fields.
Its padding and byte layout are not a public ABI. Payload enums, including Result
success payloads of these types, are rejected at exported C boundaries. Result
error types must remain fieldless enums. Existing scalar enum ABI layouts and
constants are unchanged.

Work analysis counts one tag cell plus the largest payload's cells, constructor
operand work and aggregate transfer, and the largest match branch including its
payload transfer. Match dispatch retains one comparison unit per case. Resource
effects conservatively include all variants, including memory release and file
close. A resource enum does not gain a pure budget by currently holding an empty
variant. These are abstract bounds, not elapsed-time or stack guarantees.

Invalid constructors use E_CALL, incompatible payload types use E_TYPE/E_RANGE,
invalid matches use E_MATCH, forbidden transfers use E_MOVE/E_INIT, and unsupported
exports use E_ABI. Invalid payload declarations use E_PARSE. Parenthesized cases
for fieldless variants now reach E_MATCH rather than the former E_PARSE.

Run `sh tests/payload-enum.sh` for native O0/O2, imported types, allocation failure,
active-variant cleanup, deep shared variants, loop exits, and fault regressions. The runnable
[event dispatcher](../../examples/cli/event_dispatch/main.fe) demonstrates a public
payload enum with a struct payload across local modules.
