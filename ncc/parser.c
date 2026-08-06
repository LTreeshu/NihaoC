#include "ncc.h"


/* ============================================================
 * Parser Initialization
 * ============================================================ */

void parser_init(CompilerState *cs)
{
    cs->parser.cur_module = NULL;
    cs->parser.cur_func = NULL;
    cs->parser.cur_struct = NULL;
    cs->parser.scope_depth = 0;
    cs->parser.parse_flags = 0;
    cs->parser.const_wanted = 0;
    cs->parser.nocode_wanted = 0;
    cs->parser.macro_ptr = NULL;
    cs->parser.unget_buffer_enabled = 0;
    cs->parser.error_count = 0;
    cs->parser.warning_count = 0;

    /* Register built-in symbols */
    sym_register_builtins(cs);
}

/* ============================================================
 * Token Helpers
 * ============================================================ */

TokenType cur_tok(CompilerState *cs)
{
    return cs->parser.lex->tok;
}

void next_tok(CompilerState *cs)
{
    lexer_next(cs->parser.lex);
}

void expect(CompilerState *cs, TokenType tok)
{
    if (cur_tok(cs) != tok) {
        nihao_error(cs, "expected '%s', got '%s'",
                   token_name(tok), token_name(cur_tok(cs)));
    }
    next_tok(cs);
}

void skip_newlines(CompilerState *cs)
{
    while (cur_tok(cs) == TOK_NEWLINE) {
        next_tok(cs);
    }
}

static int is_type_begin(TokenType tok)
{
    return is_type_token(tok)
        || tok == TOK_CONST || tok == TOK_FLOW || tok == TOK_STATIC
        || tok == TOK_VAR || tok == TOK_ALIAS || tok == TOK_MULTIRETURN;
}

/* ============================================================
 * Type Parsing
 *
 * NihaoC types (name comes BEFORE type in declarations):
 *   Person struct { ... }   /   Color enum { A, B }   /   Data union {...}
 *   var x i32               /   flow p void            /   arr i32[5]
 *   char[] (string)         /   void  (generic ptr)    /   void[] (ptr-to-ptr)
 *   Function ptr: void(u8, char[]) i32
 * ============================================================ */

static int is_user_type_name(CompilerState *cs)
{
    if (cur_tok(cs) != TOK_IDENTIFIER) return 0;
    Symbol *s = sym_find(cs, cs->parser.lex->tok_str);
    return s && (s->kind == SYM_STRUCT || s->kind == SYM_UNION ||
                 s->kind == SYM_ENUM || s->kind == SYM_TYPEDEF);
}

/* Parse a struct/union/enum member list in NihaoC order:
 *   [vis] name Type [:bitwidth]
 * Emits the C member declaration while parsing. */
static void parse_member_list(CompilerState *cs, Symbol *owner, int is_union)
{
    (void)is_union;
    for (;;) {
        skip_newlines(cs);
        if (cur_tok(cs) == TOK_RBRACE || cur_tok(cs) == TOK_EOF) break;

        /* optional member visibility prefix */
        if (cur_tok(cs) == TOK_CONST || cur_tok(cs) == TOK_FLOW ||
            cur_tok(cs) == TOK_STATIC || cur_tok(cs) == TOK_VAR) {
            next_tok(cs);
        }

        if (cur_tok(cs) != TOK_IDENTIFIER) {
            nihao_error(cs, "expected member name, got '%s'",
                        token_name(cur_tok(cs)));
            next_tok(cs);
            continue;
        }
        char *mname = cs->parser.lex->tok_str;
        next_tok(cs);

        CType mtype;
        parse_type(cs, &mtype);

        /* bitfield: flag u8:1 */
        if (cur_tok(cs) == TOK_COLON) {
            next_tok(cs);
            if (cur_tok(cs) == TOK_INT_CONST) {
                mtype.bit_size = (unsigned)cs->parser.lex->tok_val.i;
                next_tok(cs);
            }
        }
        /* default value: member Type = expr  (ignored in C output) */
        if (cur_tok(cs) == TOK_ASSIGN) {
            next_tok(cs);
            parse_expression(cs);
        }
        if (owner) {
            sym_add_member(cs, owner, mname, &mtype);
        }
        /* emit C member declaration */
        if (mtype.bit_size > 0) {
            cgen_line("%s %s : %u;", c_type_name(&mtype), mname, mtype.bit_size);
        } else {
            cgen_line("%s %s%s;", c_type_name(&mtype), mname, c_type_suffix(&mtype));
        }
    }
    expect(cs, TOK_RBRACE);
}

void parse_type(CompilerState *cs, CType *type)
{
    TokenType tok = cur_tok(cs);

    memset(type, 0, sizeof(CType));

    switch (tok) {
        case TOK_VOID:
            type->kind = TYPE_VOID;
            next_tok(cs);
            if (cur_tok(cs) == TOK_LPAREN) {
                /* function pointer type: void(param types) [ret] */
                type->kind = TYPE_FUNC;
                next_tok(cs);
                skip_newlines(cs);
                while (cur_tok(cs) != TOK_RPAREN && cur_tok(cs) != TOK_EOF) {
                    /* optional param attr prefix (flow/static/const/var) */
                    if (cur_tok(cs) == TOK_FLOW || cur_tok(cs) == TOK_STATIC ||
                        cur_tok(cs) == TOK_CONST || cur_tok(cs) == TOK_VAR) {
                        next_tok(cs);
                    }
                    CType pt;
                    parse_type(cs, &pt);
                    type->param_count++;
                    if (cur_tok(cs) == TOK_COMMA) next_tok(cs);
                    skip_newlines(cs);
                }
                expect(cs, TOK_RPAREN);
                /* optional return type after params */
                if (is_type_begin(cur_tok(cs)) || is_user_type_name(cs)) {
                    CType *ret = type_new(cs, TYPE_NONE);
                    parse_type(cs, ret);
                    type->next = ret;
                } else if (cur_tok(cs) == TOK_VOID) {
                    /* explicit "void" return: generic pointer */
                    CType *ret = type_new(cs, TYPE_VOID);
                    next_tok(cs);
                    type->next = ret;
                }
            }
            return;
        case TOK_CHAR:
            type->kind = TYPE_CHAR;
            type->size = 1;
            next_tok(cs);
            if (cur_tok(cs) == TOK_LBRACKET) {
                /* char[] -> string; char[N] -> array (handled by suffix loop) */
                LexerState *lex = cs->parser.lex;
                lex->peek_valid = 0;
                lexer_peek(lex);
                if (lex->peek_tok == TOK_RBRACKET) {
                    lex->peek_valid = 0;
                    next_tok(cs); /* [ */
                    next_tok(cs); /* ] */
                    type->kind = TYPE_STRING;
                    type->size = 8;
                    return;
                }
                lex->peek_valid = 0;
            }
            break;   /* fall through to array/pointer suffix loop */
        case TOK_STRING:
            type->kind = TYPE_STRING;
            type->size = 8;
            next_tok(cs);
            return;
        case TOK_BOOL:
            type->kind = TYPE_BOOL;
            type->size = 1;
            next_tok(cs);
            return;
        case TOK_U8:  type->kind = TYPE_U8;  type->size = 1; next_tok(cs); break;
        case TOK_U16: type->kind = TYPE_U16; type->size = 2; next_tok(cs); break;
        case TOK_U32: type->kind = TYPE_U32; type->size = 4; next_tok(cs); break;
        case TOK_U64: type->kind = TYPE_U64; type->size = 8; next_tok(cs); break;
        case TOK_I8:  type->kind = TYPE_I8;  type->size = 1; next_tok(cs); break;
        case TOK_I16: type->kind = TYPE_I16; type->size = 2; next_tok(cs); break;
        case TOK_I32: type->kind = TYPE_I32; type->size = 4; next_tok(cs); break;
        case TOK_I64: type->kind = TYPE_I64; type->size = 8; next_tok(cs); break;
        case TOK_F32: type->kind = TYPE_F32; type->size = 4; next_tok(cs); break;
        case TOK_F64: type->kind = TYPE_F64; type->size = 8; next_tok(cs); break;
        case TOK_FX32: type->kind = TYPE_FX32; type->size = 4; next_tok(cs); break;
        case TOK_FX64: type->kind = TYPE_FX64; type->size = 8; next_tok(cs); break;

        case TOK_STRUCT:
        case TOK_UNION: {
            type->kind = (tok == TOK_STRUCT) ? TYPE_STRUCT : TYPE_UNION;
            next_tok(cs);
            /* struct Person {...} style (name may follow) */
            if (cur_tok(cs) == TOK_IDENTIFIER && !is_user_type_name(cs)) {
                Symbol *sym = sym_find(cs, cs->parser.lex->tok_str);
                if (!sym) {
                    sym = sym_push(cs, (tok == TOK_STRUCT) ? SYM_STRUCT : SYM_UNION,
                                   cs->parser.lex->tok_str, type);
                }
                type->sym = sym;
                next_tok(cs);
            }
            if (cur_tok(cs) == TOK_LBRACE) {
                next_tok(cs);
                parse_member_list(cs, type->sym, tok == TOK_UNION);
            }
            return;
        }
        case TOK_ENUM: {
            type->kind = TYPE_ENUM;
            type->size = 4;
            next_tok(cs);
            if (cur_tok(cs) == TOK_IDENTIFIER && !is_user_type_name(cs)) {
                Symbol *sym = sym_find(cs, cs->parser.lex->tok_str);
                if (!sym) {
                    sym = sym_push(cs, SYM_ENUM, cs->parser.lex->tok_str, type);
                }
                type->sym = sym;
                next_tok(cs);
            }
            if (cur_tok(cs) == TOK_LBRACE) {
                next_tok(cs);
                int val = 0;
                skip_newlines(cs);
                while (cur_tok(cs) != TOK_RBRACE && cur_tok(cs) != TOK_EOF) {
                    if (cur_tok(cs) != TOK_IDENTIFIER) {
                        nihao_error(cs, "expected enum variant name, got '%s'",
                                    token_name(cur_tok(cs)));
                        next_tok(cs);
                        continue;
                    }
                    char *vname = cs->parser.lex->tok_str;
                    next_tok(cs);
                    CType vt;
                    memset(&vt, 0, sizeof(vt));
                    vt.kind = TYPE_ENUM;
                    vt.size = 4;
                    Symbol *vs = sym_push(cs, SYM_ENUM, vname, &vt);
                    vs->addr = (unsigned long)val;
                    if (cur_tok(cs) == TOK_ASSIGN) {
                        next_tok(cs);
                        if (cur_tok(cs) == TOK_INT_CONST) {
                            val = (int)cs->parser.lex->tok_val.i;
                            vs->addr = (unsigned long)val;
                            next_tok(cs);
                        }
                    }
                    val++;
                    if (cur_tok(cs) == TOK_COMMA) next_tok(cs);
                    skip_newlines(cs);
                }
                expect(cs, TOK_RBRACE);
            }
            return;
        }
        case TOK_IDENTIFIER: {
            /* user-defined type name (struct/union/enum/alias) */
            Symbol *s = sym_find(cs, cs->parser.lex->tok_str);
            if (!s || (s->kind != SYM_STRUCT && s->kind != SYM_UNION &&
                       s->kind != SYM_ENUM && s->kind != SYM_TYPEDEF)) {
                nihao_error(cs, "unknown type '%s'", cs->parser.lex->tok_str);
                next_tok(cs);
                return;
            }
            if (s->type) {
                memcpy(type, s->type, sizeof(CType));
                type->sym = s;
            } else {
                type->kind = TYPE_STRUCT;
                type->sym = s;
            }
            next_tok(cs);
            break;
        }
        default:
            nihao_error(cs, "expected type, got '%s'", token_name(tok));
            next_tok(cs);
            return;
    }

    /* Check for pointer/array modifiers */
    while (cur_tok(cs) == TOK_STAR || cur_tok(cs) == TOK_LBRACKET) {
        if (cur_tok(cs) == TOK_STAR) {
            next_tok(cs);
            CType *ptr_type = type_new(cs, TYPE_POINTER);
            ptr_type->ref = type_new(cs, type->kind);
            memcpy(ptr_type->ref, type, sizeof(CType));
            *type = *ptr_type;
        } else if (cur_tok(cs) == TOK_LBRACKET) {
            next_tok(cs);
            int array_size = -1; /* dynamic */
            if (cur_tok(cs) == TOK_INT_CONST) {
                array_size = (int)cs->parser.lex->tok_val.i;
                next_tok(cs);
            } else if (cur_tok(cs) == TOK_ELLIPSIS) {
                next_tok(cs);
                if (cur_tok(cs) == TOK_INT_CONST) {
                    array_size = (int)cs->parser.lex->tok_val.i;
                    next_tok(cs);
                }
            }
            expect(cs, TOK_RBRACKET);
            /* Build array type directly (type_array(NULL) leaves ref NULL) */
            CType *arr_type = type_new(cs, TYPE_ARRAY);
            arr_type->ref = type_new(cs, type->kind);
            memcpy(arr_type->ref, type, sizeof(CType));
            arr_type->param_count = array_size;
            arr_type->size = (array_size > 0 && arr_type->ref->size > 0)
                             ? (unsigned)array_size * arr_type->ref->size : 0;
            *type = *arr_type;
        }
    }
}

