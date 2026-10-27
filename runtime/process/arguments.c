#include "fermi/arguments.h"
#include <string.h>

FERMI_ARGUMENTS_API bool fermi_arguments_init(FermiArguments *context, int argc,
                                              char *const argv[]) {
    if (!context || context->available || argc < 0 || (argc && !argv)) {
        return false;
    }
    context->count = argc > 0 ? (size_t)argc - 1 : 0;
    context->values = argc > 1 ? argv + 1 : NULL;
    context->available = true;
    return true;
}

FERMI_ARGUMENTS_API FermiArgumentsStatus fermi_arguments_count(const FermiArguments *context,
                                                               size_t *count) {
    if (!context || !context->available) {
        return FERMI_ARGUMENTS_UNAVAILABLE;
    }
    if (!count) {
        return FERMI_ARGUMENTS_INVALID_ARGUMENT;
    }
    *count = context->count;
    return FERMI_ARGUMENTS_OK;
}

FERMI_ARGUMENTS_API FermiArgumentsStatus fermi_arguments_read(const FermiArguments *context,
                                                              size_t index, unsigned char *output,
                                                              size_t capacity, size_t *length) {
    if (!context || !context->available) {
        return FERMI_ARGUMENTS_UNAVAILABLE;
    }
    if (index >= context->count) {
        return FERMI_ARGUMENTS_OUT_OF_RANGE;
    }
    if (!length || (capacity && !output) || !context->values || !context->values[index]) {
        return FERMI_ARGUMENTS_INVALID_ARGUMENT;
    }
    const char *value = context->values[index];
    size_t count = 0;
    while (count < capacity && value[count]) {
        ++count;
    }
    if (value[count]) {
        return FERMI_ARGUMENTS_OUTPUT_TOO_SMALL;
    }
    if (count) {
        memcpy(output, value, count);
    }
    *length = count;
    return FERMI_ARGUMENTS_OK;
}
