#include "fermi/driver.h"
#include "fermi/emit.h"

#include <string.h>

static bool is_test(const FeProgram *program, const FeFunction *function) {
    return function->module == program->module && fe_isTestName(function->name);
}

bool fe_emit_tests(FeProgram *program, FILE *code) {
    unsigned count = 0;
    for (FeFunction *function = program->functions; function; function = function->next) {
        if (!is_test(program, function)) {
            continue;
        }
        if (function->params || function->result->kind != TY_BOOL || function->native) {
            return fe_error(program->ctx, function->span, "E_TEST",
                            "test function '%s' must take no arguments and return bool",
                            function->name);
        }
        ++count;
    }
    if (!count) {
        return fe_error(program->ctx, (FeSpan){0, 1, 1}, "E_TEST",
                        "test source requires a testName or test_* function");
    }
    FILE *header = tmpfile();
    FILE *manifest = tmpfile();
    bool ok = code && header && manifest && fe_emit_library(program, code, header, manifest);
    if (ok) {
        fputs("\n#include <stdio.h>\n", code);
        if (fe_uses_arguments(program)) {
            fputs("int main(int argc, char **argv) {\n"
                  "    if (!fermi_arguments_init(&fe_arguments, argc, argv)) {\n"
                  "        fputs(\"fermi test: cannot initialize process arguments\\n\", stderr);\n"
                  "        return 1;\n"
                  "    }\n",
                  code);
        } else {
            fputs("int main(void) {\n", code);
        }
        fprintf(code, "    unsigned failed = 0;\n    puts(\"running %u tests\");\n", count);
        for (FeFunction *function = program->functions; function; function = function->next) {
            if (!is_test(program, function)) {
                continue;
            }
            /* A fresh language-fault slot lets subsequent tests run after a
             * checked fault. This does not isolate OS signals or global state. */
            fprintf(
                code,
                "    {\n        fe_fault fault = {0};\n"
                "        bool passed = fe_f%u(&fault);\n"
                "        if (fault.code) {\n"
                "            printf(\"test %s ... FAILED (runtime fault %%u)\\n\", fault.code);\n"
                "            ++failed;\n"
                "        } else if (!passed) {\n"
                "            puts(\"test %s ... FAILED\");\n"
                "            ++failed;\n"
                "        } else {\n"
                "            puts(\"test %s ... ok\");\n"
                "        }\n    }\n",
                function->id, function->name, function->name, function->name);
        }
        fprintf(code,
                "    printf(\"test result: %%s. %%u passed; %%u failed\\n\",\n"
                "           failed ? \"FAILED\" : \"ok\", %u - failed, failed);\n"
                "    return failed || fflush(stdout) || ferror(stdout) ? 1 : 0;\n}\n",
                count);
        ok = !ferror(code);
    }
    if (header && fclose(header)) {
        ok = false;
    }
    if (manifest && fclose(manifest)) {
        ok = false;
    }
    if (!ok && !program->ctx->diagnostic.code) {
        fe_error(program->ctx, (FeSpan){0, 1, 1}, "E_IO", "cannot write generated test source");
    }
    return ok;
}
