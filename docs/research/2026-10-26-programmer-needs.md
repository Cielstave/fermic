# Programmer needs and implications for Fermi

This review connects published evidence to implementation decisions. It is not a
market-wide survey or evidence that a new language will automatically solve the
reported problems. Sources were checked on 26 October 2026.

## Findings from primary sources

| Problem | Evidence and limits | Fermi response |
| --- | --- | --- |
| Toolchain cost and complexity | The [2025 State of Rust survey](https://blog.rust-lang.org/2026/03/02/2025-State-Of-Rust-Survey-results/), published March 2026, reports recurring compilation/storage concerns and worries about growing complexity. Respondents represent that community, not all programmers. | Measure compile latency and artifacts on actual Fermi projects; require a concrete workload before adding syntax. Do not promise faster builds without measurements. |
| Memory defects survive testing | The [Android team's safety report](https://security.googleblog.com/2024/09/eliminating-memory-safety-vulnerabilities-Android.html) reports reduced memory-safety vulnerabilities during adoption of safer languages. This is contextual project evidence, not proof of Fermi safety. | Strengthen static ownership and alias rejection, retain runtime bounds/overflow checks and review native trust boundaries. Fuzzing alone is insufficient. |
| Repetitive error plumbing obscures work | Go's [Errors are values](https://go.dev/blog/errors-are-values) discusses composition that preserves error checking while reducing repetition, including cases where hiding partial progress is unsuitable. | Evaluate error-propagation ergonomics using existing multi-file applications. Preserve required Result handling and observable partial I/O progress. A new keyword is not yet justified. |
| Resource errors have state consequences | The [Linux close reference](https://man7.org/linux/man-pages/man2/close.2.html) warns that retrying failed close can affect a reused descriptor. Linux EINTR behavior differs from the POSIX.1-2024 rule. | A Linux file close consumes its owner even on error. Drop never retries or overwrites an earlier error. Other targets require their own reviewed adapter. |
| A frontend tutorial is not a production compiler design | The [LLVM language tutorial](https://llvm.org/docs/tutorial/MyFirstLanguageFrontend/index.html) teaches lexing, AST and lowering, while explicitly disclaiming production engineering practices in its examples. | Keep phase boundaries, explicit invariants, modular runtime and negative tests. Do not copy tutorial globals or replace the backend merely to appear mature. |

The design responses above are Fermi-specific inferences. No source establishes
that developers want Fermi, that its current subset is generally memory-safe, or
that its runtime is faster than established languages.

## Build the language from contracts and workloads

For each extension, write the source contract and state transitions first. Define
accepted/rejected programs, type identity, evaluation order, mutation, ownership,
failure output, effects and ABI exposure. Then map these to the existing pipeline:

1. Lexing and parsing preserve source spans and reject unsupported forms.
2. Type/flow checking resolves names and enforces initialization and Result use.
3. Ownership/access checking forbids copy, use-after-move and conflicting borrows.
4. Effects and cost analysis distinguish pure bounds from external operations.
5. Lowering preserves evaluation order and transfers cleanup obligations.
6. Native compilation and integration tests verify executable behavior and failure
   paths. A successful parser test is not evidence that resource cleanup works.

Retain the C/Clang backend while these contracts evolve. A CFG/SSA ownership IR
becomes a design requirement before general escaping references, partial moves,
suspension or more complex joins; the present typed tree must not silently claim
those capabilities. Specify and test an IR verifier before replacing working paths.

## Priority decisions

1. **File lifecycle foundation:** native ownership, bounded reads, consuming close
   and fallback drop are now integrated into std.fs.File, with source rejection
   and generated-program fault tests. General resource nesting remains restricted.
2. **Unit success Results:** Result<void, E> representation, empty success handling
   and ABI support are implemented. std.fs.close now uses this foundation; flush
   and write-resource semantics remain future work.
3. **Opaque native owners:** decide type identity, exclusive borrow, move and drop
   dispatch using files and a future socket as two resource use cases. Raw numeric
   descriptors must not become the source-language ownership model.
4. **Error ergonomics:** measure nested-match repetition in bytecount and file
   workloads. Any propagation mechanism must transfer resources and preserve the
   primary failure on its early-exit path.
5. **Developer feedback:** collect task completion time, diagnostic corrections,
   implementation workarounds, build latency and artifact size. Current surveys
   motivate questions; feedback from actual Fermi users must validate the answers.

HTTP, persistence and concurrency remain downstream of these gates. More syntax
without lifetime and failure contracts would expand the surface before the
foundations can support it.
