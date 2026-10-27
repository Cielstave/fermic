#include <assert.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void *tracked_allocate(size_t bytes);
static void tracked_release(void *pointer);
#define malloc tracked_allocate
#define free tracked_release
#include "buffers.c"
#undef malloc
#undef free

#define ALLOCATION_LIMIT 512
#define GUARD_SIZE 32
static struct {
    unsigned char *base;
    size_t size;
} allocations[ALLOCATION_LIMIT];
static unsigned attempts, fail_at, live, peak, released;
static size_t live_bytes, peak_bytes;
static unsigned release_order[ALLOCATION_LIMIT];

static void *tracked_allocate(size_t bytes) {
    assert(bytes > 0 && attempts < ALLOCATION_LIMIT);
    unsigned id = attempts++;
    if (attempts == fail_at) {
        return NULL;
    }
    assert(bytes <= 1024);
    unsigned char *base = malloc(bytes + 2 * GUARD_SIZE);
    assert(base);
    memset(base, 0xa5, bytes + 2 * GUARD_SIZE);
    allocations[id].base = base;
    allocations[id].size = bytes;
    ++live;
    live_bytes += bytes;
    if (live_bytes > peak_bytes) {
        peak_bytes = live_bytes;
    }
    if (live > peak) {
        peak = live;
    }
    return base + GUARD_SIZE;
}

static void tracked_release(void *pointer) {
    for (unsigned i = 0; i < attempts; ++i) {
        unsigned char *base = allocations[i].base;
        if (!base || pointer != base + GUARD_SIZE) {
            continue;
        }
        size_t bytes = allocations[i].size;
        for (unsigned n = 0; n < GUARD_SIZE; ++n) {
            assert(base[n] == 0xa5 && base[GUARD_SIZE + bytes + n] == 0xa5);
        }
        assert(live > 0 && released < ALLOCATION_LIMIT);
        --live;
        live_bytes -= bytes;
        release_order[released++] = i + 1;
        allocations[i].base = NULL;
        memset(base, 0xdd, bytes + 2 * GUARD_SIZE);
        free(base);
        return;
    }
    assert(!"release of an unknown or previously released owner");
}

static void reset(unsigned failure) {
    assert(live == 0 && live_bytes == 0);
    assert(released == attempts - (fail_at && fail_at <= attempts ? 1 : 0));
    attempts = released = peak = 0;
    peak_bytes = 0;
    fail_at = failure;
    memset(allocations, 0, sizeof(allocations));
}

static void test_byte_copies(void) {
    uint8_t source[256], output[256];
    for (size_t index = 0; index < sizeof(source); ++index) {
        source[index] = (uint8_t)index;
    }
    for (size_t length = 0; length <= sizeof(source); ++length) {
        reset(0);
        memset(output, 0xa5, sizeof(output));
        fermi_copy_bytes_result result =
            fermi_copy_bytes(source, length, output, sizeof(output), length);
        assert(result.status == 0 && result.value == length);
        assert(attempts == (length != 0) && released == attempts);
        assert(memcmp(source, output, length) == 0);
        for (size_t index = 0; index < sizeof(source); ++index) {
            assert(source[index] == (uint8_t)index);
            if (index >= length) {
                assert(output[index] == 0xa5);
            }
        }
    }
    reset(0);
    fermi_copy_bytes_result empty = fermi_copy_bytes(NULL, 0, NULL, 0, 0);
    assert(empty.status == 0 && empty.value == 0 && attempts == 0);

    for (unsigned failure = 0; failure <= 1; ++failure) {
        reset(failure);
        memset(output, 0xa5, sizeof(output));
        fermi_copy_bytes_result result =
            fermi_copy_bytes(source, sizeof(source), output, sizeof(output), failure ? 256 : 255);
        assert(result.status == 1 && result.code == (failure ? 0u : 2u));
        assert(attempts == failure && released == 0);
        for (size_t index = 0; index < sizeof(source); ++index) {
            assert(source[index] == (uint8_t)index && output[index] == 0xa5);
        }
    }
    reset(0);
    fermi_copy_bytes_result fault = fermi_copy_bytes(source, 4, NULL, 0, 4);
    assert(fault.status == 2 && fault.code == 1 && attempts == 1 && released == 1);

    for (unsigned failure = 0; failure <= 2; ++failure) {
        reset(failure);
        fermi_copy_lifetime_result result = fermi_copy_lifetime();
        assert(result.status == (failure ? 1u : 0u));
        assert(failure ? result.code == 0 : result.value == 272);
        assert(attempts == (failure ? failure : 2));
        assert(released == (failure ? failure - 1 : 2));
        if (!failure) {
            assert(peak == 2 && release_order[0] == 1 && release_order[1] == 2);
        }
    }
    reset(0);
}

