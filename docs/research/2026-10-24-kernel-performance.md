# Kernel performance: measurement before optimization

Date: 2026-10-24. Scope: the first performance gate in [the active plan](../PLAN.md).
This work extends the benchmark and its correctness checks. It does not remove
runtime checks, change language semantics, or claim an optimized compiler.

## Research and decision

LLVM recommends repeated measurements and reducing scheduling/frequency noise;
repeatability alone does not eliminate bias. The runner records the environment
and repeats each workload three times, but does not change CPU affinity, frequency
policy, or system configuration. Results remain local development measurements.
[LLVM benchmarking guidance](https://llvm.org/docs/Benchmarking.html).

Clang exposes successful, missed, and analysis optimization remarks through
`-Rpass`, `-Rpass-missed`, and `-Rpass-analysis`. We collect loop-vectorization
remarks for generated code and independent C references. These explain compiler
decisions; they are not a CPU profile or a proof of the dominant runtime cost.
[Clang optimization reports](https://clang.llvm.org/docs/UsersManual.html#options-to-emit-optimization-reports).

LLVM's vectorizer must preserve loop dependencies and early-exit behavior. It can
handle some early exits, but local Clang 21.1.8 rejects the generated decoding and
sum loops. Removing safety branches merely to enable SIMD would change Fermi's
semantics. The next compiler experiment should prove ranges before eliminating
specific overflow or bounds checks.
[LLVM vectorization documentation](https://llvm.org/docs/Vectorizers.html).

## Reproduction and artifacts

```sh
make
make benchmark
```

The runner writes `build/performance/`:

- `environment.txt`: compiler version, host, flags, and measurement limitations.
- `decode-{1,2,3}.csv`, `kernels-{1,2,3}.csv`: per-trial summaries.
- `decode-raw-*.csv`, `raw-*.csv`: unsorted per-batch timing observations.
- `build.csv`: three end-to-end build durations and whole-object file sizes.
- `*.remarks.txt`: vectorization decisions, including the C references.
- `sections.txt`: section sizes when `llvm-size` is available.
- Generated `.c`, `.h`, `.o`, and contract manifests for inspection.

Build timing includes process launch and the complete Fermi frontend/backend,
header/manifest generation, hashing, and object publication. C reference timing
is compilation of its translation unit. The reference-kernels object contains
both filtering and summation, so its size/time is not a per-kernel comparison.
Builds use a warm filesystem cache after the first trial; these are not clean
compiler-build times. No release-strip or executable-size comparison is claimed.

## Runtime method and correctness

Each trial uses 101 batches of 128 calls, warmed inputs, and rotating variant
order. p95/p99 describe batch-average nanoseconds per call, not individual-call
tail latency. The checksum and harness overhead are included for both variants.
Code is compiled at `-O2`, without LTO, with the C references in separate objects.
Fermi's normal public wrapper remains in the timed path.

Inputs contain 64, 1024, or 4096 samples. Decoding retains the existing wire-format
workload and both plain and ABI-checked C references. Filtering uses a repeatable
mixed signed dataset with thresholds -32768, 0, and 32767; these exercise different
selection rates, not every production distribution. The statistics kernel is
specifically signed-16-bit summation into `i64`, not a general statistics library.
Data is reused across calls: cache misses, cold startup, I/O, and allocator costs
are outside this benchmark.

The new C references reproduce pointer/length/alignment/alias checks, domain
errors, and rejection order. Their unchecked inner arithmetic is safe because
input length is capped at 4096: the sum lies in [-134217728, 134213632], and filter
indices never exceed the input length. This is the behavior a future Fermi
optimization must prove rather than assume.

Before timing, the kernel runner compares results and complete output storage
against the references over 512 deterministic input cases and five thresholds,
including short capacity and length rejection. Separate checks cover extrema,
empty/null inputs, oversized lengths, and overlapping buffers. `make test` runs
these checks at O0/O2; `make sanitize` instruments generated code and references.

## Next compiler experiment

Start with the bounded sum, whose invariant is small enough to audit: a zero
`i64` accumulator, immutable view length capped at 4096, and one widened `i16`
addition per iteration. A range-analysis pass must preserve facts across joins,
invalidate them when dependencies change, and handle early returns and nested
loops conservatively. It must retain checks for arbitrary `i64` input, mutable
limits, and unbounded loops. A positive benchmark alone cannot validate that pass.

Decoding additionally needs relational bounds for packet length, offsets, and
output capacity. Filtering has a data-dependent output index. Neither should be
optimized by adding unchecked backend assumptions or blindly marking pointers
`restrict`. Resource ownership remains the next major language feature after
this measurement/optimization work; its implementation is not claimed here.

## Observed results

Host: Termux AArch64, Android kernel 5.10.226, Clang 21.1.8. The following ranges
are the minimum/maximum ratio of medians across three final trials, each with
4096 input samples. They are not confidence intervals.

| Kernel | Fermi / checked C |
| --- | ---: |
| Decode signed samples | 7.972–8.508× |
| Filter, threshold -32768 | 1.331–1.333× |
| Filter, threshold 0 | 1.476–1.483× |
| Filter, threshold 32767 | 1.001–1.004× |
| Sum signed samples | 4.758–4.769× |

The 1.5× gate is not met across kernels. Filtering is within that threshold for
these three trials, but this is not evidence for all datasets or other machines.
Clang reports vectorization of the C sum (width 8, interleave 2) while rejecting
the Fermi sum's potentially faulting early exit. Both filters remain scalar.
The remarks support investigating range proofs; they do not isolate the entire
performance difference.

| Fermi build | Wall time across three builds | Whole object | `.text` |
| --- | ---: | ---: | ---: |
| Telemetry | 220.474–252.678 ms | 1536 B | 504 B |
| Filter | 198.651–210.644 ms | 1432 B | 400 B |
| Sum | 180.149–194.292 ms | 1104 B | 168 B |

C decoding references together occupy 1784 B (644 B `.text`); filtering and sum
references together occupy 1712 B (696 B `.text`). Their translation-unit build
ranges were 117.479–135.532 ms and 122.890–132.333 ms respectively. These grouped
objects cannot establish a per-function code-size advantage.

`make test`, `make sanitize`, `make check-format`, and the extended `make benchmark`
completed successfully. Raw CSV row counts were checked for every trial. This
turn changes the measurement infrastructure and adds the sum example; no compiler
optimization was implemented. Linux execution remains unverified.

Follow-up: [bounded sum optimization](2026-10-24-bounded-sum-optimization.md)
implements and measures the first range-proof experiment. Results above remain
the pre-optimization baseline.
