#include "integer_types.h"
#include <assert.h>
#include <limits.h>
#include <stdio.h>

#define CHECK_I8(operation, expression)                                                            \
    do {                                                                                           \
        fermi_##operation##_i8_result result = fermi_##operation##_i8(left, right);                \
        int expected = (expression);                                                               \
        if (expected < INT8_MIN || expected > INT8_MAX) {                                          \
            assert(result.status == FERMI_FAULT && result.code == FERMI_FAULT_OVERFLOW);           \
        } else {                                                                                   \
            assert(result.status == FERMI_OK && result.value == expected);                         \
        }                                                                                          \
    } while (0)

static void check_signed_byte(void) {
    for (int left = INT8_MIN; left <= INT8_MAX; ++left) {
        for (int right = INT8_MIN; right <= INT8_MAX; ++right) {
            CHECK_I8(add, left + right);
            CHECK_I8(subtract, left - right);
            CHECK_I8(multiply, left * right);
            fermi_divide_i8_result quotient = fermi_divide_i8(left, right);
            fermi_remainder_i8_result remainder = fermi_remainder_i8(left, right);
            if (!right) {
                assert(quotient.status == FERMI_FAULT && quotient.code == FERMI_FAULT_DIV_ZERO);
                assert(remainder.status == FERMI_FAULT && remainder.code == FERMI_FAULT_DIV_ZERO);
            } else if (left == INT8_MIN && right == -1) {
                assert(quotient.status == FERMI_FAULT && quotient.code == FERMI_FAULT_OVERFLOW);
                assert(remainder.status == FERMI_FAULT && remainder.code == FERMI_FAULT_OVERFLOW);
            } else {
                assert(quotient.status == FERMI_OK && quotient.value == left / right);
                assert(remainder.status == FERMI_OK && remainder.value == left % right);
            }
        }
        fermi_negate_i8_result negated = fermi_negate_i8(left);
        if (left == INT8_MIN) {
            assert(negated.status == FERMI_FAULT && negated.code == FERMI_FAULT_OVERFLOW);
        } else {
            assert(negated.status == FERMI_OK && negated.value == -left);
        }
    }
}

#define CHECK_UNSIGNED(type, operation, fault, expression)                                         \
    do {                                                                                           \
        fermi_##operation##_##type##_result result = fermi_##operation##_##type(left, right);      \
        if (fault) {                                                                               \
            assert(result.status == FERMI_FAULT && result.code == (fault));                        \
        } else {                                                                                   \
            assert(result.status == FERMI_OK && result.value == (expression));                     \
        }                                                                                          \
    } while (0)

#define CHECK_PAIR(type, maximum)                                                                  \
    do {                                                                                           \
        CHECK_UNSIGNED(type, add, left > (maximum) - right ? 2 : 0, left + right);                 \
        CHECK_UNSIGNED(type, subtract, left < right ? 2 : 0, left - right);                        \
        CHECK_UNSIGNED(type, multiply, right && left > (maximum) / right ? 2 : 0, left * right);   \
        CHECK_UNSIGNED(type, divide, right ? 0 : 3, left / right);                                 \
        CHECK_UNSIGNED(type, remainder, right ? 0 : 3, left % right);                              \
    } while (0)

static void check_unsigned(void) {
    const uint64_t words[] = {0, 1, 255, 256, 32768, UINT16_MAX - 1, UINT16_MAX};
    const uint64_t wide[] = {0, 1, 2, UINT32_MAX, UINT64_C(1) << 63, UINT64_MAX - 1, UINT64_MAX};
    for (size_t i = 0; i < sizeof(words) / sizeof(words[0]); ++i) {
        for (size_t j = 0; j < sizeof(words) / sizeof(words[0]); ++j) {
            uint64_t left = words[i], right = words[j];
            CHECK_PAIR(u16, UINT16_MAX);
        }
    }
    for (size_t i = 0; i < sizeof(wide) / sizeof(wide[0]); ++i) {
        for (size_t j = 0; j < sizeof(wide) / sizeof(wide[0]); ++j) {
            uint64_t left = wide[i], right = wide[j];
            CHECK_PAIR(u64, UINT64_MAX);
        }
    }
}

int main(void) {
    check_signed_byte();
    check_unsigned();
    assert(fermi_minimum_i8().value == INT8_MIN);
    assert(fermi_maximum_u16().value == UINT16_MAX);
    assert(fermi_maximum_u64().value == UINT64_MAX);
    assert(fermi_shift_u16(1, 15).value == 32768);
    assert(fermi_shift_u16(UINT16_MAX, 1).value == 65534);
    assert(fermi_shift_u16(1, 16).code == FERMI_FAULT_SHIFT);
    assert(fermi_shift_u64(UINT64_MAX, 63).value == 1);
    assert(fermi_shift_u64(1, 64).code == FERMI_FAULT_SHIFT);
    assert(fermi_complement_u64(0).value == UINT64_MAX);
    assert(fermi_widen_i8(INT8_MIN).value == INT8_MIN);
    assert(fermi_widen_u16(UINT16_MAX).value == UINT16_MAX);
    assert(fermi_widen_u64(UINT64_MAX).value == SIZE_MAX);
    assert(fermi_narrow_signed_i8(INT8_MIN).value == INT8_MIN);
    assert(fermi_narrow_signed_i8(INT8_MAX).value == INT8_MAX);
    assert(fermi_narrow_signed_i8(INT8_MIN - 1).status == FERMI_DOMAIN_ERROR);
    assert(fermi_narrow_signed_i8(INT8_MAX + 1).status == FERMI_DOMAIN_ERROR);
    assert(fermi_narrow_i8(INT8_MAX).value == INT8_MAX);
    assert(fermi_narrow_i8(INT8_MAX + 1).status == FERMI_DOMAIN_ERROR);
    assert(fermi_narrow_i8(UINT64_MAX).status == FERMI_DOMAIN_ERROR);
    assert(fermi_narrow_u16(UINT16_MAX).value == UINT16_MAX);
    assert(fermi_narrow_u16(-1).status == FERMI_DOMAIN_ERROR);
    assert(fermi_narrow_u16(UINT16_MAX + 1).status == FERMI_DOMAIN_ERROR);
    assert(fermi_narrow_u64(INT64_MAX).value == INT64_MAX);
    assert(fermi_narrow_u64(-1).status == FERMI_DOMAIN_ERROR);
    uint16_t values[] = {100, UINT16_MAX};
    assert(fermi_edit_u16(values, 2, 0, 20).value == 120 && values[0] == 120);
    assert(fermi_edit_u16(values, 2, 1, 1).code == FERMI_FAULT_OVERFLOW);
    assert(values[1] == UINT16_MAX);
    assert(fermi_edit_u16(values, 2, 2, 0).code == FERMI_FAULT_BOUNDS);
    assert(fermi_aggregates(UINT64_MAX).value == UINT64_MAX);
    assert(fermi_count_u16().value == 5);
    puts("PASS: i8 exhaustive arithmetic, u16/u64 boundaries, conversions, views and aggregates");
    return 0;
}
