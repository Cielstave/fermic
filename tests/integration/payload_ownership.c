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

static void *tracked_allocate(size_t size);
static void tracked_release(void *pointer);
static int tracked_open(const char *path, int flags, ...);
static int tracked_stat(int descriptor, struct stat *info);
static int tracked_close(int descriptor);
#define malloc tracked_allocate
#define free tracked_release
#define open tracked_open
#define fstat tracked_stat
#define close tracked_close
#include "payload_owners.c"
#undef malloc
#undef free
#undef open
#undef fstat
#undef close

enum { OWNER_LIMIT = 128 };
static void *owners[OWNER_LIMIT];
static unsigned attempts, releases, fail_at, live;
static bool tracked_file;
static unsigned opens, closes;

static void *tracked_allocate(size_t size) {
    assert(size == 4 && attempts < OWNER_LIMIT);
    unsigned index = attempts++;
    if (attempts == fail_at) {
        return NULL;
    }
    void *pointer = malloc(size);
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

static int tracked_open(const char *path, int flags, ...) {
    assert(!tracked_file && !strcmp(path, "input"));
    assert(flags == (O_RDONLY | O_CLOEXEC | O_NONBLOCK));
    tracked_file = true;
    ++opens;
    return 47;
}

static int tracked_stat(int descriptor, struct stat *info) {
    assert(tracked_file && descriptor == 47);
    *info = (struct stat){.st_mode = S_IFREG | 0600};
    return 0;
}

static int tracked_close(int descriptor) {
    assert(tracked_file && descriptor == 47);
    tracked_file = false;
    ++closes;
    errno = EIO;
    return -1;
}

static void reset(unsigned failure) {
    assert(live == 0 && !tracked_file && opens == closes);
    for (unsigned index = 0; index < OWNER_LIMIT; ++index) {
        assert(!owners[index]);
    }
    attempts = releases = opens = closes = 0;
    fail_at = failure;
}

int main(void) {
    for (unsigned failure = 0; failure < 2; ++failure) {
        for (unsigned accepted = 0; accepted < 2; ++accepted) {
            reset(failure);
            fermi_basic_result result = fermi_basic(accepted != 0);
            assert(result.status == (accepted && !failure ? 0u : 1u));
            if (result.status == 0) {
                assert(result.value == 73);
            } else {
                assert(result.code == (failure ? 0u : 2u));
            }
            assert(attempts == 1 && releases == !failure && live == 0);
        }
    }
    reset(0);
    assert(fermi_ignored().status == 0 && releases == 1 && live == 0);
    for (unsigned index = 0; index <= 4; ++index) {
        reset(0);
        fermi_nested_result result = fermi_nested(index);
        assert(result.status == (index == 4 ? 2u : 0u));
        if (index == 4) {
            assert(result.code == 1);
        } else {
            assert(result.value == (index == 0 ? 73 : 0));
        }
        assert(attempts == 1 && releases == 1 && live == 0);
    }
    for (unsigned accepted = 0; accepted < 2; ++accepted) {
        for (unsigned failure = 0; failure < 3; ++failure) {
            reset(failure);
            fermi_replaced_result result = fermi_replaced(accepted != 0);
            bool failed = failure == 1 || (accepted && failure == 2);
            assert(result.status == (failed ? 1u : 0u));
            if (!failed) {
                assert(result.value == (accepted ? 73 : 9));
            }
            assert(live == 0 && releases == attempts - failed);
        }
    }
    for (unsigned stopping = 0; stopping < 2; ++stopping) {
        reset(0);
        fermi_repeated_result result = fermi_repeated(64, stopping != 0);
        assert(result.status == 0 && result.value == (stopping ? 1 : 64));
        assert(attempts == result.value && releases == attempts && live == 0);
    }
    reset(17);
    fermi_repeated_result failed = fermi_repeated(64, false);
    assert(failed.status == 1 && failed.code == 0);
    assert(attempts == 17 && releases == 16 && live == 0);
    for (unsigned index = 0; index < 2; ++index) {
        reset(0);
        fermi_pending_result result = fermi_pending(index);
        assert(result.status == (index ? 2u : 0u));
        if (index) {
            assert(result.code == 1);
        } else {
            assert(result.value == 74);
        }
        assert(attempts == 1 && releases == 1 && live == 0);
    }
    for (unsigned inspect = 0; inspect < 2; ++inspect) {
        reset(0);
        fermi_file_owner_result result = fermi_file_owner(inspect != 0);
        assert(result.status == 0 && result.value == (inspect ? 7 : 0));
        assert(opens == 1 && closes == 1 && !tracked_file);
    }
    reset(0);
    assert(fermi_empty().status == 0 && !attempts && !opens);
    puts("PASS: payload ownership, nested cleanup, faults, replacement and loop exits");
    return 0;
}
