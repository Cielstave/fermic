#include "bitwise.h"
#include <assert.h>
#include <stdint.h>
#include <stdio.h>

static void test_bytes(void) {
    for (unsigned a = 0; a <= UINT8_MAX; ++a) {
        assert(fermi_invert8((uint8_t)a).value == (uint8_t)~a);
        for (unsigned b = 0; b <= UINT8_MAX; ++b) {
            assert(fermi_mask8((uint8_t)a, (uint8_t)b).value == (uint8_t)(a & b));
            assert(fermi_merge8((uint8_t)a, (uint8_t)b).value == (uint8_t)(a | b));
            assert(fermi_xor8((uint8_t)a, (uint8_t)b).value == (uint8_t)(a ^ b));
        }
        for (size_t count = 0; count < 8; ++count) {
            fermi_left8_result left = fermi_left8((uint8_t)a, count);
            fermi_right8_result right = fermi_right8((uint8_t)a, count);
            assert(left.status == FERMI_OK && left.value == (uint8_t)(a << count));
            assert(right.status == FERMI_OK && right.value == (uint8_t)(a >> count));
        }
    }
}

static void test_words(void) {
    uint64_t state = UINT64_C(0xd1b54a32d192ed03);
    for (unsigned trial = 0; trial < 10000; ++trial) {
        state ^= state << 13;
        state ^= state >> 7;
        state ^= state << 17;
        size_t a = (size_t)state;
        size_t b = (size_t)(state ^ UINT64_C(0xa5a5f00f55aa9966));
        uint32_t x = (uint32_t)a, y = (uint32_t)b;
        assert(fermi_mask_size(a, b).value == (a & b));
        assert(fermi_merge_size(a, b).value == (a | b));
        assert(fermi_xor_size(a, b).value == (a ^ b));
        assert(fermi_invert_size(a).value == ~a);
        assert(fermi_mask32(x, y).value == (x & y));
        assert(fermi_merge32(x, y).value == (x | y));
        assert(fermi_xor32(x, y).value == (x ^ y));
        assert(fermi_invert32(x).value == (uint32_t)~x);
        for (size_t count = 0; count < 64; ++count) {
            fermi_left_size_result left = fermi_left_size(a, count);
            fermi_right_size_result right = fermi_right_size(a, count);
            assert(left.status == FERMI_OK && left.value == a << count);
            assert(right.status == FERMI_OK && right.value == a >> count);
            if (count < 32) {
                fermi_left32_result l32 = fermi_left32(x, count);
                fermi_right32_result r32 = fermi_right32(x, count);
                assert(l32.status == FERMI_OK && l32.value == (uint32_t)((uint64_t)x << count));
                assert(r32.status == FERMI_OK && r32.value == x >> count);
            }
        }
    }
    assert(fermi_left32(UINT32_MAX, 31).value == UINT32_C(0x80000000));
    assert(fermi_left_size(SIZE_MAX, 63).value == (size_t)1 << 63);
}

static void test_faults(void) {
    const size_t counts[] = {8, 9, 31, 32, 33, 63, 64, 65, SIZE_MAX};
    for (size_t i = 0; i < sizeof(counts) / sizeof(counts[0]); ++i) {
        size_t count = counts[i];
        fermi_left8_result l8 = fermi_left8(0, count);
        fermi_right8_result r8 = fermi_right8(255, count);
        assert(l8.status == FERMI_FAULT && l8.code == FERMI_FAULT_SHIFT);
        assert(r8.status == FERMI_FAULT && r8.code == FERMI_FAULT_SHIFT);
        if (count >= 32) {
            fermi_left32_result l32 = fermi_left32(0, count);
            fermi_right32_result r32 = fermi_right32(UINT32_MAX, count);
            assert(l32.status == FERMI_FAULT && l32.code == FERMI_FAULT_SHIFT);
            assert(r32.status == FERMI_FAULT && r32.code == FERMI_FAULT_SHIFT);
        }
        if (count >= 64) {
            fermi_left_size_result left = fermi_left_size(0, count);
            fermi_right_size_result right = fermi_right_size(SIZE_MAX, count);
            assert(left.status == FERMI_FAULT && left.code == FERMI_FAULT_SHIFT);
            assert(right.status == FERMI_FAULT && right.code == FERMI_FAULT_SHIFT);
        }
    }
    uint8_t output[2] = {0, 0};
    fermi_evaluation_result evaluation = fermi_evaluation(output, sizeof(output));
    assert(evaluation.status == FERMI_OK && evaluation.value == 5);
    assert(output[0] == 32 && output[1] == 12);
    fermi_partial_result fault = fermi_partial(output, sizeof(output), 8);
    assert(fault.status == FERMI_FAULT && fault.code == FERMI_FAULT_SHIFT && output[0] == 7);
    assert(fermi_precedence().value);
}

int main(void) {
    test_bytes();
    test_words();
    test_faults();
    puts("PASS: bitwise values, shift faults, precedence and evaluation order");
    return 0;
}
