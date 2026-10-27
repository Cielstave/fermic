#ifndef FERMI_FE_H
#define FERMI_FE_H
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

#define FE_SOURCE_LIMIT (1024u * 1024u)
#define FE_MEMORY_LIMIT (64u * 1024u * 1024u)
#define FE_NODE_LIMIT 32768u
#define FE_DEPTH_LIMIT 128u
#define FE_LOCAL_LIMIT 512u
#define FE_PARAM_LIMIT 16u
#define FE_FUNCTION_LIMIT 128u
#define FE_MATCH_CASE_LIMIT 128u
#define FE_MODULE_NAME_LIMIT 1024u

typedef struct {
    size_t offset;
    unsigned line, column;
} FeSpan;
typedef enum { FE_DIAGNOSTIC_ERROR, FE_DIAGNOSTIC_INTERNAL } FeSeverity;
typedef struct {
    const char *code;
    FeSeverity severity;
    FeSpan span, end;
    char message[384];
    FeSpan related;
    char note[384], help[384];
} FeDiagnostic;
typedef struct FeArena FeArena;
typedef struct FeContext FeContext;
struct FeContext {
    FeContext *budget_owner;
    const char *path, *source;
    size_t length, allocated, nodes;
    FeArena *arena;
    FeDiagnostic diagnostic;
};
/* The caller retains source/path storage until context destruction. Arena
 * allocations and all syntax nodes share the context lifetime. */
bool fe_context_init(FeContext *context, const char *path, const char *source, size_t length);
void fe_context_destroy(FeContext *context);
void *fe_alloc(FeContext *context, size_t size);
/* Copy length bytes and append a terminator. Nonempty input must be live for
 * length bytes; NULL is accepted only for empty input. Size limits are checked
 * before terminator arithmetic or source access. */
char *fe_string(FeContext *context, const char *text, size_t length);
bool fe_error(FeContext *, FeSpan, const char *, const char *, ...);
void fe_print_diagnostic(const FeContext *);
void fe_render_diagnostic(FILE *, const FeContext *, bool json, bool color);

/* Punctuation uses its ASCII code. Multi-character tokens start above ASCII. */
typedef enum {
    FE_T_EOF = 256,
    FE_T_NAME,
    FE_T_INTEGER,
    FE_T_STRING,
    FE_T_FUNC,
    FE_T_NATIVE,
    FE_T_EXPORT,
    FE_T_USE,
    FE_T_PUB,
    FE_T_ENUM,
    FE_T_STRUCT,
    FE_T_TYPE,
    FE_T_LET,
    FE_T_VAR,
    FE_T_IF,
    FE_T_ELSE,
    FE_T_FOR,
    FE_T_WHILE,
    FE_T_RETURN,
    FE_T_TRY,
    FE_T_BREAK,
    FE_T_CONTINUE,
    FE_T_MATCH,
    FE_T_CASE,
    FE_T_READ,
    FE_T_EDIT,
    FE_T_TRUE,
    FE_T_FALSE,
    FE_T_ARROW,
    FE_T_EQ,
    FE_T_NE,
    FE_T_LE,
    FE_T_GE,
    FE_T_AND,
    FE_T_OR,
    FE_T_ADD_ASSIGN,
    FE_T_SUB_ASSIGN,
    FE_T_MUL_ASSIGN,
    FE_T_DIV_ASSIGN,
    FE_T_REM_ASSIGN,
    FE_T_RANGE,
    FE_T_SHL,
    FE_T_SHR
} FeTokenKind;
typedef struct {
    int kind;
    FeSpan span;
    const char *text;
    size_t length;
    uint64_t integer;
} FeToken;
typedef struct {
    FeToken *items;
    size_t count;
} FeTokens;
bool fe_lex(FeContext *, FeTokens *);
/* Entry-module tests use testName or the established test_name form. */
bool fe_isTestName(const char *name);

