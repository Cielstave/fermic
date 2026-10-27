#include "loop_control.h"
#include <assert.h>
#include <stdio.h>

int main(void) {
    const int32_t input[] = {2, -5, 3, 0, INT32_MAX};
    fermi_filtered_result filtered = fermi_filtered(input, 5);
    assert(filtered.status == FERMI_OK && filtered.value == 5);
    filtered = fermi_filtered(NULL, 0);
    assert(filtered.status == FERMI_OK && filtered.value == 0);
    const int32_t overflow[] = {INT32_MAX, 1};
    assert(fermi_filtered(overflow, 2).code == FERMI_FAULT_OVERFLOW);
    size_t index = 0;
    assert(fermi_stepped(&index, 1, 99).value == 5 && index == 5);
    index = 0;
    assert(fermi_stepped(&index, 1, 2).value == 2 && index == 2);
    index = 0;
    assert(fermi_stepped(&index, 1, 0).value == 0 && index == 0);
    assert(fermi_while_count().value == 9);
    assert(fermi_nested().value == 6);
    assert(fermi_matched().value == 3);
    puts("PASS: break/continue, for steps, while conditions, match dispatch and nested loops");
    return 0;
}
