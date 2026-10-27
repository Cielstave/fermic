#include "fermi/lex.h"

#include <string.h>

typedef struct {
    FeContext *context;
    size_t offset;
    unsigned line;
    unsigned column;
} Lexer;

static bool is_identifier_start(unsigned char ch) {
    return (ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z') || ch == '_';
}

static bool is_digit(unsigned char ch) {
    return ch >= '0' && ch <= '9';
}

static unsigned char peek_byte(const Lexer *lexer, size_t ahead) {
    size_t offset = lexer->offset + ahead;
    if (offset >= lexer->context->length) {
        return 0;
    }
    return (unsigned char)lexer->context->source[offset];
}

static void advance(Lexer *lexer) {
    if (peek_byte(lexer, 0) == '\n') {
        ++lexer->line;
        lexer->column = 1;
    } else {
        ++lexer->column;
    }
    ++lexer->offset;
}

static FeSpan current_span(const Lexer *lexer) {
    return (FeSpan){lexer->offset, lexer->line, lexer->column};
}

static bool lex_error(Lexer *lexer, FeSpan span, const char *message) {
    return fe_error(lexer->context, span, "E_LEX", "%s", message);
}

static int keyword_kind(const char *text, size_t length) {
    static const struct {
        const char *name;
        int kind;
    } keywords[] = {
        {"use", FE_T_USE},       {"pub", FE_T_PUB},           {"func", FE_T_FUNC},
        {"export", FE_T_EXPORT}, {"enum", FE_T_ENUM},         {"struct", FE_T_STRUCT},
        {"let", FE_T_LET},       {"var", FE_T_VAR},           {"if", FE_T_IF},
        {"else", FE_T_ELSE},     {"for", FE_T_FOR},           {"while", FE_T_WHILE},
        {"break", FE_T_BREAK},   {"continue", FE_T_CONTINUE}, {"return", FE_T_RETURN},
        {"match", FE_T_MATCH},   {"case", FE_T_CASE},         {"read", FE_T_READ},
        {"edit", FE_T_EDIT},     {"true", FE_T_TRUE},         {"false", FE_T_FALSE},
        {"native", FE_T_NATIVE}, {"try", FE_T_TRY},           {"type", FE_T_TYPE},
    };

    for (size_t i = 0; i < sizeof(keywords) / sizeof(keywords[0]); ++i) {
        if (strlen(keywords[i].name) == length && !memcmp(keywords[i].name, text, length)) {
            return keywords[i].kind;
        }
    }
    return FE_T_NAME;
}

static bool skip_comment(Lexer *lexer) {
    FeSpan start = current_span(lexer);
    bool block = peek_byte(lexer, 1) == '*';
    advance(lexer);
    advance(lexer);

    while (lexer->offset < lexer->context->length) {
        unsigned char ch = peek_byte(lexer, 0);
        if (!ch) {
            return lex_error(lexer, start, "NUL byte in comment");
        }
        if (!block && ch == '\n') {
            return true;
        }
        if (block && ch == '*' && peek_byte(lexer, 1) == '/') {
            advance(lexer);
            advance(lexer);
            return true;
        }
        advance(lexer);
    }
    return !block || lex_error(lexer, start, "unterminated block comment");
}

static bool scan_identifier(Lexer *lexer, FeToken *token) {
    do {
        advance(lexer);
    } while (is_identifier_start(peek_byte(lexer, 0)) || is_digit(peek_byte(lexer, 0)));

    token->length = lexer->offset - token->span.offset;
    if (token->length > 128) {
        return lex_error(lexer, token->span, "identifier exceeds 128 bytes");
    }
    token->kind = keyword_kind(token->text, token->length);
    return true;
}

static bool scan_integer(Lexer *lexer, FeToken *token) {
    bool previous_digit = false;
    unsigned char ch = peek_byte(lexer, 0);

    while (is_digit(ch) || ch == '_') {
        if (ch == '_') {
            if (!previous_digit || !is_digit(peek_byte(lexer, 1))) {
                return lex_error(lexer, token->span, "invalid digit separator");
            }
            previous_digit = false;
        } else {
            unsigned digit = ch - '0';
            if (token->integer > (UINT64_MAX - digit) / 10) {
                return lex_error(lexer, token->span, "integer literal exceeds u64");
            }
            token->integer = token->integer * 10 + digit;
            previous_digit = true;
        }
        advance(lexer);
        ch = peek_byte(lexer, 0);
    }
    if (is_identifier_start(ch)) {
        return lex_error(lexer, token->span, "invalid integer suffix");
    }
    token->length = lexer->offset - token->span.offset;
    token->kind = FE_T_INTEGER;
    return true;
}

static bool scan_string(Lexer *lexer, FeToken *token) {
    advance(lexer);
    size_t start = lexer->offset;
    while (lexer->offset < lexer->context->length && peek_byte(lexer, 0) != '"') {
        unsigned char ch = peek_byte(lexer, 0);
        if (!ch || ch == '\n' || ch == '\r') {
            return lex_error(lexer, token->span, "invalid string literal");
        }
        if (ch == '\\') {
            advance(lexer);
            ch = peek_byte(lexer, 0);
            if (!ch || ch == '\n' || ch == '\r') {
                return lex_error(lexer, token->span, "invalid string escape");
            }
        }
        advance(lexer);
    }
    if (lexer->offset == lexer->context->length) {
        return lex_error(lexer, token->span, "unterminated string literal");
    }

    char *value = fe_alloc(lexer->context, lexer->offset - start + 1);
    if (!value) {
        return false;
    }
    size_t length = 0;
    const char *source = lexer->context->source;
    for (size_t i = start; i < lexer->offset; ++i) {
        char ch = source[i];
        if (ch == '\\') {
            ch = source[++i];
            switch (ch) {
            case 'n':
                ch = '\n';
                break;
            case 'r':
                ch = '\r';
                break;
            case 't':
                ch = '\t';
                break;
            case '"':
            case '\\':
                break;
            default:
                return lex_error(lexer, token->span, "unknown string escape");
            }
        }
        value[length++] = ch;
    }
    token->kind = FE_T_STRING;
    token->text = value;
    token->length = length;
    advance(lexer);
    return true;
}

static bool scan_punctuation(Lexer *lexer, FeToken *token) {
    static const struct {
        const char *spelling;
        int kind;
    } pairs[] = {
        {"->", FE_T_ARROW},      {"==", FE_T_EQ},         {"!=", FE_T_NE},
        {"<=", FE_T_LE},         {">=", FE_T_GE},         {"&&", FE_T_AND},
        {"||", FE_T_OR},         {"+=", FE_T_ADD_ASSIGN}, {"-=", FE_T_SUB_ASSIGN},
        {"*=", FE_T_MUL_ASSIGN}, {"..", FE_T_RANGE},      {"<<", FE_T_SHL},
        {">>", FE_T_SHR},        {"/=", FE_T_DIV_ASSIGN}, {"%=", FE_T_REM_ASSIGN},
    };

    unsigned char ch = peek_byte(lexer, 0);
    for (size_t i = 0; i < sizeof(pairs) / sizeof(pairs[0]); ++i) {
        if (ch == (unsigned char)pairs[i].spelling[0] &&
            peek_byte(lexer, 1) == (unsigned char)pairs[i].spelling[1]) {
            token->kind = pairs[i].kind;
            token->length = 2;
            advance(lexer);
            advance(lexer);
            return true;
        }
    }
    if (!strchr("{}()[];,:.@+-*/%!=<>&|^~", ch)) {
        return lex_error(lexer, token->span, "unsupported character");
    }
    token->kind = ch;
    token->length = 1;
    advance(lexer);
    return true;
}

bool fe_lex(FeContext *context, FeTokens *tokens) {
    size_t capacity = context->length + 1;
    if (capacity > 131072) {
        capacity = 131072;
    }
    tokens->items = fe_alloc(context, capacity * sizeof(FeToken));
    if (!tokens->items) {
        return false;
    }
    tokens->count = 0;
    Lexer lexer = {.context = context, .line = 1, .column = 1};

    while (lexer.offset < context->length) {
        unsigned char ch = peek_byte(&lexer, 0);
        if (!ch) {
            return lex_error(&lexer, current_span(&lexer), "NUL byte in source");
        }
        if (ch == ' ' || ch == '\t' || ch == '\r' || ch == '\n') {
            advance(&lexer);
            continue;
        }
        if (ch == '/' && (peek_byte(&lexer, 1) == '/' || peek_byte(&lexer, 1) == '*')) {
            if (!skip_comment(&lexer)) {
                return false;
            }
            continue;
        }
        if (tokens->count + 1 >= capacity) {
            return fe_error(context, current_span(&lexer), "E_LIMIT", "too many tokens");
        }
        FeToken *token = &tokens->items[tokens->count++];
        token->span = current_span(&lexer);
        token->text = context->source + lexer.offset;

        bool ok;
        if (is_identifier_start(ch)) {
            ok = scan_identifier(&lexer, token);
        } else if (is_digit(ch)) {
            ok = scan_integer(&lexer, token);
        } else if (ch == '"') {
            ok = scan_string(&lexer, token);
        } else {
            ok = scan_punctuation(&lexer, token);
        }
        if (!ok) {
            return false;
        }
    }
    tokens->items[tokens->count++] = (FeToken){.kind = FE_T_EOF, .span = current_span(&lexer)};
    return true;
}
