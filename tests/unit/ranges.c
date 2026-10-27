#include "fermi/fe.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

static void check(const char *source, unsigned proofs) {
    FeContext context;
    FeTokens tokens;
    FeProgram program;
    bool ok = fe_context_init(&context, "ranges.fe", source, strlen(source)) &&
              fe_lex(&context, &tokens) && fe_parse(&context, tokens, &program) &&
              fe_check(&program) && fe_analyze(&program);
    if (!ok) {
        fe_print_diagnostic(&context);
    }
    assert(ok);
    assert(program.functions->proven_overflows == proofs);
    fe_context_destroy(&context);
}

static void reduction(const char *guard, const char *initial, const char *body, unsigned proofs) {
    char source[4096];
    int length = snprintf(source, sizeof(source),
                          "func sum(a: read i16[], other: read i16[], flag: bool) -> i64 {"
                          "%s var total: i64 = %s;"
                          "for (var i: usize = 0; i < a.len; i += 1) { %s } return total; }",
                          guard, initial, body);
    assert(length > 0 && (size_t)length < sizeof(source));
    check(source, proofs);
}

int main(void) {
    const char *add = "total += widen<i64>(a[i]);";
    const char *guard = "if (a.len > 4096) { return 0; }";
    reduction(guard, "0", add, 1);
    reduction("if (a.len > 0) { return 0; }", "0", add, 1);
    reduction("if (a.len > 281474976710655) { return 0; }", "0", add, 1);
    reduction("if (a.len > 281474976710656) { return 0; }", "0", add, 0);
    reduction("", "0", add, 0);
    reduction("if (other.len > 4096) { return 0; }", "0", add, 0);
    reduction("if (flag) { if (a.len > 4096) { return 0; } }", "0", add, 0);
    reduction("if (a.len > 4096) {}", "0", add, 0);
    reduction(guard, "9223372036854775807", add, 0);
    reduction(guard, "0", "total += widen<i64>(a[i]); total += 1;", 0);
    reduction(guard, "0", "total += widen<i64>(a[i]); i = 0;", 0);
    reduction(guard, "0", "total += widen<i64>(other[i]);", 0);
    reduction(guard, "0", "total += widen<i64>(a[0]);", 0);
    reduction(guard, "0", "if (flag) { total += widen<i64>(a[i]); }", 0);
    reduction(guard, "0", "for (var j: usize = 0; j < 2; j += 1) { total += widen<i64>(a[i]); }",
              0);
    check("func sum(a: read i64[]) -> i64 { if (a.len > 4) { return 0; }"
          "var total: i64 = 0; for (var i: usize = 0; i < a.len; i += 1) { total += a[i]; }"
          "return total; }",
          0);
    check("func sum() -> i64 { let a: i16[2] = [-32768,32767]; var total: i64 = 0;"
          "for (var i: usize = 0; i < a.len; i += 1) { total += widen<i64>(a[i]); }"
          "return total; }",
          1);
    check("func sum(a: read i16[]) -> i64 { if (a.len > 4) { return 0; }"
          "var total: i64 = 0; total = 9223372036854775807;"
          "for (var i: usize = 0; i < a.len; i += 1) { total += widen<i64>(a[i]); }"
          "return total; }",
          0);
    check("func sum(a: read i16[]) -> i64 { if (a.len > 4) { return 0; }"
          "var total: i64 = 0; var limit: usize = a.len;"
          "for (var i: usize = 0; i < limit; i += 1) { total += widen<i64>(a[i]); limit += 1; }"
          "return total; }",
          0);
    puts("PASS: bounded sum proofs, numeric limits, rejected mutations and control flow");
    return 0;
}
