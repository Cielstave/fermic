#include "unit_result.h"
#include <assert.h>
#include <stdint.h>
#include <stdio.h>

_Static_assert(sizeof(fermi_checked_result) == 2 * sizeof(uint32_t),
               "unit success must not add a dummy value field");

int main(void) {
    fermi_checked_result ok = fermi_checked(true);
    assert(ok.status == FERMI_OK && ok.code == 0);
    fermi_checked_result error = fermi_checked(false);
    assert(error.status == FERMI_DOMAIN_ERROR && error.code == FERMI_Error_rejected);
    assert(fermi_handled(true).value == 9 && fermi_handled(false).value == 7);
    assert(fermi_nested_status(true).value == 9 && fermi_nested_status(false).value == 7);
    assert(fermi_forward(true).status == FERMI_OK);
    assert(fermi_forward(false).status == FERMI_DOMAIN_ERROR);
    uint8_t output[2] = {0, 0};
    fermi_partial_result result = fermi_partial(output, sizeof(output), 0);
    assert(result.status == FERMI_OK && output[0] == 11 && output[1] == 0);
    result = fermi_partial(output, sizeof(output), 2);
    assert(result.status == FERMI_FAULT && result.code == FERMI_FAULT_BOUNDS);
    result = fermi_partial(NULL, 1, 0);
    assert(result.status == FERMI_INVALID_ARGUMENT);
    puts("PASS: unit Result ABI, forwarding, nested Results, branch state and faults");
    return 0;
}
