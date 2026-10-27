#include "fermi/fe.h"
#include <string.h>

/* UINT64_MAX denotes unknown, never a successful finite certificate. */
#define UNKNOWN UINT64_MAX
typedef struct {
    uint64_t value[FE_LOCAL_LIMIT], length[FE_LOCAL_LIMIT];
} Bounds;
static uint64_t bound_add(uint64_t left, uint64_t right) {
    if (left == UNKNOWN || right == UNKNOWN || left >= UNKNOWN - right) {
        return UNKNOWN;
    }
    return left + right;
}

static uint64_t bound_multiply(uint64_t left, uint64_t right) {
    if (!left || !right) {
        return 0;
    }
    if (left == UNKNOWN || right == UNKNOWN || left >= UNKNOWN / right) {
        return UNKNOWN;
    }
    return left * right;
}

static uint64_t type_upper_bound(FeType *t) {
    switch (t->kind) {
    case TY_U8:
        return UINT8_MAX;
    case TY_U16:
        return UINT16_MAX;
    case TY_U32:
        return UINT32_MAX;
    case TY_U64:
        return UINT64_MAX;
    case TY_USIZE:
        return SIZE_MAX;
    case TY_BOOL:
        return 1;
    default:
        return UNKNOWN;
    }
}

static uint64_t value_upper_bound(FeExpr *expr, Bounds *bounds) {
    if (!expr) {
        return UNKNOWN;
    }
    switch (expr->kind) {
    case EX_INT:
        return expr->integer;
    case EX_NAME:
        return bounds->value[expr->symbol->id];
    case EX_FIELD:
        /* Enum variant qualifiers are type names, not typed value expressions. */
        if (!expr->a->type) {
            break;
        }
        if (expr->a->type->kind == TY_BUFFER_EDIT) {
            return UNKNOWN;
        }
        if (!expr->field && expr->a->type->kind == TY_ARRAY) {
            return expr->a->type->count;
        }
        if (!expr->field && expr->a->kind == EX_NAME) {
            return bounds->length[expr->a->symbol->id];
        }
        break;
    case EX_WIDEN:
        return value_upper_bound(expr->a, bounds);
    case EX_BINARY: {
        uint64_t a = value_upper_bound(expr->a, bounds), v = value_upper_bound(expr->b, bounds);
        if (expr->op == '+') {
            return bound_add(a, v);
        }
        if (expr->op == '*') {
            return bound_multiply(a, v);
        }
        if (expr->op == '-' && !fe_is_signed(expr->type)) {
            return a;
        }
        if (expr->op == '/' && expr->b->kind == EX_INT && v) {
            return a == UNKNOWN ? UNKNOWN : a / v;
        }
        break;
    }
    default:
        break;
    }
    return type_upper_bound(expr->type);
}

static void refine_condition(FeExpr *expr, Bounds *bounds, bool truth) {
    if (expr->kind != EX_BINARY) {
        return;
    }
    if (expr->op == FE_T_AND && truth) {
        refine_condition(expr->a, bounds, true);
        refine_condition(expr->b, bounds, true);
        return;
    }
    if (expr->op == FE_T_OR && !truth) {
        refine_condition(expr->a, bounds, false);
        refine_condition(expr->b, bounds, false);
        return;
    }
    bool inclusive = (truth && expr->op == FE_T_LE) || (!truth && expr->op == '>');
    bool exclusive = (truth && expr->op == '<') || (!truth && expr->op == FE_T_GE);
    if (!inclusive && !exclusive) {
        return;
    }
    uint64_t n = value_upper_bound(expr->b, bounds);
    if (n == UNKNOWN || (exclusive && n == 0)) {
        return;
    }
    if (exclusive) {
        --n;
    }
    uint64_t *dest = NULL;
    if (expr->a->kind == EX_NAME && !fe_is_signed(expr->a->type)) {
        dest = &bounds->value[expr->a->symbol->id];
    }
    if (expr->a->kind == EX_FIELD && !expr->a->field && expr->a->a->kind == EX_NAME &&
        expr->a->a->type->kind != TY_BUFFER_EDIT) {
        dest = &bounds->length[expr->a->a->symbol->id];
    }
    if (dest && n < *dest) {
        *dest = n;
    }
}

