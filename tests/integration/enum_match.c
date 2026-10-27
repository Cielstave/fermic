#include "enum_match.h"
#include <assert.h>
#include <stdio.h>

int main(void) {
    assert(fermi_labels().status == 0 && fermi_labels().value);
    assert(fermi_initialized(true).value == 42);
    assert(fermi_initialized(false).value == 7);
    int32_t calls = 0;
    fermi_once_result result = fermi_once(&calls, 1);
    assert(result.status == 0 && result.value == 1 && calls == 1);
    result = fermi_once(NULL, 0);
    assert(result.status == 2 && result.code == 1);
    assert(fermi_nested().value == 6);
    puts("PASS: exhaustive enum matches, initialization joins, nested loops and single evaluation");
    return 0;
}
