# Engineering decision register

Reviewed 26 October 2026. Direction is distinct from implementation status.
Changes must record rationale, affected contracts and migration implications.

| ID | Decision | Status | Consequence |
| --- | --- | --- | --- |
| D01 | Focus native backend workers and services | Active direction | Judge features against service workloads |
| D02 | Retain C11 compiler and C/Clang output | Implemented | Validate generated C and preserve evaluation order |
| D03 | Use single ownership and explicit resource transfer | Implemented subset | No implicit buffer copies; resource nesting remains restricted |
| D04 | Keep protocol and service policy in libraries | Active direction | HTTP/database features do not become keywords |
| D05 | Bound first-service queues and active work | Planned | Overload must reject or wait within an explicit bound |
| D06 | First acceptance is one process with volatile state | Planned | Durable multi-tenant Foundry is not the initial release gate |
| D07 | Retain typed-tree IR for the next bounded extension | Active direction | Add explicit borrow metadata; do not silently reuse slice semantics |
| D08 | Use edit Buffer<u8> as a distinct descriptor borrow | Implemented subset | Call-scoped access; no descriptor C ABI or general object borrowing |
| D09 | Return Result<usize, mem.Error> from initial growth APIs | Implemented | Report resulting capacity/length without introducing Result<void, E> |
| D10 | No mandatory GC in the current ownership direction | Active direction | Cleanup and resource lifetime are compiler/runtime obligations |
| D11 | Unsigned bit operations with checked usize counts | Implemented subset | Invalid shifts fault; bit truncation remains distinct from checked arithmetic |
| D12 | Initialize process arguments explicitly at executable startup | Implemented subset | Raw argument copies; library mode reports unavailable |
| D13 | Separate monotonic clock acquisition from deadline arithmetic | Implemented subset | Checked nanoseconds, explicit effect; no implicit cancellation |
| D14 | Support Result<void, E> with empty success construction and matching | Implemented | No dummy payload; mandatory handling and ABI status/error remain explicit |
| D15 | Consume Linux file owners before fallible close | Implemented subset | Opaque std.fs.File, exclusive borrow and cleanup; never retry close |
| D16 | Dispatch resource cleanup and effects by owner kind | Implemented subset | Buffers and files share transfer machinery without sharing their release operation |
| D17 | Use `try(expression)` for Result propagation with exact error identity | Implemented subset | Consumes Result, preserves effects and cleanup; no implicit error conversion |
| D18 | Give enum variants one owning payload and consume payload matches | Implemented subset | Typed exhaustive cases; active-variant cleanup; internal tag/union layout |
| D19 | Use optional `@effects(...)` ceilings over inferred effects | Implemented subset | Includes transitive calls and cleanup; independent from work proofs; no purity or sandbox claim |
| D20 | Expand finite integer constraints into concrete typed bodies | Updated by D26 | Direct typed calls remain; ordinary instance creation now follows requested type arguments |
| D21 | Admit foundation extensions through explicit semantic and resource contracts | Engineering policy | Concrete workloads, dependency gates, library/core boundaries, and compatibility evidence; no checklist-driven release order |
| D22 | Borrow structs only for the duration of a call | Implemented subset | Shared read, exclusive edit, stable places, reborrows, no reference escape or borrowed containers |
| D23 | Evaluate repeated fixed-array seeds once and copy nonresource values | Implemented subset | Explicit literal count and array type; no resource duplication or dynamic allocation |
| D24 | Protect owned UTF-8 with explicit private fields and ordinary library operations | Implemented subset | Public field defaults preserved; no escaping Text-to-str conversion or mutable byte exposure |
| D25 | Extend Buffer to finite integer elements through trusted std.buffer signatures | Implemented subset | Reuse width-aware runtime, explicit byte budgets, typed views, and ordinary push policy; defer record/resource elements |
| D26 | Instantiate ordinary integer generic bodies at concrete call sites | Implemented subset | Syntax checked at declaration; semantic/effect/work checks per requested instance; native buffer templates remain eager |
| D27 | Admit resource-free data function arguments and array views | Implemented subset | Explicit copy for aggregates, scalar copy places, canonical type reuse, private-field checks; C views stay scalar |
| D28 | Construct dynamic data elements from valid typed seeds | Implemented subset | Empty owners, typed filled growth, actual sizeof byte limits; retain integer zeroing and exclude resource elements |
| D29 | Bind prefix operators above every binary operator | Implemented correction | `-a + b` means `(-a) + b`; use parentheses to retain the old grouping |
| D30 | Use camelCase in new Fermi sources and recognize testName functions | Implemented | Preserve existing library interfaces and test_* discovery; prohibit generic tests in either form |

## Rationale and alternatives

D02 avoids a backend rewrite while safety and resource semantics are still evolving.
LLVM remains an option if debug information, target coverage or measured performance
warrants it. A new backend does not repair missing language contracts automatically.

D07 retains the current representation for call-scoped borrows only. Before adding
escaping references, resource fields with partial moves, or suspension, evaluate
whether a control-flow IR is required. Do not claim the present tree supports those
features. Any new IR needs a verifier and equivalence tests before replacing it.

D08 distinguishes a pointer to an owner descriptor from a pointer/length view of
its allocation. Using edit u8[] for growth cannot update the owning descriptor.
Consuming an owner and returning it through every error path is an alternative,
but complicates ordinary failure-preserving resize. See [the integration design](buffer-borrows.md).

