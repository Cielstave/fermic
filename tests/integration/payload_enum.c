#include "payload_enum.h"
#include <assert.h>
#include <stdio.h>

int main(void) {
    for (unsigned kind = 0; kind < 5; ++kind) {
        for (unsigned value = 0; value <= UINT8_MAX; ++value) {
            unsigned expected = kind == 0   ? 0
                                : kind == 1 ? value
                                : kind == 2 ? value + 3
                                            : value + 5;
            fermi_dispatch_result result = fermi_dispatch((uint8_t)kind, (uint8_t)value);
            assert(result.status == FERMI_OK && result.value == expected);
            assert(fermi_duplicated((uint8_t)kind, (uint8_t)value).value == expected * 2);
            assert(fermi_replaced((uint8_t)kind, (uint8_t)value).value == expected);
            assert(fermi_branched((uint8_t)kind, (uint8_t)value).value == expected);
            fermi_unpack_result nested = fermi_unpack((uint8_t)kind, (uint8_t)value, true);
            assert(nested.status == FERMI_OK && nested.value == expected);
            nested = fermi_unpack((uint8_t)kind, (uint8_t)value, false);
            assert(nested.status == FERMI_DOMAIN_ERROR && nested.code == FERMI_Error_rejected);
            assert(fermi_stored((uint8_t)value).value == value);
        }
    }
    uint8_t marker = 0;
    fermi_once_result observed = fermi_once(&marker, 1);
    assert(observed.status == FERMI_OK && observed.value == 1 && marker == 1);
    observed = fermi_once(&marker, 0);
    assert(observed.status == FERMI_FAULT && observed.code == FERMI_FAULT_BOUNDS && marker == 1);
    puts("PASS: payload variants, exhaustive dispatch, copies, Results and single evaluation");
    return 0;
}
