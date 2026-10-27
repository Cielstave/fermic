#include "fermi/fe.h"

/* This pass deliberately recognizes one reduction, not arbitrary interval
 * arithmetic. Its proof is independent of the abstract work-budget analysis. */
static bool names(const FeExpr *expr, const FeSymbol *symbol) {
    return expr && expr->kind == EX_NAME && expr->symbol == symbol;
}

static FeSymbol *length_owner(const FeExpr *expr) {
    if (!expr || expr->kind != EX_FIELD || expr->field || expr->a->kind != EX_NAME) {
        return NULL;
    }
    FeSymbol *owner = expr->a->symbol;
    if (owner->type->kind == TY_ARRAY || (owner->parameter && owner->type->kind == TY_READ)) {
        return owner;
    }
    return NULL;
}

static void length_guard(const FeStmt *stmt, uint64_t limits[FE_LOCAL_LIMIT]) {
    if (stmt->kind != ST_IF || stmt->other || stmt->a->kind != EX_BINARY || stmt->a->op != '>' ||
        stmt->a->b->kind != EX_INT || stmt->body->kind != ST_BLOCK) {
        return;
    }
    const FeStmt *exit = stmt->body->body;
    FeSymbol *owner = length_owner(stmt->a->a);
    if (!owner || !exit || exit->kind != ST_RETURN || exit->next) {
        return;
    }
    /* A parameter view's length cannot be assigned or changed by a callee.
     * Array extent is fixed by its type, even if its elements are mutable. */
    if (stmt->a->b->integer < limits[owner->id]) {
        limits[owner->id] = stmt->a->b->integer;
    }
}

static bool prove_sum(FeStmt *initial, const uint64_t limits[FE_LOCAL_LIMIT]) {
    FeStmt *loop = initial->next;
    if (initial->kind != ST_DECL || initial->type->kind != TY_I64 || !initial->a ||
        initial->a->kind != EX_INT || initial->a->integer != 0 || !loop || loop->kind != ST_FOR) {
        return false;
    }
    FeStmt *init = loop->init, *step = loop->step;
    if (init->kind != ST_DECL || init->type->kind != TY_USIZE || !init->a ||
        init->a->kind != EX_INT || init->a->integer != 0 || loop->a->kind != EX_BINARY ||
        loop->a->op != '<' || !names(loop->a->a, init->symbol) || step->kind != ST_ASSIGN ||
        step->op != FE_T_ADD_ASSIGN || !names(step->a, init->symbol) || step->b->kind != EX_INT ||
        step->b->integer != 1) {
        return false;
    }
    FeSymbol *owner = length_owner(loop->a->b);
    if (!owner || owner->type->base->kind != TY_I16) {
        return false;
    }
    uint64_t count = owner->type->kind == TY_ARRAY ? owner->type->count : limits[owner->id];
    /* Every prefix sum lies in [-32768*k, 32767*k]. This conservative limit
     * proves both endpoints fit without overflowing the proof arithmetic. */
    if (count > (uint64_t)INT64_MAX / 32768) {
        return false;
    }
    FeStmt *add = loop->body->body;
    if (!add || add->next || add->kind != ST_ASSIGN || add->op != FE_T_ADD_ASSIGN ||
        !names(add->a, initial->symbol) || add->b->kind != EX_WIDEN ||
        add->b->type->kind != TY_I64) {
        return false;
    }
    FeExpr *sample = add->b->a;
    if (sample->kind != EX_INDEX || !names(sample->a, owner) || !names(sample->b, init->symbol)) {
        return false;
    }
    /* Exactly one update per iteration, with no calls, branches, or other writes.
     * Index checks and the loop-counter overflow check are intentionally retained. */
    add->arithmetic_proof = FE_PROOF_I16_SUM;
    add->proven_iterations = count;
    return true;
}

void fe_prove_ranges(FeProgram *program) {
    for (FeFunction *function = program->functions; function; function = function->next) {
        if (function->native) {
            continue;
        }
        uint64_t limits[FE_LOCAL_LIMIT];
        for (unsigned i = 0; i < FE_LOCAL_LIMIT; ++i) {
            limits[i] = UINT64_MAX;
        }
        function->proven_overflows = 0;
        /* Only statements directly in the function body participate. Facts from
         * conditional or nested-loop bodies must not escape into this proof. */
        for (FeStmt *stmt = function->body->body; stmt; stmt = stmt->next) {
            length_guard(stmt, limits);
            function->proven_overflows += prove_sum(stmt, limits);
        }
    }
}
