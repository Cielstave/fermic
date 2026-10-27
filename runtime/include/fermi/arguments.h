#ifndef FERMI_RUNTIME_ARGUMENTS_H
#define FERMI_RUNTIME_ARGUMENTS_H

#include <stdbool.h>
#include <stddef.h>

#ifndef FERMI_ARGUMENTS_API
#define FERMI_ARGUMENTS_API
#endif

/* Internal startup context, not a stable embedding ABI. The host retains the
 * vector and terminated byte strings until all readers finish. Initialize once
 * before publishing the context; serialize vector/string mutation with readers.
 * Output storage must not alias the context, vector or argument strings. */
typedef struct {
    size_t count;
    char *const *values;
    bool available;
} FermiArguments;

typedef enum {
    FERMI_ARGUMENTS_OK,
    FERMI_ARGUMENTS_UNAVAILABLE,
    FERMI_ARGUMENTS_OUT_OF_RANGE,
    FERMI_ARGUMENTS_OUTPUT_TOO_SMALL,
    FERMI_ARGUMENTS_INVALID_ARGUMENT
} FermiArgumentsStatus;

/* Start with a zeroed context. argc includes argv[0]; public indices exclude it.
 * argc == 0 is accepted. Invalid or repeated initialization preserves context. */
FERMI_ARGUMENTS_API bool fermi_arguments_init(FermiArguments *context, int argc,
                                              char *const argv[]);

/* Domain errors preserve output bytes and the caller's length/count slot.
 * A zero-capacity destination may be NULL. Nonempty storage must be live and
 * writable. No allocation, UTF-8 conversion, shell parsing or truncation occurs. */
FERMI_ARGUMENTS_API FermiArgumentsStatus fermi_arguments_count(const FermiArguments *context,
                                                               size_t *count);
FERMI_ARGUMENTS_API FermiArgumentsStatus fermi_arguments_read(const FermiArguments *context,
                                                              size_t index, unsigned char *output,
                                                              size_t capacity, size_t *length);

#endif
