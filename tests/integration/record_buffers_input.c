#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

static void *tracked_allocate(size_t size);
static void tracked_release(void *pointer);
#define malloc tracked_allocate
#define free tracked_release
#include <record_buffers.c>
#undef malloc
#undef free

static void *live[16];
static size_t attempts, releases, failures, fail_at;

static void *tracked_allocate(size_t size) {
    ++attempts;
    if (attempts == fail_at) {
        ++failures;
        return NULL;
    }
    void *pointer = malloc(size);
    assert(pointer != NULL);
    for (size_t slot = 0; slot < 16; slot++) {
        if (!live[slot]) {
            live[slot] = pointer;
            return pointer;
        }
    }
    abort();
}

static void tracked_release(void *pointer) {
    for (size_t slot = 0; slot < 16; slot++) {
        if (live[slot] == pointer) {
            assert(pointer != NULL);
            live[slot] = NULL;
            ++releases;
            free(pointer);
            return;
        }
    }
    abort();
}

static void released(void) {
    for (size_t slot = 0; slot < 16; slot++) {
        assert(!live[slot]);
    }
    assert(attempts == releases + failures);
}

int main(void) {
    struct String {
        const uint8_t *data;
        size_t length;
    };
    struct Row {
        uint8_t tag;
        size_t id;
        struct String name;
    };
    fermi_row_width_result width = fermi_row_width();
    assert(width.status == 0 && width.value == sizeof(struct Row) && !attempts);
    fermi_grow_records_result grown = fermi_grow_records(5 * width.value);
    assert(grown.status == 0 && grown.value == 5 && attempts == 2);
    released();
    grown = fermi_grow_records(5 * width.value - 1);
    assert(grown.status == 1 && grown.code == 2);
    released();
    grown = fermi_grow_records(0);
    assert(grown.status == 1 && grown.code == 2);
    released();
    fail_at = attempts + 2;
    grown = fermi_grow_records(5 * width.value);
    assert(grown.status == 1 && grown.code == 0);
    released();
    fail_at = attempts + 1;
    grown = fermi_grow_records(5 * width.value);
    assert(grown.status == 1 && grown.code == 0);
    released();
    fail_at = 0;
    size_t before = attempts;
    fermi_oversized_records_result overflow = fermi_oversized_records(SIZE_MAX);
    assert(overflow.status == 1 && overflow.code == 1 && attempts == before);
    overflow = fermi_oversized_records((size_t)PTRDIFF_MAX / width.value + 1);
    assert(overflow.status == 1 && overflow.code == 1 && attempts == before);
    released();
    fermi_cleanup_fault_result fault = fermi_cleanup_fault(SIZE_MAX);
    assert(fault.status == 2 && fault.code == 1);
    released();
    fault = fermi_cleanup_fault(0);
    assert(fault.status == 0 && fault.value == 17);
    released();
    puts("PASS: record layout, byte limits, failed growth preservation, overflow and cleanup");
    return 0;
}
