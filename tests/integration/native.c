#include "native.h"
#include <assert.h>
#include <limits.h>
#include <stdio.h>
int main(void) {
    assert(fermi_add(20, 22).value == 42);
    assert(fermi_add(INT64_MAX, 1).status == FERMI_FAULT);
    assert(fermi_add(INT64_MIN, -1).code == FERMI_FAULT_OVERFLOW);
    assert(fermi_divide(1, 0).code == FERMI_FAULT_DIV_ZERO);
    assert(fermi_divide(INT64_MIN, -1).code == FERMI_FAULT_OVERFLOW);
    assert(fermi_divide(-7, 3).value == -2);
    assert(fermi_get(NULL, 0, 0).code == FERMI_FAULT_BOUNDS);
    assert(!fermi_short_circuit(NULL, 0).value);
    assert(fermi_aggregates().value == 43);
    assert(fermi_arrays().value == 31);
    assert(fermi_convert(32767).value == 32767);
    assert(fermi_convert(32768).status == FERMI_DOMAIN_ERROR);
    assert(fermi_convert(-32769).status == FERMI_DOMAIN_ERROR);
    assert(fermi_handle(100).value == 100);
    assert(fermi_handle(INT64_MAX).value == -1);
    assert(fermi_minimum().value == INT64_MIN);
    assert(fermi_unsigned_add(255, 1).code == FERMI_FAULT_OVERFLOW);
    int16_t data[2] = {0, 0};
    assert(fermi_partial(data, 2, INT16_MAX).code == FERMI_FAULT_OVERFLOW);
    assert(data[0] == 99 && data[1] == 0);
    assert(fermi_temporary_len(data, 2).value == 2 && data[0] == 7);
    assert(fermi_temporary_index(data, 2).value == 2);
    assert(fermi_nested_struct().value == 12);
    puts("PASS: native semantics, aggregates, Result, checked faults");
    return 0;
}
