#include "ncc.h"

/* ============================================================
 * Visibility & Scope Management
 * ============================================================ */

static int scope_stack[MAX_NESTING_DEPTH];
static int scope_top = 0;

void visibility_init(CompilerState *cs)
{
    scope_top = 0;
    memset(scope_stack, 0, sizeof(scope_stack));

    /* Initial global scope */
    cs->parser.scope_depth = 0;
}

void vis_scope_enter(CompilerState *cs)
{
    if (scope_top >= MAX_NESTING_DEPTH) {
        nihao_error(cs, "maximum nesting depth exceeded (%d)", MAX_NESTING_DEPTH);
        return;
    }

    /* Record current symbol count for this scope level */
    scope_stack[scope_top] = cs->parser.scope_depth;
    scope_top++;
    cs->parser.scope_depth++;

    if (cs->verbose && cs->debug_mode) {
        printf("Scope enter: depth=%d\n", cs->parser.scope_depth);
    }
}

void vis_scope_exit(CompilerState *cs)
{
    if (scope_top <= 0) {
        nihao_error(cs, "scope exit without matching enter");
        return;
    }

    scope_top--;
    cs->parser.scope_depth--;

    /* Pop local symbols introduced in this scope.
     * In a full implementation, we would track which symbols
     * were added at each scope level and remove them here.
     * For now, we rely on function-level scope management
     * via sym_pop_locals.
     */

    if (cs->verbose && cs->debug_mode) {
        printf("Scope exit: depth=%d\n", cs->parser.scope_depth);
    }
}

/* ============================================================
 * Type Verification
 * ============================================================ */

void verify_types(CompilerState *cs)
{
    Symbol *sym;
    int error_count_before = cs->error_count;

    if (cs->verbose) {
        printf("Verifying types...\n");
    }

    /* Walk all global symbols and verify type consistency */
    for (sym = cs->global_syms; sym; sym = sym->hash_next) {
        if (!sym->type) continue;

        switch (sym->kind) {
            case SYM_FUNCTION:
                /* Verify function has return type */
                if (sym->type->kind != TYPE_FUNC) {
                    nihao_error(cs, "symbol '%s' declared as function but has non-function type",
                               sym->name);
                }
                break;

            case SYM_STRUCT:
            case SYM_UNION:
                /* Verify struct/union has members if defined */
                if (sym->is_defined && sym->member_count == 0) {
                    nihao_warning(cs, "empty %s '%s'",
                                 sym->kind == SYM_STRUCT ? "struct" : "union",
                                 sym->name);
                }
                break;

            case SYM_VARIABLE:
                /* Variables should have valid types */
                if (sym->type->kind == TYPE_NONE) {
                    nihao_error(cs, "variable '%s' has no type", sym->name);
                }
                break;

            default:
                break;
        }
    }

    if (cs->verbose) {
        int found = cs->error_count - error_count_before;
        printf("Type verification complete: %d error(s) found\n", found);
    }
}
