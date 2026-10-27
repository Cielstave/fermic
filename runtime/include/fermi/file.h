#ifndef FERMI_RUNTIME_FILE_H
#define FERMI_RUNTIME_FILE_H

#include <stdbool.h>
#include <stddef.h>

#ifndef FERMI_FILE_API
#define FERMI_FILE_API
#endif

/* Internal Linux runtime contract, not a public embedding ABI. Slots start at
 * {0}; native callers must not copy live owners, alter fields or close borrowed
 * descriptors. Operations on the same owner require exclusive access. */
typedef struct {
    int descriptor;
    bool open;
} FermiFile;

typedef enum {
    FERMI_FILE_OK,
    FERMI_FILE_INVALID_ARGUMENT,
    FERMI_FILE_INVALID_STATE,
    FERMI_FILE_INVALID_PATH,
    FERMI_FILE_PATH_TOO_LONG,
    FERMI_FILE_OPEN_FAILED,
    FERMI_FILE_INSPECT_FAILED,
    FERMI_FILE_NOT_REGULAR,
    FERMI_FILE_READ_FAILED,
    FERMI_FILE_CLOSE_FAILED,
    FERMI_FILE_WRITE_FAILED,
    FERMI_FILE_SYNC_FAILED
} FermiFileStatus;

/* Open regular files for reading, with atomic close-on-exec. Paths are 1..4096 raw
 * bytes without NUL; symlinks are followed. This is not a path sandbox. Failure
 * leaves a vacant destination unchanged and releases any acquired descriptor. */
FERMI_FILE_API FermiFileStatus fermi_file_open_read(FermiFile *output, const unsigned char *path,
                                                    size_t length);

/* Exclusively create a new regular file for writing, mode 0600 (subject to umask).
 * Existing paths, including symlinks, are rejected. Failed operations after
 * creation may leave an empty file; no pathname is removed automatically. */
FERMI_FILE_API FermiFileStatus fermi_file_create_new(FermiFile *output, const unsigned char *path,
                                                     size_t length);

/* One write of at most 4096 bytes, retrying EINTR. Short writes succeed with
 * their count; zero progress on nonempty input is a write failure. Errors preserve
 * count but cannot roll back earlier output. The owner remains live. */
FERMI_FILE_API FermiFileStatus fermi_file_write(FermiFile *file, const unsigned char *input,
                                                size_t length, size_t *count);

/* Synchronize file data and metadata, retrying EINTR. This does not synchronize
 * the parent directory or provide a transaction. The owner remains live. */
FERMI_FILE_API FermiFileStatus fermi_file_sync(FermiFile *file);

/* Transfer into a distinct vacant slot, without a syscall or allocation. */
FERMI_FILE_API FermiFileStatus fermi_file_move(FermiFile *output, FermiFile *source);

/* One read of at most 4096 bytes, retrying EINTR. EOF succeeds with zero bytes.
 * Zero capacity performs no syscall and does not establish EOF. Storage must be
 * live, disjoint from owner/count and writable. Errors preserve count; an OS
 * read error does not promise unchanged destination bytes. The owner stays live. */
FERMI_FILE_API FermiFileStatus fermi_file_read(FermiFile *file, unsigned char *output,
                                               size_t capacity, size_t *count);

/* Consume before close, including when close fails. Never retry on Linux, even
 * for EINTR: the numeric descriptor may already identify another resource. */
FERMI_FILE_API FermiFileStatus fermi_file_close(FermiFile *file);

/* Best-effort fallback; preserves errno, drops at most once and ignores close
 * errors. Use explicit close when the application needs its failure result. */
FERMI_FILE_API void fermi_file_drop(FermiFile *file);

#endif