/* ============================================================
 * Module Parsing
 * ============================================================ */

void parse_module(CompilerState *cs)
{
    /* Lexer is lazy (TCC-style): read the first token before any
     * cur_tok() based dispatch. */
    next_tok(cs);
    skip_newlines(cs);

    /* Module declaration */
    if (cur_tok(cs) == TOK_MODULE) {
        next_tok(cs);
        if (cur_tok(cs) != TOK_IDENTIFIER) {
            nihao_error(cs, "expected module name, got '%s'", token_name(cur_tok(cs)));
        } else {
            char *mod_name = cs->parser.lex->tok_str;
            next_tok(cs);
            Module *mod = module_add(cs, mod_name, cs->input_file);
            cs->parser.cur_module = mod;
        }
        skip_newlines(cs);
    } else {
        /* Default module */
        Module *mod = module_add(cs, "main", cs->input_file);
        cs->parser.cur_module = mod;
    }

    /* Parse use statements */
    while (cur_tok(cs) == TOK_USE) {
        next_tok(cs);
        if (cur_tok(cs) != TOK_IDENTIFIER) {
            nihao_error(cs, "expected module name after 'use', got '%s'",
                        token_name(cur_tok(cs)));
        } else {
            char *use_name = cs->parser.lex->tok_str;
            next_tok(cs);
            module_import(cs, use_name);
        }
        skip_newlines(cs);
    }

    /* Parse link statements */
    while (cur_tok(cs) == TOK_LINK) {
        next_tok(cs);
        /* link "libhttp.so" as http  |  link "libc.so" libc */
        if (cur_tok(cs) == TOK_STRING_LITERAL) {
            char *lib_path = cs->parser.lex->tok_str;
            next_tok(cs);
            if (cur_tok(cs) == TOK_AS) {
                next_tok(cs);
                if (cur_tok(cs) != TOK_IDENTIFIER) {
                    nihao_error(cs, "expected alias after 'link ... as', got '%s'",
                                token_name(cur_tok(cs)));
                } else {
                    char *alias = cs->parser.lex->tok_str;
                    next_tok(cs);
                    link_add_library(cs, lib_path, alias, lib_path);
                }
            } else {
                link_add_library(cs, lib_path, lib_path, lib_path);
            }
        }
        skip_newlines(cs);
    }

    /* Parse top-level declarations */
    while (cur_tok(cs) != TOK_EOF) {
        parse_declaration(cs);
        skip_newlines(cs);
    }
}

/* ============================================================
 * Declaration Parsing (NihaoC style: name comes BEFORE type)
 *
 *   [[inline]] func add(a i32, b i32) i32 { ... }
 *   const MAX_SIZE i32 = 1024          (variable)
 *   Person struct { ... }               (type definition)
 *   Color enum { RED, GREEN, BLUE }
 *   alias Byte = u8
 *   var {a = 0, b = 1} i8               (multi-variable)
 *   sum = calculate_sum(...)            (type-inferred variable)
 * ============================================================ */

/* Tokens that continue an expression (i.e. RHS is NOT a bare identifier) */
static int is_expr_continuer(TokenType t)
{
    switch (t) {
        case TOK_LPAREN: case TOK_DOT: case TOK_LBRACKET: case TOK_ARROW:
        case TOK_DOT_PAREN: case TOK_SAFE_DOT:
        case TOK_PLUS: case TOK_MINUS: case TOK_STAR: case TOK_SLASH:
        case TOK_PERCENT: case TOK_EQ: case TOK_NE: case TOK_LT: case TOK_GT:
        case TOK_LE: case TOK_GE: case TOK_LOGICAL_AND: case TOK_LOGICAL_OR:
        case TOK_BITWISE_AND: case TOK_BITWISE_OR: case TOK_BITWISE_XOR:
        case TOK_LEFT_SHIFT: case TOK_RIGHT_SHIFT: case TOK_QUESTION:
        case TOK_ASSIGN: case TOK_SAFE_ASSIGN:
        case TOK_PLUS_ASSIGN: case TOK_MINUS_ASSIGN: case TOK_STAR_ASSIGN:
        case TOK_SLASH_ASSIGN: case TOK_PERCENT_ASSIGN:
        case TOK_INCREMENT: case TOK_DECREMENT:
        case TOK_RANGE:
            return 1;
        default:
            return 0;
    }
}

/* Heuristic C type for a type-inferred variable initializer.
 * Returns 1 if the RHS first token gave a usable hint. */
static int infer_init_type(CompilerState *cs, CType *out)
{
    memset(out, 0, sizeof(CType));
    switch (cur_tok(cs)) {
        case TOK_INT_CONST:
            out->kind = TYPE_I32;
            out->size = 4;
            if (cs->parser.lex->tok_val.i > 0x7fffffffLL ||
                cs->parser.lex->tok_val.i < -0x80000000LL) {
                out->kind = TYPE_I64;
                out->size = 8;
            }
            return 1;
        case TOK_FLOAT_CONST:
            out->kind = TYPE_F64;
            out->size = 8;
            return 1;
        case TOK_STRING_LITERAL:
            out->kind = TYPE_STRING;
            out->size = 8;
            return 1;
        case TOK_TRUE:
        case TOK_FALSE:
            out->kind = TYPE_BOOL;
            out->size = 1;
            return 1;
        case TOK_IDENTIFIER: {
            Symbol *s = sym_find(cs, cs->parser.lex->tok_str);
            if (s && s->type) {
                memcpy(out, s->type, sizeof(CType));
                out->sym = s;
                return 1;
            }
            out->kind = TYPE_POINTER;
            out->size = 8;
            return 1;
        }
        default:
            out->kind = TYPE_I32;
            out->size = 4;
            return 0;
    }
}

