# Bounded sum optimization

This implements the first experiment proposed in the
[kernel performance report](2026-10-24-kernel-performance.md). The exact acceptance
rule and prefix-sum argument are documented in [range proofs](../spec/range-proofs.md).
It is a specialized compiler optimization, not general interval analysis.

## Change and evidence

A separate C analysis module recognizes a zero-based `i64` reduction over a bounded
`i16` array or immutable parameter view. It annotates only the accumulator update.
C lowering omits that update's overflow branch; ABI, index, and counter checks
remain. `fermi explain` reports one proven overflow-check site for `sum_samples`.
Its abstract work bound remains 65566, with the requested budget still 150000.

Clang 21.1.8 now reports vectorization width 8 and interleave count 2 for the sum.
The previous generated loop was rejected as a potentially faulting early-exit
loop. No unsafe assumptions or pointer alias promises were introduced.

## Local comparison

Before editing the compiler, the existing sum C/object and complete benchmark
executable were copied into `build/range-baseline/`. After validation, the extended
benchmark ran three times. Because those runs showed large timing variation, the
preserved baseline and new executable were additionally run in alternating order
for three paired trials. Both use the same workload, C reference, Clang version,
and batch method from the earlier report.

At 4096 samples, the paired runs produced:

| Artifact | Median time across trials | Ratio to checked C within each trial |
| --- | ---: | ---: |
| Preserved baseline | 3427.9 ns | 4.753–4.769× |
| Proven reduction | 720.5–727.8 ns | 1.000–1.010× |

This is roughly a 4.7× improvement for this kernel in the paired local experiment,
not a whole-language speedup. Frequencies, CPU placement, and system load were not
controlled. p95/p99 remain batch-average timings and are not individual-call tail
latency guarantees. The broad benchmark exhibited substantial variation; no
performance improvement is attributed to decoding or filtering, whose lowering
was not changed by this pass. The overall 1.5× gate remains unmet.

Vectorization increases the sum object from 1104 to 1368 bytes and its `.text`
from 168 to 432 bytes. This is a code-size tradeoff. Build timings in the broad run
were strongly affected by environment variation and do not establish compile-time
regression or improvement.

## Validation and artifacts

- `make test`: proof acceptance/rejection, native O0/O2 regression cases, and the
  existing differential suites passed.
- `make sanitize`: ASan/UBSan runs, including new unit/native cases, completed with
  all PASS markers and no reported sanitizer errors.
- `make fuzz`: 10000 frontend/lowering inputs completed; the new sum and negative
  range cases were added to the seed corpus.
- `make check-format` and Clang static analysis of the range pass passed.
- `make benchmark`: three complete measurement trials completed.

Native tests exercise all-negative/all-positive extrema, randomized data, SIMD
tail lengths, empty inputs, and rejected ABI arguments. Nonzero seeds, `i64`
samples, multiple updates, and alternate input indexing must still fault when
required. Unit tests additionally reject mutable counters/limits, nested loops,
conditional guards, and bounds immediately above the safe proof threshold.

Local raw artifacts are in `build/performance/` and `build/range-baseline/`, with
before/after generated-C and object hashes in `artifacts.sha256`. These build
artifacts are removed by `make clean`; the checked-in specifications and tests
remain the reproducible description of the transformation. Linux execution has
not been performed in this environment.
