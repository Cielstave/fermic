# Feature priorities

Status: design direction, not an implemented feature list.

Fermi targets native services, background workers, and reusable components with
explicit data access and predictable resource use. Syntax should remain familiar:
parenthesized conditions, braces, explicit types, and `let`/`var`. Similarity to
another language is not sufficient reason to adopt a feature or invent new syntax.

## Existing foundations

1. Error propagation is implemented as `try(expression)` with exact error types,
   single evaluation, and resource cleanup. See
   [the implemented contract](../spec/error-propagation.md). Implicit error
   conversions remain excluded.
2. Payload enums are implemented with one typed payload per variant, exhaustive
   matching, explicit moves/copies, and active buffer/file cleanup. See
   [the implemented contract](../spec/payload-enums.md). Payload errors in Result
   and a public enum ABI remain outside this subset.
3. Resource-owning structs are implemented for complete construction, whole moves,
   field replacement, buffer/file field borrows and cleanup, including partial
   construction failure. See [the contract](../spec/resource-structs.md). Partial
   moves, escaping references and user destructors remain unavailable.
   Call-scoped [read/edit struct borrows](../spec/struct-borrows.md) are implemented.
4. File output and `fermi test` are implemented. Exclusive file creation, partial
   writes, library write_all and explicit sync enable output without implicit
   overwrite. The runner compiles local test_* functions to a temporary native
   executable. See [files](../spec/files.md) and [tests](../spec/test-runner.md).
5. Finite integer function generics are implemented with one constrained type
   parameter and explicit calls. Constraints integer, signed_integer and
   unsigned_integer admit nine, four and five concrete types respectively. Ordinary
   function bodies are instantiated and checked only for requested types; unused
   templates are not certified across their complete domain.
   See [the contract](../spec/integer-generics.md). Nonresource data generics, data views and dynamic record buffers are now
   implemented; see [data generics](../spec/data-generics.md) and
   [record buffers](../spec/record-buffers.md). Resource type parameters,
   generic structs and function values remain candidate abstraction work; plain
   callbacks precede captured closures, traits and dynamic dispatch.

Each stage requires runnable examples, diagnostics for invalid programs, runtime
regressions, sanitizer checks, and an explicit account of resource effects.
Resource generics and function values remain proposals; their grammar is not
established by this document. Further collection and callback work must state its ownership
and specialization rules before implementation.

## Effect contracts alongside the foundations

Function-level `@effects(...)` ceilings are implemented over transitive inference,
including automatic cleanup. They can prevent a helper change from adding file,
I/O or memory effects outside a declared contract. They do not establish pure
functions, capabilities, request isolation or runtime quotas. See
[the implemented contract](../spec/effect-contracts.md). Generics and callbacks
beyond integer specialization remain candidate abstraction foundations, subject to
the admission rules and dependency order below.

## Library responsibilities

Collections, UTF-8 operations, formatting, serialization, networking, HTTP,
observability, database clients, retries, and worker policies belong in libraries.
The compiler supplies the types, ownership, effects, ABI, and diagnostics those
libraries require. A service framework is not part of the language core.

## Resource contracts

Keep static bounds, runtime quotas, and measured objectives distinct. A proven
work count does not prove elapsed time, p99 latency, energy consumption, or freedom
from deadlock. Native stack bounds require backend evidence. Unknown bounds must
remain explicit; annotations alone are not proof.

Request budgets, cancellation, capability tokens, and bounded queues are promising
later work, dependent on resource ownership and a concurrency model. Do not claim
request isolation, safe concurrency, or deterministic scheduling before those
contracts are implemented and tested.

## Feature admission rules

The roadmap's foundation-maturity phase takes precedence over the candidate list.
A feature proposal must identify a concrete program that cannot be expressed
reasonably today, its dependency on existing language mechanisms, and the smallest
contract that addresses that need. Similarity to another language, novelty, and a
long feature checklist do not establish priority.

Every accepted core extension must specify:

- Grammar and type identity, with explicit types and familiar block/control syntax.
- Initialization, mutability, ownership, borrowing, and left-to-right evaluation.
- Recoverable failures, language faults, cleanup, and any allocation or I/O effects.
- Representation and lowering, including observable copying and ABI boundaries.
- Accepted and rejected source examples, native O0/O2 behavior, and relevant failure tests.
- Compatibility impact and migration when existing accepted syntax changes.

A feature must reuse established rules where they fit. It must not silently weaken
checks when placed in an alias, generic instance, aggregate, or module. Measured
performance work must preserve equivalent safety and failure behavior.

## Responsibility boundaries

| Layer | Responsibility |
| --- | --- |
| Language/compiler | Type identity, values, control flow, ownership, access rules, error propagation, effect checking, diagnostics, and native lowering. |
| Runtime | Necessary platform mechanisms, allocation/release, native I/O adapters, and future task lifecycle mechanisms under specified contracts. |
| Library | Text operations, collections, formatting, protocols, serialization, storage clients, observability, and service policy. |

Being common in applications does not make an API a language keyword. Libraries
may require runtime support, but must not obtain privileged native operations
merely by choosing a standard-looking module name. Allocation and I/O failures
remain explicit; convenience APIs must document any intentional fault behavior.

## Dependency order for further foundations

1. Close correctness and consistency gaps in current types, control flow, modules,
   ownership, and diagnostics before extending their surface area.
2. Maintain the implemented runtime text ownership and UTF-8 boundaries in
   [std.text.Text](../spec/owned-text.md). Construction, append, replacement, clear,
   and comparison reuse existing ownership and fallible storage. Static str values
   cannot become views into temporary or reallocating storage.
3. Use concrete library workloads to scope general generics and collections.
   Require at least two uses, such as bounded lists and owned text storage, before
   choosing broader constraints. Library containers must define capacity limits,
   growth failure, element moves, and cleanup of partially initialized storage.
4. Introduce plain function values before captured closures, after defining callback
   effects, ownership transfer, and representation. Escaping references and captured
   borrows require a lifetime design and an IR suitability review.
5. Admit f32/f64 when a representative program establishes their priority. Specify
   literals, conversions, rounding, comparisons, NaN/infinity, and target behavior;
   integer overflow rules cannot simply be copied onto floating arithmetic.
6. Extend native integration with an explicit trust boundary and ownership/ABI
   obligations. Calling arbitrary C must not be described as memory-safe merely
   because the caller is written in Fermi.

These are dependency gates, not an unconditional release sequence. Networking
and concurrency remain deferred during the current foundation-maturity phase.
Their eventual service acceptance must cover bounded admission, cancellation,
backpressure, shutdown, and live-resource accounting. HTTP, TLS, database APIs,
and deployment policy remain library work.

## Current feature interpretation

Transparent type aliases provide names, not distinct validated IDs. Static str
values provide static UTF-8 text. Owned `std.text.Text` supplies validated dynamic
text through fallible library operations and private buffer storage. General
resource-element collections remain unavailable. Nonresource records now have
Buffer<T> storage with explicit initialization. Effect ceilings restrict
inferred operations; they do not enforce deadlines or isolate tenants. Work budgets
are abstract static bounds; they are not latency or process-memory guarantees.
These distinctions must remain consistent in code, specifications, examples,
and feature claims.
