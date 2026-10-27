#include "fermi/emit.h"
#include <inttypes.h>
#include <string.h>

static void emit_typedef(FILE *out, FeType *t, bool done[1024]) {
    if (t->kind <= TY_USIZE || done[t->id]) {
        return;
    }
    done[t->id] = true;
    if (t->base) {
        emit_typedef(out, t->base, done);
    }
    if (t->error) {
        emit_typedef(out, t->error, done);
    }
    for (FeField *f = t->fields; f; f = f->next) {
        if (f->type) {
            emit_typedef(out, f->type, done);
        }
    }
    unsigned id = t->id;
    if (t->kind == TY_ENUM && !t->payload) {
        fprintf(out, "typedef uint32_t fe_t%u;\n", id);
        return;
    }
    if (t->kind == TY_BUFFER) {
        fprintf(out, "typedef FermiBuffer fe_t%u;\n", id);
        return;
    }
    if (t->kind == TY_FILE) {
        fprintf(out, "typedef FermiFile fe_t%u;\n", id);
        return;
    }
    if (fe_is_owner_borrow(t)) {
        if (t->kind == TY_STRUCT_READ) {
            fputs("const ", out);
        }
        fprintf(out, "typedef fe_t%u *fe_t%u;\n", t->base->id, id);
        return;
    }
    fputs("typedef struct { ", out);
    switch (t->kind) {
    case TY_ENUM:
        fputs("uint32_t tag; union { ", out);
        for (FeField *field = t->fields; field; field = field->next) {
            if (field->type) {
                fe_emit_type(out, field->type);
                fprintf(out, " m%u; ", field->index);
            }
        }
        fputs("} data; ", out);
        break;
    case TY_STR:
        fputs("const uint8_t *data; size_t len; ", out);
        break;
    case TY_ARRAY:
        fe_emit_type(out, t->base);
        fprintf(out, " data[%" PRIu64 "]; ", t->count);
        break;
    case TY_READ:
    case TY_EDIT:
        if (t->kind == TY_READ) {
            fputs("const ", out);
        }
        fe_emit_type(out, t->base);
        fputs(" *data; size_t len; ", out);
        break;
    case TY_RESULT:
        fputs("bool ok; ", out);
        if (t->base->kind != TY_VOID) {
            fe_emit_type(out, t->base);
            fputs(" value; ", out);
        }
        fe_emit_type(out, t->error);
        fputs(" error; ", out);
        break;
    case TY_STRUCT:
        for (FeField *f = t->fields; f; f = f->next) {
            fe_emit_type(out, f->type);
            fprintf(out, " m%u; ", f->index);
        }
        break;
    default:
        break;
    }
    fprintf(out, "} fe_t%u;\n", id);
}
static void typedefs(FILE *out, FeProgram *p) {
    bool done[1024] = {0};
    for (FeType *t = p->types; t; t = t->next) {
        emit_typedef(out, t, done);
    }
}
static bool needs_drop_helper(const FeType *type) {
    return (fe_is_payload_enum(type) || type->kind == TY_STRUCT) && fe_has_resource(type);
}

static void drop_field(FILE *out, const FeField *field, bool variant) {
    const char *member = variant ? "data.m" : "m";
    if (field->type->kind == TY_BUFFER) {
        fprintf(out, "    fermi_buffer_drop(&value->%s%u);\n", member, field->index);
    } else if (field->type->kind == TY_FILE) {
        fprintf(out, "    fermi_file_drop(&value->%s%u);\n", member, field->index);
    } else {
        fprintf(out, "    fe_drop_t%u(&value->%s%u);\n", field->type->id, member, field->index);
    }
}

