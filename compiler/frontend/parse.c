#include "fermi/fe.h"
#include <setjmp.h>
#include <string.h>

typedef struct {
    FeProgram *program;
    size_t pos;
    unsigned depth;
    const char *type_parameter;
    FeType *type_argument;
    bool pending_greater;
    FeToken greater;
    jmp_buf stop;
} Parser;
static FeToken *peek_token(Parser *parser) {
    return parser->pending_greater ? &parser->greater : &parser->program->tokens.items[parser->pos];
}
static void parse_error(Parser *parser, const char *message) {
    fe_error(parser->program->ctx, peek_token(parser)->span, "E_PARSE", "%s", message);
    longjmp(parser->stop, 1);
}

static void *new_node(Parser *parser, size_t size) {
    if (++parser->program->ctx->nodes > FE_NODE_LIMIT) {
        parse_error(parser, "syntax node limit exceeded");
    }
    void *v = fe_alloc(parser->program->ctx, size);
    if (!v) {
        longjmp(parser->stop, 1);
    }
    return v;
}

static bool consume_token(Parser *parser, int kind) {
    if (peek_token(parser)->kind != kind) {
        return false;
    }
    ++parser->pos;
    parser->pending_greater = false;
    return true;
}

static FeToken *expect_token(Parser *parser, int kind, const char *message) {
    FeToken *t = peek_token(parser);
    if (!consume_token(parser, kind)) {
        parse_error(parser, message);
    }
    return t;
}

/* In a type argument list, consume one closing delimiter from >>. */
static void close_type(Parser *parser) {
    FeToken *token = peek_token(parser);
    if (token->kind == FE_T_SHR) {
        parser->greater = *token;
        parser->greater.kind = '>';
        ++parser->greater.text;
        --parser->greater.length;
        ++parser->greater.span.offset;
        ++parser->greater.span.column;
        parser->pending_greater = true;
        return;
    }
    expect_token(parser, '>', "expected '>' after type argument");
}

static const char *parse_identifier(Parser *parser) {
    FeToken *t = expect_token(parser, FE_T_NAME, "expected identifier");
    char *identifier = fe_string(parser->program->ctx, t->text, t->length);
    if (!identifier) {
        longjmp(parser->stop, 1);
    }
    return identifier;
}

static void parse_effect_contract(Parser *parser, FeFunction *function) {
    if (function->effects_declared) {
        parse_error(parser, "duplicate @effects attribute");
    }
    function->effects_declared = true;
    expect_token(parser, '(', "expected '(' after @effects");
    if (consume_token(parser, ')')) {
        return;
    }
    do {
        const char *group = parse_identifier(parser);
        char name[64];
        int length;
        if (consume_token(parser, '.')) {
            /* read is a keyword in expressions, but a name in an effect path. */
            FeToken *member = peek_token(parser);
            if (member->kind != FE_T_NAME && member->kind != FE_T_READ) {
                parse_error(parser, "expected effect name after '.'");
            }
            ++parser->pos;
            length =
                snprintf(name, sizeof(name), "%s.%.*s", group, (int)member->length, member->text);
        } else {
            length = snprintf(name, sizeof(name), "%s", group);
        }
        unsigned effect = length >= (int)sizeof(name) ? 0 : fe_effect_named(name);
        if (!effect) {
            parse_error(parser, "unknown effect name in @effects");
        }
        if (function->allowed_effects & effect) {
            parse_error(parser, "duplicate effect in @effects");
        }
        function->allowed_effects |= effect;
    } while (consume_token(parser, ','));
    expect_token(parser, ')', "expected ')' after effect list");
}

static void parse_budget(Parser *parser, FeFunction *function) {
    if (function->budgeted) {
        parse_error(parser, "duplicate @budget attribute");
    }
    expect_token(parser, '(', "expected '('");
    if (strcmp(parse_identifier(parser), "work")) {
        parse_error(parser, "expected work");
    }
    expect_token(parser, '=', "expected '='");
    function->budget = expect_token(parser, FE_T_INTEGER, "budget requires integer")->integer;
    expect_token(parser, ',', "expected ','");
    if (strcmp(parse_identifier(parser), "heap")) {
        parse_error(parser, "expected heap");
    }
    expect_token(parser, '=', "expected '='");
    if (expect_token(parser, FE_T_INTEGER, "expected zero")->integer) {
        parse_error(parser, "pilot heap budget must be zero");
    }
    expect_token(parser, ')', "expected ')'");
    function->budgeted = true;
}

static void enter_nesting(Parser *parser) {
    if (++parser->depth > FE_DEPTH_LIMIT) {
        parse_error(parser, "nesting exceeds 128");
    }
}

