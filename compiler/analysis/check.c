#include "fermi/fe.h"

/* Validate Unicode scalar encodings without accepting overlong sequences. */
static bool valid_utf8(const unsigned char *bytes, size_t length) {
    size_t offset = 0;
    while (offset < length) {
        unsigned first = bytes[offset++];
        if (first < 0x80) {
            continue;
        }
        unsigned count, value, minimum;
        if (first >= 0xc2 && first <= 0xdf) {
            count = 1;
            value = first & 0x1f;
            minimum = 0x80;
        } else if (first >= 0xe0 && first <= 0xef) {
            count = 2;
            value = first & 0x0f;
            minimum = 0x800;
        } else if (first >= 0xf0 && first <= 0xf4) {
            count = 3;
            value = first & 7;
            minimum = 0x10000;
        } else {
            return false;
        }
        if (length - offset < count) {
            return false;
        }
        while (count--) {
            unsigned byte = bytes[offset++];
            if ((byte & 0xc0) != 0x80) {
                return false;
            }
            value = (value << 6) | (byte & 0x3f);
        }
        if (value < minimum || value > 0x10ffff || (value >= 0xd800 && value <= 0xdfff)) {
            return false;
        }
    }
    return true;
}
#include <limits.h>
#include <setjmp.h>
#include <string.h>

typedef enum {
    BINDING_UNINITIALIZED,
    BINDING_READY,
    BINDING_MOVED,
    BINDING_MAYBE_READY
} BindingState;

static bool may_hold_value(BindingState state) {
    return state == BINDING_READY || state == BINDING_MAYBE_READY;
}

typedef struct LoopState LoopState;
struct LoopState {
    FeStmt *statement;
    unsigned start, body_start;
    BindingState breaks[FE_LOCAL_LIMIT], continues[FE_LOCAL_LIMIT];
    bool have_break, have_continue;
    LoopState *previous;
};

typedef struct {
    FeProgram *program;
    FeFunction *function;
    FeSymbol *visible[FE_LOCAL_LIMIT];
    unsigned count, depth, nesting;
    BindingState state[FE_LOCAL_LIMIT];
    FeSpan moved_at[FE_LOCAL_LIMIT];
    LoopState *loop;
    jmp_buf stop;
} Checker;
static void check_error(Checker *checker, FeSpan span, const char *code, const char *message) {
    fe_error(checker->program->ctx, span, code, "%s", message);
    longjmp(checker->stop, 1);
}

static void require_same_type(Checker *checker, FeSpan span, FeType *actual, FeType *expected) {
    if (actual != expected) {
        char actual_name[144], expected_name[144];
        fe_format_type(actual, actual_name, sizeof(actual_name));
        fe_format_type(expected, expected_name, sizeof(expected_name));
        FeContext *context = checker->program->ctx;
        fe_error(context, span, "E_TYPE", "expected '%s', found '%s'", expected_name, actual_name);
        if (fe_is_integer(actual) && fe_is_integer(expected)) {
            snprintf(context->diagnostic.help, sizeof(context->diagnostic.help),
                     "numeric conversions must be explicit; use widen for a lossless conversion or "
                     "handle the Result returned by narrow");
        }
        longjmp(checker->stop, 1);
    }
}

static FeSymbol *lookup_binding(Checker *checker, const char *n, FeSpan span) {
    for (unsigned i = checker->count; i > 0; --i) {
        if (!strcmp(checker->visible[i - 1]->name, n)) {
            return checker->visible[i - 1];
        }
    }
    fe_error(checker->program->ctx, span, "E_NAME", "cannot resolve binding '%s'", n);
    longjmp(checker->stop, 1);
    return NULL;
}

static FeSymbol *declare_binding(Checker *checker, const char *n, FeType *type, bool mut,
                                 FeSpan span) {
    if (fe_imported_module(checker->program, n, strlen(n))) {
        check_error(checker, span, "E_NAME", "binding conflicts with an imported module alias");
    }
    if (checker->count >= FE_LOCAL_LIMIT || checker->function->local_count >= FE_LOCAL_LIMIT) {
        check_error(checker, span, "E_LIMIT", "too many bindings");
    }
    for (unsigned i = 0; i < checker->count; ++i) {
        if (checker->visible[i]->depth == checker->depth && !strcmp(checker->visible[i]->name, n)) {
            check_error(checker, span, "E_NAME", "duplicate binding in scope");
        }
    }
    FeSymbol *symbol = fe_alloc(checker->program->ctx, sizeof(*symbol));
    if (!symbol) {
        longjmp(checker->stop, 1);
    }
    symbol->name = n;
    symbol->type = type;
    symbol->mutable = mut;
    symbol->depth = checker->depth;
    symbol->id = checker->function->local_count++;
    symbol->next = checker->function->locals;
    checker->function->locals = symbol;
    checker->visible[checker->count++] = symbol;
    return symbol;
}

static FeSymbol *storage_owner(FeExpr *expr) {
    if (!expr) {
        return NULL;
    }
    if (expr->kind == EX_NAME) {
        return expr->symbol;
    }
    if (expr->kind == EX_FIELD || expr->kind == EX_INDEX || expr->kind == EX_SLICE) {
        return storage_owner(expr->a);
    }
    return NULL;
}

static bool shared_storage(const FeExpr *expr) {
    if (expr->type->kind == TY_READ || expr->type->kind == TY_STRUCT_READ) {
        return true;
    }
    return (expr->kind == EX_FIELD || expr->kind == EX_INDEX || expr->kind == EX_SLICE) &&
           shared_storage(expr->a);
}

static bool writable_storage(const FeSymbol *owner) {
    return owner &&
           (owner->mutable || owner->type->kind == TY_EDIT || owner->type->kind == TY_BUFFER_EDIT ||
            owner->type->kind == TY_FILE_EDIT || owner->type->kind == TY_STRUCT_EDIT);
}

static bool shared_access(int access) {
    return access == TY_READ || access == TY_STRUCT_READ;
}

static bool uses_binding(FeExpr *expr, FeSymbol *symbol) {
    if (!expr) {
        return false;
    }
    return expr->symbol == symbol || uses_binding(expr->a, symbol) ||
           uses_binding(expr->b, symbol) || uses_binding(expr->c, symbol) ||
           uses_binding(expr->next, symbol);
}

/* A view's storage name is checked separately for aliasing. Its bounds can
 * still call functions that access a different argument's borrowed owner. */
static bool view_bounds_use(FeExpr *expr, FeSymbol *symbol) {
    if (expr->kind == EX_INDEX || expr->kind == EX_SLICE) {
        return uses_binding(expr->b, symbol) || uses_binding(expr->c, symbol) ||
               view_bounds_use(expr->a, symbol);
    }
    return expr->kind == EX_FIELD && view_bounds_use(expr->a, symbol);
}

static bool argument_uses(FeExpr *expr, FeSymbol *symbol) {
    /* Do not inspect expr->next: it belongs to another argument. */
    return expr->symbol == symbol || uses_binding(expr->a, symbol) ||
           uses_binding(expr->b, symbol) || uses_binding(expr->c, symbol);
}

static bool borrows_descriptor(FeExpr *expr, FeSymbol *owner) {
    for (; expr; expr = expr->next) {
        if ((expr->access == TY_BUFFER_EDIT || expr->access == TY_FILE_EDIT ||
             expr->access == TY_STRUCT_EDIT) &&
            storage_owner(expr) == owner) {
            return true;
        }
        if (borrows_descriptor(expr->a, owner) || borrows_descriptor(expr->b, owner) ||
            borrows_descriptor(expr->c, owner)) {
            return true;
        }
    }
    return false;
}

static bool transfer_required(FeExpr *expr) {
    return fe_is_aggregate(expr->type) &&
           (expr->kind == EX_NAME || expr->kind == EX_FIELD || expr->kind == EX_INDEX);
}

static void require_transfer(Checker *checker, FeExpr *expr) {
    if (transfer_required(expr)) {
        check_error(checker, expr->span, "E_MOVE",
                    "aggregate transfer requires copy(value) or move(value)");
    }
}

static uint64_t integer_max(FeType *type) {
    switch (type->kind) {
    case TY_U8:
        return UINT8_MAX;
    case TY_I8:
        return INT8_MAX;
    case TY_U16:
        return UINT16_MAX;
    case TY_I16:
        return INT16_MAX;
    case TY_I32:
        return INT32_MAX;
    case TY_U32:
        return UINT32_MAX;
    case TY_I64:
        return INT64_MAX;
    case TY_U64:
        return UINT64_MAX;
    case TY_USIZE:
        return SIZE_MAX;
    default:
        return 0;
    }
}

/* Unary integer literals acquire context just like bare integer literals.
 * This predicate excludes expressions with calls or ownership effects. */
