#define _POSIX_C_SOURCE 200809L
#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

static int tracked_open(const char *path, int flags, ...);
static int tracked_stat(int descriptor, struct stat *info);
static ssize_t tracked_read(int descriptor, void *output, size_t count);
static int tracked_close(int descriptor);
static void *tracked_allocate(size_t size);
static void tracked_release(void *pointer);
#define open(...) tracked_open(__VA_ARGS__)
#define fstat(...) tracked_stat(__VA_ARGS__)
#define read(...) tracked_read(__VA_ARGS__)
#define close(...) tracked_close(__VA_ARGS__)
#define malloc(...) tracked_allocate(__VA_ARGS__)
#define free(...) tracked_release(__VA_ARGS__)
#include "files.c"
#undef open
#undef fstat
#undef read
#undef close
#undef malloc
#undef free

enum { HANDLES = 32 };
static bool live[HANDLES];
static unsigned opens, closes, reads, fail_open;
static unsigned allocations, releases;
static int close_error, read_error;
static bool fail_allocate;
static unsigned close_order[64];
static const uint8_t path[] = "input";

static void reset(void) {
    for (unsigned index = 0; index < HANDLES; ++index) {
        assert(!live[index]);
    }
    assert(allocations == releases);
    opens = closes = reads = fail_open = allocations = releases = 0;
    close_error = read_error = 0;
    fail_allocate = false;
}

static int tracked_open(const char *name, int flags, ...) {
    assert(!strcmp(name, "input") && flags == (O_RDONLY | O_CLOEXEC | O_NONBLOCK));
    if (++opens == fail_open) {
        errno = EMFILE;
        return -1;
    }
    for (unsigned index = 0; index < HANDLES; ++index) {
        if (!live[index]) {
            live[index] = true;
            return (int)index;
        }
    }
    assert(!"descriptor test capacity exceeded");
    return -1;
}

static int tracked_stat(int descriptor, struct stat *info) {
    assert(descriptor >= 0 && descriptor < HANDLES && live[descriptor]);
    memset(info, 0, sizeof(*info));
    info->st_mode = S_IFREG;
    return 0;
}

static ssize_t tracked_read(int descriptor, void *output, size_t count) {
    assert(descriptor >= 0 && descriptor < HANDLES && live[descriptor]);
    assert(count > 0 && count <= 4096);
    ++reads;
    if (read_error) {
        errno = read_error;
        return -1;
    }
    size_t length = count < 3 ? count : 3;
    memcpy(output, "Fe!", length);
    return (ssize_t)length;
}

static int tracked_close(int descriptor) {
    assert(descriptor >= 0 && descriptor < HANDLES && live[descriptor]);
    assert(closes < sizeof(close_order) / sizeof(close_order[0]));
    live[descriptor] = false;
    close_order[closes++] = (unsigned)descriptor;
    errno = close_error;
    return close_error ? -1 : 0;
}

static void *tracked_allocate(size_t size) {
    if (fail_allocate) {
        return NULL;
    }
    void *pointer = malloc(size);
    assert(pointer);
    ++allocations;
    return pointer;
}

static void tracked_release(void *pointer) {
    assert(pointer && releases < allocations);
    ++releases;
    free(pointer);
}

static void test_transfers(void) {
    reset();
    uint8_t output[8] = {0};
    fermi_first_result first = fermi_first(path, sizeof(path) - 1, output, sizeof(output));
    assert(first.status == 0 && first.value == 3 && !memcmp(output, "Fe!", 3));
    assert(opens == 1 && reads == 1 && closes == 1);
    reset();
    fermi_return_transfer_result returned = fermi_return_transfer(path, sizeof(path) - 1, 1);
    assert(returned.status == 0 && opens == 1 && closes == 1);
    reset();
    returned = fermi_return_transfer(path, sizeof(path) - 1, 0);
    assert(returned.status == 2 && returned.code == 3 && opens == 1 && closes == 1);
    reset();
    fermi_replace_result replacement = fermi_replace(path, sizeof(path) - 1);
    assert(replacement.status == 0 && replacement.value == 2 && opens == 2 && closes == 2);
    assert(close_order[0] == 0 && close_order[1] == 1);
    for (unsigned failure = 1; failure <= 2; ++failure) {
        reset();
        fail_open = failure;
        replacement = fermi_replace(path, sizeof(path) - 1);
        assert(replacement.status == 1 && replacement.code == 4);
        assert(opens == failure && closes == failure - 1);
    }
    for (unsigned failure = 0; failure <= 16; ++failure) {
        reset();
        fail_open = failure;
        fermi_scopes_result scoped = fermi_scopes(path, sizeof(path) - 1);
        assert(scoped.status == (failure ? 1u : 0u));
        assert(opens == (failure ? failure : 16));
        assert(closes == opens - (failure ? 1 : 0));
        if (!failure) {
            for (unsigned index = 0; index < closes; index += 2) {
                assert(close_order[index] == 1 && close_order[index + 1] == 0);
            }
        }
    }
}