static FeType *parse_type(Parser *parser) {
    enter_nesting(parser);
    int access = 0;
    if (consume_token(parser, FE_T_READ)) {
        access = TY_READ;
    } else if (consume_token(parser, FE_T_EDIT)) {
        access = TY_EDIT;
    }
    const char *n = parse_identifier(parser);
    if (consume_token(parser, '.')) {
        const char *member = parse_identifier(parser);
        size_t size = strlen(n) + strlen(member) + 2;
        char *qualified = fe_alloc(parser->program->ctx, size);
        if (!qualified) {
            longjmp(parser->stop, 1);
        }
        snprintf(qualified, size, "%s.%s", n, member);
        n = qualified;
    }
    FeType *t = NULL;
    if (parser->type_parameter && !strcmp(n, parser->type_parameter)) {
        t = parser->type_argument;
    } else if (!strcmp(n, "Buffer")) {
        expect_token(parser, '<', "expected '<' after Buffer");
        FeType *base = parse_type(parser);
        close_type(parser);
        if (!fe_is_data(base)) {
            parse_error(parser, "Buffer elements must be nonresource data types");
        }
        t = fe_type(parser->program, TY_BUFFER, base, NULL, 0);
        if (!fe_type(parser->program, TY_EDIT, base, NULL, 0)) {
            longjmp(parser->stop, 1);
        }
    } else if (!strcmp(n, "Result")) {
        expect_token(parser, '<', "expected '<' after Result");
        FeType *base = parse_type(parser);
        expect_token(parser, ',', "expected ',' in Result");
        FeType *err = parse_type(parser);
        close_type(parser);
        if (base->kind == TY_READ || base->kind == TY_EDIT || fe_is_owner_borrow(base) ||
            err->kind != TY_ENUM || fe_is_payload_enum(err)) {
            parse_error(parser, "Result requires an owning value and a fieldless enum error type");
        }
        if (base->kind == TY_RESULT && fe_has_resource(base)) {
            parse_error(parser, "nested resource Results are not supported");
        }
        t = fe_type(parser->program, TY_RESULT, base, err, 0);
    } else {
        t = fe_named_type(parser->program, n);
    }
    if (!t) {
        parse_error(parser, "unknown type; declare nominal types before use");
    }
    if (consume_token(parser, '[')) {
        if (access) {
            expect_token(parser, ']', "view type uses empty []");
            if (!fe_is_data(t)) {
                parse_error(parser, "views require nonresource data elements");
            }
            t = fe_type(parser->program, (FeTypeKind)access, t, NULL, 0);
        } else {
            uint64_t count =
                expect_token(parser, FE_T_INTEGER, "array length must be a literal")->integer;
            expect_token(parser, ']', "expected ']'");
            if (!count || count > 65536 ||
                (t->kind == TY_VOID || t->kind == TY_RESULT || t->kind == TY_READ ||
                 t->kind == TY_EDIT || fe_is_owner_borrow(t) || fe_has_resource(t)) ||
                fe_type_cells(t) > 65536 / count) {
                parse_error(parser, "array size must be 1..65536 scalar cells");
            }
            t = fe_type(parser->program, TY_ARRAY, t, NULL, count);
        }
    } else if (access && t->kind == TY_STRUCT) {
        t = fe_type(parser->program, access == TY_READ ? TY_STRUCT_READ : TY_STRUCT_EDIT, t, NULL,
                    0);
    } else if (access == TY_EDIT && (t->kind == TY_BUFFER || t->kind == TY_FILE)) {
        t = fe_type(parser->program, t->kind == TY_FILE ? TY_FILE_EDIT : TY_BUFFER_EDIT, t, NULL,
                    0);
    } else if (access) {
        parse_error(parser, "view requires [] or an exclusive owner borrow");
    }
    if (!t) {
        longjmp(parser->stop, 1);
    }
    if (fe_type_cells(t) > 65536) {
        parse_error(parser, "type exceeds 65536 scalar cells");
    }
    --parser->depth;
    return t;
}

/* A recognized type name followed by >( starts an explicit specialization.
 * Ordinary comparisons remain in the binary-expression parser. */
static bool generic_call_ahead(Parser *parser) {
    FeTokens tokens = parser->program->tokens;
    size_t position = parser->pos;
    if (tokens.items[position].kind != '<' || position + 3 >= tokens.count) {
        return false;
    }
    FeToken *first = &tokens.items[++position];
    if (first->kind != FE_T_NAME) {
        return false;
    }
    char name[2 * FE_MODULE_NAME_LIMIT + 2];
    int length;
    if (tokens.items[position + 1].kind == '.') {
        if (position + 5 >= tokens.count || tokens.items[position + 2].kind != FE_T_NAME) {
            return false;
        }
        FeToken *member = &tokens.items[position + 2];
        length = snprintf(name, sizeof(name), "%.*s.%.*s", (int)first->length, first->text,
                          (int)member->length, member->text);
        position += 2;
    } else {
        length = snprintf(name, sizeof(name), "%.*s", (int)first->length, first->text);
    }
    if (length < 0 || (size_t)length >= sizeof(name) || tokens.items[position + 1].kind != '>' ||
        tokens.items[position + 2].kind != '(') {
        return false;
    }
    return (parser->type_parameter && !strcmp(name, parser->type_parameter)) ||
           fe_named_type(parser->program, name);
}

static FeExpr *parse_expression(Parser *, int);
static FeExpr *new_expression(Parser *parser, FeExprKind kind, FeSpan span) {
    FeExpr *expression = new_node(parser, sizeof(*expression));
    expression->kind = kind;
    expression->span = span;
    return expression;
}

