#include "telemetry.h"
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
    int16_t output[4096];
    for (size_t i = 0; i < 4096; ++i) {
        output[i] = 12345;
    }
    fermi_parse_samples_result r = fermi_parse_samples(data, size, output, 4096);
    if (r.status == FERMI_FAULT || r.status == FERMI_INVALID_ARGUMENT) {
        abort();
    }
    if (r.status != FERMI_OK) {
        for (size_t i = 0; i < 4096; ++i) {
            if (output[i] != 12345) {
                abort();
            }
        }
    } else if (r.value > 4096) {
        abort();
    }
    return 0;
}