typedef enum {
    TY_VOID,
    TY_BOOL,
    TY_U8,
    TY_I8,
    TY_U16,
    TY_I16,
    TY_I32,
    TY_U32,
    TY_I64,
    TY_U64,
    TY_USIZE,
    TY_ENUM,
    TY_STRUCT,
    TY_ARRAY,
    TY_READ,
    TY_EDIT,
    TY_RESULT,
    TY_BUFFER,
    TY_BUFFER_EDIT,
    TY_FILE,
    TY_FILE_EDIT,
    TY_STR,
    TY_STRUCT_READ,
    TY_STRUCT_EDIT
} FeTypeKind;
typedef struct FeModule FeModule;
typedef struct FeImport FeImport;
struct FeImport {
    const char *alias;
    FeModule *module;
    FeSpan span;
    FeImport *next;
};
struct FeModule {
    const char *name, *path;
    FeContext *ctx;
    FeImport *imports;
    bool loading, standard;
    FeModule *next;
};
typedef struct FeType FeType;
typedef struct FeField FeField;
struct FeField {
    bool private;
    const char *name;
    FeType *type;
    unsigned index;
    FeField *next;
};
struct FeType {
    FeModule *module;
    bool public, payload;
    FeTypeKind kind;
    unsigned id, nesting, resource_effects;
    uint64_t cells;
    const char *name;
    FeType *base, *error;
    uint64_t count;
    FeField *fields;
    FeType *next;
};
typedef struct FeSymbol FeSymbol;
typedef struct FeExpr FeExpr;
typedef struct FeOwnerTemp FeOwnerTemp;
typedef struct FeStmt FeStmt;
typedef struct FeFunction FeFunction;
typedef struct FeCase FeCase;
struct FeSymbol {
    const char *name;
    FeType *type;
    unsigned id, depth;
    bool mutable, parameter;
    FeSymbol *next;
};
typedef enum {
    EX_INT,
    EX_STRING,
    EX_BOOL,
    EX_NAME,
    EX_UNARY,
    EX_BINARY,
    EX_CALL,
    EX_INDEX,
    EX_FIELD,
    EX_ARRAY,
    EX_REPEAT,
    EX_COPY,
    EX_MOVE,
    EX_WIDEN,
    EX_NARROW,
    EX_OK,
    EX_ERR,
    EX_STRUCT,
    EX_SLICE,
    EX_TRY,
    EX_VARIANT
} FeExprKind;
struct FeExpr {
    FeExprKind kind;
    FeSpan span;
    FeType *type, *cast, *type_argument;
    FeSymbol *symbol;
    FeFunction *function;
    FeField *field;
    const char *name, *label;
    int op, access;
    uint64_t integer;
    unsigned owner_temp;
    FeOwnerTemp *owner_before, *owner_last;
    /* Unary/member: a=operand. Binary/index: a=left/base, b=right/index.
     * Call: a=target, b=arguments. Literal aggregate: a=elements.
     * Repeated array: a=value, integer=count; value is evaluated once.
     * Slice: a=buffer, b=start, c=end. next links argument/element siblings. */
    FeExpr *a, *b, *c, *next;
};
typedef enum {
    ST_BLOCK,
    ST_DECL,
    ST_ASSIGN,
    ST_EXPR,
    ST_IF,
    ST_FOR,
    ST_WHILE,
    ST_RETURN,
    ST_BREAK,
    ST_CONTINUE,
    ST_MATCH
} FeStmtKind;
struct FeCase {
    bool payload_pattern, wildcard;
    /* Only scalar constants; case patterns cannot call or own resources. */
    FeExpr *literal;
    const char *owner, *variant, *name;
    FeField *field;
    FeType *type;
    FeSpan span;
    FeSymbol *symbol;
    FeStmt *body;
    FeCase *next;
};
typedef enum { FE_PROOF_NONE, FE_PROOF_I16_SUM } FeArithmeticProof;
typedef struct FeDrop FeDrop;
struct FeDrop {
    FeSymbol *symbol;
    FeDrop *next;
};
struct FeStmt {
    FeStmtKind kind;
    FeSpan span;
    FeStmt *next, *body, *other, *init, *step;
    FeExpr *a, *b;
    const char *name;
    FeType *type;
    FeSymbol *symbol;
    FeCase *cases;
    FeStmt *loop;
    FeDrop *drops;
    unsigned loop_label;
    bool has_break, has_continue;
    bool mutable;
    int op;
    uint64_t iterations;
    FeArithmeticProof arithmetic_proof;
    uint64_t proven_iterations;
};
struct FeFunction {
    FeModule *module;
    bool public;
    const char *name;
    FeSpan span;
    FeType *result, *type_argument;
    FeSymbol *params, *locals;
    unsigned id, local_count, param_count, visit;
    FeStmt *body;
    bool exported, budgeted, bounded, native, effects_declared;
    unsigned effects, allowed_effects, proven_overflows;
    uint64_t budget, work;
    FeFunction *next;
};
typedef struct FeGeneric FeGeneric;
struct FeGeneric {
    FeFunction *declaration;
    FeTokens tokens;
    FeContext *context;
    const char *parameter;
    size_t signature_start;
    unsigned arguments;
    bool data;
    FeGeneric *next;
};
typedef struct FeAlias FeAlias;
struct FeAlias {
    const char *name;
    FeModule *module;
    FeType *target;
    FeSpan span;
    bool public;
    FeAlias *next;
};
typedef struct {
    FeContext *ctx;
    FeModule *module, *modules;
    FeTokens tokens;
    FeType *types, *primitive[TY_USIZE + 1], *cast_error, *data_probe;
    FeFunction *functions;
    FeGeneric *generics;
    FeAlias *aliases;
    unsigned type_count, function_count, alias_count, generic_count;
} FeProgram;
FeType *fe_type(FeProgram *, FeTypeKind, FeType *, FeType *, uint64_t);
FeType *fe_named_type(FeProgram *, const char *);
FeModule *fe_imported_module(FeProgram *, const char *, size_t);
FeFunction *fe_named_function(FeProgram *, const char *, const FeType *);
FeGeneric *fe_named_generic(FeProgram *, const char *);
FeFunction *fe_instantiate(FeProgram *, FeGeneric *, FeType *);
bool fe_parse(FeContext *context, FeTokens tokens, FeProgram *program);
bool fe_parse_module(FeContext *, FeTokens, FeProgram *, FeModule *, size_t start);
bool fe_check(FeProgram *program);
bool fe_analyze(FeProgram *program);
enum {
    FE_EFFECT_IO_WRITE = 1u,
    FE_EFFECT_IO_READ = 2u,
    FE_EFFECT_MEMORY = 4u,
    FE_EFFECT_ENV_READ = 8u,
    FE_EFFECT_ARGS_READ = 16u,
    FE_EFFECT_CLOCK_READ = 32u,
    FE_EFFECT_FS_OPEN = 64u,
    FE_EFFECT_FS_READ = 128u,
    FE_EFFECT_FS_CLOSE = 256u,
    FE_EFFECT_FS_WRITE = 512u,
    FE_EFFECT_FS_SYNC = 1024u
};
void fe_analyze_effects(FeProgram *program);
bool fe_check_effects(FeProgram *program);
unsigned fe_effect_named(const char *name);
void fe_prove_ranges(FeProgram *program);
void fe_print_effects(FILE *, unsigned effects);
bool fe_is_data(const FeType *);
bool fe_is_integer(const FeType *);
unsigned fe_integer_bits(const FeType *);
bool fe_is_signed(const FeType *);
bool fe_is_payload_enum(const FeType *);
bool fe_is_aggregate(const FeType *);
bool fe_has_resource(const FeType *);
bool fe_is_owner_borrow(const FeType *);
unsigned fe_resource_effects(const FeType *);
uint64_t fe_type_cells(const FeType *);
const char *fe_type_name(const FeType *);
void fe_format_type(const FeType *, char *buffer, size_t capacity);

/* Typed expression trees are lowered to sequenced C temporaries; statement
 * control flow remains structured. No source expression is pasted into C. */
struct FeOwnerTemp {
    FeType *type;
    unsigned id;
    FeOwnerTemp *next;
};
typedef struct {
    FeProgram *program;
    FeFunction *function;
    FILE *out;
    FeOwnerTemp *owners;
    unsigned native_result;
    unsigned temporary, indent;
} FeEmitter;
void fe_emit_type(FILE *, const FeType *);
void fe_emit_line(FeEmitter *, const char *, ...);
unsigned fe_lower_expr(FeEmitter *, FeExpr *);
void fe_emit_stmt(FeEmitter *, FeStmt *);
bool fe_emit_library(FeProgram *, FILE *, FILE *, FILE *);
#endif
