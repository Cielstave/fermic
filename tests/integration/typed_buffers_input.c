#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

static void *tracked_allocate(size_t size);
static void tracked_release(void *pointer);
#define malloc tracked_allocate
#define free tracked_release
#include <typed_buffers.c>
#undef malloc
#undef free

static void *allocations[16];
static size_t attempts, releases, failures, fail_at;

static void *tracked_allocate(size_t size) {
    ++attempts;
    if (attempts == fail_at) {
        ++failures;
        return NULL;
    }
    assert(size > 0 && size <= 64);
    void *pointer = malloc(size);
    assert(pointer != NULL);
    for (size_t slot = 0; slot < 16; slot++) {
        if (allocations[slot] == NULL) {
            allocations[slot] = pointer;
            return pointer;
        }
    }
    abort();
}

static void tracked_release(void *pointer) {
    for (size_t slot = 0; slot < 16; slot++) {
        if (allocations[slot] == pointer) {
            assert(pointer != NULL);
            allocations[slot] = NULL;
            ++releases;
            free(pointer);
            return;
        }
    }
    abort();
}

static void no_live_allocations(void) {
    for (size_t slot = 0; slot < 16; slot++) {
        assert(allocations[slot] == NULL);
    }
    assert(attempts == releases + failures);
}

#define CHECK_BUFFER(name, type, first, second, third, fourth)                                     \
    do {                                                                                           \
        const type input[4] = {first, second, third, fourth};                                      \
        type output[6] = {1, 1, 1, 1, 1, 1};                                                       \
        fermi_copy_##name##_result result =                                                        \
            fermi_copy_##name(input, 4, output + 1, 4, 6 * sizeof(type));                          \
        assert(result.status == 0 && result.value == 4);                                           \
        assert(output[0] == 1 && output[5] == 1);                                                  \
        for (size_t index = 0; index < 4; index++) {                                               \
            assert(output[index + 1] == input[index]);                                             \
        }                                                                                          \
        no_live_allocations();                                                                     \
        for (size_t index = 0; index < 6; index++) {                                               \
            output[index] = 1;                                                                     \
        }                                                                                          \
        result = fermi_copy_##name(input, 4, output + 1, 4, 6 * sizeof(type) - 1);                 \
        assert(result.status == 1 && result.code == 2);                                            \
        for (size_t index = 0; index < 6; index++) {                                               \
            assert(output[index] == 1);                                                            \
        }                                                                                          \
        no_live_allocations();                                                                     \
        fail_at = attempts + 1;                                                                    \
        result = fermi_copy_##name(input, 4, output + 1, 4, 6 * sizeof(type));                     \
        assert(result.status == 1 && result.code == 0);                                            \
        no_live_allocations();                                                                     \
        fail_at = attempts + 2;                                                                    \
        result = fermi_copy_##name(input, 4, output + 1, 4, 6 * sizeof(type));                     \
        assert(result.status == 1 && result.code == 0);                                            \
        for (size_t index = 0; index < 6; index++) {                                               \
            assert(output[index] == 1);                                                            \
        }                                                                                          \
        no_live_allocations();                                                                     \
        fail_at = 0;                                                                               \
    } while (0)

