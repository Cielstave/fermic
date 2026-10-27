#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

static void *tracked_allocate(size_t size);
static void tracked_release(void *pointer);
#define malloc tracked_allocate
#define free tracked_release
#include <array_repeat_owners.c>
#undef malloc
#undef free

static unsigned attempts, releases;
static bool fail_allocation;
static void *live;

static void *tracked_allocate(size_t size) {
    assert(size == 4 && live == NULL);
    ++attempts;
    if (fail_allocation) {
        return NULL;
    }
    live = malloc(size);
    assert(live != NULL);
    return live;
}

static void tracked_release(void *pointer) {
    assert(pointer != NULL && pointer == live);
    live = NULL;
    ++releases;
    free(pointer);
}

int main(void) {
    fermi_repeated_owner_result owner = fermi_repeated_owner();
    assert(owner.status == 0 && owner.value == 14);
    assert(attempts == 1 && releases == 1 && live == NULL);

    fermi_repeated_fault_result normal = fermi_repeated_fault(0);
    assert(normal.status == 0 && normal.value == 11);
    assert(attempts == 2 && releases == 2 && live == NULL);

    fermi_repeated_fault_result fault = fermi_repeated_fault(1);
    assert(fault.status == 2 && fault.code == 1);
    assert(attempts == 3 && releases == 3 && live == NULL);

    fermi_repeated_error_result error = fermi_repeated_error();
    assert(error.status == 1 && error.code == 3);
    assert(attempts == 4 && releases == 4 && live == NULL);

    fail_allocation = true;
    owner = fermi_repeated_owner();
    assert(owner.status == 1 && owner.code == 0);
    normal = fermi_repeated_fault(0);
    assert(normal.status == 1 && normal.code == 0);
    error = fermi_repeated_error();
    assert(error.status == 1 && error.code == 0);
    assert(attempts == 7 && releases == 4 && live == NULL);
    puts("PASS: repeated initializer ownership, single evaluation, cleanup and allocation failure");
    return 0;
}
