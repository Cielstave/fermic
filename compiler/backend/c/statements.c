#include "fermi/emit.h"
#include <string.h>

void fe_emit_stmt(FeEmitter *emitter, FeStmt *stmt) {
    for (; stmt; stmt = stmt->next) {
        unsigned a, b, p;
        switch (stmt->kind) {
        case ST_BLOCK:
            fe_emit_line(emitter, "{");
            ++emitter->indent;
            fe_emit_stmt(emitter, stmt->body);
            fe_drop_scope(emitter, stmt->body);
            --emitter->indent;
            fe_emit_line(emitter, "}");
            break;
        case ST_DECL:
            if (stmt->a) {
                a = fe_lower_expr(emitter, stmt->a);
                fe_emit_line(emitter, "v%u = t%u;", stmt->symbol->id, a);
                fe_clear_temp(emitter, stmt->type, a);
                fe_drop_expression(emitter, stmt->a);
            }
            break;
        case ST_ASSIGN:
            p = fe_lower_address(emitter, stmt->a);
            if (stmt->op != '=') {
                a = fe_emit_temp(emitter, stmt->a->type);
                fe_emit_line(emitter, "t%u = *p%u;", a, p);
            } else {
                a = 0;
            }
            b = fe_lower_expr(emitter, stmt->b);
            if (stmt->op != '=') {
                unsigned result = fe_emit_temp(emitter, stmt->a->type);
                if (stmt->arithmetic_proof == FE_PROOF_I16_SUM) {
                    fe_emit_line(emitter, "/* i16 sum: at most %llu additions from zero. */",
                                 (unsigned long long)stmt->proven_iterations);
                    fe_emit_line(emitter, "t%u = t%u + t%u;", result, a, b);
                } else {
                    fe_emit_arithmetic(emitter,
                                       stmt->op == FE_T_ADD_ASSIGN   ? '+'
                                       : stmt->op == FE_T_SUB_ASSIGN ? '-'
                                       : stmt->op == FE_T_MUL_ASSIGN ? '*'
                                       : stmt->op == FE_T_DIV_ASSIGN ? '/'
                                                                     : '%',
                                       stmt->a->type, a, b, result);
                }
                b = result;
            }
            if (fe_has_resource(stmt->a->type)) {
                char place[32];
                snprintf(place, sizeof(place), "(*p%u)", p);
                fe_drop_value(emitter, stmt->a->type, place);
            }
            fe_emit_line(emitter, "*p%u = t%u;", p, b);
            fe_clear_temp(emitter, stmt->b->type, b);
            fe_drop_expression(emitter, stmt->b);
            fe_drop_expression(emitter, stmt->a);
            break;
        case ST_EXPR:
            (void)fe_lower_expr(emitter, stmt->a);
            fe_drop_expression(emitter, stmt->a);
            break;
        case ST_BREAK:
        case ST_CONTINUE:
            for (FeDrop *drop = stmt->drops; drop; drop = drop->next) {
                fe_drop_symbol(emitter, drop->symbol);
            }
            fe_emit_line(emitter, "goto fe_%s_%u;", stmt->kind == ST_BREAK ? "break" : "continue",
                         stmt->loop->loop_label);
            break;
        case ST_RETURN:
            if (stmt->a) {
                a = fe_lower_expr(emitter, stmt->a);
                fe_emit_return(emitter, a);
            } else {
                fe_emit_return(emitter, 0);
            }
            break;
        case ST_IF:
            a = fe_lower_expr(emitter, stmt->a);
            fe_drop_expression(emitter, stmt->a);
            fe_emit_line(emitter, "if (t%u) {", a);
            ++emitter->indent;
            fe_emit_stmt(emitter, stmt->body);
            --emitter->indent;
            if (stmt->other) {
                fe_emit_line(emitter, "} else {");
                ++emitter->indent;
                fe_emit_stmt(emitter, stmt->other);
                --emitter->indent;
            }
            fe_emit_line(emitter, "}");
            break;
        case ST_FOR:
        case ST_WHILE:
            stmt->loop_label = ++emitter->temporary;
            if (stmt->init) {
                fe_emit_stmt(emitter, stmt->init);
            }
            fe_emit_line(emitter, "for (;;) {");
            ++emitter->indent;
            a = fe_lower_expr(emitter, stmt->a);
            fe_drop_expression(emitter, stmt->a);
            fe_emit_line(emitter, "if (!t%u) break;", a);
            fe_emit_stmt(emitter, stmt->body);
            if (stmt->has_continue) {
                fe_emit_line(emitter, "fe_continue_%u:;", stmt->loop_label);
            }
            if (stmt->step) {
                fe_emit_stmt(emitter, stmt->step);
            }
            --emitter->indent;
            fe_emit_line(emitter, "}");
            if (stmt->has_break) {
                fe_emit_line(emitter, "fe_break_%u:;", stmt->loop_label);
            }
            fe_drop_scope(emitter, stmt->init);
            break;
        case ST_MATCH: {
            a = fe_lower_expr(emitter, stmt->a);
            unsigned patterns[FE_MATCH_CASE_LIMIT] = {0};
            unsigned index = 0;
            /* Scalar patterns are checked constants. Lower them before the if
             * chain so temporary declarations cannot separate an else clause. */
            for (FeCase *branch = stmt->cases; branch; branch = branch->next, ++index) {
                if (branch->literal) {
                    patterns[index] = fe_lower_expr(emitter, branch->literal);
                }
            }
            index = 0;
            for (FeCase *k = stmt->cases; k; k = k->next, ++index) {
                bool ok = k->variant && !strcmp(k->variant, "ok");
                if (k->wildcard) {
                    fe_emit_line(emitter, "%s{", k == stmt->cases ? "" : "else ");
                } else if (k->literal) {
                    fe_emit_line(emitter, "%sif (t%u == t%u) {", k == stmt->cases ? "" : "else ", a,
                                 patterns[index]);
                } else if (stmt->a->type->kind == TY_ENUM) {
                    fe_emit_line(emitter, "%sif (t%u%s == %u) {", k == stmt->cases ? "" : "else ",
                                 a, fe_is_payload_enum(stmt->a->type) ? ".tag" : "",
                                 k->field->index);
                } else {
                    fe_emit_line(emitter, "%sif (%st%u.ok) {", k == stmt->cases ? "" : "else ",
                                 ok ? "" : "!", a);
                }
                ++emitter->indent;
                if (k->symbol) {
                    if (fe_is_payload_enum(stmt->a->type)) {
                        fe_emit_line(emitter, "v%u = t%u.data.m%u;", k->symbol->id, a,
                                     k->field->index);
                    } else {
                        fe_emit_line(emitter, "v%u = t%u.%s;", k->symbol->id, a,
                                     ok ? "value" : "error");
                    }
                }
                fe_clear_temp(emitter, stmt->a->type, a);
                fe_drop_expression(emitter, stmt->a);
                fe_emit_stmt(emitter, k->body);
                if (k->symbol) {
                    fe_drop_symbol(emitter, k->symbol);
                }
                --emitter->indent;
                fe_emit_line(emitter, "}");
            }
            break;
        }
        }
    }
}