static FeExpr *parse_arguments(Parser *parser, int end, FeExpr *array) {
    FeExpr *head = NULL, **tail = &head;
    if (consume_token(parser, end)) {
        return head;
    }
    do {
        const char *label = NULL;
        if (peek_token(parser)->kind == FE_T_NAME &&
            parser->program->tokens.items[parser->pos + 1].kind == '=') {
            label = parse_identifier(parser);
            ++parser->pos;
        }
        int access = 0;
        if (consume_token(parser, FE_T_READ)) {
            access = TY_READ;
        } else if (consume_token(parser, FE_T_EDIT)) {
            access = TY_EDIT;
        }
        FeExpr *expression = parse_expression(parser, 0);
        expression->label = label;
        expression->access = access;
        *tail = expression;
        tail = &expression->next;
        if (array && consume_token(parser, ';')) {
            if (head != expression) {
                parse_error(parser, "repeated array initializer requires one value");
            }
            uint64_t count =
                expect_token(parser, FE_T_INTEGER, "array repeat count must be an integer literal")
                    ->integer;
            if (!count || count > 65536) {
                parse_error(parser, "array repeat count must be 1..65536");
            }
            array->kind = EX_REPEAT;
            array->integer = count;
            expect_token(parser, end, "expected ']' after array repeat count");
            return head;
        }
    } while (consume_token(parser, ','));
    expect_token(parser, end, "expected closing delimiter");
    return head;
}

static FeExpr *parse_primary(Parser *parser) {
    FeToken *t = peek_token(parser);
    FeExpr *expression = NULL;
    if (consume_token(parser, FE_T_INTEGER)) {
        expression = new_expression(parser, EX_INT, t->span);
        expression->integer = t->integer;
    } else if (consume_token(parser, FE_T_TRUE) || consume_token(parser, FE_T_FALSE)) {
        expression = new_expression(parser, EX_BOOL, t->span);
        expression->integer = t->kind == FE_T_TRUE;
    } else if (consume_token(parser, FE_T_STRING)) {
        expression = new_expression(parser, EX_STRING, t->span);
        expression->name = t->text;
        expression->integer = t->length;
    } else if (consume_token(parser, FE_T_TRY)) {
        expression = new_expression(parser, EX_TRY, t->span);
        expect_token(parser, '(', "expected '(' after try");
        expression->a = parse_expression(parser, 0);
        expect_token(parser, ')', "expected ')' after try operand");
    } else if (consume_token(parser, '(')) {
        expression = parse_expression(parser, 0);
        expect_token(parser, ')', "expected ')'");
    } else if (consume_token(parser, '[')) {
        expression = new_expression(parser, EX_ARRAY, t->span);
        expression->a = parse_arguments(parser, ']', expression);
    } else if (t->kind == FE_T_NAME) {
        const char *n = parse_identifier(parser);
        if ((!strcmp(n, "widen") || !strcmp(n, "narrow")) && consume_token(parser, '<')) {
            expression =
                new_expression(parser, !strcmp(n, "widen") ? EX_WIDEN : EX_NARROW, t->span);
            expression->cast = parse_type(parser);
            close_type(parser);
            expect_token(parser, '(', "expected '('");
            expression->a = parse_expression(parser, 0);
            expect_token(parser, ')', "expected ')'");
        } else {
            expression = new_expression(parser, EX_NAME, t->span);
            expression->name = n;
        }
    } else if (consume_token(parser, '!') || consume_token(parser, '-') ||
               consume_token(parser, '+') || consume_token(parser, '~')) {
        expression = new_expression(parser, EX_UNARY, t->span);
        expression->op = t->kind;
        /* Prefix operators bind more tightly than every binary operator. */
        expression->a = parse_expression(parser, 10);
    } else {
        parse_error(parser, "expected expression");
    }
    unsigned suffixes = 0;
    for (;;) {
        if (++suffixes > FE_DEPTH_LIMIT) {
            parse_error(parser, "expression nesting limit");
        }
        FeType *argument = NULL;
        if ((expression->kind == EX_NAME || expression->kind == EX_FIELD) &&
            generic_call_ahead(parser)) {
            expect_token(parser, '<', "expected '<'");
            argument = parse_type(parser);
            close_type(parser);
        }
        if (consume_token(parser, '(')) {
            FeExpr *call = new_expression(parser, EX_CALL, expression->span);
            call->type_argument = argument;
            call->a = expression;
            call->b = parse_arguments(parser, ')', NULL);
            expression = call;
        } else if (consume_token(parser, '.')) {
            FeExpr *field = new_expression(parser, EX_FIELD, expression->span);
            field->a = expression;
            field->name = parse_identifier(parser);
            expression = field;
        } else if (consume_token(parser, '[')) {
            FeExpr *index = new_expression(parser, EX_INDEX, expression->span);
            index->a = expression;
            index->b = parse_expression(parser, 0);
            if (consume_token(parser, FE_T_RANGE)) {
                index->kind = EX_SLICE;
                index->c = parse_expression(parser, 0);
            }
            expect_token(parser, ']', "expected ']'");
            expression = index;
        } else {
            break;
        }
    }
    return expression;
}