/* Shared nested owners reuse helpers; struct fields drop in reverse order. */
static void drop_helpers(FILE *out, FeProgram *p) {
    for (FeType *type = p->types; type; type = type->next) {
        if (needs_drop_helper(type)) {
            fprintf(out, "static void fe_drop_t%u(fe_t%u *value);\n", type->id, type->id);
        }
    }
    for (FeType *type = p->types; type; type = type->next) {
        if (!needs_drop_helper(type)) {
            continue;
        }
        fprintf(out, "static void fe_drop_t%u(fe_t%u *value) {\n", type->id, type->id);
        if (type->kind == TY_STRUCT) {
            FeField *fields[128];
            unsigned count = 0;
            for (FeField *field = type->fields; field; field = field->next) {
                if (fe_has_resource(field->type)) {
                    fields[count++] = field;
                }
            }
            while (count) {
                drop_field(out, fields[--count], false);
            }
        } else {
            fputs("    switch (value->tag) {\n", out);
            for (FeField *field = type->fields; field; field = field->next) {
                if (fe_has_resource(field->type)) {
                    fprintf(out, "    case %u:\n", field->index);
                    drop_field(out, field, true);
                    fputs("        break;\n", out);
                }
            }
            fputs("    default: break;\n    }\n", out);
        }
        fprintf(out, "    *value = (fe_t%u){0};\n}\n", type->id);
    }
}
static void signature(FILE *out, FeFunction *f) {
    fputs("static ", out);
    fe_emit_type(out, f->result);
    fprintf(out, " fe_f%u(fe_fault *fault", f->id);
    for (FeSymbol *v = f->params; v; v = v->next) {
        fputs(", ", out);
        fe_emit_type(out, v->type);
        fprintf(out, " v%u", v->id);
    }
    fputs(")", out);
}
static void public_type(FILE *out, FeType *t) {
    if (t->kind == TY_ENUM) {
        fputs("uint32_t", out);
    } else {
        fe_emit_type(out, t);
    }
}
static void public_signature(FILE *out, FeFunction *f) {
    fprintf(out, "fermi_%s_result fermi_%s(", f->name, f->name);
    bool first = true;
    for (FeSymbol *v = f->params; v; v = v->next) {
        if (!first) {
            fputs(", ", out);
        }
        first = false;
        if (v->type->kind == TY_READ || v->type->kind == TY_EDIT) {
            if (v->type->kind == TY_READ) {
                fputs("const ", out);
            }
            public_type(out, v->type->base);
            fprintf(out, " *arg%u, size_t arg%u_len", v->id, v->id);
        } else {
            public_type(out, v->type);
            fprintf(out, " arg%u", v->id);
        }
    }
    if (first) {
        fputs("void", out);
    }
    fputc(')', out);
}
static void public_result(FILE *out, FeFunction *f) {
    fprintf(out, "typedef struct { uint32_t status; uint32_t code;");
    FeType *t = f->result->kind == TY_RESULT ? f->result->base : f->result;
    if (t->kind != TY_VOID) {
        fputc(' ', out);
        public_type(out, t);
        fputs(" value;", out);
    }
    fprintf(out, " } fermi_%s_result;\n", f->name);
}
static void wrapper(FILE *out, FeProgram *p, FeFunction *f) {
    public_result(out, f);
    public_signature(out, f);
    fputs(" {\n", out);
    fprintf(out, "    fermi_%s_result result = {0};\n", f->name);
    for (FeSymbol *v = f->params; v; v = v->next) {
        if (v->type->kind == TY_ENUM) {
            unsigned count = 0;
            for (FeField *field = v->type->fields; field; field = field->next) {
                ++count;
            }
            fprintf(out,
                    "    if (arg%u >= %u) { result.status=3; result.code=5; return result; }\n",
                    v->id, count);
        }
        if (v->type->kind != TY_READ && v->type->kind != TY_EDIT) {
            continue;
        }
        fprintf(out,
                "    if (arg%u_len && !arg%u) { result.status=3; result.code=1; return result; }\n",
                v->id, v->id);
        fprintf(out,
                "    if (arg%u_len > SIZE_MAX / sizeof(*arg%u)) { result.status=3; result.code=2; "
                "return result; }\n",
                v->id, v->id);
        fprintf(out, "    size_t bytes%u = arg%u_len * sizeof(*arg%u);\n", v->id, v->id, v->id);
        fprintf(out, "    uintptr_t address%u = (uintptr_t)arg%u;\n", v->id, v->id);
        fprintf(out, "    if (bytes%u && address%u %% _Alignof(", v->id, v->id);
        public_type(out, v->type->base);
        fputs(")) { result.status=3; result.code=3; return result; }\n", out);
        fprintf(out,
                "    if (bytes%u > UINTPTR_MAX - address%u) { result.status=3; result.code=2; "
                "return result; }\n",
                v->id, v->id);
    }
    for (FeSymbol *a = f->params; a; a = a->next) {
        if (a->type->kind != TY_READ && a->type->kind != TY_EDIT) {
            continue;
        }
        for (FeSymbol *b = a->next; b; b = b->next) {
            if ((b->type->kind != TY_READ && b->type->kind != TY_EDIT) ||
                (a->type->kind == TY_READ && b->type->kind == TY_READ)) {
                continue;
            }
            fprintf(out,
                    "    if (bytes%u && bytes%u && address%u < address%u + bytes%u && address%u < "
                    "address%u + bytes%u) { result.status=3; result.code=4; return result; }\n",
                    a->id, b->id, a->id, b->id, b->id, b->id, a->id, a->id);
        }
    }
    fputs("    fe_fault fault = {0};\n", out);
    for (FeSymbol *v = f->params; v; v = v->next) {
        if (v->type->kind == TY_READ || v->type->kind == TY_EDIT) {
            fputs("    ", out);
            fe_emit_type(out, v->type);
            fprintf(out, " view%u = {arg%u, arg%u_len};\n", v->id, v->id, v->id);
        }
    }
    fputs("    ", out);
    if (f->result->kind != TY_VOID) {
        fe_emit_type(out, f->result);
        fputs(" value = ", out);
    }
    fprintf(out, "fe_f%u(&fault", f->id);
    for (FeSymbol *v = f->params; v; v = v->next) {
        fprintf(out, ", %s%u",
                (v->type->kind == TY_READ || v->type->kind == TY_EDIT) ? "view" : "arg", v->id);
    }
    fputs(");\n    if (fault.code) { result.status=2; result.code=fault.code; return result; }\n",
          out);
    if (f->result->kind == TY_RESULT) {
        fputs("    if (!value.ok) { result.status=1; result.code=value.error; return result; }\n",
              out);
        if (f->result->base->kind != TY_VOID) {
            fputs("    result.value=value.value;\n", out);
        }
    } else if (f->result->kind != TY_VOID) {
        fputs("    result.value=value;\n", out);
    }
    fputs("    return result;\n}\n", out);
    (void)p;
}
static void manifest_type(FILE *out, FeType *t) {
    if (t->kind == TY_RESULT) {
        fputs("Result<", out);
        manifest_type(out, t->base);
        fputc(',', out);
        manifest_type(out, t->error);
        fputc('>', out);
    } else if (t->kind == TY_READ || t->kind == TY_EDIT) {
        fprintf(out, "%s ", t->kind == TY_READ ? "read" : "edit");
        manifest_type(out, t->base);
        fputs("[]", out);
    } else {
        if (t->module && strcmp(t->module->name, "<entry>")) {
            fprintf(out, "%s.", t->module->name);
        }
        fputs(fe_type_name(t), out);
    }
}
bool fe_emit_library(FeProgram *p, FILE *out, FILE *header, FILE *manifest) {
    if (fe_uses_clock(p) || fe_uses_files(p)) {
        fputs("#ifndef _POSIX_C_SOURCE\n#define _POSIX_C_SOURCE 200809L\n#endif\n", out);
    }
    if (fe_uses_clock(p)) {
        fputs("#include <time.h>\n", out);
    }
    fputs("/* Generated by Fermi 0.2.0-pilot; safety checks are mandatory. */\n#include "
          "<stdbool.h>\n#include <stdint.h>\n#include <stddef.h>\n#include <limits.h>\n",
          out);
    fputs("_Static_assert(CHAR_BIT == 8 && sizeof(size_t) == 8, \"Fermi pilot requires 64-bit "
          "target with 8-bit bytes\");\ntypedef struct { uint32_t code; } fe_fault;\n",
          out);
    for (FeFunction *f = p->functions; f; f = f->next) {
        if (f->native) {
            fputs("#include <errno.h>\n#include <unistd.h>\n#include <stdlib.h>\n#include "
                  "<string.h>\n",
                  out);
            break;
        }
    }
    for (FeType *type = p->types; type; type = type->next) {
        if (type->kind == TY_BUFFER) {
            if (!fe_emit_memory_runtime(p, out)) {
                return false;
            }
            break;
        }
    }
    if (fe_uses_arguments(p) && !fe_emit_arguments_runtime(p, out)) {
        return false;
    }
    if (fe_uses_files(p) && !fe_emit_file_runtime(p, out)) {
        return false;
    }
    typedefs(out, p);
    drop_helpers(out, p);
    for (FeFunction *f = p->functions; f; f = f->next) {
        signature(out, f);
        fputs(";\n", out);
    }
    for (FeFunction *f = p->functions; f; f = f->next) {
        signature(out, f);
        FeEmitter g = {.program = p, .function = f, .out = out, .indent = 1};
        if (!fe_emit_function_body(&g)) {
            return false;
        }
    }
    fputs("/* Fermi C ABI v1. Caller must supply live, aligned allocations, valid\n * lengths and "
          "scalar representations, with no concurrent conflicting access.\n * Empty buffers may "
          "use NULL. Faults may leave edit buffers partially written.\n * Domain-error atomicity "
          "is API-specific; see the component specification. */\n#pragma once\n#include "
          "<stdbool.h>\n#include <stdint.h>\n#include <stddef.h>\n#ifndef FERMI_ABI_V1\n#define "
          "FERMI_ABI_V1\n#define FERMI_OK 0u\n#define FERMI_DOMAIN_ERROR 1u\n#define FERMI_FAULT "
          "2u\n#define FERMI_INVALID_ARGUMENT 3u\n#define FERMI_FAULT_BOUNDS 1u\n#define "
          "FERMI_FAULT_OVERFLOW 2u\n#define FERMI_FAULT_DIV_ZERO 3u\n#define FERMI_FAULT_SHIFT "
          "4u\n#define FERMI_FAULT_IO 5u\n#endif\n#ifdef "
          "__cplusplus\nextern \"C\" {\n#endif\n",
          header);
    for (FeType *t = p->types; t; t = t->next) {
        if (t->kind == TY_ENUM && !t->payload) {
            for (FeField *f = t->fields; f; f = f->next) {
                char prefix[2 * FE_MODULE_NAME_LIMIT + 16] = "";
                if (t->module && t->module != p->module) {
                    size_t used = 0;
                    used += (size_t)snprintf(prefix, sizeof(prefix), "module_");
                    for (const unsigned char *ch = (const unsigned char *)t->module->name; *ch;
                         ++ch) {
                        used += (size_t)snprintf(prefix + used, sizeof(prefix) - used, "%02x", *ch);
                    }
                    snprintf(prefix + used, sizeof(prefix) - used, "_");
                }
                fprintf(header, "#ifndef FERMI_%s%s_%s\n#define FERMI_%s%s_%s %uu\n#endif\n",
                        prefix, t->name, f->name, prefix, t->name, f->name, f->index);
            }
        }
    }
    fputs("{\n  \"schema_version\": 1,\n  \"compiler\": \"0.2.0-pilot\",\n  \"language\": "
          "\"0.2-pilot\",\n  \"cost_model\": \"fermi-work-1\",\n  \"target\": \"native-64\",\n  "
          "\"native_stack\": \"unknown\",\n  \"assumptions\": [\"live C allocations\", \"no "
          "conflicting host access\", \"trusted C compiler\"],\n  \"exports\": [",
          manifest);
    bool first = true;
    for (FeFunction *f = p->functions; f; f = f->next) {
        if (f->exported) {
            wrapper(out, p, f);
            public_result(header, f);
            public_signature(header, f);
            fputs(";\n", header);
            fprintf(manifest, "%s\n    {\"name\": \"%s\", \"symbol\": \"fermi_%s\", \"result\": \"",
                    first ? "" : ",", f->name, f->name);
            manifest_type(manifest, f->result);
            fprintf(manifest,
                    "\", \"proof\": \"%s\", \"work_bound\": ", f->bounded ? "static" : "unknown");
            if (f->bounded) {
                fprintf(manifest, "%" PRIu64, f->work);
            } else {
                fputs("null", manifest);
            }
            fputs(", \"requested_work\": ", manifest);
            if (f->budgeted) {
                fprintf(manifest, "%" PRIu64, f->budget);
            } else {
                fputs("null", manifest);
            }
            fprintf(manifest, ", \"heap_bytes\": %s, \"effects\": ", f->effects ? "null" : "0");
            fe_print_effects(manifest, f->effects);
            fputs(", \"allowed_effects\": ", manifest);
            if (f->effects_declared) {
                fe_print_effects(manifest, f->allowed_effects);
            } else {
                fputs("null", manifest);
            }
            fputs(", ", manifest);
            fputs("\"fault_output\": \"may_be_partial\", "
                  "\"domain_error_output\": \"api_specific\", \"parameters\": [",
                  manifest);
            bool param_first = true;
            for (FeSymbol *v = f->params; v; v = v->next) {
                fprintf(manifest, "%s{\"name\": \"%s\", \"type\": \"", param_first ? "" : ", ",
                        v->name);
                manifest_type(manifest, v->type);
                fputs("\"}", manifest);
                param_first = false;
            }
            fputs("]}", manifest);
            first = false;
        }
    }
    fputs("\n  ]\n}\n", manifest);
    fputs("#ifdef __cplusplus\n}\n#endif\n", header);
    return !ferror(out) && !ferror(header) && !ferror(manifest);
}
