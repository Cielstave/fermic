# Scalar matches

`match` accepts integer and `bool` subjects in addition to enum and `Result` values. Scalar patterns are constants or `_`; each case body uses the existing scoped block syntax.

```fermi
func classify(code: u16) -> u8 {
    match (code) {
        case 200 { return 1; }
        case 404 { return 2; }
        case _ { return 0; }
    }
}

func admitted(ready: bool, capacity: bool) -> bool {
    match (ready) {
        case true { return capacity; }
        case false { return false; }
    }
}
```

## Pattern rules

Integer patterns are integer literals checked against the subject's exact type, including signed minimum values. A duplicate constant is rejected even when spelled differently, such as `0` and `-0`. Integer matches require a final wildcard because the type's full domain cannot be enumerated within the case limit.

A boolean match must cover both `true` and `false`, or end with a wildcard. The wildcard is unique and must be the last case. A wildcard after both boolean values is rejected as unreachable. Each match supports up to 128 cases. Ranges, guards, binding patterns, and fallthrough are not supported for scalar subjects.

The subject is evaluated once before dispatch, and exactly one body runs. A fault during subject evaluation exits before any case body. Branch initialization and ownership states join using the same rules as enum matches. Case bodies retain normal scope cleanup.

## Work and diagnostics

The work estimate includes the subject, dispatch, each literal evaluation and comparison, and the largest body. It does not claim a wall-clock bound. Missing coverage, duplicate patterns, misplaced wildcards, and unsupported scalar patterns report `E_MATCH`; out-of-range constants report `E_RANGE`.

The C backend lowers scalar cases to typed comparisons and an ordered branch chain. Pattern expressions are evaluated before the chain, preserving valid C `else if` structure and avoiding repeated subject evaluation.

`tests/scalar-match.sh` validates the feature at `-O0` and `-O2`, including every `i8` value, integer width boundaries, generic functions, single subject evaluation, flow joins, cleanup and diagnostic limits. See [the runnable example](../../examples/features/scalar_match/main.fe).