static bool is_integer_literal(const FeExpr *expr) {
    while (expr->kind == EX_UNARY && (expr->op == '+' || expr->op == '-' || expr->op == '~')) {
        expr = expr->a;
    }
    return expr->kind == EX_INT;
}

static FeType *check_expression(Checker *, FeExpr *, FeType *);
static void require_handled_results(Checker *, unsigned, FeSpan);
static FeType *make_type(Checker *checker, FeTypeKind kind, FeType *base, FeType *err) {
    FeType *type = fe_type(checker->program, kind, base, err, 0);
    if (!type) {
        longjmp(checker->stop, 1);
    }
    return type;
}

static void require_field_access(Checker *checker, FeType *type, FeField *field, FeSpan span) {
    if (field->private && type->module != checker->program->module) {
        fe_error(checker->program->ctx, span, "E_ACCESS",
                 "field '%s.%s' is private to its defining module", type->name, field->name);
        longjmp(checker->stop, 1);
    }
}

static FeField *find_field(Checker *checker, FeType *type, const char *n, FeSpan stmt) {
    for (FeField *f = type->fields; f; f = f->next) {
        if (!strcmp(f->name, n)) {
            require_field_access(checker, type, f, stmt);
            return f;
        }
    }
    check_error(checker, stmt, "E_NAME", "unknown field or variant");
    return NULL;
}

static bool static_disjoint(FeExpr *a, FeExpr *b) {
    /* Bounds of nested slices are relative to different origins. Only direct
     * slices of a binding share the coordinate system compared here. */
    return a->kind == EX_SLICE && b->kind == EX_SLICE && a->a->kind == EX_NAME &&
           b->a->kind == EX_NAME && a->b->kind == EX_INT && a->c->kind == EX_INT &&
           b->b->kind == EX_INT && b->c->kind == EX_INT &&
           (a->c->integer <= b->b->integer || b->c->integer <= a->b->integer);
}

/* Qualified names are resolved to symbols before lowering; no path is emitted as C. */
static void qualify_import(Checker *checker, FeExpr *expr) {
    if (expr->kind != EX_FIELD || expr->a->kind != EX_NAME ||
        !fe_imported_module(checker->program, expr->a->name, strlen(expr->a->name))) {
        return;
    }
    size_t size = strlen(expr->a->name) + strlen(expr->name) + 2;
    char *name = fe_alloc(checker->program->ctx, size);
    if (!name) {
        longjmp(checker->stop, 1);
    }
    snprintf(name, size, "%s.%s", expr->a->name, expr->name);
    expr->kind = EX_NAME;
    expr->name = name;
    expr->a = NULL;
}

static void check_call(Checker *checker, FeExpr *expr, FeType *expected) {
    FeExpr *target = expr->a, *args = expr->b;
    qualify_import(checker, target);
    if (expr->type_argument && (target->kind != EX_NAME || !fe_is_data(expr->type_argument) ||
                                fe_named_type(checker->program, target->name) ||
                                !strcmp(target->name, "copy") || !strcmp(target->name, "move"))) {
        check_error(checker, expr->span, "E_GENERIC",
                    "explicit type arguments require a generic function and a data type");
    }
    if (target->kind == EX_FIELD) {
        qualify_import(checker, target->a);
        FeType *type =
            target->a->kind == EX_NAME ? fe_named_type(checker->program, target->a->name) : NULL;
        if (type && type->kind == TY_ENUM) {
            FeField *variant = find_field(checker, type, target->name, expr->span);
            if (!variant->type) {
                check_error(checker, expr->span, "E_CALL",
                            "fieldless enum variant does not take parentheses");
            }
            if (!args || args->next || args->label || args->access) {
                check_error(checker, expr->span, "E_CALL",
                            "enum variant constructor requires one payload value");
            }
            check_expression(checker, args, variant->type);
            require_transfer(checker, args);
            expr->kind = EX_VARIANT;
            expr->type = type;
            expr->field = variant;
            expr->a = args;
            expr->b = NULL;
            return;
        }
    }
    if (target->kind == EX_FIELD && target->a->kind == EX_NAME &&
        !strcmp(target->a->name, "Result")) {
        if (!expected || expected->kind != TY_RESULT) {
            check_error(checker, expr->span, "E_TYPE",
                        "Result constructor needs an expected Result type");
        }
        bool ok = !strcmp(target->name, "ok");
        if (!ok && strcmp(target->name, "err")) {
            check_error(checker, expr->span, "E_NAME", "unknown Result constructor");
        }
        bool unit = ok && expected->base->kind == TY_VOID;
        if (unit) {
            if (args) {
                check_error(checker, expr->span, "E_CALL",
                            "unit Result success takes no arguments");
            }
        } else {
            if (!args || args->next || args->access || args->label) {
                check_error(checker, expr->span, "E_CALL", "Result constructor takes one value");
            }
            check_expression(checker, args, ok ? expected->base : expected->error);
            require_transfer(checker, args);
        }
        expr->kind = ok ? EX_OK : EX_ERR;
        expr->a = args;
        expr->b = NULL;
        expr->type = expected;
        return;
    }
    if (target->kind != EX_NAME) {
        check_error(checker, expr->span, "E_CALL", "call target must name a function");
    }
    if (!strcmp(target->name, "copy") || !strcmp(target->name, "move")) {
        if (!args || args->next || args->access || args->label) {
            check_error(checker, expr->span, "E_CALL", "copy/move requires one owning binding");
        }
        check_expression(checker, args, expected);
        expr->kind = !strcmp(target->name, "copy") ? EX_COPY : EX_MOVE;
        bool place = args->kind == EX_NAME ||
                     (expr->kind == EX_COPY && (args->kind == EX_FIELD || args->kind == EX_INDEX));
        if (!place ||
            (!fe_is_aggregate(args->type) && !(expr->kind == EX_COPY && fe_is_data(args->type)))) {
            check_error(checker, expr->span, "E_MOVE",
                        "copy requires a data place; move requires a whole aggregate binding");
        }
        if (expr->kind == EX_COPY && fe_has_resource(args->type)) {
            check_error(checker, expr->span, "E_MOVE",
                        "resource values cannot be copied; use move(value)");
        }
        if (expr->kind == EX_MOVE) {
            checker->state[args->symbol->id] = BINDING_MOVED;
            checker->moved_at[args->symbol->id] = expr->span;
        }
        expr->a = args;
        expr->b = NULL;
        expr->type = args->type;
        return;
    }
    FeType *type = fe_named_type(checker->program, target->name);
    if (type && type->kind == TY_FILE) {
        check_error(checker, expr->span, "E_TYPE",
                    "opaque file owners cannot be constructed directly");
    }
    if (type && type->kind == TY_STRUCT) {
        FeField *f = type->fields;
        for (FeExpr *a = args; a; a = a->next) {
            if (!f || !a->label || strcmp(a->label, f->name) || a->access) {
                check_error(checker, a->span, "E_CALL",
                            "struct arguments must name every field in declaration order");
            }
            require_field_access(checker, type, f, a->span);
            check_expression(checker, a, f->type);
            require_transfer(checker, a);
            f = f->next;
        }
        if (f) {
            check_error(checker, expr->span, "E_CALL", "missing struct field");
        }
        expr->kind = EX_STRUCT;
        expr->a = args;
        expr->b = NULL;
        expr->type = type;
        return;
    }
    FeFunction *f = fe_named_function(checker->program, target->name, expr->type_argument);
    if (!f) {
        FeGeneric *generic = fe_named_generic(checker->program, target->name);
        if (generic) {
            if (strchr(target->name, '.') && !generic->declaration->public) {
                check_error(checker, expr->span, "E_PRIVATE", "function is private to its module");
            }
            if (!expr->type_argument) {
                check_error(checker, expr->span, "E_GENERIC",
                            "generic function requires an explicit type argument");
            }
            if (!(generic->data ? fe_is_data(expr->type_argument)
                                : fe_is_integer(expr->type_argument) &&
                                      (generic->arguments & (1u << expr->type_argument->kind)))) {
                fe_error(checker->program->ctx, expr->span, "E_GENERIC",
                         "function '%s' does not support type argument '%s'", target->name,
                         fe_type_name(expr->type_argument));
                snprintf(checker->program->ctx->diagnostic.help,
                         sizeof(checker->program->ctx->diagnostic.help),
                         "select a type admitted by the function's declared constraint");
                longjmp(checker->stop, 1);
            }
            if (checker->program->function_count == FE_FUNCTION_LIMIT) {
                check_error(checker, expr->span, "E_GENERIC",
                            "too many functions, including requested generic specializations");
            }
            f = fe_instantiate(checker->program, generic, expr->type_argument);
            if (!f) {
                longjmp(checker->stop, 1);
            }
        }
    }
    if (!f) {
        const char *dot = strchr(target->name, '.');
        FeModule *module =
            dot ? fe_imported_module(checker->program, target->name, (size_t)(dot - target->name))
                : checker->program->module;
        for (FeFunction *candidate = checker->program->functions; candidate;
             candidate = candidate->next) {
            if (dot && candidate->module == module && !strcmp(candidate->name, dot + 1) &&
                !candidate->public) {
                FeContext *context = checker->program->ctx;
                fe_error(context, expr->span, "E_PRIVATE", "function '%s' is private to its module",
                         target->name);
                snprintf(context->diagnostic.help, sizeof(context->diagnostic.help),
                         "declare the function pub if it is part of the module interface");
                longjmp(checker->stop, 1);
            }
        }
        for (FeFunction *candidate = checker->program->functions; candidate;
             candidate = candidate->next) {
            if (candidate->module == module &&
                !strcmp(candidate->name, dot ? dot + 1 : target->name)) {
                if (candidate->type_argument && expr->type_argument) {
                    FeContext *context = checker->program->ctx;
                    fe_error(context, expr->span, "E_GENERIC",
                             "function '%s' does not support type argument '%s'", target->name,
                             fe_type_name(expr->type_argument));
                    snprintf(
                        context->diagnostic.help, sizeof(context->diagnostic.help),
                        "select a type admitted by the function's declared integer constraint");
                    longjmp(checker->stop, 1);
                }
                check_error(checker, expr->span, "E_GENERIC",
                            candidate->type_argument
                                ? "generic function requires an explicit integer type argument"
                                : "function does not accept a type argument");
            }
        }
        fe_error(checker->program->ctx, expr->span, "E_NAME", "unknown function '%s'",
                 target->name);
        longjmp(checker->stop, 1);
    }
    FeSymbol *param = f->params;
    for (FeExpr *a = args; a; a = a->next) {
        if (!param || a->label) {
            check_error(checker, a->span, "E_CALL",
                        "wrong argument count or named function argument");
        }
        if (fe_is_owner_borrow(param->type)) {
            bool structure =
                param->type->kind == TY_STRUCT_READ || param->type->kind == TY_STRUCT_EDIT;
            bool reading = param->type->kind == TY_STRUCT_READ;
            if (a->access != (reading ? TY_READ : TY_EDIT)) {
                check_error(checker, a->span, "E_ACCESS",
                            reading ? "struct borrow requires explicit read access"
                                    : "borrow requires explicit edit access");
            }
            FeType *actual = check_expression(checker, a, NULL);
            FeType *base = fe_is_owner_borrow(actual) ? actual->base : actual;
            FeSymbol *owner = storage_owner(a);
            bool place =
                a->kind == EX_NAME || a->kind == EX_FIELD || (structure && a->kind == EX_INDEX);
            if (!place || !owner || base != param->type->base) {
                check_error(checker, a->span, "E_ACCESS",
                            "borrow requires stable storage of the declared type");
            }
            if (!reading && !writable_storage(owner)) {
                check_error(checker, a->span, "E_MUT", "edit borrow requires mutable storage");
            }
            a->access = param->type->kind;
        } else if (param->type->kind == TY_READ || param->type->kind == TY_EDIT) {
            /* A literal already denotes immutable static bytes, with no owner to borrow. */
            if (!a->access && a->kind == EX_STRING && param->type->kind == TY_READ &&
                param->type->base->kind == TY_U8) {
                a->access = TY_READ;
            }
            if (a->access != (int)param->type->kind) {
                check_error(checker, a->span, "E_ACCESS",
                            "argument requires explicit read/edit access");
            }
            FeType *actual = check_expression(checker, a, NULL);
            if (actual->kind == TY_BUFFER_EDIT) {
                actual = actual->base;
            }
            if (actual->kind == TY_STR &&
                (param->type->kind != TY_READ || param->type->base->kind != TY_U8)) {
                check_error(checker, a->span, "E_TYPE", "str permits immutable byte access only");
            }
            if (actual->kind != TY_STR && ((actual->kind != TY_ARRAY && actual->kind != TY_READ &&
                                            actual->kind != TY_EDIT && actual->kind != TY_BUFFER) ||
                                           actual->base != param->type->base)) {
                check_error(checker, a->span, "E_TYPE", "view element types must match");
            }
            FeSymbol *owner = storage_owner(a);
            if (!owner && actual->kind != TY_STR &&
                !(a->kind == EX_STRING && a->access == TY_READ)) {
                check_error(checker, a->span, "E_ACCESS",
                            "view requires stable caller-owned storage");
            }
            if (a->access == TY_EDIT && (actual->kind == TY_READ || !writable_storage(owner))) {
                check_error(checker, a->span, "E_MUT", "edit requires mutable storage");
            }
        } else {
            if (a->access) {
                check_error(checker, a->span, "E_ACCESS",
                            "value parameter does not accept access modifier");
            }
            check_expression(checker, a, param->type);
            require_transfer(checker, a);
        }
        param = param->next;
    }
    if (param) {
        check_error(checker, expr->span, "E_CALL", "missing arguments");
    }
    for (FeExpr *a = args; a; a = a->next) {
        if (a->access) {
            FeSymbol *owner = storage_owner(a);
            if (!owner) {
                continue;
            }
            for (FeExpr *b = args; b; b = b->next) {
                if (a != b) {
                    if (b->access && storage_owner(b) == owner) {
                        if ((!shared_access(a->access) || !shared_access(b->access)) &&
                            !static_disjoint(a, b)) {
                            check_error(checker, b->span, "E_ALIAS",
                                        "overlapping views or disjointness cannot be proved");
                        }
                        if (view_bounds_use(b, owner)) {
                            check_error(
                                checker, b->span, "E_ALIAS",
                                "evaluate slice bounds using a borrowed owner before the call");
                        }
                    } else if (argument_uses(b, owner)) {
                        check_error(checker, b->span, "E_ALIAS",
                                    "evaluate other uses of a borrowed owner before the call");
                    }
                }
            }
        }
    }
    expr->function = f;
    expr->type = f->result;
}

