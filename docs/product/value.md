# Fermi value proposition

Status: product hypothesis with implementation evidence, 25 October 2026.
No claim of uniqueness, general performance superiority or production readiness.

## Intended reason to choose Fermi

Build native services whose ownership, resource allowances and failure behavior
can be reviewed directly in code. Keep the language small enough that application
logic does not depend on a large set of implicit conversion or lifetime rules.
The target is backend infrastructure, workers and bounded data processing.

## Evidence already available

| Value | Concrete behavior | Evidence |
| --- | --- | --- |
| Controlled growth | Buffers carry a capacity-byte limit; failed growth preserves the owner | Generated-program allocation-failure and cleanup tests |
| Visible access | read/edit distinguish element views from exclusive descriptors | Alias, lifetime and relocation rejection tests |
| Explainable costs | Pure bounded functions can carry work/heap certificates; external effects remain unknown | Decimal parser certificates and env.read propagation |
| Predictable failure | Required Results, domain errors, language faults and compiler diagnostics remain distinct | Parser, configuration, native ABI and fault tests |
| Reusable binary primitives | Checked shift counts and explicit byte order without allocation or host layout assumptions | Exhaustive byte operations, endian vectors and work certificates |
| Executable composition | User arguments, numeric conversion and bounded stream processing compose in Fermi | Multi-file bytecount application with I/O and allocation failure tests |
| Bounded input handling | Startup reads do not truncate; strict numeric validation precedes application work | Destination-preservation and startup-profile tests |

The combination is the current design direction. Ownership, bounded APIs and
static analysis exist elsewhere; unfamiliar syntax alone would not make Fermi
more useful. Compatibility with familiar braces, parentheses and explicit types
remains intentional.

## Next adoption tests

1. A command-line worker reads arguments/configuration, processes bounded input
   and emits machine-readable results without an application-specific C host.
2. A TCP process handles slow peers, partial messages, failure and clean shutdown
   under explicit memory and connection limits.
3. A bounded HTTP worker makes admission, cancellation and resource ownership
   visible across task boundaries.

Each workload must include overload and failure cases before performance claims.
Measure developer workarounds as well as throughput, latency, RSS and binary size.
Gather developer feedback before freezing broad abstractions or adding syntax.

## Remaining differentiator research

Evaluate task-scoped resource allowances and inspectable effect summaries as
extensions of existing ownership/cost machinery. Define who owns a quota, what
is charged, how failure propagates and what happens at cancellation before choosing
keywords. Static work units are not deadlines; per-buffer limits are not global
quotas; current effects do not constitute a general capability-security system.
