#include "fermi/fe.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void *trackedAlloc(size_t size);
static void trackedFree(void *pointer);
#define malloc trackedAlloc
#define free trackedFree
#include <oracle.c>
#undef malloc
#undef free

static size_t attempts, releases, failures, failAt;
static void *live[4];

static void *trackedAlloc(size_t size) {
    ++attempts;
    if (attempts == failAt) {
        ++failures;
        return NULL;
    }
    void *pointer = malloc(size);
    assert(pointer);
    for (size_t slot = 0; slot < 4; ++slot) {
        if (!live[slot]) {
            live[slot] = pointer;
            return pointer;
        }
    }
    abort();
}

static void trackedFree(void *pointer) {
    for (size_t slot = 0; slot < 4; ++slot) {
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
    for (size_t slot = 0; slot < 4; ++slot) {
        assert(!live[slot]);
    }
    assert(attempts == releases + failures);
}

static void allocationFailures(void) {
    uint8_t source[512];
    memset(source, ';', sizeof(source));
    fermi_collectCount_result result = fermi_collectCount(source, sizeof(source));
    assert(result.status == 0 && result.value == sizeof(source) + 1);
    size_t allocations = attempts;
    assert(allocations > 1);
    released();
    for (size_t failure = 1; failure <= allocations; ++failure) {
        failAt = attempts + failure;
        result = fermi_collectCount(source, sizeof(source));
        assert(result.status == 1 && result.code == 0);
        released();
    }
    failAt = 0;
    source[511] = '?';
    result = fermi_collectCount(source, sizeof(source));
    assert(result.status == 1 && result.code == 3);
    released();
    result = fermi_collectCount(source, 0);
    assert(result.status == 0 && result.value == 1);
    released();
    puts("PASS: bootstrap token collection, every allocation failure and partial-prefix cleanup");
}

#define TOKEN_CAPACITY 131072u
#define SOURCE_LIMIT (1024u * 1024u)

static uint64_t *fields;
static size_t cases;
static const char *label;

static void require(bool condition, const char *message) {
    if (!condition) {
        fprintf(stderr, "bootstrap lexer: %s: %s (case %zu)\n", label, message, cases);
        abort();
    }
}

static uint32_t errorCode(const FeContext *context) {
    static const char *messages[] = {
        "source exceeds 1 MiB",         "too many tokens",         "invalid lexer state",
        "NUL byte in source",           "NUL byte in comment",     "unterminated block comment",
        "identifier exceeds 128 bytes", "invalid digit separator", "integer literal exceeds u64",
        "invalid integer suffix",       "invalid string literal",  "invalid string escape",
        "unterminated string literal",  "unknown string escape",   "unsupported character",
    };
    for (uint32_t index = 0; index < sizeof(messages) / sizeof(messages[0]); ++index) {
        if (!strcmp(context->diagnostic.message, messages[index])) {
            return index;
        }
    }
    fprintf(stderr, "unmapped host diagnostic: %s\n", context->diagnostic.message);
    abort();
}

static size_t rawLength(const unsigned char *source, size_t length, const FeToken *token) {
    if (token->kind != FE_T_STRING) {
        return token->length;
    }
    size_t end = token->span.offset + 1;
    while (end < length && source[end] != '"') {
        if (source[end] == '\\') {
            ++end;
        }
        ++end;
    }
    assert(end < length);
    return end + 1 - token->span.offset;
}

static void compare(const unsigned char *source, size_t length) {
    FeContext context;
    FeTokens tokens = {0};
    bool valid = fe_context_init(&context, label, (const char *)source, length);
    if (valid) {
        valid = fe_lex(&context, &tokens);
    }
    fields[0] = fields[1] = fields[2] = UINT64_MAX;
    fermi_tokenize_result actual = fermi_tokenize(source, length, fields, TOKEN_CAPACITY * 7);
    require(actual.status == (valid ? 0u : 1u), "acceptance differs or Fermi runtime fault");
    if (valid) {
        require(actual.value == tokens.count, "token count differs");
        for (size_t index = 0; index < tokens.count; ++index) {
            const FeToken *token = &tokens.items[index];
            uint64_t expected[] = {(uint64_t)token->kind,
                                   token->span.offset,
                                   token->span.line,
                                   token->span.column,
                                   rawLength(source, length, token),
                                   token->length,
                                   token->integer};
            for (size_t field = 0; field < 7; ++field) {
                if (fields[index * 7 + field] != expected[field]) {
                    fprintf(stderr, "token %zu, field %zu: got %llu, expected %llu\n", index, field,
                            (unsigned long long)fields[index * 7 + field],
                            (unsigned long long)expected[field]);
                    require(false, "token data differs");
                }
            }
        }
    } else {
        require(actual.code == errorCode(&context), "diagnostic category differs");
        require(fields[0] == context.diagnostic.span.offset, "error byte offset differs");
        require(fields[1] == context.diagnostic.span.line, "error line differs");
        require(fields[2] == context.diagnostic.span.column, "error column differs");
    }
    fe_context_destroy(&context);
    ++cases;
}

static void text(const char *source) {
    label = source;
    compare((const unsigned char *)source, strlen(source));
}

static uint32_t nextRandom(uint32_t *state) {
    *state = *state * UINT32_C(1664525) + UINT32_C(1013904223);
    return *state;
}

static void generated(void) {
    unsigned char source[4096];
    uint32_t state = 17;
    label = "deterministic byte input";
    for (size_t iteration = 0; iteration < 4096; ++iteration) {
        size_t length = nextRandom(&state) % 257;
        for (size_t index = 0; index < length; ++index) {
            source[index] = (unsigned char)(nextRandom(&state) >> 24);
        }
        compare(source, length);
    }
    static const char *pieces[] = {
        "func",
        "name_1",
        "18446744073709551615",
        "0_0",
        "\"café\\n\\\"\"",
        "/* comment */",
        "// line\n",
        "->",
        "==",
        "!=",
        "<=",
        ">=",
        "&&",
        "||",
        "+=",
        "-=",
        "*=",
        "/=",
        "%=",
        "..",
        "<<",
        ">>",
        "{}()[];,:.@+-*/%!=<>&|^~",
        "pub native export use enum struct type let var if else for while return try",
        "break continue match case read edit true false",
    };
    label = "deterministic token input";
    for (size_t iteration = 0; iteration < 1024; ++iteration) {
        size_t length = 0;
        for (size_t part = 0; part < 32; ++part) {
            const char *piece = pieces[nextRandom(&state) % (sizeof(pieces) / sizeof(pieces[0]))];
            size_t size = strlen(piece);
            assert(length + size + 1 <= sizeof(source));
            memcpy(source + length, piece, size);
            length += size;
            source[length++] = part % 3 ? ' ' : '\n';
        }
        compare(source, length);
    }
    label = "all two-byte inputs";
    for (unsigned first = 0; first < 256; ++first) {
        for (unsigned second = 0; second < 256; ++second) {
            source[0] = (unsigned char)first;
            source[1] = (unsigned char)second;
            compare(source, 2);
        }
    }
}

static void boundaries(void) {
    unsigned char *source = malloc(SOURCE_LIMIT + 1);
    assert(source);
    memset(source, 'a', 129);
    label = "identifier at limit";
    compare(source, 128);
    label = "identifier beyond limit";
    compare(source, 129);
    memset(source, ';', TOKEN_CAPACITY);
    label = "token count at limit";
    compare(source, TOKEN_CAPACITY - 1);
    label = "token count beyond limit";
    compare(source, TOKEN_CAPACITY);
    memset(source, ' ', SOURCE_LIMIT + 1);
    label = "source at limit";
    compare(source, SOURCE_LIMIT);
    label = "source beyond limit";
    compare(source, SOURCE_LIMIT + 1);
    free(source);
}

static void file(const char *path) {
    FILE *input = fopen(path, "rb");
    assert(input);
    unsigned char *source = malloc(SOURCE_LIMIT + 1);
    assert(source);
    size_t length = fread(source, 1, SOURCE_LIMIT + 1, input);
    assert(!ferror(input) && feof(input));
    assert(fclose(input) == 0);
    label = path;
    compare(source, length);
    free(source);
}

int main(int argc, char **argv) {
    allocationFailures();
    fields = malloc(TOKEN_CAPACITY * 7 * sizeof(*fields));
    assert(fields);
    text("");
    text(" \t\r\n");
    text("18446744073709551615 0_0 1_234");
    text("18446744073709551616");
    text("1_ 1__0");
    text("42name");
    text("/* /* not nested */x");
    text("/* unfinished");
    text("// final comment");
    text("\"\\n\\r\\t\\\"\\\\café\"");
    text("\"\\q\"");
    text("\"\\q");
    text("\"\\q\n");
    text("\"\\");
    text("\n\r\n @ ?");
    static const unsigned char nul_comment[] = {'/', '*', 0, '*', '/'};
    static const unsigned char nul_string[] = {'"', 'x', 0, '"'};
    label = "NUL in comment";
    compare(nul_comment, sizeof(nul_comment));
    label = "NUL in string";
    compare(nul_string, sizeof(nul_string));
    generated();
    boundaries();
    for (int index = 1; index < argc; ++index) {
        file(argv[index]);
    }
    printf("PASS: bootstrap lexer, %zu differential inputs, token fields and error locations\n",
           cases);
    free(fields);
    return 0;
}
