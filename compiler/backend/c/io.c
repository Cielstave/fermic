#include "fermi/emit.h"
#include <string.h>

static void emit_write_loop(FeEmitter *emitter, unsigned input, unsigned result,
                            const char *descriptor) {
    fe_emit_line(emitter, "size_t written = 0;");
    fe_emit_line(emitter, "while (written < v%u.len) {", input);
    ++emitter->indent;
    fe_emit_line(emitter, "size_t remaining = v%u.len - written;", input);
    fe_emit_line(emitter, "size_t chunk = remaining > 4096 ? 4096 : remaining;");
    fe_emit_line(emitter, "ssize_t count = write(%s, v%u.data + written, chunk);", descriptor,
                 input);
    fe_emit_line(emitter, "if (count < 0 && errno == EINTR) continue;");
    fe_emit_line(emitter, "if (count <= 0) {");
    ++emitter->indent;
    if (result) {
        fe_emit_return(emitter, result);
    } else {
        fe_emit_line(emitter, "fault->code = 5; goto fe_fault_exit;");
    }
    --emitter->indent;
    fe_emit_line(emitter, "}");
    fe_emit_line(emitter, "written += (size_t)count;");
    --emitter->indent;
    fe_emit_line(emitter, "}");
}

void fe_emit_io_write(FeEmitter *emitter) {
    bool printing = emitter->function->result->kind == TY_VOID;
    unsigned result = printing ? 0 : fe_emit_temp(emitter, emitter->function->result);
    unsigned input = emitter->function->params->id;
    const char *descriptor =
        !strcmp(emitter->function->name, "write_stderr") ? "STDERR_FILENO" : "STDOUT_FILENO";
    emit_write_loop(emitter, input, result, descriptor);
    if (printing) {
        if (!strcmp(emitter->function->name, "println")) {
            fe_emit_line(emitter, "ssize_t count;");
            fe_emit_line(emitter, "do {");
            ++emitter->indent;
            fe_emit_line(emitter, "count = write(STDOUT_FILENO, \"\\n\", 1);");
            --emitter->indent;
            fe_emit_line(emitter, "} while (count < 0 && errno == EINTR);");
            fe_emit_line(emitter, "if (count != 1) { fault->code = 5; goto fe_fault_exit; }");
        }
        fe_emit_return(emitter, 0);
    } else {
        fe_emit_line(emitter, "t%u.ok = true;", result);
        fe_emit_line(emitter, "t%u.value = written;", result);
        fe_emit_return(emitter, result);
    }
}

void fe_emit_io_read(FeEmitter *emitter) {
    unsigned result = fe_emit_temp(emitter, emitter->function->result);
    unsigned input = emitter->function->params->id;
    fe_emit_line(emitter, "t%u.ok = true;", result);
    fe_emit_line(emitter, "if (!v%u.len) {", input);
    ++emitter->indent;
    fe_emit_return(emitter, result);
    --emitter->indent;
    fe_emit_line(emitter, "}");
    fe_emit_line(emitter, "size_t chunk = v%u.len > 4096 ? 4096 : v%u.len;", input, input);
    fe_emit_line(emitter, "ssize_t count;");
    fe_emit_line(emitter, "do {");
    ++emitter->indent;
    fe_emit_line(emitter, "count = read(STDIN_FILENO, v%u.data, chunk);", input);
    --emitter->indent;
    fe_emit_line(emitter, "} while (count < 0 && errno == EINTR);");
    fe_emit_line(emitter, "if (count < 0) {");
    ++emitter->indent;
    fe_emit_line(emitter, "t%u.ok = false;", result);
    fe_emit_line(emitter, "t%u.error = 1;", result);
    fe_emit_return(emitter, result);
    --emitter->indent;
    fe_emit_line(emitter, "}");
    fe_emit_line(emitter, "t%u.value = (size_t)count;", result);
    fe_emit_return(emitter, result);
}
