#include "fermi/fe.h"
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void *trackedAlloc(size_t size);
static void trackedFree(void *pointer);
#define malloc trackedAlloc
#define free trackedFree
#include <parserOracle.c>
#undef malloc
#undef free

static size_t attempts, releases, failures, failAt;
static void *live[8];
static uint64_t actual[65536], expected[65536];
static size_t written, cases;
static const char *label;

static void *trackedAlloc(size_t size) {
    ++attempts;
    if (attempts == failAt) {
        ++failures;
        return NULL;
    }
    void *pointer = malloc(size);
    assert(pointer);
    for (size_t slot = 0; slot < 8; ++slot) {
        if (!live[slot]) {
            live[slot] = pointer;
            return pointer;
        }
    }
    abort();
}

static void trackedFree(void *pointer) {
    for (size_t slot = 0; slot < 8; ++slot) {
        if (pointer && live[slot] == pointer) {
            live[slot] = NULL;
            ++releases;
            free(pointer);
            return;
        }
    }
    abort();
}

static void released(void) {
    for (size_t slot = 0; slot < 8; ++slot) {
        assert(!live[slot]);
    }
    assert(attempts == releases + failures);
}

static void require(bool valid, const char *message) {
    if (!valid) {
        fprintf(stderr, "bootstrap parser: case %zu: %s\n%s\n", cases, message, label);
        abort();
    }
}

static void serialize(const FeExpr *expression) {
    assert(written + 4 <= sizeof(expected) / sizeof(expected[0]));
    if (!expression) {
        expected[written++] = 0;
        return;
    }
    unsigned tag = 0;
    switch (expression->kind) {
    case EX_INT:
        tag = 1;
        break;
    case EX_BOOL:
        tag = 2;
        break;
    case EX_NAME:
        tag = 3;
        break;
    case EX_UNARY:
        tag = 4;
        break;
    case EX_BINARY:
        tag = 5;
        break;
    case EX_CALL:
        tag = 6;
        break;
    default:
        abort();
    }
    expected[written++] = tag;
    expected[written++] = expression->span.offset;
    expected[written++] = (uint64_t)expression->op;
    expected[written++] = expression->integer;
    serialize(expression->a);
    serialize(expression->b);
    serialize(expression->c);
    serialize(expression->next);
}

static void compare(const char *source) {
    label = source;
    FeContext context;
    FeTokens tokens = {0};
    FeProgram program = {0};
    assert(fe_context_init(&context, "expression.fe", source, strlen(source)));
    assert(fe_lex(&context, &tokens));
    bool valid = fe_parse(&context, tokens, &program);
    fermi_expressionTree_result result = fermi_expressionTree(
        (const uint8_t *)source, strlen(source), actual, sizeof(actual) / sizeof(actual[0]));
    require(result.status == (valid ? 0u : 1u), "acceptance differs or Fermi runtime fault");
    if (valid) {
        assert(program.functions && program.functions->body->body->kind == ST_RETURN);
        written = 0;
        serialize(program.functions->body->body->a);
        require(result.value == written, "serialized tree size differs");
        for (size_t index = 0; index < written; ++index) {
            if (actual[index] != expected[index]) {
                fprintf(stderr, "field %zu: got %llu, expected %llu\n", index,
                        (unsigned long long)actual[index], (unsigned long long)expected[index]);
                require(false, "expression tree differs");
            }
        }
    } else {
        require(actual[0] == context.diagnostic.span.offset, "error offset differs");
        require(actual[1] == context.diagnostic.span.line, "error line differs");
        require(actual[2] == context.diagnostic.span.column, "error column differs");
    }
    fe_context_destroy(&context);
    released();
    ++cases;
}

static void expression(const char *text) {
    char source[16384];
    int length = snprintf(source, sizeof(source),
                          "func f(a: i64, b: i64, c: i64) -> i64 { return %s; }", text);
    assert(length >= 0 && (size_t)length < sizeof(source));
    compare(source);
}

static uint32_t nextRandom(uint32_t *state) {
    *state = *state * UINT32_C(1664525) + UINT32_C(1013904223);
    return *state;
}

