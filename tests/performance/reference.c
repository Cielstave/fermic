#include "reference.h"

/* These checks mirror the public ABI, including rejection order. Live storage
 * and the absence of conflicting host accesses remain caller obligations. */
static uint32_t validate_view(const int16_t *data, size_t count) {
    if (count && !data) {
        return 1;
    }
    if (count > SIZE_MAX / sizeof(*data)) {
        return 2;
    }
    size_t bytes = count * sizeof(*data);
    uintptr_t address = (uintptr_t)data;
    if (bytes && address % _Alignof(int16_t)) {
        return 3;
    }
    return bytes > UINTPTR_MAX - address ? 2 : 0;
}

fermi_filter_above_result reference_filter(const int16_t *input, size_t length, int16_t *output,
                                           size_t capacity, int16_t threshold) {
    fermi_filter_above_result result = {FERMI_INVALID_ARGUMENT, 0, 0};
    result.code = validate_view(input, length);
    if (result.code) {
        return result;
    }
    result.code = validate_view(output, capacity);
    if (result.code) {
        return result;
    }
    uintptr_t source = (uintptr_t)input, destination = (uintptr_t)output;
    if (length && capacity && source < destination + capacity * sizeof(*output) &&
        destination < source + length * sizeof(*input)) {
        result.code = 4;
        return result;
    }
    result.status = FERMI_DOMAIN_ERROR;
    if (length > 4096) {
        result.code = FERMI_FilterError_input_too_large;
        return result;
    }
    if (capacity < length) {
        result.code = FERMI_FilterError_output_too_small;
        return result;
    }
    size_t written = 0;
    for (size_t i = 0; i < length; ++i) {
        if (input[i] >= threshold) {
            output[written++] = input[i];
        }
    }
    result.status = FERMI_OK;
    result.value = written;
    return result;
}

fermi_sum_samples_result reference_sum(const int16_t *input, size_t length) {
    fermi_sum_samples_result result = {FERMI_INVALID_ARGUMENT, 0, 0};
    result.code = validate_view(input, length);
    if (result.code) {
        return result;
    }
    if (length > 4096) {
        result.status = FERMI_DOMAIN_ERROR;
        result.code = FERMI_SumError_input_too_large;
        return result;
    }
    /* At most 4096 signed 16-bit samples cannot overflow an int64_t sum. */
    int64_t total = 0;
    for (size_t i = 0; i < length; ++i) {
        total += input[i];
    }
    result.status = FERMI_OK;
    result.value = total;
    return result;
}
