#ifndef FERMI_TEST_REFERENCE_H
#define FERMI_TEST_REFERENCE_H
#include "telemetry.h"
fermi_parse_samples_result reference_parse(const uint8_t *packet, size_t length, int16_t *output,
                                           size_t capacity);
fermi_parse_samples_result reference_parse_checked(const uint8_t *, size_t, int16_t *, size_t);
#endif
