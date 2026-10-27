# Bounded reduction proofs

The compiler has a deliberately narrow overflow-elimination pass in
`compiler/analysis/ranges.c`. It runs after semantic and work-budget analysis,
but does not use the work analyzer's bounds as optimization evidence. A budget
annotation is neither required nor sufficient for this optimization.

## Accepted pattern

```fermi
func sum(input: read i16[]) -> i64 {
    if (input.len > 4096) { return 0; }
    var total: i64 = 0;
    for (var i: usize = 0; i < input.len; i += 1) {
        total += widen<i64>(input[i]);
    }
    return total;
}
```

The pass requires all of the following:

- A zero-initialized `i64` declaration immediately followed by the loop, directly
  in the function body. The accumulator is identified by resolved symbol identity.
- A `usize` counter initialized to zero, tested with `< input.len`, and incremented
  by exactly one. The body contains only the single accumulator update shown.
- The indexed input is the same symbol whose length controls the loop. Its element
  type is `i16`; the index is the loop counter and the widening target is `i64`.
- The input is a fixed array, or a read-only parameter view with a preceding
  top-level `if (input.len > N) { return ...; }` guard. No `else`, compound guard,
  nested guard, or non-returning rejection branch is used as evidence.
- The bound is at most `floor(INT64_MAX / 32768)` (281474976710655).

A parameter view's length is immutable. Calls can neither replace that descriptor
nor change an array's type-level extent. Facts about those lengths therefore
survive intervening statements; facts from nested control flow are never imported.
The accumulator declaration must remain adjacent to the loop so no intervening
statement can modify its initial value.

## Proof and lowering

After k iterations, every possible prefix sum lies in
`[-32768*k, 32767*k]`. The upper bound on iteration count makes both endpoints
representable as `i64`; the check uses division rather than potentially overflowing
proof arithmetic. A single loop update cannot change the counter or the input
length. No calls, nested loops, or additional writes occur in the accepted body.

The typed statement carries `FE_PROOF_I16_SUM` and its maximum iteration count.
C lowering replaces only that accumulator's checked addition with ordinary `i64`
addition. Bounds checks, ABI validation, and counter checks remain in generated C;
Clang may subsequently eliminate checks it can independently prove redundant.
There are no inserted `assume` directives or changes to aliasing contracts.

`fermi explain` reports the number of overflow checks proven per function. The
number counts static update sites, not the number of additions executed. Generated
C includes a proof comment for inspection. The pass applies at O0 and O2; Clang
vectorization remains target- and optimization-dependent. Work-budget accounting
continues to describe the original abstract operations, not optimized instructions.

## Limits and regression coverage

This is not general range analysis. Nonzero accumulators, `i64` samples, unknown
lengths, mutable limits, multiple updates, alternative indexing, calls, nested
loops, and branch-dependent guards keep their runtime overflow checks.

Unit tests cover the exact proof threshold and rejected nearby patterns. Native
O0/O2 tests compare sums across extrema, randomized inputs, and SIMD tail lengths;
negative cases must still report signed overflow or bounds faults. The normal
suite, ASan/UBSan, frontend fuzzing, and the performance suite exercise the pass.
A successful benchmark is not a substitute for the prefix-sum proof above.
