#include "hub.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

static uint32_t random_state = 1234567;

static uint32_t next_random(void) {
    random_state = random_state * 1664525u + 1013904223u;
    return random_state;
}

static void check_batch(size_t count, int64_t gain, int64_t divisor, int64_t offset,
                        bool initial_alarm) {
    uint8_t packet[8197] = {70, 69, 1, (uint8_t)count, (uint8_t)(count >> 8)};
    int64_t output[4097];
    int64_t expected[4096];
    int64_t report[10];
    int64_t summary[9] = {(int64_t)count};
    int64_t total = 0;
    int64_t absolute_total = 0;
    bool active = initial_alarm;
    for (size_t i = 0; i < count; ++i) {
        // Include both signed extrema on every run with at least two samples.
        int32_t raw = (int32_t)(next_random() % 65536) - 32768;
        if (i == 0) {
            raw = -32768;
        } else if (i == 1) {
            raw = 32767;
        }
        uint16_t bits = (uint16_t)raw;
        packet[5 + 2 * i] = (uint8_t)bits;
        packet[6 + 2 * i] = (uint8_t)(bits >> 8);
        int64_t value = raw * gain / divisor + offset;
        expected[i] = value;
        if (i == 0 || value < summary[1]) {
            summary[1] = value;
        }
        if (i == 0 || value > summary[2]) {
            summary[2] = value;
        }
        total += value;
        absolute_total += value < 0 ? -value : value;
        bool previous = active;
        if (value >= 800) {
            active = true;
        } else if (value <= 600) {
            active = false;
        }
        summary[5] += active;
        summary[6] += active && !previous;
        summary[7] += !active && previous;
    }
    summary[3] = total / (int64_t)count;
    summary[4] = absolute_total / (int64_t)count;
    summary[8] = active;
    output[count] = 123;
    report[9] = 456;
    fermi_process_batch_result result =
        fermi_process_batch(packet, 5 + count * 2, output, count + 1, report, 10, gain, divisor,
                            offset, 600, 800, initial_alarm);
    assert(result.status == FERMI_OK && result.value == count);
    assert(memcmp(output, expected, count * sizeof(*output)) == 0);
    assert(memcmp(report, summary, sizeof(summary)) == 0);
    assert(output[count] == 123 && report[9] == 456);
}

static void check_rejections(void) {
    const uint32_t errors[] = {
        FERMI_HubError_truncated,           FERMI_HubError_bad_magic,
        FERMI_HubError_unsupported_version, FERMI_HubError_empty_batch,
        FERMI_HubError_too_many_samples,    FERMI_HubError_length_mismatch,
        FERMI_HubError_output_too_small,    FERMI_HubError_output_too_small,
        FERMI_HubError_invalid_calibration, FERMI_HubError_invalid_thresholds,
        FERMI_HubError_truncated,
    };
    for (size_t test = 0; test < sizeof(errors) / sizeof(errors[0]); ++test) {
        uint8_t packet[8] = {70, 69, 1, 1, 0, 42, 0, 0};
        size_t bytes = 7;
        size_t capacity = 1;
        size_t report_size = 9;
        int64_t divisor = 1;
        int64_t release = 600;
        switch (test) {
        case 0:
            bytes = 4;
            break;
        case 1:
            packet[0] = 0;
            break;
        case 2:
            packet[2] = 2;
            break;
        case 3:
            packet[3] = 0;
            bytes = 5;
            break;
        case 4:
            packet[3] = 1;
            packet[4] = 16;
            break;
        case 5:
            bytes = 8;
            break;
        case 6:
            capacity = 0;
            break;
        case 7:
            report_size = 8;
            break;
        case 8:
            divisor = 0;
            break;
        case 9:
            release = 800;
            break;
        case 10:
            bytes = 6;
            break;
        }
        int64_t output[1] = {123};
        int64_t report[9] = {1, 2, 3, 4, 5, 6, 7, 8, 9};
        const int64_t original[9] = {1, 2, 3, 4, 5, 6, 7, 8, 9};
        fermi_process_batch_result result =
            fermi_process_batch(packet, bytes, output, capacity, report, report_size, 1, divisor, 0,
                                release, 800, false);
        assert(result.status == FERMI_DOMAIN_ERROR && result.code == errors[test]);
        assert(output[0] == 123 && memcmp(report, original, sizeof(report)) == 0);
    }
}

static void check_hysteresis(void) {
    const uint8_t packet[] = {70, 69, 1, 6, 0, 80, 0, 70, 0, 60, 0, 70, 0, 80, 0, 70, 0};
    int64_t output[6];
    int64_t report[9];
    fermi_process_batch_result result = fermi_process_batch(packet, sizeof(packet), output, 6,
                                                            report, 9, 10, 1, 0, 600, 800, false);
    const int64_t expected[] = {6, 600, 800, 716, 716, 4, 2, 1, 1};
    assert(result.status == FERMI_OK && memcmp(report, expected, sizeof(expected)) == 0);
    const uint8_t middle[] = {70, 69, 1, 1, 0, 70, 0};
    result =
        fermi_process_batch(middle, sizeof(middle), output, 6, report, 9, 10, 1, 0, 600, 800, true);
    assert(result.status == FERMI_OK && report[8] == 1 && report[6] == 0);
    result = fermi_process_batch(middle, sizeof(middle), output, 6, report, 9, 10, 1, 0, 600, 800,
                                 false);
    assert(result.status == FERMI_OK && report[8] == 0);
    result = fermi_process_batch(middle, sizeof(middle), output, 6, output, 6, 10, 1, 0, 600, 800,
                                 false);
    assert(result.status == FERMI_INVALID_ARGUMENT && result.code == 4);
}

int main(void) {
    check_rejections();
    check_hysteresis();
    check_batch(4096, 1000000, 1, 1000000000, false);
    check_batch(4096, -1000000, 1, -1000000000, true);
    check_batch(1, 0, 1000000, 0, true);
    for (size_t i = 0; i < 2000; ++i) {
        size_t count = 1 + next_random() % 128;
        int64_t gain = (int64_t)(next_random() % 2000001) - 1000000;
        int64_t divisor = 1 + next_random() % 1000000;
        int64_t offset = (int64_t)(next_random() % 2000000001) - 1000000000;
        check_batch(count, gain, divisor, offset, i % 2 != 0);
    }
    puts("PASS: sensor hub differential batches, limits, hysteresis, rejection without writes");
    return 0;
}
