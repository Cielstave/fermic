#define _XOPEN_SOURCE 700
#include "fermi/driver.h"

#include <errno.h>
#include <stdlib.h>
#include <string.h>

#define MODULE_LIMIT 64

typedef struct {
    FeProgram *program;
    FeContext *root;
    char directory[4096];
    size_t bytes;
    unsigned count, depth;
    FeModule *stack[MODULE_LIMIT];
} Loader;

static bool module_error(Loader *loader, FeContext *context, FeSpan span, const char *message) {
    loader->program->ctx = context;
    return fe_error(context, span, "E_MODULE", "%s", message);
}

static bool parse_unit(Loader *, FeModule *);

static bool is_standard_module(const char *name) {
    static const char *modules[] = {
        "std.io",  "std.mem",  "std.env", "std.num",  "std.config", "std.binary", "std.args",
        "std.fmt", "std.time", "std.fs",  "std.bits", "std.text",   "std.buffer", "std.slice"};
    for (unsigned index = 0; index < sizeof(modules) / sizeof(modules[0]); ++index) {
        if (!strcmp(name, modules[index])) {
            return true;
        }
    }
    return false;
}

static FeModule *load_import(Loader *loader, FeModule *parent, const char *name, bool path_import,
                             FeSpan span) {
    char path[4096], standard_root[4096], base[4096];
    bool standard = !path_import && is_standard_module(name);
    const char *root = loader->directory;
    const char *directory = root;
    if (standard) {
        char *resolved_root = realpath(FERMI_STDLIB_ROOT, NULL);
        if (!resolved_root || strlen(resolved_root) >= sizeof(standard_root)) {
            free(resolved_root);
            module_error(loader, parent->ctx, span,
                         "cannot resolve the standard library directory");
            return NULL;
        }
        strcpy(standard_root, resolved_root);
        free(resolved_root);
        directory = standard_root;
        root = standard_root;
    }
    int length;
    if (path_import) {
        if (!name[0] || name[0] == '/') {
            module_error(loader, parent->ctx, span,
                         "local import path must be a nonempty relative path");
            return NULL;
        }
        if (strlen(name) >= FE_MODULE_NAME_LIMIT) {
            module_error(loader, parent->ctx, span, "import path exceeds the path limit");
            return NULL;
        }
        for (const unsigned char *byte = (const unsigned char *)name; *byte; ++byte) {
            if (*byte < 0x20 || *byte == 0x7f) {
                module_error(loader, parent->ctx, span,
                             "local import path cannot contain control characters");
                return NULL;
            }
        }
        strcpy(base, parent->path);
        char *slash = strrchr(base, '/');
        if (!slash) {
            module_error(loader, parent->ctx, span, "cannot determine importing file directory");
            return NULL;
        }
        if (slash == base) {
            slash[1] = 0;
        } else {
            *slash = 0;
        }
        length = snprintf(path, sizeof(path), "%s/%s", base, name);
    } else {
        length = snprintf(path, sizeof(path), "%s/%s.fe", directory, name);
        if (length >= 0 && (size_t)length < sizeof(path)) {
            size_t prefix = strlen(directory) + 1;
            for (size_t i = prefix; i + 3 < (size_t)length; ++i) {
                if (path[i] == '.') {
                    path[i] = '/';
                }
            }
        }
    }
    if (length < 0 || (size_t)length >= sizeof(path)) {
        module_error(loader, parent->ctx, span, "import path exceeds the path limit");
        return NULL;
    }
    char *resolved = realpath(path, NULL);
    if (!resolved) {
        loader->program->ctx = parent->ctx;
        fe_error(parent->ctx, span, "E_MODULE", "cannot resolve import '%s': %s", name,
                 strerror(errno));
        return NULL;
    }
    size_t root_length = strlen(root);
    bool inside =
        !strncmp(resolved, root, root_length) && (root_length == 1 || resolved[root_length] == '/');
    if (!inside) {
        free(resolved);
        module_error(loader, parent->ctx, span, "import resolves outside the project directory");
        return NULL;
    }
    for (FeModule *module = loader->program->modules; module; module = module->next) {
        if (!strcmp(module->path, resolved)) {
            free(resolved);
            if (module->loading) {
                loader->program->ctx = parent->ctx;
                fe_error(parent->ctx, span, "E_MODULE_CYCLE",
                         "import cycle: '%s' refers to active module '%s'", parent->name,
                         module->name);
                size_t used = 0;
                char *help = parent->ctx->diagnostic.help;
                size_t capacity = sizeof(parent->ctx->diagnostic.help);
                for (unsigned i = 0; i < loader->depth; ++i) {
                    int written = snprintf(help + used, capacity - used, "%s%s", i ? " -> " : "",
                                           loader->stack[i]->name);
                    if (written < 0 || (size_t)written >= capacity - used) {
                        break;
                    }
                    used += (size_t)written;
                }
                if (used < capacity) {
                    snprintf(help + used, capacity - used, " -> %s", module->name);
                }
                return NULL;
            }
            return module;
        }
    }
    if (loader->count >= MODULE_LIMIT) {
        free(resolved);
        module_error(loader, parent->ctx, span, "project exceeds 64 modules");
        return NULL;
    }
    FeModule *module = fe_alloc(loader->root, sizeof(*module));
    FeContext *context = fe_alloc(loader->root, sizeof(*context));
    if (!module || !context) {
        free(resolved);
        loader->program->ctx = loader->root;
        return NULL;
    }
    module->path = fe_string(loader->root, resolved, strlen(resolved));
    const char *module_name = name;
    if (path_import) {
        const char *relative = resolved + root_length;
        if (*relative == '/') {
            ++relative;
        }
        size_t name_length = strlen(relative) + sizeof("local.");
        char *qualified = fe_alloc(loader->root, name_length);
        if (!qualified) {
            free(resolved);
            loader->program->ctx = loader->root;
            return NULL;
        }
        snprintf(qualified, name_length, "local.%s", relative);
        module_name = qualified;
    }
    free(resolved);
    if (!module->path) {
        loader->program->ctx = loader->root;
        return NULL;
    }
    module->ctx = context;
    module->name = module_name;
    module->standard = standard;
    fe_context_init(context, module->path, "", 0);
    context->budget_owner = loader->root;
    module->next = loader->program->modules;
    loader->program->modules = module;
    ++loader->count;
    loader->program->ctx = context;
    FILE *input = fe_open_source(context, module->path);
    if (!input) {
        return NULL;
    }
    size_t remaining = FE_SOURCE_LIMIT - loader->bytes;
    char *source = malloc(remaining + 1);
    if (!source) {
        fclose(input);
        fe_error(context, (FeSpan){0, 1, 1}, "E_LIMIT", "cannot allocate module source");
        return NULL;
    }
    size_t bytes = fread(source, 1, remaining + 1, input);
    bool ok = !ferror(input);
    if (fclose(input)) {
        ok = false;
    }
    if (!ok || bytes > remaining) {
        free(source);
        fe_error(context, (FeSpan){0, 1, 1}, "E_LIMIT",
                 "cannot read module within the 1 MiB project source limit");
        return NULL;
    }
    char *owned = fe_string(context, source, bytes);
    free(source);
    if (!owned) {
        return NULL;
    }
    context->source = owned;
    context->length = bytes;
    loader->bytes += bytes;
    if (!parse_unit(loader, module)) {
        return NULL;
    }
    return module;
}

