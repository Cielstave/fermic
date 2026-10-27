#define _POSIX_C_SOURCE 200809L
#include "fermi/file.h"
#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

static int fake_open(const char *path, int flags, ...);
static int fake_stat(int descriptor, struct stat *info);
static ssize_t fake_read(int descriptor, void *output, size_t length);
static int fake_close(int descriptor);
static ssize_t fake_write(int descriptor, const void *input, size_t length);
static int fake_sync(int descriptor);
#define open(...) fake_open(__VA_ARGS__)
#define fstat(...) fake_stat(__VA_ARGS__)
#define read(...) fake_read(__VA_ARGS__)
#define close(...) fake_close(__VA_ARGS__)
#define write(...) fake_write(__VA_ARGS__)
#define fsync(...) fake_sync(__VA_ARGS__)
#include "../../runtime/io/file.c"
#undef open
#undef fstat
#undef read
#undef close
#undef write
#undef fsync

static unsigned opens, inspections, reads, closes;
static unsigned open_interruptions, inspect_interruptions, read_interruptions;
static int open_error, inspect_error, read_error, close_error;
static bool regular;
static size_t requested;
static unsigned writes, syncs, write_interruptions, sync_interruptions;
static int write_error, sync_error;
static ssize_t write_count;
static bool creating;

static void reset(void) {
    opens = inspections = reads = closes = 0;
    open_interruptions = inspect_interruptions = read_interruptions = 0;
    open_error = inspect_error = read_error = close_error = 0;
    regular = true;
    requested = 0;
    writes = syncs = write_interruptions = sync_interruptions = 0;
    write_error = sync_error = 0;
    write_count = 1;
    creating = false;
}

static int fake_open(const char *path, int flags, ...) {
    assert(!strcmp(path, "input"));
    assert(flags == (creating ? O_WRONLY | O_CREAT | O_EXCL | O_CLOEXEC | O_NONBLOCK
                              : O_RDONLY | O_CLOEXEC | O_NONBLOCK));
    ++opens;
    if (open_interruptions) {
        --open_interruptions;
        errno = EINTR;
        return -1;
    }
    if (open_error) {
        errno = open_error;
        return -1;
    }
    return 0; // Descriptor zero is a valid acquired owner.
}

static int fake_stat(int descriptor, struct stat *info) {
    assert(descriptor == 0);
    ++inspections;
    if (inspect_interruptions) {
        --inspect_interruptions;
        errno = EINTR;
        return -1;
    }
    if (inspect_error) {
        errno = inspect_error;
        return -1;
    }
    memset(info, 0, sizeof(*info));
    info->st_mode = regular ? S_IFREG : S_IFIFO;
    return 0;
}

static ssize_t fake_read(int descriptor, void *output, size_t length) {
    assert(descriptor == 0 && length > 0 && length <= 4096);
    ++reads;
    requested = length;
    if (read_interruptions) {
        --read_interruptions;
        errno = EINTR;
        return -1;
    }
    unsigned char *bytes = output;
    bytes[0] = 0x66;
    if (read_error) {
        errno = read_error;
        return -1;
    }
    return 1;
}

static int fake_close(int descriptor) {
    assert(descriptor == 0);
    ++closes;
    errno = close_error;
    return close_error ? -1 : 0;
}

static ssize_t fake_write(int descriptor, const void *input, size_t length) {
    assert(descriptor == 0 && input && length > 0 && length <= 4096);
    ++writes;
    requested = length;
    if (write_interruptions) {
        --write_interruptions;
        errno = EINTR;
        return -1;
    }
    if (write_error) {
        errno = write_error;
        return -1;
    }
    assert(write_count <= (ssize_t)length);
    return write_count;
}

static int fake_sync(int descriptor) {
    assert(descriptor == 0);
    ++syncs;
    if (sync_interruptions) {
        --sync_interruptions;
        errno = EINTR;
        return -1;
    }
    errno = sync_error;
    return sync_error ? -1 : 0;
}

