#include "fermi/fe.h"

#include <string.h>

static unsigned expression_effects(const FeExpr *expression) {
    unsigned effects = 0;
    for (; expression; expression = expression->next) {
        effects |= fe_resource_effects(expression->type);
        if (expression->kind == EX_CALL) {
            effects |= expression->function->effects;
        }
        effects |= expression_effects(expression->a) | expression_effects(expression->b) |
                   expression_effects(expression->c);
    }
    return effects;
}

static unsigned statement_effects(const FeStmt *statement) {
    unsigned effects = 0;
    for (; statement; statement = statement->next) {
        effects |= expression_effects(statement->a) | expression_effects(statement->b) |
                   statement_effects(statement->body) | statement_effects(statement->other) |
                   statement_effects(statement->init) | statement_effects(statement->step);
        for (const FeCase *branch = statement->cases; branch; branch = branch->next) {
            effects |= statement_effects(branch->body);
        }
    }
    return effects;
}

void fe_analyze_effects(FeProgram *program) {
    /* A fixed point includes effects propagated through recursive call graphs. */
    bool changed;
    do {
        changed = false;
        for (FeFunction *function = program->functions; function; function = function->next) {
            unsigned effects = function->effects | statement_effects(function->body);
            for (const FeSymbol *local = function->locals; local; local = local->next) {
                effects |= fe_resource_effects(local->type);
            }
            if (effects != function->effects) {
                function->effects = effects;
                changed = true;
            }
        }
    } while (changed);
}

static const struct {
    unsigned flag;
    const char *name;
} effect_names[] = {
    {FE_EFFECT_IO_READ, "io.read"},        {FE_EFFECT_IO_WRITE, "io.write"},
    {FE_EFFECT_MEMORY, "memory"},          {FE_EFFECT_ENV_READ, "env.read"},
    {FE_EFFECT_ARGS_READ, "process.args"}, {FE_EFFECT_CLOCK_READ, "clock.read"},
    {FE_EFFECT_FS_OPEN, "fs.open"},        {FE_EFFECT_FS_READ, "fs.read"},
    {FE_EFFECT_FS_CLOSE, "fs.close"},      {FE_EFFECT_FS_WRITE, "fs.write"},
    {FE_EFFECT_FS_SYNC, "fs.sync"},
};

unsigned fe_effect_named(const char *name) {
    for (size_t i = 0; i < sizeof(effect_names) / sizeof(effect_names[0]); ++i) {
        if (!strcmp(name, effect_names[i].name)) {
            return effect_names[i].flag;
        }
    }
    return 0;
}

/* Keep the witness in the caller body: related spans use the contract module. */
static const FeExpr *find_effect_call(const FeExpr *expression, unsigned effect) {
    for (; expression; expression = expression->next) {
        const FeExpr *found = find_effect_call(expression->a, effect);
        if (!found) {
            found = find_effect_call(expression->b, effect);
        }
        if (!found) {
            found = find_effect_call(expression->c, effect);
        }
        if (found) {
            return found;
        }
        if (expression->kind == EX_CALL && (expression->function->effects & effect)) {
            return expression;
        }
    }
    return NULL;
}

static const FeExpr *find_statement_effect_call(const FeStmt *statement, unsigned effect) {
    for (; statement; statement = statement->next) {
        const FeExpr *found = find_effect_call(statement->a, effect);
        if (!found) {
            found = find_effect_call(statement->b, effect);
        }
        if (!found) {
            found = find_statement_effect_call(statement->init, effect);
        }
        if (!found) {
            found = find_statement_effect_call(statement->body, effect);
        }
        if (!found) {
            found = find_statement_effect_call(statement->other, effect);
        }
        if (!found) {
            found = find_statement_effect_call(statement->step, effect);
        }
        for (const FeCase *branch = statement->cases; !found && branch; branch = branch->next) {
            found = find_statement_effect_call(branch->body, effect);
        }
        if (found) {
            return found;
        }
    }
    return NULL;
}

/* Permission ceilings restrict inference; unused permissions do not propagate. */
bool fe_check_effects(FeProgram *program) {
    for (const FeFunction *function = program->functions; function; function = function->next) {
        if (!function->effects_declared) {
            continue;
        }
        unsigned forbidden = function->effects & ~function->allowed_effects;
        if (!forbidden) {
            continue;
        }
        if (function->module) {
            program->ctx = function->module->ctx;
        }
        for (size_t i = 0; i < sizeof(effect_names) / sizeof(effect_names[0]); ++i) {
            if (forbidden & effect_names[i].flag) {
                fe_error(program->ctx, function->span, "E_EFFECT",
                         "%s: effect '%s' is not permitted by its @effects contract",
                         function->name, effect_names[i].name);
                snprintf(program->ctx->diagnostic.help, sizeof(program->ctx->diagnostic.help),
                         "permit '%s' explicitly or remove the operation, including reachable "
                         "helpers and automatic resource cleanup",
                         effect_names[i].name);
                const FeExpr *call =
                    find_statement_effect_call(function->body, effect_names[i].flag);
                if (call) {
                    program->ctx->diagnostic.related = call->span;
                    snprintf(program->ctx->diagnostic.note, sizeof(program->ctx->diagnostic.note),
                             "call to '%s' contributes '%s' through its body or cleanup",
                             call->function->name, effect_names[i].name);
                }
                return false;
            }
        }
    }
    return true;
}

void fe_print_effects(FILE *out, unsigned effects) {
    bool first = true;
    fputc('[', out);
    for (size_t i = 0; i < sizeof(effect_names) / sizeof(effect_names[0]); ++i) {
        if (effects & effect_names[i].flag) {
            fprintf(out, "%s\"%s\"", first ? "" : ", ", effect_names[i].name);
            first = false;
        }
    }
    fputc(']', out);
}
