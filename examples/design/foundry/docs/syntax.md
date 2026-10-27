# Proposed syntax

This directory uses one speculative dialect, not an approved specification.

## Core conventions

Use func, struct, enum, explicit types and semicolons. let is immutable; var is
mutable. All if, while, for, and match subjects use parentheses and braced bodies.
Fields use name: Type; construction uses Type { field: value }. Modules resolve
from src/ through dotted use paths. bin/ files are separate entry points.

## Ownership

Named aggregate transfers use move(value); fresh results transfer directly.
read T and edit T propose object borrows in addition to existing slice views.
Arguments spell read value and edit value. Read borrows share access; edit borrows
are exclusive; neither may escape the call. Structs owning resources are proposed.
All owned resources drop on every exit. Uncommitted transactions roll back on drop.
Dropping a transaction cannot undo an external side effect.

## Errors and types

try expression proposes unwrapping Result.ok or returning Result.err from the
current function. It is not an exception. All fallible backend calls here return
backend.Failure; there are no implicit error conversions. Failure contains an
allowlisted code and a private diagnostic cause. Result<void, E> uses Result.ok()
and case Result.ok without a binding. Option<T> has some(value) and none; patterns
use Option.some and Option.none. The backend.Option type is a re-export of the
proposed prelude Option type.

Vec<T>, owned String, generic library calls, Result<void, E>, object borrows,
resource fields, and function callbacks all need future compiler work. Arrays
retain explicit lengths. Integer widening remains explicit with widen<T>.

## Concurrency

Ordinary functions run on bounded runtime worker threads. No async/await or
implicit task spawning is proposed here. Named functions are typed callbacks.
Shared database/verifier handles require an explicit library thread-safety
contract: a read borrow alone does not make arbitrary objects thread-safe.
The runtime joins callbacks before borrowed application resources are destroyed.

Context carries cooperative cancellation, deadlines, allocation allowances and
step budgets. These are runtime contracts, not real-time or process-RSS proofs.
The driver and allocator must honor them. HTTP, JSON, database, logging and OIDC
remain libraries rather than language keywords.

## Value and projection details

backend.Id and digest.Hash are fixed-size immutable value types with copy semantics.
A match consumes an owned Option or Result and transfers its bound payload.
Vec<u8> projects to read u8[] for the duration of a call. Authentication parsing
returns an owned, redacted SecretToken rather than an escaping request borrow.
Resource field reads are borrowed; moving an entire owner invalidates its fields.
