#include <stdio.h>
#if defined(__has_include)
#if __has_include("fermi/lex.h")
#define FRONTEND_PRESENT 1
#endif
#endif
#ifndef FRONTEND_PRESENT
int main(void) {
    fputs("FAIL: lexical frontend is not implemented\n", stderr);
    return 1;
}
#else
#include "fermi/lex.h"
#include <assert.h>
#include <string.h>

static void test_tokens_and_positions(void) {
    const char *text = "// test\nfunc f(x: i64) -> i64 { return x + 12_000; }";
    FeContext c;
    FeTokens ts = {0};
    assert(fe_context_init(&c, "tokens.fe", text, strlen(text)));
    assert(fe_lex(&c, &ts));
    assert(ts.items[0].kind == FE_T_FUNC);
    assert(ts.items[0].span.line == 2 && ts.items[0].span.column == 1);
    assert(ts.items[7].kind == FE_T_ARROW);
    assert(ts.items[13].kind == FE_T_INTEGER && ts.items[13].integer == 12000);
    assert(ts.items[ts.count - 1].kind == FE_T_EOF);
    fe_context_destroy(&c);
}

static void test_bit_tokens(void) {
    const char *text = "& && | || ^ ~ << >> <= >= < >";
    const int expected[] = {'&',      FE_T_AND, '|',     FE_T_OR, '^', '~',     FE_T_SHL,
                            FE_T_SHR, FE_T_LE,  FE_T_GE, '<',     '>', FE_T_EOF};
    FeContext c;
    FeTokens ts = {0};
    assert(fe_context_init(&c, "bits.fe", text, strlen(text)));
    assert(fe_lex(&c, &ts));
    assert(ts.count == sizeof(expected) / sizeof(expected[0]));
    for (size_t i = 0; i < ts.count; ++i) {
        assert(ts.items[i].kind == expected[i]);
    }
    fe_context_destroy(&c);
}

static void test_string_escape(void) {
    const char *text = "\"hello\\n\\\"\\\\\"";
    FeContext c;
    FeTokens ts = {0};
    assert(fe_context_init(&c, "string.fe", text, strlen(text)));
    assert(fe_lex(&c, &ts));
    assert(ts.items[0].kind == FE_T_STRING);
    assert(ts.items[0].length == 8);
    assert(memcmp(ts.items[0].text, "hello\n\"\\", 8) == 0);
    fe_context_destroy(&c);
}

static void test_invalid_sources(void) {
    const char *cases[] = {"\"unterminated", "1__0", "18446744073709551616", "$", "\"\\q\""};
    for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); ++i) {
        FeContext c;
        FeTokens ts = {0};
        assert(fe_context_init(&c, "bad.fe", cases[i], strlen(cases[i])));
        assert(!fe_lex(&c, &ts));
        assert(c.diagnostic.code != NULL);
        fe_context_destroy(&c);
    }
    const char embedded[] = {'a', '\0', 'b'};
    FeContext c;
    FeTokens ts = {0};
    assert(fe_context_init(&c, "nul.fe", embedded, sizeof(embedded)));
    assert(!fe_lex(&c, &ts));
    fe_context_destroy(&c);
}

int main(void) {
    test_tokens_and_positions();
    test_string_escape();
    test_bit_tokens();
    test_invalid_sources();
    puts("PASS: lexical frontend");
    return 0;
}
#endif