static bool always_returns(FeStmt *stmt) {
    for (; stmt; stmt = stmt->next) {
        if (stmt->kind == ST_RETURN) {
            return true;
        }
        if (stmt->kind == ST_BLOCK && always_returns(stmt->body)) {
            return true;
        }
        if (stmt->kind == ST_IF && stmt->other && always_returns(stmt->body) &&
            always_returns(stmt->other)) {
            return true;
        }
        if (stmt->kind == ST_MATCH) {
            bool all = true;
            for (FeCase *k = stmt->cases; k; k = k->next) {
                all = all && always_returns(k->body);
            }
            if (all) {
                return true;
            }
        }
    }
    return false;
}

static bool writes_symbol(FeStmt *stmt, FeSymbol *v) {
    for (; stmt; stmt = stmt->next) {
        if (stmt->kind == ST_ASSIGN && stmt->a->kind == EX_NAME && stmt->a->symbol == v) {
            return true;
        }
        if (writes_symbol(stmt->body, v) || writes_symbol(stmt->other, v) ||
            writes_symbol(stmt->init, v) || writes_symbol(stmt->step, v)) {
            return true;
        }
        for (FeCase *k = stmt->cases; k; k = k->next) {
            if (writes_symbol(k->body, v)) {
                return true;
            }
        }
    }
    return false;
}

static bool loop_limit_changes(FeExpr *expr, FeStmt *body, FeStmt *step) {
    if (!expr) {
        return false;
    }
    if (expr->kind == EX_NAME &&
        (writes_symbol(body, expr->symbol) || writes_symbol(step, expr->symbol))) {
        return true;
    }
    return loop_limit_changes(expr->a, body, step) || loop_limit_changes(expr->b, body, step) ||
           loop_limit_changes(expr->c, body, step);
}

static void invalidate_writes(FeStmt *stmt, Bounds *bounds) {
    for (; stmt; stmt = stmt->next) {
        if (stmt->kind == ST_ASSIGN && stmt->a->kind == EX_NAME) {
            bounds->value[stmt->a->symbol->id] = UNKNOWN;
        }
        invalidate_writes(stmt->body, bounds);
        invalidate_writes(stmt->other, bounds);
        invalidate_writes(stmt->init, bounds);
        invalidate_writes(stmt->step, bounds);
        for (FeCase *k = stmt->cases; k; k = k->next) {
            invalidate_writes(k->body, bounds);
        }
    }
}

static uint64_t function_cost(FeFunction *);
static uint64_t expr_cost(FeExpr *expr) {
    if (!expr) {
        return 0;
    }
    uint64_t cost = 1;
    if (expr->kind == EX_CALL) {
        for (FeExpr *a = expr->b; a; a = a->next) {
            cost = bound_add(cost, expr_cost(a));
        }
        uint64_t callee = expr->function->visit == 2 ? expr->function->work : UNKNOWN;
        return bound_add(cost, callee);
    }
    if (expr->kind == EX_TRY) {
        /* Include both transfers rather than assuming that the error edge is cold. */
        cost = bound_add(cost, expr_cost(expr->a));
        cost = bound_add(cost, fe_type_cells(expr->type));
        return bound_add(cost, fe_type_cells(expr->function->result));
    }
    if (expr->kind == EX_ARRAY || expr->kind == EX_STRUCT) {
        for (FeExpr *a = expr->a; a; a = a->next) {
            cost = bound_add(cost, expr_cost(a));
        }
    } else if (expr->kind == EX_FIELD && expr->type->kind == TY_ENUM && expr->field &&
               expr->a->kind == EX_NAME && !expr->a->symbol) {
        /* Enum qualifier is a type, not a value expression. */
    } else {
        cost = bound_add(cost, expr_cost(expr->a));
        cost = bound_add(cost, expr_cost(expr->b));
        cost = bound_add(cost, expr_cost(expr->c));
    }
    if (fe_is_aggregate(expr->type)) {
        cost = bound_add(cost, fe_type_cells(expr->type));
    }
    return cost;
}

