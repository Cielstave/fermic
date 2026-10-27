#include "fermi/emit.h"
#include <string.h>

void fe_emit_return(FeEmitter *emitter, unsigned value) {
    if (emitter->function->result->kind != TY_VOID) {
        fe_emit_line(emitter, "fe_return = t%u;", value);
    }
    fe_clear_temp(emitter, emitter->function->result, value);
    fe_emit_line(emitter, "goto fe_exit;");
}

bool fe_emit_function_body(FeEmitter *emitter) {
    FeFunction *function = emitter->function;
    FILE *out = emitter->out;
    bool returns_value = function->result->kind != TY_VOID;
    if (!fe_plan_owners(emitter)) {
        return false;
    }

    fputs(" {\n", out);
    if (returns_value) {
        fputs("    ", out);
        fe_emit_type(out, function->result);
        fputs(" fe_return = {0};\n", out);
    }
    for (FeOwnerTemp *slot = emitter->owners; slot; slot = slot->next) {
        fe_emit_line(emitter, "fe_t%u t%u = {0};", slot->type->id, slot->id);
    }
    /* Function storage must be initialized before any edge to the epilogue.
     * Cleanup can inspect these slots even on entry faults. */
    for (FeSymbol *local = function->locals; local; local = local->next) {
        if (!local->parameter) {
            fputs("    ", out);
            fe_emit_type(out, local->type);
            fprintf(out, " v%u = {0};\n", local->id);
        }
        fe_emit_line(emitter, "(void)v%u;", local->id);
    }
    fe_emit_line(emitter, "if (fault->code) goto fe_fault_exit;");
    if (function->native) {
        if (function->effects & (FE_EFFECT_FS_OPEN | FE_EFFECT_FS_READ | FE_EFFECT_FS_CLOSE |
                                 FE_EFFECT_FS_WRITE | FE_EFFECT_FS_SYNC)) {
            fe_emit_file_native(emitter);
        } else if (function->effects == FE_EFFECT_MEMORY ||
                   (function->module && !strcmp(function->module->name, "std.buffer"))) {
            fe_emit_memory_native(emitter);
        } else if (function->effects == FE_EFFECT_CLOCK_READ) {
            fe_emit_clock_read(emitter);
        } else if (function->effects == FE_EFFECT_ARGS_READ) {
            fe_emit_arguments_native(emitter);
        } else if (function->effects == FE_EFFECT_ENV_READ) {
            fe_emit_env_read(emitter);
        } else if (function->effects == FE_EFFECT_IO_READ) {
            fe_emit_io_read(emitter);
        } else {
            fe_emit_io_write(emitter);
        }
    } else {
        fe_emit_stmt(emitter, function->body);
    }
    /* Void fallthrough is a normal exit. Value-returning functions have already
     * been checked for a return on every reachable path. */
    fe_emit_line(emitter, "goto fe_exit;");
    fputs("fe_fault_exit:\n", out);
    if (returns_value) {
        fputs("    fe_return = (", out);
        fe_emit_type(out, function->result);
        fputs("){0};\n", out);
    }
    fputs("fe_exit:;\n", out);
    fe_drop_function(emitter);
    fe_emit_line(emitter, returns_value ? "return fe_return;" : "return;");
    fputs("}\n", out);
    return !ferror(out);
}