static FeType *check_expression(Checker *checker, FeExpr *expr, FeType *expected) {
    if (++checker->nesting > FE_DEPTH_LIMIT) {
        check_error(checker, expr->span, "E_LIMIT", "expression depth limit exceeded");
    }
    FeType *u = checker->program->primitive[TY_USIZE],
           *boolean = checker->program->primitive[TY_BOOL];
    switch (expr->kind) {
    case EX_INT:
        expr->type = expected ? expected : checker->program->primitive[TY_I64];
        if (!fe_is_integer(expr->type)) {
            check_error(checker, expr->span, "E_TYPE", "integer literal requires an integer type");
        }
        if (expr->integer > integer_max(expr->type)) {
            check_error(checker, expr->span, "E_RANGE", "integer literal does not fit its type");
        }
        break;
    case EX_STRING:
        if (expected && expected->kind == TY_STR) {
            if (!valid_utf8((const unsigned char *)expr->name, (size_t)expr->integer)) {
                check_error(checker, expr->span, "E_STRING", "str literal requires valid UTF-8");
            }
            expr->type = expected;
        } else {
            expr->type = make_type(checker, TY_READ, checker->program->primitive[TY_U8], NULL);
        }
        break;
    case EX_BOOL:
        expr->type = boolean;
        break;
    case EX_NAME:
        expr->symbol = lookup_binding(checker, expr->name, expr->span);
        expr->type = expr->symbol->type;
        if (checker->state[expr->symbol->id] != BINDING_READY) {
            FeContext *context = checker->program->ctx;
            bool definitely_moved = checker->state[expr->symbol->id] == BINDING_MOVED;
            FeSpan moved = checker->moved_at[expr->symbol->id];
            const char *message =
                definitely_moved ? "cannot use binding '%s' after ownership has been transferred"
                : moved.line     ? "binding '%s' may be uninitialized or moved on this path"
                                 : "binding '%s' is not initialized on every path";
            fe_error(context, expr->span, "E_INIT", message, expr->name);
            if (moved.line) {
                context->diagnostic.related = moved;
                snprintf(context->diagnostic.note, sizeof(context->diagnostic.note), "%s",
                         definitely_moved ? "ownership was transferred here"
                                          : "ownership may have been transferred here");
            }
            snprintf(
                context->diagnostic.help, sizeof(context->diagnostic.help), "%s",
                definitely_moved && fe_has_resource(expr->type)
                    ? "use the value before moving it, or borrow it through a read/edit parameter"
                : definitely_moved
                    ? "use the value before moving it, or create an explicit copy before the "
                      "transfer"
                    : "initialize the binding on every path that reaches this expression");
            longjmp(checker->stop, 1);
        }
        break;
    case EX_UNARY: {
        FeType *literal_type = expected ? expected : checker->program->primitive[TY_I64];
        if (expr->op == '-' && expr->a->kind == EX_INT && fe_is_signed(literal_type) &&
            expr->a->integer == integer_max(literal_type) + 1) {
            expr->a->type = literal_type;
            expr->type = literal_type;
            break;
        }
        expr->type = check_expression(checker, expr->a, expr->op == '!' ? boolean : expected);
        if (expr->op != '!' && !fe_is_integer(expr->type)) {
            check_error(checker, expr->span, "E_TYPE", "numeric unary operator requires integer");
        }
        if (expr->op == '~' && fe_is_signed(expr->type)) {
            check_error(checker, expr->span, "E_TYPE",
                        "bitwise complement requires an unsigned integer");
        }
        if (expr->op == '-' && !fe_is_signed(expr->type)) {
            check_error(checker, expr->span, "E_TYPE", "negation requires signed integer");
        }
        break;
    }
    case EX_BINARY: {
        bool logical = expr->op == FE_T_AND || expr->op == FE_T_OR;
        bool shift = expr->op == FE_T_SHL || expr->op == FE_T_SHR;
        bool bitwise = shift || expr->op == '&' || expr->op == '|' || expr->op == '^';
        bool comparison = expr->op == FE_T_EQ || expr->op == FE_T_NE || expr->op == '<' ||
                          expr->op == '>' || expr->op == FE_T_LE || expr->op == FE_T_GE;
        BindingState before[FE_LOCAL_LIMIT];
        memcpy(before, checker->state, sizeof(before));
        FeType *hint = logical ? boolean : (comparison ? NULL : expected);
        if (shift) {
            check_expression(checker, expr->a, hint);
            check_expression(checker, expr->b, u);
        } else if (is_integer_literal(expr->a) && !is_integer_literal(expr->b)) {
            /* Literal has no effects; infer its type from the right operand. */
            FeType *right = check_expression(checker, expr->b, hint);
            check_expression(checker, expr->a, right);
        } else {
            check_expression(checker, expr->a, hint);
            check_expression(checker, expr->b, expr->a->type);
        }
        if (logical) {
            for (unsigned i = 0; i < FE_LOCAL_LIMIT; ++i) {
                if (before[i] != checker->state[i]) {
                    check_error(
                        checker, expr->span, "E_MOVE",
                        "ownership-changing expressions are not allowed in short-circuit operands");
                }
            }
        } else if (!fe_is_integer(expr->a->type) &&
                   !(comparison && (expr->op == FE_T_EQ || expr->op == FE_T_NE) &&
                     (expr->a->type->kind == TY_BOOL ||
                      (expr->a->type->kind == TY_ENUM && !fe_is_payload_enum(expr->a->type))))) {
            check_error(checker, expr->span, "E_TYPE", "operator requires integers");
        }
        if (bitwise && fe_is_signed(expr->a->type)) {
            check_error(checker, expr->span, "E_TYPE",
                        "bitwise operators require unsigned integers");
        }
        if (shift && expr->b->kind == EX_INT &&
            expr->b->integer >= fe_integer_bits(expr->a->type)) {
            check_error(checker, expr->b->span, "E_CONST",
                        "shift count must be less than the operand bit width");
        }
        expr->type = (logical || comparison) ? boolean : expr->a->type;
        if ((expr->op == '/' || expr->op == '%') && expr->b->kind == EX_INT && !expr->b->integer) {
            check_error(checker, expr->span, "E_CONST", "division by zero");
        }
        if (expr->a->kind == EX_INT && expr->b->kind == EX_INT && !comparison && !logical) {
            uint64_t a = expr->a->integer, b = expr->b->integer, max = integer_max(expr->type);
            if ((expr->op == '+' && a > max - b) || (expr->op == '*' && b && a > max / b) ||
                (expr->op == '-' && !fe_is_signed(expr->type) && a < b)) {
                check_error(checker, expr->span, "E_CONST", "constant arithmetic overflow");
            }
        }
        break;
    }
    case EX_TRY: {
        FeType *result = checker->function->result;
        if (result->kind != TY_RESULT) {
            check_error(checker, expr->span, "E_TRY",
                        "try requires an enclosing function returning Result");
        }
        FeType *operand = check_expression(checker, expr->a, NULL);
        if (operand->kind != TY_RESULT) {
            check_error(checker, expr->span, "E_TRY", "try operand must have a Result type");
        }
        if (operand->error != result->error) {
            char actual[144], expected_error[144];
            fe_format_type(operand->error, actual, sizeof(actual));
            fe_format_type(result->error, expected_error, sizeof(expected_error));
            fe_error(checker->program->ctx, expr->span, "E_TRY",
                     "try cannot propagate error '%s' as '%s'", actual, expected_error);
            snprintf(checker->program->ctx->diagnostic.help,
                     sizeof(checker->program->ctx->diagnostic.help),
                     "match the Result and convert its error explicitly");
            longjmp(checker->stop, 1);
        }
        if (expr->a->kind == EX_NAME) {
            checker->state[expr->a->symbol->id] = BINDING_MOVED;
            checker->moved_at[expr->a->symbol->id] = expr->span;
        } else {
            require_transfer(checker, expr->a);
        }
        /* The error edge is a return, even when the success edge continues. */
        require_handled_results(checker, 0, expr->span);
        expr->type = operand->base;
        expr->function = checker->function;
        break;
    }
    case EX_CALL:
        check_call(checker, expr, expected);
        break;
    case EX_FIELD: {
        qualify_import(checker, expr->a);
        FeType *nominal =
            expr->a->kind == EX_NAME ? fe_named_type(checker->program, expr->a->name) : NULL;
        if (nominal && nominal->kind == TY_ENUM) {
            expr->type = nominal;
            expr->field = find_field(checker, nominal, expr->name, expr->span);
            expr->integer = expr->field->index;
            if (expr->field->type) {
                check_error(checker, expr->span, "E_CALL",
                            "enum variant requires a parenthesized payload value");
            }
            if (fe_is_payload_enum(nominal)) {
                expr->kind = EX_VARIANT;
                expr->a = NULL;
            }
            break;
        }
        FeType *type = check_expression(checker, expr->a, NULL);
        if ((type->kind == TY_READ || type->kind == TY_EDIT || type->kind == TY_ARRAY ||
             type->kind == TY_BUFFER || type->kind == TY_BUFFER_EDIT || type->kind == TY_STR) &&
            !strcmp(expr->name, "len")) {
            expr->type = u;
        } else if ((type->kind == TY_BUFFER || type->kind == TY_BUFFER_EDIT) &&
                   !strcmp(expr->name, "capacity")) {
            expr->type = u;
        } else if (type->kind == TY_STRUCT || type->kind == TY_STRUCT_READ ||
                   type->kind == TY_STRUCT_EDIT) {
            FeType *record = type->kind == TY_STRUCT ? type : type->base;
            expr->field = find_field(checker, record, expr->name, expr->span);
            expr->type = expr->field->type;
        } else {
            check_error(checker, expr->span, "E_TYPE", "type has no such member");
        }
        break;
    }
    case EX_INDEX:
    case EX_SLICE: {
        FeType *type = check_expression(checker, expr->a, NULL);
        check_expression(checker, expr->b, u);
        if (type->kind != TY_READ && type->kind != TY_EDIT && type->kind != TY_ARRAY &&
            type->kind != TY_BUFFER && type->kind != TY_BUFFER_EDIT &&
            !(type->kind == TY_STR && expr->kind == EX_INDEX)) {
            check_error(checker, expr->span, "E_TYPE",
                        "index requires an array, buffer, string, or view");
        }
        FeType *element = type->kind == TY_STR           ? checker->program->primitive[TY_U8]
                          : type->kind == TY_BUFFER_EDIT ? type->base->base
                                                         : type->base;
        if (expr->kind == EX_SLICE) {
            check_expression(checker, expr->c, u);
            expr->type =
                make_type(checker, shared_storage(expr->a) ? TY_READ : TY_EDIT, element, NULL);
            if (expr->b->kind == EX_INT && expr->c->kind == EX_INT &&
                (expr->b->integer > expr->c->integer ||
                 (type->kind == TY_ARRAY && expr->c->integer > type->count))) {
                check_error(checker, expr->span, "E_CONST", "invalid constant slice");
            }
        } else {
            expr->type = element;
            if (type->kind == TY_ARRAY && expr->b->kind == EX_INT &&
                expr->b->integer >= type->count) {
                check_error(checker, expr->span, "E_CONST", "constant index outside array");
            }
        }
        FeSymbol *owner = storage_owner(expr->a);
        if (owner && (borrows_descriptor(expr->b, owner) || borrows_descriptor(expr->c, owner))) {
            check_error(checker, expr->span, "E_ALIAS",
                        "index or slice bounds cannot borrow their buffer descriptor");
        }
        if (owner && checker->state[owner->id] != BINDING_READY) {
            check_error(checker, expr->span, "E_MOVE",
                        "index or slice bounds cannot consume their storage owner");
        }
        break;
    }
    case EX_REPEAT:
    case EX_ARRAY: {
        if (!expected || expected->kind != TY_ARRAY) {
            check_error(checker, expr->span, "E_TYPE",
                        "array literal requires explicit array type");
        }
        if (expr->kind == EX_REPEAT && expr->integer != expected->count) {
            fe_error(checker->program->ctx, expr->span, "E_TYPE",
                     "array repeat count %llu does not match expected length %llu",
                     (unsigned long long)expr->integer, (unsigned long long)expected->count);
            longjmp(checker->stop, 1);
        }
        uint64_t n = 0;
        for (FeExpr *a = expr->a; a; a = a->next) {
            if (a->access || a->label) {
                check_error(checker, a->span, "E_TYPE", "invalid array element");
            }
            check_expression(checker, a, expected->base);
            require_transfer(checker, a);
            ++n;
        }
        if (expr->kind == EX_ARRAY && n != expected->count) {
            check_error(checker, expr->span, "E_TYPE", "array initializer length mismatch");
        }
        expr->type = expected;
        break;
    }
    case EX_WIDEN:
    case EX_NARROW: {
        FeType *from = check_expression(checker, expr->a, NULL), *to = expr->cast;
        if (!fe_is_integer(from) || !fe_is_integer(to)) {
            check_error(checker, expr->span, "E_TYPE", "numeric conversion requires integer types");
        }
        if (expr->kind == EX_WIDEN) {
            if (integer_max(from) > integer_max(to) || (fe_is_signed(from) && !fe_is_signed(to))) {
                check_error(checker, expr->span, "E_TYPE",
                            "widen must preserve the entire source type range; use narrow");
            }
            expr->type = to;
        } else {
            expr->type = make_type(checker, TY_RESULT, to, checker->program->cast_error);
        }
        break;
    }
    default:
        check_error(checker, expr->span, "E_INTERNAL", "unexpected expression before checking");
    }
    if (expected) {
        require_same_type(checker, expr->span, expr->type, expected);
    }
    --checker->nesting;
    return expr->type;
}

