#include "fermi/emit.h"

bool fe_uses_clock(FeProgram *program) {
    for (FeFunction *function = program->functions; function; function = function->next) {
        if (function->native && function->effects == FE_EFFECT_CLOCK_READ) {
            return true;
        }
    }
    return false;
}

void fe_emit_clock_read(FeEmitter *emitter) {
    unsigned result = fe_emit_temp(emitter, emitter->function->result);
    fe_emit_line(emitter, "struct timespec reading = {0};");
    fe_emit_line(emitter, "if (clock_gettime(CLOCK_MONOTONIC, &reading) != 0) {");
    ++emitter->indent;
    fe_emit_return(emitter, result);
    --emitter->indent;
    fe_emit_line(emitter, "}");
    fe_emit_line(
        emitter,
        "if (reading.tv_sec < 0 || reading.tv_nsec < 0 || reading.tv_nsec >= 1000000000) {");
    ++emitter->indent;
    fe_emit_line(emitter, "t%u.error = 1;", result);
    fe_emit_return(emitter, result);
    --emitter->indent;
    fe_emit_line(emitter, "}");
    fe_emit_line(
        emitter,
        "_Static_assert(sizeof(time_t) <= sizeof(uintmax_t), \"unsupported time_t width\");");
    fe_emit_line(emitter, "uintmax_t seconds = (uintmax_t)reading.tv_sec;");
    fe_emit_line(emitter, "size_t nanos = (size_t)reading.tv_nsec;");
    fe_emit_line(emitter, "if (seconds > (SIZE_MAX - nanos) / UINT64_C(1000000000)) {");
    ++emitter->indent;
    fe_emit_line(emitter, "t%u.error = 2;", result);
    fe_emit_return(emitter, result);
    --emitter->indent;
    fe_emit_line(emitter, "}");
    fe_emit_line(emitter, "t%u.ok = true;", result);
    fe_emit_line(emitter, "t%u.value = (size_t)seconds * UINT64_C(1000000000) + nanos;", result);
    fe_emit_return(emitter, result);
}
