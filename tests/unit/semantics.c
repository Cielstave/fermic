#include "fermi/fe.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static void check(const char *source, const char *diagnostic) {
    FeContext c;
    FeTokens tokens = {0};
    FeProgram p;
    bool ok = fe_context_init(&c, "test.fe", source, strlen(source)) && fe_lex(&c, &tokens) &&
              fe_parse(&c, tokens, &p) && fe_check(&p) && fe_analyze(&p);
    if (diagnostic ? ok || !c.diagnostic.code || strcmp(c.diagnostic.code, diagnostic) : !ok) {
        fprintf(stderr, "source: %s\nexpected %s\n", source, diagnostic ? diagnostic : "success");
        if (c.diagnostic.code) {
            fe_print_diagnostic(&c);
        }
        assert(false);
    }
    fe_context_destroy(&c);
}
static void check_native_data_buffers(void) {
    static const struct {
        const char *declaration, *call, *diagnostic;
    } cases[] = {
        {"pub native func empty<T: data>(limit: usize) -> Result<Buffer<T>, Error>;",
         "let value: Buffer<Row> = try(empty<Row>(128));", NULL},
        {"pub native func element_size<T: data>() -> usize;",
         "let width: usize = element_size<Row>();", NULL},
        {"pub native func empty<T: data>(limit: u8) -> Result<Buffer<T>, Error>;",
         "let value: Buffer<Row> = try(empty<Row>(128));", "E_NATIVE"},
        {"pub native func empty<T: data>(limit: usize) -> Result<Buffer<u8>, Error>;",
         "let value: Buffer<u8> = try(empty<Row>(128));", "E_NATIVE"},
        {"pub native func element_size<T: data>() -> u8;", "let width: u8 = element_size<Row>();",
         "E_NATIVE"},
        {"pub native func empty<T: data>(limit: usize) -> Result<Buffer<T>, Error>; "
         "pub native func resize_filled<T: data>(value: edit Buffer<T>, count: usize, seed: T) -> "
         "Result<usize, Error>;",
         "var value: Buffer<Row> = try(empty<Row>(128)); let size: usize = "
         "try(resize_filled<Row>(edit value, 2, Row(id = 7)));",
         NULL},
        {"pub native func empty<T: data>(limit: usize) -> Result<Buffer<T>, Error>; "
         "pub native func resize_filled<T: data>(value: edit Buffer<T>, count: usize, seed: usize) "
         "-> Result<usize, Error>;",
         "var value: Buffer<Row> = try(empty<Row>(128)); let size: usize = "
         "try(resize_filled<Row>(edit value, 2, 7));",
         "E_NATIVE"},
        {"pub native func empty<T: data>(limit: usize) -> Result<Buffer<T>, Error>; "
         "pub native func resize<T: data>(value: edit Buffer<T>, count: usize) -> Result<usize, "
         "Error>;",
         "var value: Buffer<Row> = try(empty<Row>(128)); let size: usize = try(resize<Row>(edit "
         "value, 2));",
         "E_NATIVE"},
        {"pub native func empty<T: data>(limit: usize) -> Result<Buffer<T>, Error>; "
         "pub native func reserve<T: data>(value: edit Buffer<u8>, count: usize) -> Result<usize, "
         "Error>;",
         "var value: Buffer<u8> = try(empty<u8>(128)); let size: usize = try(reserve<Row>(edit "
         "value, 2));",
         "E_NATIVE"},
        {"pub native func empty<T: data>(limit: usize) -> Result<Buffer<T>, Error>; "
         "pub native func truncate<T: data>(value: edit Buffer<T>, count: usize) -> Result<usize, "
         "Error>;",
         "var value: Buffer<Row> = try(empty<Row>(128)); let size: usize = try(truncate<Row>(edit "
         "value, 0));",
         NULL},
        {"pub native func empty<T: data>(limit: usize) -> Result<Buffer<T>, Error>; "
         "pub native func truncate<T: data>(value: read T[], count: usize) -> Result<usize, "
         "Error>;",
         "var value: Buffer<Row> = try(empty<Row>(128)); let size: usize = try(truncate<Row>(read "
         "value, 0));",
         "E_NATIVE"},
        {"pub native func empty<T: data>(limit: usize) -> Result<Buffer<T>, Error>; "
         "pub native func truncate<T: data>(value: edit Buffer<T>, count: u8) -> Result<usize, "
         "Error>;",
         "var value: Buffer<Row> = try(empty<Row>(128)); let size: usize = try(truncate<Row>(edit "
         "value, 0));",
         "E_NATIVE"},
        {"pub native func fabricate<T: data>() -> T;", "let value: Row = fabricate<Row>();",
         "E_NATIVE"},
    };
    for (size_t index = 0; index < sizeof(cases) / sizeof(cases[0]); ++index) {
        char source[2048];
        int length = snprintf(
            source, sizeof(source),
            "pub enum Error { no_memory, size_overflow, limit, invalid_argument, invalid_state, } "
            "struct Row { id: usize; } %s func f() -> Result<void, Error> { %s return Result.ok(); "
            "}",
            cases[index].declaration, cases[index].call);
        assert(length > 0 && (size_t)length < sizeof(source));
        FeContext context;
        FeTokens tokens = {0};
        FeProgram program = {0};
        FeModule module = {.name = "std.buffer", .standard = true, .ctx = &context};
        bool ok = fe_context_init(&context, "buffer.fe", source, (size_t)length) &&
                  fe_lex(&context, &tokens) &&
                  fe_parse_module(&context, tokens, &program, &module, 0) && fe_check(&program) &&
                  fe_analyze(&program);
        if (cases[index].diagnostic ? ok || !context.diagnostic.code ||
                                          strcmp(context.diagnostic.code, cases[index].diagnostic)
                                    : !ok) {
            if (context.diagnostic.code) {
                fe_print_diagnostic(&context);
            }
            assert(false);
        }
        fe_context_destroy(&context);
    }
}

static void check_native_integer_buffers(void) {
    static const struct {
        const char *declaration;
        const char *diagnostic;
        const char *module_name;
    } cases[] = {
        {"pub native func allocate<T: integer>(length: usize, limit: usize) -> Result<Buffer<T>, "
         "Error>;",
         NULL, "std.buffer"},
        {"pub native func reserve<T: integer>(value: edit Buffer<T>, capacity: usize) -> "
         "Result<usize, Error>;",
         NULL, "std.buffer"},
        {"pub native func resize<T: integer>(value: edit Buffer<T>, length: usize) -> "
         "Result<usize, Error>;",
         NULL, "std.buffer"},
        {"pub native func allocate<T: integer>(length: usize, limit: usize) -> Result<Buffer<u8>, "
         "Error>;",
         "E_NATIVE", "std.buffer"},
        {"pub native func reserve<T: integer>(value: edit Buffer<u8>, capacity: usize) -> "
         "Result<usize, Error>;",
         "E_NATIVE", "std.buffer"},
        {"pub native func allocate<T: integer>(length: usize, limit: usize) -> Result<usize, "
         "Error>;",
         "E_NATIVE", "std.buffer"},
        {"pub native func allocate(length: usize, limit: usize) -> Result<Buffer<i32>, Error>;",
         "E_NATIVE", "std.buffer"},
        {"pub native func other<T: integer>(length: usize, limit: usize) -> Result<Buffer<T>, "
         "Error>;",
         "E_NATIVE", "std.buffer"},
        {"pub native func reserve(value: edit Buffer<i32>, capacity: usize) -> Result<usize, "
         "Error>;",
         "E_NATIVE", "std.mem"},
        {"pub native func bytes(length: usize, limit: usize) -> Result<Buffer<i32>, Error>;",
         "E_NATIVE", "std.mem"},
    };
    for (size_t index = 0; index < sizeof(cases) / sizeof(cases[0]); index++) {
        char source[1024];
        int length = snprintf(source, sizeof(source),
                              "pub enum Error { no_memory, size_overflow, limit, invalid_argument, "
                              "invalid_state, } %s",
                              cases[index].declaration);
        assert(length > 0 && (size_t)length < sizeof(source));
        FeContext context;
        FeTokens tokens = {0};
        FeProgram program = {0};
        FeModule module = {.name = cases[index].module_name, .standard = true, .ctx = &context};
        bool ok = fe_context_init(&context, "buffer.fe", source, (size_t)length) &&
                  fe_lex(&context, &tokens) &&
                  fe_parse_module(&context, tokens, &program, &module, 0) && fe_check(&program) &&
                  fe_analyze(&program);
        const char *expected = cases[index].diagnostic;
        if (expected ? ok || !context.diagnostic.code || strcmp(context.diagnostic.code, expected)
                     : !ok) {
            fe_print_diagnostic(&context);
            assert(false);
        }
        fe_context_destroy(&context);
    }
}