D09 supplies meaningful state after success. It avoids adding a new unit-result
representation solely for two library operations. The implemented signature reports capacity for reserve and length for resize.
It remains experimental rather than a stable standard-library ABI.

D11 supplies flags and byte-processing primitives without signed-shift ambiguity.
Existing expressions retain their relative precedence. The additive fault code 4
preserves earlier ABI status/code values. Encoding belongs to std.binary, written
in Fermi; it adds no protocol keywords. See [the contract](../spec/binary.md) and
[C lowering research](../research/2026-10-25-bitwise-foundation.md). No existing
accepted operator expression requires migration.

D12 initializes argument access through ordinary C executable startup, using a
private explicit runtime context. Library mode returns unavailable; no constructor
or platform introspection silently supplies host arguments. This additive library
API preserves func main() -> i32. A future embedding ABI needs an explicit context
lifetime and isolation contract. See [arguments](../spec/process-arguments.md).

D13 uses CLOCK_MONOTONIC with checked nanosecond conversion and an explicit
clock.read effect. Deadline addition and remaining allowance are pure Fermi,
with overflow errors and saturation at expiration. Raw usize units remain a
pilot limitation; arithmetic does not enforce I/O cancellation. See
[time semantics](../spec/time.md).

D14 adds a real success-without-data shape for validation and future close/flush
APIs. Existing scalar success layouts and buffer capacity/length APIs remain
unchanged. See [unit Results](../spec/unit-results.md). D15 separates fallible
explicit close from fallback cleanup and preserves primary errors; the current
[file resource](../spec/files.md) is now available in Fermi source. D16 shares
owner temporary/transfer planning while dispatching payload drop and effects by
resource kind. Resource fields and general native owners remain restricted.

## Open decisions and required evidence

- General generics/resource fields: at least two working library use cases and a
  specified instantiation, cleanup, and diagnostic model.
- Task scheduler: bounded blocking prototype, cancellation and shutdown results,
  then comparison against an event-driven prototype on equivalent I/O workloads.
- String representation: UTF-8 validation and byte/text boundary requirements;
  avoid assuming byte indexes are character indexes.
- Installation/package model: relocatable compiler and versioned stdlib first;
  registry and dependency resolution need a separate reproducibility design.
- Stable native ABI: resource ownership and versioning contract before exposing
  descriptors or task handles. Existing internal C layouts are not the answer.

D17 shortens repeated error-forwarding matches while keeping the enclosing Result
contract visible. Parentheses match other explicit control forms. Callers use
match when they need recovery or error conversion. A possible error exit checks
that other Result bindings have already been handled. See
[Result propagation](../spec/error-propagation.md) for the accepted grammar.

D19 keeps effect permissions distinct from work proofs. An effect-free but
unbounded helper can be useful without a work certificate. Ceilings restrict
actual inferred effects, while unused permissions do not spread through calls.
The contract is closed-project checking; capability tokens and effect polymorphism
require separate ownership and interface designs.

D20 began with scalar ordering and typed-view search over a finite, nonresource
integer domain. Its initial eager expansion checked all bodies at a cost of nine,
four or five function slots per declaration. D26 supersedes that expansion and
full-domain checking contract; signed/unsigned argument restrictions remain.

D21 keeps language flexibility subordinate to the native-service focus and the
current foundation-maturity phase. Dynamic text and collections require ownership
and failure contracts; callbacks require effect/lifetime contracts; floats require
numeric semantics; C integration requires a stated trust boundary. The candidate
list is not an unconditional implementation sequence. See the
[admission rules](feature-priorities.md#feature-admission-rules).

D22 extends existing call-scoped access to struct storage, including fixed-array
elements and resource fields. It reuses pointer lowering and root-based alias
checks without introducing escaping references. Const-qualified C targets preserve
read access through nested addresses. Borrow aliases cannot become array elements.
See [struct borrows](../spec/struct-borrows.md); field-disjoint proofs and general
lifetimes remain separate work.

D26 reduces unnecessary concrete functions in library-heavy programs without
raising the existing function limit. Templates retain immutable tokens and their
defining source/module. Repeated requests reuse a concrete body, and recursive
requests resolve to the existing instance. The compatibility tradeoff is explicit:
unrequested ordinary bodies are syntax-checked but have no typing, ownership,
effect or work certificate. The 128-template limit is independent of the
128-concrete-function limit. Trusted native buffer declarations still receive
eager finite validation. General type parameters and record collections are
separate work; see [the contract](../spec/integer-generics.md).

D27 opens reusable record algorithms without resource copying or escaping borrows.
D28 extends that data contract to dynamic storage without treating zero bytes as
constructed records. Private values and active payload variants are supplied by
existing typed values. Raw reserve preserves initialized elements; publication of
a new length follows typed initialization. Resource-element cleanup and callback
effects are not implied by this work. See [data](../spec/data-generics.md) and
[record buffers](../spec/record-buffers.md).

D29 corrects the former prefix operand threshold of 7, which unintentionally
consumed shifts, addition and multiplication. The threshold is now 10, above
all binary binding powers. Native regression tests distinguish `-a + b` from
`-(a + b)` and `~a << 1` from `~(a << 1)`. The Fermi bootstrap parser uses the
same corrected rule. This is a behavior correction; existing code that relied
on the former grouping must add parentheses.

D30 follows the requested concise camelCase convention. A shared test-name
predicate keeps parser generic restrictions and native discovery consistent.
Imported tests remain excluded, and bare test/testhelper names remain helpers.
