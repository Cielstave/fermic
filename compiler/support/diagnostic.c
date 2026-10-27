#include "fermi/fe.h"

#include <string.h>

/* Encode bytes rather than assuming paths or a rejected source are valid UTF-8.
 * JSON clients can recover original bytes from U+0000..U+00FF code points. */
static void write_json_string(FILE *out, const char *text) {
    fputc('"', out);
    for (const unsigned char *p = (const unsigned char *)text; *p; ++p) {
        if (*p == '"' || *p == '\\') {
            fputc('\\', out);
            fputc(*p, out);
        } else if (*p < 32 || *p >= 127) {
            fprintf(out, "\\u%04x", *p);
        } else {
            fputc(*p, out);
        }
    }
    fputc('"', out);
}

static void write_escaped_text(FILE *out, const char *text) {
    for (const unsigned char *p = (const unsigned char *)text; *p; ++p) {
        if (*p < 32 || *p >= 127) {
            fprintf(out, "\\x%02x", *p);
        } else {
            fputc(*p, out);
        }
    }
}

static void write_location(FILE *out, const FeContext *context, FeSpan span) {
    write_escaped_text(out, context->path);
    if (span.line) {
        fprintf(out, ":%u:%u", span.line, span.column);
    }
}

static void write_json_span(FILE *out, FeSpan span) {
    fprintf(out, "{\"offset\":%zu,\"line\":%u,\"column\":%u}", span.offset, span.line, span.column);
}

static void write_source_excerpt(FILE *out, const FeContext *context, FeSpan span) {
    if (!context->source || span.offset > context->length || !span.line) {
        return;
    }
    size_t start = span.offset;
    while (start && context->source[start - 1] != '\n' && span.offset - start < 80) {
        --start;
    }
    size_t end = span.offset;
    while (end < context->length && context->source[end] != '\n' && end - start < 160) {
        ++end;
    }
    fprintf(out, "%5u | ", span.line);
    size_t caret = 0;
    for (size_t i = start; i < end; ++i) {
        unsigned char ch = (unsigned char)context->source[i];
        size_t width = 1;
        if (ch == '\t') {
            fputs("    ", out);
            width = 4;
        } else if (ch < 32 || ch >= 127) {
            fprintf(out, "\\x%02x", ch);
            width = 4;
        } else {
            fputc(ch, out);
        }
        if (i < span.offset) {
            caret += width;
        }
    }
    fprintf(out, "\n      | %*s^\n", (int)caret, "");
}

void fe_render_diagnostic(FILE *out, const FeContext *context, bool json, bool color) {
    const FeDiagnostic *diagnostic = &context->diagnostic;
    if (!diagnostic->code) {
        return;
    }
    if (json) {
        fputs("{\"schema_version\":1,\"encoding\":\"byte-preserving\",\"severity\":\"error\","
              "\"code\":",
              out);
        write_json_string(out, diagnostic->code);
        fputs(",\"message\":", out);
        write_json_string(out, diagnostic->message);
        fputs(",\"path\":", out);
        write_json_string(out, context->path);
        fputs(",\"primary\":", out);
        write_json_span(out, diagnostic->span);
        fputs(",\"end\":", out);
        write_json_span(out, diagnostic->end);
        fputs(",\"category\":", out);
        write_json_string(out,
                          diagnostic->severity == FE_DIAGNOSTIC_INTERNAL ? "compiler" : "user");
        fputs(",\"related\":", out);
        if (*diagnostic->note) {
            fputs("{\"span\":", out);
            write_json_span(out, diagnostic->related);
            fputs(",\"message\":", out);
            write_json_string(out, diagnostic->note);
            fputc('}', out);
        } else {
            fputs("null", out);
        }
        fputs(",\"help\":", out);
        write_json_string(out, diagnostic->help);
        fputs("}\n", out);
        return;
    }
    write_location(out, context, diagnostic->span);
    fprintf(out, ": %serror[%s]%s: ", color ? "\033[1;31m" : "", diagnostic->code,
            color ? "\033[0m" : "");
    if (diagnostic->severity == FE_DIAGNOSTIC_INTERNAL) {
        fputs("internal compiler error: ", out);
    }
    write_escaped_text(out, diagnostic->message);
    fputc('\n', out);
    write_source_excerpt(out, context, diagnostic->span);
    if (*diagnostic->note) {
        fputs("note: ", out);
        write_escaped_text(out, diagnostic->note);
        fputs(" at ", out);
        write_location(out, context, diagnostic->related);
        fputc('\n', out);
        write_source_excerpt(out, context, diagnostic->related);
    }
    if (*diagnostic->help) {
        fputs("help: ", out);
        write_escaped_text(out, diagnostic->help);
        fputc('\n', out);
    }
}

void fe_print_diagnostic(const FeContext *context) {
    fe_render_diagnostic(stderr, context, false, false);
}