int main(void) {
    CHECK_BUFFER(u8, uint8_t, 0, UINT8_MAX, 1, 7);
    CHECK_BUFFER(i8, int8_t, INT8_MIN, INT8_MAX, -1, 0);
    CHECK_BUFFER(u16, uint16_t, 0, UINT16_MAX, 1, 7);
    CHECK_BUFFER(i16, int16_t, INT16_MIN, INT16_MAX, -1, 0);
    CHECK_BUFFER(u32, uint32_t, 0, UINT32_MAX, 1, 7);
    CHECK_BUFFER(i32, int32_t, INT32_MIN, INT32_MAX, -1, 0);
    CHECK_BUFFER(u64, uint64_t, 0, UINT64_MAX, 1, 7);
    CHECK_BUFFER(i64, int64_t, INT64_MIN, INT64_MAX, -1, 0);
    CHECK_BUFFER(usize, size_t, 0, SIZE_MAX, 1, 7);
    fermi_zero_length_result empty = fermi_zero_length();
    assert(empty.status == 0 && empty.value == 0);
    no_live_allocations();
    size_t before = attempts;
    fermi_allocation_error_result overflow = fermi_allocation_error(SIZE_MAX / 8 + 1, SIZE_MAX);
    assert(overflow.status == 1 && overflow.code == 1 && attempts == before);
    overflow = fermi_allocation_error((size_t)PTRDIFF_MAX / 8 + 1, SIZE_MAX);
    assert(overflow.status == 1 && overflow.code == 1 && attempts == before);
    overflow = fermi_allocation_error(4, 31);
    assert(overflow.status == 1 && overflow.code == 2 && attempts == before);
    fail_at = attempts + 2;
    fermi_failed_growth_result failed = fermi_failed_growth();
    assert(failed.status == 1 && failed.code == 0);
    fail_at = 0;
    no_live_allocations();
    fermi_fault_cleanup_result fault = fermi_fault_cleanup(SIZE_MAX);
    assert(fault.status == 2 && fault.code == 1);
    no_live_allocations();
    fault = fermi_fault_cleanup(1);
    assert(fault.status == 0 && fault.value == 1);
    no_live_allocations();
    size_t push_attempts = attempts;
    fermi_push_values_result pushed = fermi_push_values(32, false);
    assert(pushed.status == 0 && pushed.value == 8 && attempts == push_attempts + 2);
    no_live_allocations();
    pushed = fermi_push_values(20, false);
    assert(pushed.status == 0 && pushed.value == 5);
    no_live_allocations();
    pushed = fermi_push_values(0, true);
    assert(pushed.status == 1 && pushed.code == 2);
    no_live_allocations();
    pushed = fermi_push_values(19, true);
    assert(pushed.status == 1 && pushed.code == 2);
    no_live_allocations();
    fail_at = attempts + 2;
    pushed = fermi_push_values(32, true);
    assert(pushed.status == 1 && pushed.code == 0);
    fail_at = 0;
    no_live_allocations();
    const int32_t slice[3] = {INT32_MIN, 0, INT32_MAX};
    size_t copy_attempts = attempts;
    fermi_copy_slice_result copied = fermi_copy_slice(slice, 3, 12);
    assert(copied.status == 0 && copied.value == 3 && attempts == copy_attempts + 1);
    no_live_allocations();
    copied = fermi_copy_slice(slice, 3, 11);
    assert(copied.status == 1 && copied.code == 2);
    no_live_allocations();
    fail_at = attempts + 1;
    copied = fermi_copy_slice(slice, 3, 12);
    assert(copied.status == 1 && copied.code == 0);
    fail_at = 0;
    no_live_allocations();
    copy_attempts = attempts;
    copied = fermi_copy_slice(slice, 0, 0);
    assert(copied.status == 0 && copied.value == 0 && attempts == copy_attempts);
    no_live_allocations();
    size_t extend_attempts = attempts;
    fermi_extend_values_result extended = fermi_extend_values(20);
    assert(extended.status == 0 && extended.value == 5 && attempts == extend_attempts + 2);
    no_live_allocations();
    extended = fermi_extend_values(19);
    assert(extended.status == 1 && extended.code == 2);
    no_live_allocations();
    fail_at = attempts + 2;
    extended = fermi_extend_values(20);
    assert(extended.status == 1 && extended.code == 0);
    fail_at = 0;
    no_live_allocations();
    size_t stack_attempts = attempts;
    fermi_stack_cycle_result stack = fermi_stack_cycle();
    assert(stack.status == 0 && stack.value == 9 && attempts == stack_attempts + 1);
    no_live_allocations();
    puts("PASS: nine integer buffers, exact byte limits, zero initialization, resize, allocation "
         "failures and cleanup");
    return 0;
}
