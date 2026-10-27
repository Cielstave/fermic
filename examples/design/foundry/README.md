# Foundry: future Fermi backend

**Design simulation only. These files do not compile with the current Fermi
compiler.** This is a language-design reference, not a runnable production server.
All application source is .fe. No C or Python host is required by the proposed
architecture. HTTP, database, authentication, and concurrency APIs are imagined
libraries, not existing Fermi capabilities.

Foundry is a multi-tenant background-job service. The API accepts bounded sample
batches; workers compute histograms and persist results. PostgreSQL provides the
durable queue. A shared limiter and an identity provider are external services.
The project illustrates a large backend's boundaries using one complete business
flow rather than many unrelated placeholder features.

## Reading order

1. [Syntax](docs/syntax.md) and [feature status](docs/features.md).
2. [Architecture and contracts](docs/architecture.md).
3. [API entry](src/bin/api.fe) and [worker entry](src/bin/worker.fe).
4. [Routes](src/http/routes.fe), [service](src/jobs/service.fe),
   [storage](src/storage/jobs.fe), and [worker](src/workers/runner.fe).
5. [Acceptance scenarios](tests/scenarios.fe).

## Organization

- src/bin/: API and worker executables.
- src/platform/: configuration and resource ownership.
- src/security/: identity verification and tenant membership.
- src/http/: routing, decoding, limits, and public error handling.
- src/jobs/: domain models and transactional operations.
- src/storage/: parameterized persistence and queue operations.
- src/workers/: bounded computation, retries, retention, and shutdown.
- tests/: future integration scenarios; not executable tests today.

## API

| Method | Path | Behavior |
| --- | --- | --- |
| POST | /v1/tenants/{tenant}/jobs | Submit with Idempotency-Key; 202 |
| GET | /v1/tenants/{tenant}/jobs/{job} | Tenant-scoped state and result |
| POST | /v1/tenants/{tenant}/jobs/{job}/cancel | Idempotent cancellation |
| GET | /health/live | Internal process health |
| GET | /health/ready | Internal dependency readiness |

Clients submit byte samples, never scripts, URLs, paths, or SQL. Extensibility
comes from adding typed job handlers. This example does not claim arbitrary
plugin execution, exactly-once processing, or measured performance.

## Validation

There is deliberately no build manifest or runnable command. Do not add these
files to compiler acceptance tests. Review file structure and design consistency;
compiler, runtime, security, and performance validation require implementation.
Actual migrations, codecs, clients, and deployment remain future work.