static int precedence(int op) {
    switch (op) {
    case FE_T_OR:
        return 1;
    case FE_T_AND:
        return 2;
    case '|':
        return 3;
    case '^':
        return 4;
    case '&':
        return 5;
    case FE_T_EQ:
    case FE_T_NE:
        return 6;
    case '<':
    case '>':
    case FE_T_LE:
    case FE_T_GE:
        return 7;
    case FE_T_SHL:
    case FE_T_SHR:
        return 8;
    case '+':
    case '-':
        return 9;
    case '*':
    case '/':
    case '%':
        return 10;
    default:
        return 0;
    }
}

static FeExpr *parse_expression(Parser *parser, int min) {
    enter_nesting(parser);
    FeExpr *left = parse_primary(parser);
    unsigned chain = 0;
    while (precedence(peek_token(parser)->kind) > min) {
        if (++chain > 64) {
            parse_error(parser, "expression chain exceeds 64 operators");
        }
        FeToken *op = peek_token(parser);
        ++parser->pos;
        FeExpr *expression = new_expression(parser, EX_BINARY, op->span);
        expression->op = op->kind;
        expression->a = left;
        expression->b = parse_expression(parser, precedence(op->kind));
        left = expression;
    }
    --parser->depth;
    return left;
}

static FeStmt *parse_statement(Parser *);
static FeStmt *new_statement(Parser *parser, FeStmtKind k) {
    FeStmt *statement = new_node(parser, sizeof(*statement));
    statement->kind = k;
    statement->span = peek_token(parser)->span;
    return statement;
}

static FeStmt *parse_block(Parser *parser) {
    enter_nesting(parser);
    FeStmt *statement = new_statement(parser, ST_BLOCK);
    expect_token(parser, '{', "expected '{'");
    FeStmt **tail = &statement->body;
    while (!consume_token(parser, '}')) {
        if (peek_token(parser)->kind == FE_T_EOF) {
            parse_error(parser, "unterminated block");
        }
        *tail = parse_statement(parser);
        tail = &(*tail)->next;
    }
    --parser->depth;
    return statement;
}

static FeStmt *parse_simple_statement(Parser *parser) {
    FeStmt *statement;
    if (peek_token(parser)->kind == FE_T_LET || peek_token(parser)->kind == FE_T_VAR) {
        statement = new_statement(parser, ST_DECL);
        statement->mutable = consume_token(parser, FE_T_VAR);
        if (!statement->mutable) {
            ++parser->pos;
        }
        statement->name = parse_identifier(parser);
        expect_token(parser, ':', "binding requires explicit type");
        statement->type = parse_type(parser);
        if (consume_token(parser, '=')) {
            statement->a = parse_expression(parser, 0);
        } else if (!statement->mutable) {
            parse_error(parser, "let requires an initializer");
        }
    } else {
        statement = new_statement(parser, ST_EXPR);
        statement->a = parse_expression(parser, 0);
        int op = peek_token(parser)->kind;
        if (op == '=' || op == FE_T_ADD_ASSIGN || op == FE_T_SUB_ASSIGN || op == FE_T_MUL_ASSIGN ||
            op == FE_T_DIV_ASSIGN || op == FE_T_REM_ASSIGN) {
            ++parser->pos;
            statement->kind = ST_ASSIGN;
            statement->op = op;
            statement->b = parse_expression(parser, 0);
        }
    }
    return statement;
}

static FeExpr *parse_case_literal(Parser *parser) {
    FeToken *pattern = peek_token(parser);
    if (pattern->kind != FE_T_INTEGER && pattern->kind != '-' && pattern->kind != FE_T_TRUE &&
        pattern->kind != FE_T_FALSE) {
        return NULL;
    }
    bool negative = consume_token(parser, '-');
    FeToken *literal = peek_token(parser);
    if (negative) {
        expect_token(parser, FE_T_INTEGER, "negative case requires an integer literal");
    } else {
        ++parser->pos;
    }
    FeExprKind kind = literal->kind == FE_T_INTEGER ? EX_INT : EX_BOOL;
    FeExpr *expression = new_expression(parser, kind, literal->span);
    expression->integer = kind == EX_INT ? literal->integer : literal->kind == FE_T_TRUE;
    if (negative) {
        FeExpr *negation = new_expression(parser, EX_UNARY, pattern->span);
        negation->op = '-';
        negation->a = expression;
        return negation;
    }
    return expression;
}

