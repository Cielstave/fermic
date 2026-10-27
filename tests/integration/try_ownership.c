#define _POSIX_C_SOURCE 200809L
#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

static int tracked_open(const char *path, int flags, ...);
static int tracked_stat(int descriptor, struct stat *info);
static ssize_t tracked_read(int descriptor, void *bytes, size_t count);
static int tracked_close(int descriptor);
static void *tracked_allocate(size_t bytes);
static void tracked_release(void *pointer);
#define open tracked_open
#define fstat tracked_stat
#define read tracked_read
#define close tracked_close
#define malloc tracked_allocate
#define free tracked_release
#include "try_owners.c"
#undef open
#undef fstat
#undef read
#undef close
#undef malloc
#undef free

enum { OWNER_LIMIT = 128 };
static void *owners[OWNER_LIMIT];
static unsigned attempts, releases, fail_at, live;

static void *tracked_allocate(size_t bytes) {
    assert(bytes == 4 && attempts < OWNER_LIMIT);
    unsigned index = attempts++;
    if (attempts == fail_at) {
        return NULL;
    }
    void *pointer = malloc(bytes);
    assert(pointer);
    owners[index] = pointer;
    ++live;
    return pointer;
}

static void tracked_release(void *pointer) {
    for (unsigned index = 0; index < attempts; ++index) {
        if (owners[index] == pointer) {
            assert(pointer && live);
            owners[index] = NULL;
            --live;
            ++releases;
            free(pointer);
            return;
        }
    }
    assert(!"unknown or previously released owner");
}

static void reset(unsigned failure) {
    assert(live == 0);
    for (unsigned index = 0; index < OWNER_LIMIT; ++index) {
        assert(!owners[index]);
    }
    attempts = releases = 0;
    fail_at = failure;
}

static bool tracked_live_file, open_failure, read_failure, close_failure;
static unsigned opens, reads, closes;

static int tracked_open(const char *path, int flags, ...) {
    assert(!strcmp(path, "input") && flags == (O_RDONLY | O_CLOEXEC | O_NONBLOCK));
    assert(!tracked_live_file);
    ++opens;
    if (open_failure) {
        errno = EMFILE;
        return -1;
    }
    tracked_live_file = true;
    return 47;
}

static int tracked_stat(int descriptor, struct stat *info) {
    assert(descriptor == 47 && tracked_live_file);
    *info = (struct stat){.st_mode = S_IFREG | 0600};
    return 0;
}

static ssize_t tracked_read(int descriptor, void *bytes, size_t count) {
    assert(descriptor == 47 && tracked_live_file && count);
    ++reads;
    if (read_failure) {
        errno = EIO;
        return -1;
    }
    ((uint8_t *)bytes)[0] = 71;
    return 1;
}

static int tracked_close(int descriptor) {
    assert(descriptor == 47 && tracked_live_file);
    ++closes;
    tracked_live_file = false;
    if (close_failure) {
        errno = EIO;
        return -1;
    }
    return 0;
}

static void test_files(void) {
    for (unsigned scenario = 0; scenario < 5; ++scenario) {
        opens = reads = closes = 0;
        open_failure = scenario == 1;
        read_failure = scenario == 2;
        close_failure = scenario == 2 || scenario == 3;
        uint8_t output = 0;
        fermi_inspect_input_result result = fermi_inspect_input(scenario == 4, &output, 1);
        assert(!tracked_live_file && opens == 1);
        assert(reads == (scenario != 1) && closes == (scenario != 1));
        assert(result.status == (scenario == 0 ? 0u : scenario == 4 ? 2u : 1u));
        if (scenario == 0) {
            assert(result.value == 1 && output == 71);
        } else {
            const uint32_t errors[] = {0, 4, 7, 8, 1};
            assert(result.code == errors[scenario]);
        }
    }
}

int main(void) {
    for (unsigned failure = 0; failure <= 2; ++failure) {
        for (unsigned accepted = 0; accepted < 2; ++accepted) {
            reset(failure);
            uint8_t marker = 0;
            fermi_chain_result result = fermi_chain(accepted != 0, &marker, 1);
            bool success = accepted && !failure;
            assert(result.status == (success ? 0u : 1u));
            assert(marker == (success ? 1 : 0));
            if (success) {
                assert(result.value == 142);
            } else {
                assert(result.code == (failure ? 0u : 2u));
            }
            assert(live == 0 && releases == attempts - (failure != 0));
        }
    }
    for (unsigned accepted = 0; accepted < 2; ++accepted) {
        reset(0);
        uint8_t marker = 0;
        fermi_temporary_result result = fermi_temporary(accepted != 0, &marker, 1);
        assert(result.status == (accepted ? 0u : 1u));
        assert(marker == accepted && attempts == 1 && releases == 1 && live == 0);
        if (accepted) {
            assert(result.value == 72);
        }
        reset(0);
        fermi_replaced_result replaced = fermi_replaced(accepted != 0);
        assert(replaced.status == (accepted ? 0u : 1u));
        assert(attempts == 2 && releases == 2 && live == 0);
        if (accepted) {
            assert(replaced.value == 71);
        }
    }
    for (unsigned index = 0; index <= 4; ++index) {
        reset(0);
        fermi_bounds_result result = fermi_bounds(index);
        assert(result.status == (index == 4 ? 2u : 0u));
        if (index == 4) {
            assert(result.code == 1);
        } else {
            assert(result.value == (index == 0 ? 71 : 0));
        }
        assert(attempts == 1 && releases == 1 && live == 0);
    }
    reset(0);
    fermi_repeated_result repeated = fermi_repeated(64, true);
    assert(repeated.status == 0 && repeated.value == 64);
    assert(attempts == 64 && releases == 64 && live == 0);
    reset(17);
    repeated = fermi_repeated(64, true);
    assert(repeated.status == 1 && repeated.code == 0);
    assert(attempts == 17 && releases == 16 && live == 0);
    reset(0);
    repeated = fermi_repeated(64, false);
    assert(repeated.status == 1 && repeated.code == 2);
    assert(attempts == 1 && releases == 1 && live == 0);
    test_files();
    puts("PASS: try owner moves, cleanup, allocation failures and file error preservation");
    return 0;
}
