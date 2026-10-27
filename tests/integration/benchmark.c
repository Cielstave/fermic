#define _POSIX_C_SOURCE 200809L
#include "reference.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

enum { BATCHES = 101, CALLS_PER_BATCH = 128 };
static volatile size_t checksum;

typedef fermi_parse_samples_result (*ParseFunction)(const uint8_t *, size_t, int16_t *, size_t);

static double monotonic_ns(void) {
    struct timespec now;
    assert(clock_gettime(CLOCK_MONOTONIC, &now) == 0);
    return (double)now.tv_sec * 1e9 + (double)now.tv_nsec;
}

static double measure(ParseFunction parse, const uint8_t *packet, size_t count) {
    int16_t output[4096];
    double start = monotonic_ns();
    for (unsigned call = 0; call < CALLS_PER_BATCH; ++call) {
        fermi_parse_samples_result result = parse(packet, 5 + 2 * count, output, count);
        assert(result.status == FERMI_OK);
        checksum += result.value + (uint16_t)output[call % count];
    }
    return (monotonic_ns() - start) / CALLS_PER_BATCH;
}

static int compare_double(const void *left, const void *right) {
    double a = *(const double *)left;
    double b = *(const double *)right;
    return (a > b) - (a < b);
}

static void benchmark(size_t count, FILE *raw) {
    uint8_t packet[8197] = {'F', 'E', 1, 0, 0};
    packet[3] = (uint8_t)count;
    packet[4] = (uint8_t)(count >> 8);
    for (size_t i = 5; i < 5 + 2 * count; ++i) {
        packet[i] = (uint8_t)(i * 37);
    }

    double fermi[BATCHES];
    double reference[BATCHES];
    double checked[BATCHES];
    (void)measure(fermi_parse_samples, packet, count);
    (void)measure(reference_parse, packet, count);
    (void)measure(reference_parse_checked, packet, count);
    for (unsigned batch = 0; batch < BATCHES; ++batch) {
        /* Rotate all three measurements so each occupies each order position. */
        ParseFunction parsers[] = {fermi_parse_samples, reference_parse, reference_parse_checked};
        double *measurements[] = {fermi, reference, checked};
        for (unsigned step = 0; step < 3; ++step) {
            unsigned index = (batch + step) % 3;
            measurements[index][batch] = measure(parsers[index], packet, count);
        }
    }
    if (raw) {
        for (unsigned batch = 0; batch < BATCHES; ++batch) {
            fprintf(raw, "%zu,%u,%.3f,%.3f,%.3f\n", count, batch, fermi[batch], reference[batch],
                    checked[batch]);
        }
    }
    qsort(fermi, BATCHES, sizeof(*fermi), compare_double);
    qsort(reference, BATCHES, sizeof(*reference), compare_double);
    qsort(checked, BATCHES, sizeof(*checked), compare_double);
    printf("%zu,%.1f,%.1f,%.1f,%.1f,%.1f,%.1f,%.3f,%.1f,%.1f,%.1f,%.3f\n", count, fermi[50],
           fermi[95], fermi[99], reference[50], reference[95], reference[99],
           fermi[50] / reference[50], checked[50], checked[95], checked[99],
           fermi[50] / checked[50]);
}

int main(int argc, char **argv) {
    if (argc > 2) {
        fputs("usage: decode [RAW_CSV]\n", stderr);
        return 2;
    }
    FILE *raw = argc == 2 ? fopen(argv[1], "w") : NULL;
    if (argc == 2 && !raw) {
        perror("cannot open raw measurements");
        return 1;
    }
    if (raw) {
        fputs("samples,batch,fermi_ns,plain_c_ns,checked_c_ns\n", raw);
    }
    puts("samples,fermi_median_ns,fermi_p95_ns,fermi_p99_ns,c_median_ns,c_p95_ns,c_p99_ns,ratio_"
         "plain,checked_c_median_ns,checked_c_p95_ns,checked_c_p99_ns,ratio_checked");
    benchmark(64, raw);
    benchmark(1024, raw);
    benchmark(4096, raw);
    bool failed = raw && ferror(raw);
    if (raw && fclose(raw)) {
        failed = true;
    }
    return failed || ferror(stdout) || checksum == 0;
}
