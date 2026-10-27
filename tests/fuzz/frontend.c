#include "fermi/fe.h"
#include <stddef.h>
#include <stdint.h>
int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
    FeContext c;
    FeTokens tokens = {0};
    FeProgram p;
    if (fe_context_init(&c, "fuzz.fe", (const char *)data, size) && fe_lex(&c, &tokens) &&
        fe_parse(&c, tokens, &p) && fe_check(&p) && fe_analyze(&p)) {
        FILE *a = tmpfile(), *b = tmpfile(), *d = tmpfile();
        if (a && b && d) {
            (void)fe_emit_library(&p, a, b, d);
        }
        if (a) {
            fclose(a);
        }
        if (b) {
            fclose(b);
        }
        if (d) {
            fclose(d);
        }
    }
    fe_context_destroy(&c);
    return 0;
}
