#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void *tracked_allocate(size_t size);
static void tracked_release(void *pointer);
#define malloc tracked_allocate
#define free tracked_release
#include <owned_text.c>
#undef malloc
#undef free

static void *live[16];
static unsigned attempts, releases, fail_at;

static void *tracked_allocate(size_t size) {
    ++attempts;
    if (fail_at == attempts) {
        return NULL;
    }
    void *pointer = malloc(size);
    assert(pointer != NULL);
    for (unsigned slot = 0; slot < 16; slot++) {
        if (live[slot] == NULL) {
            live[slot] = pointer;
            return pointer;
        }
    }
    abort();
}

static void tracked_release(void *pointer) {
    for (unsigned slot = 0; slot < 16; slot++) {
        if (live[slot] == pointer) {
            assert(pointer != NULL);
            live[slot] = NULL;
            ++releases;
            free(pointer);
            return;
        }
    }
    abort();
}

/* Decode scalar values, independently of the library's second-byte bounds. */
static bool reference_utf8(const uint8_t *bytes, size_t length) {
    size_t offset = 0;
    while (offset < length) {
        unsigned first = bytes[offset++], width;
        uint32_t scalar, minimum;
        if (first < 128) {
            continue;
        }
        if ((first & 0xe0) == 0xc0) {
            width = 2;
            scalar = first & 31;
            minimum = 128;
        } else if ((first & 0xf0) == 0xe0) {
            width = 3;
            scalar = first & 15;
            minimum = 2048;
        } else if ((first & 0xf8) == 0xf0) {
            width = 4;
            scalar = first & 7;
            minimum = 65536;
        } else {
            return false;
        }
        if (width - 1 > length - offset) {
            return false;
        }
        for (unsigned index = 1; index < width; index++) {
            unsigned byte = bytes[offset++];
            if ((byte & 0xc0) != 0x80) {
                return false;
            }
            scalar = (scalar << 6) | (byte & 63);
        }
        if (scalar < minimum || scalar > 0x10ffff || (scalar >= 0xd800 && scalar <= 0xdfff)) {
            return false;
        }
    }
    return true;
}

static size_t encode(uint32_t scalar, uint8_t output[4]) {
    if (scalar < 128) {
        output[0] = (uint8_t)scalar;
        return 1;
    }
    if (scalar < 2048) {
        output[0] = (uint8_t)(0xc0 | (scalar >> 6));
        output[1] = (uint8_t)(0x80 | (scalar & 63));
        return 2;
    }
    if (scalar < 65536) {
        output[0] = (uint8_t)(0xe0 | (scalar >> 12));
        output[1] = (uint8_t)(0x80 | ((scalar >> 6) & 63));
        output[2] = (uint8_t)(0x80 | (scalar & 63));
        return 3;
    }
    output[0] = (uint8_t)(0xf0 | (scalar >> 18));
    output[1] = (uint8_t)(0x80 | ((scalar >> 12) & 63));
    output[2] = (uint8_t)(0x80 | ((scalar >> 6) & 63));
    output[3] = (uint8_t)(0x80 | (scalar & 63));
    return 4;
}

static void compare(const uint8_t *bytes, size_t length) {
    fermi_validate_utf8_result result = fermi_validate_utf8(bytes, length);
    assert(result.status == 0 && result.value == reference_utf8(bytes, length));
}

static void test_validation(void) {
    compare(NULL, 0);
    for (unsigned first = 0; first < 256; first++) {
        uint8_t bytes[2] = {(uint8_t)first, 0};
        compare(bytes, 1);
        for (unsigned second = 0; second < 256; second++) {
            bytes[1] = (uint8_t)second;
            compare(bytes, 2);
        }
    }
    for (uint32_t scalar = 0; scalar <= 0x10ffff; scalar++) {
        uint8_t encoded[4];
        size_t length = encode(scalar, encoded);
        compare(encoded, length);
        if (length > 1) {
            compare(encoded, length - 1);
        }
    }
    uint32_t random = UINT32_C(0xc19f8a73);
    for (unsigned trial = 0; trial < 10000; trial++) {
        uint8_t bytes[24];
        for (size_t index = 0; index < sizeof(bytes); index++) {
            random ^= random << 13;
            random ^= random >> 17;
            random ^= random << 5;
            bytes[index] = (uint8_t)random;
        }
        compare(bytes, trial % 25);
    }
}