static void require_handled_results(Checker *checker, unsigned start, FeSpan span) {
    for (unsigned i = start; i < checker->count; ++i) {
        FeSymbol *symbol = checker->visible[i];
        if (symbol->type->kind == TY_RESULT && may_hold_value(checker->state[symbol->id])) {
            check_error(
                checker, span, "E_RESULT",
                "Result must be matched, propagated, or explicitly forwarded before leaving scope");
        }
    }
}

static void join_states(Checker *checker, const BindingState *a, const BindingState *b) {
    for (unsigned i = 0; i < FE_LOCAL_LIMIT; ++i) {
        if (a[i] == b[i]) {
            checker->state[i] = a[i];
        } else if (may_hold_value(a[i]) || may_hold_value(b[i])) {
            /* A Result still needs handling on any path that retains its value. */
            checker->state[i] = BINDING_MAYBE_READY;
        } else {
            checker->state[i] = BINDING_UNINITIALIZED;
        }
    }
}

static bool check_statements(Checker *, FeStmt *);
static bool check_statement(Checker *checker, FeStmt *stmt) {
    switch (stmt->kind) {
    case ST_BLOCK: {
        unsigned start = checker->count;
        ++checker->depth;
        bool returns = check_statements(checker, stmt->body);
        if (!returns) {
            require_handled_results(checker, start, stmt->span);
        }
        checker->count = start;
        --checker->depth;
        return returns;
    }
    case ST_DECL:
        if (stmt->type->kind == TY_VOID || stmt->type->kind == TY_READ ||
            stmt->type->kind == TY_EDIT || fe_is_owner_borrow(stmt->type)) {
            check_error(checker, stmt->span, "E_TYPE", "local bindings must own a value");
        }
        if (stmt->a) {
            check_expression(checker, stmt->a, stmt->type);
            require_transfer(checker, stmt->a);
        }
        stmt->symbol = declare_binding(checker, stmt->name, stmt->type, stmt->mutable, stmt->span);
        checker->state[stmt->symbol->id] = stmt->a ? BINDING_READY : BINDING_UNINITIALIZED;
        break;
    case ST_ASSIGN: {
        FeType *type;
        if (stmt->a->kind == EX_NAME) {
            stmt->a->symbol = lookup_binding(checker, stmt->a->name, stmt->span);
            stmt->a->type = stmt->a->symbol->type;
            type = stmt->a->type;
            if (stmt->op != '=' && checker->state[stmt->a->symbol->id] != BINDING_READY) {
                check_error(checker, stmt->span, "E_INIT",
                            "compound assignment reads uninitialized binding");
            }
        } else {
            type = check_expression(checker, stmt->a, NULL);
        }
        if (stmt->a->kind == EX_INDEX && stmt->a->a->type->kind == TY_STR) {
            check_error(checker, stmt->span, "E_MUT", "string bytes are immutable");
        }
        FeSymbol *owner = storage_owner(stmt->a);
        if (!owner || !writable_storage(owner) ||
            (stmt->a->kind != EX_NAME && stmt->a->kind != EX_FIELD && stmt->a->kind != EX_INDEX)) {
            check_error(checker, stmt->span, "E_MUT",
                        "assignment requires mutable owning storage or edit view");
        }
        if (type->kind == TY_READ || type->kind == TY_EDIT || fe_is_owner_borrow(type) ||
            (stmt->a->kind == EX_FIELD && !stmt->a->field)) {
            check_error(checker, stmt->span, "E_MUT", "view descriptors and lengths are immutable");
        }
        if (stmt->op != '=' && !fe_is_integer(type)) {
            check_error(checker, stmt->span, "E_TYPE", "compound assignment requires integer");
        }
        if (type->kind == TY_RESULT && may_hold_value(checker->state[owner->id])) {
            check_error(checker, stmt->span, "E_RESULT", "cannot overwrite an unhandled Result");
        }
        check_expression(checker, stmt->b, type);
        if ((stmt->op == FE_T_DIV_ASSIGN || stmt->op == FE_T_REM_ASSIGN) &&
            stmt->b->kind == EX_INT && !stmt->b->integer) {
            check_error(checker, stmt->b->span, "E_CONST", "division by zero");
        }
        require_transfer(checker, stmt->b);
        if (stmt->a->kind != EX_NAME && borrows_descriptor(stmt->b, owner)) {
            check_error(checker, stmt->span, "E_ALIAS",
                        "assignment value cannot borrow its target buffer descriptor");
        }
        if (stmt->a->kind != EX_NAME && checker->state[owner->id] != BINDING_READY) {
            check_error(checker, stmt->span, "E_MOVE",
                        "assignment target owner was moved while evaluating its value");
        }
        if (stmt->a->kind == EX_NAME) {
            checker->state[owner->id] = BINDING_READY;
        }
        break;
    }
    case ST_EXPR:
        if (check_expression(checker, stmt->a, NULL)->kind != TY_VOID) {
            check_error(checker, stmt->span, "E_RESULT",
                        "non-void expression result cannot be discarded");
        }
        break;
    case ST_BREAK:
    case ST_CONTINUE: {
        LoopState *loop = checker->loop;
        if (!loop) {
            check_error(checker, stmt->span, "E_FLOW",
                        "loop control statement requires an enclosing loop");
        }
        bool breaking = stmt->kind == ST_BREAK;
        require_handled_results(checker, breaking ? loop->start : loop->body_start, stmt->span);
        stmt->loop = loop->statement;
        FeDrop **tail = &stmt->drops;
        for (unsigned i = checker->count; i > loop->body_start; --i) {
            FeSymbol *symbol = checker->visible[i - 1];
            if (fe_has_resource(symbol->type)) {
                FeDrop *drop = fe_alloc(checker->program->ctx, sizeof(*drop));
                if (!drop) {
                    longjmp(checker->stop, 1);
                }
                drop->symbol = symbol;
                *tail = drop;
                tail = &drop->next;
            }
        }
        BindingState *states = breaking ? loop->breaks : loop->continues;
        bool *have = breaking ? &loop->have_break : &loop->have_continue;
        if (*have) {
            join_states(checker, states, checker->state);
        }
        memcpy(states, checker->state, sizeof(loop->breaks));
        *have = true;
        if (breaking) {
            loop->statement->has_break = true;
        } else {
            loop->statement->has_continue = true;
        }
        return true;
    }
    case ST_RETURN:
        if (checker->function->result->kind == TY_VOID) {
            if (stmt->a) {
                check_error(checker, stmt->span, "E_TYPE", "void function cannot return a value");
            }
        } else {
            if (!stmt->a) {
                check_error(checker, stmt->span, "E_TYPE", "missing return value");
            }
            check_expression(checker, stmt->a, checker->function->result);
            require_transfer(checker, stmt->a);
        }
        require_handled_results(checker, 0, stmt->span);
        return true;
    case ST_IF: {
        check_expression(checker, stmt->a, checker->program->primitive[TY_BOOL]);
        BindingState before[FE_LOCAL_LIMIT], yes[FE_LOCAL_LIMIT], no[FE_LOCAL_LIMIT];
        memcpy(before, checker->state, sizeof(before));
        bool ry = check_statement(checker, stmt->body);
        memcpy(yes, checker->state, sizeof(yes));
        memcpy(checker->state, before, sizeof(before));
        bool rn = stmt->other ? check_statement(checker, stmt->other) : false;
        memcpy(no, checker->state, sizeof(no));
        if (ry && !rn) {
            memcpy(checker->state, no, sizeof(no));
        } else if (rn && !ry) {
            memcpy(checker->state, yes, sizeof(yes));
        } else {
            join_states(checker, yes, no);
        }
        return ry && rn;
    }
    case ST_FOR:
    case ST_WHILE: {
        unsigned start = checker->count;
        ++checker->depth;
        if (stmt->init) {
            check_statement(checker, stmt->init);
        }
        BindingState before[FE_LOCAL_LIMIT];
        memcpy(before, checker->state, sizeof(before));
        check_expression(checker, stmt->a, checker->program->primitive[TY_BOOL]);
        for (unsigned id = 0; id < FE_LOCAL_LIMIT; ++id) {
            if (before[id] != checker->state[id]) {
                check_error(checker, stmt->span, "E_MOVE",
                            "loop conditions cannot consume owning bindings");
            }
        }
        LoopState loop = {.statement = stmt,
                          .start = start,
                          .body_start = checker->count,
                          .previous = checker->loop};
        checker->loop = &loop;
        bool terminal = check_statement(checker, stmt->body);
        checker->loop = loop.previous;
        bool back_edge = !terminal || loop.have_continue;
        if (loop.have_continue) {
            if (terminal) {
                memcpy(checker->state, loop.continues, sizeof(loop.continues));
            } else {
                join_states(checker, checker->state, loop.continues);
            }
        } else if (terminal) {
            /* The header still needs type checking when no path reaches its step. */
            memcpy(checker->state, before, sizeof(before));
        }
        if (stmt->step) {
            check_statement(checker, stmt->step);
        }
        for (unsigned i = 0; i < checker->count; ++i) {
            FeSymbol *symbol = checker->visible[i];
            unsigned id = symbol->id;
            if (back_edge && before[id] == BINDING_READY && checker->state[id] != BINDING_READY) {
                check_error(
                    checker, stmt->span, "E_MOVE",
                    "loop iteration must restore moved outer bindings before its back edge");
            }
            if (back_edge && symbol->type->kind == TY_RESULT && !may_hold_value(before[id]) &&
                may_hold_value(checker->state[id])) {
                check_error(
                    checker, stmt->span, "E_RESULT",
                    "loop iteration must handle a newly assigned Result before its back edge");
            }
        }
        if (loop.have_break) {
            join_states(checker, before, loop.breaks);
        } else {
            memcpy(checker->state, before, sizeof(before));
        }
        require_handled_results(checker, start, stmt->span);
        checker->count = start;
        --checker->depth;
        break;
    }
    case ST_MATCH: {
        FeType *type = check_expression(checker, stmt->a, NULL);
        bool result = type->kind == TY_RESULT;
        bool scalar = fe_is_integer(type) || type->kind == TY_BOOL;
        if (!result && type->kind != TY_ENUM && !scalar) {
            check_error(checker, stmt->span, "E_TYPE",
                        "match requires an integer, bool, Result or enum value");
        }
        bool consuming = result || fe_is_payload_enum(type);
        if (consuming && stmt->a->kind == EX_NAME) {
            checker->state[stmt->a->symbol->id] = BINDING_MOVED;
            checker->moved_at[stmt->a->symbol->id] = stmt->span;
        } else if (consuming) {
            require_transfer(checker, stmt->a);
        }
        BindingState before[FE_LOCAL_LIMIT], joined[FE_LOCAL_LIMIT];
        memcpy(before, checker->state, sizeof(before));
        bool seen_ok = false, seen_err = false, all = true, have = false;
        bool seen_variant[128] = {false};
        bool wildcard = false;
        unsigned boolean_cases = 0, literal_count = 0;
        uint64_t literals[FE_MATCH_CASE_LIMIT];
        for (FeCase *match_case = stmt->cases; match_case; match_case = match_case->next) {
            if (scalar) {
                if (wildcard) {
                    check_error(checker, match_case->span, "E_MATCH",
                                "case after a wildcard is unreachable");
                }
                if (match_case->wildcard) {
                    if (type->kind == TY_BOOL && boolean_cases == 3) {
                        check_error(checker, match_case->span, "E_MATCH",
                                    "wildcard after both boolean cases is unreachable");
                    }
                    wildcard = true;
                } else {
                    if (!match_case->literal) {
                        check_error(checker, match_case->span, "E_MATCH",
                                    "scalar match requires literal or wildcard cases");
                    }
                    check_expression(checker, match_case->literal, type);
                    FeExpr *literal = match_case->literal;
                    uint64_t value = literal->kind == EX_UNARY ? UINT64_C(0) - literal->a->integer
                                                               : literal->integer;
                    for (unsigned i = 0; i < literal_count; ++i) {
                        if (literals[i] == value) {
                            check_error(checker, match_case->span, "E_MATCH",
                                        "duplicate scalar case value");
                        }
                    }
                    if (literal_count == FE_MATCH_CASE_LIMIT) {
                        check_error(checker, match_case->span, "E_LIMIT", "too many scalar cases");
                    }
                    literals[literal_count++] = value;
                    if (type->kind == TY_BOOL) {
                        boolean_cases |= 1u << (unsigned)value;
                    }
                }
                match_case->type = checker->program->primitive[TY_VOID];
            } else if (match_case->literal || match_case->wildcard) {
                check_error(checker, match_case->span, "E_MATCH",
                            "enum and Result matches require explicit variant cases");
            } else if (!result) {
                if (fe_named_type(checker->program, match_case->owner) != type) {
                    check_error(checker, match_case->span, "E_MATCH",
                                "case type must match the enum being examined");
                }
                FeField *field = type->fields;
                while (field && strcmp(field->name, match_case->variant)) {
                    field = field->next;
                }
                if (!field || seen_variant[field->index]) {
                    fe_error(checker->program->ctx, match_case->span, "E_MATCH",
                             field ? "duplicate enum case '%s.%s'" : "unknown enum case '%s.%s'",
                             match_case->owner, match_case->variant);
                    longjmp(checker->stop, 1);
                }
                seen_variant[field->index] = true;
                match_case->field = field;
                if (field->type) {
                    if (!match_case->name || !match_case->type) {
                        check_error(checker, match_case->span, "E_MATCH",
                                    "enum payload case requires a typed let binding");
                    }
                    require_same_type(checker, match_case->span, match_case->type, field->type);
                } else {
                    if (match_case->payload_pattern) {
                        check_error(checker, match_case->span, "E_MATCH",
                                    "fieldless enum case does not take parentheses");
                    }
                    match_case->type = checker->program->primitive[TY_VOID];
                }
            } else {
                if (strcmp(match_case->owner, "Result")) {
                    check_error(checker, match_case->span, "E_MATCH",
                                "Result match requires Result cases");
                }
                bool ok = !strcmp(match_case->variant, "ok");
                if ((!ok && strcmp(match_case->variant, "err")) || (ok ? seen_ok : seen_err)) {
                    check_error(checker, match_case->span, "E_MATCH",
                                "duplicate or unknown Result case");
                }
                if (ok) {
                    seen_ok = true;
                } else {
                    seen_err = true;
                }
                FeType *payload = ok ? type->base : type->error;
                if (payload->kind == TY_VOID) {
                    if (match_case->name) {
                        check_error(checker, match_case->span, "E_MATCH",
                                    "unit success case must not bind a payload");
                    }
                    match_case->type = payload;
                } else {
                    if (!match_case->type) {
                        check_error(checker, match_case->span, "E_MATCH",
                                    "Result case requires a typed payload");
                    }
                    require_same_type(checker, match_case->span, match_case->type, payload);
                }
            }
            memcpy(checker->state, before, sizeof(before));
            unsigned start = checker->count;
            ++checker->depth;
            if (match_case->name) {
                match_case->symbol = declare_binding(checker, match_case->name, match_case->type,
                                                     false, match_case->span);
                checker->state[match_case->symbol->id] = BINDING_READY;
            }
            bool returns = check_statement(checker, match_case->body);
            if (!returns) {
                require_handled_results(checker, start, match_case->span);
            }
            checker->count = start;
            --checker->depth;
            if (!returns) {
                if (have) {
                    join_states(checker, joined, checker->state);
                }
                memcpy(joined, checker->state, sizeof(joined));
                have = true;
            }
            all = all && returns;
        }
        if (result && (!seen_ok || !seen_err)) {
            check_error(checker, stmt->span, "E_MATCH", "Result match must handle ok and err");
        }
        if (scalar && !wildcard && (type->kind != TY_BOOL || boolean_cases != 3)) {
            check_error(checker, stmt->span, "E_MATCH",
                        type->kind == TY_BOOL ? "boolean match must handle true and false"
                                              : "integer match requires a final wildcard case");
        }
        if (!result && !scalar) {
            for (FeField *field = type->fields; field; field = field->next) {
                if (!seen_variant[field->index]) {
                    FeContext *context = checker->program->ctx;
                    fe_error(context, stmt->span, "E_MATCH",
                             "enum match does not handle variant '%s.%s'", type->name, field->name);
                    snprintf(context->diagnostic.help, sizeof(context->diagnostic.help),
                             "add a case for every variant of the matched enum");
                    longjmp(checker->stop, 1);
                }
            }
        }
        if (have) {
            memcpy(checker->state, joined, sizeof(joined));
        } else {
            memcpy(checker->state, before, sizeof(before));
        }
        return all;
    }
    }
    return false;
}

