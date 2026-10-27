#include "fermi/fe.h"
#include <stdalign.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>

struct FeArena {
    FeArena *next;
    size_t used, capacity;
    max_align_t data[];
};

bool fe_error(FeContext *context, FeSpan span, const char *code, const char *format, ...) {
    if (!context->diagnostic.code) {
        context->diagnostic.code = code;
        context->diagnostic.span = span;
        context->diagnostic.end = span;
        context->diagnostic.severity =
            !strncmp(code, "E_INTERNAL", 10) ? FE_DIAGNOSTIC_INTERNAL : FE_DIAGNOSTIC_ERROR;
        if (context->source && span.offset < context->length) {
            size_t end = span.offset;
            while (end < context->length) {
                unsigned char ch = (unsigned char)context->source[end];
                if (!((ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z') ||
                      (ch >= '0' && ch <= '9') || ch == '_')) {
                    break;
                }
                ++end;
            }
            if (end == span.offset && context->source[end] != '\n') {
                ++end;
            }
            context->diagnostic.end.offset = end;
            context->diagnostic.end.column += (unsigned)(end - span.offset);
        }
        va_list args;
        va_start(args, format);
        vsnprintf(context->diagnostic.message, sizeof(context->diagnostic.message), format, args);
        va_end(args);
    }
    return false;
}

bool fe_context_init(FeContext *context, const char *path, const char *source, size_t length) {
    memset(context, 0, sizeof(*context));
    context->path = path;
    context->source = source;
    if (length > FE_SOURCE_LIMIT) {
        return fe_error(context, (FeSpan){0, 1, 1}, "E_LIMIT", "source exceeds 1 MiB");
    }
    context->length = length;
    return true;
}

void *fe_alloc(FeContext *context, size_t size) {
    FeContext *budget = context->budget_owner ? context->budget_owner : context;
    size_t alignment = alignof(max_align_t);
    if (size > FE_MEMORY_LIMIT) {
        goto exhausted;
    }
    size = (size + alignment - 1) / alignment * alignment;
    if (!size) {
        size = alignment;
    }
    if (!context->arena || size > context->arena->capacity - context->arena->used) {
        size_t capacity = size > 65536 ? size : 65536;
        if (capacity + sizeof(FeArena) > FE_MEMORY_LIMIT - budget->allocated) {
            goto exhausted;
        }
        FeArena *arena = calloc(1, sizeof(*arena) + capacity);
        if (!arena) {
            goto exhausted;
        }
        arena->capacity = capacity;
        arena->next = context->arena;
        context->arena = arena;
        budget->allocated += capacity + sizeof(*arena);
    }
    void *p = (unsigned char *)context->arena->data + context->arena->used;
    context->arena->used += size;
    return p;
exhausted:
    fe_error(context, (FeSpan){0, 1, 1}, "E_LIMIT", "compiler memory limit exceeded");
    return NULL;
}

char *fe_string(FeContext *context, const char *s, size_t n) {
    /* Check before adding the terminator or touching caller storage. */
    if (n >= FE_MEMORY_LIMIT) {
        fe_error(context, (FeSpan){0, 1, 1}, "E_LIMIT", "compiler string limit exceeded");
        return NULL;
    }
    char *p = fe_alloc(context, n + 1);
    if (p) {
        if (n) {
            memcpy(p, s, n);
        }
        p[n] = 0;
    }
    return p;
}

void fe_context_destroy(FeContext *context) {
    while (context->arena) {
        FeArena *next = context->arena->next;
        free(context->arena);
        context->arena = next;
    }
}

bool fe_isTestName(const char *name) {
    return !strncmp(name, "test", 4) && (name[4] == '_' || (name[4] >= 'A' && name[4] <= 'Z'));
}
