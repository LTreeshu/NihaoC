#include "ncc.h"

/* ============================================================
 * Visibility & Ownership Static Analysis (NihaoC spec ch.11-14)
 *
 * State machine for pointer transfer rules (ch.12.1 / 12.2):
 *   flow  -> var    : mutable borrow, source FROZEN
 *   flow  -> const  : read-only borrow, source FROZEN
 *   flow  -> flow   : ownership transfer, source INVALID
 *   var   -> var    : mutable borrow, source FROZEN
 *   var   -> const  : read-only borrow, source FROZEN
 *   const -> const  : read-only borrow, source unchanged
 *   static-> const  : read-only borrow, source unchanged
 *   static-> static : shared reference, source unchanged
 *   FORBIDDEN: flow->static, var->flow, const->var, const->flow,
 *              static->var, static->flow
 * ============================================================ */

#define BS_VALID    0
#define BS_FROZEN   1
#define BS_INVALID  2

static int scope_stack[MAX_NESTING_DEPTH];
static int scope_top = 0;

void visibility_init(CompilerState *cs)
{
    scope_top = 0;
    memset(scope_stack, 0, sizeof(scope_stack));
    cs->parser.scope_depth = 0;
}

void vis_scope_enter(CompilerState *cs)
{
    if (scope_top >= MAX_NESTING_DEPTH) {
        nihao_error(cs, "maximum nesting depth exceeded (%d)", MAX_NESTING_DEPTH);
        return;
    }
    scope_stack[scope_top] = cs->parser.scope_depth;
    scope_top++;
    cs->parser.scope_depth++;
}

void vis_scope_exit(CompilerState *cs)
{
    if (scope_top <= 0) {
        nihao_error(cs, "scope exit without matching enter");
        return;
    }
    scope_top--;
    cs->parser.scope_depth--;
}

/* Is this type a pointer-like value (ownership applies)? */
int vis_is_pointer_type(CType *t)
{
    if (!t) return 0;
    return t->kind == TYPE_VOID || t->kind == TYPE_POINTER ||
           t->kind == TYPE_STRING || t->kind == TYPE_ARRAY;
}

/* Normalize default visibility to VAR for matrix lookups */
static int vis_norm(Visibility v)
{
    return (v == VIS_DEFAULT) ? VIS_DEFAULT /* var */ : (int)v;
}

/* Storage-lifetime + ownership/borrow transfer check (ch.12.2 matrix).
 * Returns 1 if the transfer is FORBIDDEN (caller reports the error). */
int vis_check_transfer(Visibility src, Visibility dst)
{
    int s = vis_norm(src);
    int d = vis_norm(dst);

    switch (s) {
        case VIS_CONST:
            return (d == VIS_CONST) ? 0 : 1;   /* const -> only const */
        case VIS_STATIC:
            return (d == VIS_CONST || d == VIS_STATIC) ? 0 : 1;
        case VIS_FLOW:
            return (d == VIS_CONST || d == VIS_FLOW || d == VIS_DEFAULT) ? 0 : 1;
        case VIS_DEFAULT: /* var */
        default:
            return (d == VIS_CONST || d == VIS_DEFAULT) ? 0 : 1;
    }
}

/* Apply the source state change implied by a transfer (ch.12.1). */
void vis_update_source(Visibility src, Visibility dst, Symbol *src_sym)
{
    if (!src_sym) return;
    int s = vis_norm(src);
    int d = vis_norm(dst);

    switch (s) {
        case VIS_FLOW:
            if (d == VIS_FLOW) {
                src_sym->borrow_state = BS_INVALID;   /* ownership moved */
            } else if (d == VIS_CONST || d == VIS_DEFAULT) {
                src_sym->borrow_state = BS_FROZEN;    /* borrowed */
            }
            break;
        case VIS_DEFAULT: /* var */
            if (d == VIS_CONST || d == VIS_DEFAULT) {
                src_sym->borrow_state = BS_FROZEN;
            }
            break;
        case VIS_CONST:
        case VIS_STATIC:
        default:
            break;   /* source unchanged */
    }
}

