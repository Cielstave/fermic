#include "fermi/emit.h"
#include <inttypes.h>
#include <stdarg.h>
#include <string.h>

void fe_emit_line(FeEmitter *emitter, const char *format, ...) {
    for (unsigned i = 0; i < emitter->indent; ++i) {
        fputs("    ", emitter->out);
    }
    va_list args;
    va_start(args, format);
    vfprintf(emitter->out, format, args);
    va_end(args);
    fputc('\n', emitter->out);
}

void fe_emit_type(FILE *out, const FeType *t) {
    static const char *names[] = {
        [TY_VOID] = "void",    [TY_BOOL] = "bool",    [TY_U8] = "uint8_t",  [TY_I8] = "int8_t",
        [TY_U16] = "uint16_t", [TY_I16] = "int16_t",  [TY_I32] = "int32_t", [TY_U32] = "uint32_t",
        [TY_I64] = "int64_t",  [TY_U64] = "uint64_t", [TY_USIZE] = "size_t"};
    if (t->kind <= TY_USIZE) {
        fputs(names[t->kind], out);
    } else {
        fprintf(out, "fe_t%u", t->id);
    }
}

unsigned fe_emit_temp(FeEmitter *emitter, FeType *t) {
    unsigned id = ++emitter->temporary;
    for (unsigned i = 0; i < emitter->indent; ++i) {
        fputs("    ", emitter->out);
    }
    fe_emit_type(emitter->out, t);
    fprintf(emitter->out, " t%u = {0};\n", id);
    return id;
}

static unsigned expression_temp(FeEmitter *emitter, FeExpr *expr) {
    return fe_has_resource(expr->type) ? expr->owner_temp : fe_emit_temp(emitter, expr->type);
}

static unsigned new_address(FeEmitter *emitter, FeType *t, bool readonly) {
    unsigned id = ++emitter->temporary;
    for (unsigned i = 0; i < emitter->indent; ++i) {
        fputs("    ", emitter->out);
    }
    if (readonly) {
        fputs("const ", emitter->out);
    }
    fe_emit_type(emitter->out, t);
    fprintf(emitter->out, " *p%u;\n", id);
    return id;
}

static void lower_buffer(FeEmitter *emitter, FeExpr *expr, unsigned *id, bool *array) {
    if (expr->type->kind == TY_BUFFER || expr->type->kind == TY_BUFFER_EDIT) {
        unsigned base = fe_lower_address(emitter, expr);
        FeType *buffer = expr->type->kind == TY_BUFFER_EDIT ? expr->type->base : expr->type;
        FeType *view = fe_type(emitter->program, TY_EDIT, buffer->base, NULL, 0);
        *id = fe_emit_temp(emitter, view);
        *array = false;
        fe_emit_line(emitter, "t%u.data = p%u->data; t%u.len = p%u->length;", *id, base, *id, base);
        return;
    }
    *array = expr->type->kind == TY_ARRAY;
    *id = *array ? fe_lower_address(emitter, expr) : fe_lower_expr(emitter, expr);
}

static bool readonly_address(const FeExpr *expr) {
    if (expr->type->kind == TY_STRUCT_READ || expr->type->kind == TY_READ) {
        return true;
    }
    return (expr->kind == EX_FIELD || expr->kind == EX_INDEX) && readonly_address(expr->a);
}