static void join_bounds(Bounds *out, const Bounds *a, const Bounds *bounds) {
    for (unsigned i = 0; i < FE_LOCAL_LIMIT; ++i) {
        out->value[i] = a->value[i] > bounds->value[i] ? a->value[i] : bounds->value[i];
        out->length[i] = a->length[i] > bounds->length[i] ? a->length[i] : bounds->length[i];
    }
}

static uint64_t block_cost(FeStmt *, Bounds *);
/* Recognize exactly: for (var i: unsigned = 0; i < limit; i += 1).
 * Both the counter and the limit must survive each body execution unchanged. */
static FeSymbol *counting_loop_counter(FeStmt *loop) {
    FeStmt *init = loop->init;
    FeStmt *step = loop->step;
    FeExpr *condition = loop->a;

    if (init->kind != ST_DECL || !init->a || init->a->kind != EX_INT || init->a->integer) {
        return NULL;
    }
    FeSymbol *counter = init->symbol;
    if (!fe_is_integer(counter->type) || fe_is_signed(counter->type)) {
        return NULL;
    }
    if (condition->kind != EX_BINARY || condition->op != '<' || condition->a->kind != EX_NAME ||
        condition->a->symbol != counter) {
        return NULL;
    }
    if (step->kind != ST_ASSIGN || step->a->kind != EX_NAME || step->a->symbol != counter ||
        step->op != FE_T_ADD_ASSIGN || step->b->kind != EX_INT || step->b->integer != 1) {
        return NULL;
    }
    if (writes_symbol(loop->body, counter) || loop_limit_changes(condition->b, loop->body, step)) {
        return NULL;
    }
    return counter;
}