static void test_failures(void) {
    uint8_t output[1] = {0};
    reset();
    fermi_local_fault_result local = fermi_local_fault(path, sizeof(path) - 1, output, 1, 1);
    assert(local.status == 2 && local.code == 1 && opens == 1 && closes == 1);
    reset();
    fermi_late_fault_result late = fermi_late_fault(path, sizeof(path) - 1, output, 1, 1);
    assert(late.status == 2 && late.code == 1 && opens == 1 && closes == 1);
    reset();
    late = fermi_late_fault(path, sizeof(path) - 1, output, 1, 0);
    assert(late.status == 0 && opens == 1 && closes == 1);
    for (unsigned failure = 0; failure < 2; ++failure) {
        reset();
        fail_open = failure;
        fermi_pending_result_result pending =
            fermi_pending_result(path, sizeof(path) - 1, output, 1, 1);
        assert(pending.status == 2 && pending.code == 1 && opens == 1);
        assert(closes == (failure ? 0u : 1u));
    }
    reset();
    read_error = EIO;
    fermi_first_result first = fermi_first(path, sizeof(path) - 1, output, 1);
    assert(first.status == 1 && first.code == 7 && closes == 1);
    const int errors[] = {0, EINTR, EIO};
    for (unsigned index = 0; index < sizeof(errors) / sizeof(errors[0]); ++index) {
        reset();
        close_error = errors[index];
        fermi_close_file_result closed = fermi_close_file(path, sizeof(path) - 1);
        assert(closed.status == (close_error ? 1u : 0u));
        assert(!close_error || closed.code == 8);
        assert(opens == 1 && closes == 1);
    }
    reset();
    close_error = EIO;
    local = fermi_local_fault(path, sizeof(path) - 1, output, 1, 1);
    assert(local.status == 2 && local.code == 1 && closes == 1);
    for (unsigned failure = 0; failure < 3; ++failure) {
        reset();
        fail_allocate = failure == 1;
        fail_open = failure == 2 ? 1 : 0;
        fermi_mixed_result mixed = fermi_mixed(path, sizeof(path) - 1, false);
        assert(mixed.status == 0 && mixed.value == (failure == 1 ? 99u : failure == 2 ? 98u : 1u));
        assert(allocations == releases);
        assert(opens == (failure == 1 ? 0u : 1u));
        assert(closes == (failure ? 0u : 1u));
    }
    reset();
    fermi_mixed_result mixed = fermi_mixed(path, sizeof(path) - 1, true);
    assert(mixed.status == 2 && mixed.code == 1 && closes == 1 && releases == 1);
    reset();
}

static void test_loop_jumps(void) {
    for (size_t mode = 0; mode < 4; ++mode) {
        reset();
        fermi_jump_cleanup_result result = fermi_jump_cleanup(path, sizeof(path) - 1, mode);
        unsigned iterations = mode == 1 || mode == 2 ? 1 : 3;
        assert(result.status == (mode == 2 ? 2u : 0u));
        assert(mode == 2 ? result.code == 1 : result.value == iterations);
        assert(opens == 1 + 2 * iterations && closes == opens);
        assert(allocations == 1 + iterations && releases == allocations);
        for (unsigned i = 0; i < iterations; ++i) {
            assert(close_order[2 * i] == 2 && close_order[2 * i + 1] == 1);
        }
        assert(close_order[closes - 1] == 0);
    }
    for (unsigned failure = 1; failure <= 7; ++failure) {
        reset();
        fail_open = failure;
        fermi_jump_cleanup_result result = fermi_jump_cleanup(path, sizeof(path) - 1, 0);
        assert(result.status == 0 && closes + 1 == opens);
        assert(allocations == releases);
    }
    reset();
    fail_allocate = true;
    assert(fermi_jump_cleanup(path, sizeof(path) - 1, 0).value == 88);
    assert(opens == 1 && closes == 1 && allocations == 0);
    reset();
    close_error = EINTR;
    assert(fermi_jump_cleanup(path, sizeof(path) - 1, 0).value == 3);
    assert(opens == 7 && closes == 7 && releases == 4);
    for (unsigned skip = 0; skip < 2; ++skip) {
        reset();
        fermi_init_cleanup_result result = fermi_init_cleanup(skip != 0);
        assert(result.status == 0 && result.value == (skip ? 8u : 0u));
        assert(allocations == 1 && releases == 1);
    }
    reset();
}

int main(void) {
    test_transfers();
    test_failures();
    test_loop_jumps();
    puts("PASS: file moves, reborrows, replacement, close errors and mixed-resource cleanup");
    return 0;
}
