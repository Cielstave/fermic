# Argument startup and numeric output

The implementation goal is an executable useful outside a hard-coded example:
accept user parameters, validate limits, consume a byte stream and report numeric
results. Arguments and formatting are separate APIs so pure application logic
can remain independently budgeted.

## Primary sources and conclusions

- [WG14 C11 draft N1570](https://www.open-std.org/jtc1/sc22/wg14/www/docs/n1570.pdf),
  section 5.1.2.2.1: hosted startup supplies argc/argv, permits argc zero and
  distinguishes the program-name slot from user parameters. Fermi initializes
  from this ordinary startup path and copies bytes into caller-owned storage.
- [Rust args_os documentation](https://doc.rust-lang.org/std/env/fn.args_os.html)
  separates OS arguments from UTF-8 validation and cautions against trusting the
  first argument as executable identity. Its documentation also describes special
  glibc startup integration for libraries. Fermi deliberately does not reproduce
  that platform-specific initialization mechanism.
- [Zig process implementation](https://github.com/ziglang/zig/blob/master/lib/std/process.zig)
  provides a reference for keeping process facilities in a standard library.
  Fermi's initial API uses explicit caller storage rather than promising a
  cross-platform owned argument collection before its allocator and string model
  exist. This is a Fermi design decision, not a source compatibility claim.

The Open Group exec reference was inaccessible during this review; it is not
used as evidence. These sources do not establish Windows argument behavior or
Fermi deployment portability.

## Chosen boundary and alternatives

A nested read view at main would require a new view/aggregate lifetime contract.
A heap-allocated argument collection would introduce allocation failure and
ownership before a caller can inspect basic startup state. Both may become useful
later. The current entry syntax remains unchanged; a private context captures
the host vector once, with no argument pointer exposed to source code.

Library mode reports unavailable rather than presenting an empty list as though
initialization succeeded. This makes the limitation explicit without linker
constructors, process introspection or an unreviewed public embedding ABI. The
internal runtime accepts explicit independent contexts and is testable without
process-global state. Only generated executable wiring owns the private instance.

Raw bytes avoid an implicit UTF-8 panic/conversion policy. Index and capacity
checks happen before writes. Empty values remain distinct from absent indices.
No log echoes untrusted argument values. Future UTF-8, option parsing and path
handling belong to separate contracts; a copied argument is still untrusted input.

Formatting uses fixed bounded loops and a preflight length check, then writes
digits directly to the caller's view. No heap, locale or I/O dependency is needed.
The pure formatter has a finite work certificate; process access remains an
external effect. This separation lets programs compose an explicit input boundary
with certifiable conversion logic.

## Application and failure evidence

The bytecount application validates a decimal input limit before allocation,
processes fixed-size chunks and emits a machine-readable count. Testing includes
real exec argument fidelity, non-UTF-8/empty arguments, over-capacity rejection,
multi-chunk input, exact limits, read/write failures and allocation failure.
Runtime tests additionally cover independent contexts and unchanged outputs on
errors. Frontend fuzzing does not exercise OS argument startup or stream failure
behavior; these require native integration tests.

On the tested Android host, launching the tool with stdin closed produced EOF
rather than the intended read failure. The failure test therefore supplies a
write-only descriptor, which consistently produces EBADF. Android's historical
[stdio initialization](https://android.googlesource.com/platform/bionic/+/2aebf5429bb1241a3298b5b642d38f73124c2026/linker/linker.cpp)
shows a mechanism for replacing closed standard descriptors with /dev/null;
this is supporting context, not proof of the exact loader build on this host.
The test checks an actual unreadable descriptor and does not assume closure
survives host startup.