static void test_output_failures(void) {
    reset();
    creating = true;
    FermiFile file = {0};
    open_error = EEXIST;
    assert(fermi_file_create_new(&file, (const unsigned char *)"input", 5) ==
           FERMI_FILE_OPEN_FAILED);
    assert(!file.open && opens == 1 && closes == 0);
    open_error = 0;
    inspect_error = EIO;
    assert(fermi_file_create_new(&file, (const unsigned char *)"input", 5) ==
           FERMI_FILE_INSPECT_FAILED);
    assert(!file.open && closes == 1);
    inspect_error = 0;
    assert(fermi_file_create_new(&file, (const unsigned char *)"input", 5) == FERMI_FILE_OK);
    unsigned char input[8192] = {0};
    size_t count = 99;
    assert(fermi_file_write(&file, NULL, 0, &count) == FERMI_FILE_OK);
    assert(count == 0 && writes == 0);
    write_interruptions = 2;
    assert(fermi_file_write(&file, input, sizeof(input), &count) == FERMI_FILE_OK);
    assert(count == 1 && writes == 3 && requested == 4096);
    write_count = 0;
    count = 99;
    assert(fermi_file_write(&file, input, 1, &count) == FERMI_FILE_WRITE_FAILED);
    assert(count == 99 && file.open);
    const int failures[] = {ENOSPC, EIO, EAGAIN, EBADF};
    for (size_t i = 0; i < sizeof(failures) / sizeof(failures[0]); ++i) {
        write_error = failures[i];
        assert(fermi_file_write(&file, input, 1, &count) == FERMI_FILE_WRITE_FAILED);
        assert(count == 99 && file.open);
    }
    assert(fermi_file_write(&file, NULL, 1, &count) == FERMI_FILE_INVALID_ARGUMENT);
    assert(fermi_file_write(&file, input, 1, NULL) == FERMI_FILE_INVALID_ARGUMENT);
    assert(fermi_file_write(NULL, input, 1, &count) == FERMI_FILE_INVALID_ARGUMENT);
    sync_interruptions = 2;
    assert(fermi_file_sync(&file) == FERMI_FILE_OK && syncs == 3);
    sync_error = EIO;
    assert(fermi_file_sync(&file) == FERMI_FILE_SYNC_FAILED && file.open);
    assert(fermi_file_sync(NULL) == FERMI_FILE_INVALID_ARGUMENT);
    fermi_file_drop(&file);
    assert(closes == 2 && !file.open);
    assert(fermi_file_write(&file, input, 1, &count) == FERMI_FILE_INVALID_STATE);
    assert(fermi_file_sync(&file) == FERMI_FILE_INVALID_STATE);
}

static FermiFile acquire(void) {
    FermiFile file = {0};
    assert(fermi_file_open_read(&file, (const unsigned char *)"input", 5) == FERMI_FILE_OK);
    return file; // Native test transfer; the returned slot is the only live owner.
}

static void test_failed_acquisition(void) {
    reset();
    FermiFile file = {0};
    open_error = EMFILE;
    assert(fermi_file_open_read(&file, (const unsigned char *)"input", 5) ==
           FERMI_FILE_OPEN_FAILED);
    assert(opens == 1 && inspections == 0 && closes == 0 && !file.open);
    reset();
    inspect_error = EIO;
    close_error = EINTR;
    assert(fermi_file_open_read(&file, (const unsigned char *)"input", 5) ==
           FERMI_FILE_INSPECT_FAILED);
    assert(errno == EIO && inspections == 1 && closes == 1 && !file.open);
    fermi_file_drop(&file);
    assert(closes == 1);
    reset();
    regular = false;
    assert(fermi_file_open_read(&file, (const unsigned char *)"input", 5) ==
           FERMI_FILE_NOT_REGULAR);
    assert(closes == 1 && !file.open);
    reset();
    open_interruptions = inspect_interruptions = 2;
    file = acquire();
    assert(file.open && file.descriptor == 0 && opens == 3 && inspections == 3);
    fermi_file_drop(&file);
    assert(closes == 1 && !file.open);
}

static void test_reads(void) {
    reset();
    FermiFile file = acquire();
    unsigned char output[8192];
    memset(output, 0xa5, sizeof(output));
    size_t count = 99;
    assert(fermi_file_read(&file, NULL, 0, &count) == FERMI_FILE_OK && count == 0 && reads == 0);
    read_interruptions = 2;
    assert(fermi_file_read(&file, output, sizeof(output), &count) == FERMI_FILE_OK);
    assert(count == 1 && reads == 3 && requested == 4096 && output[1] == 0xa5);
    assert(fermi_file_read(&file, output, 1, &count) == FERMI_FILE_OK && requested == 1);
    read_error = EIO;
    count = 99;
    assert(fermi_file_read(&file, output, sizeof(output), &count) == FERMI_FILE_READ_FAILED);
    assert(file.open && count == 99 && output[0] == 0x66);
    assert(fermi_file_read(&file, NULL, 1, &count) == FERMI_FILE_INVALID_ARGUMENT);
    assert(fermi_file_read(&file, output, sizeof(output), NULL) == FERMI_FILE_INVALID_ARGUMENT);
    fermi_file_drop(&file);
    assert(closes == 1);
}

static void test_close_consumption(void) {
    const int failures[] = {EINTR, EIO, EBADF};
    for (size_t index = 0; index < sizeof(failures) / sizeof(failures[0]); ++index) {
        reset();
        FermiFile file = acquire();
        close_error = failures[index];
        assert(fermi_file_close(&file) == FERMI_FILE_CLOSE_FAILED);
        assert(errno == failures[index] && !file.open && file.descriptor == 0 && closes == 1);
        assert(fermi_file_close(&file) == FERMI_FILE_INVALID_STATE);
        fermi_file_drop(&file);
        assert(closes == 1); // Retrying could close a newly reused descriptor.
    }
    reset();
    FermiFile file = acquire();
    close_error = EIO;
    errno = EDOM;
    fermi_file_drop(&file);
    assert(errno == EDOM && !file.open && closes == 1);
    fermi_file_drop(&file);
    fermi_file_drop(NULL);
    assert(errno == EDOM && closes == 1);
}

int main(void) {
    test_failed_acquisition();
    test_reads();
    test_close_consumption();
    test_output_failures();
    puts("PASS: file acquisition failures, interrupted reads and consuming close without retries");
    return 0;
}
