#include "try_result.h"
#include <assert.h>
#include <stdio.h>

static void test_values(void) {
    for (unsigned value = 0; value <= UINT8_MAX; ++value) {
        bool accepted = value != 0 && value != UINT8_MAX;
        uint32_t code = value == 0 ? FERMI_Error_rejected : FERMI_Error_exhausted;
        fermi_scalar_result scalar = fermi_scalar((uint8_t)value);
        fermi_unpack_result nested = fermi_unpack((uint8_t)value);
        fermi_explicit_move_result moved = fermi_explicit_move((uint8_t)value);
        assert(scalar.status == (accepted ? FERMI_OK : FERMI_DOMAIN_ERROR));
        assert(nested.status == scalar.status && moved.status == scalar.status);
        if (accepted) {
            assert(scalar.value == value + 1 && nested.value == value && moved.value == value);
        } else {
            assert(scalar.code == code && nested.code == code && moved.code == code);
        }
    }
}

static void test_order(void) {
    const uint8_t values[] = {0, 1, 254, 255};
    for (unsigned left = 0; left < sizeof(values); ++left) {
        for (unsigned right = 0; right < sizeof(values); ++right) {
            uint8_t marker = 0, a = values[left], b = values[right];
            fermi_ordered_result result = fermi_ordered(a, b, &marker, 1);
            if (a == 0 || a == 255) {
                assert(result.status == FERMI_DOMAIN_ERROR && marker == 1);
                assert(result.code == (a == 0 ? FERMI_Error_rejected : FERMI_Error_exhausted));
            } else if (b == 0 || b == 255) {
                assert(result.status == FERMI_DOMAIN_ERROR && marker == 2);
                assert(result.code == (b == 0 ? FERMI_Error_rejected : FERMI_Error_exhausted));
            } else {
                assert(result.status == FERMI_OK && result.value == a + b && marker == 12);
            }
        }
    }
    uint8_t marker = 0;
    fermi_ordered_result fault = fermi_ordered(0, 1, &marker, 0);
    assert(fault.status == FERMI_FAULT && fault.code == FERMI_FAULT_BOUNDS);
}

static void test_conditions(void) {
    for (unsigned gate = 0; gate < 2; ++gate) {
        for (unsigned accepted = 0; accepted < 2; ++accepted) {
            uint8_t marker = 0;
            fermi_conditional_result both = fermi_conditional(gate, accepted, &marker, 1);
            assert(marker == gate);
            assert(both.status == (gate && !accepted ? FERMI_DOMAIN_ERROR : FERMI_OK));
            if (both.status == FERMI_OK) {
                assert(both.value == (gate != 0));
            } else {
                assert(both.code == FERMI_Error_exhausted);
            }
            marker = 0;
            fermi_alternative_result either = fermi_alternative(gate, accepted, &marker, 1);
            assert(marker == !gate);
            assert(either.status == (!gate && !accepted ? FERMI_DOMAIN_ERROR : FERMI_OK));
            if (either.status == FERMI_OK) {
                assert(either.value);
            } else {
                assert(either.code == FERMI_Error_exhausted);
            }
            marker = 9;
            fermi_validate_result unit = fermi_validate(accepted, &marker, 1);
            assert(unit.status == (accepted ? FERMI_OK : FERMI_DOMAIN_ERROR));
            assert(marker == (accepted ? 42 : 9));
        }
    }
}

static void test_loops_and_assignments(void) {
    uint8_t bytes[] = {1, 2, 3, 4}, marker = 0;
    fermi_accumulate_result total = fermi_accumulate(bytes, sizeof(bytes), &marker, 1);
    assert(total.status == FERMI_OK && total.value == 10 && marker == 4);
    for (size_t index = 0; index < sizeof(bytes); ++index) {
        uint8_t saved = bytes[index];
        bytes[index] = 0;
        marker = 0;
        total = fermi_accumulate(bytes, sizeof(bytes), &marker, 1);
        assert(total.status == FERMI_DOMAIN_ERROR && total.code == FERMI_Error_rejected);
        assert(marker == index + 1);
        bytes[index] = saved;
    }
    marker = 17;
    fermi_assigned_result assigned = fermi_assigned(0, &marker, 1, 0);
    assert(assigned.status == FERMI_DOMAIN_ERROR && marker == 17);
    assigned = fermi_assigned(12, &marker, 1, 0);
    assert(assigned.status == FERMI_OK && marker == 12);
    assigned = fermi_assigned(0, &marker, 1, 1);
    assert(assigned.status == FERMI_FAULT && assigned.code == FERMI_FAULT_BOUNDS);
    assert(marker == 12);
    fermi_compound_result compound = fermi_compound(0, &marker, 1);
    assert(compound.status == FERMI_DOMAIN_ERROR && marker == 12);
    compound = fermi_compound(1, &marker, 1);
    assert(compound.status == FERMI_OK && marker == 13);
    marker = 254;
    compound = fermi_compound(2, &marker, 1);
    assert(compound.status == FERMI_FAULT && compound.code == FERMI_FAULT_OVERFLOW);
    assert(marker == 254);
}

int main(void) {
    test_values();
    test_order();
    test_conditions();
    test_loops_and_assignments();
    puts("PASS: Result propagation, exact errors, evaluation order, short circuits and faults");
    return 0;
}
