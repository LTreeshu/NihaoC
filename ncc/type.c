#include "ncc.h"

/* ============================================================
 * Type Size Table
 * ============================================================ */

static unsigned int type_default_size(TypeKind kind)
{
    switch (kind) {
        case TYPE_VOID:    return 0;
        case TYPE_CHAR:    return 1;
        case TYPE_STRING:  return 8; /* pointer size */
        case TYPE_U8:      return 1;
        case TYPE_U16:     return 2;
        case TYPE_U32:     return 4;
        case TYPE_U64:     return 8;
        case TYPE_I8:      return 1;
        case TYPE_I16:     return 2;
        case TYPE_I32:     return 4;
        case TYPE_I64:     return 8;
        case TYPE_F32:     return 4;
        case TYPE_F64:     return 8;
        case TYPE_FX32:    return 4;
        case TYPE_FX64:    return 8;
        case TYPE_BOOL:    return 1;
        case TYPE_POINTER: return 8; /* 64-bit pointer */
        case TYPE_ARRAY:   return 0; /* variable */
        case TYPE_FUNC:    return 0;
        case TYPE_STRUCT:  return 0; /* variable */
        case TYPE_UNION:   return 0;
        case TYPE_ENUM:    return 4;
        case TYPE_ALIAS:   return 0;
        default:           return 0;
    }
}

static unsigned int type_default_align(TypeKind kind)
{
    switch (kind) {
        case TYPE_VOID:    return 1;
        case TYPE_CHAR:    return 1;
        case TYPE_STRING:  return 8;
        case TYPE_U8:      return 1;
        case TYPE_U16:     return 2;
        case TYPE_U32:     return 4;
        case TYPE_U64:     return 8;
        case TYPE_I8:      return 1;
        case TYPE_I16:     return 2;
        case TYPE_I32:     return 4;
        case TYPE_I64:     return 8;
        case TYPE_F32:     return 4;
        case TYPE_F64:     return 8;
        case TYPE_FX32:    return 4;
        case TYPE_FX64:    return 8;
        case TYPE_BOOL:    return 1;
        case TYPE_POINTER: return 8;
        case TYPE_ARRAY:   return 1;
        case TYPE_FUNC:    return 1;
        case TYPE_STRUCT:  return 1;
        case TYPE_UNION:   return 1;
        case TYPE_ENUM:    return 4;
        case TYPE_ALIAS:   return 1;
        default:           return 1;
    }
}

/* ============================================================
 * Type Construction
 * ============================================================ */

CType *type_new(CompilerState *cs, TypeKind kind)
{
    CType *type;

    type = nihao_malloc(cs, sizeof(CType));
    memset(type, 0, sizeof(CType));

    type->kind = kind;
    type->vis = VIS_DEFAULT;
    type->size = type_default_size(kind);
    type->align = type_default_align(kind);
    type->bit_size = 0;
    type->bit_offset = 0;
    type->ref = NULL;
    type->sym = NULL;
    type->next = NULL;
    type->params = NULL;
    type->param_count = 0;
    type->is_multireturn = 0;
    type->return_count = 0;

    return type;
}

CType *type_array(CompilerState *cs, void *elem_type, int size)
{
    CType *arr_type;

    arr_type = nihao_malloc(cs, sizeof(CType));
    memset(arr_type, 0, sizeof(CType));

    arr_type->kind = TYPE_ARRAY;
    arr_type->vis = VIS_DEFAULT;
    arr_type->align = 1;
    arr_type->bit_size = 0;
    arr_type->bit_offset = 0;

    /* Element type reference */
    if (elem_type) {
        arr_type->ref = (CType *)elem_type;
        if (size >= 0 && arr_type->ref) {
            arr_type->size = size * arr_type->ref->size;
        } else {
            arr_type->size = 0; /* dynamic array */
        }
    } else {
        arr_type->ref = NULL;
        arr_type->size = 0;
    }

    /* Store array size in param_count field (reused) */
    arr_type->param_count = size;

    return arr_type;
}

/* ============================================================
 * Type Checking (Placeholder)
 * ============================================================ */

CType *type_check_statement(CompilerState *cs)
{
    /* Placeholder for statement type checking.
     * A full implementation would:
     * - Verify expression types match expected types
     * - Check assignment compatibility
     * - Validate function call arguments
     * - Detect type errors before code generation
     *
     * For now, we return NULL indicating no type error detected
     * at this stage (parsing-level type checking is sufficient).
     */

    /* The actual type check is done during parsing in parse_type,
     * parse_expression, etc. This function serves as a hook for
     * deeper semantic analysis passes. */
    (void)cs;

    return NULL;
}
