#include "reference.h"

/* Independent reference: decode unsigned wire words, then map to signed range. */
fermi_parse_samples_result reference_parse(const uint8_t *p, size_t len, int16_t *out,
                                           size_t capacity) {
    fermi_parse_samples_result r = {FERMI_DOMAIN_ERROR, 0, 0};
    if (len < 5) {
        r.code = FERMI_ParseError_truncated;
        return r;
    }
    if (p[0] != 'F' || p[1] != 'E') {
        r.code = FERMI_ParseError_bad_magic;
        return r;
    }
    if (p[2] != 1) {
        r.code = FERMI_ParseError_unsupported_version;
        return r;
    }
    size_t count = (size_t)p[3] | ((size_t)p[4] << 8);
    if (count > 4096) {
        r.code = FERMI_ParseError_too_many_samples;
        return r;
    }
    size_t required = 5 + 2 * count;
    if (len < required) {
        r.code = FERMI_ParseError_truncated;
        return r;
    }
    if (len != required) {
        r.code = FERMI_ParseError_length_mismatch;
        return r;
    }
    if (capacity < count) {
        r.code = FERMI_ParseError_output_too_small;
        return r;
    }
    for (size_t i = 0; i < count; ++i) {
        unsigned word = (unsigned)p[5 + 2 * i] | ((unsigned)p[6 + 2 * i] << 8);
        out[i] = (int16_t)(word < 32768 ? (int)word : (int)word - 65536);
    }
    r.status = FERMI_OK;
    r.value = count;
    return r;
}

/* Match the public Fermi ABI checks before invoking the independent decoder.
 * Live allocations and conflicting host threads remain caller obligations. */
fermi_parse_samples_result reference_parse_checked(const uint8_t *packet, size_t length,
                                                   int16_t *output, size_t capacity) {
    fermi_parse_samples_result rejected = {FERMI_INVALID_ARGUMENT, 0, 0};
    uintptr_t input_address = (uintptr_t)packet;
    uintptr_t output_address = (uintptr_t)output;
    if (length && !packet) {
        rejected.code = 1;
        return rejected;
    }
    if (length > UINTPTR_MAX - input_address) {
        rejected.code = 2;
        return rejected;
    }
    if (capacity && !output) {
        rejected.code = 1;
        return rejected;
    }
    if (capacity > SIZE_MAX / sizeof(*output)) {
        rejected.code = 2;
        return rejected;
    }
    size_t output_bytes = capacity * sizeof(*output);
    if (output_bytes && output_address % _Alignof(int16_t)) {
        rejected.code = 3;
        return rejected;
    }
    if (output_bytes > UINTPTR_MAX - output_address) {
        rejected.code = 2;
        return rejected;
    }
    if (length && output_bytes && input_address < output_address + output_bytes &&
        output_address < input_address + length) {
        rejected.code = 4;
        return rejected;
    }
    return reference_parse(packet, length, output, capacity);
}
