#include "binary.h"
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

static void test_value(uint32_t value) {
    uint8_t big[4], little[4], output[12];
    for (size_t index = 0; index < 4; ++index) {
        big[index] = (uint8_t)(value >> (24 - index * 8));
        little[index] = (uint8_t)(value >> (index * 8));
    }
    fermi_read_be_result be = fermi_read_be(big, sizeof(big));
    fermi_read_le_result le = fermi_read_le(little, sizeof(little));
    assert(be.status == FERMI_OK && be.value == value);
    assert(le.status == FERMI_OK && le.value == value);
    for (size_t length = 0; length <= 8; ++length) {
        memset(output, 0xa5, sizeof(output));
        fermi_write_be_result written = fermi_write_be(value, output + 1, length);
        if (length < 4) {
            assert(written.status == FERMI_DOMAIN_ERROR && written.code == 1);
            for (size_t index = 0; index < sizeof(output); ++index) {
                assert(output[index] == 0xa5);
            }
        } else {
            assert(written.status == FERMI_OK && written.value == 4);
            assert(!memcmp(output + 1, big, 4));
            be = fermi_read_be(output + 1, length);
            assert(be.status == FERMI_OK && be.value == value);
            assert(output[0] == 0xa5);
            for (size_t index = 5; index < sizeof(output); ++index) {
                assert(output[index] == 0xa5);
            }
        }
        memset(output, 0xa5, sizeof(output));
        fermi_write_le_result emitted = fermi_write_le(value, output + 1, length);
        if (length < 4) {
            assert(emitted.status == FERMI_DOMAIN_ERROR && emitted.code == 1);
            for (size_t index = 0; index < sizeof(output); ++index) {
                assert(output[index] == 0xa5);
            }
        } else {
            assert(emitted.status == FERMI_OK && emitted.value == 4);
            assert(!memcmp(output + 1, little, 4));
            le = fermi_read_le(output + 1, length);
            assert(le.status == FERMI_OK && le.value == value);
            assert(output[0] == 0xa5);
            for (size_t index = 5; index < sizeof(output); ++index) {
                assert(output[index] == 0xa5);
            }
        }
    }
}

int main(void) {
    uint8_t input[3] = {0, 128, 255};
    for (size_t length = 0; length < 4; ++length) {
        fermi_read_be_result be = fermi_read_be(input, length);
        fermi_read_le_result le = fermi_read_le(input, length);
        assert(be.status == FERMI_DOMAIN_ERROR && be.code == 0);
        assert(le.status == FERMI_DOMAIN_ERROR && le.code == 0);
    }
    assert(fermi_read_be(NULL, 0).status == FERMI_DOMAIN_ERROR);
    assert(fermi_read_le(NULL, 0).status == FERMI_DOMAIN_ERROR);
    assert(fermi_write_be(0, NULL, 0).status == FERMI_DOMAIN_ERROR);
    assert(fermi_write_le(0, NULL, 0).status == FERMI_DOMAIN_ERROR);
    assert(fermi_read_be(NULL, 4).status == FERMI_INVALID_ARGUMENT);
    assert(fermi_write_le(0, NULL, 4).status == FERMI_INVALID_ARGUMENT);
    const uint32_t boundaries[] = {0,
                                   1,
                                   127,
                                   128,
                                   255,
                                   256,
                                   65535,
                                   65536,
                                   UINT32_MAX,
                                   UINT32_C(0x80000000),
                                   UINT32_C(0x01020304)};
    for (size_t i = 0; i < sizeof(boundaries) / sizeof(boundaries[0]); ++i) {
        test_value(boundaries[i]);
    }
    // Exercise every possible byte in every encoded position independently.
    for (unsigned position = 0; position < 4; ++position) {
        for (uint32_t byte = 0; byte <= 255; ++byte) {
            test_value(byte << (position * 8));
        }
    }
    uint32_t state = UINT32_C(0x91e10da5);
    for (unsigned trial = 0; trial < 5000; ++trial) {
        state ^= state << 13;
        state ^= state >> 17;
        state ^= state << 5;
        test_value(state);
    }
    puts("PASS: endian encoding, independent byte vectors and failure-preserving output");
    return 0;
}