void parse_declaration(CompilerState *cs)
{
    TokenType tok = cur_tok(cs);
    Visibility vis = VIS_DEFAULT;
    char attr[256];
    attr[0] = '\0';


    /* [[ ... ]] function attributes */
    if (tok == TOK_LBRACKET && cs->parser.lex->peek_valid == 0) {
        /* lookahead: is next token also '['? */
        cs->parser.lex->peek_valid = 0;
        lexer_peek(cs->parser.lex);
        if (cs->parser.lex->peek_tok == TOK_LBRACKET) {
            cs->parser.lex->peek_valid = 0;
            next_tok(cs); /* [ */
            next_tok(cs); /* [ */
            while (cur_tok(cs) != TOK_RBRACKET && cur_tok(cs) != TOK_EOF) {
                if (cur_tok(cs) == TOK_IDENTIFIER) {
                    const char *a = cs->parser.lex->tok_str;
                    if (strcmp(a, "inline") == 0) strcat(attr, "inline ");
                    else if (strcmp(a, "weak") == 0) strcat(attr, "__attribute__((weak)) ");
                    else if (strcmp(a, "local") == 0) strcat(attr, "static ");
                    else if (strcmp(a, "used") == 0) strcat(attr, "__attribute__((used)) ");
                    else if (strcmp(a, "unused") == 0) strcat(attr, "__attribute__((unused)) ");
                    /* export "sec" - ignored for C output */
                    next_tok(cs);
                    if (cur_tok(cs) == TOK_STRING_LITERAL) next_tok(cs);
                } else {
                    next_tok(cs);
                }
                if (cur_tok(cs) == TOK_COMMA) next_tok(cs);
            }
            expect(cs, TOK_RBRACKET);
            expect(cs, TOK_RBRACKET);
            tok = cur_tok(cs);
        } else {
            cs->parser.lex->peek_valid = 0;
        }
    }

    /* Visibility modifiers (attribute system) */
    if (tok == TOK_CONST) {
        vis = VIS_CONST;
        next_tok(cs);
    } else if (tok == TOK_FLOW) {
        vis = VIS_FLOW;
        next_tok(cs);
    } else if (tok == TOK_STATIC) {
        vis = VIS_STATIC;
        next_tok(cs);
    } else if (tok == TOK_VAR) {
        vis = VIS_DEFAULT;
        next_tok(cs);
    }

    /* func keyword (no-return function definition) */
    int explicit_func = 0;
    if (cur_tok(cs) == TOK_FUNC) {
        explicit_func = 1;
        next_tok(cs);
    }

    /* align N { ... } byte-alignment block */
    if (cur_tok(cs) == TOK_ALIGN) {
        next_tok(cs);
        int align_n = 1;
        if (cur_tok(cs) == TOK_INT_CONST) {
            align_n = (int)cs->parser.lex->tok_val.i;
            next_tok(cs);
        }
        cgen_raw("#pragma pack(push, %d)", align_n);
        cgen_line("");
        expect(cs, TOK_LBRACE);
        skip_newlines(cs);
        while (cur_tok(cs) != TOK_RBRACE && cur_tok(cs) != TOK_EOF) {
            parse_declaration(cs);
            skip_newlines(cs);
        }
        expect(cs, TOK_RBRACE);
        cgen_line("#pragma pack(pop)");
        return;
    }

    /* Alias */
    if (cur_tok(cs) == TOK_ALIAS) {
        next_tok(cs);
        if (cur_tok(cs) != TOK_IDENTIFIER) {
            nihao_error(cs, "expected alias name, got '%s'", token_name(cur_tok(cs)));
            return;
        }
        char *alias_name = cs->parser.lex->tok_str;
        next_tok(cs);
        expect(cs, TOK_ASSIGN);   /* expect() already advances */
        CType aliased_type;
        parse_type(cs, &aliased_type);
        Symbol *as = sym_push(cs, SYM_TYPEDEF, alias_name, &aliased_type);
        as->vis = vis;
        cgen_line("typedef %s %s;", c_type_name(&aliased_type), alias_name);
        return;
    }

    /* Multi-variable declaration: var {a = 0, b = 1} i8 */
    if (cur_tok(cs) == TOK_LBRACE) {
        next_tok(cs);
        char names[8][64];
        int nnames = 0;
        int has_init[8] = {0};
        skip_newlines(cs);
        while (cur_tok(cs) != TOK_RBRACE && cur_tok(cs) != TOK_EOF && nnames < 8) {
            if (cur_tok(cs) != TOK_IDENTIFIER) {
                nihao_error(cs, "expected variable name in multi-declaration");
                next_tok(cs);
                continue;
            }
            snprintf(names[nnames], 64, "%s", cs->parser.lex->tok_str);
            next_tok(cs);
            if (cur_tok(cs) == TOK_ASSIGN) {
                has_init[nnames] = 1;
                next_tok(cs);
                parse_expression(cs); /* emits init expr */
            }
            nnames++;
            if (cur_tok(cs) == TOK_COMMA) next_tok(cs);
            skip_newlines(cs);
        }
        expect(cs, TOK_RBRACE);
        CType bt;
        parse_type(cs, &bt);
        /* C:  type a = ...; type b = ...;  (multi-decl handled per-name) */
        cgen_raw("%s%s %s", vis == VIS_CONST ? "const " : "", c_type_name(&bt), names[0]);
        cgen_line(";");
        for (int i = 1; i < nnames; i++) {
            cgen_line("%s%s %s;", vis == VIS_CONST ? "const " : "", c_type_name(&bt), names[i]);
        }
        (void)has_init;
        return;
    }

    /* Main dispatch: must start with a name (identifier) */
    if (cur_tok(cs) != TOK_IDENTIFIER) {
        nihao_error(cs, "unexpected token '%s' at declaration level",
                    token_name(cur_tok(cs)));
        next_tok(cs);
        return;
    }

    char *name = cs->parser.lex->tok_str;

    /* --- Type definition: Name struct|union|enum { ... } --- */
    cs->parser.lex->peek_valid = 0;
    lexer_peek(cs->parser.lex);
    TokenType nxt = cs->parser.lex->peek_tok;
    cs->parser.lex->peek_valid = 0;

    if (nxt == TOK_STRUCT || nxt == TOK_UNION || nxt == TOK_ENUM) {
        next_tok(cs); /* consume name */
        TokenType kw = cur_tok(cs);
        next_tok(cs); /* consume struct/union/enum */

        if (kw == TOK_ENUM) {
            CType et;
            memset(&et, 0, sizeof(et));
            et.kind = TYPE_ENUM;
            et.size = 4;
            Symbol *sym = sym_push(cs, SYM_ENUM, name, &et);
            sym->is_defined = 1;
            cgen_raw("typedef enum %s ", name);
            expect(cs, TOK_LBRACE);
            cgen_line("{");
            cgen_indent();
            int val = 0;
            skip_newlines(cs);
            while (cur_tok(cs) != TOK_RBRACE && cur_tok(cs) != TOK_EOF) {
                if (cur_tok(cs) != TOK_IDENTIFIER) {
                    nihao_error(cs, "expected enum variant name");
                    next_tok(cs);
                    continue;
                }
                char *vname = cs->parser.lex->tok_str;
                next_tok(cs);
                if (cur_tok(cs) == TOK_ASSIGN) {
                    next_tok(cs);
                    if (cur_tok(cs) == TOK_INT_CONST) {
                        val = (int)cs->parser.lex->tok_val.i;
                        next_tok(cs);
                    }
                }
                cgen_raw("%s", vname);
                cgen_raw(" = %d", val);
                cgen_line(",");
                CType vt;
                memset(&vt, 0, sizeof(vt));
                vt.kind = TYPE_ENUM;
                vt.size = 4;
                Symbol *vs = sym_push(cs, SYM_ENUM, vname, &vt);
                vs->addr = (unsigned long)val;
                val++;
                if (cur_tok(cs) == TOK_COMMA) next_tok(cs);
                skip_newlines(cs);
            }
            expect(cs, TOK_RBRACE);
            cgen_dedent();
            cgen_line("} %s;", name);
            return;
        }

        /* struct / union definition */
        CType st;
        memset(&st, 0, sizeof(st));
        st.kind = (kw == TOK_STRUCT) ? TYPE_STRUCT : TYPE_UNION;
        Symbol *sym = sym_push(cs, (kw == TOK_STRUCT) ? SYM_STRUCT : SYM_UNION,
                               name, &st);
        sym->is_defined = 1;
        cgen_raw("typedef %s %s ", kw == TOK_STRUCT ? "struct" : "union", name);
        expect(cs, TOK_LBRACE);
        cgen_line("{");
        cgen_indent();
        parse_member_list(cs, sym, kw == TOK_UNION);
        cgen_dedent();
        cgen_line("} %s;", name);
        return;
    }

    /* --- Function: Name(params) [ret] {body|;} --- */
    if (nxt == TOK_LPAREN || explicit_func) {
        next_tok(cs); /* consume name */
        if (cur_tok(cs) != TOK_LPAREN) {
            nihao_error(cs, "expected '(' after function name '%s'", name);
            return;
        }
        next_tok(cs); /* ( */

        CType *ftype = type_new(cs, TYPE_FUNC);
        Symbol *func_sym = sym_push(cs, SYM_FUNCTION, name, ftype);
        func_sym->vis = vis;

        /* parameters: name Type, name Type, ... */
        char cparams[1024];
        cparams[0] = '\0';
        skip_newlines(cs);
        if (cur_tok(cs) != TOK_RPAREN) {
            for (;;) {
                /* optional param visibility prefix */
                if (cur_tok(cs) == TOK_FLOW || cur_tok(cs) == TOK_STATIC ||
                    cur_tok(cs) == TOK_CONST || cur_tok(cs) == TOK_VAR) {
                    next_tok(cs);
                }
        if (cur_tok(cs) != TOK_IDENTIFIER) {
            nihao_error(cs, "expected parameter name, got '%s'",
                        token_name(cur_tok(cs)));
            break;
        }
        char *pname = cs->parser.lex->tok_str;
        next_tok(cs);
        CType ptype;
        parse_type(cs, &ptype);
        Symbol *param = sym_push_local(cs, func_sym, pname, &ptype);
        param->vis = VIS_DEFAULT;
        param->next = func_sym->params;
        func_sym->params = param;

                char one[128];
                snprintf(one, sizeof(one), "%s%s %s%s",
                         cparams[0] ? ", " : "", c_type_name(&ptype), pname,
                         c_type_suffix(&ptype));
                strncat(cparams, one, sizeof(cparams) - strlen(cparams) - 1);

                if (cur_tok(cs) == TOK_COMMA) {
                    next_tok(cs);
                    skip_newlines(cs);
                    continue;
                }
                break;
            }
        }
        expect(cs, TOK_RPAREN);

        /* optional return type after ')' */
        CType ret_type;
        int has_ret = 0;
        skip_newlines(cs);
        if (is_type_begin(cur_tok(cs)) || is_user_type_name(cs) || cur_tok(cs) == TOK_VOID) {
            parse_type(cs, &ret_type);
            has_ret = 1;
        }

        /* determine C return type */
        char cret[128];
        int is_main = (strcmp(name, "main") == 0);
        if (is_main) {
            snprintf(cret, sizeof(cret), "int");
        } else if (has_ret && ret_type.kind != TYPE_VOID) {
            snprintf(cret, sizeof(cret), "%s", c_type_name(&ret_type));
        } else if (vis == VIS_FLOW || vis == VIS_CONST) {
            /* flow/const function returning a pointer -> void* */
            snprintf(cret, sizeof(cret), "void*");
        } else {
            snprintf(cret, sizeof(cret), "void");
        }

        /* emit C signature */
        if (is_main) {
            cgen_raw("%sint main(", attr);
        } else {
            cgen_raw("%s%s %s(", attr, cret, name);
        }
        if (cparams[0]) {
            cgen_raw("%s", cparams);
        } else if (!is_main) {
            cgen_raw("void");
        }
        cgen_raw(")");

        /* function body or prototype */
        skip_newlines(cs);
        if (cur_tok(cs) == TOK_LBRACE) {
            func_sym->is_defined = 1;
            parse_function(cs, func_sym);
        } else if (cur_tok(cs) == TOK_SEMICOLON) {
            next_tok(cs);
            cgen_line(";");
        } else {
            cgen_line("; /* prototype (no body) */");
        }
        return;
    }

    /* --- Variable: Name Type [= expr]  |  Name = expr (inferred) --- */
    next_tok(cs); /* consume name */

    CType vtype;
    int is_const = (vis == VIS_CONST);
    int is_static = (vis == VIS_STATIC);

    if (is_type_begin(cur_tok(cs)) || is_user_type_name(cs) || cur_tok(cs) == TOK_VOID) {
        parse_type(cs, &vtype);
    } else {
        /* type-inferred variable */
        if (cur_tok(cs) != TOK_ASSIGN) {
            nihao_error(cs, "expected type or '=' for variable '%s'", name);
            return;
        }
        infer_init_type(cs, &vtype);
        /* skip the '=' so init below handles it */
    }

    Symbol *var_sym;
    if (cs->parser.cur_func) {
        var_sym = sym_push_local(cs, cs->parser.cur_func, name, &vtype);
    } else {
        var_sym = sym_push(cs, SYM_VARIABLE, name, &vtype);
    }
    var_sym->vis = vis;

    /* emit declaration header */
    cgen_raw("%s%s%s %s%s", is_const ? "const " : "", is_static ? "static " : "",
             c_type_name(&vtype), name, c_type_suffix(&vtype));

    /* initializer */
    if (cur_tok(cs) == TOK_ASSIGN) {
        next_tok(cs);
        cgen_raw(" = ");
        /* Ownership/lifetime check: single-identifier initializer */
        if (cur_tok(cs) == TOK_IDENTIFIER) {
            LexerState *lex = cs->parser.lex;
            lex->peek_valid = 0;
            lexer_peek(lex);
            TokenType after = lex->peek_tok;
            lex->peek_valid = 0;
            if (!is_expr_continuer(after)) {
                Symbol *init_sym = sym_find(cs, cs->parser.lex->tok_str);
                if (init_sym && init_sym->kind == SYM_VARIABLE) {
                    vis_check_assign(cs, var_sym->vis, init_sym, var_sym, name);
                }
            }
        }
        /* Type{...} compound literal: Person{name, age} -> (Person){...} */
        if (cur_tok(cs) == TOK_IDENTIFIER &&
            (cs->parser.lex->peek_tok == 0 || 1)) {
            /* handled generically below */
        }
        parse_expression(cs);
    }
    cgen_line(";");
}