static size_t generate(char *output, size_t capacity, unsigned depth, uint32_t *state) {
    assert(capacity >= 4);
    if (!depth || nextRandom(state) % 4 == 0) {
        const char *leaves[] = {"a", "b", "c", "0", "42", "true", "false", "18446744073709551615"};
        const char *leaf = leaves[nextRandom(state) % 8];
        size_t length = strlen(leaf);
        assert(length < capacity);
        memcpy(output, leaf, length + 1);
        return length;
    }
    output[0] = '(';
    size_t length = 1 + generate(output + 1, capacity - 1, depth - 1, state);
    const char *operators[] = {" + ", " * ", " << ", " == ", " && ", " | ", " - ", " % "};
    const char *op = operators[nextRandom(state) % 8];
    size_t size = strlen(op);
    assert(length + size + 3 < capacity);
    memcpy(output + length, op, size);
    length += size;
    length += generate(output + length, capacity - length, depth - 1, state);
    assert(length + 2 < capacity);
    output[length++] = ')';
    output[length] = 0;
    return length;
}

static void precedenceCases(void) {
    const char *operators[] = {"||", "&&", "|",  "^",  "&", "==", "!=", "<", ">",
                               "<=", ">=", "<<", ">>", "+", "-",  "*",  "/", "%"};
    const char *prefixes[] = {"-", "+", "!", "~"};
    char source[8192];
    for (size_t left = 0; left < 18; ++left) {
        for (size_t right = 0; right < 18; ++right) {
            snprintf(source, sizeof(source), "a %s b %s c", operators[left], operators[right]);
            expression(source);
        }
        for (size_t prefix = 0; prefix < 4; ++prefix) {
            snprintf(source, sizeof(source), "%sa %s b", prefixes[prefix], operators[left]);
            expression(source);
        }
    }
    uint32_t state = 17;
    for (size_t index = 0; index < 1024; ++index) {
        generate(source, sizeof(source), 6, &state);
        expression(source);
    }
    expression("a(b, c + 1)(42)");
    expression("((a))(b)");
    expression("-a(b) + c");
    expression("-~+a + b");
    expression("a - b - c");
    expression("a()");
    expression("a<b>(c)");
    expression("a +");
    expression("(a + b");
    expression("a(b,)");
    expression("a(,b)");
    expression("a b");
    for (size_t count = 64; count <= 65; ++count) {
        size_t length = 0;
        source[length++] = 'a';
        for (size_t index = 0; index < count; ++index) {
            memcpy(source + length, "+a", 2);
            length += 2;
        }
        source[length] = 0;
        expression(source);
    }
    for (size_t count = 124; count <= 129; ++count) {
        memset(source, '(', count);
        source[count] = 'a';
        memset(source + count + 1, ')', count);
        source[count * 2 + 1] = 0;
        expression(source);
    }
    compare("func f() -> i64 { return; }");
    compare("func f() i64 { return 0; }");
    compare("func f(a i64) -> i64 { return 0; }");
    compare("func f() -> i64 { return 0 }");
    compare("func f() -> i64 { return 0;");
    compare("func f() -> i64 { return 0; } trailing");
}

static void serializeStmt(const FeStmt *statement) {
    assert(written + 4 < sizeof(expected) / sizeof(expected[0]));
    if (!statement) {
        expected[written++] = 0;
        return;
    }
    unsigned tag = 0;
    switch (statement->kind) {
    case ST_BLOCK:
        tag = 7;
        break;
    case ST_DECL:
        tag = 8;
        break;
    case ST_RETURN:
        tag = 9;
        break;
    case ST_EXPR:
        tag = 10;
        break;
    case ST_ASSIGN:
        tag = 11;
        break;
    case ST_IF:
        tag = 12;
        break;
    case ST_WHILE:
        tag = 13;
        break;
    case ST_FOR:
        tag = 14;
        break;
    case ST_BREAK:
        tag = 15;
        break;
    case ST_CONTINUE:
        tag = 16;
        break;
    default:
        abort();
    }
    expected[written++] = tag;
    expected[written++] = 0;
    unsigned op = (unsigned)statement->op;
    if (statement->kind == ST_DECL) {
        op = statement->mutable ? FE_T_VAR : FE_T_LET;
    }
    expected[written++] = op;
    expected[written++] = 0;
    if (statement->kind == ST_BLOCK) {
        serializeStmt(statement->body);
    } else {
        serialize(statement->kind == ST_DECL ? NULL : statement->a);
    }
    serialize(statement->kind == ST_DECL ? statement->a : statement->b);
    serializeStmt(statement->kind == ST_BLOCK ? NULL : statement->body);
    serializeStmt(statement->next);
    serializeStmt(statement->other);
    serializeStmt(statement->init);
    serializeStmt(statement->step);
}