/* Unfreeze sources that were borrowed by the given borrow variables
 * (called when the borrow scope ends, NihaoC ch.13.1). */
void vis_unfreeze_borrows(Symbol *borrow_head, Symbol *scope_start)
{
    for (Symbol *s = borrow_head; s && s != scope_start; s = s->next) {
        if (s->borrow_source && s->borrow_source->borrow_state == BS_FROZEN) {
            s->borrow_source->borrow_state = BS_VALID;
        }
    }
}

/* Check that a variable is usable (not moved-from). */
int vis_check_usable(CompilerState *cs, Symbol *s)
{
    if (!s) return 0;
    if (s->borrow_state == BS_INVALID) {
        nihao_error(cs, "'%s' is invalidated: its ownership has been moved",
                    s->name);
        return 1;
    }
    return 0;
}

/* Check that a variable can be written (not frozen by a borrow). */
int vis_check_writable(CompilerState *cs, Symbol *s)
{
    if (!s) return 0;
    if (s->borrow_state == BS_FROZEN) {
        nihao_error(cs, "'%s' is frozen by an active borrow (const/var)",
                    s->name);
        return 1;
    }
    return vis_check_usable(cs, s);
}

/* Validate an assignment / initializer transfer:
 *   dst_vis is the receiving variable's visibility,
 *   src_sym (if known) is the source symbol being transferred.
 * Reports errors for forbidden transfers and updates borrow state. */
void vis_check_assign(CompilerState *cs, Visibility dst_vis, Symbol *src_sym,
                      Symbol *dst_sym, const char *dst_name)
{
    if (!src_sym) return;   /* literal / unknown source: nothing to check */
    if (src_sym->kind == SYM_FUNCTION) return;

    Visibility src_vis = src_sym->vis;

    /* Only pointer-like transfers are governed by the matrix;
     * plain values are copied and have no ownership semantics. */
    if (!vis_is_pointer_type(src_sym->type)) {
        return;
    }

    /* A value being copied from a frozen/void source is allowed,
     * but using an invalidated (moved) source is not. */
    if (src_sym->borrow_state == BS_INVALID) {
        nihao_error(cs, "'%s' is invalidated (ownership moved); cannot use it",
                    src_sym->name);
        return;
    }

    if (vis_check_transfer(src_vis, dst_vis)) {
        nihao_error(cs, "cannot assign %s (visibility %s) to '%s' (%s): "
                        "target lifetime would be shorter than source",
                    src_sym->name,
                    src_vis == VIS_CONST ? "const" :
                    src_vis == VIS_STATIC ? "static" :
                    src_vis == VIS_FLOW ? "flow" : "var",
                    dst_name ? dst_name : "(target)",
                    dst_vis == VIS_CONST ? "const" :
                    dst_vis == VIS_STATIC ? "static" :
                    dst_vis == VIS_FLOW ? "flow" : "var");
        return;
    }

    /* Update borrow state on the source */
    vis_update_source(src_vis, dst_vis, src_sym);

    /* Record the borrow relationship so the source can be unfrozen
     * when the borrow variable's scope ends (ch.13.1) */
    if (dst_sym && src_sym->borrow_state == BS_FROZEN) {
        dst_sym->borrow_source = src_sym;
    }
}

void verify_types(CompilerState *cs)
{
    Symbol *sym;
    int error_count_before = cs->error_count;

    if (cs->verbose) {
        printf("Verifying types...\n");
    }

    for (sym = cs->global_syms; sym; sym = sym->hash_next) {
        if (!sym->type) continue;

        switch (sym->kind) {
            case SYM_FUNCTION:
                if (sym->type->kind != TYPE_FUNC) {
                    nihao_error(cs, "symbol '%s' declared as function but has non-function type",
                               sym->name);
                }
                break;
            case SYM_STRUCT:
            case SYM_UNION:
                if (sym->is_defined && sym->member_count == 0) {
                    nihao_warning(cs, "empty %s '%s'",
                                 sym->kind == SYM_STRUCT ? "struct" : "union",
                                 sym->name);
                }
                break;
            case SYM_VARIABLE:
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
