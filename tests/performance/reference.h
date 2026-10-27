#ifndef FERMI_PERFORMANCE_REFERENCE_H
#define FERMI_PERFORMANCE_REFERENCE_H
#include "filter_above.h"
#include "sum_samples.h"

fermi_filter_above_result reference_filter(const int16_t *, size_t, int16_t *, size_t, int16_t);
fermi_sum_samples_result reference_sum(const int16_t *, size_t);
void validate_kernels(void);
#endif
