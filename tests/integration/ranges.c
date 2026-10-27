#include "ranges.h"

#include <assert.h>
#include <stdio.h>

int main(void) {
    int16_t input[4096];
    const size_t lengths[] = {0, 1, 2, 7, 8, 15, 16, 17, 63, 64, 65, 4095, 4096};
    uint32_t seed = 11;
    for (unsigned trial = 0; trial < 100; ++trial) {
        for (size_t i = 0; i < 4096; ++i) {
            seed = seed * 1664525u + 1013904223u;
            input[i] = trial == 0   ? INT16_MIN
                       : trial == 1 ? INT16_MAX
                                    : (int16_t)((int32_t)(seed & 65535u) - 32768);
        }
        for (unsigned j = 0; j < sizeof(lengths) / sizeof(lengths[0]); ++j) {
            size_t length = lengths[j];
            int64_t expected = 0;
            for (size_t i = 0; i < length; ++i) {
                expected += input[i];
            }
            fermi_bounded_sum_result actual = fermi_bounded_sum(input, length);
            assert(actual.status == FERMI_OK && actual.value == expected);
        }
    }
    assert(fermi_bounded_sum(NULL, 0).status == FERMI_OK);
    assert(fermi_bounded_sum(NULL, 1).status == FERMI_INVALID_ARGUMENT);
    input[0] = 1;
    fermi_seeded_sum_result seeded = fermi_seeded_sum(input, 1, INT64_MAX);
    assert(seeded.status == FERMI_FAULT && seeded.code == FERMI_FAULT_OVERFLOW);
    input[0] = -1;
    seeded = fermi_seeded_sum(input, 1, INT64_MIN);
    assert(seeded.status == FERMI_FAULT && seeded.code == FERMI_FAULT_OVERFLOW);
    int64_t wide[2] = {INT64_MAX, 1};
    fermi_wide_sum_result result = fermi_wide_sum(wide, 2);
    assert(result.status == FERMI_FAULT && result.code == FERMI_FAULT_OVERFLOW);
    wide[0] = INT64_MIN;
    wide[1] = -1;
    result = fermi_wide_sum(wide, 2);
    assert(result.status == FERMI_FAULT && result.code == FERMI_FAULT_OVERFLOW);
    input[0] = 1;
    fermi_two_updates_result twice = fermi_two_updates(input, 1);
    assert(twice.status == FERMI_FAULT && twice.code == FERMI_FAULT_OVERFLOW);
    fermi_different_input_result bounds = fermi_different_input(input, 1, NULL, 0);
    assert(bounds.status == FERMI_FAULT && bounds.code == FERMI_FAULT_BOUNDS);
    puts("PASS: proven sums, SIMD tails, extrema, retained overflow and bounds faults");
    return 0;
}
