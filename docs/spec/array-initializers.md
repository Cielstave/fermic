# Fixed-array initialization

Fixed arrays retain explicit types and complete initialization. In addition to
`[first, second, ...]`, a repeated initializer has the form `[value; count]`.
The count is a decimal integer literal in 1..65536 and must equal the expected
array length. Existing aggregate cell limits still apply. No inferred array type,
zero-length array, dynamic length, implicit default value, or allocation is added.

```fermi
var bytes: u8[256] = [0; 256];
let ready: bool[8] = [true; 8];
```

The value is checked against the element type and evaluated exactly once before
any element is initialized. Its resulting value is copied into every element.
Named aggregate values still require `copy(value)` or `move(value)`; moving a
nonresource value consumes that binding once. Array elements remain independent
values, including when a struct contains nested fixed arrays. Static strings
retain their existing immutable backing storage.

Resource-owning values, Results, and borrows remain invalid array element types,
including through aliases. Repetition cannot duplicate a buffer, file, or a
resource-containing struct. Effects from the initializer are inferred once;
`@effects` ceilings apply normally. If evaluation faults or `try` propagates an
error, the array is not published and normal cleanup runs.

Lowering evaluates the initializer into a temporary and copies it with a bounded
C loop. The AST stores the count rather than expanding repeated expression nodes.
No runtime adapter or heap allocation is required. The abstract work cost is one
unit, plus the initializer expression cost once, plus the result array's cell
count. This extends `fermi-work-1` without changing existing expression costs;
it is not a timing or stack guarantee. Counts and existing type limits bound
compiler work and generated storage; native stack remains uncertified.

Compatibility is additive: comma-separated arrays and their evaluation order
remain unchanged. Function calls do not gain semicolon argument syntax. Counts
are literals rather than constant expressions; general compile-time evaluation
is outside this feature.