static uint64_t statement_cost(FeStmt *stmt, Bounds *bounds) {
    uint64_t cost = 1;
    switch (stmt->kind) {
    case ST_BLOCK:
        return block_cost(stmt->body, bounds);
    case ST_DECL:
        bounds->value[stmt->symbol->id] =
            stmt->a ? value_upper_bound(stmt->a, bounds) : type_upper_bound(stmt->type);
        return bound_add(bound_add(cost, expr_cost(stmt->a)), fe_type_cells(stmt->type));
    case ST_ASSIGN:
        if (stmt->a->kind == EX_NAME) {
            uint64_t n = value_upper_bound(stmt->b, bounds),
                     old = bounds->value[stmt->a->symbol->id];
            if (stmt->op == FE_T_ADD_ASSIGN) {
                n = bound_add(old, n);
            } else if (stmt->op == FE_T_SUB_ASSIGN) {
                n = old;
            } else if (stmt->op == FE_T_MUL_ASSIGN) {
                n = bound_multiply(old, n);
            } else if (stmt->op == FE_T_DIV_ASSIGN || stmt->op == FE_T_REM_ASSIGN) {
                /* Keep signed bounds conservative, including division by -1. */
                n = fe_is_signed(stmt->a->type) ? type_upper_bound(stmt->a->type) : old;
            }
            bounds->value[stmt->a->symbol->id] = n;
        }
        return bound_add(bound_add(bound_add(cost, expr_cost(stmt->a)), expr_cost(stmt->b)),
                         fe_type_cells(stmt->a->type));
    case ST_BREAK:
    case ST_CONTINUE:
        return cost;
    case ST_RETURN:
    case ST_EXPR:
        return bound_add(cost, expr_cost(stmt->a));
    case ST_IF: {
        Bounds yes = *bounds, no = *bounds;
        refine_condition(stmt->a, &yes, true);
        refine_condition(stmt->a, &no, false);
        uint64_t y = statement_cost(stmt->body, &yes),
                 n = stmt->other ? statement_cost(stmt->other, &no) : 0;
        bool ry = always_returns(stmt->body), rn = stmt->other && always_returns(stmt->other);
        if (ry && !rn) {
            *bounds = no;
        } else if (rn && !ry) {
            *bounds = yes;
        } else {
            join_bounds(bounds, &yes, &no);
        }
        return bound_add(bound_add(cost, expr_cost(stmt->a)), y > n ? y : n);
    }
    case ST_FOR: {
        uint64_t initial = statement_cost(stmt->init, bounds);
        FeSymbol *counter = counting_loop_counter(stmt);
        uint64_t n = counter ? value_upper_bound(stmt->a->b, bounds) : UNKNOWN;
        stmt->iterations = n;
        Bounds loop = *bounds;
        /* A loop body executes repeatedly: facts about outer values written by
         * that body are not invariant. Forget them before bounding its work. */
        invalidate_writes(stmt->body, &loop);
        invalidate_writes(stmt->step, &loop);
        if (counter) {
            loop.value[counter->id] = n;
        }
        uint64_t body = statement_cost(stmt->body, &loop), step = statement_cost(stmt->step, &loop);
        /* Jump paths may bypass later writes that restore an earlier bound. */
        if (stmt->has_break || stmt->has_continue) {
            invalidate_writes(stmt->body, bounds);
            invalidate_writes(stmt->step, bounds);
        }
        /* Outer assignments invalidate bounds after any number of iterations. */
        for (unsigned i = 0; i < FE_LOCAL_LIMIT; ++i) {
            if (loop.value[i] != bounds->value[i]) {
                bounds->value[i] = UNKNOWN;
            }
            if (loop.length[i] != bounds->length[i]) {
                bounds->length[i] = UNKNOWN;
            }
        }
        return bound_add(bound_add(initial, bound_multiply(bound_add(n, 1), expr_cost(stmt->a))),
                         bound_add(cost, bound_multiply(n, bound_add(bound_add(body, step), 1))));
    }
    case ST_WHILE:
        for (unsigned i = 0; i < FE_LOCAL_LIMIT; ++i) {
            bounds->value[i] = UNKNOWN;
        }
        return UNKNOWN;
    case ST_MATCH: {
        Bounds joined = *bounds;
        bool have = false;
        uint64_t largest = 0;
        for (FeCase *k = stmt->cases; k; k = k->next) {
            if (k->literal) {
                cost = bound_add(cost, bound_add(1, expr_cost(k->literal)));
            } else if (stmt->a->type->kind == TY_ENUM) {
                cost = bound_add(cost, 1);
            }
            Bounds branch = *bounds;
            if (k->symbol) {
                branch.value[k->symbol->id] = type_upper_bound(k->type);
            }
            uint64_t n = bound_add(fe_type_cells(k->type), statement_cost(k->body, &branch));
            if (n > largest) {
                largest = n;
            }
            if (!always_returns(k->body)) {
                if (have) {
                    join_bounds(&joined, &joined, &branch);
                } else {
                    joined = branch;
                }
                have = true;
            }
        }
        if (have) {
            *bounds = joined;
        }
        return bound_add(bound_add(cost, expr_cost(stmt->a)), largest);
    }
    }
    return UNKNOWN;
}

static uint64_t block_cost(FeStmt *stmt, Bounds *bounds) {
    uint64_t n = 0;
    for (; stmt; stmt = stmt->next) {
        n = bound_add(n, statement_cost(stmt, bounds));
    }
    return n;
}

static void analyze_expression_callees(FeExpr *expr) {
    for (; expr; expr = expr->next) {
        if (expr->kind == EX_CALL) {
            (void)function_cost(expr->function);
        }
        analyze_expression_callees(expr->a);
        analyze_expression_callees(expr->b);
        analyze_expression_callees(expr->c);
    }
}