static void check_array_repetition(void) {
    static const struct {
        const char *source;
        const char *diagnostic;
    } cases[] = {
        {"func f() -> void { let a: u8[3] = [7; 3]; }", NULL},
        {"func f() -> void { let a: u8[3] = [7; 2]; }", "E_TYPE"},
        {"func f() -> void { let a: u8[3] = [7; 0]; }", "E_PARSE"},
        {"func f() -> void { let a: u8[3] = [7; 65537]; }", "E_PARSE"},
        {"func f() -> void { let a: u8[3] = [7; 18446744073709551615]; }", "E_PARSE"},
        {"func f(n: usize) -> void { let a: u8[3] = [7; n]; }", "E_PARSE"},
        {"func f() -> void { let a: u8[3] = [7; 1 + 2]; }", "E_PARSE"},
        {"func f() -> void { let a: u8[3] = [7; -3]; }", "E_PARSE"},
        {"func f() -> void { let a: u8[3] = [7, 8; 3]; }", "E_PARSE"},
        {"func f() -> void { let a: u8[3] = [7; 3, 7]; }", "E_PARSE"},
        {"func f() -> void { let a: u8[3] = [true; 3]; }", "E_TYPE"},
        {"func f() -> void { let a: u8[3] = [256; 3]; }", "E_RANGE"},
        {"func f() -> void { let a: u8 = [7; 3]; }", "E_TYPE"},
        {"func f(a: u8) -> void {} func g() -> void { f(1; 3); }", "E_PARSE"},
        {"struct S { n: u8; } func f() -> void { let s: S = S(n = 1); let a: S[2] = [s; 2]; }",
         "E_MOVE"},
        {"struct S { n: u8; } func f() -> u8 { let s: S = S(n = 1); let a: S[2] = [move(s); 2]; "
         "return s.n; }",
         "E_INIT"},
        {"type Owner = Buffer<u8>; func f() -> void { let a: Owner[2] = [0; 2]; }", "E_PARSE"},
        {"type View = read u8[]; func f() -> void { let a: View[2] = [0; 2]; }", "E_PARSE"},
        {"enum E { bad, } type Outcome = Result<u8, E>; func f() -> void { let a: Outcome[2] = "
         "[Result.ok(1); 2]; }",
         "E_PARSE"},
        {"func f() -> void { var a: u8[2] = [read 1; 2]; }", "E_TYPE"},
        {"func f() -> void { var a: u8[2] = [value = 1; 2]; }", "E_TYPE"},
    };
    for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); ++i) {
        check(cases[i].source, cases[i].diagnostic);
    }
}

static void check_struct_borrows(void) {
    static const struct {
        const char *source;
        const char *diagnostic;
    } cases[] = {
        {"struct C { n: u32; } func f(c: read C) -> u32 { return c.n; }", NULL},
        {"struct C { n: u32; } func f(c: read C) -> void { c.n = 1; }", "E_MUT"},
        {"struct C { n: u32; } func f(c: edit C) -> void { c.n += 1; }", NULL},
        {"struct C { n: u32; } func f(c: edit C) -> void {} func g(c: read C) -> void { f(edit c); "
         "}",
         "E_MUT"},
        {"struct C { n: u32; } func f(c: edit C) -> void {} func g() -> void { let c: C = C(n = "
         "0); f(edit c); }",
         "E_MUT"},
        {"struct C { n: u32; } func f(c: read C) -> void {} func g() -> void { let c: C = C(n = "
         "0); f(c); }",
         "E_ACCESS"},
        {"struct C { n: u32; } func f(c: read C) -> void {} func g() -> void { f(read C(n = 0)); }",
         "E_ACCESS"},
        {"struct C { n: u32; } func f() -> read C {}", "E_PARSE"},
        {"struct C { n: u32; } func f(c: read C) -> void { let local: read C = c; }", "E_TYPE"},
        {"struct C { n: u32; } struct Bad { ref: read C; }", "E_PARSE"},
        {"struct C { n: u32; } type Ref = read C; type Bad = Ref[1];", "E_PARSE"},
        {"type View = read u8[]; type Bad = View[1];", "E_PARSE"},
        {"type View = edit u8[]; type Bad = View[1];", "E_PARSE"},
        {"type Ref = edit Buffer<u8>; type Bad = Ref[1];", "E_PARSE"},
        {"struct C { n: u32; } enum E { bad(read C), }", "E_PARSE"},
        {"struct C { n: u32; } func f(c: read C) -> void {} func g(c: read C) -> void { f(read c); "
         "}",
         NULL},
        {"struct C { n: u32; } func f(c: read C) -> void {} func g(c: edit C) -> void { f(read c); "
         "}",
         NULL},
        {"struct C { n: u32; } func pair(a: read C, b: read C) -> void {} func f(c: read C) -> "
         "void { pair(read c, read c); }",
         NULL},
        {"struct C { n: u32; } func pair(a: edit C, b: read C) -> void {} func f(c: edit C) -> "
         "void { pair(edit c, read c); }",
         "E_ALIAS"},
        {"struct C { n: u32; } func pair(a: edit C, b: edit C) -> void {} func f(c: edit C) -> "
         "void { pair(edit c, edit c); }",
         "E_ALIAS"},
        {"struct C { n: u32; } struct S { c: C; } func f(c: edit C) -> void {} func g(s: read S) "
         "-> void { f(edit s.c); }",
         "E_MUT"},
        {"struct C { bytes: u8[2]; } func f(c: read C) -> void { c.bytes[0] = 1; }", "E_MUT"},
        {"struct C { bytes: u8[2]; } func f(v: edit u8[]) -> void {} func g(c: read C) -> void { "
         "f(edit c.bytes); }",
         "E_MUT"},
        {"struct C { n: u32; } func change(c: edit C) -> u32 { c.n = 3; return 1; } func f(c: edit "
         "C) -> void { c.n = change(edit c); }",
         "E_ALIAS"},
        {"struct C { n: u32; } export func f(c: read C) -> u32 { return c.n; }", "E_ABI"},
        {"struct C { n: u32; } func f(c: read C) -> C { return move(c); }", "E_TYPE"},
        {"struct C { n: u32; } func f(c: read C) -> C { return copy(c); }", "E_TYPE"},
    };
    for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); ++i) {
        check(cases[i].source, cases[i].diagnostic);
    }
}

static void check_scalar_matches(void) {
    static const struct {
        const char *source;
        const char *diagnostic;
    } cases[] = {
        {"func f(value: u8) -> u8 { match (value) { case 0 { return 1; } case _ { return 2; } } }",
         NULL},
        {"func f(value: bool) -> u8 { match (value) { case true { return 1; } case false { return "
         "2; } } }",
         NULL},
        {"func f(value: bool) -> u8 { match (value) { case true { return 1; } case _ { return 2; } "
         "} }",
         NULL},
        {"func f(value: i64) -> u8 { match (value) { case -9223372036854775808 { return 1; } case "
         "_ { return 2; } } }",
         NULL},
        {"func f(value: u64) -> u8 { match (value) { case 18446744073709551615 { return 1; } case "
         "_ { return 2; } } }",
         NULL},
        {"func f(value: u8) -> u8 { match (value) { case _ { return 2; } } }", NULL},
        {"func f(value: bool) -> u8 { var result: u8; match (value) { case true { result = 1; } "
         "case false { result = 2; } } return result; }",
         NULL},
        {"func f(value: bool) -> u8 { var result: u8; match (value) { case true { result = 1; } "
         "case false {} } return result; }",
         "E_INIT"},
        {"func f(value: u8) -> void { match (value) { case 0 {} } }", "E_MATCH"},
        {"func f(value: bool) -> void { match (value) { case true {} } }", "E_MATCH"},
        {"func f(value: u8) -> void { match (value) {} }", "E_MATCH"},
        {"func f(value: u8) -> void { match (value) { case 1 {} case 1 {} case _ {} } }",
         "E_MATCH"},
        {"func f(value: i8) -> void { match (value) { case 0 {} case -0 {} case _ {} } }",
         "E_MATCH"},
        {"func f(value: bool) -> void { match (value) { case true {} case true {} case _ {} } }",
         "E_MATCH"},
        {"func f(value: bool) -> void { match (value) { case true {} case false {} case _ {} } }",
         "E_MATCH"},
        {"func f(value: u8) -> void { match (value) { case _ {} case 1 {} } }", "E_MATCH"},
        {"func f(value: u8) -> void { match (value) { case _ {} case _ {} } }", "E_MATCH"},
        {"func f(value: u8) -> void { match (value) { case 256 {} case _ {} } }", "E_RANGE"},
        {"func f(value: i8) -> void { match (value) { case -129 {} case _ {} } }", "E_RANGE"},
        {"func f(value: u8) -> void { match (value) { case -1 {} case _ {} } }", "E_TYPE"},
        {"func f(value: u8) -> void { match (value) { case true {} case _ {} } }", "E_TYPE"},
        {"func f(value: bool) -> void { match (value) { case 1 {} case _ {} } }", "E_TYPE"},
        {"enum State { idle, } func f(value: State) -> void { match (value) { case _ {} } }",
         "E_MATCH"},
        {"enum Error { invalid, } func f(value: Result<u8, Error>) -> void { match (value) { case "
         "0 {} case _ {} } }",
         "E_MATCH"},
        {"func f(value: u8) -> void { match (value) { case 1 + 2 {} case _ {} } }", "E_PARSE"},
        {"func f(value: u8) -> u8 { match (value) { case _ { return 1; } } return 2; }", "E_FLOW"},
        {"func f(flag: bool, buffer: Buffer<u8>) -> usize { match (flag) { case true { let taken: "
         "Buffer<u8> = move(buffer); } case false {} } return buffer.len; }",
         "E_INIT"},
        {"@effects() @budget(work = 100, heap = 0) func f(value: u8) -> u8 { match (value) { case "
         "0 { return 1; } case _ { return 2; } } }",
         NULL},
    };
    for (unsigned i = 0; i < sizeof(cases) / sizeof(cases[0]); ++i) {
        check(cases[i].source, cases[i].diagnostic);
    }
}

static void check_generic_instances(void) {
    const char *source = "func identity<T: integer>(value: T) -> T { return value; } "
                         "func f() -> u16 { return identity<u16>(1) + identity<u16>(2); }";
    FeContext context;
    FeTokens tokens = {0};
    FeProgram program;
    assert(fe_context_init(&context, "instances.fe", source, strlen(source)));
    assert(fe_lex(&context, &tokens) && fe_parse(&context, tokens, &program));
    assert(program.generic_count == 1 && program.function_count == 1);
    assert(fe_check(&program) && fe_analyze(&program));
    assert(program.function_count == 2);
    FeFunction *instance = fe_named_function(&program, "identity", program.primitive[TY_U16]);
    assert(instance != NULL && instance->id == 1);
    assert(fe_named_function(&program, "identity", program.primitive[TY_U8]) == NULL);
    fe_context_destroy(&context);
}

