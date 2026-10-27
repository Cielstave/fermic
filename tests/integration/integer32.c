#include "integer32.h"

#include <assert.h>
#include <limits.h>
#include <stdio.h>

int main(void) {
    assert(fermi_minimum32().value == INT32_MIN);
    assert(fermi_add32(INT32_MAX, 0).value == INT32_MAX);
    assert(fermi_add32(INT32_MAX, 1).status == FERMI_FAULT);
    assert(fermi_sub32(INT32_MIN, 1).status == FERMI_FAULT);
    assert(fermi_multiply32(46340, 46340).value == 2147395600);
    assert(fermi_multiply32(46341, 46341).status == FERMI_FAULT);
    assert(fermi_divide32(INT32_MIN, -1).code == FERMI_FAULT_OVERFLOW);
    assert(fermi_remainder32(INT32_MIN, -1).code == FERMI_FAULT_OVERFLOW);
    assert(fermi_divide32(1, 0).code == FERMI_FAULT_DIV_ZERO);
    assert(fermi_divide32(-7, 3).value == -2);
    assert(fermi_remainder32(-7, 3).value == -1);
    assert(fermi_negate32(INT32_MIN).status == FERMI_FAULT);
    assert(fermi_widen32(INT32_MIN).value == INT32_MIN);
    assert(fermi_narrow32(INT32_MIN).status == FERMI_OK);
    assert(fermi_narrow32(INT32_MAX).value == INT32_MAX);
    assert(fermi_narrow32((int64_t)INT32_MAX + 1).status == FERMI_DOMAIN_ERROR);
    assert(fermi_narrow32((int64_t)INT32_MIN - 1).status == FERMI_DOMAIN_ERROR);
    puts("PASS: i32 extrema, arithmetic faults, widening and fallible narrowing");
    return 0;
}