unsigned fe_lower_address(FeEmitter *emitter, FeExpr *expr) {
    if (expr->kind == EX_NAME && fe_is_owner_borrow(expr->type)) {
        unsigned p = new_address(emitter, expr->type->base, expr->type->kind == TY_STRUCT_READ);
        fe_emit_line(emitter, "p%u = v%u;", p, expr->symbol->id);
        return p;
    }
    if (expr->kind == EX_NAME) {
        unsigned p = new_address(emitter, expr->type, false);
        fe_emit_line(emitter, "p%u = &v%u;", p, expr->symbol->id);
        return p;
    }
    if (expr->kind == EX_FIELD) {
        unsigned base = fe_lower_address(emitter, expr->a),
                 p = new_address(emitter, expr->type, readonly_address(expr));
        fe_emit_line(emitter, "p%u = &p%u->m%u;", p, base, expr->field->index);
        return p;
    }
    if (expr->kind != EX_INDEX) {
        unsigned value = fe_lower_expr(emitter, expr), p = new_address(emitter, expr->type, false);
        fe_emit_line(emitter, "p%u = &t%u;", p, value);
        return p;
    }
    unsigned base;
    bool array;
    lower_buffer(emitter, expr->a, &base, &array);
    unsigned index = fe_lower_expr(emitter, expr->b);
    if (array) {
        fe_emit_line(emitter,
                     "if (t%u >= UINT64_C(%" PRIu64 ")) { fault->code = 1; goto fe_fault_exit; }",
                     index, expr->a->type->count);
    } else {
        fe_emit_line(emitter, "if (t%u >= t%u.len) { fault->code = 1; goto fe_fault_exit; }", index,
                     base);
    }
    unsigned p = new_address(emitter, expr->type,
                             expr->a->type->kind == TY_STR || readonly_address(expr->a));
    if (array) {
        fe_emit_line(emitter, "p%u = &p%u->data[t%u];", p, base, index);
    } else {
        fe_emit_line(emitter, "p%u = &t%u.data[t%u];", p, base, index);
    }
    return p;
}

unsigned fe_lower_view(FeEmitter *emitter, FeExpr *expr, FeType *type) {
    if (fe_is_owner_borrow(type)) {
        unsigned address = fe_lower_address(emitter, expr);
        unsigned result = fe_emit_temp(emitter, type);
        fe_emit_line(emitter, "t%u = p%u;", result, address);
        return result;
    }
    if (expr->kind == EX_SLICE) {
        unsigned src = fe_lower_expr(emitter, expr), dst = fe_emit_temp(emitter, type);
        fe_emit_line(emitter, "t%u.data = t%u.data; t%u.len = t%u.len;", dst, src, dst, src);
        return dst;
    }
    unsigned base;
    bool array;
    lower_buffer(emitter, expr, &base, &array);
    unsigned dst = fe_emit_temp(emitter, type);
    if (array) {
        fe_emit_line(emitter, "t%u.data = p%u->data; t%u.len = UINT64_C(%" PRIu64 ");", dst, base,
                     dst, expr->type->count);
    } else {
        fe_emit_line(emitter, "t%u.data = t%u.data; t%u.len = t%u.len;", dst, base, dst, base);
    }
    return dst;
}

static const char *integer_min_macro(FeType *t) {
    return t->kind == TY_I8    ? "INT8_MIN"
           : t->kind == TY_I16 ? "INT16_MIN"
           : t->kind == TY_I32 ? "INT32_MIN"
                               : "INT64_MIN";
}
static const char *integer_max_macro(FeType *t) {
    switch (t->kind) {
    case TY_U8:
        return "UINT8_MAX";
    case TY_I8:
        return "INT8_MAX";
    case TY_U16:
        return "UINT16_MAX";
    case TY_I16:
        return "INT16_MAX";
    case TY_I32:
        return "INT32_MAX";
    case TY_U32:
        return "UINT32_MAX";
    case TY_I64:
        return "INT64_MAX";
    case TY_U64:
        return "UINT64_MAX";
    default:
        return "SIZE_MAX";
    }
}

void fe_emit_arithmetic(FeEmitter *emitter, int op, FeType *type, unsigned a, unsigned b,
                        unsigned out) {
    if (op == '+' || op == '-' || op == '*') {
        const char *name = op == '+' ? "add" : op == '-' ? "sub" : "mul";
        fe_emit_line(
            emitter,
            "if (__builtin_%s_overflow(t%u, t%u, &t%u)) { fault->code = 2; goto fe_fault_exit; }",
            name, a, b, out);
    } else {
        fe_emit_line(emitter, "if (t%u == 0) { fault->code = 3; goto fe_fault_exit; }", b);
        if (fe_is_signed(type)) {
            fe_emit_line(emitter,
                         "if (t%u == %s && t%u == -1) { fault->code = 2; goto fe_fault_exit; }", a,
                         integer_min_macro(type), b);
        }
        fe_emit_line(emitter, "t%u = t%u %c t%u;", out, a, op, b);
    }
}

