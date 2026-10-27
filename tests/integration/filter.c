#include "filter_above.h"

#include <assert.h>
#include <stdio.h>

int main(void) {
    const int16_t input[4] = {2, 7, 1, 9};
    int16_t output[4] = {11, 12, 13, 14};
    fermi_filter_above_result result = fermi_filter_above(input, 4, output, 4, 5);
    assert(result.status == FERMI_OK && result.value == 2);
    assert(output[0] == 7 && output[1] == 9 && output[2] == 13 && output[3] == 14);
    result = fermi_filter_above(input, 4, output, 1, 5);
    assert(result.status == FERMI_DOMAIN_ERROR &&
           result.code == FERMI_FilterError_output_too_small);
    assert(output[0] == 7 && output[1] == 9);
    assert(fermi_filter_above(NULL, 0, NULL, 0, 5).status == FERMI_OK);
    int16_t large[4097] = {0};
    result = fermi_filter_above(large, 4097, output, 4, 0);
    assert(result.status == FERMI_DOMAIN_ERROR && result.code == FERMI_FilterError_input_too_large);
    puts("PASS: filter values, rejection without writes, empty buffers");
    return 0;
}
