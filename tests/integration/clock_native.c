#include "time.h"
#include <assert.h>
#include <stdio.h>

int main(void) {
    size_t previous = 0;
    for (unsigned index = 0; index < 1000; ++index) {
        fermi_now_result result = fermi_now();
        assert(result.status == 0 && result.value >= previous);
        previous = result.value;
    }
    puts("PASS: native monotonic samples do not move backward");
    return 0;
}
