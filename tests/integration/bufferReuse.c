#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

static void *trackAllocation(size_t bytes);
static void trackRelease(void *pointer);
#define malloc trackAllocation
#define free trackRelease
#include <bufferReuse.c>
#undef malloc
#undef free

static size_t attempts, releases;
static void *live;
static bool failAllocation;

static void *trackAllocation(size_t bytes) {
    ++attempts;
    if (failAllocation) {
        return NULL;
    }
    assert(!live && bytes);
    live = malloc(bytes);
    assert(live);
    return live;
}

static void trackRelease(void *pointer) {
    assert(pointer && pointer == live);
    ++releases;
    live = NULL;
    free(pointer);
}

int main(void) {
    fermi_reuseRecords_result records = fermi_reuseRecords();
    assert(records.status == 0 && records.value);
    assert(attempts == 1 && releases == 1 && !live);
    fermi_reuseScalars_result scalars = fermi_reuseScalars();
    assert(scalars.status == 0 && scalars.value);
    assert(attempts == 2 && releases == 2 && !live);
    fermi_reuseEmpty_result empty = fermi_reuseEmpty();
    assert(empty.status == 0 && empty.value);
    assert(attempts == 2 && releases == 2 && !live);

    failAllocation = true;
    records = fermi_reuseRecords();
    assert(records.status == 1 && records.code == 0);
    scalars = fermi_reuseScalars();
    assert(scalars.status == 1 && scalars.code == 0);
    assert(attempts == 4 && releases == 2 && !live);
    puts(
        "PASS: buffer reuse performs no additional allocation; failed acquisition releases safely");
    return 0;
}
