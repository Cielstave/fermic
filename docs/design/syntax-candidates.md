# Resource-oriented syntax candidates

Status: design proposals, not accepted grammar or implemented compiler features.
Existing programs continue to use @budget, @effects, Result, try and match.

The concrete needs are bounded parsing work, restricting effects inside a larger
function, and reducing resource-acquisition boilerplate. Familiar braces,
parenthesized headers, explicit types and let/var remain consistent throughout.
These designs are written for Fermi's contracts. No claim of worldwide syntactic
novelty, exclusivity, or production readiness is made; similar ideas may exist
elsewhere. Spelling alone is not a reason to admit a language feature.

## limit: a checked local resource ceiling

```fermi
var total: usize = 0;
limit (work = 20000, heap = 0) {
    for (var index: usize = 0; index < 100; index += 1) {
        total += index;
    }
}
```

The block would require a static upper bound on abstract work and evidence of no
heap allocation, including called helpers and cleanup. Unknown bounds would be
rejected. This is a compile-time ceiling, not a timeout, runtime quota, CPU-time
promise or memory sandbox. Existing checked overflow and bounds faults remain.
Nested ceilings must satisfy every enclosing ceiling; branching follows the
existing work model. Calls must not obtain a fresh allowance by entering another
limit block. Loop multiplication and repeated invocation need explicit accounting.
A block leaves no new value and does not intercept return, break or continue.

Before implementation, specify block costs, cleanup charging and how ceilings
compose with @budget. A candidate spelling must not imply a runtime watchdog.

## access: narrow allowed effects within a block

```fermi
@effects(fs.read, io.write)
func inspect(path: read u8[]) -> i32 {
    access (fs.read) {
        // Helpers called here must stay within this effect set.
    }
    return 0;
}
```

This block would intersect its allowed effects with all enclosing ceilings. It
could narrow but never grant permission absent from the function contract. Calls,
implicit resource cleanup, error propagation and all branches would count. This
is static closed-project checking, not an unforgeable capability or OS sandbox.

The empty block above demonstrates grammar only. Actual file acquisition and
cleanup require fs.open and fs.close in addition to fs.read; placing them under
this deliberately narrow ceiling must fail. The implementation gate is a real
helper composition test showing an effect that the outer function permits but
the inner block rejects. Callback effect polymorphism remains separate work.

## acquire: explicit Result-based resource scope

```fermi
acquire (var file: fs.File = fs.open_read(read path)) {
    // Use read/edit borrows; the still-owned file is closed at scope exit.
} else (let error: fs.Error) {
    return 74;
}
```

The initializer would evaluate once and must return Result<resource, Error>.
The successful owner belongs to the success block, with explicit let/var
mutability. The error belongs only to the else block. Both blocks are required,
no implicit error conversion occurs, and the statement produces no value.

This is structured sugar for the existing exhaustive Result match. Ownership,
explicit moves, borrow restrictions, effects, cleanup and fault unwinding must be
identical after lowering. Moving the resource transfers its cleanup obligation;
automatic fallback cleanup does not report close errors. Programs requiring an
observable close result must still call fs.close explicitly and handle its Result.
No escaping reference, deferred expression or exception mechanism is introduced.

The admission test is a side-by-side match/acquire program with identical output,
allocation counts, close behavior and fault cleanup at O0/O2 and under sanitizers.

## Selection and implementation gates

These are candidates, not three mandatory additions. Start with a complete grammar,
accepted/rejected examples, scope rules, diagnostic codes and typed-tree changes.
No sample in this document should be added to runnable examples until implemented.
The current array/struct borrow correctness work takes precedence over new syntax.
General record storage and type abstraction still have higher foundational value
than alternate spellings for existing control flow.
