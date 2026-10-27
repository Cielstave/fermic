#define _POSIX_C_SOURCE 200809L
#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

static int tracked_open(const char *path, int flags, ...);
static int tracked_stat(int descriptor, struct stat *info);
static ssize_t tracked_write(int descriptor, const void *input, size_t count);
static int tracked_sync(int descriptor);
static int tracked_close(int descriptor);
#define open(...) tracked_open(__VA_ARGS__)
#define fstat(...) tracked_stat(__VA_ARGS__)
#define write(...) tracked_write(__VA_ARGS__)
#define fsync(...) tracked_sync(__VA_ARGS__)
#define close(...) tracked_close(__VA_ARGS__)
#include <file_output.c>
#undef open
#undef fstat
#undef write
#undef fsync
#undef close

enum { TEST_OK = 0, TEST_DOMAIN_ERROR = 1, TEST_FAULT = 2 };
static bool live;
static unsigned opens, writes, syncs, closes, fail_write, interruptions;
static int open_error, sync_error, close_error;
static bool zero_progress;
static unsigned char stored[16384];
static size_t length;
static const uint8_t path[] = "output";

static void reset(void) {
    assert(!live);
    opens = writes = syncs = closes = fail_write = interruptions = 0;
    open_error = sync_error = close_error = 0;
    zero_progress = false;
    length = 0;
}
static int tracked_open(const char *name, int flags, ...) {
    assert(!strcmp(name, "output"));
    assert(flags == (O_WRONLY | O_CREAT | O_EXCL | O_CLOEXEC | O_NONBLOCK));
    va_list args;
    va_start(args, flags);
    assert(va_arg(args, int) == 0600);
    va_end(args);
    ++opens;
    if (open_error) {
        errno = open_error;
        return -1;
    }
    assert(!live);
    live = true;
    return 47;
}
static int tracked_stat(int descriptor, struct stat *info) {
    assert(descriptor == 47 && live);
    memset(info, 0, sizeof(*info));
    info->st_mode = S_IFREG;
    return 0;
}
static ssize_t tracked_write(int descriptor, const void *input, size_t count) {
    assert(descriptor == 47 && live && count > 0 && count <= 4096);
    ++writes;
    if (interruptions) {
        --interruptions;
        errno = EINTR;
        return -1;
    }
    if (writes == fail_write) {
        errno = ENOSPC;
        return -1;
    }
    if (zero_progress) {
        return 0;
    }
    size_t accepted = count < 997 ? count : 997;
    assert(length + accepted <= sizeof(stored));
    memcpy(stored + length, input, accepted);
    length += accepted;
    return (ssize_t)accepted;
}
static int tracked_sync(int descriptor) {
    assert(descriptor == 47 && live);
    ++syncs;
    errno = sync_error;
    return sync_error ? -1 : 0;
}
static int tracked_close(int descriptor) {
    assert(descriptor == 47 && live);
    live = false;
    ++closes;
    errno = close_error;
    return close_error ? -1 : 0;
}
int main(void) {
    uint8_t input[8193];
    for (size_t i = 0; i < sizeof(input); ++i) {
        input[i] = (uint8_t)i;
    }
    reset();
    interruptions = 2;
    fermi_save_result result = fermi_save(path, sizeof(path) - 1, input, sizeof(input));
    assert(result.status == TEST_OK && length == sizeof(input));
    assert(memcmp(stored, input, length) == 0 && syncs == 1 && closes == 1 && !live);
    unsigned complete_writes = writes - 2;
    for (unsigned failure = 1; failure <= complete_writes; ++failure) {
        reset();
        fail_write = failure;
        close_error = EIO;
        result = fermi_save(path, sizeof(path) - 1, input, sizeof(input));
        assert(result.status == TEST_DOMAIN_ERROR && result.code == 9);
        assert(writes == failure && syncs == 0 && closes == 1 && !live);
        assert(length < sizeof(input) && memcmp(stored, input, length) == 0);
    }
    reset();
    zero_progress = true;
    result = fermi_save(path, sizeof(path) - 1, input, 1);
    assert(result.status == TEST_DOMAIN_ERROR && result.code == 9 && writes == 1 && closes == 1 &&
           !live);
    reset();
    sync_error = EIO;
    close_error = EINTR;
    result = fermi_save(path, sizeof(path) - 1, input, sizeof(input));
    assert(result.status == TEST_DOMAIN_ERROR && result.code == 10 && syncs == 1 && closes == 1 &&
           !live);
    reset();
    close_error = EIO;
    result = fermi_save(path, sizeof(path) - 1, input, 0);
    assert(result.status == TEST_DOMAIN_ERROR && result.code == 8 && writes == 0 && syncs == 1 &&
           closes == 1 && !live);
    reset();
    open_error = EEXIST;
    result = fermi_save(path, sizeof(path) - 1, input, 1);
    assert(result.status == TEST_DOMAIN_ERROR && result.code == 4 && writes == 0 && closes == 0 &&
           !live);
    reset();
    fermi_save_owned_result owned = fermi_save_owned(path, sizeof(path) - 1, input, sizeof(input));
    assert(owned.status == TEST_OK && closes == 1 && !live);
    reset();
    sync_error = EIO;
    owned = fermi_save_owned(path, sizeof(path) - 1, input, sizeof(input));
    assert(owned.status == TEST_DOMAIN_ERROR && owned.code == 10 && closes == 1 && !live);
    reset();
    fermi_save_fault_result fault = fermi_save_fault(path, sizeof(path) - 1, input, 1, 1);
    assert(fault.status == TEST_FAULT && writes == 0 && closes == 1 && !live);
    puts("PASS: file output, short writes, failure prefixes, sync, owner fields and exact cleanup");
    return 0;
}