static void test_storage(void) {
    const uint8_t input[] = {'c', 'a', 'f', 0xc3, 0xa9};
    for (size_t capacity = 0; capacity <= 8; capacity++) {
        uint8_t output[12];
        memset(output, 0xa5, sizeof(output));
        fermi_text_roundtrip_result result =
            fermi_text_roundtrip(input, sizeof(input), 64, output + 1, capacity);
        if (capacity < sizeof(input)) {
            assert(result.status == 1 && result.code == 4);
            for (size_t index = 0; index < sizeof(output); index++) {
                assert(output[index] == 0xa5);
            }
        } else {
            assert(result.status == 0 && result.value == sizeof(input));
            assert(output[0] == 0xa5 && !memcmp(output + 1, input, sizeof(input)));
            for (size_t index = sizeof(input) + 1; index < sizeof(output); index++) {
                assert(output[index] == 0xa5);
            }
        }
    }
    uint8_t output[32];
    memset(output, 0xa5, sizeof(output));
    unsigned before = attempts;
    const uint8_t invalid[] = {0xed, 0xa0, 0x80};
    fermi_text_roundtrip_result rejected =
        fermi_text_roundtrip(invalid, sizeof(invalid), 64, output, sizeof(output));
    assert(rejected.status == 1 && rejected.code == 0 && attempts == before);
    rejected = fermi_text_roundtrip(invalid, sizeof(invalid), 1, output, sizeof(output));
    assert(rejected.status == 1 && rejected.code == 3 && attempts == before);
    for (size_t index = 0; index < sizeof(output); index++) {
        assert(output[index] == 0xa5);
    }
    fermi_text_append_result appended = fermi_text_append(
        (const uint8_t *)"ab", 2, input, sizeof(input), 64, output, sizeof(output));
    assert(appended.status == 0 && appended.value == 7 && !memcmp(output, "abcaf\xc3\xa9", 7));
    memset(output, 0xa5, sizeof(output));
    appended = fermi_text_append((const uint8_t *)"ab", 2, invalid, sizeof(invalid), 64, output,
                                 sizeof(output));
    assert(appended.status == 1 && appended.code == 0 && !memcmp(output, "ab", 2) &&
           output[2] == 0xa5);
    appended = fermi_text_append((const uint8_t *)"ab", 2, input, sizeof(input), 2, output,
                                 sizeof(output));
    assert(appended.status == 1 && appended.code == 3 && !memcmp(output, "ab", 2) &&
           output[2] == 0xa5);
    fail_at = attempts + 2;
    appended = fermi_text_append((const uint8_t *)"ab", 2, input, sizeof(input), 64, output,
                                 sizeof(output));
    assert(appended.status == 1 && appended.code == 1 && !memcmp(output, "ab", 2) &&
           output[2] == 0xa5);
    fail_at = attempts + 1;
    rejected = fermi_text_roundtrip(input, sizeof(input), 64, output, sizeof(output));
    assert(rejected.status == 1 && rejected.code == 1);
    fail_at = 0;
    rejected = fermi_text_roundtrip(NULL, 0, 0, NULL, 0);
    assert(rejected.status == 0 && rejected.value == 0);
    for (unsigned slot = 0; slot < 16; slot++) {
        assert(live[slot] == NULL);
    }
    assert(attempts == releases + 2);
}

static void test_editing(void) {
    uint8_t output[32];
    memset(output, 0xa5, sizeof(output));
    unsigned before = attempts;
    fermi_text_reuse_result reused = fermi_text_reuse(output, sizeof(output));
    assert(reused.status == 0 && reused.value == 6 && !memcmp(output, "xy1234", 6));
    assert(output[6] == 0xa5 && attempts == before + 1);
    const uint8_t invalid[] = {0xc0, 0x80};
    before = attempts;
    fermi_text_replace_result result = fermi_text_replace(
        (const uint8_t *)"ab", 2, invalid, sizeof(invalid), 64, output, sizeof(output));
    assert(result.status == 1 && result.code == 0 && !memcmp(output, "ab", 2));
    assert(attempts == before + 1);
    result = fermi_text_replace((const uint8_t *)"ab", 2, (const uint8_t *)"longer", 6, 2, output,
                                sizeof(output));
    assert(result.status == 1 && result.code == 3 && !memcmp(output, "ab", 2));
    fail_at = attempts + 2;
    result = fermi_text_replace((const uint8_t *)"ab", 2, (const uint8_t *)"longer", 6, 64, output,
                                sizeof(output));
    assert(result.status == 1 && result.code == 1 && !memcmp(output, "ab", 2));
    fail_at = 0;
    const uint8_t replacement[] = {0xf0, 0x9f, 0x98, 0x80};
    for (size_t capacity = 0; capacity <= 8; capacity++) {
        memset(output, 0xa5, sizeof(output));
        result = fermi_text_replace((const uint8_t *)"before", 6, replacement, sizeof(replacement),
                                    64, output + 1, capacity);
        if (capacity < sizeof(replacement)) {
            assert(result.status == 1 && result.code == 4);
            for (size_t index = 0; index < sizeof(output); index++) {
                assert(output[index] == 0xa5);
            }
        } else {
            assert(result.status == 0 && result.value == sizeof(replacement));
            assert(output[0] == 0xa5 && !memcmp(output + 1, replacement, sizeof(replacement)));
            for (size_t index = sizeof(replacement) + 1; index < sizeof(output); index++) {
                assert(output[index] == 0xa5);
            }
        }
    }
    uint32_t random = UINT32_C(0xa4521f83);
    for (unsigned trial = 0; trial < 1000; trial++) {
        uint8_t left[16], right[16];
        for (size_t index = 0; index < 16; index++) {
            random ^= random << 13;
            random ^= random >> 17;
            random ^= random << 5;
            left[index] = (uint8_t)(random & 127);
            right[index] = (uint8_t)((random >> 8) & 127);
        }
        size_t left_length = trial % 17, right_length = (trial / 17) % 17;
        if (trial % 3 == 0) {
            memcpy(right, left, sizeof(left));
        }
        size_t common = left_length < right_length ? left_length : right_length;
        int order = memcmp(left, right, common);
        if (order == 0) {
            order = (left_length > right_length) - (left_length < right_length);
        }
        int expected = (order > 0) - (order < 0);
        fermi_text_order_result compared = fermi_text_order(left, left_length, right, right_length);
        assert(compared.status == 0 && compared.value == expected);
    }
    const uint8_t accent[] = {0xc3, 0xa9}, emoji[] = {0xf0, 0x9f, 0x98, 0x80};
    fermi_text_order_result compared =
        fermi_text_order(accent, sizeof(accent), emoji, sizeof(emoji));
    assert(compared.status == 0 && compared.value == -1);
    for (unsigned slot = 0; slot < 16; slot++) {
        assert(live[slot] == NULL);
    }
    assert(attempts == releases + 3);
}

int main(void) {
    test_validation();
    test_storage();
    test_editing();
    puts("PASS: owned text, exhaustive UTF-8 scalar encodings, byte pairs, random bytes, limits "
         "and allocation failure");
    return 0;
}
