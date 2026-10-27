#include "send.h"

#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

static unsigned calls;
static int failure_mode;
static size_t received;
static uint8_t input[9000];

/* Substitute only the generated library's write symbol. The host and sanitizer
 * retain their normal output functions. */
ssize_t fermi_test_write(int fd, const void *bytes, size_t length) {
    assert(fd == STDOUT_FILENO);
    assert(length > 0 && length <= 4096);
    ++calls;
    if (calls == 1) {
        errno = EINTR;
        return -1;
    }
    if (failure_mode && calls == 3) {
        errno = EIO;
        return failure_mode == 1 ? -1 : 0;
    }
    size_t count = length > 13 ? 13 : length;
    assert(received + count <= sizeof(input));
    assert(memcmp(bytes, input + received, count) == 0);
    received += count;
    return (ssize_t)count;
}

int main(void) {
    for (size_t i = 0; i < sizeof(input); ++i) {
        input[i] = (uint8_t)i;
    }
    fermi_send_result empty = fermi_send(NULL, 0);
    assert(empty.status == FERMI_OK && empty.value == 0 && calls == 0);
    fermi_send_result full = fermi_send(input, sizeof(input));
    assert(full.status == FERMI_OK && full.value == sizeof(input));
    assert(received == sizeof(input) && calls > 2);
    for (failure_mode = 1; failure_mode <= 2; ++failure_mode) {
        calls = 0;
        received = 0;
        fermi_send_result failed = fermi_send(input, sizeof(input));
        assert(failed.status == FERMI_DOMAIN_ERROR && failed.code == 0);
        assert(received == 13 && calls == 3);
    }
    puts("PASS: interrupted writes, short writes, partial failure, zero progress");
    return 0;
}
