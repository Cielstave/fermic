#include <assert.h>
#include <errno.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static ssize_t printing_write(int, const void *, size_t);
static void *printing_allocate(size_t);
static void printing_release(void *);
#define write printing_write
#define malloc printing_allocate
#define free printing_release
#include "printing.c"
#undef write
#undef malloc
#undef free

static uint8_t expected[9001];
static size_t received, fail_at;
static unsigned calls, allocations, releases;
static bool interrupted, zero_progress;

static ssize_t printing_write(int descriptor, const void *bytes, size_t length) {
    assert(descriptor == STDOUT_FILENO && length && length <= 4096);
    ++calls;
    if (!interrupted) {
        interrupted = true;
        errno = EINTR;
        return -1;
    }
    if (received >= fail_at) {
        errno = EIO;
        return zero_progress ? 0 : -1;
    }
    size_t count = length > 13 ? 13 : length;
    assert(received + count <= sizeof(expected));
    assert(!memcmp(bytes, expected + received, count));
    received += count;
    return (ssize_t)count;
}

static void *printing_allocate(size_t size) {
    ++allocations;
    return malloc(size);
}

static void printing_release(void *pointer) {
    assert(pointer);
    ++releases;
    free(pointer);
}

static void reset(size_t failure) {
    assert(allocations == releases);
    received = calls = allocations = releases = 0;
    fail_at = failure;
    interrupted = zero_progress = false;
}

int main(void) {
    for (size_t i = 0; i < sizeof(expected) - 1; ++i) {
        expected[i] = (uint8_t)i;
    }
    expected[9000] = '\n';
    uint8_t marker;
    for (unsigned line = 0; line < 2; ++line) {
        reset(SIZE_MAX);
        marker = 0;
        fermi_output_result output = fermi_output(expected, 9000, line != 0, &marker, 1);
        assert(output.status == 0u && marker == 1);
        assert(received == 9000 + line && allocations == 1 && releases == 1);
        assert(calls > 2);
        for (unsigned zero = 0; zero < 2; ++zero) {
            reset(13);
            zero_progress = zero != 0;
            marker = 0;
            output = fermi_output(expected, 9000, line != 0, &marker, 1);
            assert(output.status == 2u && output.code == 5u);
            assert(marker == 0 && received == 13 && releases == 1);
        }
    }
    reset(9000);
    marker = 0;
    fermi_output_result newline = fermi_output(expected, 9000, true, &marker, 1);
    assert(newline.status == 2u && newline.code == 5u);
    assert(marker == 0 && received == 9000 && releases == 1);
    reset(SIZE_MAX);
    marker = 0;
    assert(fermi_output(NULL, 0, false, &marker, 1).status == 0u);
    assert(marker == 1 && calls == 0 && releases == 1);
    reset(SIZE_MAX);
    expected[0] = '\n';
    assert(fermi_output(NULL, 0, true, &marker, 1).status == 0u);
    assert(received == 1 && releases == 1);
    reset(SIZE_MAX);
    memcpy(expected, "AB\n", 3);
    assert(fermi_literal().status == 0u && received == 3);
    assert(allocations == 0);
    puts("PASS: concise printers, partial writes, EINTR, I/O faults and resource cleanup");
    return 0;
}