/* ============================================================
 * Function Parsing
 * ============================================================ */

void parse_function(CompilerState *cs, Symbol *func_sym)
{
    Symbol *prev_func = cs->parser.cur_func;
    cs->parser.cur_func = func_sym;

    expect(cs, TOK_LBRACE);
    cgen_raw(" {");   /* signature line ended with ')' */
    cgen_line("");
    cgen_indent();
    skip_newlines(cs);

    /* Parse function body */
    while (cur_tok(cs) != TOK_RBRACE && cur_tok(cs) != TOK_EOF) {
        parse_statement(cs);
        skip_newlines(cs);
    }

    /* Auto-free flow pointers declared at function scope
     * (ownership-transferred ones are excluded via return handling) */
    for (Symbol *s = func_sym->locals; s; s = s->next) {
        if (s->vis == VIS_FLOW && s->type &&
            (s->type->kind == TYPE_VOID || s->type->kind == TYPE_POINTER ||
             s->type->kind == TYPE_STRING) &&
            !s->ownership_transferred) {
            cgen_line("free(%s);", s->name);
        }
    }

    /* main() must return 0 explicitly */
    if (strcmp(func_sym->name, "main") == 0) {
        cgen_line("return 0;");
    }

    expect(cs, TOK_RBRACE);
    cgen_dedent();
    cgen_line("}");

    /* Pop local symbols */
    sym_pop_locals(cs, func_sym);

    cs->parser.cur_func = prev_func;
}

/* ============================================================
 * Statement Parsing
 * ============================================================ */

/* ============================================================
 * Statement Parsing (emits C)
 * ============================================================ */

static void parse_if_stmt(CompilerState *cs);

static void parse_is_stmt(CompilerState *cs)
{
    /* Pattern match inside a while loop:
     *   is -1 { ... }   /   is 0..50 { ... }   /   is _flow => expr
     * Matches against the implicit __is_val temp (set by while). */
    next_tok(cs);
    cgen_raw("if (__is_val");
    TokenType t = cur_tok(cs);
    if (t == TOK_MINUS) {
        next_tok(cs);
        if (cur_tok(cs) == TOK_INT_CONST) {
            cgen_raw(" == -%lld", (long long)cs->parser.lex->tok_val.i);
            next_tok(cs);
        }
    } else if (t == TOK_INT_CONST) {
        long long v = (long long)cs->parser.lex->tok_val.i;
        next_tok(cs);
        if (cur_tok(cs) == TOK_RANGE) {
            next_tok(cs);
            if (cur_tok(cs) == TOK_INT_CONST) {
                long long hi = (long long)cs->parser.lex->tok_val.i;
                cgen_raw(" >= %lld && __is_val <= %lld", v, hi);
                next_tok(cs);
            }
        } else {
            cgen_raw(" == %lld", v);
        }
    } else if (t == TOK_IDENTIFIER) {
        const char *pat = cs->parser.lex->tok_str;
        if (strcmp(pat, "_flow") == 0)     cgen_raw(" == NH_FLOW");
        else if (strcmp(pat, "_static") == 0) cgen_raw(" == NH_STATIC");
        else if (strcmp(pat, "_const") == 0)  cgen_raw(" == NH_CONST");
        else if (strcmp(pat, "_var") == 0)    cgen_raw(" == NH_VAR");
        else if (strcmp(pat, "_undef") == 0)  cgen_raw(" == NH_UNDEF");
        else cgen_raw(" == %s", pat);
        next_tok(cs);
    } else if (t == TOK__UNDEF || t == TOK__CONST || t == TOK__FLOW ||
               t == TOK__STATIC || t == TOK__VAR) {
        /* 可见性枚举 token：_flow 等是关键字，不走 identifier 分支（TODO P1 修复） */
        cgen_raw(" == %s", t == TOK__UNDEF ? "NH_UNDEF" :
                           t == TOK__CONST ? "NH_CONST" :
                           t == TOK__FLOW  ? "NH_FLOW"  :
                           t == TOK__STATIC? "NH_STATIC" : "NH_VAR");
        next_tok(cs);
    } else {
        nihao_error(cs, "invalid 'is' pattern");
        next_tok(cs);
    }
    cgen_raw(")");

    if (cur_tok(cs) == TOK_LBRACE) {
        parse_statement(cs);
    } else if (cur_tok(cs) == TOK_ARROW) {
        next_tok(cs);
        cgen_raw(" { ");
        parse_expression(cs);
        cgen_line("; }");
    } else {
        nihao_error(cs, "expected block or '=>' after 'is' pattern");
    }
}