static void test_growth(void) {
    for (size_t initial = 0; initial <= 16; ++initial) {
        size_t targets[] = {0, 1, initial, initial + 1, 32};
        for (size_t n = 0; n < sizeof(targets) / sizeof(targets[0]); ++n) {
            size_t target = targets[n];
            unsigned allocation_count = (initial != 0) + (target > initial);
            for (unsigned reserve = 0; reserve < 2; ++reserve) {
                for (unsigned failure = 0; failure <= 2; ++failure) {
                    reset(failure);
                    fermi_growth_result result = fermi_growth(initial, target, 64, reserve != 0);
                    bool failed = failure && failure <= allocation_count;
                    assert(result.status == (failed ? 1u : 0u));
                    assert(failed
                               ? result.code == 0
                               : result.value == (reserve && initial > target ? initial : target));
                    assert(attempts == (failed ? failure : allocation_count));
                    assert(released == attempts - (failed ? 1 : 0));
                    if (!failed) {
                        assert(peak_bytes == (target > initial ? initial + target : initial));
                    }
                }
            }
        }
    }
    size_t invalid[] = {65, SIZE_MAX};
    for (unsigned n = 0; n < 2; ++n) {
        for (unsigned reserve = 0; reserve < 2; ++reserve) {
            reset(0);
            fermi_growth_result result = fermi_growth(4, invalid[n], 64, reserve != 0);
            assert(result.status == 1 && result.code == (n ? 1u : 2u));
            assert(attempts == 1 && released == 1 && peak_bytes == 4);
        }
    }
    for (unsigned failure = 0; failure <= 2; ++failure) {
        reset(failure);
        fermi_shrink_regrow_result result = fermi_shrink_regrow();
        assert(result.status == (failure ? 1u : 0u));
        assert(failure ? result.code == 0 : result.value == 8);
        assert(attempts == (failure ? failure : 2));
        assert(released == (failure ? failure - 1 : 2));
        if (!failure) {
            assert(peak_bytes == 11);
        }
    }
    reset(0);
    fermi_growth_fault_result fault = fermi_growth_fault(0, false);
    assert(fault.status == 2 && fault.code == 3 && attempts == 1 && released == 1);
    reset(0);
    fault = fermi_growth_fault(1, true);
    assert(fault.status == 2 && fault.code == 1 && attempts == 2 && released == 2);
    assert(release_order[0] == 1 && release_order[1] == 2 && peak_bytes == 12);
    reset(2);
    fault = fermi_growth_fault(1, true);
    assert(fault.status == 0 && fault.value == 99990 && released == 1);

    for (unsigned failure = 0; failure <= 3; ++failure) {
        reset(failure);
        fermi_growth_arguments_result result = fermi_growth_arguments(1);
        assert(result.status == (failure ? 1u : 0u));
        assert(failure ? result.code == 0 : result.value == 8);
        assert(attempts == (failure ? failure : 3));
        assert(released == (failure ? failure - 1 : 3));
        if (!failure) {
            assert(peak_bytes == 13 && release_order[0] == 1 && release_order[1] == 2 &&
                   release_order[2] == 3);
        }
    }
    reset(0);
    fermi_growth_arguments_result argument = fermi_growth_arguments(0);
    assert(argument.status == 2 && argument.code == 3 && attempts == 2 && released == 2);
    assert(release_order[0] == 2 && release_order[1] == 1);

    for (unsigned failure = 0; failure <= 64; ++failure) {
        reset(failure);
        fermi_growth_loop_result result = fermi_growth_loop(64);
        assert(result.status == (failure ? 1u : 0u));
        assert(failure ? result.code == 0 : result.value == 64);
        assert(attempts == (failure ? failure : 64));
        assert(released == (failure ? failure - 1 : 64));
        if (!failure) {
            assert(peak_bytes == 127 && peak == 2);
        }
    }
    for (unsigned failure = 0; failure <= 2; ++failure) {
        reset(failure);
        fermi_growth_return_result result = fermi_growth_return();
        assert(result.status == (failure ? 1u : 0u));
        assert(failure ? result.code == 0 : result.value == 6);
        assert(attempts == (failure ? failure : 2));
        assert(released == (failure ? failure - 1 : 2));
    }
    reset(0);
}

static void test_shift_cleanup(void) {
    reset(0);
    fermi_shift_fault_result fault = fermi_shift_fault(8);
    assert(fault.status == 2 && fault.code == 4 && attempts == 1 && released == 1);
    reset(0);
    fault = fermi_shift_fault(SIZE_MAX);
    assert(fault.status == 2 && fault.code == 4 && attempts == 1 && released == 1);
    reset(0);
    fault = fermi_shift_fault(7);
    assert(fault.status == 0 && fault.value == 128 && attempts == 1 && released == 1);
    reset(1);
    fault = fermi_shift_fault(8);
    assert(fault.status == 1 && fault.code == 0 && attempts == 1 && released == 0);
    reset(0);
}

static void test_unit_cleanup(void) {
    reset(0);
    fermi_unit_owner_result result = fermi_unit_owner(false);
    assert(result.status == 0 && attempts == 1 && released == 1);
    reset(0);
    result = fermi_unit_owner(true);
    assert(result.status == 2 && result.code == 1 && attempts == 1 && released == 1);
    reset(1);
    result = fermi_unit_owner(false);
    assert(result.status == 1 && result.code == 0 && attempts == 1 && released == 0);
    reset(0);
}

