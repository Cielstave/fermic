#include "fermi/driver.h"

#include <string.h>

static FeCommand command_named(const char *name) {
    if (!strcmp(name, "build") || !strcmp(name, "b")) {
        return FE_COMMAND_BUILD;
    }
    if (!strcmp(name, "check") || !strcmp(name, "c")) {
        return FE_COMMAND_CHECK;
    }
    if (!strcmp(name, "explain")) {
        return FE_COMMAND_EXPLAIN;
    }
    if (!strcmp(name, "run") || !strcmp(name, "r")) {
        return FE_COMMAND_RUN;
    }
    if (!strcmp(name, "test") || !strcmp(name, "t")) {
        return FE_COMMAND_TEST;
    }
    return FE_COMMAND_NONE;
}

bool fe_parse_options(int argc, char **argv, FeOptions *options) {
    options->optimization = "-O2";
    options->help = argc == 1;
    bool format_seen = false, optimization_seen = false, directory_output = false;
    bool help_command = false;
    for (int i = 1; i < argc; ++i) {
        const char *arg = argv[i];
        if (!strcmp(arg, "--")) {
            if (options->command != FE_COMMAND_RUN || help_command) {
                return false;
            }
            options->argument_count = argc - i - 1;
            options->arguments = argv + i + 1;
            break;
        }
        if (!strcmp(arg, "-h") || !strcmp(arg, "--help")) {
            options->help = true;
        } else if (!strcmp(arg, "-V") || !strcmp(arg, "--version")) {
            options->version = true;
        } else if (!strncmp(arg, "--diagnostic-format=", 20)) {
            if (format_seen || (strcmp(arg + 20, "text") && strcmp(arg + 20, "json"))) {
                return false;
            }
            options->json = !strcmp(arg + 20, "json");
            format_seen = true;
        } else if (!options->command) {
            if (!strcmp(arg, "help") && !help_command) {
                help_command = options->help = true;
            } else {
                options->command = command_named(arg);
                if (!options->command) {
                    return false;
                }
            }
        } else if (help_command) {
            return false;
        } else if (!strcmp(arg, "--no-cache")) {
            if (options->no_cache) {
                return false;
            }
            options->no_cache = true;
        } else if (!strcmp(arg, "--library")) {
            if (options->library) {
                return false;
            }
            options->library = true;
        } else if (!strcmp(arg, "-O0") || !strcmp(arg, "-O2")) {
            if (optimization_seen) {
                return false;
            }
            options->optimization = arg;
            optimization_seen = true;
        } else if (!strcmp(arg, "-o") || !strcmp(arg, "--out-dir")) {
            if (++i == argc || options->output || !*argv[i]) {
                return false;
            }
            directory_output = !strcmp(arg, "--out-dir");
            options->output = argv[i];
        } else if (!*arg || *arg == '-' || options->source) {
            return false;
        } else {
            options->source = arg;
        }
    }
    if (options->help && options->version) {
        return false;
    }
    if (options->version) {
        return !options->command && !options->source && !options->output && !options->library &&
               !optimization_seen && !options->no_cache;
    }
    if (options->help) {
        return true;
    }
    if (options->command == FE_COMMAND_TEST) {
        if (options->library || options->output || options->no_cache) {
            return false;
        }
        if (!options->source) {
            options->source = "tests.fe";
        }
        return true;
    }
    if (options->command == FE_COMMAND_RUN) {
        if (options->library || directory_output) {
            return false;
        }
        if (!options->source) {
            options->source = "main.fe";
        }
        return true;
    }
    if (options->no_cache || !options->source) {
        return false;
    }
    if (options->command == FE_COMMAND_BUILD) {
        return options->output && directory_output == options->library;
    }
    return (options->command == FE_COMMAND_CHECK || options->command == FE_COMMAND_EXPLAIN) &&
           !options->output && !options->library && !strcmp(options->optimization, "-O2");
}

void fe_print_help(FeCommand command) {
    if (command == FE_COMMAND_NONE) {
        puts("Fermi native compiler\n\n"
             "Usage: fermi [OPTIONS] <COMMAND>\n\n"
             "Options:\n"
             "  -h, --help                       Print help\n"
             "  -V, --version                    Print compiler version\n"
             "      --diagnostic-format=<FORMAT>  Diagnostic format: text or json\n\n"
             "Commands:\n"
             "  build, b    Compile a native executable or C library\n"
             "  check, c    Check source without building native output\n"
             "  explain     Show work bounds and resource effects\n"
             "  run, r      Build and run a native executable\n"
             "  test, t     Build and run test functions\n"
             "  help        Print help for a command\n\n"
             "Run 'fermi help <COMMAND>' for command-specific options.");
        return;
    }
    if (command == FE_COMMAND_RUN) {
        puts("Build and run a Fermi program\n\n"
             "Usage: fermi run [FILE.fe] [OPTIONS] [-- ARGS...]\n\n"
             "Arguments:\n"
             "  [FILE.fe]  Entry source file (default: main.fe)\n"
             "  [ARGS...]  Program arguments, passed literally after --\n\n"
             "Options:\n"
             "      --no-cache  Build a temporary executable without cache reuse\n"
             "  -o <PROGRAM>  Keep the executable at this path\n"
             "  -O0, -O2      Native optimization level (default: -O2)\n\n"
             "Unchanged default-backend builds reuse executables in build/.fermi-run.\n"
             "The program inherits the working directory, environment, and standard streams.");
    } else if (command == FE_COMMAND_TEST) {
        puts("Build and run Fermi tests\n\n"
             "Usage: fermi test [FILE.fe] [-O0|-O2]\n\n"
             "The source defaults to tests.fe. Local testName and test_* functions take\n"
             "no arguments and return bool. True passes; false or a runtime fault fails.\n"
             "Tests run sequentially in one process; main is not called.\n"
             "The executable is temporary; test builds do not use the run cache.");
    } else if (command == FE_COMMAND_BUILD) {
        puts("Compile Fermi source to native output\n\n"
             "Usage: fermi build FILE.fe -o PROGRAM [-O0|-O2]\n"
             "       fermi build --library FILE.fe --out-dir DIR [-O0|-O2]\n\n"
             "Options:\n"
             "  -o <PROGRAM>     Executable output path\n"
             "      --library    Build a C library instead of an executable\n"
             "      --out-dir <DIR>  Library output directory\n"
             "  -O0, -O2         Native optimization level (default: -O2)");
    } else if (command == FE_COMMAND_CHECK) {
        puts("Check Fermi source without building native output\n\nUsage: fermi check FILE.fe");
    } else {
        puts("Show abstract work bounds and resource effects\n\nUsage: fermi explain FILE.fe");
    }
    puts("\nCommon options:\n"
         "  -h, --help                       Print help\n"
         "      --diagnostic-format=<FORMAT>  Diagnostic format: text or json");
}