static FeStmt *parse_statement(Parser *parser) {
    enter_nesting(parser);
    FeStmt *statement = NULL;
    if (peek_token(parser)->kind == '{') {
        statement = parse_block(parser);
    } else if (consume_token(parser, FE_T_IF)) {
        statement = new_statement(parser, ST_IF);
        expect_token(parser, '(', "if requires '('");
        statement->a = parse_expression(parser, 0);
        expect_token(parser, ')', "expected ')'");
        statement->body = parse_block(parser);
        if (consume_token(parser, FE_T_ELSE)) {
            statement->other =
                peek_token(parser)->kind == FE_T_IF ? parse_statement(parser) : parse_block(parser);
        }
    } else if (consume_token(parser, FE_T_FOR)) {
        statement = new_statement(parser, ST_FOR);
        expect_token(parser, '(', "for requires '('");
        statement->init = parse_simple_statement(parser);
        expect_token(parser, ';', "expected ';'");
        statement->a = parse_expression(parser, 0);
        expect_token(parser, ';', "expected ';'");
        statement->step = parse_simple_statement(parser);
        if (statement->step->kind == ST_DECL) {
            parse_error(parser, "for step requires an assignment or expression");
        }
        expect_token(parser, ')', "expected ')'");
        statement->body = parse_block(parser);
    } else if (consume_token(parser, FE_T_WHILE)) {
        statement = new_statement(parser, ST_WHILE);
        expect_token(parser, '(', "while requires '('");
        statement->a = parse_expression(parser, 0);
        expect_token(parser, ')', "expected ')'");
        statement->body = parse_block(parser);
    } else if (peek_token(parser)->kind == FE_T_BREAK ||
               peek_token(parser)->kind == FE_T_CONTINUE) {
        statement =
            new_statement(parser, peek_token(parser)->kind == FE_T_BREAK ? ST_BREAK : ST_CONTINUE);
        ++parser->pos;
        expect_token(parser, ';', "loop control statement requires ';'");
    } else if (consume_token(parser, FE_T_RETURN)) {
        statement = new_statement(parser, ST_RETURN);
        if (peek_token(parser)->kind != ';') {
            statement->a = parse_expression(parser, 0);
        }
        expect_token(parser, ';', "return requires ';'");
    } else if (consume_token(parser, FE_T_MATCH)) {
        statement = new_statement(parser, ST_MATCH);
        expect_token(parser, '(', "match requires '('");
        statement->a = parse_expression(parser, 0);
        expect_token(parser, ')', "expected ')'");
        expect_token(parser, '{', "expected '{'");
        FeCase **tail = &statement->cases;
        unsigned case_count = 0;
        while (!consume_token(parser, '}')) {
            expect_token(parser, FE_T_CASE, "expected case");
            FeCase *c = new_node(parser, sizeof(*c));
            c->span = peek_token(parser)->span;
            if (++case_count > FE_MATCH_CASE_LIMIT) {
                parse_error(parser, "match exceeds 128 cases");
            }
            c->literal = parse_case_literal(parser);
            if (c->literal) {
                c->body = parse_block(parser);
                *tail = c;
                tail = &c->next;
                continue;
            }
            c->owner = parse_identifier(parser);
            if (!strcmp(c->owner, "_") && peek_token(parser)->kind == '{') {
                c->wildcard = true;
                c->body = parse_block(parser);
                *tail = c;
                tail = &c->next;
                continue;
            }
            expect_token(parser, '.', "expected '.'");
            c->variant = parse_identifier(parser);
            if (consume_token(parser, '.')) {
                size_t size = strlen(c->owner) + strlen(c->variant) + 2;
                char *owner = new_node(parser, size);
                snprintf(owner, size, "%s.%s", c->owner, c->variant);
                c->owner = owner;
                c->variant = parse_identifier(parser);
            }
            bool result = !strcmp(c->owner, "Result");
            if (result) {
                expect_token(parser, '(', "expected '('");
            }
            if (result || consume_token(parser, '(')) {
                c->payload_pattern = true;
                if (!consume_token(parser, ')')) {
                    expect_token(parser, FE_T_LET, "case payload requires let");
                    c->name = parse_identifier(parser);
                    expect_token(parser, ':', "payload requires type");
                    c->type = parse_type(parser);
                    expect_token(parser, ')', "expected ')'");
                }
            }
            c->body = parse_block(parser);
            *tail = c;
            tail = &c->next;
        }
    } else {
        statement = parse_simple_statement(parser);
        expect_token(parser, ';', "statement requires ';'");
    }
    --parser->depth;
    return statement;
}

static void parse_type_alias(Parser *parser, bool public) {
    FeSpan span = peek_token(parser)->span;
    const char *name = parse_identifier(parser);
    FeProgram *program = parser->program;
    if (fe_named_type(program, name) || !strcmp(name, "Result") || !strcmp(name, "Buffer") ||
        !strcmp(name, "copy") || !strcmp(name, "move") || !strcmp(name, "widen") ||
        !strcmp(name, "narrow")) {
        parse_error(parser, "duplicate or reserved type alias name");
    }
    for (FeFunction *function = program->functions; function; function = function->next) {
        if (function->module == program->module && !strcmp(function->name, name)) {
            parse_error(parser, "type alias name conflicts with a function");
        }
    }
    if (fe_named_generic(program, name)) {
        parse_error(parser, "type alias name conflicts with a generic function");
    }
    if (program->alias_count >= 1024) {
        parse_error(parser, "type alias limit exceeded");
    }
    expect_token(parser, '=', "type alias requires '='");
    FeType *target = parse_type(parser);
    expect_token(parser, ';', "type alias requires ';'");
    FeAlias *alias = new_node(parser, sizeof(*alias));
    alias->name = name;
    alias->module = program->module;
    alias->target = target;
    alias->span = span;
    alias->public = public;
    alias->next = program->aliases;
    program->aliases = alias;
    ++program->alias_count;
}