static bool check_statements(Checker *checker, FeStmt *stmt) {
    bool returns = false;
    for (; stmt; stmt = stmt->next) {
        if (returns) {
            check_error(checker, stmt->span, "E_FLOW", "unreachable statement");
        }
        returns = check_statement(checker, stmt);
    }
    return returns;
}

static void check_file_native(Checker *checker, FeFunction *function) {
    FeType *result = function->result;
    FeType *error = result->error;
    bool opening = !strcmp(function->name, "open_read") || !strcmp(function->name, "create_new");
    bool creating = !strcmp(function->name, "create_new");
    bool reading = !strcmp(function->name, "read_into");
    bool writing = !strcmp(function->name, "write");
    bool syncing = !strcmp(function->name, "sync");
    bool closing = !strcmp(function->name, "close");
    if ((!opening && !reading && !writing && !syncing && !closing) || !function->public ||
        function->exported || function->budgeted ||
        function->param_count != (reading || writing ? 2u : 1u) || result->kind != TY_RESULT ||
        !error || error->kind != TY_ENUM || error->module != function->module || !error->public ||
        strcmp(error->name, "Error")) {
        check_error(checker, function->span, "E_NATIVE", "unsupported native file signature");
    }
    FeType *first = function->params->type;
    bool valid;
    if (opening) {
        valid = first->kind == TY_READ && first->base->kind == TY_U8 &&
                result->base->kind == TY_FILE && result->base->module == function->module;
    } else if (reading || writing) {
        FeType *bytes = function->params->next->type;
        valid = first->kind == TY_FILE_EDIT && first->base->module == function->module &&
                bytes->kind == (reading ? TY_EDIT : TY_READ) && bytes->base->kind == TY_U8 &&
                result->base->kind == TY_USIZE;
    } else if (syncing) {
        valid = first->kind == TY_FILE_EDIT && first->base->module == function->module &&
                result->base->kind == TY_VOID;
    } else {
        valid = first->kind == TY_FILE && first->module == function->module &&
                result->base->kind == TY_VOID;
    }
    if (!valid) {
        check_error(checker, function->span, "E_NATIVE", "unsupported native file signature");
    }
    static const char *variants[] = {"invalid_argument", "invalid_state", "invalid_path",
                                     "path_too_long",    "open_failed",   "inspect_failed",
                                     "not_regular",      "read_failed",   "close_failed",
                                     "write_failed",     "sync_failed"};
    FeField *field = error->fields;
    for (unsigned index = 0; index < sizeof(variants) / sizeof(variants[0]); ++index) {
        if (!field || field->type || strcmp(field->name, variants[index])) {
            check_error(checker, function->span, "E_NATIVE", "unsupported file error variants");
        }
        field = field->next;
    }
    if (field) {
        check_error(checker, function->span, "E_NATIVE", "unsupported file error variants");
    }
    function->effects =
        opening   ? FE_EFFECT_FS_OPEN | FE_EFFECT_FS_CLOSE | (creating ? FE_EFFECT_FS_WRITE : 0u)
        : reading ? FE_EFFECT_FS_READ
        : writing ? FE_EFFECT_FS_WRITE
        : syncing ? FE_EFFECT_FS_SYNC
                  : FE_EFFECT_FS_CLOSE;
}