static void compareControl(const char *source) {
    label = source;
    FeContext context;
    FeTokens tokens = {0};
    FeProgram program = {0};
    assert(fe_context_init(&context, "control.fe", source, strlen(source)));
    assert(fe_lex(&context, &tokens));
    bool valid = fe_parse(&context, tokens, &program);
    fermi_statementTree_result result = fermi_statementTree(
        (const uint8_t *)source, strlen(source), actual, sizeof(actual) / sizeof(actual[0]));
    require(result.status == (valid ? 0u : 1u), "statement acceptance differs or runtime fault");
    if (valid) {
        written = 0;
        serializeStmt(program.functions->body);
        require(result.value == written, "statement tree length differs");
        for (size_t index = 0; index < written; ++index) {
            if (actual[index] != expected[index]) {
                fprintf(stderr, "statement field %zu: got %llu, expected %llu\n", index,
                        (unsigned long long)actual[index], (unsigned long long)expected[index]);
                require(false, "statement tree differs");
            }
        }
    } else {
        require(actual[0] == context.diagnostic.span.offset, "statement error offset differs");
        require(actual[1] == context.diagnostic.span.line, "statement error line differs");
        require(actual[2] == context.diagnostic.span.column, "statement error column differs");
    }
    fe_context_destroy(&context);
    released();
    ++cases;
}

static void controlCases(void) {
    const char *bodies[] = {
        "if (true) {}",
        "if (a) { a = 1; } else if (b) { return 2; } else { return 3; }",
        "while (a < 5) { a += 1; if (a == 2) { continue; } break; }",
        "for (var i: i32 = 0; i < 10; i += 1) { while (b) { break; } continue; }",
        "for (i = 0; i < 10; update(i)) {}",
        "for (let i: i32 = 0; true; i *= 2) {}",
        "break; continue;",
        "if true {}",
        "if (true) return 1;",
        "if (true) {} else return 1;",
        "if (true) {} else if (false) {} else",
        "while true {}",
        "while (true {}",
        "while (true) { break }",
        "continue 1;",
        "for (;;) {}",
        "for (var i: i32 = 0; ; i += 1) {}",
        "for (var i: i32 = 0; i < 3; var j: i32 = 1) {}",
        "for (var i: i32 = 0; i < 3; i += 1 {}",
        "else {}",
    };
    char source[16384];
    for (size_t index = 0; index < sizeof(bodies) / sizeof(bodies[0]); ++index) {
        snprintf(source, sizeof(source), "func f() -> i32 { %s }", bodies[index]);
        compareControl(source);
    }
    // Exercise both arms, sibling links and loop headers at increasing depths.
    for (size_t depth = 0; depth < 64; ++depth) {
        strcpy(source, "func f() -> i32 {");
        for (size_t level = 0; level < depth; ++level) {
            const char *prefix[] = {"if (a) {", "while (b) {",
                                    "for (var i: i32 = 0; i < 3; i += 1) {"};
            strcat(source, prefix[level % 3]);
        }
        strcat(source, "a += 1; break; continue;");
        for (size_t level = depth; level > 0; --level) {
            if ((level - 1) % 3 == 0) {
                strcat(source, "} else if (c) {} else { return 0; }");
            } else {
                strcat(source, "}");
            }
        }
        strcat(source, "return a; }");
        compareControl(source);
    }
    puts("PASS: bootstrap control-flow trees, headers, else-if branches and nesting diagnostics");
}

static void allocationCases(void) {
    char source[8192] = "func f() -> void { for (var i: i32 = 0; i < 5; i += 1) { if (i == 2) { "
                        "continue; } else { break; } }";
    for (size_t index = 0; index < 512; ++index) {
        strcat(source, "var x: i32;");
    }
    strcat(source, "}");
    attempts = releases = failures = 0;
    failAt = 0;
    fermi_parseCount_result result = fermi_parseCount((const uint8_t *)source, strlen(source));
    assert(result.status == 0 && result.value > 1000);
    size_t allocations = attempts;
    released();
    for (size_t failure = 1; failure <= allocations; ++failure) {
        failAt = attempts + failure;
        result = fermi_parseCount((const uint8_t *)source, strlen(source));
        assert(result.status == 1 && result.code == 0);
        released();
    }
    failAt = 0;
    source[strlen(source) - 1] = '?';
    result = fermi_parseCount((const uint8_t *)source, strlen(source));
    assert(result.status == 1 && result.code == 3);
    released();
    source[strlen(source) - 1] = 0;
    result = fermi_parseCount((const uint8_t *)source, strlen(source));
    assert(result.status == 1 && result.code == 3);
    released();
    puts("PASS: bootstrap parser allocation failures, lexical/syntax errors and owner cleanup");
}

int main(void) {
    precedenceCases();
    controlCases();
    allocationCases();
    printf("PASS: bootstrap parser, %zu host-tree comparisons, precedence, associativity and "
           "diagnostics\n",
           cases);
    return 0;
}