static bool parse_unit(Loader *loader, FeModule *module) {
    FeTokens tokens;
    loader->program->ctx = module->ctx;
    module->loading = true;
    loader->stack[loader->depth++] = module;
    if (!fe_lex(module->ctx, &tokens)) {
        return false;
    }
    size_t pos = 0;
    while (tokens.items[pos].kind == FE_T_USE) {
        FeSpan span = tokens.items[pos++].span;
        char name[FE_MODULE_NAME_LIMIT], alias[FE_MODULE_NAME_LIMIT];
        size_t length = 0, alias_start = 0;
        bool path_import = tokens.items[pos].kind == FE_T_STRING;
        if (path_import) {
            FeToken *path = &tokens.items[pos++];
            if (!path->length || path->length >= sizeof(name)) {
                return module_error(loader, module->ctx, path->span,
                                    "local import path is empty or exceeds the path limit");
            }
            memcpy(name, path->text, path->length);
            length = path->length;
            name[length] = 0;
            const char *base = strrchr(name, '/');
            base = base ? base + 1 : name;
            alias_start = (size_t)(base - name);
            size_t base_length = strlen(base);
            if (base_length > 3 && !strcmp(base + base_length - 3, ".fe")) {
                base_length -= 3;
            }
            if (!base_length || base_length >= sizeof(alias)) {
                return module_error(loader, module->ctx, path->span,
                                    "cannot infer import alias; add 'as identifier'");
            }
            memcpy(alias, base, base_length);
            alias[base_length] = 0;
            if (tokens.items[pos].kind == FE_T_NAME && tokens.items[pos].length == 2 &&
                !memcmp(tokens.items[pos].text, "as", 2)) {
                ++pos;
                FeToken *explicit_alias = &tokens.items[pos++];
                if (explicit_alias->kind != FE_T_NAME || explicit_alias->length >= sizeof(alias)) {
                    return module_error(loader, module->ctx, explicit_alias->span,
                                        "expected import alias after 'as'");
                }
                memcpy(alias, explicit_alias->text, explicit_alias->length);
                alias[explicit_alias->length] = 0;
            }
        } else {
            for (;;) {
                FeToken *part = &tokens.items[pos];
                if (part->kind != FE_T_NAME || length + part->length + 2 > sizeof(name)) {
                    return module_error(
                        loader, module->ctx, part->span,
                        "use requires a dotted module name or quoted relative path");
                }
                alias_start = length;
                memcpy(name + length, part->text, part->length);
                length += part->length;
                ++pos;
                if (tokens.items[pos].kind != '.') {
                    break;
                }
                name[length++] = '.';
                ++pos;
            }
            name[length] = 0;
            snprintf(alias, sizeof(alias), "%s", name + alias_start);
        }
        if (tokens.items[pos].kind != ';') {
            return module_error(loader, module->ctx, tokens.items[pos].span,
                                "expected ';' after import");
        }
        ++pos;
        if (!path_import && (!strcmp(name, "std") || !strncmp(name, "std.", 4)) &&
            !is_standard_module(name)) {
            return module_error(loader, module->ctx, span,
                                "requested standard library module is not available");
        }
        static const char *reserved[] = {"i8",    "u16",    "u64",       "void", "bool",
                                         "u8",    "i16",    "i32",       "u32",  "i64",
                                         "usize", "Result", "CastError", "copy", "move",
                                         "widen", "narrow", "Buffer",    "str"};
        for (size_t i = 0; i < sizeof(reserved) / sizeof(reserved[0]); ++i) {
            if (!strcmp(alias, reserved[i])) {
                return module_error(loader, module->ctx, span,
                                    "module alias conflicts with a prelude name");
            }
        }
        for (FeImport *old = module->imports; old; old = old->next) {
            if (!strcmp(old->alias, alias)) {
                return module_error(loader, module->ctx, span, "duplicate import alias");
            }
        }
        FeImport *use = fe_alloc(module->ctx, sizeof(*use));
        const char *owned_name = fe_string(module->ctx, name, length);
        const char *owned_alias = fe_string(module->ctx, alias, strlen(alias));
        if (!use || !owned_name || !owned_alias) {
            loader->program->ctx = module->ctx;
            return false;
        }
        use->alias = owned_alias;
        use->span = span;
        use->module = load_import(loader, module, owned_name, path_import, span);
        if (!use->module) {
            return false;
        }
        use->next = module->imports;
        module->imports = use;
    }
    if (!fe_parse_module(module->ctx, tokens, loader->program, module, pos)) {
        return false;
    }
    module->loading = false;
    --loader->depth;
    return true;
}

