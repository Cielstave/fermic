#include <assert.h>
#include <errno.h>
#include <stdio.h>

/* The test script inserts this counter at each internal function epilogue.
 * Observing exits catches return paths that bypass future resource cleanup. */
static unsigned exit_count;
#define write exit_test_write
#define read exit_test_read
#include "function_exits.instrumented.c"
#undef write
#undef read

static bool fail_io;
static unsigned io_calls;

ssize_t exit_test_write(int descriptor, const void *data, size_t length) {
    assert(descriptor == STDOUT_FILENO && data && length == 1);
    ++io_calls;
    if (fail_io) {
        errno = EIO;
        return -1;
    }
    return (ssize_t)length;
}

ssize_t exit_test_read(int descriptor, void *data, size_t length) {
    assert(descriptor == STDIN_FILENO && data && length == 1);
    ++io_calls;
    if (fail_io) {
        errno = EIO;
        return -1;
    }
    *(uint8_t *)data = 65;
    return 1;
}

static void expect_exits(unsigned expected) {
    assert(exit_count == expected);
    exit_count = 0;
}

int main(void) {
    fermi_select_result selected = fermi_select(true, 40, 1);
    assert(selected.status == 0 && selected.value == 41);
    expect_exits(2);
    selected = fermi_select(false, INT32_MAX, 99);
    assert(selected.status == 0 && selected.value == -1);
    expect_exits(2);
    selected = fermi_select(true, 40, 2);
    assert(selected.status == 2 && selected.code == 1 && selected.value == 0);
    expect_exits(2);
    selected = fermi_select(true, INT32_MAX, 0);
    assert(selected.status == 2 && selected.code == 2 && selected.value == 0);
    expect_exits(2);

    const int32_t values[] = {10, 20, 30};
    for (size_t i = 0; i < 3; ++i) {
        fermi_find_result found = fermi_find(values, 3, values[i]);
        assert(found.status == 0 && found.value == i);
        expect_exits(1);
    }
    assert(fermi_find(values, 3, 99).value == 3);
    expect_exits(1);
    assert(fermi_find(NULL, 0, 99).value == 0);
    expect_exits(1);

    int32_t output = 7;
    assert(fermi_update(&output, 1, true).status == 0 && output == 7);
    expect_exits(2);
    assert(fermi_update(&output, 1, false).status == 0 && output == 42);
    expect_exits(2);
    fermi_update_result updated = fermi_update(NULL, 0, false);
    assert(updated.status == 2 && updated.code == 1);
    expect_exits(2);

    int32_t first = 0, last = 0, entered = 0;
    fermi_arguments_result args = fermi_arguments(&first, 1, &last, 1, &entered, 1, 0);
    assert(args.status == 2 && args.code == 3 && args.value == 0);
    assert(first == 10 && last == 0 && entered == 0);
    expect_exits(2);
    args = fermi_arguments(&first, 1, &last, 1, &entered, 1, 2);
    assert(args.status == 0 && args.value == 43);
    assert(first == 10 && last == 30 && entered == 1);
    expect_exits(4);

    for (unsigned failure = 0; failure < 2; ++failure) {
        fail_io = failure != 0;
        io_calls = 0;
        fermi_send_result sent = fermi_send(true);
        assert(sent.status == 0 && sent.value == 0 && io_calls == 0);
        expect_exits(2);
        sent = fermi_send(false);
        assert(sent.status == failure && io_calls == 1);
        assert(failure ? sent.code == 0 : sent.value == 1);
        expect_exits(2);
        fermi_receive_result received = fermi_receive(NULL, 0);
        assert(received.status == 0 && received.value == 0 && io_calls == 1);
        expect_exits(2);
        uint8_t byte = 0;
        received = fermi_receive(&byte, 1);
        assert(received.status == failure && io_calls == 2);
        assert(failure ? received.code == 1 : received.value == 1 && byte == 65);
        expect_exits(2);
    }
    puts("PASS: single function exits, aggregate returns, fault propagation and native I/O");
    return 0;
}
