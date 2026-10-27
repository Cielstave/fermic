#include <assert.h>
#include <data_generics.c>
#include <stdint.h>
#include <stdio.h>

int main(void) {
    uint32_t state = 17;
    for (size_t iteration = 0; iteration < 4096; iteration++) {
        state = state * UINT32_C(1664525) + UINT32_C(1013904223);
        size_t seed = state % 1000001;
        size_t index = iteration % 8;
        fermi_record_pipeline_result result = fermi_record_pipeline(seed, index);
        if (index >= 4) {
            assert(result.status == 1 && result.code == 1);
            continue;
        }
        size_t rows[4] = {seed, seed + 1, seed + 2, 99};
        size_t first = rows[0];
        rows[0] = rows[index];
        rows[index] = first;
        assert(result.status == 0);
        assert(result.value == rows[0] * 10000000 + rows[1] + rows[3]);
    }
    fermi_record_pipeline_result result = fermi_record_pipeline(0, SIZE_MAX);
    assert(result.status == 1 && result.code == 1);
    result = fermi_record_pipeline(SIZE_MAX, 0);
    assert(result.status == 1 && result.code == 1);
    puts("PASS: generic record pipeline, 4096 independent cases and rejected bounds");
    return 0;
}