static void check_clock_native(Checker *checker, FeFunction *function) {
    FeType *result = function->result;
    FeType *error = result->error;
    if (strcmp(function->name, "monotonic_ns") || !function->public || function->exported ||
        function->budgeted || function->param_count || result->kind != TY_RESULT ||
        result->base->kind != TY_USIZE || !error || error->kind != TY_ENUM ||
        error->module != function->module || !error->public || strcmp(error->name, "Error")) {
        check_error(checker, function->span, "E_NATIVE", "unsupported native clock signature");
    }
    static const char *variants[] = {"clock_failed", "invalid_reading", "overflow"};
    FeField *field = error->fields;
    for (unsigned index = 0; index < 3; ++index) {
        if (!field || strcmp(field->name, variants[index])) {
            check_error(checker, function->span, "E_NATIVE", "unsupported clock error variants");
        }
        field = field->next;
    }
    if (field) {
        check_error(checker, function->span, "E_NATIVE", "unsupported clock error variants");
    }
    function->effects = FE_EFFECT_CLOCK_READ;
}

static void check_arguments_native(Checker *checker, FeFunction *function) {
    FeType *result = function->result;
    FeType *error = result->error;
    bool count = !strcmp(function->name, "count");
    if ((!count && strcmp(function->name, "read_into")) || !function->public ||
        function->exported || function->budgeted || function->param_count != (count ? 0u : 2u) ||
        result->kind != TY_RESULT || result->base->kind != TY_USIZE || !error ||
        error->kind != TY_ENUM || error->module != function->module || !error->public ||
        strcmp(error->name, "Error")) {
        check_error(checker, function->span, "E_NATIVE", "unsupported native argument signature");
    }
    if (!count && (function->params->type->kind != TY_USIZE ||
                   function->params->next->type->kind != TY_EDIT ||
                   function->params->next->type->base->kind != TY_U8)) {
        check_error(checker, function->span, "E_NATIVE", "unsupported native argument signature");
    }
    static const char *variants[] = {"unavailable", "out_of_range", "output_too_small",
                                     "invalid_argument"};
    FeField *field = error->fields;
    for (unsigned index = 0; index < 4; ++index) {
        if (!field || strcmp(field->name, variants[index])) {
            check_error(checker, function->span, "E_NATIVE", "unsupported argument error variants");
        }
        field = field->next;
    }
    if (field) {
        check_error(checker, function->span, "E_NATIVE", "unsupported argument error variants");
    }
    function->effects = FE_EFFECT_ARGS_READ;
}