static void analyze_statement_callees(FeStmt *stmt) {
    for (; stmt; stmt = stmt->next) {
        analyze_expression_callees(stmt->a);
        analyze_expression_callees(stmt->b);
        analyze_statement_callees(stmt->body);
        analyze_statement_callees(stmt->other);
        analyze_statement_callees(stmt->init);
        analyze_statement_callees(stmt->step);
        for (FeCase *branch = stmt->cases; branch; branch = branch->next) {
            analyze_statement_callees(branch->body);
        }
    }
}

static uint64_t function_body_cost(FeFunction *function) {
    Bounds bounds;
    for (unsigned i = 0; i < FE_LOCAL_LIMIT; ++i) {
        bounds.value[i] = bounds.length[i] = UNKNOWN;
    }
    uint64_t parameters = 0;
    for (FeSymbol *parameter = function->params; parameter; parameter = parameter->next) {
        bounds.value[parameter->id] = type_upper_bound(parameter->type);
        parameters = bound_add(parameters, fe_type_cells(parameter->type));
    }
    return bound_add(parameters, statement_cost(function->body, &bounds));
}

static uint64_t function_cost(FeFunction *function) {
    if (function->visit == 2) {
        return function->work;
    }
    if (function->visit == 1) {
        return UNKNOWN;
    }
    function->visit = 1;
    /* Resolve callees before allocating the larger per-body bounds snapshots.
     * A long helper chain must not keep one snapshot per call on the C stack. */
    analyze_statement_callees(function->body);
    if (function->native) {
        bool metadata =
            function->module && !strcmp(function->module->name, "std.buffer") &&
            (!strcmp(function->name, "element_size") || !strcmp(function->name, "truncate"));
        function->work = metadata ? 1 : UNKNOWN;
    } else {
        function->work = function->effects ? UNKNOWN : function_body_cost(function);
    }
    function->bounded = function->work != UNKNOWN;
    function->visit = 2;
    return function->work;
}

bool fe_analyze(FeProgram *p) {
    fe_analyze_effects(p);
    if (!fe_check_effects(p)) {
        return false;
    }
    for (FeFunction *function = p->functions; function; function = function->next) {
        function_cost(function);
    }
    for (FeFunction *function = p->functions; function; function = function->next) {
        if (function->budgeted && function->effects) {
            if (function->module) {
                p->ctx = function->module->ctx;
            }
            return fe_error(p->ctx, function->span, "E_EFFECT",
                            "%s: %s is not permitted in a budgeted function", function->name,
                            function->effects & FE_EFFECT_MEMORY ? "memory allocation or release"
                            : function->effects & FE_EFFECT_ENV_READ   ? "environment access"
                            : function->effects & FE_EFFECT_ARGS_READ  ? "process argument access"
                            : function->effects & FE_EFFECT_CLOCK_READ ? "clock access"
                            : function->effects &
                                    (FE_EFFECT_FS_OPEN | FE_EFFECT_FS_READ | FE_EFFECT_FS_CLOSE |
                                     FE_EFFECT_FS_WRITE | FE_EFFECT_FS_SYNC)
                                ? "file access or cleanup"
                                : "I/O");
        }
        if (function->budgeted && !function->bounded) {
            if (function->module) {
                p->ctx = function->module->ctx;
            }
            return fe_error(p->ctx, function->span, "E_BUDGET",
                            "%s: bound unknown (unbounded loop, recursive/helper call, or cost "
                            "arithmetic overflow); contract rejected",
                            function->name);
        }
        if (function->budgeted && function->work > function->budget) {
            if (function->module) {
                p->ctx = function->module->ctx;
            }
            return fe_error(
                p->ctx, function->span, "E_BUDGET",
                "%s: computed work %llu exceeds requested budget %llu, including reachable helpers",
                function->name, (unsigned long long)function->work,
                (unsigned long long)function->budget);
        }
    }
    fe_prove_ranges(p);
    return true;
}
