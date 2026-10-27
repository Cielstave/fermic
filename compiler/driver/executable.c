#define _POSIX_C_SOURCE 200809L
#include "fermi/driver.h"
#include "fermi/emit.h"

#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static int failure(FeProgram *program, const char *code, const char *message) {
    fe_error(program->ctx, (FeSpan){0, 1, 1}, code, "%s", message);
    return 1;
}

bool fe_emit_executable(FeProgram *program, FILE *code) {
    FeFunction *entry = NULL;
    for (FeFunction *fn = program->functions; fn; fn = fn->next) {
        if (fn->module == program->module && !strcmp(fn->name, "main")) {
            entry = fn;
        }
    }
    FeContext *context = program->ctx;
    if (!entry) {
        bool hasTests = false;
        for (FeFunction *fn = program->functions; fn; fn = fn->next) {
            hasTests = hasTests || (fn->module == program->module && fe_isTestName(fn->name));
        }
        fe_error(context, (FeSpan){context->length, 0, 0}, "E_ENTRY_MISSING",
                 "cannot create an executable: the root module has no main function");
        snprintf(
            context->diagnostic.help, sizeof(context->diagnostic.help), "%s",
            hasTests
                ? "This module contains test functions. Use 'fermi test FILE.fe' to run them, "
                  "or 'fermi check FILE.fe' to validate the module without running it."
                : "Modules do not require main. Use 'fermi check FILE.fe' or import this "
                  "module from a program. To run a program, provide func main() -> void or i32 "
                  "in its root module; use 'fermi build --library' for C ABI exports.");
        return false;
    }
    const char *errorCode = NULL;
    if (entry->exported) {
        errorCode = "E_ENTRY_EXPORTED";
        fe_error(context, entry->span, errorCode,
                 "main is exported through the C ABI and cannot serve as the executable entry");
        snprintf(context->diagnostic.help, sizeof(context->diagnostic.help),
                 "Remove export from the program entry, or use 'fermi build --library FILE.fe "
                 "--out-dir DIR' to build C ABI exports.");
    } else if (entry->params) {
        errorCode = "E_ENTRY_PARAMETERS";
        fe_error(context, entry->span, errorCode,
                 "main declares %u parameter(s); executable entry does not accept parameters",
                 entry->param_count);
        snprintf(context->diagnostic.help, sizeof(context->diagnostic.help),
                 "Declare func main() -> void or i32. Read command-line arguments through "
                 "std.args, and call parameterized functions from main.");
    } else if (entry->result->kind != TY_I32 && entry->result->kind != TY_VOID) {
        errorCode = "E_ENTRY_RETURN_TYPE";
        fe_error(context, entry->span, errorCode,
                 "main returns %s; executable entry must return void or i32",
                 fe_type_name(entry->result));
        snprintf(context->diagnostic.help, sizeof(context->diagnostic.help),
                 "Use void for an entry without an exit value, or i32 for an exit status from 0 to "
                 "255. Handle Result errors inside main.");
    }
    if (errorCode) {
        return false;
    }
    FILE *header = tmpfile();
    FILE *manifest = tmpfile();
    bool ok = code && header && manifest;
    if (ok) {
        ok = fe_emit_library(program, code, header, manifest);
        fputs("\n#include <stdio.h>\n", code);
        if (fe_uses_arguments(program)) {
            fputs("int main(int argc, char **argv) {\n"
                  "    if (!fermi_arguments_init(&fe_arguments, argc, argv)) {\n"
                  "        fputs(\"fermi: cannot initialize process arguments\\n\", stderr);\n"
                  "        return 1;\n"
                  "    }\n",
                  code);
        } else {
            fputs("int main(void) {\n", code);
        }
        fputs("    fe_fault fault = {0};\n", code);
        if (entry->result->kind == TY_VOID) {
            fprintf(code, "    fe_f%u(&fault);\n    int32_t status = 0;\n", entry->id);
        } else {
            fprintf(code, "    int32_t status = fe_f%u(&fault);\n", entry->id);
        }
        fputs("    if (fault.code) {\n"
              "        fprintf(stderr, \"fermi: runtime fault[%u]: %s\\n\", fault.code,\n"
              "                fault.code == 1 ? \"index outside buffer\" :\n"
              "                fault.code == 2 ? \"integer overflow\" :\n"
              "                fault.code == 3 ? \"division by zero\" :\n"
              "                fault.code == 4 ? \"shift count outside operand width\" : "
              "fault.code == 5 ? \"standard output write failed\" : \"unknown fault\");\n"
              "        return 1;\n"
              "    }\n"
              "    if (status < 0 || status > 255) {\n"
              "        fputs(\"fermi: exit status must be between 0 and 255\\n\", stderr);\n"
              "        return 1;\n"
              "    }\n"
              "    return (int)status;\n}\n",
              code);
        ok = ok && !ferror(code);
    }
    if (header && fclose(header)) {
        ok = false;
    }
    if (manifest && fclose(manifest)) {
        ok = false;
    }
    if (!ok) {
        failure(program, "E_IO", "cannot write generated executable source");
    }
    return ok;
}

const char *const fe_executable_flags[] = {"-std=c11",
                                           "-Wall",
                                           "-Wextra",
                                           "-Werror",
                                           "-Wno-unused-function",
                                           "-Wno-unused-but-set-variable",
                                           NULL};

bool fe_compile_executable(FeProgram *program, const char *source, const char *binary,
                           const char *optimization) {
    const char *cc = getenv("FERMI_CC");
    if (!cc || !*cc) {
        cc = "clang";
    }
    char *arguments[sizeof(fe_executable_flags) / sizeof(*fe_executable_flags) + 7];
    unsigned count = 0;
    arguments[count++] = (char *)cc;
    for (unsigned i = 0; fe_executable_flags[i]; ++i) {
        arguments[count++] = (char *)fe_executable_flags[i];
    }
    arguments[count++] = (char *)optimization;
    arguments[count++] = "-o";
    arguments[count++] = (char *)binary;
    arguments[count++] = "--";
    arguments[count++] = (char *)source;
    arguments[count] = NULL;
    return fe_run_tool(program->ctx, arguments);
}

int fe_build_executable(FeProgram *program, const char *output, const char *optimization) {
    return fe_build_native(program, output, optimization, fe_emit_executable);
}

int fe_build_native(FeProgram *program, const char *output, const char *optimization,
                    bool (*emit)(FeProgram *, FILE *)) {
    /* Stage next to the destination so publication is a single filesystem rename. */
    char stage[4096];
    if (snprintf(stage, sizeof(stage), "%s.fermi-XXXXXX", output) >= (int)sizeof(stage)) {
        return failure(program, "E_PATH", "executable output path is too long");
    }
    if (!mkdtemp(stage)) {
        return failure(program, "E_IO", strerror(errno));
    }
    char source[4128], binary[4128];
    snprintf(source, sizeof(source), "%s/main.c", stage);
    snprintf(binary, sizeof(binary), "%s/program", stage);
    int result = 1;
    FILE *code = fopen(source, "w");
    bool ok = code && emit(program, code);
    if (code && fclose(code)) {
        ok = false;
    }
    if (!ok) {
        if (!program->ctx->diagnostic.code) {
            failure(program, "E_IO", "cannot write generated executable source");
        }
        goto cleanup;
    }
    if (!fe_compile_executable(program, source, binary, optimization)) {
        goto cleanup;
    }
    if (rename(binary, output)) {
        failure(program, "E_IO", strerror(errno));
        goto cleanup;
    }
    result = 0;
cleanup:
    unlink(source);
    unlink(binary);
    rmdir(stage);
    return result;
}
