#include <assert.h>
#include <errno.h>
#include <inttypes.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static char *test_getenv(const char *name);
#define getenv test_getenv
#include "configuration.c"
#undef getenv

enum { ENV_MISSING = 0, ENV_NAME = 1, ENV_SPACE = 2 };
static char environment_value[256];
static char requested_name[129];
static unsigned lookups;
static bool missing;

static char *test_getenv(const char *name) {
    ++lookups;
    assert(strlen(name) <= 128);
    strcpy(requested_name, name);
    return missing ? NULL : environment_value;
}

static void assert_filled(const uint8_t *bytes, size_t length, uint8_t expected) {
    for (size_t index = 0; index < length; ++index) {
        assert(bytes[index] == expected);
    }
}

static void test_environment(void) {
    const uint8_t name[] = "FERMI_TEST_VALUE";
    uint8_t output[260];
    for (size_t length = 0; length < sizeof(environment_value); ++length) {
        memset(environment_value, 'v', length);
        environment_value[length] = 0;
        memset(output, 0xa5, sizeof(output));
        missing = false;
        fermi_read_value_result result =
            fermi_read_value(name, sizeof(name) - 1, output + 1, length);
        assert(result.status == 0 && result.value == length);
        assert(output[0] == 0xa5);
        assert_filled(output + 1, length, 'v');
        assert_filled(output + length + 1, sizeof(output) - length - 1, 0xa5);
        if (length) {
            memset(output, 0xa5, sizeof(output));
            result = fermi_read_value(name, sizeof(name) - 1, output, length - 1);
            assert(result.status == 1 && result.code == ENV_SPACE);
            assert_filled(output, sizeof(output), 0xa5);
        }
    }
    environment_value[0] = 0;
    fermi_read_value_result empty = fermi_read_value(name, sizeof(name) - 1, NULL, 0);
    assert(empty.status == 0 && empty.value == 0);
    missing = true;
    memset(output, 0xa5, sizeof(output));
    fermi_read_value_result absent =
        fermi_read_value(name, sizeof(name) - 1, output, sizeof(output));
    assert(absent.status == 1 && absent.code == ENV_MISSING);
    assert_filled(output, sizeof(output), 0xa5);
    missing = false;

    const uint8_t invalid[][4] = {
        {'1', 'A', 0, 0}, {'A', '=', 'B', 0}, {'A', 0, 'B', 0}, {0xff, 'A', 0, 0}};
    const size_t lengths[] = {2, 3, 3, 2};
    lookups = 0;
    for (size_t index = 0; index < sizeof(lengths) / sizeof(lengths[0]); ++index) {
        fermi_read_value_result result =
            fermi_read_value(invalid[index], lengths[index], output, sizeof(output));
        assert(result.status == 1 && result.code == ENV_NAME);
        assert_filled(output, sizeof(output), 0xa5);
    }
    assert(fermi_read_value(NULL, 0, output, sizeof(output)).code == ENV_NAME);
    uint8_t long_name[129];
    memset(long_name, 'A', sizeof(long_name));
    assert(fermi_read_value(long_name, sizeof(long_name), output, sizeof(output)).code == ENV_NAME);
    assert(lookups == 0);
    assert(fermi_read_value(long_name, 128, output, sizeof(output)).status == 0);
    assert(strlen(requested_name) == 128 && lookups == 1);
    const uint8_t portable[] = "_VALUE_9";
    assert(fermi_read_value(portable, sizeof(portable) - 1, output, sizeof(output)).status == 0);
    assert(!strcmp(requested_name, (const char *)portable));

    strcpy(environment_value, "first");
    assert(fermi_read_value(name, sizeof(name) - 1, output, sizeof(output)).value == 5);
    strcpy(environment_value, "other");
    assert(!memcmp(output, "first", 5));
    environment_value[0] = (char)0xff;
    environment_value[1] = 0;
    assert(fermi_read_value(name, sizeof(name) - 1, output, sizeof(output)).value == 1);
    assert(output[0] == 0xff);
    unsigned before = lookups;
    assert(fermi_read_value(NULL, 1, output, sizeof(output)).status == 3);
    assert(fermi_read_value(name, sizeof(name) - 1, NULL, 1).status == 3);
    assert(fermi_read_value(output, 1, output, 1).status == 3);
    assert(lookups == before);
}

