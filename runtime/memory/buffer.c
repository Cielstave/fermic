#include "fermi/buffer.h"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

static void *system_allocate(void *context, size_t bytes) {
    (void)context;
    return malloc(bytes);
}

static void system_release(void *context, void *allocation) {
    (void)context;
    free(allocation);
}

static FermiMemoryStatus allocation_size(size_t count, size_t width, size_t limit, size_t *bytes) {
    if (count > SIZE_MAX / width) {
        return FERMI_MEMORY_SIZE_OVERFLOW;
    }
    *bytes = count * width;
    if (*bytes > PTRDIFF_MAX) {
        return FERMI_MEMORY_SIZE_OVERFLOW;
    }
    return *bytes > limit ? FERMI_MEMORY_LIMIT : FERMI_MEMORY_OK;
}

FermiMemoryStatus fermi_buffer_create(FermiBuffer *out, size_t length, size_t element_size,
                                      size_t limit_bytes, const FermiAllocator *allocator) {
    if (!out || !element_size || (allocator && (!allocator->allocate || !allocator->release))) {
        return FERMI_MEMORY_INVALID_ARGUMENT;
    }
    if (out->element_size) {
        return FERMI_MEMORY_INVALID_STATE;
    }
    size_t bytes;
    FermiMemoryStatus status = allocation_size(length, element_size, limit_bytes, &bytes);
    if (status != FERMI_MEMORY_OK) {
        return status;
    }
    FermiAllocator selected =
        allocator ? *allocator : (FermiAllocator){NULL, system_allocate, system_release};
    void *data = bytes ? selected.allocate(selected.context, bytes) : NULL;
    if (bytes && !data) {
        return FERMI_MEMORY_NO_MEMORY;
    }
    if (bytes) {
        memset(data, 0, bytes);
    }
    *out = (FermiBuffer){data, length, length, element_size, limit_bytes, selected};
    return FERMI_MEMORY_OK;
}

FermiMemoryStatus fermi_buffer_reserve(FermiBuffer *buffer, size_t capacity) {
    if (!buffer) {
        return FERMI_MEMORY_INVALID_ARGUMENT;
    }
    if (!buffer->element_size) {
        return FERMI_MEMORY_INVALID_STATE;
    }
    if (capacity <= buffer->capacity) {
        return FERMI_MEMORY_OK;
    }
    size_t bytes;
    FermiMemoryStatus status =
        allocation_size(capacity, buffer->element_size, buffer->limit_bytes, &bytes);
    if (status != FERMI_MEMORY_OK) {
        return status;
    }
    void *data = buffer->allocator.allocate(buffer->allocator.context, bytes);
    if (!data) {
        return FERMI_MEMORY_NO_MEMORY;
    }
    if (buffer->length) {
        memcpy(data, buffer->data, buffer->length * buffer->element_size);
    }
    void *previous = buffer->data;
    buffer->data = data;
    buffer->capacity = capacity;
    if (previous) {
        buffer->allocator.release(buffer->allocator.context, previous);
    }
    return FERMI_MEMORY_OK;
}

FermiMemoryStatus fermi_buffer_resize(FermiBuffer *buffer, size_t length) {
    if (!buffer) {
        return FERMI_MEMORY_INVALID_ARGUMENT;
    }
    if (!buffer->element_size) {
        return FERMI_MEMORY_INVALID_STATE;
    }
    FermiMemoryStatus status = fermi_buffer_reserve(buffer, length);
    if (status != FERMI_MEMORY_OK) {
        return status;
    }
    if (length > buffer->length) {
        size_t offset = buffer->length * buffer->element_size;
        size_t added = (length - buffer->length) * buffer->element_size;
        memset((unsigned char *)buffer->data + offset, 0, added);
    }
    buffer->length = length;
    return FERMI_MEMORY_OK;
}

FermiMemoryStatus fermi_buffer_truncate(FermiBuffer *buffer, size_t length) {
    if (!buffer) {
        return FERMI_MEMORY_INVALID_ARGUMENT;
    }
    if (!buffer->element_size) {
        return FERMI_MEMORY_INVALID_STATE;
    }
    if (length > buffer->length) {
        return FERMI_MEMORY_INVALID_ARGUMENT;
    }
    buffer->length = length;
    return FERMI_MEMORY_OK;
}

FermiMemoryStatus fermi_buffer_move(FermiBuffer *destination, FermiBuffer *source) {
    if (!destination || !source || destination == source) {
        return FERMI_MEMORY_INVALID_ARGUMENT;
    }
    if (destination->element_size || !source->element_size) {
        return FERMI_MEMORY_INVALID_STATE;
    }
    *destination = *source;
    *source = (FermiBuffer){0};
    return FERMI_MEMORY_OK;
}

void fermi_buffer_drop(FermiBuffer *buffer) {
    if (!buffer) {
        return;
    }
    void *data = buffer->data;
    FermiAllocator allocator = buffer->allocator;
    *buffer = (FermiBuffer){0};
    if (data) {
        allocator.release(allocator.context, data);
    }
}
