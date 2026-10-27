#define _POSIX_C_SOURCE 200809L
#include "fermi/driver.h"
#include "fermi/fe.h"

#include <errno.h>
#include <inttypes.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static void explain(const FeProgram *program) {
    puts("cost model: fermi-work-1 (abstract work; not CPU time); heap: see effects; native stack: "
         "unknown");
    for (FeFunction *fn = program->functions; fn; fn = fn->next) {
        if (fn->module && fn->module != program->module) {
            printf("%s.", fn->module->name);
        }
        printf("%s", fn->name);
        if (fn->type_argument) {
            char argument[2 * FE_MODULE_NAME_LIMIT + 128];
            fe_format_type(fn->type_argument, argument, sizeof(argument));
            printf("<%s>", argument);
        }
        fputs(": ", stdout);
        if (fn->bounded) {
            printf("static upper bound = %" PRIu64, fn->work);
        } else {
            printf("work bound unknown");
        }
        if (fn->budgeted) {
            printf("; requested = %" PRIu64, fn->budget);
        }
        printf("; heap: %s; effects: ", fn->effects ? "unknown" : "0");
        fe_print_effects(stdout, fn->effects);
        if (fn->effects_declared) {
            fputs("; permitted effects: ", stdout);
            fe_print_effects(stdout, fn->allowed_effects);
        }
        printf("; overflow checks proven: %u\n", fn->proven_overflows);
    }
}

int main(int argc, char **argv) {
    FeOptions options = {0};
    /* Even invalid command lines should honor an explicitly requested JSON sink. */
    for (int i = 1; i < argc; ++i) {
        if (!strcmp(argv[i], "--")) {
            break;
        }
        if (!strcmp(argv[i], "--diagnostic-format=json")) {
            options.json = true;
        }
    }
    FeContext context;
    FeProgram program = {0};
    fe_context_init(&context, "<command-line>", "", 0);
    char *source = NULL;
    int result = 1;
    if (!fe_parse_options(argc, argv, &options)) {
        fe_error(&context, (FeSpan){0, 1, 1}, "E_USAGE",
                 "command arguments do not match the selected operation");
        snprintf(context.diagnostic.help, sizeof(context.diagnostic.help),
                 "Run 'fermi help' for available commands or 'fermi help COMMAND' for required "
                 "arguments and output options.");
        result = 2;
        goto finish;
    }
    if (options.help || options.version) {
        if (options.version) {
            puts("fermi 0.2.0-pilot");
        } else {
            fe_print_help(options.command);
        }
        result = 0;
        goto finish;
    }
    context.path = options.source;
    FILE *input = fe_open_source(&context, options.source);
    if (!input) {
        goto finish;
    }
    source = malloc(FE_SOURCE_LIMIT + 1);
    if (!source) {
        fclose(input);
        fe_error(&context, (FeSpan){0, 1, 1}, "E_LIMIT", "cannot allocate source buffer");
        goto finish;
    }
    size_t length = fread(source, 1, FE_SOURCE_LIMIT + 1, input);
    bool readable = !ferror(input);
    if (fclose(input)) {
        readable = false;
    }
    if (!readable) {
        fe_error(&context, (FeSpan){0, 1, 1}, "E_IO", "cannot read source");
        goto finish;
    }
    if (!fe_context_init(&context, options.source, source, length) ||
        !fe_load_project(&context, &program) || !fe_check(&program) || !fe_analyze(&program)) {
        goto finish;
    }
    result = 0;
    if (options.command == FE_COMMAND_BUILD) {
        result =
            options.library
                ? fe_build_library(&program, options.source, options.output, options.optimization)
                : fe_build_executable(&program, options.output, options.optimization);
    } else if (options.command == FE_COMMAND_RUN || options.command == FE_COMMAND_TEST) {
        result = fe_run_executable(&program, &options);
    } else if (options.command == FE_COMMAND_EXPLAIN) {
        explain(&program);
    } else {
        printf("checked %s\n", options.source);
    }
finish:
    fe_render_diagnostic(stderr, program.ctx ? program.ctx : &context, options.json,
                         isatty(STDERR_FILENO) && !getenv("NO_COLOR"));
    fe_destroy_project(&program);
    fe_context_destroy(&context);
    free(source);
    return result;
}