static void check_environment_native(Checker *checker, FeFunction *function) {
    FeType *result = function->result;
    FeType *error = result->error;
    if (strcmp(function->name, "read_into") || !function->public || function->exported ||
        function->budgeted || function->param_count != 2 ||
        function->params->type->kind != TY_READ || function->params->type->base->kind != TY_U8 ||
        function->params->next->type->kind != TY_EDIT ||
        function->params->next->type->base->kind != TY_U8 || result->kind != TY_RESULT ||
        result->base->kind != TY_USIZE || !error || error->kind != TY_ENUM ||
        error->module != function->module || !error->public || strcmp(error->name, "Error")) {
        check_error(checker, function->span, "E_NATIVE",
                    "unsupported native environment signature");
    }
    static const char *variants[] = {"not_found", "invalid_name", "output_too_small"};
    FeField *field = error->fields;
    for (unsigned index = 0; index < 3; ++index) {
        if (!field || strcmp(field->name, variants[index])) {
            check_error(checker, function->span, "E_NATIVE",
                        "unsupported environment error variants");
        }
        field = field->next;
    }
    if (field) {
        check_error(checker, function->span, "E_NATIVE", "unsupported environment error variants");
    }
    function->effects = FE_EFFECT_ENV_READ;
}

/* Public wrappers share the C identifier namespace with their result typedefs
 * and with the embedded runtime. Reject collisions before invoking the backend. */