void parse_statement(CompilerState *cs)
{
    TokenType tok = cur_tok(cs);


    switch (tok) {
        case TOK_IF:
            parse_if_stmt(cs);
            break;

        case TOK_WHILE:
            next_tok(cs);
            cgen_line("{");
            cgen_indent();
            cgen_line("int __is_val;");
            cgen_line("for (;;) {");
            cgen_indent();
            cgen_raw("__is_val = (");
            parse_expression(cs);
            cgen_raw(")");
            cgen_line(";");
            parse_statement(cs);           /* body */
            cgen_line("if (!__is_val) break;");
            cgen_dedent();
            cgen_line("}");
            cgen_dedent();
            cgen_line("}");
            break;

        case TOK_DO:
            /* NihaoC: do cond { ... }  ->  C: while (cond) { ... } */
            next_tok(cs);
            cgen_raw("while (");
            parse_expression(cs);
            cgen_raw(")");
            parse_statement(cs);
            break;

        case TOK_FOR:
            next_tok(cs);
            cgen_raw("for (");
            /* init may be a type-inferred declaration: "for i = 0; ..." */
            if (cur_tok(cs) == TOK_IDENTIFIER) {
                LexerState *lex = cs->parser.lex;
                lex->peek_valid = 0;
                lexer_peek(lex);
                TokenType nt = lex->peek_tok;
                lex->peek_valid = 0;
                if (nt == TOK_ASSIGN && !sym_find(cs, cs->parser.lex->tok_str)) {
                    char *iname = cs->parser.lex->tok_str;
                    next_tok(cs); /* name */
                    next_tok(cs); /* = */
                    CType it;
                    infer_init_type(cs, &it);
                    if (cs->parser.cur_func) {
                        Symbol *isym = sym_push_local(cs, cs->parser.cur_func,
                                                     iname, &it);
                        isym->vis = VIS_DEFAULT;
                    }
                    cgen_raw("%s %s%s = ", c_type_name(&it), iname,
                             c_type_suffix(&it));
                    parse_expression(cs);
                } else {
                    parse_expression(cs);
                }
            } else {
                parse_expression(cs);
            }
            if (cur_tok(cs) == TOK_SEMICOLON) next_tok(cs);
            cgen_raw("; ");
            parse_expression(cs);          /* condition */
            if (cur_tok(cs) == TOK_SEMICOLON) next_tok(cs);
            cgen_raw("; ");
            parse_expression(cs);          /* increment */
            cgen_raw(")");
            parse_statement(cs);           /* body */
            break;

        case TOK_RETURN:
            next_tok(cs);
            if (cur_tok(cs) == TOK_NEWLINE || cur_tok(cs) == TOK_RBRACE ||
                cur_tok(cs) == TOK_SEMICOLON || cur_tok(cs) == TOK_EOF) {
                Symbol *f = cs->parser.cur_func;
                if (f && strcmp(f->name, "main") == 0) {
                    cgen_line("return 0;");
                } else {
                    cgen_line("return;");
                }
            } else {
                cgen_raw("return ");
                if (cur_tok(cs) == TOK_LBRACE) {
                    /* return {v1, v2, ...} compound literal */
                    next_tok(cs);
                    cgen_raw("(type__compound)");
                    cgen_raw("{");
                    if (cur_tok(cs) != TOK_RBRACE) {
                        parse_expression(cs);
                        while (cur_tok(cs) == TOK_COMMA) {
                            next_tok(cs);
                            cgen_raw(", ");
                            parse_expression(cs);
                        }
                    }
                    cgen_raw("}");
                    expect(cs, TOK_RBRACE);
                } else {
                    /* Returning a flow pointer transfers ownership:
                     * mark it so the auto-free pass skips it. */
                    if (cur_tok(cs) == TOK_IDENTIFIER) {
                        Symbol *rs = sym_find(cs, cs->parser.lex->tok_str);
                        if (rs && rs->vis == VIS_FLOW && rs->type &&
                            (rs->type->kind == TYPE_VOID ||
                             rs->type->kind == TYPE_POINTER ||
                             rs->type->kind == TYPE_STRING)) {
                            rs->ownership_transferred = 1;
                        }
                    }
                    parse_expression(cs);
                }
                cgen_line(";");
            }
            if (cur_tok(cs) == TOK_SEMICOLON) next_tok(cs);
            break;

        case TOK_BREAK:
            next_tok(cs);
            cgen_line("break;");
            break;

        case TOK_CONTINUE:
            next_tok(cs);
            cgen_line("continue;");
            break;

        case TOK_IS:
            parse_is_stmt(cs);
            break;

        case TOK_LBRACE: {
            /* Block statement */
            next_tok(cs);
            vis_scope_enter(cs);
            Symbol *scope_start = cs->parser.cur_func ?
                                  cs->parser.cur_func->locals : NULL;
            cgen_line("{");
            cgen_indent();
            skip_newlines(cs);
            while (cur_tok(cs) != TOK_RBRACE && cur_tok(cs) != TOK_EOF) {
                parse_statement(cs);
                skip_newlines(cs);
            }
            /* Auto-free flow pointers declared in this scope */
            if (cs->parser.cur_func) {
                for (Symbol *s = cs->parser.cur_func->locals;
                     s && s != scope_start; s = s->next) {
                    if (s->vis == VIS_FLOW && s->type &&
                        (s->type->kind == TYPE_VOID ||
                         s->type->kind == TYPE_POINTER ||
                         s->type->kind == TYPE_STRING) &&
                        !s->ownership_transferred) {
                        cgen_line("free(%s);", s->name);
                    }
                }
            }
            /* Pop block-scoped locals (real scoping: names declared inside
             * this block are no longer visible, and the function-level
             * auto-free pass won't double-free them) */
            if (cs->parser.cur_func) {
                /* Unfreeze sources borrowed by vars in this scope (ch.13.1) */
                vis_unfreeze_borrows(cs->parser.cur_func->locals, scope_start);
                cs->parser.cur_func->locals = scope_start;
            }
            cgen_dedent();
            cgen_line("}");
            vis_scope_exit(cs);
            expect(cs, TOK_RBRACE);
            break;
        }

        case TOK_COOKING:
            /* Compile-time execution block: skip for core subset */
            next_tok(cs);
            expect(cs, TOK_LBRACE);
            {
                int depth = 1;
                while (depth > 0 && cur_tok(cs) != TOK_EOF) {
                    if (cur_tok(cs) == TOK_LBRACE) depth++;
                    else if (cur_tok(cs) == TOK_RBRACE) depth--;
                    if (depth > 0) next_tok(cs);
                }
                expect(cs, TOK_RBRACE);
            }
            break;

        default:
            /* Declaration or expression statement */
            if (is_type_begin(cur_tok(cs)) || cur_tok(cs) == TOK_ALIAS) {
                parse_declaration(cs);
            } else if (cur_tok(cs) == TOK_IDENTIFIER) {
                /* local declaration "name Type" vs expression */
                LexerState *lex = cs->parser.lex;
                lex->peek_valid = 0;
                lexer_peek(lex);
                TokenType nt = lex->peek_tok;
                lex->peek_valid = 0;
                if (is_type_begin(nt) || nt == TOK_VOID || nt == TOK_IDENTIFIER) {
                    /* name Type / name UserType -> declaration */
                    parse_declaration(cs);
                } else if (nt == TOK_ASSIGN || nt == TOK_SAFE_ASSIGN) {
                    /* name = expr: assignment if already declared,
                     * otherwise a type-inferred declaration */
                    if (sym_find(cs, cs->parser.lex->tok_str)) {
                        parse_expression(cs);
                        cgen_line(";");
                        if (cur_tok(cs) == TOK_SEMICOLON) next_tok(cs);
                    } else {
                        parse_declaration(cs);
                    }
                } else {
                    parse_expression(cs);
                    cgen_line(";");
                    if (cur_tok(cs) == TOK_SEMICOLON) next_tok(cs);
                }
            } else {
                parse_expression(cs);
                cgen_line(";");
                if (cur_tok(cs) == TOK_SEMICOLON) next_tok(cs);
            }
            break;
    }
}

/* if (cond) stmt  [else if (cond) stmt]* [else stmt] */
static void parse_if_stmt(CompilerState *cs)
{
    next_tok(cs);
    cgen_raw("if (");
    parse_expression(cs);
    cgen_raw(")");
    parse_statement(cs);

    while (cur_tok(cs) == TOK_ELSE) {
        next_tok(cs);
        if (cur_tok(cs) == TOK_IF) {
            next_tok(cs);
            cgen_raw("else if (");
            parse_expression(cs);
            cgen_raw(")");
            parse_statement(cs);
        } else {
            cgen_raw("else");
            parse_statement(cs);
            break;
        }
    }
}

/* ============================================================
 * Expression Parsing (one-pass, emits C code via cgen)
 *
 * Precedence chain (highest at the bottom):
 *   assign -> ternary -> || -> && -> | -> ^ -> & -> == != -> < > <= >=
 *         -> << >> -> + - -> * / % -> unary -> postfix -> primary
 * ============================================================ */

static void parse_postfix(CompilerState *cs, int line);

/* 关键字内置函数：sizeof/typeof/alignof/offsetof/visof。
 * 这些是关键字 token（TOK_SIZEOF 等），identifier 分支的字符串比较
 * 永远走不到 → 在 parse_primary 的 switch 中直接分派到这里（修复 TODO P1）。 */
