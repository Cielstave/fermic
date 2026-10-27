#include "fermi/emit.h"

/* Owner temporaries live at function scope. Slot numbers are reserved before
 * lowering ordinary C temporaries, so a fault never refers into an expired block. */
static unsigned plan_temp(FeEmitter *emitter, FeType *type) {
    FeOwnerTemp *slot = fe_alloc(emitter->program->ctx, sizeof(*slot));
    if (!slot) {
        return 0;
    }
    slot->type = type;
    slot->id = ++emitter->temporary;
    slot->next = emitter->owners;
    emitter->owners = slot;
    return slot->id;
}

static bool plan_expression(FeEmitter *emitter, FeExpr *expr) {
    for (; expr; expr = expr->next) {
        expr->owner_before = emitter->owners;
        if (!plan_expression(emitter, expr->a) || !plan_expression(emitter, expr->b) ||
            !plan_expression(emitter, expr->c)) {
            return false;
        }
        /* Borrow arguments lower to addresses/views, never owning values. */
        if (fe_has_resource(expr->type) && !expr->access) {
            expr->owner_temp = plan_temp(emitter, expr->type);
            if (!expr->owner_temp) {
                return false;
            }
        }
        expr->owner_last = emitter->owners;
    }
    return true;
}

static bool plan_statements(FeEmitter *emitter, FeStmt *stmt) {
    for (; stmt; stmt = stmt->next) {
        if (!plan_expression(emitter, stmt->a) || !plan_expression(emitter, stmt->b) ||
            !plan_statements(emitter, stmt->init) || !plan_statements(emitter, stmt->body) ||
            !plan_statements(emitter, stmt->step) || !plan_statements(emitter, stmt->other)) {
            return false;
        }
        for (FeCase *branch = stmt->cases; branch; branch = branch->next) {
            if (!plan_statements(emitter, branch->body)) {
                return false;
            }
        }
    }
    return true;
}

bool fe_plan_owners(FeEmitter *emitter) {
    if (emitter->function->native && fe_has_resource(emitter->function->result)) {
        emitter->native_result = plan_temp(emitter, emitter->function->result);
        return emitter->native_result != 0;
    }
    return plan_statements(emitter, emitter->function->body);
}

void fe_clear_temp(FeEmitter *emitter, FeType *type, unsigned id) {
    if (fe_has_resource(type)) {
        fe_emit_line(emitter, "t%u = (fe_t%u){0};", id, type->id);
    }
}

void fe_drop_value(FeEmitter *emitter, FeType *type, const char *place) {
    if (type->kind == TY_BUFFER) {
        fe_emit_line(emitter, "fermi_buffer_drop(&%s);", place);
    } else if (type->kind == TY_FILE) {
        fe_emit_line(emitter, "fermi_file_drop(&%s);", place);
    } else if ((fe_is_payload_enum(type) || type->kind == TY_STRUCT) && fe_has_resource(type)) {
        fe_emit_line(emitter, "fe_drop_t%u(&%s);", type->id, place);
    } else if (type->kind == TY_RESULT && fe_has_resource(type)) {
        fe_emit_line(emitter, "if ((%s).ok) {", place);
        ++emitter->indent;
        char payload[96];
        snprintf(payload, sizeof(payload), "(%s).value", place);
        fe_drop_value(emitter, type->base, payload);
        --emitter->indent;
        fe_emit_line(emitter, "}");
        fe_emit_line(emitter, "%s = (fe_t%u){0};", place, type->id);
    }
}

void fe_drop_symbol(FeEmitter *emitter, FeSymbol *symbol) {
    char place[32];
    snprintf(place, sizeof(place), "v%u", symbol->id);
    fe_drop_value(emitter, symbol->type, place);
}

static void drop_temps(FeEmitter *emitter, FeOwnerTemp *last, FeOwnerTemp *before) {
    for (FeOwnerTemp *slot = last; slot != before; slot = slot->next) {
        char place[32];
        snprintf(place, sizeof(place), "t%u", slot->id);
        fe_drop_value(emitter, slot->type, place);
    }
}

void fe_drop_expression(FeEmitter *emitter, FeExpr *expr) {
    if (expr) {
        drop_temps(emitter, expr->owner_last, expr->owner_before);
    }
}

void fe_drop_scope(FeEmitter *emitter, FeStmt *stmt) {
    FeSymbol *locals[FE_LOCAL_LIMIT];
    unsigned count = 0;
    for (; stmt; stmt = stmt->next) {
        if (stmt->kind == ST_DECL && fe_has_resource(stmt->symbol->type)) {
            locals[count++] = stmt->symbol;
        }
    }
    while (count) {
        fe_drop_symbol(emitter, locals[--count]);
    }
}

void fe_drop_function(FeEmitter *emitter) {
    drop_temps(emitter, emitter->owners, NULL);
    if (emitter->function->native) {
        for (FeSymbol *param = emitter->function->params; param; param = param->next) {
            fe_drop_symbol(emitter, param);
        }
    }
    /* Checker locals are linked in reverse declaration order, including params. */
    for (FeSymbol *local = emitter->function->locals; local; local = local->next) {
        fe_drop_symbol(emitter, local);
    }
}
