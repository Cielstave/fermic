#include "fermi/emit.h"
#include <string.h>

bool fe_emit_memory_runtime(FeProgram *program, FILE *out) {
    fputs("#define FERMI_BUFFER_API static\n", out);
    if (!fe_emit_runtime_file(program, out, "include/fermi/buffer.h", NULL) ||
        !fe_emit_runtime_file(program, out, "memory/buffer.c", "#include \"fermi/buffer.h\"\n")) {
        return false;
    }
    fputs("#undef FERMI_BUFFER_API\n", out);
    return !ferror(out);
}

void fe_emit_memory_native(FeEmitter *emitter) {
    if (!strcmp(emitter->function->name, "element_size")) {
        unsigned result = fe_emit_temp(emitter, emitter->function->result);
        for (unsigned i = 0; i < emitter->indent; ++i) {
            fputs("    ", emitter->out);
        }
        fprintf(emitter->out, "t%u = sizeof(", result);
        fe_emit_type(emitter->out, emitter->function->type_argument);
        fputs(");\n", emitter->out);
        fe_emit_return(emitter, result);
        return;
    }
    if (!strcmp(emitter->function->name, "resize_filled")) {
        FeSymbol *buffer = emitter->function->params;
        FeSymbol *length = buffer->next;
        FeSymbol *seed = length->next;
        unsigned result = fe_emit_temp(emitter, emitter->function->result);
        fe_emit_line(emitter, "FermiMemoryStatus status = fermi_buffer_reserve(v%u, v%u);",
                     buffer->id, length->id);
        fe_emit_line(emitter, "if (status == FERMI_MEMORY_OK) {");
        ++emitter->indent;
        fe_emit_line(emitter, "for (size_t index = v%u->length; index < v%u; ++index) {",
                     buffer->id, length->id);
        ++emitter->indent;
        for (unsigned i = 0; i < emitter->indent; ++i) {
            fputs("    ", emitter->out);
        }
        fputs("((", emitter->out);
        fe_emit_type(emitter->out, seed->type);
        fprintf(emitter->out, " *)v%u->data)[index] = v%u;\n", buffer->id, seed->id);
        --emitter->indent;
        fe_emit_line(emitter, "}");
        fe_emit_line(emitter, "v%u->length = v%u;", buffer->id, length->id);
        --emitter->indent;
        fe_emit_line(emitter, "}");
        fe_emit_line(emitter, "t%u.ok = status == FERMI_MEMORY_OK;", result);
        fe_emit_line(emitter, "if (t%u.ok) t%u.value = v%u->length;", result, result, buffer->id);
        fe_emit_line(emitter, "else t%u.error = (uint32_t)status - 1;", result);
        fe_emit_return(emitter, result);
        return;
    }
    if (!strcmp(emitter->function->name, "empty")) {
        unsigned result = emitter->native_result;
        FeSymbol *limit = emitter->function->params;
        FeType *element = emitter->function->result->base->base;
        for (unsigned i = 0; i < emitter->indent; ++i) {
            fputs("    ", emitter->out);
        }
        fprintf(emitter->out,
                "FermiMemoryStatus status = fermi_buffer_create(&t%u.value, 0, sizeof(", result);
        fe_emit_type(emitter->out, element);
        fprintf(emitter->out, "), v%u, NULL);\n", limit->id);
        fe_emit_line(emitter, "t%u.ok = status == FERMI_MEMORY_OK;", result);
        fe_emit_line(emitter, "if (!t%u.ok) t%u.error = (uint32_t)status - 1;", result, result);
        fe_emit_return(emitter, result);
        return;
    }
    if (strcmp(emitter->function->name, "bytes") && strcmp(emitter->function->name, "allocate")) {
        FeSymbol *buffer = emitter->function->params;
        FeSymbol *count = buffer->next;
        unsigned result = fe_emit_temp(emitter, emitter->function->result);
        bool reserve = !strcmp(emitter->function->name, "reserve");
        fe_emit_line(emitter, "FermiMemoryStatus status = fermi_buffer_%s(v%u, v%u);",
                     reserve                                        ? "reserve"
                     : !strcmp(emitter->function->name, "truncate") ? "truncate"
                                                                    : "resize",
                     buffer->id, count->id);
        fe_emit_line(emitter, "t%u.ok = status == FERMI_MEMORY_OK;", result);
        fe_emit_line(emitter, "if (t%u.ok) t%u.value = v%u->%s;", result, result, buffer->id,
                     reserve ? "capacity" : "length");
        fe_emit_line(emitter, "else t%u.error = (uint32_t)status - 1;", result);
        fe_emit_return(emitter, result);
        return;
    }
    unsigned result = emitter->native_result;
    FeSymbol *length = emitter->function->params;
    FeSymbol *limit = length->next;
    fe_emit_line(
        emitter, "FermiMemoryStatus status = fermi_buffer_create(&t%u.value, v%u, %u, v%u, NULL);",
        result, length->id, fe_integer_bits(emitter->function->result->base->base) / 8, limit->id);
    fe_emit_line(emitter, "t%u.ok = status == FERMI_MEMORY_OK;", result);
    fe_emit_line(emitter, "if (!t%u.ok) t%u.error = (uint32_t)status - 1;", result, result);
    fe_emit_return(emitter, result);
}