static void check_integer_generics(void) {
    static const struct {
        const char *source;
        const char *diagnostic;
    } cases[] = {
        {"func invalid<T: integer>(value: T) -> T { return true; }", NULL},
        {"func negate<T: signed_integer>(value: T) -> T { return -value; } "
         "func f() -> i16 { return negate<i16>(-7); }",
         NULL},
        {"func negate<T: signed_integer>(value: T) -> T { return -value; } "
         "func f() -> u16 { return negate<u16>(7); }",
         "E_GENERIC"},
        {"func flags<T: unsigned_integer>(value: T, mask: T) -> T { return value | mask; } "
         "func f() -> usize { return flags<usize>(1, 4); }",
         NULL},
        {"func flags<T: unsigned_integer>(value: T, mask: T) -> T { return value | mask; } "
         "func f() -> i32 { return flags<i32>(1, 4); }",
         "E_GENERIC"},
        {"func invalid<T: signed_integer>(value: T) -> T { return ~value; } "
         "func f() -> i8 { return invalid<i8>(1); }",
         "E_TYPE"},
        {"func invalid<T: unsigned_integer>(value: T) -> T { return -value; } "
         "func f() -> u8 { return invalid<u8>(1); }",
         "E_TYPE"},
        {"func negate<T: signed_integer>(value: T) -> T { return -value; } "
         "func invalid<T: integer>(value: T) -> T { return negate<T>(value); } "
         "func f() -> u8 { return invalid<u8>(1); }",
         "E_GENERIC"},
        {"@effects() func invalid<T: signed_integer>(buffer: Buffer<u8>, value: T) -> T "
         "{ return value; } func f(buffer: Buffer<u8>) -> i8 { return invalid<i8>(move(buffer), "
         "1); }",
         "E_EFFECT"},
        {"func identity<T: signed_integer>(value: T) -> T { return value; } "
         "func invalid() -> i8 { return identity<i8>(128); }",
         "E_RANGE"},
        {"func invalid<T: signed>(value: T) -> T { return value; }", "E_PARSE"},
        {"func identity<T: integer>(value: T) -> T { return value; } "
         "func f() -> u32 { return identity<u32>(7); }",
         NULL},
        {"func identity<T: integer>(value: T) -> T { return value; } "
         "func f() -> i8 { return identity<i8>(-128); }",
         NULL},
        {"func identity<T: integer>(value: T) -> T { return value; } "
         "func f() -> u8 { return identity(7); }",
         "E_GENERIC"},
        {"func identity<T: integer>(value: T) -> T { return value; } "
         "func f() -> bool { return identity<bool>(true); }",
         "E_GENERIC"},
        {"func ordinary(value: u8) -> u8 { return value; } "
         "func f() -> u8 { return ordinary<u8>(1); }",
         "E_GENERIC"},
        {"func identity<T: integer>(value: T) -> T { return value; } "
         "func f(value: i16) -> u16 { return identity<u16>(value); }",
         "E_TYPE"},
        {"func identity<T: integer>(value: T) -> T { return value; } "
         "func f() -> u8 { return identity<u8>(256); }",
         "E_RANGE"},
        {"func bad<T: integer>(value: T) -> T { return true; } func f() -> u8 { return bad<u8>(1); "
         "}",
         "E_TYPE"},
        {"func bad<T: integer>(value: T) -> T { return -value; } func f() -> u8 { return "
         "bad<u8>(1); }",
         "E_TYPE"},
        {"func bad<T: integer>(value: T) -> T { return 256; } func f() -> u8 { return bad<u8>(1); "
         "}",
         "E_RANGE"},
        {"func identity<T>(value: T) -> T { return value; }", "E_PARSE"},
        {"func identity<T: scalar>(value: T) -> T { return value; }", "E_PARSE"},
        {"func identity<u8: integer>(value: u8) -> u8 { return value; }", "E_PARSE"},
        {"export func identity<T: integer>(value: T) -> T { return value; }", "E_PARSE"},
        {"func main<T: integer>() -> void {}", "E_PARSE"},
        {"func test_bad<T: integer>() -> bool { return true; }", "E_PARSE"},
        {"func duplicate<T: integer>(value: T) -> T { return value; } "
         "func duplicate(value: u8) -> u8 { return value; }",
         "E_PARSE"},
        {"@effects() func bad<T: integer>(buffer: Buffer<u8>, value: T) -> T { return value; } "
         "func f(buffer: Buffer<u8>) -> u8 { return bad<u8>(move(buffer), 1); }",
         "E_EFFECT"},
        {"@effects(memory) func release<T: integer>(buffer: Buffer<u8>, value: T) -> T { return "
         "value; }",
         NULL},
        {"@budget(work = 50, heap = 0) func identity<T: integer>(value: T) -> T { return value; }",
         NULL},
        {"func recursive<T: integer>(value: T) -> T { if (value == 0) { return value; } "
         "return recursive<T>(value - 1); }",
         NULL},
        {"enum Error { invalid, } func nested<T: integer>(value: T) -> Result<Result<T, Error>, "
         "Error> "
         "{ return Result.ok(Result.ok(value)); }",
         NULL},
        {"func relation(left: u8, right: u8) -> bool { return left < right; }", NULL},
        {"struct Item { value: u8; } func f() -> Item { return Item<u8>(value = 1); }",
         "E_GENERIC"},
    };
    for (unsigned i = 0; i < sizeof(cases) / sizeof(cases[0]); ++i) {
        check(cases[i].source, cases[i].diagnostic);
    }
}

static void check_effect_contracts(void) {
    static const struct {
        const char *source;
        const char *diagnostic;
    } cases[] = {
        {"@effects() func f() -> void {}", NULL},
        {"@effects(memory) func f() -> void {}", NULL},
        {"@effects(io.read, io.write, memory, env.read, process.args, clock.read, "
         "fs.open, fs.read, fs.close, fs.write, fs.sync) func f() -> void {}",
         NULL},
        {"@effects() func f(flag: bool) -> void { while (flag) {} }", NULL},
        {"@effects() func f(value: u8) -> u8 { return 1 / value; }", NULL},
        {"@effects() func f(flag: bool) -> void { if (flag) { f(false); } }", NULL},
        {"@effects() @budget(work = 20, heap = 0) func f() -> void {}", NULL},
        {"@budget(work = 20, heap = 0) @effects() func f() -> void {}", NULL},
        {"@effects(memory) func f(buffer: Buffer<u8>) -> void {}", NULL},
        {"@effects() func f(buffer: Buffer<u8>) -> void {}", "E_EFFECT"},
        {"func drop_buffer(buffer: Buffer<u8>) -> void {} "
         "@effects() func f(buffer: Buffer<u8>) -> void { drop_buffer(move(buffer)); }",
         "E_EFFECT"},
        {"struct Owner { bytes: Buffer<u8>; } @effects() func f(owner: Owner) -> void {}",
         "E_EFFECT"},
        {"enum Event { bytes(Buffer<u8>), idle, } @effects() func f(event: Event) -> void {}",
         "E_EFFECT"},
        {"@effects(memory) @budget(work = 100, heap = 0) func f(buffer: Buffer<u8>) -> void {}",
         "E_EFFECT"},
        {"@effects() @effects() func f() -> void {}", "E_PARSE"},
        {"@budget(work = 10, heap = 0) @budget(work = 10, heap = 0) func f() -> void {}",
         "E_PARSE"},
        {"@effects(memory, memory) func f() -> void {}", "E_PARSE"},
        {"@effects(fs.read, fs.read) func f() -> void {}", "E_PARSE"},
        {"@effects(pure) func f() -> void {}", "E_PARSE"},
        {"@effects(fs.unknown) func f() -> void {}", "E_PARSE"},
        {"@effects(io.read.extra) func f() -> void {}", "E_PARSE"},
        {"@effects(memory,) func f() -> void {}", "E_PARSE"},
        {"@effects func f() -> void {}", "E_PARSE"},
        {"@effects(memory) struct Bad { value: u8; }", "E_PARSE"},
        {"pub @effects() func f() -> void {}", "E_PARSE"},
        {"@effects() pub func f() -> void {}", NULL},
    };
    for (unsigned i = 0; i < sizeof(cases) / sizeof(cases[0]); ++i) {
        check(cases[i].source, cases[i].diagnostic);
    }
}

