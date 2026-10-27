#include "fermi/arguments.h"
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

static void assert_bytes(const unsigned char *bytes, size_t length, unsigned char value) {
    for (size_t index = 0; index < length; ++index) {
        assert(bytes[index] == value);
    }
}

static void test_startup(void) {
    FermiArguments context = {0};
    size_t count = 99;
    assert(fermi_arguments_count(&context, &count) == FERMI_ARGUMENTS_UNAVAILABLE);
    assert(count == 99);
    assert(!fermi_arguments_init(NULL, 0, NULL));
    assert(!fermi_arguments_init(&context, -1, NULL));
    assert(!fermi_arguments_init(&context, 1, NULL));
    assert(!context.available && !context.values && context.count == 0);
    assert(fermi_arguments_init(&context, 0, NULL));
    assert(fermi_arguments_count(&context, &count) == FERMI_ARGUMENTS_OK && count == 0);
    assert(!fermi_arguments_init(&context, 2, NULL));
    assert(context.available && !context.values && context.count == 0);
    assert(fermi_arguments_count(&context, NULL) == FERMI_ARGUMENTS_INVALID_ARGUMENT);
    assert(fermi_arguments_count(NULL, &count) == FERMI_ARGUMENTS_UNAVAILABLE);

    char name[] = "arbitrary program name";
    char first[] = "first";
    char second[] = "second";
    char *values[] = {name, first, second, NULL};
    FermiArguments left = {0}, right = {0};
    assert(fermi_arguments_init(&left, 3, values));
    assert(fermi_arguments_init(&right, 2, values + 1));
    assert(fermi_arguments_count(&left, &count) == FERMI_ARGUMENTS_OK && count == 2);
    assert(fermi_arguments_count(&right, &count) == FERMI_ARGUMENTS_OK && count == 1);
    unsigned char output[8] = {0};
    size_t length = 0;
    assert(fermi_arguments_read(&left, 0, output, sizeof(output), &length) == FERMI_ARGUMENTS_OK);
    assert(length == 5 && !memcmp(output, "first", length));
    assert(fermi_arguments_read(&right, 0, output, sizeof(output), &length) == FERMI_ARGUMENTS_OK);
    assert(length == 6 && !memcmp(output, "second", length));
    assert(!fermi_arguments_init(&left, 0, NULL));
    assert(left.available && left.count == 2 && left.values == values + 1);
}

static void test_copying(void) {
    char value[256], name[] = "tool";
    char *values[] = {name, value, NULL};
    FermiArguments context = {0};
    assert(fermi_arguments_init(&context, 2, values));
    unsigned char output[260];
    for (size_t length = 0; length < sizeof(value); ++length) {
        memset(value, 'x', length);
        value[length] = 0;
        for (size_t capacity = 0; capacity <= length + 1; ++capacity) {
            memset(output, 0xa5, sizeof(output));
            size_t written = 999;
            FermiArgumentsStatus status =
                fermi_arguments_read(&context, 0, output + 1, capacity, &written);
            if (capacity < length) {
                assert(status == FERMI_ARGUMENTS_OUTPUT_TOO_SMALL && written == 999);
                assert_bytes(output, sizeof(output), 0xa5);
            } else {
                assert(status == FERMI_ARGUMENTS_OK && written == length);
                assert(output[0] == 0xa5);
                assert_bytes(output + 1, length, 'x');
                assert_bytes(output + length + 1, sizeof(output) - length - 1, 0xa5);
            }
        }
    }
    size_t written = 999;
    memset(output, 0xa5, sizeof(output));
    assert(fermi_arguments_read(&context, SIZE_MAX, output, sizeof(output), &written) ==
           FERMI_ARGUMENTS_OUT_OF_RANGE);
    assert(written == 999);
    assert(fermi_arguments_read(&context, 1, output, sizeof(output), &written) ==
           FERMI_ARGUMENTS_OUT_OF_RANGE);
    assert(fermi_arguments_read(NULL, 0, output, sizeof(output), &written) ==
           FERMI_ARGUMENTS_UNAVAILABLE);
    assert(fermi_arguments_read(&context, 0, NULL, 1, &written) ==
           FERMI_ARGUMENTS_INVALID_ARGUMENT);
    assert(fermi_arguments_read(&context, 0, output, sizeof(output), NULL) ==
           FERMI_ARGUMENTS_INVALID_ARGUMENT);
    assert_bytes(output, sizeof(output), 0xa5);
    value[0] = 0;
    assert(fermi_arguments_read(&context, 0, NULL, 0, &written) == FERMI_ARGUMENTS_OK);
    assert(written == 0);
    value[0] = (char)0xff;
    value[1] = 0;
    assert(fermi_arguments_read(&context, 0, output, sizeof(output), &written) ==
           FERMI_ARGUMENTS_OK);
    assert(written == 1 && output[0] == 0xff);
    // The copied bytes are independent; mutation here occurs between calls only.
    value[0] = 'y';
    assert(output[0] == 0xff);
    values[1] = NULL;
    assert(fermi_arguments_read(&context, 0, output, sizeof(output), &written) ==
           FERMI_ARGUMENTS_INVALID_ARGUMENT);
}

int main(void) {
    test_startup();
    test_copying();
    puts("PASS: argument contexts, bounded copies, raw bytes and failure preservation");
    return 0;
}
