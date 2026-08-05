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

    /* Check if already imported (also covers builtin stdlib modules) */
    for (int i = 0; i < cs->module_count; i++) {
        if (cs->modules[i].name && strcmp(cs->modules[i].name, name) == 0) {
            return &cs->modules[i];
        }
    }

    /* Try to find module file: name.nc (cwd, source dir, then stdlib/).
     * Probe existence first so failed attempts stay silent. */
    char dirbuf[512];
    dirbuf[0] = '\0';
    if (cs->input_file) {
        const char *slash = strrchr(cs->input_file, '/');
        const char *bslash = strrchr(cs->input_file, '\\');
        const char *sep = (bslash && (!slash || bslash > slash)) ? bslash : slash;
        if (sep) {
            int len = (int)(sep - cs->input_file);
            if (len > 0 && len < (int)sizeof(dirbuf) - 1) {
                memcpy(dirbuf, cs->input_file, len);
                dirbuf[len] = '\0';
            }
        }
    }

    static const char *candidates[8];
    char cwd_path[512], src_path[512], std_path[512];
    int ncand = 0;
    snprintf(cwd_path, sizeof(cwd_path), "%s.nc", name);
    candidates[ncand++] = cwd_path;
    if (dirbuf[0]) {
        snprintf(src_path, sizeof(src_path), "%s/%s.nc", dirbuf, name);
        candidates[ncand++] = src_path;
    }
    snprintf(std_path, sizeof(std_path), "stdlib/%s.nc", name);
    candidates[ncand++] = std_path;

    source = NULL;
    for (int ci = 0; ci < ncand; ci++) {
        FILE *probe = fopen(candidates[ci], "rb");
        if (probe) {
            fclose(probe);
            snprintf(filename, sizeof(filename), "%s", candidates[ci]);
            source = load_source_file(candidates[ci], &source_size);
            break;
        }
    }

    if (!source) {
        /* Not a real file: treat as external (builtin or system library) */
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

    /* Parse the imported module file (recursively, with cycle guard) */
    if (!mod->visited) {
        mod->visited = 1;

        /* Save the current lexer / parser context */
        LexerState *saved_lex_ptr = cs->parser.lex;
        Symbol *saved_module = cs->parser.cur_module;
        Symbol *saved_func = cs->parser.cur_func;
        Symbol *saved_struct = cs->parser.cur_struct;
        int saved_scope = cs->parser.scope_depth;
        int saved_errors = cs->error_count;

        /* Create a fresh lexer context for the module source */
        LexerState mod_lex;
        memset(&mod_lex, 0, sizeof(mod_lex));
        cs->parser.lex = &mod_lex;
        cs->parser.cur_module = (Symbol *)mod;
        cs->parser.cur_func = NULL;
        cs->parser.cur_struct = NULL;
        cs->parser.scope_depth = 0;

        lexer_init(cs, filename, source);
        parse_module(cs);

        /* Restore the main-file context */
        cs->parser.lex = saved_lex_ptr;
        cs->parser.cur_module = saved_module;
        cs->parser.cur_func = saved_func;
        cs->parser.cur_struct = saved_struct;
        cs->parser.scope_depth = saved_scope;
        cs->error_count = saved_errors; /* module errors already reported */
    }

    free(source);
    return mod;
}
