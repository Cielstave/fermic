#ifndef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 200809L
#endif
#include "fermi/file.h"

#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#if !defined(__linux__)
#error "Fermi file runtime requires Linux close semantics"
#endif

static bool file_live(const FermiFile *file) {
    return file && file->open && file->descriptor >= 0;
}

static void release_failed_open(int descriptor) {
    int saved = errno;
    (void)close(descriptor);
    errno = saved;
}

static FermiFileStatus file_open(FermiFile *output, const unsigned char *path, size_t length,
                                 bool creating) {
    if (!output || (!path && length)) {
        return FERMI_FILE_INVALID_ARGUMENT;
    }
    if (output->open) {
        return FERMI_FILE_INVALID_STATE;
    }
    if (!length) {
        return FERMI_FILE_INVALID_PATH;
    }
    if (length > 4096) {
        return FERMI_FILE_PATH_TOO_LONG;
    }
    if (memchr(path, 0, length)) {
        return FERMI_FILE_INVALID_PATH;
    }
    char name[4097];
    memcpy(name, path, length);
    name[length] = 0;
    /* Nonblocking acquisition prevents a FIFO path from waiting for a writer
     * before it can be rejected. Regular-file reads can still wait on storage. */
    int descriptor;
    do {
        descriptor = creating
                         ? open(name, O_WRONLY | O_CREAT | O_EXCL | O_CLOEXEC | O_NONBLOCK, 0600)
                         : open(name, O_RDONLY | O_CLOEXEC | O_NONBLOCK);
    } while (descriptor < 0 && errno == EINTR);
    if (descriptor < 0) {
        return FERMI_FILE_OPEN_FAILED;
    }
    struct stat info;
    int status;
    do {
        status = fstat(descriptor, &info);
    } while (status < 0 && errno == EINTR);
    if (status < 0) {
        release_failed_open(descriptor);
        return FERMI_FILE_INSPECT_FAILED;
    }
    if (!S_ISREG(info.st_mode)) {
        release_failed_open(descriptor);
        return FERMI_FILE_NOT_REGULAR;
    }
    *output = (FermiFile){descriptor, true};
    return FERMI_FILE_OK;
}

FERMI_FILE_API FermiFileStatus fermi_file_open_read(FermiFile *output, const unsigned char *path,
                                                    size_t length) {
    return file_open(output, path, length, false);
}

FERMI_FILE_API FermiFileStatus fermi_file_create_new(FermiFile *output, const unsigned char *path,
                                                     size_t length) {
    return file_open(output, path, length, true);
}

FERMI_FILE_API FermiFileStatus fermi_file_write(FermiFile *file, const unsigned char *input,
                                                size_t length, size_t *count) {
    if (!file || !count || (length && !input)) {
        return FERMI_FILE_INVALID_ARGUMENT;
    }
    if (!file_live(file)) {
        return FERMI_FILE_INVALID_STATE;
    }
    if (!length) {
        *count = 0;
        return FERMI_FILE_OK;
    }
    size_t chunk = length > 4096 ? 4096 : length;
    ssize_t written;
    do {
        written = write(file->descriptor, input, chunk);
    } while (written < 0 && errno == EINTR);
    if (written <= 0) {
        return FERMI_FILE_WRITE_FAILED;
    }
    *count = (size_t)written;
    return FERMI_FILE_OK;
}

FERMI_FILE_API FermiFileStatus fermi_file_sync(FermiFile *file) {
    if (!file) {
        return FERMI_FILE_INVALID_ARGUMENT;
    }
    if (!file_live(file)) {
        return FERMI_FILE_INVALID_STATE;
    }
    int status;
    do {
        status = fsync(file->descriptor);
    } while (status < 0 && errno == EINTR);
    return status == 0 ? FERMI_FILE_OK : FERMI_FILE_SYNC_FAILED;
}

FERMI_FILE_API FermiFileStatus fermi_file_move(FermiFile *output, FermiFile *source) {
    if (!output || !source || output == source) {
        return FERMI_FILE_INVALID_ARGUMENT;
    }
    if (output->open || !file_live(source)) {
        return FERMI_FILE_INVALID_STATE;
    }
    *output = *source;
    *source = (FermiFile){0};
    return FERMI_FILE_OK;
}

FERMI_FILE_API FermiFileStatus fermi_file_read(FermiFile *file, unsigned char *output,
                                               size_t capacity, size_t *count) {
    if (!file || !count || (capacity && !output)) {
        return FERMI_FILE_INVALID_ARGUMENT;
    }
    if (!file_live(file)) {
        return FERMI_FILE_INVALID_STATE;
    }
    if (!capacity) {
        *count = 0;
        return FERMI_FILE_OK;
    }
    size_t chunk = capacity > 4096 ? 4096 : capacity;
    ssize_t received;
    do {
        received = read(file->descriptor, output, chunk);
    } while (received < 0 && errno == EINTR);
    if (received < 0) {
        return FERMI_FILE_READ_FAILED;
    }
    *count = (size_t)received;
    return FERMI_FILE_OK;
}

FERMI_FILE_API FermiFileStatus fermi_file_close(FermiFile *file) {
    if (!file) {
        return FERMI_FILE_INVALID_ARGUMENT;
    }
    if (!file_live(file)) {
        return FERMI_FILE_INVALID_STATE;
    }
    int descriptor = file->descriptor;
    *file = (FermiFile){0};
    return close(descriptor) == 0 ? FERMI_FILE_OK : FERMI_FILE_CLOSE_FAILED;
}

FERMI_FILE_API void fermi_file_drop(FermiFile *file) {
    int saved = errno;
    if (file_live(file)) {
        (void)fermi_file_close(file);
    }
    errno = saved;
}