static void check_export_names(void) {
    check("export func file_read() -> void {}", "E_ABI");
    check("export func buffer_resize() -> void {}", "E_ABI");
    check("export func arguments_read() -> void {}", "E_ABI");
    check("export func inspect() -> u8 { return 1; } "
          "export func inspect_result() -> u8 { return 2; }",
          "E_ABI");
    check("export func inspect_result() -> u8 { return 2; }", NULL);
}
static void check_resource_structs(void) {
    const char *prefix = "enum Error { rejected, } "
                         "struct Packet { bytes: Buffer<u8>; code: u8; } ";
    static const struct {
        const char *source;
        const char *diagnostic;
    } cases[] = {
        {"func f(bytes: Buffer<u8>) -> Packet { return Packet(bytes = move(bytes), code = 1); }",
         NULL},
        {"func f(packet: Packet) -> u8 { return packet.code + packet.code; }", NULL},
        {"func f(packet: Packet) -> Packet { return move(packet); }", NULL},
        {"func f(packet: Packet) -> Packet { return copy(packet); }", "E_MOVE"},
        {"func f(packet: Packet) -> Buffer<u8> { return move(packet.bytes); }", "E_MOVE"},
        {"func f(packet: Packet) -> Buffer<u8> { return packet.bytes; }", "E_MOVE"},
        {"func f(packet: Packet) -> void { let other: Packet = move(packet); let n: usize = "
         "packet.bytes.len; }",
         "E_INIT"},
        {"func f(packet: Packet) -> void { packet.bytes[0] = 1; }", "E_MUT"},
        {"func f() -> void { var packet: Packet; packet.code = 1; }", "E_INIT"},
        {"func grow(bytes: edit Buffer<u8>) -> void {} "
         "func f(packet: Packet) -> void { var owned: Packet = move(packet); grow(edit "
         "owned.bytes); }",
         NULL},
        {"func grow(bytes: edit Buffer<u8>) -> void {} "
         "func f(packet: Packet) -> void { grow(edit packet.bytes); }",
         "E_MUT"},
        {"func both(a: edit Buffer<u8>, b: read u8[]) -> void {} "
         "func f(packet: Packet) -> void { var owned: Packet = move(packet); both(edit "
         "owned.bytes, read owned.bytes); }",
         "E_ALIAS"},
        {"func take(packet: Packet) -> usize { return 0; } "
         "func f(packet: Packet) -> u8 { return packet.bytes[take(move(packet))]; }",
         "E_MOVE"},
        {"func grow(bytes: edit Buffer<u8>) -> usize { return 0; } "
         "func f(packet: Packet) -> u8 { var owned: Packet = move(packet); return "
         "owned.bytes[grow(edit owned.bytes)]; }",
         "E_ALIAS"},
        {"func grow(bytes: edit Buffer<u8>) -> void {} "
         "func f(bytes: Buffer<u8>) -> void { grow(edit Packet(bytes = move(bytes), code = "
         "1).bytes); }",
         "E_ACCESS"},
        {"func f(bytes: Buffer<u8>) -> Packet { return Packet(bytes = move(bytes), code = "
         "bytes[0]); }",
         "E_INIT"},
        {"struct Pair { first: Buffer<u8>; second: Buffer<u8>; } "
         "func both(a: edit Buffer<u8>, b: edit Buffer<u8>) -> void {} "
         "func f(pair: Pair) -> void { var owned: Pair = move(pair); both(edit owned.first, edit "
         "owned.second); }",
         "E_ALIAS"},
        {"func f(packets: Packet[2]) -> void {}", "E_PARSE"},
        {"struct Bad { pending: Result<Packet, Error>; }", "E_PARSE"},
        {"export func f(packet: Packet) -> void {}", "E_ABI"},
        {"@budget(work = 1000, heap = 0) func f(packet: Packet) -> void {}", "E_EFFECT"},
    };
    char source[2048];
    for (unsigned i = 0; i < sizeof(cases) / sizeof(cases[0]); ++i) {
        snprintf(source, sizeof(source), "%s%s", prefix, cases[i].source);
        check(source, cases[i].diagnostic);
    }
    size_t length = (size_t)snprintf(source, sizeof(source), "struct S0 { bytes: Buffer<u8>; } ");
    for (unsigned i = 1; i <= 30; ++i) {
        length += (size_t)snprintf(source + length, sizeof(source) - length,
                                   "struct S%u { nested: S%u; } ", i, i - 1);
    }
    snprintf(source + length, sizeof(source) - length, "func f(value: S30) -> void {}");
    check(source, NULL);
    snprintf(source + length, sizeof(source) - length,
             "struct S31 { nested: S30; } func f() -> void {}");
    check(source, "E_PARSE");
}

static void check_payload_enums(void) {
    const char *prefix = "enum Error { rejected, } enum Value { none, byte(u8), } "
                         "enum Other { none, byte(u8), } ";
    static const struct {
        const char *source;
        const char *diagnostic;
    } cases[] = {
        {"func f() -> Value { return Value.byte(7); }", NULL},
        {"func f() -> Value { return Value.none; }", NULL},
        {"func f(value: Value) -> u8 { match (value) { case Value.none { return 0; } "
         "case Value.byte(let byte: u8) { return byte; } } }",
         NULL},
        {"func f(value: Value) -> Value { match (copy(value)) { case Value.none {} "
         "case Value.byte(let byte: u8) {} } return move(value); }",
         NULL},
        {"struct Holder { value: Value; } func f(holder: Holder) -> Value { "
         "return copy(holder.value); }",
         NULL},
        {"func f(values: Value[2]) -> Value { return copy(values[0]); }", NULL},
        {"func f() -> Value { return Value.byte; }", "E_CALL"},
        {"func f() -> Value { return Value.byte(); }", "E_CALL"},
        {"func f() -> Value { return Value.byte(1, 2); }", "E_CALL"},
        {"func f() -> Value { return Value.byte(payload = 1); }", "E_CALL"},
        {"func f() -> Value { return Value.byte(read 1); }", "E_CALL"},
        {"func f() -> Value { return Value.byte(true); }", "E_TYPE"},
        {"func f() -> Value { return Value.byte(256); }", "E_RANGE"},
        {"func f() -> Value { return Value.none(); }", "E_CALL"},
        {"func f() -> Value { return Value.missing(1); }", "E_NAME"},
        {"func f() -> Value { return Other.byte(1); }", "E_TYPE"},
        {"func f(value: Value) -> void { match (value) { case Value.none {} } }", "E_MATCH"},
        {"func f(value: Value) -> void { match (value) { case Value.none {} "
         "case Value.byte {} } }",
         "E_MATCH"},
        {"func f(value: Value) -> void { match (value) { case Value.none {} "
         "case Value.byte() {} } }",
         "E_MATCH"},
        {"func f(value: Value) -> void { match (value) { case Value.none() {} "
         "case Value.byte(let byte: u8) {} } }",
         "E_MATCH"},
        {"func f(value: Value) -> void { match (value) { case Value.none {} "
         "case Value.byte(let byte: u16) {} } }",
         "E_TYPE"},
        {"func f(value: Value) -> void { match (value) { case Value.none {} "
         "case Other.byte(let byte: u8) {} } }",
         "E_MATCH"},
        {"func f(value: Value) -> void { match (value) { case Value.none {} "
         "case Value.byte(let byte: u8) {} case Value.none {} } }",
         "E_MATCH"},
        {"func f(value: Value) -> Value { match (value) { case Value.none {} "
         "case Value.byte(let byte: u8) {} } return move(value); }",
         "E_INIT"},
        {"func f(value: Value) -> bool { return value == Value.none; }", "E_TYPE"},
        {"func f(value: Value) -> Value { return value; }", "E_MOVE"},
        {"func f(values: Value[2]) -> Value { return move(values[0]); }", "E_MOVE"},
        {"export func f(value: Value) -> void {}", "E_ABI"},
        {"export func f() -> Value { return Value.none; }", "E_ABI"},
        {"export func f() -> Result<Value, Error> { return Result.ok(Value.none); }", "E_ABI"},
        {"func f() -> Result<u8, Value> { return Result.ok(1); }", "E_PARSE"},
        {"enum Bad { invalid(void), }", "E_PARSE"},
        {"enum Bad { invalid(read u8[]), }", "E_PARSE"},
        {"enum Bad { invalid(edit Buffer<u8>), }", "E_PARSE"},
        {"enum Bad { invalid(Result<u8, Error>), }", "E_PARSE"},
        {"enum Bad { invalid(Bad), }", "E_PARSE"},
        {"enum Bad { invalid(u8, u16), }", "E_PARSE"},
        {"enum Bad { invalid(u8[65536]), }", "E_PARSE"},
        {"enum Owner { bytes(Buffer<u8>), empty, } "
         "func f(owner: Owner) -> Owner { return copy(owner); }",
         "E_MOVE"},
        {"enum Owner { bytes(Buffer<u8>), empty, } "
         "struct Holder { owner: Owner; } func f() -> void {}",
         NULL},
        {"enum Owner { bytes(Buffer<u8>), empty, } "
         "func f(owners: Owner[2]) -> void {}",
         "E_PARSE"},
        {"enum Owner { bytes(Buffer<u8>), empty, } "
         "@budget(work = 1000, heap = 0) func f(owner: Owner) -> void {}",
         "E_EFFECT"},
        {"func f(value: Value) -> u8 { var byte: u8; match (value) { case Value.none {} "
         "case Value.byte(let payload: u8) { byte = payload; } } return byte; }",
         "E_INIT"},
        {"func f(value: Value) -> u8 { match (value) { case Value.none {} "
         "case Value.byte(let byte: u8) {} } return byte; }",
         "E_NAME"},
        {"func f(value: Value) -> void { while (true) { match (value) { "
         "case Value.none {} case Value.byte(let byte: u8) {} } } }",
         "E_MOVE"},
    };
    char source[8192];
    for (unsigned i = 0; i < sizeof(cases) / sizeof(cases[0]); ++i) {
        snprintf(source, sizeof(source), "%s%s", prefix, cases[i].source);
        check(source, cases[i].diagnostic);
    }
    size_t length = (size_t)snprintf(source, sizeof(source), "enum E0 { byte(u8), } ");
    for (unsigned i = 1; i <= 32; ++i) {
        length += (size_t)snprintf(source + length, sizeof(source) - length,
                                   "enum E%u { nested(E%u), } ", i, i - 1);
    }
    check(source, "E_PARSE");
}

