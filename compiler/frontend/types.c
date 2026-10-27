#include "fermi/fe.h"
#include <inttypes.h>
#include <stdarg.h>
#include <string.h>

_Static_assert(SIZE_MAX == UINT64_MAX, "Fermi pilot requires a 64-bit compiler host");

FeModule *fe_imported_module(FeProgram *p, const char *name, size_t length) {
    if (!p->module) {
        return NULL;
    }
    for (FeImport *use = p->module->imports; use; use = use->next) {
        if (strlen(use->alias) == length && !memcmp(use->alias, name, length)) {
            return use->module;
        }
    }
    return NULL;
}
FeFunction *fe_named_function(FeProgram *p, const char *name, const FeType *argument) {
    const char *dot = strchr(name, '.');
    FeModule *module = dot ? fe_imported_module(p, name, (size_t)(dot - name)) : p->module;
    if (dot && !module) {
        return NULL;
    }
    for (FeFunction *f = p->functions; f; f = f->next) {
        if (f->module == module && f->type_argument == argument && (!dot || f->public) &&
            !strcmp(f->name, dot ? dot + 1 : name)) {
            return f;
        }
    }
    return NULL;
}
FeType *fe_named_type(FeProgram *p, const char *name) {
    const char *dot = strchr(name, '.');
    FeModule *module = dot ? fe_imported_module(p, name, (size_t)(dot - name)) : p->module;
    if (dot && !module) {
        return NULL;
    }
    for (FeAlias *alias = p->aliases; alias; alias = alias->next) {
        if (alias->module == module && (!dot || alias->public) &&
            !strcmp(alias->name, dot ? dot + 1 : name)) {
            return alias->target;
        }
    }
    for (FeType *t = p->types; t; t = t->next) {
        if (t->name && (t->module == module || (!dot && !t->module)) && (!dot || t->public) &&
            !strcmp(t->name, dot ? dot + 1 : name)) {
            return t;
        }
    }
    return NULL;
}
FeType *fe_type(FeProgram *p, FeTypeKind k, FeType *base, FeType *error, uint64_t n) {
    if (k != TY_ENUM && k != TY_STRUCT && k != TY_FILE) {
        for (FeType *t = p->types; t; t = t->next) {
            if (t->kind == k && t->base == base && t->error == error && t->count == n) {
                return t;
            }
        }
    }
    if (p->type_count >= 1024) {
        fe_error(p->ctx, (FeSpan){0, 1, 1}, "E_LIMIT", "too many types");
        return NULL;
    }
    FeType *t = fe_alloc(p->ctx, sizeof(*t));
    if (!t) {
        return NULL;
    }
    t->kind = k;
    if (k == TY_ENUM || k == TY_STRUCT || k == TY_FILE) {
        t->module = p->module;
    }
    t->base = base;
    t->error = error;
    t->count = n;
    t->nesting = base ? base->nesting + 1 : 0;
    if (error && error->nesting + 1 > t->nesting) {
        t->nesting = error->nesting + 1;
    }
    t->cells = k == TY_VOID                                  ? 0
               : k == TY_BUFFER                              ? 8
               : k == TY_FILE                                ? 2
               : k == TY_ARRAY                               ? n * base->cells
               : k == TY_RESULT                              ? 1 + base->cells + error->cells
               : k == TY_READ || k == TY_EDIT || k == TY_STR ? 2
                                                             : 1;
    t->resource_effects = k == TY_BUFFER   ? FE_EFFECT_MEMORY
                          : k == TY_FILE   ? FE_EFFECT_FS_CLOSE
                          : k == TY_RESULT ? base->resource_effects
                                           : 0;
    t->id = p->type_count++;
    t->next = p->types;
    p->types = t;
    return t;
}
bool fe_is_integer(const FeType *t) {
    return t && t->kind >= TY_U8 && t->kind <= TY_USIZE;
}
unsigned fe_integer_bits(const FeType *t) {
    switch (t->kind) {
    case TY_U8:
    case TY_I8:
        return 8;
    case TY_U16:
    case TY_I16:
        return 16;
    case TY_I32:
    case TY_U32:
        return 32;
    case TY_I64:
    case TY_U64:
    case TY_USIZE:
        return 64;
    default:
        return 0;
    }
}
bool fe_is_signed(const FeType *t) {
    return t->kind == TY_I8 || t->kind == TY_I16 || t->kind == TY_I32 || t->kind == TY_I64;
}
bool fe_is_payload_enum(const FeType *t) {
    return t && t->kind == TY_ENUM && t->payload;
}
bool fe_is_aggregate(const FeType *t) {
    return t->kind == TY_ARRAY || t->kind == TY_STRUCT || t->kind == TY_RESULT ||
           t->kind == TY_BUFFER || t->kind == TY_FILE || fe_is_payload_enum(t);
}
bool fe_has_resource(const FeType *t) {
    return t && t->resource_effects != 0;
}
bool fe_is_owner_borrow(const FeType *t) {
    return t && (t->kind == TY_BUFFER_EDIT || t->kind == TY_FILE_EDIT ||
                 t->kind == TY_STRUCT_READ || t->kind == TY_STRUCT_EDIT);
}
unsigned fe_resource_effects(const FeType *t) {
    return t ? t->resource_effects : 0;
}
uint64_t fe_type_cells(const FeType *t) {
    return t->cells;
}
const char *fe_type_name(const FeType *t) {
    if (t->name) {
        return t->name;
    }
    switch (t->kind) {
    case TY_ARRAY:
        return "array";
    case TY_READ:
        return "read view";
    case TY_EDIT:
        return "edit view";
    case TY_RESULT:
        return "Result";
    case TY_BUFFER:
        return "Buffer";
    case TY_BUFFER_EDIT:
        return "edit Buffer";
    case TY_FILE_EDIT:
        return "edit File";
    case TY_STRUCT_READ:
        return "read struct";
    case TY_STRUCT_EDIT:
        return "edit struct";
    default:
        return "type";
    }
}