static void check_export_name(Checker *checker, FeFunction *function) {
    static const char *const runtime_names[] = {
        "arguments_count", "arguments_init",  "arguments_read", "buffer_create",
        "buffer_drop",     "buffer_move",     "buffer_reserve", "buffer_resize",
        "file_close",      "file_drop",       "file_move",      "file_open_read",
        "file_read",       "file_create_new", "file_write",     "file_sync"};
    for (size_t i = 0; i < sizeof(runtime_names) / sizeof(runtime_names[0]); ++i) {
        if (!strcmp(function->name, runtime_names[i])) {
            check_error(checker, function->span, "E_ABI",
                        "export name conflicts with an internal runtime symbol");
        }
    }
    size_t length = strlen(function->name);
    for (FeFunction *other = checker->program->functions; other; other = other->next) {
        if (other->exported && other != function && !strncmp(other->name, function->name, length) &&
            !strcmp(other->name + length, "_result")) {
            check_error(checker, function->span, "E_ABI",
                        "export name conflicts with another export's result type");
        }
    }
}

bool fe_check(FeProgram *program) {
    Checker checker = {.program = program};
    if (setjmp(checker.stop)) {
        return false;
    }
    FeContext *root = program->ctx;
    FeModule *root_module = program->module;
    for (FeFunction *f = program->functions; f; f = f->next) {
        program->module = f->module;
        program->ctx = f->module ? f->module->ctx : root;
        if (f->exported) {
            check_export_name(&checker, f);
        }
        if (f->native && f->module && f->module->standard && !strcmp(f->module->name, "std.fs")) {
            check_file_native(&checker, f);
            continue;
        }
        if (f->native && f->module && f->module->standard && !strcmp(f->module->name, "std.time")) {
            check_clock_native(&checker, f);
            continue;
        }
        if (f->native && f->module && f->module->standard && !strcmp(f->module->name, "std.args")) {
            check_arguments_native(&checker, f);
            continue;
        }
        if (f->native && f->module && f->module->standard && !strcmp(f->module->name, "std.env")) {
            check_environment_native(&checker, f);
            continue;
        }
        if (f->native && f->module && f->module->standard &&
            (!strcmp(f->module->name, "std.mem") || !strcmp(f->module->name, "std.buffer"))) {
            FeType *result = f->result;
            bool typed = !strcmp(f->module->name, "std.buffer");
            if (typed && !strcmp(f->name, "element_size")) {
                if (!f->type_argument || !fe_is_data(f->type_argument) || !f->public ||
                    f->exported || f->budgeted || f->param_count || result->kind != TY_USIZE) {
                    check_error(&checker, f->span, "E_NATIVE",
                                "unsupported native element-size signature");
                }
                f->effects = 0;
                continue;
            }
            bool empty = typed && !strcmp(f->name, "empty");
            bool truncating = typed && !strcmp(f->name, "truncate");
            bool filled = typed && !strcmp(f->name, "resize_filled");
            bool creating = empty || !strcmp(f->name, typed ? "allocate" : "bytes");
            bool reserve = !strcmp(f->name, "reserve");
            bool resize = !strcmp(f->name, "resize");
            unsigned parameters = empty ? 1 : filled ? 3 : 2;
            if ((typed && (!f->type_argument || !fe_is_data(f->type_argument) ||
                           ((!strcmp(f->name, "allocate") || resize) &&
                            !fe_is_integer(f->type_argument)))) ||
                (!creating && !reserve && !resize && !filled && !truncating) || !f->public ||
                f->exported || f->budgeted || f->param_count != parameters ||
                result->kind != TY_RESULT) {
                check_error(&checker, f->span, "E_NATIVE", "unsupported native memory signature");
            }
            FeType *error = result->error;
            if (!error || error->kind != TY_ENUM || fe_is_payload_enum(error) ||
                error->module != f->module || !error->public || strcmp(error->name, "Error")) {
                check_error(&checker, f->span, "E_NATIVE", "unsupported native memory error type");
            }
            if (creating) {
                if (f->params->type->kind != TY_USIZE ||
                    (!empty && f->params->next->type->kind != TY_USIZE) ||
                    result->base->kind != TY_BUFFER ||
                    (typed ? result->base->base != f->type_argument
                           : result->base->base->kind != TY_U8)) {
                    check_error(&checker, f->span, "E_NATIVE",
                                "unsupported native memory signature");
                }
            } else if (f->params->type->kind != TY_BUFFER_EDIT ||
                       f->params->next->type->kind != TY_USIZE || result->base->kind != TY_USIZE ||
                       (typed ? f->params->type->base->base != f->type_argument
                              : f->params->type->base->base->kind != TY_U8) ||
                       (filled && f->params->next->next->type != f->type_argument)) {
                check_error(&checker, f->span, "E_NATIVE", "unsupported native memory signature");
            }
            const char *variants[] = {"no_memory", "size_overflow", "limit", "invalid_argument",
                                      "invalid_state"};
            FeField *field = error->fields;
            for (unsigned i = 0; i < 5; ++i) {
                if (!field || strcmp(field->name, variants[i])) {
                    check_error(&checker, f->span, "E_NATIVE", "unsupported memory error variants");
                }
                field = field->next;
            }
            if (field) {
                check_error(&checker, f->span, "E_NATIVE", "unsupported memory error variants");
            }
            f->effects = truncating ? 0 : FE_EFFECT_MEMORY;
            continue;
        }
        if (f->native) {
            FeType *result = f->result;
            FeType *error = result->error;
            bool reading = !strcmp(f->name, "read_into");
            bool printing = !strcmp(f->name, "print") || !strcmp(f->name, "println");
            if (!f->module || !f->module->standard || strcmp(f->module->name, "std.io") ||
                (!reading && !printing && strcmp(f->name, "write") &&
                 strcmp(f->name, "write_stderr")) ||
                !f->public || f->exported || f->budgeted || f->param_count != 1 ||
                f->params->type->kind != (reading ? TY_EDIT : TY_READ) ||
                f->params->type->base->kind != TY_U8 ||
                (printing ? result->kind != TY_VOID : result->kind != TY_RESULT)) {
                check_error(&checker, f->span, "E_NATIVE", "unsupported native library signature");
            }
            if (!printing &&
                (result->base->kind != TY_USIZE || !error || error->kind != TY_ENUM ||
                 error->module != f->module || !error->public || strcmp(error->name, "Error") ||
                 !error->fields || strcmp(error->fields->name, "write_failed") ||
                 !error->fields->next || strcmp(error->fields->next->name, "read_failed") ||
                 error->fields->next->next)) {
                check_error(&checker, f->span, "E_NATIVE", "unsupported native library signature");
            }
            f->effects = reading ? FE_EFFECT_IO_READ : FE_EFFECT_IO_WRITE;
            continue;
        }
        checker.function = f;
        checker.count = 0;
        checker.depth = 0;
        checker.nesting = 0;
        memset(checker.state, 0, sizeof(checker.state));
        memset(checker.moved_at, 0, sizeof(checker.moved_at));
        for (FeSymbol *symbol = f->params; symbol; symbol = symbol->next) {
            FeSymbol *local = declare_binding(&checker, symbol->name, symbol->type, false, f->span);
            local->parameter = true;
            symbol->id = local->id;
            checker.state[local->id] = BINDING_READY;
        }
        bool returned = check_statement(&checker, f->body);
        if (!returned && f->result->kind != TY_VOID) {
            check_error(&checker, f->span, "E_RETURN", "not every path returns a value");
        }
        if (!returned) {
            require_handled_results(&checker, 0, f->span);
        }
        if (f->exported) {
            for (FeSymbol *symbol = f->params; symbol; symbol = symbol->next) {
                if ((symbol->type->kind == TY_READ || symbol->type->kind == TY_EDIT) &&
                    !fe_is_integer(symbol->type->base) && symbol->type->base->kind != TY_BOOL) {
                    check_error(&checker, f->span, "E_ABI",
                                "exported views require scalar elements");
                }
                if (symbol->type->kind == TY_STR || fe_is_aggregate(symbol->type) ||
                    fe_is_owner_borrow(symbol->type)) {
                    check_error(&checker, f->span, "E_ABI",
                                "pilot exports accept scalars and views only");
                }
            }
            if (f->result->kind == TY_STR ||
                (fe_is_aggregate(f->result) &&
                 (f->result->kind != TY_RESULT || fe_is_aggregate(f->result->base) ||
                  f->result->base->kind == TY_STR))) {
                check_error(&checker, f->span, "E_ABI",
                            "pilot export result must be scalar or Result<scalar, enum>");
            }
        }
    }
    program->ctx = root;
    program->module = root_module;
    return !program->ctx->diagnostic.code;
}
