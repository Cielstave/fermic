# Architecture and library contracts

## Request path

The HTTP library caps bodies before buffering. OIDC verification pins configured
issuer, audience, algorithms, expiry and key policy. Authorization queries trusted
membership state for the requested tenant on every job endpoint. A shared limiter
enforces tenant admission before body decoding. Browser CORS and cookies are not
enabled. TLS terminates at a configured ingress; the API binds privately.

Submission validates samples and key length, fingerprints the operation version,
tenant and canonical bytes, and begins a transaction. It locks the idempotency key,
checks replay, reserves quota, inserts the job and receipt, then commits. Replay
returns the original receipt without charging quota. Changed payload means 409.
Transaction drop rolls back all changes on error.

## Storage contract

Migrations must create memberships, jobs, tenant_quota and idempotency tables.
Job keys include tenant_id; all reads and writes require it. States are queued,
running, succeeded, failed and cancelled. Samples/results are bounded bytes.
Index queue availability and lease expiry. Database row codecs reject invalid
states and oversized data. SQL parameters are bound, never interpolated.

queue.lock_key takes a transaction-scoped database lock for (tenant, key).
Hash collisions may serialize unrelated keys but cannot merge records. Idempotency
has a unique (tenant_id, key) constraint and 24-hour retention. queue.replay ignores
expired records and queue.remember replaces them under the same lock. After expiry
a repeated key may create a new job. A matching replay keeps its original response,
including its original queued state; GET supplies current state.

queue.reserve_slot locks a tenant quota row and increments pending below its cap.
Every first transition to a terminal state decrements pending atomically; repeated
completion/cancellation never decrements it twice. Cancellation makes queued or
running work terminal immediately; success already committed remains success.

queue.claim uses FOR UPDATE SKIP LOCKED, database time, a fresh lease token and an
incremented attempt count. It may reclaim expired leases below five attempts.
Exhausted work becomes failed and releases quota. Completion checks tenant, job,
running state, token and unexpired lease. Stale workers cannot publish results.
The system is at least once; histogram execution has no external side effects.
Future external effects need an outbox plus recipient deduplication.

## Worker and shutdown contract

Each worker holds one payload. Compute checks cancellation every 1024 bytes, with
a five-second task deadline and a 30-second lease. No renewal is modeled; if a
worker stalls beyond its lease, its completion is rejected. Cancellation may not
stop computation immediately but wins against later completion in the database.

queue.recover leaves stale/shutdown leases for reclamation; permanent errors become
failed. Transient errors requeue with exponential backoff and jitter capped at
60 seconds, with five attempts total. Fencing and quota updates are transactional.
If recovery itself fails, the supervisor cancels siblings and exits unsuccessfully;
leases eventually expire. All database calls have bounded statement timeouts.

The runtime stops admission on shutdown, cancels contexts, and joins within ten
seconds. Pools close after callbacks finish. A forced process exit leaves leases
for later recovery. Cooperative cancellation cannot interrupt arbitrary native code;
blocking adapters must support bounded waits. API runtime registers internal
health routes and normalizes shutdown cancellation to successful process exit.

## Security and observability

Secrets come from configuration and use redacted types. Logs record operation,
request ID and allowlisted error code, not credentials, payloads or private causes.
Public failure mapping: 400 malformed input, 401 authentication, 403 membership,
404 absent tenant-scoped job, 409 idempotency conflict, 413 oversized body,
422 domain validation, 429 shared limit, 503 capacity/dependency failure, 500 other.
Responses contain a stable code and request ID. Metrics use bounded labels only.

Sample bytes are non-personal by the application's input contract. Terminal jobs
expire after seven days; idempotency after 24 hours. Maintenance deletes bounded
batches; backups require a separate retention policy. No user-directed outbound
URL fetch, filesystem operation, or shell execution occurs.

## Library boundary

std provides basic collections and bytes; backend proposes HTTP, database, OIDC,
configuration, queue, runtime, telemetry and codecs. Model-specific codecs and
queue row mappings used here are proposed application adapters exposed by backend
for brevity, not automatic compiler reflection. Their implementations are absent.

## Deadline limitation

HTTP timeout limits the response lifetime; it does not by itself roll back a
transaction that already committed. A client timing out after commit must replay
the same idempotency key. Database adapters enforce bounded pool acquisition and
statement waits. Propagating a request deadline through every storage operation
is an additional API-design task, not a guarantee of this sample.
