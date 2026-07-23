#include "ncc.h"

/* ============================================================
 * Hash Function
 * ============================================================ */

static unsigned int sym_hash(const char *str)
{
    unsigned int h = 0;
    while (*str) {
        h = h * 31 + (unsigned char)*str++;
    }
    return h % MAX_SYMBOLS;
}

/* ============================================================
 * Symbol Lookup
 * ============================================================ */

Symbol *sym_find(CompilerState *cs, const char *tok_str)
{
    Symbol *sym;

    if (!tok_str) return NULL;

    /* Search local symbols first (current function scope) */
    if (cs->parser.cur_func && cs->parser.cur_func->locals) {
        for (sym = cs->parser.cur_func->locals; sym; sym = sym->next) {
            if (sym->name && strcmp(sym->name, tok_str) == 0) {
                return sym;
            }
        }
    }

    /* Search module-level / global symbols */
    for (sym = cs->global_syms; sym; sym = sym->hash_next) {
        if (sym->name && strcmp(sym->name, tok_str) == 0) {
            return sym;
        }
    }

    return NULL;
}

/* ============================================================
 * Symbol Creation
 * ============================================================ */

Symbol *sym_push(CompilerState *cs, SymKind kind, const char *tok_str, CType *type)
{
    Symbol *sym;

    sym = nihao_malloc(cs, sizeof(Symbol));
    memset(sym, 0, sizeof(Symbol));

    sym->kind = kind;
    sym->name = nihao_strdup(cs, tok_str);
    sym->hash = sym_hash(tok_str);
    sym->vis = VIS_DEFAULT;
    sym->is_defined = 0;
    sym->is_extern = 0;
    sym->is_builtin = 0;

    if (type) {
        /* Copy type data */
        CType *t = nihao_malloc(cs, sizeof(CType));
        memcpy(t, type, sizeof(CType));
        sym->type = t;
    }

    /* Set location info */
    if (cs->parser.lex) {
        sym->filename = cs->parser.lex->filename;
        sym->line_num = cs->parser.lex->line_num;
    }

    /* Add to global symbol list (chain at head) */
    sym->hash_next = cs->global_syms;
    cs->global_syms = sym;

    return sym;
}

/* ============================================================
 * Struct/Union Member Management
 * ============================================================ */

Symbol *sym_add_member(CompilerState *cs, Symbol *sym, const char *name, CType *type)
{
    Symbol *member;
    Symbol *last;

    if (!sym || !name || !type) return NULL;

    member = nihao_malloc(cs, sizeof(Symbol));
    memset(member, 0, sizeof(Symbol));

    member->kind = SYM_VARIABLE;
    member->name = nihao_strdup(cs, name);
    member->vis = VIS_DEFAULT;

    /* Copy type */
    CType *t = nihao_malloc(cs, sizeof(CType));
    memcpy(t, type, sizeof(CType));
    member->type = t;

    /* Calculate offset (simple sequential layout) */
    if (sym->member_count == 0) {
        sym->members = member;
    } else {
        last = sym->members;
        while (last->next) {
            last = last->next;
        }
        last->next = member;
    }

    sym->member_count++;

    /* Update struct total size and alignment */
    if ((int)type->align > sym->total_align) {
        sym->total_align = type->align ? type->align : 1;
    }

    /* Simple size calculation (will be properly laid out later) */
    sym->total_size += type->size ? type->size : 1;

    return member;
}

/* ============================================================
 * Local Symbol Management
 * ============================================================ */

Symbol *sym_push_local(CompilerState *cs, Symbol *func_sym, const char *name, CType *type)
{
    Symbol *sym;

    if (!func_sym || !name) return NULL;

    sym = nihao_malloc(cs, sizeof(Symbol));
    memset(sym, 0, sizeof(Symbol));

    sym->kind = SYM_VARIABLE;
    sym->name = nihao_strdup(cs, name);
    sym->vis = VIS_DEFAULT;

    if (type) {
        CType *t = nihao_malloc(cs, sizeof(CType));
        memcpy(t, type, sizeof(CType));
        sym->type = t;
    }

    /* Stack offset assignment (grows downward, simple increment) */
    sym->stack_offset = func_sym->local_count * 8; /* simple 8-byte slots */

    /* Prepend to local list */
    sym->next = func_sym->locals;
    func_sym->locals = sym;
    func_sym->local_count++;

    return sym;
}

Symbol *sym_pop_locals(CompilerState *cs, Symbol *func_sym)
{
    /* Mark locals as out of scope; memory is reclaimed at compiler cleanup */
    if (!func_sym) return NULL;
    (void)cs;

    /* We don't actually free here (nihao_malloc tracks everything),
     * just clear the locals list for the function scope */
    func_sym->locals = NULL;
    func_sym->local_count = 0;

    return NULL;
}

/* ============================================================
 * Built-in Symbols Registration
 * ============================================================ */

Symbol *sym_register_builtins(CompilerState *cs)
{
    Symbol *first = NULL;
    CType type;

    /* Built-in types are handled by the parser via keywords.
     * Here we register built-in functions and runtime symbols. */

    /* puts function: void puts(string) */
    memset(&type, 0, sizeof(type));
    type.kind = TYPE_FUNC;
    type.size = 0;
    type.param_count = 1;

    Symbol *puts_sym = sym_push(cs, SYM_FUNCTION, "puts", &type);
    puts_sym->is_builtin = 1;
    puts_sym->is_defined = 1;
    if (!first) first = puts_sym;

    /* printf function: void printf(string, ...) */
    Symbol *printf_sym = sym_push(cs, SYM_FUNCTION, "printf", &type);
    printf_sym->is_builtin = 1;
    printf_sym->is_defined = 1;

    /* malloc function: void* malloc(u64) */
    Symbol *malloc_sym = sym_push(cs, SYM_FUNCTION, "malloc", &type);
    malloc_sym->is_builtin = 1;
    malloc_sym->is_defined = 1;

    /* free function: void free(void*) */
    Symbol *free_sym = sym_push(cs, SYM_FUNCTION, "free", &type);
    free_sym->is_builtin = 1;
    free_sym->is_defined = 1;

    /* memcpy function: void* memcpy(void*, void*, u64) */
    Symbol *memcpy_sym = sym_push(cs, SYM_FUNCTION, "memcpy", &type);
    memcpy_sym->is_builtin = 1;
    memcpy_sym->is_defined = 1;

    /* memset function: void* memset(void*, i32, u64) */
    Symbol *memset_sym = sym_push(cs, SYM_FUNCTION, "memset", &type);
    memset_sym->is_builtin = 1;
    memset_sym->is_defined = 1;

    /* strlen function: u64 strlen(string) */
    Symbol *strlen_sym = sym_push(cs, SYM_FUNCTION, "strlen", &type);
    strlen_sym->is_builtin = 1;
    strlen_sym->is_defined = 1;

    return first;
}
