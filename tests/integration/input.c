#include "receive.h"

#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

static unsigned calls;
static int mode;
static size_t requested;

ssize_t fermi_test_read(int fd, void *bytes, size_t length) {
    assert(fd == STDIN_FILENO);
    assert(length > 0 && length <= 4096);
    requested = length;
    ++calls;
    if (calls == 1) {
        errno = EINTR;
        return -1;
    }
    if (mode == 1) {
        return 0;
    }
    if (mode == 2 || mode == 3) {
        errno = mode == 2 ? EIO : EAGAIN;
        return -1;
    }
    size_t count = length < 3 ? length : 3;
    memcpy(bytes, "A\0B", count);
    return (ssize_t)count;
}

int main(void) {
    uint8_t buffer[8194];
    fermi_receive_result empty = fermi_receive(NULL, 0);
    assert(empty.status == FERMI_OK && empty.value == 0 && calls == 0);
    fermi_receive_result invalid = fermi_receive(NULL, 1);
    assert(invalid.status == FERMI_INVALID_ARGUMENT && calls == 0);
    for (mode = 0; mode <= 3; ++mode) {
        memset(buffer, 0xcc, sizeof(buffer));
        calls = 0;
        fermi_receive_result result = fermi_receive(buffer + 1, sizeof(buffer) - 2);
        assert(calls == 2 && requested == 4096);
        if (mode == 0) {
            assert(result.status == FERMI_OK && result.value == 3);
            assert(memcmp(buffer + 1, "A\0B", 3) == 0);
        } else if (mode == 1) {
            assert(result.status == FERMI_OK && result.value == 0);
        } else {
            assert(result.status == FERMI_DOMAIN_ERROR && result.code == 1);
        }
        assert(buffer[0] == 0xcc);
        for (size_t i = mode == 0 ? 4 : 1; i < sizeof(buffer); ++i) {
            assert(buffer[i] == 0xcc);
        }
    }
    mode = 0;
    calls = 0;
    fermi_receive_result single = fermi_receive(buffer + 1, 1);
    assert(single.status == FERMI_OK && single.value == 1 && requested == 1);
    puts("PASS: input bounds, EINTR, short reads, EOF, errors, nonblocking errors");
    return 0;
}
