#include "fermi/emit.h"
#include <string.h>

/* Generated objects retain their trusted runtime snapshot and private linkage.
 * Omit only the runtime source's own header, which has already been embedded. */
bool fe_emit_runtime_file(FeProgram *program, FILE *out, const char *relative, const char *header) {
    char path[4096];
    int length = snprintf(path, sizeof(path), "%s/%s", FERMI_RUNTIME_ROOT, relative);
    if (length < 0 || (size_t)length >= sizeof(path)) {
        return fe_error(program->ctx, (FeSpan){0, 1, 1}, "E_IO", "runtime path exceeds limit");
    }
    FILE *input = fopen(path, "r");
    if (!input) {
        return fe_error(program->ctx, (FeSpan){0, 1, 1}, "E_IO", "cannot open runtime source");
    }
    char line[1024];
    while (fgets(line, sizeof(line), input)) {
        if (!header || strcmp(line, header)) {
            fputs(line, out);
        }
    }
    bool ok = !ferror(input) && !ferror(out);
    if (fclose(input)) {
        ok = false;
    }
    return ok || fe_error(program->ctx, (FeSpan){0, 1, 1}, "E_IO", "cannot embed runtime source");
}
