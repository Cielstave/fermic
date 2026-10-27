# Loop control

`break;` and `continue;` operate on the nearest enclosing `for` or `while` loop.
They are statements, require semicolons, and accept no labels or values. A match
is not a loop: a jump inside a case targets the surrounding loop. Both keywords
outside a loop are rejected with `E_FLOW`.

```fermi
func count_before_zero(values: read i32[]) -> usize {
    var count: usize = 0;
    for (var i: usize = 0; i < values.len; i += 1) {
        if (values[i] < 0) { continue; }
        if (values[i] == 0) { break; }
        count += 1;
    }
    return count;
}
```

## Execution and scopes

Break exits the loop without executing its step. Continue skips the rest of the
current body. In a for loop it then executes the step exactly once and checks
the condition again; in a while loop it checks the condition again directly.
Nested jumps do not exit or continue outer loops. Statements after an unconditional
jump in the same block are unreachable and rejected with `E_FLOW`.

Each jump releases resource bindings in the scopes it leaves, in reverse order
of declaration. This includes owning Result payloads introduced by match cases.
Outer resources remain alive. Resources declared in a for initializer remain
alive across continue and are released when the loop exits, including by break.
No rollback of earlier writes occurs. Fallback file drops retain their existing
consuming-close and primary-error preservation rules.

A for header still requires an initializer, boolean condition, and step. The
initializer may declare a binding. The step must be an assignment or a void
expression; it cannot declare a binding. Bodies require braces.

## Static flow checks

Conditional jumps do not constrain the initialization state of code reachable
only through other branches. Continue states join with ordinary body fallthrough
before the for step is checked. Bindings ready at loop entry must be restored
before every reachable back edge, including those created by continue. Moving
an outer owner and breaking is permitted, but its post-loop state must account
for both the break path and the condition-false path.

Loop bodies are conservatively allowed to execute zero times. A variable first
initialized in the body is therefore not definitely initialized after the loop.
A loop alone does not establish that a non-void function returns on every path,
even with a literal true condition.

Results cannot be abandoned by jumps. Results in exiting scopes must be handled
or forwarded before jumping. A newly assigned outer Result must also be handled
before a back edge so that the next iteration cannot overwrite an unhandled
result. An outer Result that already existed at loop entry may remain untouched
through the loop and be handled afterward.

## Lowering, work bounds and compatibility

The checker records the jump target and the exact resource bindings to drop.
The C backend emits cleanup followed by a jump to a loop-specific step or exit
label. Ordinary scope exits, returns, and faults retain their existing cleanup.
The work analyzer counts jumps conservatively and forgets written outer bounds
when jumps can bypass later assignments; see [work model](work-model.md).

`break` and `continue` are now reserved keywords. Experimental programs that used
them as identifiers must rename those identifiers. Declarations formerly accepted
in for steps must move to the initializer or an explicit surrounding scope.
The new Result back-edge check can also reject previously accepted loops that
silently retained unhandled Results.

Tests cover native for/while/nested/match behavior at `-O0` and `-O2`, invalid
jumps, unreachable code, initialization joins, moves, pending Results, and budget
bounds bypassed by jumps. File tests check mixed owner cleanup and close order,
resource preservation across continue, initializer cleanup, acquisition failures,
close failures, and faults after earlier loop activity.
