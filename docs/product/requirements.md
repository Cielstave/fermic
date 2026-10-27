# Fermi product requirements

Status: active product direction; service acceptance remains unimplemented.
Reviewed 25 October 2026. Delivery order is defined in [the roadmap](../PLAN.md).

## Problem and intended users

Backend and infrastructure developers need native services whose data ownership,
allocation failures and resource limits can be understood during code review.
Fermi targets background workers, bounded data processing and network services.
It is not intended to replace every language used by an organization.

The product hypothesis is that explicit ownership plus observable resource costs
can make these services easier to operate. This is a hypothesis, not a claim of
novelty, customer demand, stronger safety than Rust, or superior performance.
No user interviews or production adoption have established it yet.

## Product boundaries

Compile .fe to native executables and libraries. Keep the compiler in C11 and
retain the C/Clang backend until a measured limitation justifies another backend.
Use familiar braces, parentheses, explicit types, let/var and visible transfers.
Keep HTTP, serialization, TLS, storage clients and scheduling policy in libraries.
The language owns type rules, ownership, access permissions and fault semantics.
Feature selection follows the [foundation admission rules](../design/feature-priorities.md#feature-admission-rules); a flexibility checklist is not a release commitment.

Linux x86-64 is the first service acceptance target. Termux AArch64 is the current
verified development environment; it does not establish Linux support. Public
service deployment, stable ABI and production readiness require separate evidence.

## Requirements and verification

| ID | Requirement | Acceptance evidence |
| --- | --- | --- |
| P01 | Compile a multi-module native application without an application-specific C host | End-to-end Fermi program build and execution |
| P02 | Prevent resource copies, conflicting borrows and use after transfer in safe source | Negative checker tests and generated-program lifetime tests |
| P03 | Preserve live owners on allocation/growth failure | Deterministic failure injection at every allocation site |
| P04 | Bound admission, queued data, active work and retained results | Overload tests checking counts and allocation accounting |
| P05 | Distinguish domain errors, language faults and compiler defects | Text/JSON diagnostics and runtime failure tests |
| P06 | Stop accepting work and release resources on shutdown | Cancellation, join and descriptor/allocation leak tests |
| P07 | Permit reusable libraries and controlled native integration | Module/visibility tests and documented C ABI obligations |
| P08 | Measure performance without disabling safety checks | Reproducible workload, compiler flags and latency/RSS reports |
| P09 | Preserve maintainability | Professional English source, stable names, focused modules and relevant tests |

P01 and parts of P02/P03/P05/P07/P09 exist today. P04/P06 service behavior is not
implemented. Existing microbenchmarks do not satisfy P08 for a running service.
See the [specification index](../spec/README.md) for available features.

## Foundation flexibility gate

Service workloads guide validation without restricting ordinary data and control
abstractions to backend programs. Before describing Fermi as sufficiently flexible
for independent application development, demonstrate these native, multi-module
programs using supported source syntax:

| Program | Required evidence | Current gap |
| --- | --- | --- |
| Record transformation | User structs, arrays, shared reads, in-place updates, checked faults | Call-scoped struct access implemented; broader containers remain limited. |
| Runtime text processor | Read text, validate UTF-8, retain variable-sized values, write a report | Dynamically owned text and safe text views are unavailable. |
| Variable-sized record store | Grow a bounded collection of user structs, update entries, recover from allocation failure, release resources | General generic collections are unavailable. |
| Reusable algorithm library | Share typed algorithms across user types and supply caller-defined operations | General generics and function values are unavailable. |
| Numeric application | Fractional arithmetic, explicit conversions, documented exceptional values | Floating-point types are unavailable. |

The gate requires expected results, invalid-source diagnostics, O0/O2 execution,
and relevant cleanup/failure checks. It is not a promise that arbitrary programs
compile, that safe references may escape, or that system APIs already exist.
Missing language mechanisms must be distinguished from intentional safety errors.
Passing this foundation gate does not establish service acceptance or production
readiness. Record actual applications and remaining restrictions before announcing
that the gate has been reached.

## First acceptance application

Build one process with HTTP submission, status lookup, health endpoints and a
bounded in-memory queue. Jobs compute a byte histogram. Responses distinguish
accepted, invalid, missing and overloaded requests. Reject unsupported job kinds.
Job data and results are lost on restart; do not promise durable delivery.

The acceptance profile starts with proposed, configurable limits: 64 KiB per
payload, 128 queued jobs, 4 active workers, 128 connections and 1024 retained
results. Queued payload capacity is therefore at most 8 MiB, excluding active
payloads, response buffers, metadata and allocator overhead. These are test inputs,
not measured production defaults or a process-wide RSS guarantee.

Verify a complete submit/execute/read cycle; invalid and truncated requests;
slow peers and disconnects; overload rejection; expiration of results; injected
allocation/I/O failure; and shutdown during queued, running and waiting work.
Use a ten-second shutdown grace in the test profile and define forced termination
separately from orderly cleanup. Run a 30-minute bounded-load soak after warmup;
track live resources and RSS trends rather than requiring RSS to return exactly
to startup. Run these cases on Linux before declaring the milestone complete.

No database, multi-tenancy, billing or distributed delivery is required for this
first application. Foundry exercises those longer-term needs as a design reference.
Initial networking acceptance uses loopback/private test environments; internet
exposure requires a separate transport and security review.

## Product validation and release limits

Before freezing broad abstractions, obtain feedback from developers working on
at least three representative service tasks. Record difficulty, rejected programs,
missing abstractions and workarounds. Prefer removing repeated friction over adding
unrelated features. No feedback is claimed to have been collected yet.

Measure throughput, request p50/p95/p99, errors, allocation counts, peak RSS, binary
size, compile time and shutdown time using matched workload semantics. Establish
numerical performance gates after a reproducible baseline; inventing a speed target
would not prove fitness. A release remains experimental until its acceptance
report, platform coverage, known limitations and compatibility policy are published.