int main(void) {
    test_unit_cleanup();
    test_shift_cleanup();
    test_growth();
    test_byte_copies();
    reset(0);
    fermi_basic_result basic = fermi_basic(8, 16);
    assert(basic.status == 0 && basic.value == 8 && attempts == 1 && released == 1);
    reset(1);
    basic = fermi_basic(8, 16);
    assert(basic.status == 1 && basic.code == 0 && attempts == 1);
    reset(0);
    basic = fermi_basic(0, 0);
    assert(basic.status == 0 && basic.value == 0 && attempts == 0);
    basic = fermi_basic(9, 8);
    assert(basic.status == 1 && basic.code == 2 && attempts == 0);
    basic = fermi_basic(SIZE_MAX, SIZE_MAX);
    assert(basic.status == 1 && basic.code == 1 && attempts == 0);

    for (unsigned failure = 0; failure <= 4; ++failure) {
        reset(failure);
        fermi_chain_result chain = fermi_chain();
        assert(chain.status == (failure ? 1u : 0u));
        assert(failure ? chain.code == 0 : chain.value == 10);
        assert(attempts == (failure ? failure : 4));
        assert(released == (failure ? failure - 1 : 4));
        for (unsigned i = 0; i < released; ++i) {
            assert(release_order[i] == released - i);
        }
    }
    reset(0);
    assert(fermi_scopes(64).value == 0);
    assert(attempts == 128 && released == 128 && peak == 2);
    for (unsigned i = 0; i < 128; i += 2) {
        assert(release_order[i] == i + 2 && release_order[i + 1] == i + 1);
    }
    for (unsigned failure = 1; failure <= 6; ++failure) {
        reset(failure);
        fermi_scopes_result scoped = fermi_scopes(3);
        assert(scoped.status == 0 && scoped.value == (failure % 2 ? -1 : -2));
        assert(attempts == failure && released == failure - 1);
    }
    for (unsigned create = 0; create < 2; ++create) {
        for (unsigned consume = 0; consume < 2; ++consume) {
            reset(0);
            assert(fermi_conditional(create != 0, consume != 0).value == 0);
            assert(attempts == create && released == create);
        }
    }
    for (unsigned failure = 0; failure <= 2; ++failure) {
        reset(failure);
        fermi_replacement_result result = fermi_replacement();
        assert(result.status == 0);
        assert(result.value == (failure == 1 ? 0 : failure == 2 ? 77 : 4));
        if (!failure) {
            assert(released == 2 && release_order[0] == 1 && release_order[1] == 2);
        }
    }
    reset(0);
    fermi_fault_result fault = fermi_fault(9, 1, true);
    assert(fault.status == 2 && fault.code == 1 && released == 1);
    reset(0);
    fault = fermi_fault(0, 0, false);
    assert(fault.status == 2 && fault.code == 3 && released == 1);
    reset(0);
    fault = fermi_fault(0, 3, false);
    assert(fault.status == 0 && fault.value == 3 && released == 1);
    for (int32_t divisor = 0; divisor <= 10; ++divisor) {
        reset(0);
        fermi_two_arguments_result result = fermi_two_arguments(divisor);
        bool faults = divisor == 0 || divisor == 10;
        assert(result.status == (faults ? 2u : 0u));
        assert(faults ? result.code == 3 : result.value == 9 / (9 / divisor));
        assert(released == 2 && release_order[0] == 2 && release_order[1] == 1);
    }
    reset(0);
    assert(fermi_slices(0, 0, 0).status == 0 && attempts == 0);
    assert(fermi_slices(8, 0, 8).value == 8 && released == 1);
    reset(0);
    fermi_slices_result sliced = fermi_slices(8, 2, 9);
    assert(sliced.status == 2 && sliced.code == 1 && released == 1);
    reset(0);
    for (unsigned failure = 0; failure < 2; ++failure) {
        for (int32_t divisor = 0; divisor < 2; ++divisor) {
            reset(failure);
            fermi_pending_result_result pending = fermi_pending_result(divisor);
            assert(pending.status == (divisor ? 0u : 2u));
            assert(divisor ? pending.value == (failure ? -1 : 9) : pending.code == 3);
            reset(failure);
            fermi_result_argument_result argument = fermi_result_argument(divisor);
            assert(argument.status == (divisor ? 0u : 2u));
            assert(divisor ? argument.value == (failure ? -1 : 9) : argument.code == 3);
        }
    }
    reset(0);
    fermi_loop_owner_result loop = fermi_loop_owner();
    assert(loop.status == 0 && loop.value == 0 && attempts == 2 && released == 2 && peak == 1);
    reset(0);
    puts("PASS: buffer ownership, descriptor growth, scope order, allocation failures and bounds");
    return 0;
}