static void parse_nominal_type(Parser *parser, bool structure, bool public) {
    const char *n = parse_identifier(parser);
    if (fe_named_type(parser->program, n) || !strcmp(n, "Result") || !strcmp(n, "Buffer") ||
        !strcmp(n, "copy") || !strcmp(n, "move") || !strcmp(n, "widen") || !strcmp(n, "narrow")) {
        parse_error(parser, "duplicate or reserved type name");
    }
    for (FeFunction *function = parser->program->functions; function; function = function->next) {
        if (function->module == parser->program->module && !strcmp(function->name, n)) {
            parse_error(parser, "type name conflicts with a function");
        }
    }
    if (fe_named_generic(parser->program, n)) {
        parse_error(parser, "type name conflicts with a generic function");
    }
    FeType *t = fe_type(parser->program, structure ? TY_STRUCT : TY_ENUM, NULL, NULL, 0);
    if (!t) {
        longjmp(parser->stop, 1);
    }
    /* Do not make recursive owning types available while parsing their fields. */
    expect_token(parser, '{', "expected '{'");
    FeField **tail = &t->fields;
    unsigned count = 0;
    while (!consume_token(parser, '}')) {
        FeField *f = new_node(parser, sizeof(*f));
        if (structure && peek_token(parser)->kind == FE_T_NAME && peek_token(parser)->length == 7 &&
            !memcmp(peek_token(parser)->text, "private", 7) &&
            parser->program->tokens.items[parser->pos + 1].kind == FE_T_NAME) {
            ++parser->pos;
            f->private = true;
        }
        f->name = parse_identifier(parser);
        f->index = count++;
        for (FeField *old = t->fields; old; old = old->next) {
            if (!strcmp(old->name, f->name)) {
                parse_error(parser, "duplicate field or enum variant");
            }
        }
        if (structure) {
            expect_token(parser, ':', "expected ':'");
            f->type = parse_type(parser);
            if (f->type->kind == TY_VOID || f->type->kind == TY_READ || f->type->kind == TY_EDIT ||
                f->type->kind == TY_RESULT || fe_is_owner_borrow(f->type)) {
                parse_error(parser, "struct fields require owning value types");
            }
            if (f->type->nesting >= 32) {
                parse_error(parser, "struct field type nesting exceeds 32 levels");
            }
            expect_token(parser, ';', "field requires ';'");
        } else if (consume_token(parser, '(')) {
            f->type = parse_type(parser);
            if (f->type->kind == TY_VOID || f->type->kind == TY_READ || f->type->kind == TY_EDIT ||
                f->type->kind == TY_RESULT || fe_is_owner_borrow(f->type)) {
                parse_error(parser, "enum payload requires an owning non-Result value type");
            }
            if (f->type->nesting >= 32) {
                parse_error(parser, "enum payload type nesting exceeds 32 levels");
            }
            expect_token(parser, ')', "enum variant requires one payload type");
            t->payload = true;
        }
        if (f->type) {
            t->resource_effects |= fe_resource_effects(f->type);
            uint64_t cells = fe_type_cells(f->type);
            if (structure) {
                t->cells = count == 1 ? cells : t->cells + cells;
            } else if (t->payload && cells + 1 > t->cells) {
                t->cells = cells + 1;
            }
        }
        if (f->type && f->type->nesting + 1 > t->nesting) {
            t->nesting = f->type->nesting + 1;
        }
        *tail = f;
        tail = &f->next;
        if (!structure && !consume_token(parser, ',') && peek_token(parser)->kind != '}') {
            parse_error(parser, "enum variants require ','");
        }
        if (count > 128) {
            parse_error(parser, "too many fields or variants");
        }
    }
    if (!count) {
        parse_error(parser, "empty nominal types are unsupported");
    }
    t->name = n;
    t->public = public;
    if (fe_type_cells(t) > 65536) {
        parse_error(parser, "aggregate exceeds 65536 cells");
    }
}

static void parse_function_tail(Parser *parser, FeFunction *function) {
    expect_token(parser, '(', "expected '('");
    FeSymbol **tail = &function->params;
    if (!consume_token(parser, ')')) {
        do {
            FeSymbol *parameter = new_node(parser, sizeof(*parameter));
            parameter->name = parse_identifier(parser);
            parameter->parameter = true;
            expect_token(parser, ':', "parameter requires type");
            parameter->type = parse_type(parser);
            parameter->id = function->param_count++;
            if (parameter->type->kind == TY_VOID || function->param_count > FE_PARAM_LIMIT) {
                parse_error(parser, "invalid parameter type or count");
            }
            *tail = parameter;
            tail = &parameter->next;
        } while (consume_token(parser, ','));
        expect_token(parser, ')', "expected ')'");
    }
    expect_token(parser, FE_T_ARROW, "function requires explicit result type");
    function->result = parse_type(parser);
    if (function->result->kind == TY_READ || function->result->kind == TY_EDIT ||
        fe_is_owner_borrow(function->result)) {
        parse_error(parser, "views cannot escape through return");
    }
    if (function->native) {
        expect_token(parser, ';', "expected ';' after native declaration");
    } else {
        function->body = parse_block(parser);
    }
}

