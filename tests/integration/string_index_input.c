#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <strings.c>

int main(void) {
    const uint8_t expected[] = {65, 195, 169, 240, 159, 152, 128};
    for (size_t index = 0; index < sizeof(expected); index++) {
        fermi_string_byte_result result = fermi_string_byte(index);
        assert(result.status == 0 && result.value == expected[index]);
    }
    const size_t invalid[] = {sizeof(expected), sizeof(expected) + 1, SIZE_MAX};
    for (size_t index = 0; index < sizeof(invalid) / sizeof(invalid[0]); index++) {
        fermi_string_byte_result result = fermi_string_byte(invalid[index]);
        assert(result.status == 2 && result.code == 1);
    }
    fermi_empty_string_byte_result empty = fermi_empty_string_byte(0);
    assert(empty.status == 2 && empty.code == 1);
    empty = fermi_empty_string_byte(SIZE_MAX);
    assert(empty.status == 2 && empty.code == 1);
    puts("PASS: string byte values and bounds faults");
    return 0;
}
