#include "control_flow.h"
#include <assert.h>
#include <stdint.h>
#include <stdio.h>

int main(void) {
    for (int32_t value = -23; value <= 23; ++value) {
        for (int32_t divisor = -5; divisor <= 5; ++divisor) {
            fermi_quotient_result q = fermi_quotient(value, divisor);
            fermi_remainder_result r = fermi_remainder(value, divisor);
            if (!divisor) {
                assert(q.status == 2 && q.code == 3);
                assert(r.status == 2 && r.code == 3);
            } else {
                assert(q.status == 0 && q.value == value / divisor);
                assert(r.status == 0 && r.value == value % divisor);
            }
        }
    }
    assert(fermi_quotient(INT32_MIN, -1).code == 2);
    assert(fermi_remainder(INT32_MIN, -1).code == 2);
    int32_t value = 25, calls = 0;
    assert(fermi_indexed(&value, 1, &calls, 1, 2).status == 0);
    assert(value == 0 && calls == 2);
    value = 25;
    calls = 0;
    assert(fermi_indexed(&value, 1, &calls, 1, 0).code == 3);
    assert(value == 25 && calls == 1);
    assert(fermi_guarded(false, 0).value);
    assert(fermi_guarded(true, 0).code == 3);
    assert(fermi_guarded(true, 3).value);
    assert(!fermi_guarded(true, 12).value);
    assert(fermi_branches(true, 0).value == 4);
    assert(fermi_branches(true, 3).value == 10);
    assert(fermi_branches(false, 0).value == -1);
    assert(fermi_branches(false, 2).value == 0);
    assert(fermi_branches(false, 3).value == 2);
    assert(fermi_bounded(100, 3).value == 1);
    assert(fermi_signed_limits(INT16_MIN, INT32_MIN, INT64_MIN).value);
    assert(fermi_signed_limits(INT16_MAX, INT32_MAX, INT64_MAX).value);
    calls = 0;
    assert(fermi_negative_first(&calls, 1, 0).value && calls == 1);
    assert(!fermi_negative_first(&calls, 1, -1).value && calls == 2);
    fermi_literal_overflow_result overflow = fermi_literal_overflow(0);
    assert(overflow.status == 2 && overflow.code == 2);
    assert(fermi_complement(UINT8_MAX).value);
    assert(!fermi_complement(0).value);
    puts("PASS: control flow, checked compound division and evaluation order");
    return 0;
}