static void check_result_propagation(void) {
    const char *prefix = "enum Error { rejected, } enum Other { rejected, } "
                         "func checked() -> Result<u8, Error> { return Result.ok(7); } "
                         "func unit() -> Result<void, Error> { return Result.ok(); } "
                         "func foreign() -> Result<u8, Other> { return Result.ok(7); } ";
    static const struct {
        const char *source;
        const char *diagnostic;
    } cases[] = {
        {"func f() -> Result<u16, Error> { let byte: u8 = try(checked()); "
         "return Result.ok(widen<u16>(byte)); }",
         NULL},
        {"func f(value: Result<u8, Error>) -> Result<u8, Error> { "
         "return Result.ok(try(value)); }",
         NULL},
        {"func f() -> Result<void, Error> { try(unit()); return Result.ok(); }", NULL},
        {"func f() -> Result<u8, Error> { var pending: Result<u8, Error>; "
         "var value: u8 = 0; for (var i: usize = 0; i < 2; i += 1) { "
         "pending = checked(); value = try(pending); } return Result.ok(value); }",
         NULL},
        {"func f() -> i32 { let value: u8 = try(checked()); return 0; }", "E_TRY"},
        {"func f() -> void { try(unit()); }", "E_TRY"},
        {"func f() -> Result<u8, Error> { return Result.ok(try(7)); }", "E_TRY"},
        {"func f() -> Result<u8, Error> { return Result.ok(try(foreign())); }", "E_TRY"},
        {"func f() -> Result<u16, Error> { return Result.ok(try(checked())); }", "E_TYPE"},
        {"func f() -> Result<void, Error> { try(checked()); return Result.ok(); }", "E_RESULT"},
        {"func f() -> Result<u8, Error> { let value: u8 = try(unit()); "
         "return Result.ok(value); }",
         "E_TYPE"},
        {"func f() -> Result<u8, Error> { let a: Result<u8, Error> = checked(); "
         "let b: Result<u8, Error> = checked(); let value: u8 = try(b); "
         "return move(a); }",
         "E_RESULT"},
        {"func f(value: Result<u8, Error>) -> Result<u8, Error> { "
         "let byte: u8 = try(value); return move(value); }",
         "E_INIT"},
        {"func f(value: Result<u8, Error>) -> Result<u8, Error> { "
         "return Result.ok(try(copy(value))); }",
         "E_RESULT"},
        {"func f() -> Result<u8, Error> { var value: Result<u8, Error>; "
         "return Result.ok(try(value)); }",
         "E_INIT"},
        {"func f(flag: bool) -> Result<u8, Error> { let value: Result<u8, Error> = checked(); "
         "if (flag) { let byte: u8 = try(value); } return move(value); }",
         "E_INIT"},
        {"func f() -> Result<u8, Error> { let value: Result<u8, Error> = checked(); "
         "for (var i: usize = 0; i < 2; i += 1) { let byte: u8 = try(value); } "
         "return Result.ok(0); }",
         "E_MOVE"},
        {"func f() -> Result<u8, Error> { return Result.ok(try checked()); }", "E_PARSE"},
        {"func f() -> Result<u8, Error> { return Result.ok(try()); }", "E_PARSE"},
        {"func f() -> Result<u8, Error> { return Result.ok(try(checked(), checked())); }",
         "E_PARSE"},
        {"func f() -> Result<u8, Error> { return Result.ok(try(Result.ok(7))); }", "E_TYPE"},
        {"func try() -> void {}", "E_PARSE"},
        {"@budget(work = 1, heap = 0) func f() -> Result<u8, Error> { "
         "return Result.ok(try(checked())); }",
         "E_BUDGET"},
    };
    char source[2048];
    for (unsigned i = 0; i < sizeof(cases) / sizeof(cases[0]); ++i) {
        snprintf(source, sizeof(source), "%s%s", prefix, cases[i].source);
        check(source, cases[i].diagnostic);
    }
}

static void check_literal_views(void) {
    check("func read_bytes(bytes: read u8[]) -> void {} "
          "func f() -> void { read_bytes(\"literal\"); }",
          NULL);
    check("func read_bytes(bytes: read u8[]) -> void {} "
          "func f() -> void { read_bytes(read \"literal\"); }",
          NULL);
    check("func read_bytes(bytes: read u8[]) -> void {} "
          "func f() -> void { let bytes: u8[1] = [0]; read_bytes(bytes); }",
          "E_ACCESS");
    check("func read_bytes(bytes: read u16[]) -> void {} "
          "func f() -> void { read_bytes(\"literal\"); }",
          "E_ACCESS");
    check("func edit_bytes(bytes: edit u8[]) -> void {} "
          "func f() -> void { edit_bytes(\"literal\"); }",
          "E_ACCESS");
    check("func f() -> void { return 0; }", "E_TYPE");
}

static void check_loop_control(void) {
    check("func f() -> void { for (var i: usize = 0; i < 1; var j: usize = 0) {} }", "E_PARSE");
    check("func take(a: u8[1]) -> void {} func make() -> u8[1] { return [1]; } "
          "func f() -> u8 { var a: u8[1] = [1]; var i: usize = 0; "
          "for (var unused: usize = 0; i < 1; a = make()) { i += 1; take(move(a)); continue; } "
          "return a[0]; }",
          NULL);

    check("func f() -> void { break; }", "E_FLOW");
    check("func f() -> void { continue; }", "E_FLOW");
    check("func f() -> void { while (true) { break; } continue; }", "E_FLOW");
    check("func f() -> void { while (true) { break; let n: i32 = 0; } }", "E_FLOW");
    check("func f() -> void { while (true) { continue; return; } }", "E_FLOW");
    check("func f() -> void { while (true) { break } }", "E_PARSE");
    check("func f() -> void { while (true) { continue 1; } }", "E_PARSE");
    check("func f() -> i32 { while (true) { break; } }", "E_RETURN");
    check("func f(flag: bool) -> void { for (var i: usize = 0; i < 3; i += 1) { "
          "var n: i32; if (flag) { continue; } else { n = 1; } let used: i32 = n; } }",
          NULL);
    check("func take(a: u8[1]) -> void {} func f() -> u8 { var a: u8[1] = [1]; "
          "while (true) { take(move(a)); break; } return a[0]; }",
          "E_INIT");
    check("func take(a: u8[1]) -> void {} func f() -> void { var a: u8[1] = [1]; "
          "while (true) { take(move(a)); continue; } }",
          "E_MOVE");
    check("func f() -> void { for (var i: usize = 0; i < 2; i += 1) { "
          "let r: Result<u8, CastError> = narrow<u8>(1); break; } }",
          "E_RESULT");
    check("func f() -> void { for (var i: usize = 0; i < 2; i += 1) { "
          "let r: Result<u8, CastError> = narrow<u8>(1); continue; } }",
          "E_RESULT");
    check("func f() -> void { var r: Result<u8, CastError>; "
          "for (var i: usize = 0; i < 2; i += 1) { r = narrow<u8>(1); } }",
          "E_RESULT");
    check("func f() -> void { var r: Result<u8, CastError>; "
          "for (var i: usize = 0; i < 2; i += 1) { r = narrow<u8>(1); continue; } }",
          "E_RESULT");
    check("@budget(work = 500, heap = 0) func g(flag: bool) -> void { var limit: usize = 0; "
          "for (var i: usize = 0; i < 1; i += 1) { "
          "if (flag) { limit = 100; break; } limit = 0; } "
          "for (var j: usize = 0; j < limit; j += 1) {} }",
          "E_BUDGET");
    check("@budget(work = 500, heap = 0) func f(flag: bool) -> void { var limit: usize = 0; "
          "for (var i: usize = 0; i < 1; i += 1) { "
          "if (flag) { limit = 100; continue; } limit = 0; } "
          "for (var j: usize = 0; j < limit; j += 1) {} }",
          "E_BUDGET");
}

static void check_integer_types(void) {
    check("func f() -> i8 { return -128; }", NULL);
    check("func f() -> i8 { return 127; }", NULL);
    check("func f() -> i8 { return 128; }", "E_RANGE");
    check("func f() -> i8 { return -129; }", "E_RANGE");
    check("func f() -> u16 { return 65535; }", NULL);
    check("func f() -> u16 { return 65536; }", "E_RANGE");
    check("func f() -> u64 { return 18446744073709551615; }", NULL);
    check("func f() -> u64 { return -1; }", "E_TYPE");
    check("func f(a: u64) -> usize { return a; }", "E_TYPE");
    check("func f(a: usize) -> u64 { return a; }", "E_TYPE");
    check("func f(a: i8) -> i16 { return widen<i16>(a); }", NULL);
    check("func f(a: u16) -> i32 { return widen<i32>(a); }", NULL);
    check("func f(a: u32) -> u64 { return widen<u64>(a); }", NULL);
    check("func f(a: u64) -> usize { return widen<usize>(a); }", NULL);
    check("func f(a: usize) -> u64 { return widen<u64>(a); }", NULL);
    check("func f(a: i8) -> u16 { return widen<u16>(a); }", "E_TYPE");
    check("func f(a: i16) -> i8 { return widen<i8>(a); }", "E_TYPE");
    check("func f(a: u16) -> i16 { return widen<i16>(a); }", "E_TYPE");
    check("func f(a: u64) -> i64 { return widen<i64>(a); }", "E_TYPE");
    check("func f(a: u16) -> u16 { return a << 16; }", "E_CONST");
    check("func f(a: u64) -> u64 { return a >> 64; }", "E_CONST");
    check("func f(a: i8) -> i8 { return a & 1; }", "E_TYPE");
    check("func f(a: u64, n: u64) -> u64 { return a << n; }", "E_TYPE");
}