static void test_number(const uint8_t *bytes, size_t length) {
    fermi_parse_value_result actual = fermi_parse_value(bytes, length);
    unsigned code = 0;
    bool valid = true;
    size_t value = 0;
    if (!length) {
        valid = false;
        code = 0;
    } else if (length > 20) {
        valid = false;
        code = 1;
    } else {
        for (size_t index = 0; index < length; ++index) {
            if (bytes[index] < '0' || bytes[index] > '9') {
                valid = false;
                code = 2;
                break;
            }
        }
        if (valid) {
            char text[21];
            memcpy(text, bytes, length);
            text[length] = 0;
            errno = 0;
            char *end;
            uintmax_t parsed = strtoumax(text, &end, 10);
            assert(end == text + length);
            if (errno == ERANGE || parsed > SIZE_MAX) {
                valid = false;
                code = 3;
            } else {
                value = (size_t)parsed;
            }
        }
    }
    assert(actual.status == (valid ? 0u : 1u));
    assert(valid ? actual.value == value : actual.code == code);
}

static void test_numbers(void) {
    const char *cases[] = {"",
                           "0",
                           "00",
                           "32",
                           "18446744073709551615",
                           "18446744073709551616",
                           "99999999999999999999",
                           "00000000000000000000",
                           "000000000000000000000",
                           "-1",
                           "+1",
                           " 1",
                           "1 ",
                           "1_0",
                           "0x10",
                           "12x",
                           "\xff"};
    for (size_t index = 0; index < sizeof(cases) / sizeof(cases[0]); ++index) {
        test_number((const uint8_t *)cases[index], strlen(cases[index]));
    }
    const uint8_t embedded_nul[] = {'1', 0, '2'};
    test_number(embedded_nul, sizeof(embedded_nul));
    uint32_t seed = 0x7e57a11u;
    for (unsigned sample = 0; sample < 5000; ++sample) {
        seed = seed * 1664525u + 1013904223u;
        size_t length = seed % 26;
        uint8_t text[26];
        for (size_t index = 0; index < length; ++index) {
            seed = seed * 1664525u + 1013904223u;
            text[index] = (uint8_t)('0' + (seed % 16));
        }
        test_number(text, length);
    }
    assert(fermi_parse_value(NULL, 0).code == 0);
    assert(fermi_parse_value(NULL, 1).status == 3);
    const uint8_t thirty_two[] = "32";
    fermi_parse_range_result bounded = fermi_parse_range(thirty_two, 2, 1, 32);
    assert(bounded.status == 0 && bounded.value == 32);
    bounded = fermi_parse_range(thirty_two, 2, 33, 40);
    assert(bounded.status == 1 && bounded.code == 5);
    bounded = fermi_parse_range(thirty_two, 2, 40, 33);
    assert(bounded.status == 1 && bounded.code == 4);
}

static void test_configuration(void) {
    const uint8_t name[] = "FERMI_TEST_VALUE";
    const char *values[] = {"1",
                            "32",
                            "0",
                            "33",
                            "",
                            "-1",
                            "secret-value",
                            "18446744073709551616",
                            "000000000000000000001"};
    const unsigned codes[] = {0, 0, 5, 5, 3, 3, 3, 3, 2};
    for (unsigned index = 0; index < sizeof(values) / sizeof(values[0]); ++index) {
        strcpy(environment_value, values[index]);
        missing = false;
        fermi_configured_result result = fermi_configured(name, sizeof(name) - 1, 1, 32);
        assert(result.status == (index < 2 ? 0u : 1u));
        assert(index < 2 ? result.value == (index ? 32u : 1u) : result.code == codes[index]);
    }
    missing = true;
    assert(fermi_configured(name, sizeof(name) - 1, 1, 32).code == 0);
    unsigned before = lookups;
    assert(fermi_configured(name, sizeof(name) - 1, 32, 1).code == 4);
    assert(lookups == before);
    missing = false;
    assert(fermi_configured(NULL, 0, 1, 32).code == 1);
}

int main(void) {
    test_environment();
    test_numbers();
    test_configuration();
    puts("PASS: bounded environment reads, atomic errors, 5000 decimal cases and startup ranges");
    return 0;
}
