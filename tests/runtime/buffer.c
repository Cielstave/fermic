#include "fermi/buffer.h"

#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum { GUARD = sizeof(max_align_t), ALLOCATIONS = 16 };
typedef struct {
    unsigned char *raw;
    size_t bytes;
} Allocation;
typedef struct {
    Allocation blocks[ALLOCATIONS];
    size_t attempts, fail_at, acquired, released, live_bytes, peak_bytes;
} Heap;

static void *allocate(void *context, size_t bytes) {
    Heap *heap = context;
    assert(bytes && bytes <= 4096);
    if (++heap->attempts == heap->fail_at) {
        return NULL;
    }
    for (unsigned i = 0; i < ALLOCATIONS; ++i) {
        Allocation *block = &heap->blocks[i];
        if (block->raw) {
            continue;
        }
        block->raw = malloc(bytes + 2 * GUARD);
        assert(block->raw);
        block->bytes = bytes;
        memset(block->raw, 0xd3, bytes + 2 * GUARD);
        memset(block->raw + GUARD, 0xa5, bytes);
        ++heap->acquired;
        heap->live_bytes += bytes;
        if (heap->live_bytes > heap->peak_bytes) {
            heap->peak_bytes = heap->live_bytes;
        }
        return block->raw + GUARD;
    }
    assert(!"test allocator exhausted");
    return NULL;
}

static void release(void *context, void *data) {
    Heap *heap = context;
    for (unsigned i = 0; i < ALLOCATIONS; ++i) {
        Allocation *block = &heap->blocks[i];
        if (!block->raw || data != block->raw + GUARD) {
            continue;
        }
        for (size_t j = 0; j < GUARD; ++j) {
            assert(block->raw[j] == 0xd3);
            assert(block->raw[GUARD + block->bytes + j] == 0xd3);
        }
        heap->live_bytes -= block->bytes;
        ++heap->released;
        free(block->raw);
        *block = (Allocation){0};
        return;
    }
    assert(!"release through wrong allocator or duplicate release");
}

static FermiAllocator allocator_for(Heap *heap) {
    return (FermiAllocator){heap, allocate, release};
}

static void assert_vacant(const FermiBuffer *buffer) {
    assert(!buffer->data && !buffer->length && !buffer->capacity && !buffer->element_size);
    assert(!buffer->limit_bytes && !buffer->allocator.context && !buffer->allocator.allocate &&
           !buffer->allocator.release);
}

static void assert_same(const FermiBuffer *buffer, const FermiBuffer *snapshot) {
    assert(buffer->data == snapshot->data && buffer->length == snapshot->length &&
           buffer->capacity == snapshot->capacity &&
           buffer->element_size == snapshot->element_size &&
           buffer->limit_bytes == snapshot->limit_bytes);
    assert(buffer->allocator.context == snapshot->allocator.context &&
           buffer->allocator.allocate == snapshot->allocator.allocate &&
           buffer->allocator.release == snapshot->allocator.release);
}

static void assert_clean(const Heap *heap) {
    assert(!heap->live_bytes && heap->acquired == heap->released);
    for (unsigned i = 0; i < ALLOCATIONS; ++i) {
        assert(!heap->blocks[i].raw);
    }
}

static void scalar_widths(void) {
    for (size_t width = 1; width <= 8; width *= 2) {
        Heap heap = {0};
        FermiAllocator allocator = allocator_for(&heap);
        FermiBuffer buffer = {0};
        assert(fermi_buffer_create(&buffer, 4, width, 64, &allocator) == FERMI_MEMORY_OK);
        unsigned char *data = buffer.data;
        for (size_t i = 0; i < 4 * width; ++i) {
            assert(!data[i]);
            data[i] = (unsigned char)(i + 1);
        }
        assert(fermi_buffer_reserve(&buffer, 8) == FERMI_MEMORY_OK);
        assert(buffer.length == 4 && buffer.capacity == 8);
        data = buffer.data;
        for (size_t i = 0; i < 4 * width; ++i) {
            assert(data[i] == i + 1);
        }
        assert(fermi_buffer_resize(&buffer, 8) == FERMI_MEMORY_OK);
        for (size_t i = 4 * width; i < 8 * width; ++i) {
            assert(!data[i]);
        }
        memset(data, 0x77, 8 * width);
        assert(fermi_buffer_resize(&buffer, 1) == FERMI_MEMORY_OK);
        assert(fermi_buffer_resize(&buffer, 8) == FERMI_MEMORY_OK);
        for (size_t i = 0; i < 8 * width; ++i) {
            assert(data[i] == (i < width ? 0x77 : 0));
        }
        assert(heap.attempts == 2 && heap.peak_bytes == 12 * width);
        assert(fermi_buffer_resize(&buffer, 0) == FERMI_MEMORY_OK);
        assert(buffer.data && buffer.capacity == 8);
        fermi_buffer_drop(&buffer);
        fermi_buffer_drop(&buffer);
        assert_vacant(&buffer);
        assert_clean(&heap);
    }
    FermiBuffer system = {0};
    assert(fermi_buffer_create(&system, 2, sizeof(uint64_t), 128, NULL) == FERMI_MEMORY_OK);
    ((uint64_t *)system.data)[0] = UINT64_MAX;
    assert(fermi_buffer_resize(&system, 16) == FERMI_MEMORY_OK);
    assert(((uint64_t *)system.data)[0] == UINT64_MAX && !((uint64_t *)system.data)[15]);
    fermi_buffer_drop(&system);
}

