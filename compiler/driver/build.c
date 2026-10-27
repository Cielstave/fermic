#define _POSIX_C_SOURCE 200809L
#include "fermi/driver.h"
#include "fermi/hash.h"

#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

static bool path_join(char *out, size_t size, const char *dir, const char *stem,
                      const char *suffix) {
    int n = snprintf(out, size, "%s/%s%s", dir, stem, suffix);
    return n >= 0 && (size_t)n < size;
}
int fe_build_library(FeProgram *p, const char *source, const char *dir, const char *optimization) {
    bool exported = false;
    for (FeFunction *fn = p->functions; fn; fn = fn->next) {
        exported = exported || fn->exported;
    }
    if (!exported) {
        fe_error(p->ctx, (FeSpan){p->ctx->length, 0, 0}, "E_EXPORT",
                 "C ABI library has no exported functions");
        snprintf(p->ctx->diagnostic.help, sizeof(p->ctx->diagnostic.help),
                 "Use export func to expose a C ABI function. Fermi modules use pub and local "
                 "imports; validate them with 'fermi check FILE.fe'. No main function is required "
                 "for a module or C ABI library.");
        return 1;
    }
    const char *base = strrchr(source, '/');
    base = base ? base + 1 : source;
    size_t len = strlen(base);
    if (len < 4 || strcmp(base + len - 3, ".fe") || len > 128) {
        fe_error(p->ctx, (FeSpan){0, 1, 1}, "E_PATH",
                 "build requires a plain basename ending in .fe");
        return 2;
    }
    char stem[129];
    memcpy(stem, base, len - 3);
    stem[len - 3] = 0;
    /* Artifact names are deliberately plain; output-directory paths may contain spaces. */
    for (size_t i = 0; i < len - 3; ++i) {
        if (!((stem[i] >= 'a' && stem[i] <= 'z') || (stem[i] >= 'A' && stem[i] <= 'Z') ||
              (stem[i] >= '0' && stem[i] <= '9') || stem[i] == '_' || stem[i] == '-')) {
            fe_error(p->ctx, (FeSpan){0, 1, 1}, "E_PATH",
                     "source basename must contain only letters, digits, '_' or '-'");
            return 2;
        }
    }
    if (mkdir(dir, 0777) && errno != EEXIST) {
        fe_error(p->ctx, (FeSpan){0, 1, 1}, "E_IO", "%s: %s", dir, strerror(errno));
        return 1;
    }
    char stage[4096];
    if (snprintf(stage, sizeof(stage), "%s/.fermi-XXXXXX", dir) >= (int)sizeof(stage) ||
        !mkdtemp(stage)) {
        fe_error(p->ctx, (FeSpan){0, 1, 1}, "E_IO", "cannot create staging directory: %s",
                 strerror(errno));
        return 1;
    }
    const char *suffix[] = {".c", ".h", ".contract.json", ".o"};
    char paths[4][4096];
    int result = 1;
    for (unsigned i = 0; i < 4; ++i) {
        if (!path_join(paths[i], sizeof(paths[i]), stage, stem, suffix[i])) {
            goto cleanup_dir;
        }
    }
    FILE *c = fopen(paths[0], "w"), *h = fopen(paths[1], "w"), *m = fopen(paths[2], "w");
    bool ok = c && h && m;
    if (ok) {
        ok = fe_emit_library(p, c, h, m);
    }
    if (c && fclose(c)) {
        ok = false;
    }
    if (h && fclose(h)) {
        ok = false;
    }
    if (m && fclose(m)) {
        ok = false;
    }
    if (!ok) {
        fe_error(p->ctx, (FeSpan){0, 1, 1}, "E_IO", "cannot write generated files");
        goto cleanup;
    }
    const char *cc = getenv("FERMI_CC");
    if (!cc || !*cc) {
        cc = "clang";
    }
    char triple[128];
    if (!fe_backend_target(cc, triple)) {
        fe_error(p->ctx, (FeSpan){0, 1, 1}, "E_TOOLCHAIN",
                 "cannot determine backend target; check FERMI_CC");
        goto cleanup;
    }
    char *argv[] = {(char *)cc,
                    "-std=c11",
                    "-Wall",
                    "-Wextra",
                    "-Werror",
                    "-Wno-unused-function",
                    "-Wno-unused-but-set-variable",
                    (char *)optimization,
                    "-c",
                    "-o",
                    paths[3],
                    "--",
                    paths[0],
                    NULL};
    if (!fe_run_tool(p->ctx, argv)) {

        goto cleanup;
    }
    char object_hash[65], source_hash[65];
    if (!fe_sha256_file(paths[3], object_hash)) {
        goto cleanup;
    }
    fe_sha256_bytes(p->ctx->source, p->ctx->length, source_hash);
    m = fopen(paths[2], "r+");
    if (!m) {
        goto cleanup;
    }
    ok = fseek(m, -2, SEEK_END) == 0;
    if (ok) {
        fprintf(m,
                ", \"backend_target\": \"%s\", \"object_sha256\": \"%s\", \"source_sha256\": "
                "\"%s\"\n}\n",
                triple, object_hash, source_hash);
    }
    if (ferror(m)) {
        ok = false;
    }
    if (fclose(m)) {
        ok = false;
    }
    if (!ok) {
        goto cleanup;
    }
    /* Bind every imported source snapshot, not merely the entrypoint, to this artifact. */
    if (p->modules) {
        m = fopen(paths[2], "r+");
        if (!m) {
            goto cleanup;
        }
        ok = fseek(m, -2, SEEK_END) == 0;
        if (ok) {
            fputs(", \"modules\": [", m);
            bool first = true;
            for (FeModule *module = p->modules; module; module = module->next) {
                char hash[65];
                fe_sha256_bytes(module->ctx->source, module->ctx->length, hash);
                fprintf(m, "%s{\"name\":\"%s\",\"source_sha256\":\"%s\"}", first ? "" : ",",
                        module->name, hash);
                first = false;
            }
            fputs("]\n}\n", m);
        }
        if (ferror(m)) {
            ok = false;
        }
        if (fclose(m)) {
            ok = false;
        }
        if (!ok) {
            goto cleanup;
        }
    }
    /* The manifest is published last, after native compilation succeeds. */
    const unsigned order[] = {0, 1, 3, 2};
    for (unsigned j = 0; j < 4; ++j) {
        unsigned i = order[j];
        char destination[4096];
        if (!path_join(destination, sizeof(destination), dir, stem, suffix[i]) ||
            rename(paths[i], destination)) {
            fe_error(p->ctx, (FeSpan){0, 1, 1}, "E_IO", "cannot publish artifact: %s",
                     strerror(errno));
            goto cleanup;
        }
    }
    printf("built %s/%s.o, .h, .contract.json\n", dir, stem);
    result = 0;
cleanup:
    for (unsigned i = 0; i < 4; ++i) {
        unlink(paths[i]);
    }
cleanup_dir:
    rmdir(stage);
    if (result && !p->ctx->diagnostic.code) {
        fe_error(p->ctx, (FeSpan){0, 1, 1}, "E_IO", "failed to generate library artifacts");
    }
    return result;
}
