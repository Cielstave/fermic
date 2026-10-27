#include "reference.h"
#include <assert.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static uint32_t rng = 0x12345678;
static uint32_t random_word(void) {
    rng ^= rng << 13;
    rng ^= rng >> 17;
    rng ^= rng << 5;
    return rng;
}
static void compare(const uint8_t *p, size_t len, size_t capacity) {
    int16_t actual[4098], expected[4098];
    for (size_t i = 0; i < 4098; ++i) {
        actual[i] = expected[i] = 12345;
    }
    fermi_parse_samples_result a = fermi_parse_samples(p, len, actual + 1, capacity);
    fermi_parse_samples_result b = reference_parse_checked(p, len, expected + 1, capacity);
    assert(a.status == b.status && a.code == b.code && a.value == b.value);
    assert(memcmp(actual, expected, sizeof(actual)) == 0);
}
int main(void) {
    uint8_t packet[8198] = {'F', 'E', 1, 0, 0};
    compare(NULL, 0, 0);
    assert(fermi_parse_samples(packet, 5, NULL, 0).status == FERMI_OK);
    for (size_t len = 0; len < 5; ++len) {
        compare(packet, len, 0);
    }
    compare(packet, 6, 0);
    packet[0] = 'X';
    compare(packet, 5, 0);
    packet[0] = 'F';
    packet[2] = 2;
    compare(packet, 5, 0);
    packet[2] = 1;
    packet[3] = 1;
    packet[4] = 16;
    compare(packet, 5, 4096);
    packet[3] = 0;
    packet[4] = 16;
    for (size_t i = 0; i < 8192; ++i) {
        packet[5 + i] = (uint8_t)random_word();
    }
    compare(packet, 8197, 4096);
    compare(packet, 8197, 4095);
    compare(packet, 8196, 4096);
    /* Exhaustive 16-bit payload values; signed edge cases are not sampled. */
    packet[3] = 1;
    packet[4] = 0;
    for (unsigned value = 0; value < 65536; ++value) {
        packet[5] = (uint8_t)value;
        packet[6] = (uint8_t)(value >> 8);
        int16_t output = 0;
        fermi_parse_samples_result result = fermi_parse_samples(packet, 7, &output, 1);
        assert(result.status == FERMI_OK && result.value == 1);
        assert(output == (int16_t)(value < 32768 ? (int)value : (int)value - 65536));
    }
    for (unsigned trial = 0; trial < 4000; ++trial) {
        size_t count = random_word() % 4097;
        packet[0] = 'F';
        packet[1] = 'E';
        packet[2] = 1;
        packet[3] = (uint8_t)count;
        packet[4] = (uint8_t)(count >> 8);
        size_t length = 5 + 2 * count;
        for (size_t i = 5; i < length; ++i) {
            packet[i] = (uint8_t)random_word();
        }
        size_t capacity = count;
        switch (trial % 6) {
        case 0:
            if (length) {
                --length;
            }
            break;
        case 1:
            ++length;
            break;
        case 2:
            if (count) {
                --capacity;
            }
            break;
        case 3:
            packet[2] = 4;
            break;
        case 4:
            packet[random_word() % 5] ^= 0x80;
            break;
        default:
            break;
        }
        compare(packet, length, capacity);
    }
    int16_t output[4] = {1, 2, 3, 4};
    assert(fermi_parse_samples(NULL, 1, output, 4).status == FERMI_INVALID_ARGUMENT);
    assert(fermi_parse_samples(packet, 5, NULL, 1).status == FERMI_INVALID_ARGUMENT);
    assert(fermi_parse_samples(packet, 5, output, SIZE_MAX).status == FERMI_INVALID_ARGUMENT);
    _Alignas(int16_t) uint8_t overlap[16] = {'F', 'E', 1, 1, 0, 0, 0};
    assert(fermi_parse_samples(overlap, 7, (int16_t *)overlap, 1).code == 4);
    assert(fermi_parse_samples(packet, 7, (int16_t *)(overlap + 1), 1).code == 3);
    assert(reference_parse_checked(NULL, 1, output, 4).code == 1);
    assert(reference_parse_checked(packet, 5, NULL, 1).code == 1);
    assert(reference_parse_checked(packet, 5, output, SIZE_MAX).code == 2);
    assert(reference_parse_checked(overlap, 7, (int16_t *)overlap, 1).code == 4);
    assert(reference_parse_checked(packet, 7, (int16_t *)(overlap + 1), 1).code == 3);
    puts("PASS: telemetry reference, 65536 values, 4000 differential packets, ABI guards");
    return 0;
}