static void parse_builtin_kw(CompilerState *cs, TokenType kw)
{
    next_tok(cs);               /* 关键字 */
    if (cur_tok(cs) != TOK_LPAREN) {
        nihao_error(cs, "expected '(' after builtin");
        return;
    }
    next_tok(cs);               /* ( */

    if (kw == TOK_SIZEOF || kw == TOK_TYPEOF) {
        CType tmp;
        if (is_type_begin(cur_tok(cs)) || is_user_type_name(cs)) {
            parse_type(cs, &tmp);
            cgen_raw("sizeof(%s)", c_type_name(&tmp));
        } else {
            cgen_raw("sizeof(");
            parse_expression(cs);
            cgen_raw(")");
        }
        if (cur_tok(cs) != TOK_RPAREN) nihao_error(cs, "expected ')' in sizeof/typeof");
        else next_tok(cs);
        return;
    }
    if (kw == TOK_ALIGNOF) {
        CType tmp;
        parse_type(cs, &tmp);
        cgen_raw("_Alignof(%s)", c_type_name(&tmp));
        if (cur_tok(cs) != TOK_RPAREN) nihao_error(cs, "expected ')' in alignof");
        else next_tok(cs);
        return;
    }
    if (kw == TOK_OFFSETOF) {
        CType tmp;
        parse_type(cs, &tmp);
        if (cur_tok(cs) != TOK_COMMA) {
            nihao_error(cs, "offsetof expects (type, member)");
        } else {
            next_tok(cs);
            char *member = cs->parser.lex->tok_str;
            next_tok(cs);
            cgen_raw("offsetof(%s, %s)", c_type_name(&tmp), member);
        }
        if (cur_tok(cs) != TOK_RPAREN) nihao_error(cs, "expected ')' in offsetof");
        else next_tok(cs);
        return;
    }
    if (kw == TOK_VISOF) {
        if (cur_tok(cs) == TOK_IDENTIFIER) {
            Symbol *s = sym_find(cs, cs->parser.lex->tok_str);
            next_tok(cs);
            if (s) {
                switch (s->vis) {
                    case VIS_CONST:  cgen_raw("NH_CONST"); break;
                    case VIS_FLOW:   cgen_raw("NH_FLOW"); break;
                    case VIS_STATIC: cgen_raw("NH_STATIC"); break;
                    case VIS_UNDEF:  cgen_raw("NH_UNDEF"); break;
                    default:         cgen_raw("NH_VAR"); break;
                }
            } else {
                cgen_raw("NH_UNDEF");
            }
        } else {
            nihao_error(cs, "visof expects an identifier");
        }
        if (cur_tok(cs) != TOK_RPAREN) nihao_error(cs, "expected ')' in visof");
        else next_tok(cs);
        return;
    }
    nihao_error(cs, "unknown builtin keyword");
}

static void parse_primary(CompilerState *cs)
{
    TokenType tok = cur_tok(cs);

    switch (tok) {
        case TOK_SIZEOF: case TOK_TYPEOF: case TOK_ALIGNOF:
        case TOK_OFFSETOF: case TOK_VISOF:
            /* 关键字内置函数（TODO P1 修复） */
            parse_builtin_kw(cs, tok);
            break;
        case TOK__UNDEF: case TOK__CONST: case TOK__FLOW:
        case TOK__STATIC: case TOK__VAR:
            /* 可见性枚举常量 _undef/_const/_flow/_static/_var → NH_* */
            cgen_raw(tok == TOK__UNDEF ? "NH_UNDEF" :
                     tok == TOK__CONST ? "NH_CONST" :
                     tok == TOK__FLOW  ? "NH_FLOW"  :
                     tok == TOK__STATIC? "NH_STATIC" : "NH_VAR");
            next_tok(cs);
            break;
        case TOK_INT_CONST:
            cgen_raw("%lld", (long long)cs->parser.lex->tok_val.i);
            next_tok(cs);
            break;
        case TOK_FLOAT_CONST:
            cgen_raw("%.10g", cs->parser.lex->tok_val.f);
            next_tok(cs);
            break;
        case TOK_CHAR_CONST:
            cgen_raw("'\\x%02x'", (unsigned)(cs->parser.lex->tok_val.i & 0xff));
            next_tok(cs);
            break;
        case TOK_STRING_LITERAL:
            cgen_raw("\"%s\"", cs->parser.lex->tok_str ? cs->parser.lex->tok_str : "");
            next_tok(cs);
            break;
        case TOK_TRUE:
            cgen_raw("true");
            next_tok(cs);
            break;
        case TOK_FALSE:
            cgen_raw("false");
            next_tok(cs);
            break;
        case TOK_IDENTIFIER: {
            char *name = cs->parser.lex->tok_str;
            Symbol *sym = sym_find(cs, name);
            cs->parser.last_ident = sym;
            /* Using a moved-from (invalid) variable is an error */
            if (sym && sym->kind == SYM_VARIABLE) {
                vis_check_usable(cs, sym);
            }
            next_tok(cs);

            /* Builtin calls: sizeof/typeof/alignof/offsetof/visof/malloc/... */
            if (cur_tok(cs) == TOK_LPAREN) {
                next_tok(cs); /* ( */

                if (strcmp(name, "sizeof") == 0) {
                    /* sizeof(type) or sizeof(expr) */
                    CType tmp;
                    if (is_type_begin(cur_tok(cs))) {
                        parse_type(cs, &tmp);
                        cgen_raw("sizeof(%s)", c_type_name(&tmp));
                    } else {
                        cgen_raw("sizeof(");
                        parse_expression(cs);
                        cgen_raw(")");
                    }
                    expect(cs, TOK_RPAREN);
                    break;
                }
                if (strcmp(name, "alignof") == 0) {
                    CType tmp;
                    parse_type(cs, &tmp);
                    cgen_raw("_Alignof(%s)", c_type_name(&tmp));
                    expect(cs, TOK_RPAREN);
                    break;
                }
                if (strcmp(name, "offsetof") == 0) {
                    CType tmp;
                    parse_type(cs, &tmp);
                    if (cur_tok(cs) != TOK_COMMA) {
                        nihao_error(cs, "offsetof expects (type, member)");
                    } else {
                        next_tok(cs);
                        char *member = cs->parser.lex->tok_str;
                        next_tok(cs);
                        cgen_raw("offsetof(%s, %s)", c_type_name(&tmp), member);
                    }
                    expect(cs, TOK_RPAREN);
                    break;
                }
                if (strcmp(name, "typeof") == 0) {
                    /* typeof(expr) == type  -> compile-time-ish check via sizeof */
                    CType tmp;
                    if (is_type_begin(cur_tok(cs))) {
                        parse_type(cs, &tmp);
                        cgen_raw("sizeof(%s)", c_type_name(&tmp));
                    } else {
                        cgen_raw("sizeof(");
                        parse_expression(cs);
                        cgen_raw(")");
                    }
                    expect(cs, TOK_RPAREN);
                    break;
                }
                if (strcmp(name, "visof") == 0) {
                    /* visof(symbol) -> compile-time visibility constant */
                    if (cur_tok(cs) == TOK_IDENTIFIER) {
                        Symbol *s = sym_find(cs, cs->parser.lex->tok_str);
                        next_tok(cs);
                        if (s) {
                            switch (s->vis) {
                                case VIS_CONST:  cgen_raw("NH_CONST"); break;
                                case VIS_FLOW:   cgen_raw("NH_FLOW"); break;
                                case VIS_STATIC: cgen_raw("NH_STATIC"); break;
                                case VIS_UNDEF:  cgen_raw("NH_UNDEF"); break;
                                default:         cgen_raw("NH_VAR"); break;
                            }
                        } else {
                            cgen_raw("NH_UNDEF");
                        }
                    } else {
                        nihao_error(cs, "visof expects an identifier");
                    }
                    expect(cs, TOK_RPAREN);
                    break;
                }
                if (strcmp(name, "malloc") == 0) {
                    /* malloc(i32) / malloc(u8, 100) / malloc(void[3]) */
                    CType tmp;
                    if (cur_tok(cs) == TOK_INT_CONST) {
                        /* malloc(64): raw byte count */
                        cgen_raw("malloc(%lld)", (long long)cs->parser.lex->tok_val.i);
                        next_tok(cs);
                    } else {
                        parse_type(cs, &tmp);
                        cgen_raw("malloc(sizeof(%s)", c_type_name(&tmp));
                        if (cur_tok(cs) == TOK_COMMA) {
                            next_tok(cs);
                            cgen_raw(" * (");
                            parse_expression(cs);
                            cgen_raw(")");
                        }
                        cgen_raw(")");
                    }
                    expect(cs, TOK_RPAREN);
                    break;
                }
                if (strcmp(name, "print") == 0) {
                    /* print("fmt", args...) -> printf(...);
                     * print(expr)          -> printf("%lld\n", (long long)expr) */
                    if (cur_tok(cs) == TOK_STRING_LITERAL) {
                        cgen_raw("printf(");
                        parse_expression(cs);
                        while (cur_tok(cs) == TOK_COMMA) {
                            next_tok(cs);
                            cgen_raw(", ");
                            parse_expression(cs);
                        }
                        cgen_raw(")");
                    } else {
                        cgen_raw("printf(\"%%lld\\n\", (long long)(");
                        parse_expression(cs);
                        cgen_raw("))");
                    }
                    expect(cs, TOK_RPAREN);
                    break;
                }
                if (strcmp(name, "puts") == 0 ||
                    strcmp(name, "printf") == 0) {
                    /* plain passthrough call */
                    cgen_raw("%s(", name);
                    if (cur_tok(cs) != TOK_RPAREN) {
                        parse_expression(cs);
                        while (cur_tok(cs) == TOK_COMMA) {
                            next_tok(cs);
                            cgen_raw(", ");
                            parse_expression(cs);
                        }
                    }
                    cgen_raw(")");
                    expect(cs, TOK_RPAREN);
                    break;
                }

                /* generic function call */
                cgen_raw("%s(", name);
                if (cur_tok(cs) != TOK_RPAREN) {
                    parse_expression(cs);
                    while (cur_tok(cs) == TOK_COMMA) {
                        next_tok(cs);
                        cgen_raw(", ");
                        parse_expression(cs);
                    }
                }
                cgen_raw(")");
                expect(cs, TOK_RPAREN);
            } else {
                /* plain identifier (variable/function name) */
                cgen_raw("%s", name);
            }
            break;
        }
        case TOK_LPAREN:
            next_tok(cs);
            cgen_raw("(");
            parse_expression(cs);
            cgen_raw(")");
            expect(cs, TOK_RPAREN);
            break;
        default:
            nihao_error(cs, "unexpected token '%s' in expression",
                        token_name(cur_tok(cs)));
            next_tok(cs);
            break;
    }
}

