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

static void *tracked_allocate(size_t size);
static void tracked_release(void *pointer);
static int tracked_open(const char *path, int flags, ...);
static int tracked_stat(int descriptor, struct stat *info);
static ssize_t tracked_read(int descriptor, void *output, size_t count);
static int tracked_close(int descriptor);
#define malloc tracked_allocate
#define free tracked_release
#define open tracked_open
#define fstat tracked_stat
#define read tracked_read
#define close tracked_close
#include <resource_structs.c>
#undef malloc
#undef free
#undef open
#undef fstat
#undef read
#undef close

enum { OWNER_LIMIT = 128, FILE_RELEASE = 1000 };
static void *owners[OWNER_LIMIT];
static unsigned attempts, releases, fail_at, live, events[OWNER_LIMIT], event_count;
static unsigned opens, closes;
static bool file_live_test, fail_open, fail_read;

static void *tracked_allocate(size_t size) {
    assert((size == 4 || size == 8) && attempts < OWNER_LIMIT);
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
    for (unsigned i = 0; i < attempts; ++i) {
        if (owners[i] == pointer) {
            assert(pointer && live && event_count < OWNER_LIMIT);
            owners[i] = NULL;
            --live;
            ++releases;
            events[event_count++] = i + 1;
            free(pointer);
            return;
        }
    }
    assert(!"unknown or previously released owner");
}

static int tracked_open(const char *path, int flags, ...) {
    assert(!file_live_test && !strcmp(path, "input"));
    assert(flags == (O_RDONLY | O_CLOEXEC | O_NONBLOCK));
    ++opens;
    if (fail_open) {
        errno = EACCES;
        return -1;
    }
    file_live_test = true;
    return 47;
}

static int tracked_stat(int descriptor, struct stat *info) {
    assert(file_live_test && descriptor == 47);
    *info = (struct stat){.st_mode = S_IFREG | 0600};
    return 0;
}

static ssize_t tracked_read(int descriptor, void *output, size_t count) {
    assert(file_live_test && descriptor == 47 && count == 1);
    if (fail_read) {
        errno = EIO;
        return -1;
    }
    *(unsigned char *)output = 23;
    return 1;
}

static int tracked_close(int descriptor) {
    assert(file_live_test && descriptor == 47 && event_count < OWNER_LIMIT);
    file_live_test = false;
    ++closes;
    events[event_count++] = FILE_RELEASE;
    errno = EIO;
    return -1;
}

static void reset(unsigned failure) {
    assert(!live && !file_live_test);
    for (unsigned i = 0; i < OWNER_LIMIT; ++i) {
        assert(!owners[i]);
    }
    attempts = releases = opens = closes = event_count = 0;
    fail_at = failure;
    fail_open = fail_read = false;
}

static void all_released(void) {
    assert(!live && releases == attempts - (fail_at && fail_at <= attempts));
    assert(!file_live_test);
}

int main(void) {
    for (unsigned failure = 0; failure <= 2; ++failure) {
        reset(failure);
        fermi_lifecycle_result result = fermi_lifecycle();
        assert(result.status == (failure ? 1u : 0u));
        if (!failure) {
            assert(result.value == 15 && events[0] == 2 && events[1] == 1);
        }
        all_released();
    }
    for (unsigned index = 0; index <= 1; ++index) {
        reset(0);
        fermi_partial_result result = fermi_partial(index);
        assert(result.status == (index ? 2u : 0u));
        assert(index ? result.code == 1 : result.value == 9);
        assert(attempts == 1 && releases == 1);
        all_released();
    }
    for (unsigned failure = 0; failure <= 3; ++failure) {
        reset(failure);
        fermi_growth_result result = fermi_growth();
        assert(result.status == (failure ? 1u : 0u));
        if (!failure) {
            assert(result.value == 19);
            assert(events[0] == 1 && events[1] == 2 && events[2] == 3);
        }
        all_released();
        reset(failure);
        fermi_replace_field_result field = fermi_replace_field();
        assert(field.status == (failure ? 1u : 0u));
        if (!failure) {
            assert(field.value == 15);
            assert(events[0] == 1 && events[1] == 2 && events[2] == 3);
        }
        all_released();
    }
    for (unsigned failure = 0; failure <= 4; ++failure) {
        reset(failure);
        fermi_replace_owner_result result = fermi_replace_owner();
        assert(result.status == (failure ? 1u : 0u));
        if (!failure) {
            assert(result.value == 15);
            assert(events[0] == 2 && events[1] == 1 && events[2] == 4 && events[3] == 3);
        }
        all_released();
    }
    reset(0);
    assert(fermi_temporary().value == 7);
    assert(attempts == 2 && releases == 2);
    all_released();
    for (unsigned index = 0; index <= 4; ++index) {
        reset(0);
        fermi_variant_result result = fermi_variant(index);
        assert(result.status == (index == 4 ? 2u : 0u));
        assert(index == 4 ? result.code == 1 : result.value == 0);
        assert(releases == 2);
        all_released();
    }
    for (unsigned index = 0; index <= 1; ++index) {
        reset(0);
        fermi_pending_result result = fermi_pending(index);
        assert(result.status == (index ? 2u : 0u));
        assert(index ? result.code == 1 : result.value == 16);
        assert(releases == 2);
        all_released();
    }
    for (unsigned stop = 0; stop <= 1; ++stop) {
        reset(0);
        fermi_repeated_result result = fermi_repeated(32, stop != 0);
        assert(result.status == 0 && result.value == (stop ? 4 : 128));
        assert(attempts == (stop ? 2 : 64));
        all_released();
    }
    reset(17);
    assert(fermi_repeated(32, false).status == 1);
    all_released();
    for (unsigned index = 0; index <= 4; ++index) {
        reset(0);
        fermi_mixed_result result = fermi_mixed(index);
        assert(result.status == (index == 4 ? 2u : 0u));
        assert(index == 4 ? result.code == 1 : result.value == (index ? 1 : 24));
        assert(opens == 1 && closes == 1 && events[0] == 1 && events[1] == FILE_RELEASE);
        all_released();
    }
    reset(0);
    fail_open = true;
    assert(fermi_mixed(0).status == 1 && closes == 0 && releases == 1);
    all_released();
    reset(0);
    fail_read = true;
    fermi_mixed_result failed_read = fermi_mixed(0);
    assert(failed_read.status == 1 && failed_read.code == 7 && closes == 1);
    all_released();
    for (unsigned failure = 0; failure <= 1; ++failure) {
        reset(failure);
        fermi_acquire_result result = fermi_acquire();
        assert(result.status == (failure ? 1u : 0u));
        assert(opens == 1 && closes == 1);
        assert(events[event_count - 1] == FILE_RELEASE);
        all_released();
    }
    reset(0);
    fermi_copied_header_result header = fermi_copied_header();
    assert(header.status == 0 && header.value == 7 && releases == 2);
    all_released();
    reset(0);
    fermi_wrapped_result wrapped = fermi_wrapped();
    assert(wrapped.status == 0 && wrapped.value == 11 && releases == 2);
    all_released();
    reset(0);
    assert(fermi_empty().status == 0 && !attempts && !opens);
    puts("PASS: struct ownership, field access, partial construction, cleanup order and faults");
    return 0;
}
