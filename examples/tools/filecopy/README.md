# Filecopy

A native Fermi file-copy tool with a fixed 4096-byte transfer buffer and an 8 MiB
input limit. It uses local functions, byte views, explicit file owners, exclusive
borrows, Result propagation, and automatic cleanup. No application-specific C host
is required.

```sh
./build/fermi run examples/tools/filecopy/main.fe -- input.bin output.bin
```

The destination must not exist. Creation uses owner-only permissions subject to
umask. The source must be a regular file. On success, output is synchronized and
both owners are explicitly closed. Failure preserves the primary operation error;
a partial destination may remain and is never removed automatically. Copying is
not transactional, and synchronization does not persist the parent directory.

Exit status: 0 success, 64 invalid arguments, 65 size limit, 71 unavailable process
arguments, 74 file or diagnostic I/O failure. Native stack usage is not certified.