static void record_widths(void) {
    const size_t widths[] = {3, 12, 24, 40};
    for (size_t index = 0; index < sizeof(widths) / sizeof(widths[0]); ++index) {
        size_t width = widths[index];
        Heap heap = {0};
        FermiAllocator allocator = allocator_for(&heap);
        FermiBuffer buffer = {0};
        assert(fermi_buffer_create(&buffer, 2, width, 8 * width, &allocator) == FERMI_MEMORY_OK);
        memset(buffer.data, 0x73, 2 * width);
        assert(fermi_buffer_reserve(&buffer, 8) == FERMI_MEMORY_OK);
        for (size_t byte = 0; byte < 2 * width; ++byte) {
            assert(((unsigned char *)buffer.data)[byte] == 0x73);
        }
        size_t attempts = heap.attempts;
        assert(fermi_buffer_reserve(&buffer, 9) == FERMI_MEMORY_LIMIT);
        assert(heap.attempts == attempts && buffer.length == 2 && buffer.capacity == 8);
        fermi_buffer_drop(&buffer);
        assert(!heap.live_bytes && heap.acquired == heap.released);
    }
}

static void failures(void) {
    Heap heap = {0};
    FermiAllocator allocator = allocator_for(&heap);
    FermiBuffer buffer = {0};
    assert(fermi_buffer_create(&buffer, 4, 0, 64, &allocator) == FERMI_MEMORY_INVALID_ARGUMENT);
    assert(fermi_buffer_create(&buffer, SIZE_MAX, 8, SIZE_MAX, &allocator) ==
           FERMI_MEMORY_SIZE_OVERFLOW);
    assert(fermi_buffer_create(&buffer, (size_t)PTRDIFF_MAX + 1, 1, SIZE_MAX, &allocator) ==
           FERMI_MEMORY_SIZE_OVERFLOW);
    assert(fermi_buffer_create(&buffer, 9, 8, 64, &allocator) == FERMI_MEMORY_LIMIT);
    assert_vacant(&buffer);
    assert(!heap.attempts);
    heap.fail_at = 1;
    assert(fermi_buffer_create(&buffer, 4, 2, 64, &allocator) == FERMI_MEMORY_NO_MEMORY);
    assert_vacant(&buffer);
    assert(fermi_buffer_create(&buffer, 4, 2, 64, &allocator) == FERMI_MEMORY_OK);
    memset(buffer.data, 0x42, 8);
    /* A non-owning descriptor snapshot is used only to compare failure state. */
    FermiBuffer snapshot = buffer;
    heap.fail_at = heap.attempts + 1;
    assert(fermi_buffer_reserve(&buffer, 32) == FERMI_MEMORY_NO_MEMORY);
    assert_same(&buffer, &snapshot);
    heap.fail_at = heap.attempts + 1;
    assert(fermi_buffer_resize(&buffer, 32) == FERMI_MEMORY_NO_MEMORY);
    assert_same(&buffer, &snapshot);
    assert(fermi_buffer_reserve(&buffer, 33) == FERMI_MEMORY_LIMIT);
    assert(fermi_buffer_resize(&buffer, SIZE_MAX) == FERMI_MEMORY_SIZE_OVERFLOW);
    assert_same(&buffer, &snapshot);
    for (size_t i = 0; i < 8; ++i) {
        assert(((unsigned char *)buffer.data)[i] == 0x42);
    }
    assert(fermi_buffer_create(&buffer, 1, 1, 8, &allocator) == FERMI_MEMORY_INVALID_STATE);
    assert_same(&buffer, &snapshot);
    fermi_buffer_drop(&buffer);
    assert_clean(&heap);
}