/* Dereference chain: x.(T) / x?.(T) / x.()  followed by .m [i] (args) ... */
static void parse_deref_chain(CompilerState *cs)
{
    char *name = cs->parser.lex->tok_str;
    Symbol *sym = sym_find(cs, name);
    cs->parser.last_ident = sym;
    if (sym && sym->kind == SYM_VARIABLE) {
        vis_check_usable(cs, sym);
    }
    next_tok(cs);                       /* consume ident */
    TokenType op = cur_tok(cs);         /* .( or ?. */
    next_tok(cs);                       /* consume .( */

    if (cur_tok(cs) == TOK_RPAREN) {
        /* .() : dereference a void* one level -> yields void* */
        next_tok(cs);                   /* consume ) */
        cgen_raw("(*(void**)%s)", name);
    } else {
        CType tmp;
        parse_type(cs, &tmp);
        cgen_raw("(*(%s*)%s)", c_type_name(&tmp), name);
        expect(cs, TOK_RPAREN);
    }
    (void)op;

    /* continue postfix chain */
    for (;;) {
        TokenType tok = cur_tok(cs);
        if (tok == TOK_DOT) {
            next_tok(cs);
            if (cur_tok(cs) != TOK_IDENTIFIER) {
                nihao_error(cs, "expected member name after '.'");
                next_tok(cs);
            } else {
                cgen_raw(".%s", cs->parser.lex->tok_str);
                next_tok(cs);
            }
        } else if (tok == TOK_ARROW) {
            next_tok(cs);
            if (cur_tok(cs) != TOK_IDENTIFIER) {
                nihao_error(cs, "expected member name after '->'");
                next_tok(cs);
            } else {
                cgen_raw("->%s", cs->parser.lex->tok_str);
                next_tok(cs);
            }
        } else if (tok == TOK_LBRACKET) {
            next_tok(cs);
            if (cur_tok(cs) == TOK_RANGE) {
                next_tok(cs);
                parse_expression(cs);
                expect(cs, TOK_RBRACKET);
                nihao_error(cs, "slice is only supported on assignment targets");
            } else {
                cgen_raw("[");
                parse_expression(cs);
                cgen_raw("]");
                expect(cs, TOK_RBRACKET);
            }
        } else if (tok == TOK_LPAREN) {
            next_tok(cs);
            cgen_raw("(");
            if (cur_tok(cs) != TOK_RPAREN) {
                parse_expression(cs);
                while (cur_tok(cs) == TOK_COMMA) {
                    next_tok(cs);
                    cgen_raw(", ");
                    parse_expression(cs);
                }
            }
            cgen_raw(")");
            expect(cs, TOK_RPAREN);
        } else if (tok == TOK_INCREMENT) {
            cgen_raw("++");
            next_tok(cs);
        } else if (tok == TOK_DECREMENT) {
            cgen_raw("--");
            next_tok(cs);
        } else {
            break;
        }
    }
}

/* Postfix: call / .(T) deref / ?.(T) safe deref / [i] / [a..b] / .member / ++ -- */
static void parse_postfix(CompilerState *cs, int line)
{
    (void)line;
    /* Lookahead: dereference chain "x.(T)" / "x?.(T)" / "x.()" */
    if (cur_tok(cs) == TOK_IDENTIFIER) {
        LexerState *lex = cs->parser.lex;
        lex->peek_valid = 0;
        lexer_peek(lex);
        if (lex->peek_tok == TOK_DOT_PAREN || lex->peek_tok == TOK_SAFE_DOT) {
            lex->peek_valid = 0;
            parse_deref_chain(cs);
            return;
        }
        lex->peek_valid = 0;
    }

    parse_primary(cs);

    for (;;) {
        TokenType tok = cur_tok(cs);
        if (tok == TOK_LPAREN) {
            /* function call */
            next_tok(cs);
            cgen_raw("(");
            if (cur_tok(cs) != TOK_RPAREN) {
                parse_expression(cs);
                while (cur_tok(cs) == TOK_COMMA) {
                    next_tok(cs);
                    cgen_raw(", ");
                    parse_expression(cs);
                }
            }
            cgen_raw(")");
            expect(cs, TOK_RPAREN);
        } else if (tok == TOK_LBRACKET) {
            /* [i] index or [a..b] slice */
            next_tok(cs);
            if (cur_tok(cs) == TOK_RANGE) {
                /* slice read: not directly expressible in C */
                next_tok(cs);
                parse_expression(cs);
                expect(cs, TOK_RBRACKET);
                nihao_error(cs, "slice expression is only supported on assignment targets");
            } else {
                cgen_raw("[");
                parse_expression(cs);
                cgen_raw("]");
                expect(cs, TOK_RBRACKET);
            }
        } else if (tok == TOK_DOT) {
            next_tok(cs);
            if (cur_tok(cs) != TOK_IDENTIFIER) {
                nihao_error(cs, "expected member name after '.'");
                next_tok(cs);
            } else {
                cgen_raw(".%s", cs->parser.lex->tok_str);
                next_tok(cs);
            }
        } else if (tok == TOK_ARROW) {
            next_tok(cs);
            if (cur_tok(cs) != TOK_IDENTIFIER) {
                nihao_error(cs, "expected member name after '->'");
                next_tok(cs);
            } else {
                cgen_raw("->%s", cs->parser.lex->tok_str);
                next_tok(cs);
            }
        } else if (tok == TOK_INCREMENT) {
            cgen_raw("++");
            next_tok(cs);
        } else if (tok == TOK_DECREMENT) {
            cgen_raw("--");
            next_tok(cs);
        } else {
            break;
        }
    }
}

static void parse_unary(CompilerState *cs, int line)
{
    TokenType tok = cur_tok(cs);
    switch (tok) {
        case TOK_MINUS:
            next_tok(cs);
            cgen_raw("-");
            parse_unary(cs, line);
            break;
        case TOK_LOGICAL_NOT:
            next_tok(cs);
            cgen_raw("!");
            parse_unary(cs, line);
            break;
        case TOK_BITWISE_NOT:
            next_tok(cs);
            cgen_raw("~");
            parse_unary(cs, line);
            break;
        case TOK_BITWISE_AND:
            next_tok(cs);
            cgen_raw("&");
            parse_unary(cs, line);
            break;
        case TOK_STAR:
            next_tok(cs);
            cgen_raw("*");
            parse_unary(cs, line);
            break;
        case TOK_INCREMENT:
            next_tok(cs);
            cgen_raw("++");
            parse_unary(cs, line);
            break;
        case TOK_DECREMENT:
            next_tok(cs);
            cgen_raw("--");
            parse_unary(cs, line);
            break;
        default:
            parse_postfix(cs, line);
            break;
    }
}

#define BINOP_NEXT_LEVEL(fn, toks) \
    static void fn(CompilerState *cs) { \
        parse_multiplicative(cs); \
        for (;;) { \
            TokenType t = cur_tok(cs); \
            if (t == TOK_STAR) { next_tok(cs); cgen_raw(" * "); parse_multiplicative(cs); } \
            else if (t == TOK_SLASH) { next_tok(cs); cgen_raw(" / "); parse_multiplicative(cs); } \
            else if (t == TOK_PERCENT) { next_tok(cs); cgen_raw(" %% "); parse_multiplicative(cs); } \
            else break; \
        } \
    }

static void parse_multiplicative(CompilerState *cs, int line)
{
    parse_unary(cs, line);
    for (;;) {
        TokenType t = cur_tok(cs);
        if (cs->parser.lex->line_num != line) break;
        if (t == TOK_STAR) { next_tok(cs); cgen_raw(" * "); parse_unary(cs, line); }
        else if (t == TOK_SLASH) { next_tok(cs); cgen_raw(" / "); parse_unary(cs, line); }
        else if (t == TOK_PERCENT) { next_tok(cs); cgen_raw(" %% "); parse_unary(cs, line); }
        else break;
    }
}

static void parse_additive(CompilerState *cs, int line)
{
    parse_multiplicative(cs, line);
    for (;;) {
        TokenType t = cur_tok(cs);
        if (cs->parser.lex->line_num != line) break;
        if (t == TOK_PLUS) { next_tok(cs); cgen_raw(" + "); parse_multiplicative(cs, line); }
        else if (t == TOK_MINUS) { next_tok(cs); cgen_raw(" - "); parse_multiplicative(cs, line); }
        else break;
    }
}

static void parse_shift(CompilerState *cs, int line)
{
    parse_additive(cs, line);
    for (;;) {
        TokenType t = cur_tok(cs);
        if (cs->parser.lex->line_num != line) break;
        if (t == TOK_LEFT_SHIFT) { next_tok(cs); cgen_raw(" << "); parse_additive(cs, line); }
        else if (t == TOK_RIGHT_SHIFT) { next_tok(cs); cgen_raw(" >> "); parse_additive(cs, line); }
        else break;
    }
}

static void parse_relational(CompilerState *cs, int line)
{
    parse_shift(cs, line);
    for (;;) {
        TokenType t = cur_tok(cs);
        if (cs->parser.lex->line_num != line) break;
        if (t == TOK_LT) { next_tok(cs); cgen_raw(" < "); parse_shift(cs, line); }
        else if (t == TOK_GT) { next_tok(cs); cgen_raw(" > "); parse_shift(cs, line); }
        else if (t == TOK_LE) { next_tok(cs); cgen_raw(" <= "); parse_shift(cs, line); }
        else if (t == TOK_GE) { next_tok(cs); cgen_raw(" >= "); parse_shift(cs, line); }
        else break;
    }
}

static void parse_equality(CompilerState *cs, int line)
{
    parse_relational(cs, line);
    for (;;) {
        TokenType t = cur_tok(cs);
        if (cs->parser.lex->line_num != line) break;
        if (t == TOK_EQ) { next_tok(cs); cgen_raw(" == "); parse_relational(cs, line); }
        else if (t == TOK_NE) { next_tok(cs); cgen_raw(" != "); parse_relational(cs, line); }
        else break;
    }
}

