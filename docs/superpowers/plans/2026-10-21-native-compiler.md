# Fermi Native Compiler Implementation Plan
> SUPERSEDED on 2026-10-23. Do not execute this plan against the v0.2 design. See docs/PLAN.md and docs/design/fermi-v0.2.md.


> **For agentic workers:** Use superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox syntax for tracking.

**Goal:** Deliver a modular C11 compiler that checks and builds the documented Fermi subset into native executables.

**Architecture:** Separate support, frontend, semantic analysis, work analysis, C emission, and POSIX driver. Keep runtime safety primitives and standard I/O outside the compiler implementation.

**Tech Stack:** C11, Make, POSIX shell, Clang/GCC, clang-format; no Python or package dependencies.

**Spec:** `docs/spec/language.md`, `docs/spec/work-model.md`; strategic scope in `docs/PLAN.md`.

## Global Constraints

- Explicit types; immutable `let`, mutable `var`; familiar brace syntax.
- Initial values: i64, bool, fixed scalar arrays of 1..4096 elements, text literals for std.io.
- Source limit: 1 MiB; syntax nesting: 128; compiler arena: 64 MiB.
- Safety checks stay enabled at every optimization level.
- Work model version 1 uses checked uint64 arithmetic and rejects unprovable bounds.
- No interpreter, arbitrary FFI, user pointers, or dynamic allocation in compiled programs.
- Preserve the existing root AGENTS.md as requested earlier.

## Review Focus

- Left-to-right calls and short-circuit expressions must preserve observable I/O order.
- Source-derived identifiers, strings, and filenames must never become executable shell text.
- Malformed input and deep expressions must fail diagnostically, not crash the compiler.
- Array mutation and index-check elimination must respect immutability and current ranges.
- Work overflow, recursive calls, and zero-iteration loops must not produce false proofs.

## Task 1: Support and lexical frontend

Files: `compiler/include/fermi/support.h`, `compiler/support/context.c`,
`compiler/include/fermi/lex.h`, `compiler/frontend/lexer.c`, `tests/unit/frontend.c`, `Makefile`.
Interfaces: context init/destroy; `bool fe_lex(FeContext *, FeTokens *)`.

- [ ] Write unit cases for token locations, operators, escapes, malformed input, and integer overflow; run RED.
- [ ] Implement bounded allocation, diagnostics, and tokenization.
- [ ] Run `make test-unit`; expect success without warnings.

## Task 2: Typed syntax and semantic checking

Files: `compiler/include/fermi/ast.h`, `compiler/frontend/parser.c`,
`compiler/include/fermi/sema.h`, `compiler/analysis/{types,check,work}.c`, semantic unit cases.
Interfaces: `FeProgram *fe_parse(FeContext *, FeTokens *)`; `bool fe_check(FeContext *, FeProgram *)`.

- [ ] Add valid dot-product and invalid-type/mutability/effect/budget/return cases; run RED.
- [ ] Implement recursive-descent parsing, binding resolution, typed AST, range and effect checking.
- [ ] Implement versioned work analysis and checked arithmetic, including call cycles and array copies.
- [ ] Run `make test-unit`; all accepted and rejected cases must match the specification.

## Task 3: Native backend and driver

Files: `compiler/backend/c/emitter.c`, `compiler/driver/{main,process}.c`,
`runtime/{include/fermi/runtime.h,src/runtime.c}`, `library/std/{io.h,io.c}`,
`tests/integration/run.sh`, executable fixtures and examples.
Interfaces: `bool fe_emit_c(FeContext *, FeProgram *, FILE *)`; POSIX spawn with argv.

- [ ] Add executable tests for output, array value semantics, evaluation order, overflow and index traps; run RED.
- [ ] Lower typed expressions to ordered C temporaries, explicit control flow and checked runtime calls.
- [ ] Add check/explain/emit-c/build CLI, with safe temporary files and non-overwriting output publication.
- [ ] Run `make test`; verify native execution at -O0 and -O2.

## Task 4: Hardening, documentation, and review

Files: `tests/fuzz/frontend.c`, `.github/workflows/ci.yml`, formatting configuration,
`docs/architecture/compiler.md`, `docs/development/contributing.md`, README and progress ledger.

- [ ] Add malformed-input/depth tests and run the sanitizer/fuzz targets where supported.
- [ ] Format and compile with strict warnings; add reproducible CI definitions.
- [ ] Publish grammar, cost equations, support limits, and exact build commands.
- [ ] Obtain a separate final code review and fix material findings with regression tests.
- [ ] Run final verification; report tested behavior and remaining maturity limits.

Git commits/worktrees are inapplicable until this workspace has a Git repository.