static void check_enum_matches(void) {
    char source[16384];
    size_t length = (size_t)snprintf(source, sizeof(source), "enum E {");
    for (unsigned i = 0; i < 128; ++i) {
        length += (size_t)snprintf(source + length, sizeof(source) - length, "v%u,", i);
    }
    length += (size_t)snprintf(source + length, sizeof(source) - length,
                               "} func f(e: E) -> void { match (e) {");
    for (unsigned i = 0; i < 128; ++i) {
        length += (size_t)snprintf(source + length, sizeof(source) - length, "case E.v%u {}", i);
    }
    snprintf(source + length, sizeof(source) - length, "} }");
    check(source, NULL);
    length = (size_t)snprintf(source, sizeof(source), "enum E {");
    for (unsigned i = 0; i < 129; ++i) {
        length += (size_t)snprintf(source + length, sizeof(source) - length, "v%u,", i);
    }
    snprintf(source + length, sizeof(source) - length, "}");
    check(source, "E_PARSE");
    check("enum E { a, } func f() -> bool { let e: E = E.a; return e == E.a; }", NULL);

    check("enum E { a, b, } func f(e: E) -> i32 { match (e) { "
          "case E.a { return 1; } case E.b { return 2; } } }",
          NULL);
    check("enum E { a, } func f(r: Result<i32, E>) -> i32 { match (r) { "
          "case Result.ok(let n: i32) { return n; } case Result.err(let e: E) { "
          "match (e) { case E.a { return -1; } } } } }",
          NULL);
    check("enum E { a, } func f(e: E) -> bool { match (e) { case E.a {} } "
          "match (e) { case E.a {} } return e == E.a; }",
          NULL);
    check("enum E { a, b, } func f(e: E) -> void { match (e) { case E.a {} } }", "E_MATCH");
    check("enum E { a, } func f(e: E) -> void { match (e) {} }", "E_MATCH");
    check("enum E { a, } func f(e: E) -> void { match (e) { case E.a {} case E.a {} } }",
          "E_MATCH");
    check("enum E { a, } func f(e: E) -> void { match (e) { case E.b {} } }", "E_MATCH");
    check("enum E { a, } enum F { a, } func f(e: E) -> void { match (e) { case F.a {} } }",
          "E_MATCH");
    check("enum E { a, } func f(e: E) -> void { match (e) { case Missing.a {} } }", "E_MATCH");
    check("enum E { a, } func f(e: E) -> void { match (e) { case Result.ok() {} } }", "E_MATCH");
    check("enum E { a, } func f(e: Result<i32, E>) -> void { match (e) { case E.a {} } }",
          "E_MATCH");
    check("enum E { a, } func f(e: E) -> void { match (e) { case E.a() {} } }", "E_MATCH");
    check("func f(e: i32) -> void { match (e) {} }", "E_MATCH");
    check("enum E { a, b, } func f(e: E) -> i32 { var n: i32; match (e) { "
          "case E.a { n = 1; } case E.b {} } return n; }",
          "E_INIT");
    check("enum E { a, b, } func f(e: E) -> i32 { match (e) { "
          "case E.a { return 1; } case E.b {} } }",
          "E_RETURN");
    check("enum E { a, } func f(e: E) -> i32 { match (e) { "
          "case E.a { return 1; } } return 2; }",
          "E_FLOW");
}

static void check_literal_context(void) {
    check("func f(value: i16) -> bool { return -1 < value; }", NULL);
    check("func f(value: i32) -> bool { return -2147483648 == value; }", NULL);
    check("func f(value: i64) -> bool { return -9223372036854775808 == value; }", NULL);
    check("func f(value: u32) -> bool { return +1 < value; }", NULL);
    check("func f(value: u8) -> bool { return ~0 == value; }", NULL);
    check("func f(value: i32) -> bool { return -(-1) == value; }", NULL);
    check("func f(value: i16) -> bool { return -32769 < value; }", "E_RANGE");
    check("func f(value: i32) -> bool { return -2147483649 < value; }", "E_RANGE");
    check("func f(value: u32) -> bool { return -1 < value; }", "E_TYPE");
    check("func f(value: i32) -> bool { return ~0 == value; }", "E_TYPE");
    check("func f(value: bool) -> bool { return +1 == value; }", "E_TYPE");
    check("func f(a: i16, b: i32) -> bool { return a < b; }", "E_TYPE");
    check("func f(a: i32, b: i16) -> bool { return a < b; }", "E_TYPE");
    check("func f(a: i32) -> bool { let b: i64 = -1; return b < a; }", "E_TYPE");
}

static void check_control_flow(void) {
    static const char *integers[] = {"u8", "i8", "u16", "i16", "i32", "u32", "i64", "u64", "usize"};
    char source[512];
    for (size_t i = 0; i < sizeof(integers) / sizeof(integers[0]); ++i) {
        snprintf(source, sizeof(source),
                 "func f(value: %s, divisor: %s) -> %s { var n: %s = value; "
                 "n /= divisor; n %%= divisor; return n; }",
                 integers[i], integers[i], integers[i], integers[i]);
        check(source, NULL);
    }
    check("func f() -> void { var n: i32 = 4; n /= 0; }", "E_CONST");
    check("func f() -> void { var n: i32 = 4; n %= 0; }", "E_CONST");
    check("func f() -> void { let n: i32 = 4; n /= 2; }", "E_MUT");
    check("func f() -> void { var n: i32; n %= 2; }", "E_INIT");
    check("func f() -> void { var n: bool = true; n /= false; }", "E_TYPE");
    check("func f(n: u32) -> void { var x: i32 = 4; x %= n; }", "E_TYPE");
    check("func f() -> void { if (1) {} }", "E_TYPE");
    check("func f() -> void { while (1) {} }", "E_TYPE");
    check("func f() -> void { for (var i: usize = 0; 1; i += 1) {} }", "E_TYPE");
    check("func f(flag: bool) -> i32 { var n: i32; if (flag) { n = 1; } "
          "else { n = 2; } return n; }",
          NULL);
    check("func f(flag: bool) -> i32 { var n: i32; if (flag) { n = 1; } return n; }", "E_INIT");
    check("func f(flag: bool) -> i32 { var n: i32; while (flag) { n = 1; } return n; }", "E_INIT");
    check("func f() -> usize { for (var i: usize = 0; i < 1; i += 1) {} return i; }", "E_NAME");
    check("func f(flag: bool) -> i32 { if (flag) { return 1; } }", "E_RETURN");
    check("func f() -> i32 { return 1; return 2; }", "E_FLOW");
    check("func f() -> void { return 1; }", "E_TYPE");
    check("func f() -> i32 { return; }", "E_TYPE");
    check("func f() -> void { if true {} }", "E_PARSE");
    check("func f() -> void { for var i: usize = 0; i < 1; i += 1 {} }", "E_PARSE");
    check("enum Error { rejected, } func f(r: Result<i32, Error>) -> i32 { "
          "match (r) { case Result.ok(let n: i32) { return n; } } }",
          "E_MATCH");
    check("enum Error { rejected, } func f(r: Result<i32, Error>) -> i32 { "
          "match (r) { case Result.ok(let n: i32) { return n; } "
          "case Result.ok(let n: i32) { return n; } } }",
          "E_MATCH");
}

static void check_bitwise(void) {
    check("func f(a: u8, b: u8, count: usize) -> u8 { return (~a & b | a ^ b) << count; }", NULL);
    check("func f(a: u32, count: usize) -> u32 { return a >> count; }", NULL);
    check("func f(a: usize) -> usize { return ~a >> 63; }", NULL);
    check("func f(a: u8) -> u8 { return a << 8; }", "E_CONST");
    check("func f(a: u32) -> u32 { return a >> 32; }", "E_CONST");
    check("func f(a: usize) -> usize { return a << 64; }", "E_CONST");
    check("func f(a: u8) -> u8 { return a << -1; }", "E_TYPE");
    check("func f(a: u8, count: u8) -> u8 { return a << count; }", "E_TYPE");
    check("func f(a: u8, count: i64) -> u8 { return a >> count; }", "E_TYPE");
    check("func f(a: i16) -> i16 { return ~a; }", "E_TYPE");
    check("func f(a: i32) -> i32 { return a & 1; }", "E_TYPE");
    check("func f(a: i64) -> i64 { return a << 1; }", "E_TYPE");
    check("func f(a: bool) -> bool { return ~a; }", "E_TYPE");
    check("func f(a: bool) -> bool { return a | false; }", "E_TYPE");
    check("func f(a: u8, b: u32) -> u8 { return a ^ b; }", "E_TYPE");
    check("enum E { item, } func f(a: E) -> E { return a & E.item; }", "E_TYPE");
    check("func f(a: u32) -> bool { return a & 1 == 1; }", "E_TYPE");
    check("func f(a: u32) -> bool { return (a & 1) == 1; }", NULL);
    check("func f(a: u8) -> u8 { return a <<= 1; }", "E_PARSE");
    // A shift token in an invalid cast must still close nested type arguments.
    check("func f(a: u8) -> u8 { return narrow<Buffer<u8>>(a); }", "E_TYPE");
}