static void parse_bitand(CompilerState *cs, int line)
{
    parse_equality(cs, line);
    while (cur_tok(cs) == TOK_BITWISE_AND &&
           cs->parser.lex->line_num == line) { next_tok(cs); cgen_raw(" & "); parse_equality(cs, line); }
}

static void parse_bitxor(CompilerState *cs, int line)
{
    parse_bitand(cs, line);
    while (cur_tok(cs) == TOK_BITWISE_XOR &&
           cs->parser.lex->line_num == line) { next_tok(cs); cgen_raw(" ^ "); parse_bitand(cs, line); }
}

static void parse_bitor(CompilerState *cs, int line)
{
    parse_bitxor(cs, line);
    while (cur_tok(cs) == TOK_BITWISE_OR &&
           cs->parser.lex->line_num == line) { next_tok(cs); cgen_raw(" | "); parse_bitxor(cs, line); }
}

static void parse_logical_and(CompilerState *cs, int line)
{
    parse_bitor(cs, line);
    while (cur_tok(cs) == TOK_LOGICAL_AND &&
           cs->parser.lex->line_num == line) { next_tok(cs); cgen_raw(" && "); parse_bitor(cs, line); }
}

static void parse_logical_or(CompilerState *cs, int line)
{
    parse_logical_and(cs, line);
    while (cur_tok(cs) == TOK_LOGICAL_OR &&
           cs->parser.lex->line_num == line) { next_tok(cs); cgen_raw(" || "); parse_logical_and(cs, line); }
}

static void parse_ternary(CompilerState *cs, int line)
{
    parse_logical_or(cs, line);
    if (cur_tok(cs) == TOK_QUESTION) {
        next_tok(cs);
        cgen_raw(" ? ");
        parse_ternary(cs, line);
        expect(cs, TOK_COLON);
        cgen_raw(" : ");
        parse_ternary(cs, line);
    }
}

static void parse_assign(CompilerState *cs, int line)
{
    parse_ternary(cs, line);
    TokenType t = cur_tok(cs);
    if (cs->parser.lex->line_num != line) return;
    switch (t) {
        case TOK_ASSIGN:
        case TOK_SAFE_ASSIGN: {
            Symbol *lhs = cs->parser.last_ident;
            /* LHS must be writable (not frozen by an active borrow) */
            if (lhs && lhs->kind == SYM_VARIABLE) {
                vis_check_writable(cs, lhs);
            }
            next_tok(cs);
            cgen_raw(" = ");
            /* Single-identifier RHS: check lifetime/ownership transfer */
            if (lhs && lhs->kind == SYM_VARIABLE && cur_tok(cs) == TOK_IDENTIFIER) {
                LexerState *lex = cs->parser.lex;
                lex->peek_valid = 0;
                lexer_peek(lex);
                TokenType after = lex->peek_tok;
                lex->peek_valid = 0;
                if (!is_expr_continuer(after)) {
                    Symbol *rhs = sym_find(cs, cs->parser.lex->tok_str);
                    if (rhs && rhs->kind == SYM_VARIABLE) {
                        vis_check_assign(cs, lhs->vis, rhs, lhs, lhs->name);
                    }
                }
            }
            parse_assign(cs, line);
            break;
        }
        case TOK_PLUS_ASSIGN:
        case TOK_MINUS_ASSIGN:
        case TOK_STAR_ASSIGN:
        case TOK_SLASH_ASSIGN:
        case TOK_PERCENT_ASSIGN:
        case TOK_AND_ASSIGN:
        case TOK_OR_ASSIGN:
        case TOK_XOR_ASSIGN:
        case TOK_LEFT_SHIFT_ASSIGN:
        case TOK_RIGHT_SHIFT_ASSIGN: {
            Symbol *lhs = cs->parser.last_ident;
            if (lhs && lhs->kind == SYM_VARIABLE) {
                vis_check_writable(cs, lhs);
            }
            next_tok(cs);
            cgen_raw(" %s ", token_name(t));
            parse_assign(cs, line);
            break;
        }
        default:
            break;
    }
}

void parse_expression(CompilerState *cs)
{
    /* 换行即语句边界：binop 链各层按行号停止（函数体内 lexer 不产生 NEWLINE） */
    parse_assign(cs, cs->parser.lex->line_num);
}


/*
 * Updated parser sections to integrate all new features
 * Add these to the original parser.c at the appropriate locations
 */

/* ============================================================
 * Updated parse_function to use full code generation
 * ============================================================ */

void parse_function_full(CompilerState *cs, Symbol *func_sym) {
    Symbol *prev_func = cs->parser.cur_func;
    cs->parser.cur_func = func_sym;

    // /* Check for multireturn type */
    // infer_multireturn_type(cs, func_sym);

    expect(cs, TOK_LBRACE);
    skip_newlines(cs);

    /* Generate function prologue with register saving */
    gen_function_prologue_full(func_sym);

    /* Parse function body */
    while (cur_tok(cs) != TOK_RBRACE && cur_tok(cs) != TOK_EOF) {
        /* Type check statement before generating */
        type_check_statement(cs);

        /* Parse and generate code */
        parse_statement(cs);
        skip_newlines(cs);
    }

    expect(cs, TOK_RBRACE);

    /* Generate function epilogue */
    gen_function_epilogue_full(func_sym);

    /* Pop local symbols */
    sym_pop_locals(cs, func_sym);

    cs->parser.cur_func = prev_func;
}

/* ============================================================
 * Updated statement parsing to dispatch to new code generators
 * ============================================================ */

void parse_statement_full(CompilerState *cs) {
    TokenType tok = cur_tok(cs);

    switch (tok) {
        case TOK_IF:
            next_tok(cs);
            parse_expression(cs);
            gen_if_statement(cs);  /* Full IF code generation */
            break;

        case TOK_WHILE:
            next_tok(cs);
            parse_expression(cs);
            gen_while_loop(cs);  /* Full WHILE code generation */
            break;

        case TOK_FOR:
            next_tok(cs);
            /* Init already executed */
            parse_expression(cs); /* Init */
            expect(cs, TOK_SEMICOLON);
            parse_expression(cs); /* Condition */
            expect(cs, TOK_SEMICOLON);
            parse_expression(cs); /* Increment */
            gen_for_loop(cs);  /* Full FOR code generation */
            break;

        case TOK_DO:
            next_tok(cs);
            gen_do_while_loop(cs);
            break;

        case TOK_RETURN:
            next_tok(cs);
            if (cur_tok(cs) != TOK_NEWLINE && cur_tok(cs) != TOK_RBRACE
                && cur_tok(cs) != TOK_SEMICOLON) {

                // if (cur_tok(cs) == TOK_MULTIRETURN) {
                //     parse_multireturn(cs);
                // } else {
                //     parse_expression(cs);
                // }
                parse_expression(cs);
            }
            gen_return_statement(cs);
            break;

        case TOK_BREAK:
            next_tok(cs);
            /* TODO: Emit jump to loop exit */
            break;

        case TOK_CONTINUE:
            next_tok(cs);
            /* TODO: Emit jump to loop increment/condition */
            break;

        case TOK_LBRACE:
            /* Block */
            next_tok(cs);
            skip_newlines(cs);
            vis_scope_enter(cs);
            while (cur_tok(cs) != TOK_RBRACE && cur_tok(cs) != TOK_EOF) {
                parse_statement_full(cs);
                skip_newlines(cs);
            }
            vis_scope_exit(cs);
            expect(cs, TOK_RBRACE);
            break;

        case TOK_COOKING:
            // parse_cooking_block(cs);
            // cooking_generate_code(cs);
            break;

        default:
            /* Expression statement or declaration */
            if (is_type_begin(cur_tok(cs)) || cur_tok(cs) == TOK_ALIAS) {
                parse_declaration(cs);
            } else {
                parse_expression(cs);
            }
            break;
    }

    /* Consume optional semicolon */
    if (cur_tok(cs) == TOK_SEMICOLON) {
        next_tok(cs);
    }
}

/* ============================================================
 * Updated compilation entry to apply optimizations
 * ============================================================ */

static int compile_file_full(CompilerState *cs, const char *filename) {
    char *source;
    size_t source_size;

    /* Load file */
    source = load_source_file(filename, &source_size);
    if (!source) return -1;

    if (cs->verbose) {
        printf("Compiling: %s (%zu bytes)\n", filename, source_size);
    }

    /* Initialize subsystems */
    LexerState *lex = nihao_malloc(cs, sizeof(LexerState));
    cs->parser.lex = lex;
    lexer_init(cs, filename, source);

    parser_init(cs);
    codegen_init(cs);
    visibility_init(cs);
    linker_init(cs);

    /* Register standard library */
    stdlib_register_all(cs);
    stdlib_resolve_link_libraries(cs);
    stdlib_generate_runtime_stubs(cs);

    /* Parse module */
    parse_module(cs);

    /* Type verification pass */
    verify_types(cs);

    /* Apply peephole optimizations */
    codegen_optimize(cs);

    /* Check errors */
    if (cs->error_count > 0) {
        fprintf(stderr, "Compilation failed with %d error(s), %d warning(s)\n",
                cs->error_count, cs->warning_count);
        return -1;
    }

    /* Generate output */
    switch (cs->output_type) {
        case 0: /* Executable */
            linker_generate_executable_full(cs, cs->output_file);
            break;
        case 1: /* Object file */
            linker_generate_object(cs, cs->output_file);
            break;
        case 2: /* Shared library */
            break;
        case 3: /* Static library */
            break;
    }

    if (cs->verbose) {
        printf("Compilation successful (%d warnings)\n", cs->warning_count);
    }

    return 0;
}

