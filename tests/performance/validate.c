#include "reference.h"

#include <assert.h>
#include <string.h>

static void compare_sum(const int16_t *data, size_t count) {
    fermi_sum_samples_result actual = fermi_sum_samples(data, count);
    fermi_sum_samples_result expected = reference_sum(data, count);
    assert(actual.status == expected.status && actual.code == expected.code);
    assert(actual.status != FERMI_OK || actual.value == expected.value);
}

void validate_kernels(void) {
    int16_t input[4097], actual[4097], expected[4097];
    uint32_t seed = 19;
    for (unsigned trial = 0; trial < 512; ++trial) {
        size_t count = trial < 4 ? trial : (trial * 127u) % 4098;
        if (trial == 510) {
            count = 4096;
        } else if (trial == 511) {
            count = 4097;
        }
        for (size_t i = 0; i < count; ++i) {
            seed = seed * 1664525u + 1013904223u;
            input[i] = (int16_t)((int32_t)(seed & 65535u) - 32768);
        }
        compare_sum(input, count);
        const int16_t thresholds[] = {INT16_MIN, -1, 0, 1, INT16_MAX};
        for (unsigned j = 0; j < sizeof(thresholds) / sizeof(thresholds[0]); ++j) {
            size_t capacity = trial % 3 ? count : count / 2;
            memset(actual, 0x55, sizeof(actual));
            memset(expected, 0x55, sizeof(expected));
            fermi_filter_above_result a =
                fermi_filter_above(input, count, actual, capacity, thresholds[j]);
            fermi_filter_above_result b =
                reference_filter(input, count, expected, capacity, thresholds[j]);
            assert(a.status == b.status && a.code == b.code);
            assert(a.status != FERMI_OK || a.value == b.value);
            assert(memcmp(actual, expected, sizeof(actual)) == 0);
        }
    }
    for (size_t i = 0; i < 4096; ++i) {
        input[i] = INT16_MIN;
    }
    compare_sum(input, 4096);
    assert(fermi_sum_samples(input, 4096).value == -INT64_C(134217728));
    for (size_t i = 0; i < 4096; ++i) {
        input[i] = INT16_MAX;
    }
    compare_sum(input, 4096);
    assert(fermi_sum_samples(input, 4096).value == INT64_C(134213632));
    compare_sum(NULL, 0);
    compare_sum(NULL, 1);
    compare_sum(input, SIZE_MAX);
    fermi_filter_above_result a = fermi_filter_above(input, 1, input, 1, 0);
    fermi_filter_above_result b = reference_filter(input, 1, input, 1, 0);
    assert(a.status == FERMI_INVALID_ARGUMENT && a.status == b.status && a.code == b.code);
    a = fermi_filter_above(NULL, 0, NULL, 0, 0);
    b = reference_filter(NULL, 0, NULL, 0, 0);
    assert(a.status == b.status && a.code == b.code && a.value == b.value);
}
