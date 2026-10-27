#include "buffers.h"
#include "buffers_second.h"
#include <assert.h>
#include <stdio.h>

int main(void) {
    fermi_basic_result first = fermi_basic(64, 128);
    fermi_second_buffer_result second = fermi_second_buffer();
    assert(first.status == FERMI_OK && first.value == 64);
    assert(second.status == FERMI_OK && second.value == 2);
    fermi_second_growth_result grown = fermi_second_growth();
    assert(grown.status == FERMI_OK && grown.value == 8);
    puts("PASS: independent resource objects link through the scalar C ABI");
    return 0;
}
