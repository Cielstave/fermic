#include "fermi/emit.h"
#include <string.h>

bool fe_uses_arguments(FeProgram *program) {
    for (FeFunction *function = program->functions; function; function = function->next) {
        if (function->native && function->effects == FE_EFFECT_ARGS_READ) {
            return true;
        }
    }
    return false;
}

bool fe_emit_arguments_runtime(FeProgram *program, FILE *out) {
    fputs("#define FERMI_ARGUMENTS_API static\n", out);
    if (!fe_emit_runtime_file(program, out, "include/fermi/arguments.h", NULL) ||
        !fe_emit_runtime_file(program, out, "process/arguments.c",
                              "#include \"fermi/arguments.h\"\n")) {
        return false;
    }
    fputs("#undef FERMI_ARGUMENTS_API\nstatic FermiArguments fe_arguments = {0};\n", out);
    return !ferror(out);
}

void fe_emit_arguments_native(FeEmitter *emitter) {
    unsigned result = fe_emit_temp(emitter, emitter->function->result);
    if (!strcmp(emitter->function->name, "count")) {
        fe_emit_line(
            emitter,
            "FermiArgumentsStatus status = fermi_arguments_count(&fe_arguments, &t%u.value);",
            result);
    } else {
        FeSymbol *index = emitter->function->params;
        FeSymbol *output = index->next;
        fe_emit_line(emitter,
                     "FermiArgumentsStatus status = fermi_arguments_read(&fe_arguments, v%u, "
                     "v%u.data, v%u.len, &t%u.value);",
                     index->id, output->id, output->id, result);
    }
    fe_emit_line(emitter, "t%u.ok = status == FERMI_ARGUMENTS_OK;", result);
    fe_emit_line(emitter, "if (!t%u.ok) t%u.error = (uint32_t)status - 1;", result, result);
    fe_emit_return(emitter, result);
}
