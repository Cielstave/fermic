# Startup configuration review

Reviewed 25 October 2026. The implemented contract is in
[configuration](../spec/configuration.md). Sources guide the design; they do not
establish that Fermi's implementation is correct.

## Primary references

- [Linux getenv manual](https://man7.org/linux/man-pages/man3/getenv.3.html): lookup
  returns a pointer to environment storage; later calls or mutations can invalidate
  assumptions about that storage. Fermi copies within the call and exposes no pointer.
- [Go LookupEnv](https://pkg.go.dev/os#LookupEnv): absent and present-empty values
  are distinct. Fermi preserves that distinction with a domain error versus zero
  successful bytes rather than treating an empty string as absence.
- [Zig allocator interface](https://github.com/ziglang/zig/blob/master/lib/std/mem/Allocator.zig):
  allocation and failure are explicit contracts. Fermi's startup adapter instead
  requires caller storage; no automatic resize/retry is introduced at this boundary.

## Fermi-specific decisions

Use a portable ASCII key policy with a fixed 128-byte name limit. Bound value-copy
work by the destination and leave it unchanged on any domain error. Keep libc
lookup outside static work/heap claims. Distinguish `env.read` from I/O and memory
effects so ambient state access is visible to the caller and to tooling.

Write numeric parsing and range validation in Fermi. Restrict input length before
looping; guard arithmetic before overflow. The compiler can then certify the pure
parser while keeping environment-dependent configuration outside the certificate.
This reuses the language's ownership, Result and budget system without adding
configuration-specific syntax.

Environment contents and libc behavior cross a trust boundary. Abuse cases include
embedded NUL/invalid names, oversized values, hidden truncation, numeric overflow,
invalid intervals, concurrent host mutation and raw-value disclosure in logs.
Tests cover the sequential input/failure cases. Concurrent environment mutation is
excluded by the native-host contract, not claimed to be solved by the adapter.

## Limits of the conclusion

These choices make a concrete startup workload possible. They do not establish
market demand, a unique language feature, service throughput, a scheduler or a
security capability model. CLI arguments, deadlines and resource-file lifecycle
remain separate milestones. No third-party implementation is bundled or copied.
