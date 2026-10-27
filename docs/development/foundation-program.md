# General programming foundations

Authorization: implement the complete foundation sequence requested by the user.
This ledger separates completed work from subsequent implementation; a design or
unchecked example is not an implementation claim. Existing AGENTS.md is retained.

| Stage | Deliverable | State |
| --- | --- | --- |
| 1 | On-request generics for nonresource data and call-scoped record views | Implemented; validation recorded below |
| 2 | Dynamic nonresource record containers with explicit initialization and byte limits | Implemented; validation recorded below |
| 3 | Resource-element construction, removal, transfer and partial-failure cleanup | Pending |
| 4 | Plain function values with checked signatures, effects and ownership | Pending |
| 5 | Error payloads with structured context and explicit propagation | Pending |
| 6 | f32/f64 literals, arithmetic, comparisons, conversions and target semantics | Pending |
| 7 | Directory/metadata operations and explicit safe file-publication contracts | Pending |
| 8 | C calls with declared ABI and an explicit unsafe/trust boundary | Pending |
| Libraries | Map/set, sorting, text parsing, JSON and serialization | Follow their prerequisites |
| Systems | Networking, concurrency and asynchronous execution | Require separate lifecycle/effect contracts |

Every stage requires a specified contract, runnable workloads, invalid-source
regressions, O0/O2 behavior and relevant sanitizer coverage. Admission must preserve
left-to-right evaluation, explicit types and mutability, privacy, checked faults,
Result handling, ownership and effect accounting. Static work bounds do not prove
elapsed time, stack consumption or process isolation.

## Stage 1 contract

`T: data` admits integers, bool, static str, nonresource structs/enums and fixed
arrays of those values. Results, views, resource owners and owner borrows are
excluded. Explicit copy remains necessary for aggregate places. Scalar copy is
permitted so one generic body can explicitly copy either scalar or aggregate data.
Move remains an operation on whole aggregate bindings.

Views of data are call-scoped and preserve read/edit access through projections.
A generic function cannot bypass private fields, manufacture zero-valued records,
or make a resource copyable. Generic structs, trait constraints and multiple type
parameters remain separate work. Record views are internal-language interfaces;
exported C views retain integer/bool elements until an aggregate ABI is specified.

Workloads: copying/filling record arrays in an inventory-style pipeline and
swapping/reordering parser-token records. Library algorithms must not allocate or
relax alias rules. Rejected bounds/lengths must be checked before mutation.

## Container gate

Do not equate zeroed allocator bytes with a constructed record. Private fields,
valid enum tags, resource owners and nontrivial cleanup require explicit element
construction. Reallocation must preserve initialized-prefix length and propagate
failure without changing the original owner. Byte limits must include native
padding/alignment in the payload representation; they are not process RSS limits.

## Current implementation evidence

Stage 1 is specified in [data generics](../spec/data-generics.md) and stage 2 in
[record buffers](../spec/record-buffers.md). Full make test passed with the new
suites, existing integer buffer regressions, capability lab, library system and
inventory. Focused sanitizer results are recorded in validation.md. The remaining
stages above are still pending; this ledger does not mark the entire program
complete. The 2,048-record example removes the former fixed-array language limit
for nonresource records, while resource-element lifecycle work remains separate.

## Bootstrap workload

The [bootstrap frontend](../../bootstrap/README.md) now exercises source I/O,
private scan state and dynamic token records in Fermi. It follows the host lexer
and has differential/ownership tests. This is an additional executable workload,
not completion of stages 3–8 or a self-hosting compiler. A [flat AST parser](../../bootstrap/PARSER.md)
now supports functions, bindings, returns, assignments, if/else, while/for,
break/continue and core expressions. Match/module/declaration grammar and
semantic analysis remain subsequent gates.
