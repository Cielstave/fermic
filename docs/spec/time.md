# Monotonic time and deadline arithmetic

Status: std.time is implemented for the current POSIX native target. All units
below are nanoseconds stored in the pilot's 64-bit usize. The origin is unspecified;
these values are not Unix timestamps and must not be persisted as calendar time.

```fermi
pub enum Error { clock_failed, invalid_reading, overflow, }
pub native func monotonic_ns() -> Result<usize, Error>;
@budget(work = 100, heap = 0)
pub func deadline_after(now_ns: usize, timeout_ns: usize) -> Result<usize, Error>;
@budget(work = 100, heap = 0)
pub func remaining_ns(deadline_ns: usize, now_ns: usize) -> usize;
```

## Native clock

monotonic_ns calls clock_gettime(CLOCK_MONOTONIC). Failure returns clock_failed.
Negative seconds, negative nanoseconds or nanoseconds at least one billion return
invalid_reading. Conversion checks multiplication and addition before producing
a value; values above usize maximum return overflow rather than wrapping or
raising an arithmetic fault. It performs no explicit allocation.

This call has the transitive clock.read effect. Work and external heap metadata
are unknown, so @budget functions cannot call it. Native library mode works
without a process-argument context. Generated C requests POSIX declarations before
system headers. The target must provide CLOCK_MONOTONIC and a time_t no wider than
uintmax_t. Existing target/platform limitations still apply.

Equal consecutive readings are allowed. Nanosecond units do not promise nanosecond
resolution. On the tested Android/Linux family, CLOCK_MONOTONIC excludes suspended
time and can be frequency-adjusted; it is not a suspend-aware service timeout
policy. This API does not silently switch to a calendar clock if monotonic access
fails.

## Pure deadline operations

deadline_after takes an explicit current sample and duration. If their sum is not
representable, it returns Error.overflow. Otherwise it returns the absolute
monotonic deadline. A zero duration produces the current sample and is immediately
expired. No clock query occurs inside this function.

remaining_ns returns deadline - now when now is earlier, and zero when now is at
or after the deadline. Subtraction cannot underflow. The function does not read the
clock, sleep, wait or cancel an operation. Both helpers are ordinary Fermi, have
finite work certificates, zero heap use and no external effects.

Callers must consistently use nanoseconds and the same clock domain. These pilot
APIs use usize rather than opaque instant/duration types; the checker does not
prevent mixing units or fabricated timestamps. Recompute remaining time from a
fresh sample when retrying an operation. Kernel waits and blocking stdin are not
interruptible through this API. Deadline enforcement requires future I/O/runtime
integration; do not describe arithmetic alone as a timeout guarantee.

[deadline.fe](../../examples/cli/deadline.fe) exercises a real clock read and
constructs a one-second deadline without spinning or sleeping. Run sh tests/time.sh
for injected failure/conversion tests, pure boundary arithmetic and native samples
at both O0 and O2. When compiling generated headers named time.h, use -iquote for
the artifact directory so <time.h> still resolves to the system header.