static void transfers(void) {
    Heap first = {0}, second = {0};
    FermiAllocator a = allocator_for(&first), b = allocator_for(&second);
    FermiBuffer source = {0}, destination = {0};
    assert(fermi_buffer_create(&source, 2, 1, 32, &a) == FERMI_MEMORY_OK);
    assert(fermi_buffer_create(&destination, 3, 2, 64, &b) == FERMI_MEMORY_OK);
    FermiBuffer source_snapshot = source, destination_snapshot = destination;
    assert(fermi_buffer_move(&destination, &source) == FERMI_MEMORY_INVALID_STATE);
    assert_same(&source, &source_snapshot);
    assert_same(&destination, &destination_snapshot);
    assert(fermi_buffer_move(&source, &source) == FERMI_MEMORY_INVALID_ARGUMENT);
    fermi_buffer_drop(&destination);
    assert(fermi_buffer_move(&destination, &source) == FERMI_MEMORY_OK);
    assert_vacant(&source);
    assert_same(&destination, &source_snapshot);
    assert(fermi_buffer_reserve(&source, 4) == FERMI_MEMORY_INVALID_STATE);
    fermi_buffer_drop(&source);
    assert(fermi_buffer_resize(&destination, 16) == FERMI_MEMORY_OK);
    assert(first.attempts == 2 && second.attempts == 1);
    fermi_buffer_drop(&destination);
    assert_clean(&first);
    assert_clean(&second);

    assert(fermi_buffer_create(&source, 0, 8, 0, &a) == FERMI_MEMORY_OK);
    assert(!source.data && source.element_size == 8);
    assert(fermi_buffer_resize(&source, 0) == FERMI_MEMORY_OK);
    assert(fermi_buffer_move(&destination, &source) == FERMI_MEMORY_OK);
    assert(fermi_buffer_resize(&destination, 1) == FERMI_MEMORY_LIMIT);
    fermi_buffer_drop(&destination);
    assert_clean(&first);
}

static void fail_each_allocation(void) {
    /* Re-run one ownership chain, failing each allocation site in turn. Cleanup
     * must release only allocations that actually succeeded, even after a move. */
    for (size_t fail_at = 1; fail_at <= 5; ++fail_at) {
        Heap heap = {.fail_at = fail_at};
        FermiAllocator allocator = allocator_for(&heap);
        FermiBuffer a = {0}, b = {0};
        if (fermi_buffer_create(&a, 2, 4, 128, &allocator) != FERMI_MEMORY_OK) {
            goto cleanup;
        }
        if (fermi_buffer_reserve(&a, 4) != FERMI_MEMORY_OK) {
            goto cleanup;
        }
        assert(fermi_buffer_move(&b, &a) == FERMI_MEMORY_OK);
        if (fermi_buffer_resize(&b, 8) != FERMI_MEMORY_OK) {
            goto cleanup;
        }
        if (fermi_buffer_resize(&b, 16) != FERMI_MEMORY_OK) {
            goto cleanup;
        }
        assert(fail_at == 5);
    cleanup:
        fermi_buffer_drop(&b);
        fermi_buffer_drop(&a);
        assert_clean(&heap);
        assert(heap.attempts == (fail_at < 5 ? fail_at : 4));
    }
}