bool fe_parse_module(FeContext *c, FeTokens tokens, FeProgram *program, FeModule *module,
                     size_t start) {
    program->ctx = c;
    program->tokens = tokens;
    program->module = module;
    Parser parser = {.program = program, .pos = start};
    if (setjmp(parser.stop)) {
        return false;
    }
    if (!program->types) {
        const char *names[] = {
            [TY_VOID] = "void", [TY_BOOL] = "bool", [TY_U8] = "u8",      [TY_I8] = "i8",
            [TY_U16] = "u16",   [TY_I16] = "i16",   [TY_I32] = "i32",    [TY_U32] = "u32",
            [TY_I64] = "i64",   [TY_U64] = "u64",   [TY_USIZE] = "usize"};
        for (unsigned i = 0; i <= TY_USIZE; ++i) {
            program->primitive[i] = fe_type(program, (FeTypeKind)i, NULL, NULL, 0);
            if (!program->primitive[i]) {
                longjmp(parser.stop, 1);
            }
            program->primitive[i]->name = names[i];
        }
        FeType *text = fe_type(program, TY_STR, NULL, NULL, 0);
        if (!text) {
            longjmp(parser.stop, 1);
        }
        text->name = "str";
        program->cast_error = fe_type(program, TY_ENUM, NULL, NULL, 0);
        if (!program->cast_error) {
            longjmp(parser.stop, 1);
        }
        program->cast_error->name = "CastError";
        program->cast_error->module = NULL;
        FeField *bad = new_node(&parser, sizeof(*bad));
        bad->name = "out_of_range";
        program->cast_error->fields = bad;
    }
    FeFunction **tail = &program->functions;
    while (*tail) {
        tail = &(*tail)->next;
    }
    while (peek_token(&parser)->kind != FE_T_EOF) {
        bool public = consume_token(&parser, FE_T_PUB);
        if (public && peek_token(&parser)->kind == '@') {
            parse_error(&parser, "attributes precede pub in a function declaration");
        }
        if (consume_token(&parser, FE_T_TYPE)) {
            parse_type_alias(&parser, public);
            continue;
        }
        if (consume_token(&parser, FE_T_ENUM)) {
            parse_nominal_type(&parser, false, public);
            continue;
        }
        if (consume_token(&parser, FE_T_STRUCT)) {
            parse_nominal_type(&parser, true, public);
            continue;
        }
        FeFunction *f = new_node(&parser, sizeof(*f));
        f->module = module;
        f->public = public;
        f->span = peek_token(&parser)->span;
        while (consume_token(&parser, '@')) {
            const char *attribute = parse_identifier(&parser);
            if (!strcmp(attribute, "budget")) {
                parse_budget(&parser, f);
            } else if (!strcmp(attribute, "effects")) {
                parse_effect_contract(&parser, f);
            } else {
                parse_error(&parser, "unsupported function attribute");
            }
        }
        f->public = public || consume_token(&parser, FE_T_PUB);
        f->exported = consume_token(&parser, FE_T_EXPORT);
        f->native = consume_token(&parser, FE_T_NATIVE);
        if (f->native && (!module || !module->standard)) {
            parse_error(&parser, "native declarations are restricted to the standard library");
        }
        if (f->native && consume_token(&parser, FE_T_STRUCT)) {
            const char *name = parse_identifier(&parser);
            if (strcmp(module->name, "std.fs") || strcmp(name, "File") || !f->public ||
                f->exported || f->budgeted || f->effects_declared || fe_named_type(program, name)) {
                parse_error(&parser, "unsupported native resource declaration");
            }
            FeType *type = fe_type(program, TY_FILE, NULL, NULL, 0);
            if (!type) {
                longjmp(parser.stop, 1);
            }
            type->name = name;
            type->public = true;
            expect_token(&parser, ';', "native resource declaration requires ';'");
            continue;
        }
        expect_token(&parser, FE_T_FUNC, "expected func");
        f->name = parse_identifier(&parser);
        for (FeFunction *old = program->functions; old; old = old->next) {
            if (old->module == module && !strcmp(old->name, f->name)) {
                parse_error(&parser, "duplicate function");
            }
        }
        for (FeGeneric *old = program->generics; old; old = old->next) {
            if (old->declaration->module == module && !strcmp(old->declaration->name, f->name)) {
                parse_error(&parser, "duplicate function");
            }
        }
        if (fe_named_type(program, f->name) || !strcmp(f->name, "copy") ||
            !strcmp(f->name, "move") || !strcmp(f->name, "widen") || !strcmp(f->name, "narrow") ||
            !strcmp(f->name, "Result") || !strcmp(f->name, "Buffer")) {
            parse_error(&parser, "reserved function name");
        }
        if (consume_token(&parser, '<')) {
            bool buffer_native =
                f->native && module && module->standard && !strcmp(module->name, "std.buffer");
            if ((f->native && !buffer_native) || f->exported || !strcmp(f->name, "main") ||
                fe_isTestName(f->name)) {
                parse_error(&parser, "generic functions cannot be native, exported, main or tests");
            }
            const char *parameter = parse_identifier(&parser);
            if (fe_named_type(program, parameter) || !strcmp(parameter, "Buffer") ||
                !strcmp(parameter, "Result")) {
                parse_error(&parser, "generic parameter conflicts with an existing type");
            }
            expect_token(&parser, ':', "generic parameter requires a constraint");
            const char *constraint = parse_identifier(&parser);
            bool data = !strcmp(constraint, "data");
            bool signed_only = !strcmp(constraint, "signed_integer");
            bool unsigned_only = !strcmp(constraint, "unsigned_integer");
            if (!data && !signed_only && !unsigned_only && strcmp(constraint, "integer")) {
                parse_error(
                    &parser,
                    "generic constraint must be data, integer, signed_integer or unsigned_integer");
            }
            close_type(&parser);
            size_t signature_start = parser.pos;
            FeFunction declaration = *f;
            parser.type_parameter = parameter;
            if (!f->native || data) {
                if (program->generic_count == FE_FUNCTION_LIMIT) {
                    parse_error(&parser, "too many generic declarations");
                }
                FeGeneric *generic = new_node(&parser, sizeof(*generic));
                generic->data = data;
                generic->declaration = f;
                generic->tokens = tokens;
                generic->context = c;
                generic->parameter = parameter;
                generic->signature_start = signature_start;
                if (data) {
                    if (!program->data_probe) {
                        FeType *probe = fe_type(program, TY_STRUCT, NULL, NULL, 0);
                        if (!probe) {
                            longjmp(parser.stop, 1);
                        }
                        FeField *field = new_node(&parser, sizeof(*field));
                        field->name = "probe";
                        field->type = program->primitive[TY_U8];
                        probe->fields = field;
                        probe->cells = 1;
                        probe->nesting = 1;
                        program->data_probe = probe;
                    }
                    parser.type_argument = program->data_probe;
                }
                for (unsigned kind = TY_U8; kind <= TY_USIZE; ++kind) {
                    bool signed_type = fe_is_signed(program->primitive[kind]);
                    if ((!signed_only || signed_type) && (!unsigned_only || !signed_type)) {
                        generic->arguments |= 1u << kind;
                        if (!parser.type_argument) {
                            parser.type_argument = program->primitive[kind];
                        }
                    }
                }
                /* Parse once for syntax; calls receive independent concrete trees. */
                parse_function_tail(&parser, f);
                f->params = NULL;
                f->body = NULL;
                f->param_count = 0;
                generic->next = program->generics;
                program->generics = generic;
                ++program->generic_count;
                parser.type_parameter = NULL;
                parser.type_argument = NULL;
                continue;
            }
            bool first_instance = true;
            for (unsigned kind = TY_U8; kind <= TY_USIZE; ++kind) {
                bool signed_type = fe_is_signed(program->primitive[kind]);
                if ((signed_only && !signed_type) || (unsigned_only && signed_type)) {
                    continue;
                }
                FeFunction *instance = first_instance ? f : new_node(&parser, sizeof(*instance));
                first_instance = false;
                *instance = declaration;
                instance->type_argument = program->primitive[kind];
                parser.type_argument = instance->type_argument;
                parser.pos = signature_start;
                parse_function_tail(&parser, instance);
                instance->id = program->function_count++;
                if (program->function_count > FE_FUNCTION_LIMIT) {
                    parse_error(&parser, "too many functions, including generic specializations");
                }
                *tail = instance;
                tail = &instance->next;
            }
            parser.type_parameter = NULL;
            parser.type_argument = NULL;
            continue;
        }
        parse_function_tail(&parser, f);
        f->id = program->function_count++;
        if (program->function_count > FE_FUNCTION_LIMIT) {
            parse_error(&parser, "too many functions");
        }
        *tail = f;
        tail = &f->next;
    }
    if (!module && !program->functions && !program->generics) {
        parse_error(&parser, "source must contain a function");
    }
    return !c->diagnostic.code;
}

bool fe_parse(FeContext *context, FeTokens tokens, FeProgram *program) {
    memset(program, 0, sizeof(*program));
    return fe_parse_module(context, tokens, program, NULL, 0);
}

FeFunction *fe_instantiate(FeProgram *program, FeGeneric *generic, FeType *argument) {
    FeContext *context = program->ctx;
    FeModule *module = program->module;
    FeTokens tokens = program->tokens;
    program->ctx = generic->context;
    program->module = generic->declaration->module;
    program->tokens = generic->tokens;
    Parser parser = {.program = program,
                     .pos = generic->signature_start,
                     .type_parameter = generic->parameter,
                     .type_argument = argument};
    if (setjmp(parser.stop)) {
        /* Keep the failing source context available to diagnostic reporting. */
        program->tokens = tokens;
        return NULL;
    }
    FeFunction *instance = new_node(&parser, sizeof(*instance));
    *instance = *generic->declaration;
    instance->type_argument = argument;
    parse_function_tail(&parser, instance);
    instance->id = program->function_count++;
    FeFunction **tail = &program->functions;
    while (*tail) {
        tail = &(*tail)->next;
    }
    *tail = instance;
    program->ctx = context;
    program->module = module;
    program->tokens = tokens;
    return instance;
}
