# Exhaustive enum matches

Fermi supports `match` over fieldless enum values as well as Results. This is an
extension of the implemented [pilot grammar](pilot.md). [Payload enums](payload-enums.md)
now extend these rules with typed payload bindings and consuming matches. Scalar
subjects have separate [integer and boolean match rules](scalar-matches.md). Guards
and fallthrough remain unsupported.

```fermi
enum State { idle, running, stopped, }

func active(state: State) -> bool {
    match (state) {
        case State.idle { return false; }
        case State.running { return true; }
        case State.stopped { return false; }
    }
}
```

## Grammar and type identity

A case uses `case Enum.variant { ... }`; imported types use
`case module.Enum.variant { ... }`. Fieldless variants have no parentheses or
payload binding, consistent with variant expressions such as `State.idle`.
Result cases retain `Result.ok(...)` and `Result.err(...)`.

The case's enum must have the same nominal identity as the subject. An unrelated
enum with identically named variants cannot substitute for it. Imported enums
must be public and reachable through the current module's imports. Variant order
is unrestricted. Every variant must occur exactly once. Unknown, repeated,
missing, or wrong-type cases produce `E_MATCH`; malformed syntax produces
`E_PARSE`. The parser's existing limit of 128 variants per enum remains in force.

## Evaluation and control flow

The subject is evaluated once before dispatch. A fault during evaluation exits
before any case executes. Exactly one case body executes; bodies have separate
lexical scopes. Enums are scalar values, so matching does not move or invalidate
the subject binding. Mutating a `var` subject inside a case does not redispatch.

Initialization and ownership states join across branches that continue after the
match. Branches that return do not constrain subsequent initialization. A value
assigned only in some continuing branches remains unavailable afterward. An
exhaustive match whose cases all return satisfies the function's return-path
requirement; a statement immediately after it is unreachable.

Effects propagate from the subject and all case bodies. Work analysis counts the
subject, one unit for the match, one comparison unit per case as a conservative
bound, and the largest case body. No payload transfer is charged for enum cases.
Budget certificates remain abstract work bounds, not execution-time guarantees.

## Implementation and validation

The semantic checker resolves cases to enum variant fields. The C backend emits
comparisons against their ordinal values and reuses the existing branch cleanup
and function exit paths. Source enum representations and the C ABI are unchanged;
this feature does not enable arbitrary integer-to-enum conversion.

`tests/control-flow.sh` checks generated native libraries at `-O0` and `-O2`,
including single evaluation, initialization joins, repeated enum use, nested loops,
and a multi-file executable with an imported enum. Semantic tests cover invalid
cases, return coverage, nested Result error matching, and the 128-variant limit.
