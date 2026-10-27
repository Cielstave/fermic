#include "fermi/emit.h"

void fe_emit_env_read(FeEmitter *emitter) {
    unsigned result = fe_emit_temp(emitter, emitter->function->result);
    unsigned name = emitter->function->params->id;
    unsigned output = emitter->function->params->next->id;
    fe_emit_line(emitter, "if (!v%u.len || v%u.len > 128) {", name, name);
    ++emitter->indent;
    fe_emit_line(emitter, "t%u.error = 1;", result);
    fe_emit_return(emitter, result);
    --emitter->indent;
    fe_emit_line(emitter, "}");
    fe_emit_line(emitter, "char key[129];");
    fe_emit_line(emitter, "for (size_t index = 0; index < v%u.len; ++index) {", name);
    ++emitter->indent;
    fe_emit_line(emitter, "unsigned char byte = v%u.data[index];", name);
    fe_emit_line(emitter, "bool valid = byte == '_' || (byte >= 'A' && byte <= 'Z') ||");
    fe_emit_line(emitter,
                 "    (byte >= 'a' && byte <= 'z') || (index && byte >= '0' && byte <= '9');");
    fe_emit_line(emitter, "if (!valid) {");
    ++emitter->indent;
    fe_emit_line(emitter, "t%u.error = 1;", result);
    fe_emit_return(emitter, result);
    --emitter->indent;
    fe_emit_line(emitter, "}");
    fe_emit_line(emitter, "key[index] = (char)byte;");
    --emitter->indent;
    fe_emit_line(emitter, "}");
    fe_emit_line(emitter, "key[v%u.len] = '\\0';", name);
    fe_emit_line(emitter, "const char *value = getenv(key);");
    fe_emit_line(emitter, "if (!value) {");
    ++emitter->indent;
    fe_emit_return(emitter, result);
    --emitter->indent;
    fe_emit_line(emitter, "}");
    /* Validate the full value before touching caller storage. No strlen or
     * allocation is needed, and a short destination never receives a prefix. */
    fe_emit_line(emitter, "size_t length = 0;");
    fe_emit_line(emitter, "while (length < v%u.len && value[length]) ++length;", output);
    fe_emit_line(emitter, "if (value[length]) {");
    ++emitter->indent;
    fe_emit_line(emitter, "t%u.error = 2;", result);
    fe_emit_return(emitter, result);
    --emitter->indent;
    fe_emit_line(emitter, "}");
    fe_emit_line(emitter, "if (length) memmove(v%u.data, value, length);", output);
    fe_emit_line(emitter, "t%u.ok = true; t%u.value = length;", result, result);
    fe_emit_return(emitter, result);
}
