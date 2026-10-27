#include "files.h"
#include "files_second.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

int main(int argc, char **argv) {
    assert(argc == 2);
    const unsigned char *path = (const unsigned char *)argv[1];
    size_t length = strlen(argv[1]);
    assert(fermi_close_file(path, length).status == FERMI_OK);
    assert(fermi_close_second(path, length).status == FERMI_OK);
    unsigned char output[8] = {0};
    fermi_first_result first = fermi_first(path, length, output, sizeof(output));
    assert(first.status == FERMI_OK && first.value == 6 && !memcmp(output, "Fermi\n", 6));
    puts("PASS: separate file runtimes link through scalar/view ABI exports");
    return 0;
}
