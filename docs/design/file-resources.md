# Read-only file resource foundation

Status: the Linux C runtime and compiler integration are implemented.
[std.fs](../spec/files.md) exposes opaque File ownership, exclusive borrowing,
read_into and consuming close. The restrictions below remain part of the gate
for broader native owners and resource nesting.

## Implemented runtime

runtime/include/fermi/file.h and runtime/io/file.c define an internal FermiFile
owner with an integer descriptor and a separate live-state flag. A zeroed slot
is vacant, so descriptor zero can be a valid acquired resource. Native callers
must not copy live owners, alter descriptors or close them behind the runtime.
Operations on an owner require exclusive access.

| Operation | Success | Failure |
| --- | --- | --- |
| open_read | Install one regular-file owner | Preserve vacant destination; close any descriptor acquired before inspection failure |
| move | Transfer into a distinct vacant slot and vacate source | Preserve both slots |
| read | Report bytes from one read, capped at 4096 | Keep owner live and preserve count; OS errors may modify output bytes |
| close | Consume owner and close once | Owner remains consumed after a failed syscall; no retry |
| drop | Best-effort close and vacate | Ignore close error, preserve errno and never retry |

Paths are raw bytes, 1..4096 bytes, without embedded NUL. The path is copied into
fixed local storage and terminated before open. This is a runtime capacity policy;
OS/filesystem path limits may be stricter. There is no UTF-8 conversion, heap
allocation or truncation.

Open uses O_RDONLY, O_CLOEXEC and O_NONBLOCK in one acquisition. Close-on-exec
avoids an open-then-fcntl inheritance window. Nonblocking acquisition prevents a
FIFO from waiting for a writer before inspection can reject it. fstat inspects
the acquired descriptor; only regular files are accepted. A directory, FIFO or
other object is released rather than published as an owner. Open and inspection
retry EINTR. Cleanup after failed inspection preserves that operation's errno.

Symlinks are followed; relative paths use the process working directory. This API
is not a filesystem sandbox or a guarantee that opening a special object has no
side effects before rejection. A capability-root/path-containment API requires a
separate contract. Regular-file reads can still wait on storage; O_NONBLOCK does
not establish a deadline. Files can change while being read; no snapshot is made.

read retries EINTR, accepts short reads and reports EOF with zero bytes. A
zero-capacity read does not issue a syscall and does not prove EOF. Caller output
and count storage must be live and disjoint from the owner and one another.

The adapter is explicitly restricted to Linux close semantics, including Android.
It vacates the slot before close; even EINTR is returned as CLOSE_FAILED without
retry. A numeric descriptor might already be reused. Explicit close is fallible;
fallback drop cannot report its failure and must preserve an earlier diagnostic.
Read-only files are the initial scope. Write, fsync, create/truncate, seek,
nonregular handles and directory capabilities are absent.

## Implemented compiler integration

A file owner is non-copyable, movable and automatically dropped on scope,
return and fault exits. An exclusive call-scoped borrow may read without consuming
the file. Explicit close consumes it on both success and failure. Arrays, nested
resources and user construction of native owner fields remain rejected until
cleanup is specified. Raw descriptors must not be exposed as freely copied values.

Cleanup now dispatches by resource kind and recursively handles a resource Result
payload. File owners reach file drop rather than buffer release. Effect analysis
distinguishes fs.close from memory cleanup and propagates native fs.open/fs.read.
Generated-program tests cover owner temporaries, Result transfer, late argument
faults, return cleanup, replacement and mixed buffer/file owners.

[Unit-success Results](../spec/unit-results.md) are now implemented, removing the
need for a meaningless success payload in future close APIs.
The compiler accepts the opaque native struct declaration only for std.fs.File
and validates every native file signature and error enum. This is a trusted
standard-library facility, not a general unchecked native declaration mechanism.

## Evidence and remaining gate

make test-runtime exercises real files and injected syscalls. Tests cover chunked
reads, EOF, descriptor zero, move invalidation, close-on-exec, FIFO/directory
rejection, malformed paths, failed open/inspection, interrupted acquisition/read,
partial read, failed close and drop preserving errno. make sanitize-runtime runs
these under ASan/UBSan. Source integration adds a separate generated-program
suite; no new platform or unrestricted native interoperability is claimed.

Compile-time copy/use-after-close/alias rejection, explicit close Results, effect
propagation and exactly-once generated cleanup now have tests at O0/O2.
[filecat](../../examples/tools/filecat/README.md) is an executable Fermi application.
The next gate concerns resource fields, broader opaque owners and the control-flow
representation needed for more general lifetimes.
