#ifndef FERMI_DRIVER_H
#define FERMI_DRIVER_H
#include "fermi/fe.h"

typedef enum {
    FE_COMMAND_NONE,
    FE_COMMAND_BUILD,
    FE_COMMAND_CHECK,
    FE_COMMAND_EXPLAIN,
    FE_COMMAND_RUN,
    FE_COMMAND_TEST
} FeCommand;
typedef struct {
    FeCommand command;
    const char *source, *output, *optimization;
    char **arguments;
    int argument_count;
    bool library, json, help, version, no_cache;
} FeOptions;
bool fe_parse_options(int, char **, FeOptions *);
void fe_print_help(FeCommand);
extern const char *const fe_executable_flags[];
bool fe_emit_executable(FeProgram *, FILE *);
bool fe_emit_tests(FeProgram *, FILE *);
int fe_build_native(FeProgram *, const char *, const char *, bool (*)(FeProgram *, FILE *));
bool fe_compile_executable(FeProgram *, const char *, const char *, const char *);
/* 1: cached executable ready; 0: compilation failed; -1: use the uncached path. */
int fe_prepare_run_cache(FeProgram *, const FeOptions *, char output[4096]);
int fe_run_executable(FeProgram *, const FeOptions *);

FILE *fe_open_source(FeContext *, const char *);
bool fe_load_project(FeContext *, FeProgram *);
void fe_destroy_project(FeProgram *);
bool fe_run_tool(FeContext *, char *const arguments[]);
bool fe_backend_target(const char *compiler, char target[128]);
int fe_build_library(FeProgram *program, const char *source, const char *directory,
                     const char *optimization);
int fe_build_executable(FeProgram *, const char *output, const char *optimization);
#endif