/* Diagnostic type names are bounded independently of source nesting limits. */
typedef struct {
    char *buffer;
    size_t capacity, length;
    bool truncated;
} TypeName;

static void append_type_name(TypeName *name, const char *format, ...) {
    if (name->truncated) {
        return;
    }
    va_list args;
    va_start(args, format);
    int written =
        vsnprintf(name->buffer + name->length, name->capacity - name->length, format, args);
    va_end(args);
    if (written < 0 || (size_t)written >= name->capacity - name->length) {
        name->truncated = true;
        return;
    }
    name->length += (size_t)written;
}

static void describe_type(const FeType *type, TypeName *name, unsigned depth) {
    if (name->truncated) {
        return;
    }
    if (depth == 16) {
        append_type_name(name, "...");
    } else if (type->name) {
        if (type->module) {
            append_type_name(name, "%s.", type->module->name);
        }
        append_type_name(name, "%s", type->name);
    } else if (fe_is_owner_borrow(type)) {
        append_type_name(name, type->kind == TY_STRUCT_READ ? "read " : "edit ");
        describe_type(type->base, name, depth + 1);
    } else if (type->kind == TY_BUFFER) {
        append_type_name(name, "Buffer<");
        describe_type(type->base, name, depth + 1);
        append_type_name(name, ">");
    } else if (type->kind == TY_RESULT) {
        append_type_name(name, "Result<");
        describe_type(type->base, name, depth + 1);
        append_type_name(name, ", ");
        describe_type(type->error, name, depth + 1);
        append_type_name(name, ">");
    } else if (type->kind == TY_READ || type->kind == TY_EDIT) {
        append_type_name(name, "%s ", type->kind == TY_READ ? "read" : "edit");
        describe_type(type->base, name, depth + 1);
        append_type_name(name, "[]");
    } else if (type->kind == TY_ARRAY) {
        describe_type(type->base, name, depth + 1);
        append_type_name(name, "[%" PRIu64 "]", type->count);
    } else {
        append_type_name(name, "%s", fe_type_name(type));
    }
}

void fe_format_type(const FeType *type, char *buffer, size_t capacity) {
    if (!capacity) {
        return;
    }
    buffer[0] = 0;
    TypeName name = {.buffer = buffer, .capacity = capacity};
    describe_type(type, &name, 0);
    if (name.truncated && capacity >= 4) {
        memcpy(buffer + capacity - 4, "...", 4);
    }
}

FeGeneric *fe_named_generic(FeProgram *program, const char *name) {
    const char *dot = strchr(name, '.');
    FeModule *module =
        dot ? fe_imported_module(program, name, (size_t)(dot - name)) : program->module;
    if (dot && !module) {
        return NULL;
    }
    for (FeGeneric *generic = program->generics; generic; generic = generic->next) {
        FeFunction *function = generic->declaration;
        if (function->module == module && !strcmp(function->name, dot ? dot + 1 : name)) {
            return generic;
        }
    }
    return NULL;
}

/* Resource-free owning data can be copied without changing its lifetime. */
bool fe_is_data(const FeType *type) {
    return type && !fe_has_resource(type) &&
           (fe_is_integer(type) || type->kind == TY_BOOL || type->kind == TY_STR ||
            type->kind == TY_ARRAY || type->kind == TY_STRUCT || type->kind == TY_ENUM);
}
