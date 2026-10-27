# Owned regular files

Status: std.fs is implemented for the Linux/Android pilot. It uses the tested
[Linux file runtime](../design/file-resources.md), embedded with private linkage
in generated code. No separately linked Fermi runtime is required.

## Standard interface

```fermi
pub native struct File;
pub enum Error {
    invalid_argument, invalid_state, invalid_path, path_too_long,
    open_failed, inspect_failed, not_regular, read_failed, close_failed,
    write_failed, sync_failed,
}
pub native func open_read(path: read u8[]) -> Result<File, Error>;
pub native func read_into(file: edit File, output: edit u8[]) -> Result<usize, Error>;
pub native func close(file: File) -> Result<void, Error>;
pub native func create_new(path: read u8[]) -> Result<File, Error>;
pub native func write(file: edit File, input: read u8[]) -> Result<usize, Error>;
pub func write_all(file: edit File, input: read u8[]) -> Result<void, Error>;
pub native func sync(file: edit File) -> Result<void, Error>;
```

The native struct declaration is accepted only for the compiler-trusted std.fs.File
interface. It is not a general native-extension facility. The source owner is
named fs.File after use std.fs; its descriptor/state fields are inaccessible and
there is no source constructor. A user-defined File struct is a distinct type.

## Ownership and access

An opened file has exactly one owner. Copying is rejected. Named owner transfers
require move(file), including function arguments and returns. A successful
Result<fs.File, E> owns its payload; matching transfers that ownership into the
success binding. Resource Results cannot be discarded without handling.

Reading changes the file position, so it requires edit fs.File and a mutable
owning binding. A borrowed file parameter can be reborrowed for another call.
Borrows require stable bindings or fields of mutable structs and cannot be stored in locals, aggregates or
Results, returned, or passed alongside a conflicting use/transfer of the owner.
Two exclusive borrows of the same file in one call are rejected.

close consumes an owning File on both success and error. Its Result must be
handled. A close error does not return a retryable file. The source binding stays
moved on either branch and may only be used after explicit reinitialization of a
mutable binding. On Linux the runtime clears the owner before close and never
retries EINTR or another close error, avoiding accidental closure of a reused fd.

Locals, owning parameters, pending arguments and owning Result payloads receive
automatic fallback cleanup on scope exit, return or language fault. Replacement
evaluates the new owner before dropping the old one. Failed acquisition leaves
an existing owner intact until its own scope/exit cleanup. Borrow parameters have
no cleanup obligation. Drop errors do not replace an existing fault or errno;
applications needing close failure must call close explicitly.

[Resource struct fields](resource-structs.md) can own files and borrow them for reads, writes and sync.
Arrays of files, nested resource Results, and owner/borrow C ABI exports remain rejected. Public Fermi functions may transfer owners between local
modules. Exported C interfaces can accept paths/output views and return scalar or
unit Results while keeping file owners internal.

## Paths, reads and failures

Paths contain 1..4096 raw bytes without NUL. The runtime copies and terminates the
path without allocation or truncation; OS limits may be stricter. Symlinks are
followed and relative paths use the current directory. This is not a filesystem
sandbox or a path authorization policy.

Acquisition uses O_RDONLY | O_CLOEXEC | O_NONBLOCK, then inspects the acquired fd
with fstat and accepts only regular files. Nonblocking acquisition lets FIFO paths
be rejected without waiting for a writer. Other special objects may have effects
on open before inspection; rejection is not a universal side-effect guarantee.

A read performs at most 4096 bytes of I/O, retries EINTR and accepts short reads.
EOF returns zero with a nonempty destination. Zero output capacity performs no
syscall and also returns zero, so it does not establish EOF. Domain errors return
no byte count and may leave output partially modified by the OS. No terminator is
written. Read errors keep the file owner live; callers may close it explicitly.
Regular-file operations can block on storage and do not enforce std.time deadlines.

## Creation, writes and synchronization

create_new exclusively creates a new regular file for writing, with mode 0600
subject to umask and atomic close-on-exec. Existing paths, including final-component
symlinks, are rejected without truncating their contents. It does not create parent
directories or authorize the path. Intermediate symlinks are still resolved by the
OS. Inspection failure after creation may leave an empty file. Failure never
removes a path automatically.

write borrows the file exclusively and the source bytes read-only. It performs at
most 4096 bytes per call, retries EINTR, and returns the actual count on a short
write. Empty input succeeds without a syscall. Zero progress for nonempty input is
write_failed. Other errors leave the owner live and provide no count. Earlier
writes cannot be rolled back. write_all is ordinary Fermi library code that loops
over the remaining slice until complete or the first error.

sync borrows the owner, calls fsync, retries EINTR and returns sync_failed on other
errors. It synchronizes file data and metadata; it does not synchronize the parent
directory, make output transactional, or impose an operation deadline. Neither
write_all nor close implies sync. A successful sync must still be followed by
explicit close when the application needs close errors. Access mode is currently
an OS-level property of the opaque File, so writing to an open_read owner returns
write_failed rather than a compile-time mode error.

## Effects and certificates

- open_read carries fs.open and fs.close, including cleanup of failed acquisition.
- read_into carries fs.read; borrowing alone does not add a close obligation.
- close and ownership cleanup carry fs.close.
- create_new carries fs.open, fs.close and fs.write.
- write and write_all carry fs.write; sync carries fs.sync.
- File ownership does not imply a memory effect. Mixed buffer/file functions carry
  both kinds of effects when appropriate.

These effects propagate through calls and owning values. Work/external heap
metadata remain unknown and @budget functions reaching file access or cleanup are
rejected. An unused borrowed parameter alone does not turn into an owning file.
The descriptor wrapper allocates no heap, but this does not certify kernel/libc
costs or native stack size.

## Compatibility

The write_failed and sync_failed error variants are appended after the existing
read/close variants; earlier numeric discriminants are unchanged. Existing source
that exhaustively matches fs.Error must handle the new variants. The opaque owner
layout and existing exported scalar/view Result ABI are unchanged.

## Executable and evidence

[filecat](../../examples/tools/filecat/README.md) opens a path, streams bytes through
one fixed buffer and explicitly closes the owner. Tests exercise O0/O2 generated
cleanup for moves, Result forwarding, pending arguments, faults, replacement,
loops, mixed resources and failed close. Separate objects retain private runtimes.
Source rejection tests cover copies, moved/immutable owners, conflicting borrows,
opaque construction, escape and ABI boundaries. Run sh tests/files.sh; sanitizer
coverage is included in make sanitize.

[filecopy](../../examples/tools/filecopy/README.md) copies at most 8 MiB using
bounded buffers, rejects existing destinations, synchronizes output and handles
explicit close results. Run sh tests/file-output.sh for O0/O2 write-failure,
short-write, sync, exact-limit, overwrite and cleanup checks.

Append, truncation, seek, directory handles, sockets and concurrency remain
unavailable. Automatic cleanup does not guarantee write durability, transactional
I/O or cleanup after arbitrary process termination.

The runtime contract follows the Linux man-pages for
[open](https://man7.org/linux/man-pages/man2/open.2.html),
[write](https://man7.org/linux/man-pages/man2/write.2.html), and
[fsync](https://man7.org/linux/man-pages/man2/fsync.2.html).
