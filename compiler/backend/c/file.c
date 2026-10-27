#include "fermi/emit.h"
#include <string.h>

bool fe_uses_files(FeProgram *program) {
    for (FeType *type = program->types; type; type = type->next) {
        if (type->kind == TY_FILE) {
            return true;
        }
    }
    return false;
}

bool fe_emit_file_runtime(FeProgram *program, FILE *out) {
    fputs("#define FERMI_FILE_API static\n", out);
    if (!fe_emit_runtime_file(program, out, "include/fermi/file.h", NULL) ||
        !fe_emit_runtime_file(program, out, "io/file.c", "#include \"fermi/file.h\"\n")) {
        return false;
    }
    fputs("#undef FERMI_FILE_API\n", out);
    return !ferror(out);
}

void fe_emit_file_native(FeEmitter *emitter) {
    FeFunction *function = emitter->function;
    bool opening = !strcmp(function->name, "open_read") || !strcmp(function->name, "create_new");
    unsigned result = opening ? emitter->native_result : fe_emit_temp(emitter, function->result);
    FeSymbol *first = function->params;
    if (opening) {
        fe_emit_line(emitter,
                     "FermiFileStatus status = fermi_file_%s(&t%u.value, v%u.data, v%u.len);",
                     function->name, result, first->id, first->id);
    } else if (!strcmp(function->name, "read_into")) {
        FeSymbol *output = first->next;
        fe_emit_line(
            emitter,
            "FermiFileStatus status = fermi_file_read(v%u, v%u.data, v%u.len, &t%u.value);",
            first->id, output->id, output->id, result);
    } else if (!strcmp(function->name, "write")) {
        FeSymbol *input = first->next;
        fe_emit_line(
            emitter,
            "FermiFileStatus status = fermi_file_write(v%u, v%u.data, v%u.len, &t%u.value);",
            first->id, input->id, input->id, result);
    } else if (!strcmp(function->name, "sync")) {
        fe_emit_line(emitter, "FermiFileStatus status = fermi_file_sync(v%u);", first->id);
    } else {
        fe_emit_line(emitter, "FermiFileStatus status = fermi_file_close(&v%u);", first->id);
    }
    fe_emit_line(emitter, "t%u.ok = status == FERMI_FILE_OK;", result);
    fe_emit_line(emitter, "if (!t%u.ok) t%u.error = (uint32_t)status - 1;", result, result);
    fe_emit_return(emitter, result);
}
