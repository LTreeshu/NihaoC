#include "ncc.h"

/* ============================================================
 * Module Management
 * ============================================================ */

Module *module_add(CompilerState *cs, const char *tok_str, char *input_file)
{
    Module *mod;
    int idx;

    if (!tok_str) return NULL;

    /* Check if module already exists */
    for (int i = 0; i < cs->module_count; i++) {
        if (cs->modules[i].name && strcmp(cs->modules[i].name, tok_str) == 0) {
            return &cs->modules[i];
        }
    }

    /* Check capacity */
    if (cs->module_count >= MAX_MODULES) {
        nihao_error(cs, "too many modules (max %d)", MAX_MODULES);
        return NULL;
    }

    idx = cs->module_count++;
    mod = &cs->modules[idx];

    memset(mod, 0, sizeof(Module));
    mod->name = nihao_strdup(cs, tok_str);
    mod->filename = input_file ? nihao_strdup(cs, input_file) : NULL;
    mod->is_external = 0;
    mod->symbols = NULL;
    mod->symbol_count = 0;

    if (cs->verbose) {
        printf("Module added: %s (%s)\n", mod->name,
               mod->filename ? mod->filename : "(builtin)");
    }

    return mod;
}

Module *module_import(CompilerState *cs, const char *name)
{
    Module *mod;
    char *source;
    size_t source_size;
    char filename[512];

    if (!name) return NULL;

    /* Check if already imported */
    for (int i = 0; i < cs->module_count; i++) {
        if (cs->modules[i].name && strcmp(cs->modules[i].name, name) == 0) {
            return &cs->modules[i];
        }
    }

    /* Try to find module file: name.nc */
    snprintf(filename, sizeof(filename), "%s.nc", name);

    source = load_source_file(filename, &source_size);
    if (!source) {
        /* Try in stdlib path */
        snprintf(filename, sizeof(filename), "stdlib/%s.nc", name);
        source = load_source_file(filename, &source_size);
    }

    if (!source) {
        nihao_warning(cs, "module '%s' not found, treated as external", name);
        mod = module_add(cs, name, NULL);
        if (mod) mod->is_external = 1;
        return mod;
    }

    /* Create module entry */
    mod = module_add(cs, name, filename);
    if (!mod) {
        free(source);
        return NULL;
    }

    /* TODO: Actually parse the imported module.
     * This would require setting up a new lexer/parser context
     * and parsing the module file. For now we just register it.
     */
    if (cs->verbose) {
        printf("Imported module: %s (%zu bytes)\n", name, source_size);
    }

    free(source);
    return mod;
}
