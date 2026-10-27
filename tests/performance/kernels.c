#define _POSIX_C_SOURCE 200809L
#include "reference.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

enum { BATCHES = 101, CALLS = 128 };
static volatile uint64_t checksum;
static int16_t input[4096], output[4096];

typedef fermi_filter_above_result (*Filter)(const int16_t *, size_t, int16_t *, size_t, int16_t);
typedef fermi_sum_samples_result (*Sum)(const int16_t *, size_t);

static double now_ns(void) {
    struct timespec now;
    assert(clock_gettime(CLOCK_MONOTONIC, &now) == 0);
    return (double)now.tv_sec * 1e9 + (double)now.tv_nsec;
}

static double measure_filter(Filter function, size_t count, int16_t threshold) {
    double start = now_ns();
    for (unsigned i = 0; i < CALLS; ++i) {
        fermi_filter_above_result result = function(input, count, output, count, threshold);
        assert(result.status == FERMI_OK);
        checksum += result.value;
        if (result.value) {
            checksum += (uint16_t)output[i % result.value];
        }
    }
    return (now_ns() - start) / CALLS;
}

static double measure_sum(Sum function, size_t count) {
    double start = now_ns();
    for (unsigned i = 0; i < CALLS; ++i) {
        fermi_sum_samples_result result = function(input, count);
        assert(result.status == FERMI_OK);
        checksum += (uint64_t)result.value;
    }
    return (now_ns() - start) / CALLS;
}

static int compare(const void *left, const void *right) {
    double a = *(const double *)left, b = *(const double *)right;
    return (a > b) - (a < b);
}

static void measure(const char *kernel, size_t count, int16_t threshold, FILE *raw) {
    bool sum = !strcmp(kernel, "sum");
    double samples[2][BATCHES];
    Filter filters[] = {fermi_filter_above, reference_filter};
    Sum sums[] = {fermi_sum_samples, reference_sum};
    for (unsigned variant = 0; variant < 2; ++variant) {
        if (sum) {
            (void)measure_sum(sums[variant], count);
        } else {
            (void)measure_filter(filters[variant], count, threshold);
        }
    }
    for (unsigned batch = 0; batch < BATCHES; ++batch) {
        for (unsigned step = 0; step < 2; ++step) {
            unsigned variant = (batch + step) % 2;
            samples[variant][batch] = sum ? measure_sum(sums[variant], count)
                                          : measure_filter(filters[variant], count, threshold);
        }
        fprintf(raw, "%s,%zu,%d,%u,%.3f,%.3f\n", kernel, count, threshold, batch, samples[0][batch],
                samples[1][batch]);
    }
    for (unsigned variant = 0; variant < 2; ++variant) {
        qsort(samples[variant], BATCHES, sizeof(double), compare);
    }
    printf("%s,%zu,%d,%.1f,%.1f,%.1f,%.1f,%.1f,%.1f,%.3f\n", kernel, count, threshold,
           samples[0][50], samples[0][95], samples[0][99], samples[1][50], samples[1][95],
           samples[1][99], samples[0][50] / samples[1][50]);
}

int main(int argc, char **argv) {
    validate_kernels();
    if (argc == 2 && !strcmp(argv[1], "--validate")) {
        puts("PASS: filtering and sum differential cases, extrema, rejection and output "
             "preservation");
        return 0;
    }
    if (argc != 2) {
        fputs("usage: kernels RAW_CSV | --validate\n", stderr);
        return 2;
    }
    FILE *raw = fopen(argv[1], "w");
    if (!raw) {
        perror("cannot open raw measurements");
        return 1;
    }
    uint32_t seed = 7;
    for (size_t i = 0; i < 4096; ++i) {
        seed = seed * 1664525u + 1013904223u;
        input[i] = (int16_t)((int32_t)(seed & 65535u) - 32768);
    }
    fputs("kernel,samples,threshold,batch,fermi_ns,checked_c_ns\n", raw);
    puts("kernel,samples,threshold,fermi_median_ns,fermi_p95_ns,fermi_p99_ns,"
         "checked_c_median_ns,checked_c_p95_ns,checked_c_p99_ns,ratio_checked");
    const size_t sizes[] = {64, 1024, 4096};
    for (unsigned i = 0; i < sizeof(sizes) / sizeof(sizes[0]); ++i) {
        measure("filter", sizes[i], INT16_MIN, raw);
        measure("filter", sizes[i], 0, raw);
        measure("filter", sizes[i], INT16_MAX, raw);
        measure("sum", sizes[i], 0, raw);
    }
    bool failed = ferror(raw);
    if (fclose(raw)) {
        failed = true;
    }
    return failed || ferror(stdout);
}
