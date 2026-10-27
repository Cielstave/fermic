#include "fermi/fe.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

static void render(const FeContext *context, bool json, char output[8192]) {
    FILE *stream = tmpfile();
    assert(stream);
    fe_render_diagnostic(stream, context, json, false);
    assert(!ferror(stream));
    rewind(stream);
    size_t length = fread(output, 1, 8191, stream);
    output[length] = 0;
    assert(!ferror(stream));
    fclose(stream);
}

int main(void) {
    const char *source = "func main() -> i32 {\n\treturn missing;\n}\n";
    FeContext context;
    FeTokens tokens;
    FeProgram program;
    assert(fe_context_init(&context, "bad\"name\033.fe", source, strlen(source)));
    assert(fe_lex(&context, &tokens) && fe_parse(&context, tokens, &program));
    assert(!fe_check(&program));
    char output[8192];
    render(&context, false, output);
    assert(strstr(output, "error[E_NAME]"));
    assert(strstr(output, "return missing;"));
    assert(strstr(output, "^"));
    assert(!strchr(output, '\033'));
    render(&context, true, output);
    assert(strstr(output, "\"schema_version\":1"));
    assert(strstr(output, "bad\\\"name\\u001b.fe"));
    assert(strstr(output, "\"line\":2,\"column\":9"));
    assert(strstr(output, "\"related\":null"));
    fe_context_destroy(&context);

    source = "func main() -> i32 { let a: i16[1] = [1]; let b: i16[1] = move(a); return "
             "widen<i32>(a[0]); }";
    assert(fe_context_init(&context, "move.fe", source, strlen(source)));
    assert(fe_lex(&context, &tokens) && fe_parse(&context, tokens, &program));
    assert(!fe_check(&program));
    assert(!strcmp(context.diagnostic.code, "E_INIT"));
    assert(context.diagnostic.related.line == 1);
    render(&context, false, output);
    assert(strstr(output, "ownership was transferred here"));
    assert(strstr(output, "help:"));
    render(&context, true, output);
    assert(strstr(output, "\"related\":{\"span\":"));
    fe_context_destroy(&context);
    source = "func f(value: i16) -> i32 { return value; }";
    assert(fe_context_init(&context, "types.fe", source, strlen(source)));
    assert(fe_lex(&context, &tokens) && fe_parse(&context, tokens, &program));
    assert(!fe_check(&program));
    assert(!strcmp(context.diagnostic.message, "expected 'i32', found 'i16'"));
    assert(strstr(context.diagnostic.help, "numeric conversions must be explicit"));
    fe_context_destroy(&context);

    source = "func f(value: i16[2]) -> i16[3] { return move(value); }";
    assert(fe_context_init(&context, "arrays.fe", source, strlen(source)));
    assert(fe_lex(&context, &tokens) && fe_parse(&context, tokens, &program));
    assert(!fe_check(&program));
    assert(!strcmp(context.diagnostic.message, "expected 'i16[3]', found 'i16[2]'"));
    assert(!*context.diagnostic.help);
    char short_name[5];
    fe_format_type(program.functions->params->type, short_name, sizeof(short_name));
    assert(!strcmp(short_name, "i..."));
    fe_context_destroy(&context);

    source = "func f() -> i32 { var count: i32; return count; }";
    assert(fe_context_init(&context, "initialization.fe", source, strlen(source)));
    assert(fe_lex(&context, &tokens) && fe_parse(&context, tokens, &program));
    assert(!fe_check(&program));
    assert(!strcmp(context.diagnostic.message, "binding 'count' is not initialized on every path"));
    assert(!*context.diagnostic.note);
    fe_context_destroy(&context);
    source = "func f(bytes: Buffer<u8>) -> usize { let owner: Buffer<u8> = move(bytes); return "
             "bytes.len; }";
    assert(fe_context_init(&context, "resource-move.fe", source, strlen(source)));
    assert(fe_lex(&context, &tokens) && fe_parse(&context, tokens, &program));
    assert(!fe_check(&program));
    assert(!strcmp(context.diagnostic.code, "E_INIT"));
    assert(context.diagnostic.related.line == 1);
    assert(strstr(context.diagnostic.help, "borrow"));
    assert(!strstr(context.diagnostic.help, "copy"));
    fe_context_destroy(&context);
    source = "func f(buffer: edit Buffer<u8>) -> usize { return buffer; }";
    assert(fe_context_init(&context, "descriptor.fe", source, strlen(source)));
    assert(fe_lex(&context, &tokens) && fe_parse(&context, tokens, &program));
    assert(!fe_check(&program));
    assert(!strcmp(context.diagnostic.code, "E_TYPE"));
    assert(!strcmp(context.diagnostic.message, "expected 'usize', found 'edit Buffer<u8>'"));
    fe_context_destroy(&context);
    puts("PASS: diagnostic wording, type mismatches, move locations, JSON and terminal escaping");
    return 0;
}
