#ifndef FERMI_EMIT_H
#define FERMI_EMIT_H
#include "fermi/fe.h"
unsigned fe_lower_address(FeEmitter *, FeExpr *);
unsigned fe_lower_view(FeEmitter *, FeExpr *, FeType *);
unsigned fe_emit_temp(FeEmitter *, FeType *);
void fe_emit_arithmetic(FeEmitter *, int, FeType *, unsigned, unsigned, unsigned);
/* Internal functions publish their result through one function-scope slot. */
void fe_emit_return(FeEmitter *, unsigned value);
bool fe_emit_function_body(FeEmitter *);
bool fe_plan_owners(FeEmitter *);
void fe_clear_temp(FeEmitter *, FeType *, unsigned);
void fe_drop_value(FeEmitter *, FeType *, const char *);
void fe_drop_symbol(FeEmitter *, FeSymbol *);
void fe_drop_expression(FeEmitter *, FeExpr *);
void fe_drop_scope(FeEmitter *, FeStmt *);
void fe_drop_function(FeEmitter *);
bool fe_emit_memory_runtime(FeProgram *, FILE *);
void fe_emit_memory_native(FeEmitter *);
void fe_emit_io_write(FeEmitter *);
void fe_emit_io_read(FeEmitter *);
void fe_emit_env_read(FeEmitter *);
bool fe_emit_runtime_file(FeProgram *, FILE *, const char *, const char *);
bool fe_uses_arguments(FeProgram *);
bool fe_emit_arguments_runtime(FeProgram *, FILE *);
void fe_emit_arguments_native(FeEmitter *);
bool fe_uses_clock(FeProgram *);
void fe_emit_clock_read(FeEmitter *);
bool fe_uses_files(FeProgram *);
bool fe_emit_file_runtime(FeProgram *, FILE *);
void fe_emit_file_native(FeEmitter *);
#endif