static void check_buffer_borrows(void) {
    check("func grow(b: edit Buffer<u8>) -> usize { b[0] = 1; return b.len + b.capacity; }", NULL);
    check("func edit_bytes(b: edit u8[]) -> void { b[0] = 1; }"
          "func inner(b: edit Buffer<u8>) -> void { edit_bytes(edit b); }"
          "func outer(b: edit Buffer<u8>) -> void { inner(edit b); }",
          NULL);
    check("func edit_bytes(a: edit u8[], b: edit u8[]) -> void {}"
          "func f(b: edit Buffer<u8>) -> void { edit_bytes(edit b[0..1], edit b[1..2]); }",
          NULL);
    check("@budget(work = 1000, heap = 0) func inspect(b: edit Buffer<u8>) -> usize { return "
          "b.len; }",
          NULL);
    check("func f(b: read Buffer<u8>) -> void {}", "E_PARSE");
    check("func f(b: edit Buffer<u8>) -> edit Buffer<u8> { return b; }", "E_PARSE");
    check("func f() -> void { var b: edit Buffer<u8>; }", "E_TYPE");
    check("struct Holder { b: edit Buffer<u8>; }", "E_PARSE");
    check("enum Error { invalid, } func f(b: Result<edit Buffer<u8>, Error>) -> void {}",
          "E_PARSE");
    check("func f(b: edit Buffer<u8>[2]) -> void {}", "E_PARSE");
    check("export func f(b: edit Buffer<u8>) -> void {}", "E_ABI");
    check("func f(b: edit Buffer<u8>) -> void { copy(b); }", "E_MOVE");
    check("func f(b: edit Buffer<u8>) -> void { move(b); }", "E_MOVE");
    check("func f(b: edit Buffer<u8>) -> void { b = b; }", "E_MUT");
    check("func f(b: edit Buffer<u8>) -> void { b.len = 1; }", "E_MUT");
    check("func f(b: edit Buffer<u8>) -> void { b.capacity = 1; }", "E_MUT");
    check("func edit_buffer(b: edit Buffer<u8>) -> void {}"
          "func f(b: Buffer<u8>) -> void { edit_buffer(edit b); }",
          "E_MUT");
    check("func edit_buffer(b: edit Buffer<u8>) -> void {}"
          "func f(b: edit Buffer<u8>) -> void { edit_buffer(b); }",
          "E_ACCESS");
    check("func edit_buffer(b: edit Buffer<u8>) -> void {}"
          "func f(b: edit Buffer<u8>) -> void { edit_buffer(read b); }",
          "E_ACCESS");
    check("func edit_buffer(b: edit Buffer<u8>) -> void {}"
          "func f(b: edit Buffer<u8>) -> void { edit_buffer(edit b[0..1]); }",
          "E_ACCESS");
    check("func identity(b: Buffer<u8>) -> Buffer<u8> { return move(b); }"
          "func edit_buffer(b: edit Buffer<u8>) -> void {}"
          "func f(b: Buffer<u8>) -> void { edit_buffer(edit identity(move(b))); }",
          "E_ACCESS");
    check("func pair(a: edit Buffer<u8>, b: read u8[]) -> void {}"
          "func f(b: edit Buffer<u8>) -> void { pair(edit b, read b); }",
          "E_ALIAS");
    check("func pair(a: read u8[], b: edit Buffer<u8>) -> void {}"
          "func f(b: edit Buffer<u8>) -> void { pair(read b, edit b); }",
          "E_ALIAS");
    check("func pair(a: edit Buffer<u8>, b: edit Buffer<u8>) -> void {}"
          "func f(b: edit Buffer<u8>) -> void { pair(edit b, edit b); }",
          "E_ALIAS");
    check("func set(b: edit Buffer<u8>, count: usize) -> void {}"
          "func f(b: edit Buffer<u8>) -> void { set(edit b, b.len); }",
          "E_ALIAS");
    check("func index(b: edit Buffer<u8>) -> usize { return 0; }"
          "func f(b: edit Buffer<u8>) -> u8 { return b[index(edit b)]; }",
          "E_ALIAS");
    check("func index(b: edit Buffer<u8>) -> usize { return 0; }"
          "func view(b: read u8[]) -> void {}"
          "func f(b: edit Buffer<u8>) -> void { view(read b[0..index(edit b)]); }",
          "E_ALIAS");
    check("func change(b: edit Buffer<u8>) -> u8 { return 0; }"
          "func f(b: edit Buffer<u8>) -> void { b[0] = change(edit b); }",
          "E_ALIAS");
    check("func change(b: edit Buffer<u8>) -> usize { return 0; }"
          "func set(b: edit Buffer<u8>, count: usize) -> void {}"
          "func f(b: edit Buffer<u8>) -> void { set(edit b, change(edit b)); }",
          "E_ALIAS");
    check("@budget(work = 1000, heap = 0) func f(b: edit Buffer<u8>) -> usize {"
          "if (b.len > 8) { return 0; } var count: usize = 0;"
          "for (var index: usize = 0; index < b.len; index += 1) { count += 1; } return count; }",
          "E_BUDGET");
}

