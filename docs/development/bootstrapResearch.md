# Bootstrap assessment and research

Reviewed 27 October 2026 against the repository and the primary sources linked
below. This is an implementation roadmap, not a claim that bootstrap succeeded.
Naming follows [source conventions](naming.md).

## Present position

The production compiler is C11. Fermi-written source loading, lexing and a
bounded AST parser exist in `bootstrap/`. The parser supports plain functions,
bare type references, blocks, bindings, assignments, returns, if/else, while/for,
break/continue and core expressions.
It cannot parse its own implementation. Name resolution, type/ownership analysis,
IR lowering and code emission have not been ported.

Recorded initial O0/O2 tests compared 1,446 expression trees with the host parser and
70,760 inputs with the host lexer; focused sanitizer and allocation-failure tests
passed. These are frontend correctness evidence, not a self-hosting certificate.
The current smoke checks still report 35 AST nodes for the arithmetic example
and 564 tokens for bootstrap/main.fe. Control-flow structure now extends the
parser reference harness to 1,530 cases; its native example has 49 AST nodes.
See [validation](validation.md) for completed test runs.

## Findings from other compilers

- Rust documents a seed compiler, compiler/library build stages and an optional
  same-result check. This supports explicit staged acceptance rather than merely
  rewriting source files. [Rust bootstrap](https://rustc-dev-guide.rust-lang.org/building/bootstrapping/what-bootstrapping-does.html)
- GCC's normal native bootstrap includes three builds and compares stages 2/3.
  Divergence is investigated as a potential compiler defect; comparison itself
  has platform limitations. [GCC build](https://gcc.gnu.org/install/build.html)
- Go 1.5 translated its compiler from C and required an existing Go toolchain.
  Its release notes also report an initial build-performance regression: moving
  implementation language does not establish performance parity.
  [Go 1.5](https://go.dev/doc/go1.5)
- LLVM's tutorial separates parsing/AST from IR generation and later target/debug
  work. It illustrates a pipeline, not the complete semantics of a safe language.
  [LLVM frontend tutorial](https://llvm.org/docs/tutorial/)
- Rust checks moves, initialization and borrows using MIR and dataflow. The lesson
  for Fermi is explicit semantic passes over lowered control flow; Rust's full
  type system is not a prerequisite. [MIR borrow checking](https://rustc-dev-guide.rust-lang.org/borrow-check.html)
- chibicc's build tests both its initial and self-built compiler. Running workloads
  with the new compiler is distinct from merely producing that compiler binary.
  [chibicc Makefile](https://github.com/rui314/chibicc/blob/main/Makefile)

## Recommended Fermi gates

These gates are engineering recommendations derived from the repository state.
They do not replace the broader [foundation program](foundation-program.md).

| Gate | Deliverable | Acceptance |
| --- | --- | --- |
| B0 | Freeze a bootstrap source/dependency manifest and semantics | List every syntax/runtime feature used by the compiler and its library dependencies; retain the C seed |
| B1 | Complete the grammar used by those sources | Parse all bootstrap sources; structural and diagnostic reference tests; no silent feature skipping |
| B2 | Resolve symbols, modules and types | Scope/type IDs, canonical module identity, privacy, signatures and generic-instance checks; valid and invalid reference cases |
| B3 | Lower and check safe execution | Preserve move/borrow rules, definite initialization, effects, cleanup and checked faults on all exits |
| B4 | Emit C from Fermi | Build representative programs and bootstrap modules at O0/O2; compare behavior, diagnostics and resource cleanup |
| B5 | Compile the compiler source | The C seed builds Fermi compiler F1, which compiles the same source/dependencies to produce F2 |
| B6 | Repeat and validate | F2 produces F3; compare deterministic generated C and run regression/application suites with F2/F3 |

B1 still needs imports/visibility, structs/enums, resource fields, Result,
match, array/record constructors, field/index/slice operations,
read/edit/copy/move, try and the constrained generic/native/library forms actually
used. Recognizing their tokens is already possible; parsing and enforcing them is
separate work. B2/B3/B4 are the largest unimplemented portions of the port.

The bootstrap source set must be compilable by both the seed and successor.
Freeze that dependency closure before adding language features to its source.
During the first C-emitting milestones, a shell/C driver can invoke Clang; a
Fermi subprocess API is not a mandatory frontend prerequisite. A standalone driver
that launches tools itself will need a specified process API later.

## Systems and low-level obligations

Retain the C backend for the first self-hosted compiler. A C/Clang toolchain and
small C runtime can remain dependencies; this does not claim a toolchain entirely
written in Fermi. Document the native/runtime trust boundary explicitly.

The successor emitter must preserve integer widths, checked overflow/division,
shift semantics, alignment/padding, enum tags, bounds checks, initialization,
left-to-right evaluation and cleanup during both ordinary returns and faults.
Native signatures, protected standard modules and ABI lowering need explicit
validation. Removing safety checks to get a bootstrap pass is not acceptance.

LLVM IR also has undefined behavior, poison and target-specific layout. Direct
LLVM output would not supply these language checks automatically. Keep a typed
internal representation independent of emitted C, and consider another backend
when measured needs justify the additional work.
[LLVM data layout](https://llvm.org/docs/LangRef.html#data-layout),
[LLVM undefined behavior](https://llvm.org/docs/LangRef.html#undefined-values).

Start acceptance on the currently exercised Termux AArch64 platform. The pilot
uses 64-bit usize; other CPU/OS combinations need independent layout, ABI and
runtime tests. No cross-platform bootstrap is established by the current suite.
Current limits (128 concrete functions, 32,768 nodes, 1 MiB per source, 64 MiB
compiler arena) must be measured against the actual compiler workload. Increase
limits only with overflow, exhaustion and diagnostic regressions.

## Required data services and deferred features

The existing data buffers support flat token/node/type/symbol tables. Use typed
IDs and source offsets, explicit owner roots and bounded byte pools. A symbol
lookup service can begin with deterministic linear lookup; a reusable hash map
can follow measured scaling needs. Add diagnostic records, source/literal tables,
checked byte output and stable module/type identity as compiler services.

Floats, closures, async, networking, generic resource-element containers and a
package registry are not strict prerequisites for this compiler design. Existing
resource structs, Result, constrained data generics and borrow rules still need
to be implemented correctly by the successor because the compiler uses them.
Do not confuse deferred advanced features with permission to omit current safety.
Standard libraries already exist in Fermi with native adapters; they evolve with
the compiler rather than waiting for self-hosting.

## Reproducibility and completion

For Fermi, use explicit names C0 (current C seed), F1 (built by C0), F2 (built by
F1), and F3 (built by F2). These labels describe this proposed chain, not Rust's
exact artifact numbering. Pin source/dependencies, runtime, target and toolchain.
Generated symbol ordering and source/module traversal must be deterministic.
Compare the C emitted by F1 and F2 for the same compiler sources. Compare native
bytes only after controlling paths, debug metadata and other build variability.

Matching generations is a consistency check, not proof against shared compiler
bugs or every unsafe behavior. Keep independent expected outputs, rejected-source
cases, allocation-failure tests, sanitizers and external application workloads.
No percentage or calendar estimate is justified yet: the complete bootstrap
source manifest and B2/B3/B4 port have not been established. The next measurable
milestone is parsing the compiler's own source/dependency set, not another token
count demonstration.
