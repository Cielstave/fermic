# Fermi engineering documents

Status review: 25 October 2026. Read requirements, implemented contracts, then
feature proposals. A documented design does not make a feature available.

## Document map

| Document | Authority and purpose |
| --- | --- |
| [Value proposition](product/value.md) | Adoption hypothesis, existing evidence and next workloads |
| [Product requirements](product/requirements.md) | Target users, scope and measurable acceptance |
| [Implementation roadmap](PLAN.md) | Delivery order, dependencies and milestone gates |
| [Language specification index](spec/README.md) | Entry point to implemented language contracts |
| [Compiler architecture](architecture/compiler.md) | Current stages, representations and trust boundaries |
| [Runtime architecture](architecture/runtime.md) | Implemented runtime and planned service contracts |
| [Dynamic integer buffers](spec/typed-buffers.md) | Typed runtime storage, bounded allocation, growth and append |
| [Owned UTF-8 text](spec/owned-text.md) | Validated runtime text, fallible growth, and private struct fields |
| [Fixed-array initialization](spec/array-initializers.md) | Explicit repeated values, evaluation once, and resource restrictions |
| [Decimal formatting](spec/formatting.md) | Allocation-free usize/i32/i64 output and preserved destinations |
| [Decimal input](spec/configuration.md) | Strict unsigned/signed parsing, overflow checks and configuration |
| [Payload enums](spec/payload-enums.md) | Implemented constructors, typed cases and resource cleanup |
| [Result propagation](spec/error-propagation.md) | Implemented try grammar, exact error types and cleanup |
| [Feature priorities](design/feature-priorities.md) | Candidate stages after the current foundations |
| [Decision register](design/decisions.md) | Decisions, consequences and unresolved choices |
| [Buffer borrowing design](design/buffer-borrows.md) | Implemented descriptor access, lowering and acceptance tests |
| [Programmer needs](research/2026-10-26-programmer-needs.md) | Primary evidence, design inferences and implementation priorities |
| [File resource integration](design/file-resources.md) | Tested runtime, opaque owner, borrowing and generated cleanup |
| [Research review](research/2026-10-25-backend-design.md) | Primary sources and Fermi-specific conclusions |
| [Coding style](development/coding-style.md) | Naming, source language, diagnostics and review conventions |
| [Validation history](development/validation.md) | Dated evidence, not a rolling claim that all tests passed |
| [Foundation audit](development/foundation-audit.md) | Borrow-evaluation finding, correction and remaining scope |
| [Progress log](development/progress.md) | Implementation changes and recorded checks |
| [Capability lab](../examples/capability_lab/README.md) | Runnable programs at different scales, differential checks, and concrete language gaps |

## Status and change control

**Implemented** means supported by source and tests; limitations still apply.
**Planned** means selected work with a written gate, not accepted source syntax.
**Experimental** means a candidate requiring evidence before commitment.
**Historical** means context, not current authority.

The product requirements govern scope. The specification index identifies current
semantics. The roadmap governs order. A proposed change becomes implemented only
when its code, tests, diagnostics and specification are updated together. A mismatch
between code and specification is a defect to resolve, not permission to silently
change either. Record the affected decision and migration when behavior changes.

The v0.1 archive, v0.2 proposal and earlier implementation plans remain historical
where superseded. [Foundry](../examples/design/foundry/README.md) is an experimental
application workload. Its generic types, callbacks and runtime APIs do not override
the pilot. Do not copy its dependency layout into compiler documentation as fact.

No new document changes AGENTS.md or retroactively changes prior test evidence.

- [Executable library circulation system](../examples/library_system/README.md): a modular application exceeding 1,000 source lines, with native and differential tests.