bool fe_load_project(FeContext *root, FeProgram *program) {
    memset(program, 0, sizeof(*program));
    program->ctx = root;
    Loader loader = {.program = program, .root = root, .bytes = root->length, .count = 1};
    char *resolved = realpath(root->path, NULL);
    if (!resolved) {
        return fe_error(root, (FeSpan){0, 1, 1}, "E_IO", "cannot resolve entrypoint: %s",
                        strerror(errno));
    }
    if (strlen(resolved) >= sizeof(loader.directory)) {
        free(resolved);
        return module_error(&loader, root, (FeSpan){0, 1, 1}, "project directory is too long");
    }
    strcpy(loader.directory, resolved);
    char *slash = strrchr(loader.directory, '/');
    if (slash == loader.directory) {
        slash[1] = 0;
    } else {
        *slash = 0;
    }
    FeModule *entry = fe_alloc(root, sizeof(*entry));
    if (!entry) {
        free(resolved);
        return false;
    }
    entry->path = fe_string(root, resolved, strlen(resolved));
    free(resolved);
    if (!entry->path) {
        return false;
    }
    entry->ctx = root;
    entry->name = "<entry>";
    program->modules = entry;
    if (!parse_unit(&loader, entry)) {
        return false;
    }
    program->ctx = root;
    program->module = entry;
    for (FeAlias *alias = program->aliases; alias; alias = alias->next) {
        for (FeImport *use = alias->module->imports; use; use = use->next) {
            if (!strcmp(use->alias, alias->name)) {
                return module_error(&loader, alias->module->ctx, alias->span,
                                    "type alias conflicts with an import alias");
            }
        }
    }
    for (FeType *type = program->types; type; type = type->next) {
        if (!type->module || !type->name) {
            continue;
        }
        // Reserve the generated imported-enum macro prefix to avoid C-header collisions.
        if (type->module == entry && !strncmp(type->name, "module_", 7)) {
            return module_error(&loader, root, (FeSpan){0, 1, 1},
                                "entrypoint type names beginning with module_ are reserved for "
                                "imported C ABI names");
        }
        for (FeImport *use = type->module->imports; use; use = use->next) {
            if (!strcmp(use->alias, type->name)) {
                return module_error(&loader, type->module->ctx, use->span,
                                    "type conflicts with an import alias");
            }
        }
    }
    for (FeFunction *f = program->functions; f; f = f->next) {
        for (FeImport *use = f->module->imports; use; use = use->next) {
            if (!strcmp(use->alias, f->name)) {
                return module_error(&loader, f->module->ctx, f->span,
                                    "function conflicts with an import alias");
            }
        }
        for (FeFunction *other = f->next; other; other = other->next) {
            if (f->exported && other->exported && !strcmp(f->name, other->name)) {
                return module_error(&loader, other->module->ctx, other->span,
                                    "duplicate exported C symbol");
            }
        }
    }
    return true;
}

void fe_destroy_project(FeProgram *program) {
    for (FeModule *module = program->modules; module; module = module->next) {
        if (module->ctx->budget_owner) {
            fe_context_destroy(module->ctx);
        }
    }
}
