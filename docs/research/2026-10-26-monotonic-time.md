# Monotonic clock boundary

The next runtime prerequisite is a timestamp source and explicit deadline
arithmetic. Network waits and task cancellation cannot be implemented correctly
by reusing wall-clock values or resetting a relative timeout after every retry.
This change supplies the clock boundary, not a scheduler or cancellation system.

## Primary reference

The [Linux man-pages clock_gettime reference](https://man7.org/linux/man-pages/man2/clock_gettime.2.html)
documents CLOCK_MONOTONIC, permitted equal readings, frequency adjustments and
exclusion of suspended time on Linux. It also identifies feature-test macro
requirements and failure reporting. Those details determine the initial POSIX
backend and prohibit promising calendar, suspend-aware or nanosecond-resolution
semantics from a nanosecond integer representation.

Fermi requests CLOCK_MONOTONIC explicitly and reports failure instead of falling
back to CLOCK_REALTIME. A checked conversion rejects malformed timespec fields
and values outside 64-bit nanoseconds. The implementation validates before
multiplying seconds or adding the fractional component. A 64-bit nanosecond
representation is finite even when time_t can represent much larger seconds.

## Composition and limits

Clock acquisition is a native external effect. Adding a duration and calculating
remaining allowance are pure Fermi functions with explicit inputs, making them
deterministically testable and independently budgeted. Overflow is a domain error;
expired deadlines produce zero remaining allowance. A zero-duration deadline is
immediate, not a sentinel for an unlimited wait.

The pilot currently uses usize rather than introducing partially enforced opaque
time types. This preserves the existing type grammar but leaves unit/domain
mixing unchecked. A future strong-time-type proposal should cover instants,
durations, checked conversion, source identity and ABI before changing the API.
Suspend-inclusive timing, calendar logging, sleeping and I/O cancellation need
separate contracts; they are not inferred from this implementation.

## Evidence

A fake clock drives success, host failure, negative/invalid fields and exact
SIZE_MAX conversion boundaries. Pure helper tests verify addition overflow,
immediate/exhausted deadlines and large values without calling the fake clock.
Native samples verify nondecreasing readings while allowing equal values.
Generated programs run at O0/O2, with sanitizer coverage. The executable example
uses a real monotonic sample and initializes a deadline without busy waiting.

A generated artifact named time.h can shadow the system header if consumers add
its directory with -I. The integration harness uses -iquote for generated headers,
keeping quoted artifact includes separate from system <time.h> lookup. No special
absolute system-header path is embedded in generated code.