unsigned fe_lower_expr(FeEmitter *emitter, FeExpr *expr) {
    unsigned out = 0, a = 0, b = 0;
    switch (expr->kind) {
    case EX_STRING:
        out = expression_temp(emitter, expr);
        fprintf(emitter->out, "    static const uint8_t bytes_%u[] = {", out);
        if (!expr->integer) {
            fputs("0", emitter->out);
        }
        for (uint64_t i = 0; i < expr->integer; ++i) {
            fprintf(emitter->out, "%s%u", i ? "," : "", (unsigned char)expr->name[i]);
        }
        fputs("};\n", emitter->out);
        fe_emit_line(emitter, "t%u.data = bytes_%u;", out, out);
        fe_emit_line(emitter, "t%u.len = UINT64_C(%" PRIu64 ");", out, expr->integer);
        break;
    case EX_INT:
    case EX_BOOL:
        out = expression_temp(emitter, expr);
        fe_emit_line(emitter, "t%u = UINT64_C(%" PRIu64 ");", out, expr->integer);
        break;
    case EX_NAME:
        out = expression_temp(emitter, expr);
        fe_emit_line(emitter, "t%u = v%u;", out, expr->symbol->id);
        if (fe_has_resource(expr->type)) {
            fe_emit_line(emitter, "v%u = (fe_t%u){0};", expr->symbol->id, expr->type->id);
        }
        break;
    case EX_UNARY:
        if (expr->op == '-' && expr->a->kind == EX_INT && fe_is_signed(expr->type) &&
            expr->a->integer == (UINT64_C(1) << (fe_integer_bits(expr->type) - 1))) {
            out = expression_temp(emitter, expr);
            fe_emit_line(emitter, "t%u = %s;", out, integer_min_macro(expr->type));
            break;
        }
        a = fe_lower_expr(emitter, expr->a);
        out = expression_temp(emitter, expr);
        if (expr->op == '-') {
            fe_emit_line(emitter, "if (t%u == %s) { fault->code = 2; goto fe_fault_exit; }", a,
                         integer_min_macro(expr->type));
        }
        if (expr->op == '~') {
            fe_emit_line(emitter, "t%u = (", out);
            fe_emit_type(emitter->out, expr->type);
            fprintf(emitter->out, ")~(uint64_t)t%u;\n", a);
        } else {
            fe_emit_line(emitter, "t%u = %ct%u;", out, expr->op, a);
        }
        break;
    case EX_BINARY:
        a = fe_lower_expr(emitter, expr->a);
        if (expr->op == FE_T_AND || expr->op == FE_T_OR) {
            out = expression_temp(emitter, expr);
            fe_emit_line(emitter, "t%u = t%u;", out, a);
            fe_emit_line(emitter, "if (%st%u) {", expr->op == FE_T_OR ? "!" : "", a);
            ++emitter->indent;
            b = fe_lower_expr(emitter, expr->b);
            fe_emit_line(emitter, "t%u = t%u;", out, b);
            --emitter->indent;
            fe_emit_line(emitter, "}");
        } else {
            b = fe_lower_expr(emitter, expr->b);
            out = expression_temp(emitter, expr);
            if (expr->op == '+' || expr->op == '-' || expr->op == '*' || expr->op == '/' ||
                expr->op == '%') {
                fe_emit_arithmetic(emitter, expr->op, expr->type, a, b, out);
            } else if (expr->op == FE_T_SHL || expr->op == FE_T_SHR || expr->op == '&' ||
                       expr->op == '|' || expr->op == '^') {
                bool shift = expr->op == FE_T_SHL || expr->op == FE_T_SHR;
                if (shift) {
                    fe_emit_line(emitter, "if (t%u >= %u) { fault->code = 4; goto fe_fault_exit; }",
                                 b, fe_integer_bits(expr->type));
                }
                const char *op = expr->op == FE_T_SHL   ? "<<"
                                 : expr->op == FE_T_SHR ? ">>"
                                 : expr->op == '&'      ? "&"
                                 : expr->op == '|'      ? "|"
                                                        : "^";
                fe_emit_line(emitter, "t%u = (", out);
                fe_emit_type(emitter->out, expr->type);
                fprintf(emitter->out, ")((uint64_t)t%u %s (uint64_t)t%u);\n", a, op, b);
            } else {
                const char *op = expr->op == FE_T_EQ   ? "=="
                                 : expr->op == FE_T_NE ? "!="
                                 : expr->op == FE_T_LE ? "<="
                                 : expr->op == FE_T_GE ? ">="
                                 : expr->op == '<'     ? "<"
                                                       : ">";
                fe_emit_line(emitter, "t%u = t%u %s t%u;", out, a, op, b);
            }
        }
        break;
    case EX_CALL: {
        unsigned args[FE_PARAM_LIMIT], n = 0;
        FeSymbol *param = expr->function->params;
        for (FeExpr *arg = expr->b; arg; arg = arg->next, param = param->next) {
            args[n++] = arg->access ? fe_lower_view(emitter, arg, param->type)
                                    : fe_lower_expr(emitter, arg);
        }
        if (expr->type->kind != TY_VOID) {
            out = expression_temp(emitter, expr);
        }
        for (unsigned i = 0; i < emitter->indent; ++i) {
            fputs("    ", emitter->out);
        }
        if (out) {
            fprintf(emitter->out, "t%u = ", out);
        }
        fprintf(emitter->out, "fe_f%u(fault", expr->function->id);
        for (unsigned i = 0; i < n; ++i) {
            fprintf(emitter->out, ", t%u", args[i]);
        }
        fputs(");\n", emitter->out);
        param = expr->function->params;
        for (unsigned i = 0; i < n; ++i, param = param->next) {
            fe_clear_temp(emitter, param->type, args[i]);
        }
        fe_emit_line(emitter, "if (fault->code) goto fe_fault_exit;");
        break;
    }
    case EX_FIELD:
        if (expr->field && expr->type->kind == TY_ENUM && expr->a->kind == EX_NAME &&
            !expr->a->symbol) {
            out = expression_temp(emitter, expr);
            fe_emit_line(emitter, "t%u = %u;", out, expr->field->index);
        } else if (!expr->field) {
            out = expression_temp(emitter, expr);
            if (expr->a->type->kind == TY_BUFFER || expr->a->type->kind == TY_BUFFER_EDIT) {
                a = fe_lower_address(emitter, expr->a);
                fe_emit_line(emitter, "t%u = p%u->%s;", out, a,
                             !strcmp(expr->name, "capacity") ? "capacity" : "length");
            } else if (expr->a->type->kind == TY_ARRAY) {
                a = fe_lower_expr(emitter, expr->a);
                fe_emit_line(emitter, "(void)t%u; t%u = UINT64_C(%" PRIu64 ");", a, out,
                             expr->a->type->count);
            } else {
                a = fe_lower_expr(emitter, expr->a);
                fe_emit_line(emitter, "t%u = t%u.len;", out, a);
            }
        } else if (fe_has_resource(expr->a->type) || expr->a->type->kind == TY_STRUCT_READ ||
                   expr->a->type->kind == TY_STRUCT_EDIT) {
            /* Reading a field does not transfer the enclosing owner. */
            a = fe_lower_address(emitter, expr->a);
            out = expression_temp(emitter, expr);
            fe_emit_line(emitter, "t%u = p%u->m%u;", out, a, expr->field->index);
        } else {
            a = fe_lower_expr(emitter, expr->a);
            out = expression_temp(emitter, expr);
            fe_emit_line(emitter, "t%u = t%u.m%u;", out, a, expr->field->index);
        }
        break;
    case EX_INDEX:
        a = fe_lower_address(emitter, expr);
        out = expression_temp(emitter, expr);
        fe_emit_line(emitter, "t%u = *p%u;", out, a);
        break;
    case EX_SLICE: {
        bool array;
        lower_buffer(emitter, expr->a, &a, &array);
        b = fe_lower_expr(emitter, expr->b);
        unsigned end = fe_lower_expr(emitter, expr->c);
        if (array) {
            fe_emit_line(emitter,
                         "if (t%u > t%u || t%u > UINT64_C(%" PRIu64
                         ")) { fault->code = 1; goto fe_fault_exit; }",
                         b, end, end, expr->a->type->count);
        } else {
            fe_emit_line(emitter,
                         "if (t%u > t%u || t%u > t%u.len) { fault->code = 1; goto fe_fault_exit; }",
                         b, end, end, a);
        }
        out = expression_temp(emitter, expr);
        if (array) {
            fe_emit_line(emitter, "t%u.data = p%u->data + t%u;", out, a, b);
        } else {
            fe_emit_line(emitter, "t%u.data = t%u ? t%u.data + t%u : t%u.data;", out, b, a, b, a);
        }
        fe_emit_line(emitter, "t%u.len = t%u - t%u;", out, end, b);
        break;
    }
    case EX_REPEAT: {
        a = fe_lower_expr(emitter, expr->a);
        out = expression_temp(emitter, expr);
        unsigned index = ++emitter->temporary;
        fe_emit_line(emitter, "for (size_t i%u = 0; i%u < UINT64_C(%" PRIu64 "); ++i%u) {", index,
                     index, expr->integer, index);
        ++emitter->indent;
        fe_emit_line(emitter, "t%u.data[i%u] = t%u;", out, index, a);
        --emitter->indent;
        fe_emit_line(emitter, "}");
        break;
    }
    case EX_ARRAY:
    case EX_STRUCT: {
        out = expression_temp(emitter, expr);
        unsigned n = 0;
        for (FeExpr *value = expr->a; value; value = value->next, ++n) {
            a = fe_lower_expr(emitter, value);
            if (expr->kind == EX_ARRAY) {
                fe_emit_line(emitter, "t%u.data[%u] = t%u;", out, n, a);
            } else {
                fe_emit_line(emitter, "t%u.m%u = t%u;", out, n, a);
                fe_clear_temp(emitter, value->type, a);
            }
        }
        break;
    }
    case EX_COPY:
    case EX_MOVE:
        out = fe_lower_expr(emitter, expr->a);
        break;
    case EX_VARIANT:
        out = expression_temp(emitter, expr);
        if (expr->a) {
            a = fe_lower_expr(emitter, expr->a);
            fe_emit_line(emitter, "t%u.data.m%u = t%u;", out, expr->field->index, a);
            fe_clear_temp(emitter, expr->a->type, a);
        }
        fe_emit_line(emitter, "t%u.tag = %u;", out, expr->field->index);
        break;
    case EX_TRY:
        a = fe_lower_expr(emitter, expr->a);
        fe_emit_line(emitter, "if (!t%u.ok) {", a);
        ++emitter->indent;
        b = fe_emit_temp(emitter, emitter->function->result);
        fe_emit_line(emitter, "t%u.error = t%u.error;", b, a);
        fe_emit_return(emitter, b);
        --emitter->indent;
        fe_emit_line(emitter, "}");
        if (expr->type->kind != TY_VOID) {
            out = expression_temp(emitter, expr);
            fe_emit_line(emitter, "t%u = t%u.value;", out, a);
        }
        fe_clear_temp(emitter, expr->a->type, a);
        break;
    case EX_OK:
    case EX_ERR:
        out = expression_temp(emitter, expr);
        if (expr->a) {
            a = fe_lower_expr(emitter, expr->a);
            fe_emit_line(emitter, "t%u.ok = %s; t%u.%s = t%u;", out,
                         expr->kind == EX_OK ? "true" : "false", out,
                         expr->kind == EX_OK ? "value" : "error", a);
            fe_clear_temp(emitter, expr->a->type, a);
        } else {
            fe_emit_line(emitter, "t%u.ok = true;", out);
        }
        break;
    case EX_WIDEN:
        a = fe_lower_expr(emitter, expr->a);
        out = expression_temp(emitter, expr);
        fe_emit_line(emitter, "t%u = t%u;", out, a);
        break;
    case EX_NARROW: {
        a = fe_lower_expr(emitter, expr->a);
        out = expression_temp(emitter, expr);
        FeType *to = expr->type->base;
        if (fe_is_signed(expr->a->type)) {
            if (fe_is_signed(to)) {
                fe_emit_line(emitter, "t%u.ok = (int64_t)t%u >= %s && (int64_t)t%u <= %s;", out, a,
                             integer_min_macro(to), a, integer_max_macro(to));
            } else {
                fe_emit_line(emitter, "t%u.ok = t%u >= 0 && (uint64_t)t%u <= %s;", out, a, a,
                             integer_max_macro(to));
            }
        } else {
            fe_emit_line(emitter, "t%u.ok = (uint64_t)t%u <= %s;", out, a, integer_max_macro(to));
        }
        fe_emit_line(emitter, "if (t%u.ok) t%u.value = t%u;", out, out, a);
        break;
    }
    }
    return out;
}
