#define _POSIX_C_SOURCE 200809L
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <time.h>

static int test_clock(clockid_t clock, struct timespec *reading);
#define clock_gettime test_clock
#include "time.c"
#undef clock_gettime

static struct timespec sample;
static int failure;
static unsigned calls;
static int test_clock(clockid_t clock, struct timespec *reading) {
    assert(clock == CLOCK_MONOTONIC);
    ++calls;
    *reading = sample;
    return failure;
}

static void test_reading(void) {
    sample = (struct timespec){0, 0};
    fermi_now_result result = fermi_now();
    assert(result.status == 0 && result.value == 0 && calls == 1);
    sample = (struct timespec){123, 456};
    result = fermi_now();
    assert(result.status == 0 && result.value == UINT64_C(123000000456));
    failure = -1;
    result = fermi_now();
    assert(result.status == 1 && result.code == 0);
    failure = 0;
    const struct timespec invalid[] = {{-1, 0}, {0, -1}, {0, 1000000000}};
    for (size_t i = 0; i < sizeof(invalid) / sizeof(invalid[0]); ++i) {
        sample = invalid[i];
        result = fermi_now();
        assert(result.status == 1 && result.code == 1);
    }
    sample = (struct timespec){(time_t)(SIZE_MAX / 1000000000), (long)(SIZE_MAX % 1000000000)};
    result = fermi_now();
    assert(result.status == 0 && result.value == SIZE_MAX);
    ++sample.tv_nsec;
    result = fermi_now();
    assert(result.status == 1 && result.code == 2);
    sample = (struct timespec){(time_t)(SIZE_MAX / 1000000000 + 1), 0};
    result = fermi_now();
    assert(result.status == 1 && result.code == 2);
}

int main(void) {
    test_reading();
    calls = 0;
    const size_t values[] = {0, 1, 999999999, 1000000000, SIZE_MAX / 2, SIZE_MAX - 1, SIZE_MAX};
    for (size_t i = 0; i < sizeof(values) / sizeof(values[0]); ++i) {
        for (size_t j = 0; j < sizeof(values) / sizeof(values[0]); ++j) {
            size_t now = values[i], duration = values[j];
            fermi_after_result after = fermi_after(now, duration);
            if (duration > SIZE_MAX - now) {
                assert(after.status == 1 && after.code == 2);
            } else {
                assert(after.status == 0 && after.value == now + duration);
            }
            fermi_remaining_result remaining = fermi_remaining(now, duration);
            assert(remaining.status == 0 &&
                   remaining.value == (duration >= now ? 0 : now - duration));
        }
    }
    assert(calls == 0);
    puts("PASS: clock failure, invalid readings, exact conversion limits and pure deadline "
         "arithmetic");
    return 0;
}