static void operation_sequences(void) {
    Heap heap = {0};
    FermiAllocator allocator = allocator_for(&heap);
    FermiBuffer slots[2] = {{0}, {0}};
    unsigned active = 0;
    unsigned char model[2048] = {0};
    size_t length = 0;
    uint32_t random = 29;
    assert(fermi_buffer_create(&slots[active], 0, 8, sizeof(model), &allocator) == FERMI_MEMORY_OK);
    for (unsigned step = 0; step < 10000; ++step) {
        random = random * 1664525u + 1013904223u;
        unsigned operation = (random >> 20) % 4;
        size_t requested = (random >> 8) % 300;
        FermiBuffer *buffer = &slots[active];
        FermiBuffer snapshot = *buffer;
        heap.fail_at = step % 7 == 0 ? heap.attempts + 1 : 0;
        if (operation < 2) {
            FermiMemoryStatus status = operation == 0 ? fermi_buffer_resize(buffer, requested)
                                                      : fermi_buffer_reserve(buffer, requested);
            if (status == FERMI_MEMORY_OK) {
                if (operation == 0) {
                    if (requested > length) {
                        memset(model + length * 8, 0, (requested - length) * 8);
                    }
                    length = requested;
                }
            } else {
                assert(status == FERMI_MEMORY_NO_MEMORY || status == FERMI_MEMORY_LIMIT);
                assert_same(buffer, &snapshot);
            }
        } else if (operation == 2 && length) {
            size_t byte = requested % (length * 8);
            model[byte] = (unsigned char)random;
            ((unsigned char *)buffer->data)[byte] = model[byte];
        } else if (operation == 3) {
            assert(fermi_buffer_move(&slots[active ^ 1], buffer) == FERMI_MEMORY_OK);
            assert_vacant(buffer);
            active ^= 1;
            buffer = &slots[active];
        }
        assert(buffer->length == length && buffer->capacity >= length);
        assert(buffer->capacity <= sizeof(model) / 8);
        if (length) {
            assert(memcmp(model, buffer->data, length * 8) == 0);
        }
    }
    fermi_buffer_drop(&slots[0]);
    fermi_buffer_drop(&slots[1]);
    assert_clean(&heap);
}

static void invalid_arguments(void) {
    FermiBuffer vacant = {0};
    FermiAllocator invalid = {0};
    assert(fermi_buffer_create(NULL, 0, 1, 0, NULL) == FERMI_MEMORY_INVALID_ARGUMENT);
    assert(fermi_buffer_create(&vacant, 0, 1, 0, &invalid) == FERMI_MEMORY_INVALID_ARGUMENT);
    assert(fermi_buffer_reserve(NULL, 0) == FERMI_MEMORY_INVALID_ARGUMENT);
    assert(fermi_buffer_resize(NULL, 0) == FERMI_MEMORY_INVALID_ARGUMENT);
    assert(fermi_buffer_resize(&vacant, 0) == FERMI_MEMORY_INVALID_STATE);
    assert(fermi_buffer_move(NULL, &vacant) == FERMI_MEMORY_INVALID_ARGUMENT);
    fermi_buffer_drop(NULL);
    assert_vacant(&vacant);
}

static void truncateStates(void) {
    Heap heap = {0};
    FermiAllocator allocator = allocator_for(&heap);
    FermiBuffer buffer = {0};
    assert(fermi_buffer_truncate(NULL, 0) == FERMI_MEMORY_INVALID_ARGUMENT);
    assert(fermi_buffer_truncate(&buffer, 0) == FERMI_MEMORY_INVALID_STATE);
    assert(fermi_buffer_create(&buffer, 4, 8, 64, &allocator) == FERMI_MEMORY_OK);
    memset(buffer.data, 0x73, 32);
    FermiBuffer snapshot = buffer;
    heap.fail_at = heap.attempts + 1;
    assert(fermi_buffer_truncate(&buffer, SIZE_MAX) == FERMI_MEMORY_INVALID_ARGUMENT);
    assert_same(&buffer, &snapshot);
    assert(fermi_buffer_truncate(&buffer, 2) == FERMI_MEMORY_OK);
    assert(buffer.length == 2 && buffer.data == snapshot.data && buffer.capacity == 4);
    assert(fermi_buffer_truncate(&buffer, 3) == FERMI_MEMORY_INVALID_ARGUMENT);
    assert(buffer.length == 2);
    assert(fermi_buffer_truncate(&buffer, 0) == FERMI_MEMORY_OK);
    assert(fermi_buffer_truncate(&buffer, 0) == FERMI_MEMORY_OK);
    for (size_t i = 0; i < 32; ++i) {
        assert(((unsigned char *)buffer.data)[i] == 0x73);
    }
    assert(fermi_buffer_resize(&buffer, 4) == FERMI_MEMORY_OK);
    for (size_t i = 0; i < 32; ++i) {
        assert(((unsigned char *)buffer.data)[i] == 0);
    }
    assert(heap.attempts == 1 && !heap.released);
    fermi_buffer_drop(&buffer);
    assert_clean(&heap);
    assert(fermi_buffer_truncate(&buffer, 0) == FERMI_MEMORY_INVALID_STATE);
}

int main(void) {
    truncateStates();
    scalar_widths();
    record_widths();
    failures();
    transfers();
    fail_each_allocation();
    operation_sequences();
    invalid_arguments();
    puts("PASS: scalar buffers, allocation failure, size limits, move, zeroing and exact release");
    return 0;
}
