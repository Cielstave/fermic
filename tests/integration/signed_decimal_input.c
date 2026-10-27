#include "signed_decimal.c"
#include <assert.h>
#include <inttypes.h>
#include <limits.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

static void check_value(int64_t value, int check_capacity) {
    char expected[32];
    int count = snprintf(expected, sizeof(expected), "%" PRId64, value);
    assert(count > 0 && count <= 20);
    size_t length = (size_t)count;
    fermi_signed_parse64_result parsed = fermi_signed_parse64((const uint8_t *)expected, length);
    assert(parsed.status == 0 && parsed.value == value);
    fermi_signed_parse32_result small = fermi_signed_parse32((const uint8_t *)expected, length);
    if (value < INT32_MIN || value > INT32_MAX) {
        assert(small.status == 1 && small.code == 3);
    } else {
        assert(small.status == 0 && small.value == value);
    }
    if (!check_capacity) {
        return;
    }
    for (size_t capacity = 0; capacity <= 24; capacity++) {
        uint8_t output[28];
        memset(output, 0xa5, sizeof(output));
        fermi_signed_format64_result result = fermi_signed_format64(value, output + 1, capacity);
        if (capacity < length) {
            assert(result.status == 1 && result.code == 0);
            for (size_t index = 0; index < sizeof(output); index++) {
                assert(output[index] == 0xa5);
            }
        } else {
            assert(result.status == 0 && result.value == length);
            assert(memcmp(output + 1, expected, length) == 0);
            assert(output[0] == 0xa5);
            for (size_t index = length + 1; index < sizeof(output); index++) {
                assert(output[index] == 0xa5);
            }
        }
        if (value >= INT32_MIN && value <= INT32_MAX) {
            uint8_t narrow_output[28];
            memset(narrow_output, 0xa5, sizeof(narrow_output));
            fermi_signed_format32_result narrow =
                fermi_signed_format32((int32_t)value, narrow_output + 1, capacity);
            assert(narrow.status == result.status && narrow.code == result.code);
            if (narrow.status == 0) {
                assert(narrow.value == result.value);
            }
            assert(memcmp(narrow_output, output, sizeof(output)) == 0);
        }
    }
}

static void reject(const uint8_t *bytes, size_t length, uint32_t code) {
    fermi_signed_parse64_result wide = fermi_signed_parse64(bytes, length);
    fermi_signed_parse32_result small = fermi_signed_parse32(bytes, length);
    assert(wide.status == 1 && wide.code == code);
    assert(small.status == 1 && small.code == code);
}

static void check_error(const char *text, uint32_t code) {
    reject((const uint8_t *)text, strlen(text), code);
}

static void test_syntax(void) {
    check_error("", 0);
    check_error("-", 0);
    check_error("+1", 2);
    check_error("--1", 2);
    check_error("1-", 2);
    check_error(" 1", 2);
    check_error("1 ", 2);
    check_error("1\n", 2);
    check_error("1_0", 2);
    check_error("0x10", 2);
    check_error("1.0", 2);
    check_error("2147483648x", 2);
    check_error("-2147483649x", 2);
    check_error("9223372036854775808", 3);
    check_error("-9223372036854775809", 3);
    check_error("18446744073709551615", 3);
    check_error("000000000000000000000", 1);
    check_error("-000000000000000000000", 1);
    check_error("invalid-input-too-long", 1);
    check_error("9223372036854775808x", 3);
    check_error("x9223372036854775808", 2);
    reject(NULL, 0, 0);
    for (unsigned byte = 0; byte <= UINT8_MAX; byte++) {
        uint8_t input[3] = {'1', (uint8_t)byte, '2'};
        uint8_t original[3];
        memcpy(original, input, sizeof(input));
        if (byte >= '0' && byte <= '9') {
            fermi_signed_parse64_result parsed = fermi_signed_parse64(input, sizeof(input));
            assert(parsed.status == 0 && parsed.value == 102 + 10 * (byte - '0'));
        } else {
            reject(input, sizeof(input), 2);
        }
        assert(memcmp(original, input, sizeof(input)) == 0);
    }
    const char *zeros[] = {"-0", "00000000000000000000", "-00000000000000000000"};
    for (size_t index = 0; index < sizeof(zeros) / sizeof(zeros[0]); index++) {
        fermi_signed_parse64_result result =
            fermi_signed_parse64((const uint8_t *)zeros[index], strlen(zeros[index]));
        assert(result.status == 0 && result.value == 0);
    }
}

int main(void) {
    test_syntax();
    const int64_t boundaries[] = {INT64_MIN, INT64_MIN + 1,
                                  INT64_MAX, INT64_MAX - 1,
                                  INT32_MIN, (int64_t)INT32_MIN - 1,
                                  INT32_MAX, (int64_t)INT32_MAX + 1,
                                  -1,        0,
                                  1};
    for (size_t index = 0; index < sizeof(boundaries) / sizeof(boundaries[0]); index++) {
        check_value(boundaries[index], 1);
    }
    for (int64_t value = INT16_MIN; value <= INT16_MAX; value++) {
        check_value(value, 0);
    }
    for (int64_t power = 1;;) {
        check_value(power - 1, 1);
        check_value(power, 1);
        check_value(power + 1, 1);
        check_value(-power - 1, 1);
        check_value(-power, 1);
        check_value(-power + 1, 1);
        if (power > INT64_MAX / 10) {
            break;
        }
        power *= 10;
    }
    uint64_t state = UINT64_C(0xe12d793b450c68af);
    for (unsigned trial = 0; trial < 5000; trial++) {
        state ^= state << 13;
        state ^= state >> 7;
        state ^= state << 17;
        int64_t value = (int64_t)(state & INT64_MAX);
        if (state >> 63) {
            value = -value - 1;
        }
        check_value(value, 1);
        int32_t small = (int32_t)(state & INT32_MAX);
        if (state & UINT64_C(0x80000000)) {
            small = -small - 1;
        }
        check_value(small, 1);
    }
    puts("PASS: signed decimal boundaries, 65536 small integers, 10000 seeded values and guarded "
         "output capacities");
    return 0;
}
