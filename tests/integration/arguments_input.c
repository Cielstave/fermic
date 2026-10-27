#include "arguments.c"
#include <assert.h>
#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

static void test_arguments(void) {
    uint8_t output[16];
    memset(output, 0xa5, sizeof(output));
    fermi_count_arguments_result count = fermi_count_arguments();
    assert(count.status == 1 && count.code == 0);
    fermi_read_argument_result result = fermi_read_argument(0, output, sizeof(output));
    assert(result.status == 1 && result.code == 0 && output[0] == 0xa5);
    char name[] = "ignored name";
    char first[] = "first";
    char empty[] = "";
    char raw[] = {(char)0xff, 'x', 0};
    char *values[] = {name, first, empty, raw, NULL};
    assert(fermi_arguments_init(&fe_arguments, 4, values));
    count = fermi_count_arguments();
    assert(count.status == 0 && count.value == 3);
    result = fermi_read_argument(0, output, sizeof(output));
    assert(result.status == 0 && result.value == 5 && !memcmp(output, "first", 5));
    assert(output[5] == 0xa5);
    result = fermi_read_argument(1, NULL, 0);
    assert(result.status == 0 && result.value == 0);
    result = fermi_read_argument(2, output, sizeof(output));
    assert(result.status == 0 && result.value == 2 && output[0] == 0xff && output[1] == 'x');
    memset(output, 0xa5, sizeof(output));
    result = fermi_read_argument(0, output, 4);
    assert(result.status == 1 && result.code == 2);
    for (size_t index = 0; index < sizeof(output); ++index) {
        assert(output[index] == 0xa5);
    }
    result = fermi_read_argument(SIZE_MAX, output, sizeof(output));
    assert(result.status == 1 && result.code == 1);
    result = fermi_read_argument(0, NULL, 1);
    assert(result.status == 3 && result.code == 1);
}

static void test_format(size_t value) {
    char expected[21];
    int length = snprintf(expected, sizeof(expected), "%zu", value);
    assert(length > 0 && length <= 20);
    for (size_t capacity = 0; capacity <= 24; ++capacity) {
        uint8_t output[28];
        memset(output, 0xa5, sizeof(output));
        fermi_format_value_result result = fermi_format_value(value, output + 1, capacity);
        if (capacity < (size_t)length) {
            assert(result.status == 1 && result.code == 0);
            for (size_t index = 0; index < sizeof(output); ++index) {
                assert(output[index] == 0xa5);
            }
        } else {
            assert(result.status == 0 && result.value == (size_t)length);
            assert(!memcmp(output + 1, expected, (size_t)length));
            assert(output[0] == 0xa5);
            for (size_t index = (size_t)length + 1; index < sizeof(output); ++index) {
                assert(output[index] == 0xa5);
            }
        }
    }
}

int main(void) {
    test_arguments();
    test_format(0);
    test_format(SIZE_MAX);
    for (size_t power = 1;;) {
        test_format(power - 1);
        test_format(power);
        test_format(power + 1);
        if (power > SIZE_MAX / 10) {
            break;
        }
        power *= 10;
    }
    uint64_t state = UINT64_C(0x9e3779b97f4a7c15);
    for (unsigned trial = 0; trial < 5000; ++trial) {
        state ^= state << 13;
        state ^= state >> 7;
        state ^= state << 17;
        test_format((size_t)state);
    }
    assert(fermi_format_value(0, NULL, 0).status == 1);
    assert(fermi_format_value(0, NULL, 1).status == 3);
    puts("PASS: generated argument adapter and 5000 decimal-format differential cases");
    return 0;
}
