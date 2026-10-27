#ifndef FERMI_RUNTIME_BUFFER_H
#define FERMI_RUNTIME_BUFFER_H

#include <stddef.h>

#ifndef FERMI_BUFFER_API
#define FERMI_BUFFER_API
#endif

/* Internal runtime interface, not a stable public C ABI. Allocators must return
 * distinct, malloc-aligned storage or NULL, and must not reenter the same owner.
 * The context must outlive every buffer using it; release must not fail. */
typedef struct {
    void *context;
    void *(*allocate)(void *context, size_t bytes);
    void (*release)(void *context, void *allocation);
} FermiAllocator;

typedef enum {
    FERMI_MEMORY_OK,
    FERMI_MEMORY_NO_MEMORY,
    FERMI_MEMORY_SIZE_OVERFLOW,
    FERMI_MEMORY_LIMIT,
    FERMI_MEMORY_INVALID_ARGUMENT,
    FERMI_MEMORY_INVALID_STATE
} FermiMemoryStatus;

/* Initialize each slot with {0}. Never copy this struct: transfer ownership with
 * fermi_buffer_move. Only [0, length) elements are initialized and accessible.
 * A zero element_size identifies a vacant slot, including a moved-from slot.
 * Descriptor fields are private to the runtime and future generated code. */
typedef struct {
    void *data;
    size_t length, capacity, element_size, limit_bytes;
    FermiAllocator allocator;
} FermiBuffer;

/* Width must be positive. This raw byte layer zero-initializes live storage;
 * zero bytes alone do not construct arbitrary typed records. Generated record
 * buffers start empty and use typed seed assignment before publishing a length.
 * NULL allocator selects malloc/free. Failure leaves the vacant slot unchanged.
 * limit_bytes bounds capacity bytes, not allocator overhead or process RSS. */
FERMI_BUFFER_API FermiMemoryStatus fermi_buffer_create(FermiBuffer *out, size_t length,
                                                       size_t element_size, size_t limit_bytes,
                                                       const FermiAllocator *allocator);

/* Capacity grows to exactly the requested element count; no implicit doubling.
 * Growth allocates before releasing old storage, so transient payload memory can
 * reach old capacity + new capacity. Failure preserves all old owner state/data.
 * Successful growth invalidates views. Reserve does not change logical length. */
FERMI_BUFFER_API FermiMemoryStatus fermi_buffer_reserve(FermiBuffer *buffer, size_t capacity);

/* Newly exposed elements are zeroed, including after shrink/regrow. Shrinking
 * retains capacity and does not erase confidential data. No secure wipe is offered. */
FERMI_BUFFER_API FermiMemoryStatus fermi_buffer_resize(FermiBuffer *buffer, size_t length);

/* Shorten logical length without accessing elements or calling the allocator.
 * Growth is rejected without mutation. Capacity and retained bytes are unchanged. */
FERMI_BUFFER_API FermiMemoryStatus fermi_buffer_truncate(FermiBuffer *buffer, size_t length);

/* Destination must be vacant and distinct. Transfer does not allocate; allocator
 * identity and limits travel with the owner. Failure changes neither slot. */
FERMI_BUFFER_API FermiMemoryStatus fermi_buffer_move(FermiBuffer *destination, FermiBuffer *source);

/* Release and vacate the slot. Vacant slots and NULL are harmless. This function
 * does not make arbitrary shallow copies or concurrent owner access safe. */
FERMI_BUFFER_API void fermi_buffer_drop(FermiBuffer *buffer);

#endif
