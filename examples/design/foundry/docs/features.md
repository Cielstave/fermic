# Feature status

| Feature | Status |
| --- | --- |
| Explicit types, let/var, braces, parenthesized control flow | Existing syntax |
| Checked integers, fixed arrays, fieldless enums | Existing compiler support |
| Modules, Result match, move, scalar read/edit slices | Existing with pilot restrictions |
| Fixed-length Buffer<u8> | Existing, not a dynamic Vec or String |
| General object borrows and structs containing owned resources | Proposed extension; only byte-buffer descriptor borrows exist today |
| Vec<T>, String, Option<T>, generic library calls | Proposed support |
| try expression, Result<void, E>, callback values | Proposed extension |
| Thread-safe shared handles, bounded thread pools | Proposed runtime/type contracts |
| HTTP, JSON, SQL, OIDC, shared rate limits | Proposed libraries |
| Leases, idempotency, quotas, retention | Application/shared-library design |
| Integration harness and fault injection | Proposed test library |

## Demonstrated backend requirements

Tenant authorization; strict decoding; bounded requests; transactional submission;
idempotency; queue admission; lease fencing; capped retries; cancellation races;
structured logs; readiness; graceful shutdown; retention; testable failure paths.

The example does not implement dependencies behind convenient API names. Their
contracts are listed in architecture.md. Full migrations, row/JSON codecs,
package resolution, transport drivers, cryptographic verification, and executable
tests remain absent. This directory is a design workload for evaluating Fermi.