int main(void) {
    check("struct S { bytes: u8[2]; } func first(bytes: read u8[]) -> u8 { return bytes[0]; } "
          "func f(value: read S) -> u8 { return first(read value.bytes[0..1]); }",
          NULL);
    check("struct S { bytes: u8[2]; } func write(bytes: edit u8[]) -> void {} "
          "func f(value: read S) -> void { write(edit value.bytes[0..1]); }",
          "E_MUT");

    check_generic_instances();
    check("func f(value: str, index: usize) -> u8 { return value[index]; }", NULL);
    check("func f() -> void { var value: str = \"abc\"; value[0] = 0; }", "E_MUT");
    check("struct S { value: str; } func f(value: edit S) -> void { value.value[0] += 1; }",
          "E_MUT");
    check("func f() -> void { var values: str[1] = [\"abc\"]; values[0][0] = 0; }", "E_MUT");

    check_native_integer_buffers();
    check_native_data_buffers();
    check_array_repetition();
    check("func f() -> str { return \"valid\"; }", NULL);
    check("func f() -> str { return \"\xc0\xaf\"; }", "E_STRING");
    check("func f() -> str { return \"\xed\xa0\x80\"; }", "E_STRING");
    check("func f() -> str { return \"\xf4\x90\x80\x80\"; }", "E_STRING");
    check("func f() -> str { return \"\xe2\x82\"; }", "E_STRING");
    check("func f() -> str { return \"\x80\"; }", "E_STRING");
    check("func f() -> str { return \"\xc2\x80\xe0\xa0\x80\xf4\x8f\xbf\xbf\"; }", NULL);
    check("func f() -> void { let s: str = \"a\"; s = \"b\"; }", "E_MUT");
    check("func write(s: edit u8[]) -> void {} func f() -> void { var s: str = \"a\"; write(edit "
          "s); }",
          "E_TYPE");
    check("func f(bytes: read u8[]) -> str { return bytes; }", "E_TYPE");
    check("export func f(value: str) -> void {}", "E_ABI");
    check("export func f() -> str { return \"a\"; }", "E_ABI");
    check("type Count = usize; type Total = Count; func f(value: Total) -> usize { return value; }",
          NULL);
    check("type Small = u8; func f() -> Small { return 256; }", "E_RANGE");
    check("type Count = usize; func f() -> Count { return true; }", "E_TYPE");
    check("type Count = usize; type Count = u32;", "E_PARSE");
    check("type usize = u32;", "E_PARSE");
    check("type Buffer = u32;", "E_PARSE");
    check("type Loop = Loop;", "E_PARSE");
    check("type First = Later; type Later = u32;", "E_PARSE");
    check("type Count = usize", "E_PARSE");
    check("func count() -> void {} type count = usize;", "E_PARSE");
    check("func count<T: integer>(value: T) -> T { return value; } type count = usize;", "E_PARSE");
    check("type count = usize; func count() -> void {}", "E_PARSE");
    check("type View = read u8[]; func f() -> View {}", "E_PARSE");
    check("type Bytes = Buffer<u8>; func f(value: Bytes) -> Bytes { return copy(value); }",
          "E_MOVE");
    check("type Bytes = Buffer<u8>; @effects() func f(value: Bytes) -> void {}", "E_EFFECT");
    check_struct_borrows();
    check_scalar_matches();
    check_integer_generics();
    check_effect_contracts();
    check_export_names();
    check_resource_structs();
    check_payload_enums();
    check_result_propagation();
    check_literal_views();
    check_loop_control();
    check_integer_types();
    check_enum_matches();
    check_literal_context();
    check_control_flow();
    check_bitwise();
    check_buffer_borrows();
    check("func f() -> void { if true { return; } }", "E_PARSE");
    check("func f() -> void { for var i: usize = 0; i < 2; i += 1 {} }", "E_PARSE");
    check("func f() -> void { let a: i16[1] = [0]; a[0] = 1; }", "E_MUT");
    check("func f() -> i64 { var x: i64; return x; }", "E_INIT");
    check("func f(b: bool) -> i64 { var x: i64; if (b) { x = 1; } return x; }", "E_INIT");
    check("func f(b: bool) -> i64 { var x: i64; if (b) { x = 1; } else { x = 2; } return x; }",
          NULL);
    check("func f() -> i64 { let a: i64[1] = [1]; let b: i64[1] = move(a); return a[0]; }",
          "E_INIT");
    check("func f() -> void { let a: i64[1] = [1]; let b: i64[1] = a; }", "E_MOVE");
    check("func f(a: read u8[]) -> read u8[] { return a; }", "E_PARSE");
    check("func g(a: read u8[], b: edit u8[]) -> void {} func f() -> void { var a: u8[2] = [0,0]; "
          "g(read a, edit a); }",
          "E_ALIAS");
    check("func g(a: read u8[], b: edit u8[]) -> void {} func f() -> void { var a: u8[2] = [0,0]; "
          "g(read a[0..1], edit a[1..2]); }",
          NULL);
    check("func f() -> u8 { return 256; }", "E_RANGE");
    check("func f() -> u8 { return 255 + 1; }", "E_CONST");
    check("func f(x: i64) -> i64 { return x / 0; }", "E_CONST");
    check("func f() -> i64 { let a: i64[1] = [1]; return a[1]; }", "E_CONST");
    check("func f(x: u8) -> i64 { return x; }", "E_TYPE");
    check("func f(x: i64) -> u8 { return widen<u8>(x); }", "E_TYPE");
    check("func f(x: i64) -> void { let a: Result<i16, CastError> = narrow<i16>(x); }", "E_RESULT");
    check("func f(x: i64) -> void { narrow<i16>(x); }", "E_RESULT");
    check("func f(x: i64) -> i16 { let a: Result<i16, CastError> = narrow<i16>(x); match (a) { "
          "case Result.ok(let n: i16) { return n; } } }",
          "E_MATCH");
    check("@budget(work = 10, heap = 0) func f(n: usize) -> void { for (var i: usize = 0; i < n; i "
          "+= 1) {} }",
          "E_BUDGET");
    check("@budget(work = 1000, heap = 0) func f(n: usize) -> void { if (n > 4) { return; } for "
          "(var i: usize = 0; i < n; i += 1) {} }",
          NULL);
    check("@budget(work = 1000, heap = 0) func f() -> void { while (true) {} }", "E_BUDGET");
    check("@budget(work = 1000, heap = 0) func f() -> void { f(); }", "E_BUDGET");
    check("func g() -> void { for (var i: usize = 0; i < 100; i += 1) { for (var j: usize = 0; j < "
          "100; j += 1) {} } } @budget(work = 100, heap = 0) func f() -> void { g(); }",
          "E_BUDGET");
    check("func f() -> void { malloc(); }", "E_NAME");
    check("@budget(work = 100000, heap = 0) func f() -> void { var n: usize = 1; for (var i: usize "
          "= 0; i < 100; i += 1) { n *= 2; for (var j: usize = 0; j < n; j += 1) {} } }",
          "E_BUDGET");
    check("func f() -> void { let r: Result<u8, CastError>[1] = [narrow<u8>(1)]; }", "E_PARSE");
    check("func pred(a: i64[1]) -> bool { return true; } func f() -> void { let a: i64[1] = [1]; "
          "while (pred(move(a))) {} }",
          "E_MOVE");
    check("func f(b: bool) -> void { let r: Result<i16, CastError> = narrow<i16>(1); if (b) { "
          "match (r) { case Result.ok(let n: i16) {} case Result.err(let e: CastError) {} } } }",
          "E_RESULT");
    check("func g(a: read u8[], b: edit u8[]) -> void {} func f() -> void { var a: u8[3] = "
          "[0,0,0]; g(read a[1..3][0..1], edit a[0..2][1..2]); }",
          "E_ALIAS");
    check("func f() -> Result<i16, CastError> { return narrow<i16>(-9223372036854775808); }", NULL);
    check(
        "func f() -> Result<i64[65536], CastError> { return Result.err(CastError.out_of_range); }",
        "E_PARSE");
    char chain[16384];
    size_t used = 0;
    for (unsigned i = 0; i < 127; ++i) {
        used += (size_t)snprintf(chain + used, sizeof(chain) - used,
                                 "func f%u() -> void { f%u(); }\n", i, i + 1);
    }
    (void)snprintf(chain + used, sizeof(chain) - used, "func f127() -> void {}\n");
    check(chain, NULL);
    check("func Pair() -> void {} struct Pair { x: i64; }", "E_PARSE");

    check("func f(b: Buffer<u8>) -> Buffer<u8> { return copy(b); }", "E_MOVE");
    check("func f(b: Buffer<u8>) -> Buffer<u8> { return b; }", "E_MOVE");
    check("func f(b: Buffer<u8>) -> usize { let c: Buffer<u8> = move(b); return b.len; }",
          "E_INIT");
    check("func f(b: Buffer<u8>) -> void { b[0] = 1; }", "E_MUT");
    check("func f(b: Buffer<u8>) -> void { let v: read u8[] = b; }", "E_TYPE");
    check("func g(b: edit u8[]) -> void {} func f(b: Buffer<u8>) -> void { g(edit b); }", "E_MUT");
    check("func g(v: read u8[], b: Buffer<u8>) -> void {}"
          "func f(b: Buffer<u8>) -> void { g(read b, move(b)); }",
          "E_ALIAS");
    check("func g(b: Buffer<u8>, v: read u8[]) -> void {}"
          "func f(b: Buffer<u8>) -> void { g(move(b), read b); }",
          "E_INIT");
    check("func g(a: edit u8[], b: edit u8[]) -> void {}"
          "func f(b: Buffer<u8>) -> void { var c: Buffer<u8> = move(b); g(edit c, edit c); }",
          "E_ALIAS");
    check("func f() -> usize { var b: Buffer<u8>; return b.len; }", "E_INIT");
    check("func f(b: Buffer<u8>, take: bool) -> usize {"
          "if (take) { let c: Buffer<u8> = move(b); } return b.len; }",
          "E_INIT");
    check("func f(b: Buffer<u8>) -> void { let byte: u8 = move(b[0]); }", "E_MOVE");
    check("struct Holder { b: Buffer<u8>; } func f() -> void {}", NULL);
    check("func f(b: Buffer<u8>[2]) -> void {}", "E_PARSE");
    check("func f(b: Buffer<i32>) -> void {}", NULL);
    check("func f(b: Buffer<bool>) -> void {}", NULL);
    check("struct S { n: u8; } func f(b: Buffer<S>) -> void {}", NULL);
    check("enum Error { failed, } func f(b: Result<Result<Buffer<u8>, Error>, Error>) -> void {}",
          "E_PARSE");
    check("enum Error { failed, } func f(b: Result<Buffer<u8>, Error>) -> Result<Buffer<u8>, "
          "Error> { return copy(b); }",
          "E_MOVE");
    check("export func f(b: Buffer<u8>) -> void {}", "E_ABI");
    check("export func f(b: bool) -> Buffer<u8> { while (true) {} }", "E_RETURN");
    check("enum Error { failed, } export func f() -> Result<Buffer<u8>, Error> { return "
          "Result.err(Error.failed); }",
          "E_ABI");
    check("@budget(work = 1000, heap = 0) func f(b: Buffer<u8>) -> void {}", "E_EFFECT");
    check("func f(b: Buffer<u8>) -> Buffer<u8> { return move(b); }", NULL);
    check("func f(b: Buffer<u8>) -> void { var c: Buffer<u8> = move(b); c = move(c); }", NULL);
    check("func g(a: edit u8[], b: edit u8[]) -> void {}"
          "func f(b: Buffer<u8>) -> void { var c: Buffer<u8> = move(b); g(edit c[0..1], edit "
          "c[1..2]); }",
          NULL);
    check("func index(b: Buffer<u8>) -> usize { return 0; }"
          "func f(b: Buffer<u8>) -> u8 { return b[index(move(b))]; }",
          "E_MOVE");
    check("func end(b: Buffer<u8>) -> usize { return 0; } func read_bytes(b: read u8[]) -> void {}"
          "func f(b: Buffer<u8>) -> void { read_bytes(read b[0..end(move(b))]); }",
          "E_MOVE");
    check(
        "func start(b: Buffer<u8>) -> usize { return 0; } func read_bytes(b: read u8[]) -> void {}"
        "func f(b: Buffer<u8>) -> void { read_bytes(read b[start(move(b))..1]); }",
        "E_MOVE");
    /* A different view's bounds must not consume a borrowed owner. */
    check("func consume(a: Buffer<u8>) -> usize { return 0; }"
          "func pair(a: read u8[], b: read u8[]) -> void {}"
          "func f(a: Buffer<u8>, b: read u8[]) -> void {"
          "pair(read a, read b[consume(move(a))..1]); }",
          "E_ALIAS");
    check("func consume(a: Buffer<u8>) -> usize { return 1; }"
          "func pair(a: read u8[], b: read u8[]) -> void {}"
          "func f(a: Buffer<u8>, b: read u8[]) -> void {"
          "pair(read a, read b[0..consume(move(a))]); }",
          "E_ALIAS");
    check("func consume(a: Buffer<u8>) -> usize { return 0; }"
          "func pair(a: read u8[], b: read u8[]) -> void {}"
          "func f(a: Buffer<u8>, b: read u8[]) -> void {"
          "pair(read b[consume(move(a))..1], read a); }",
          "E_INIT");
    check("func consume(a: Buffer<u8>) -> usize { return 0; }"
          "func pair(a: read u8[], b: read u8[]) -> void {}"
          "func f(a: Buffer<u8>, b: read u8[]) -> void {"
          "pair(read a, read b[consume(move(a))..1][0..1]); }",
          "E_ALIAS");
    check("func consume(a: Buffer<u8>) -> usize { return 0; }"
          "func pair(a: read u8[], b: read u8[]) -> void {}"
          "struct Row { bytes: u8[1]; }"
          "func f(a: Buffer<u8>, row: Row) -> void {"
          "pair(read a, read row.bytes[consume(move(a))..1]); }",
          "E_ALIAS");
    check("func size(a: edit u8[]) -> usize { a[0] = 1; return 1; }"
          "func pair(a: read u8[], b: read u8[]) -> void {}"
          "func f() -> void { var a: u8[2] = [0, 0];"
          "pair(read a, read a[0..size(edit a)]); }",
          "E_ALIAS");
    check("func size(a: read u8[]) -> usize { return a.len; }"
          "func pair(a: read u8[], b: read u8[]) -> void {}"
          "func f(a: read u8[], b: read u8[]) -> void {"
          "pair(read a, read b[0..size(read a)]); }",
          "E_ALIAS");
    check("func pair(a: read u8[], b: read u8[]) -> void {}"
          "func f(a: read u8[], b: read u8[], length: usize) -> void {"
          "pair(read a, read b[0..length]); pair(read a, read a); }",
          NULL);
    check("func size(a: read u8[]) -> usize { return a.len; }"
          "func pair(a: read u8[], b: read u8[]) -> void {}"
          "func f(a: read u8[], b: read u8[]) -> void {"
          "let length: usize = size(read a); pair(read a, read b[0..length]); }",
          NULL);
    check("func consume(a: Buffer<u8>) -> usize { return 0; }"
          "func pair(a: read u8[], b: edit u8[]) -> void {}"
          "func f(a: Buffer<u8>, b: edit u8[]) -> void {"
          "pair(read a, edit b[consume(move(a))..1]); }",
          "E_ALIAS");
    check("func consume(a: Buffer<u8>) -> usize { return 0; }"
          "func pair(a: read u8[], b: read u8[]) -> void {}"
          "func f(a: read u8[], b: read u8[], c: Buffer<u8>) -> void {"
          "pair(read a, read b[consume(move(c))..1]); }",
          NULL);
    puts("PASS: semantic rejection and static-contract cases");
    return 0;
}
