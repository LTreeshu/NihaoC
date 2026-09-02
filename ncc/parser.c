#include "ncc.h"

/* cooking 编译期（A 方案）：前向声明——parse_statement 内 case 先于定义使用 */
static long long pc_or(CompilerState *cs);
static void parse_static_assert(CompilerState *cs);
static void parse_cooking_block(CompilerState *cs);


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
        || tok == TOK_VAR || tok == TOK_ALIAS;
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
                    /* 参数类型链入 params 链表（头插；c_type_name 收集后反转） */
                    CType *psave = type_new(cs, TYPE_NONE);
                    memcpy(psave, &pt, sizeof(CType));
                    psave->next = type->params;
                    type->params = psave;
                    type->param_count++;
                    if (cur_tok(cs) == TOK_COMMA) next_tok(cs);
                    skip_newlines(cs);
                }
                expect(cs, TOK_RPAREN);
                /* optional return type after params */
                if (is_type_begin(cur_tok(cs)) || is_user_type_name(cs)) {
                    CType *ret = type_new(cs, TYPE_NONE);
                    parse_type(cs, ret);
                    if (ret->kind == TYPE_ARRAY && ret->ref) {
                        /* `void(params) T[N]`：返回类型解析把 [N] 吃掉了。
                         * NihaoC 惯例（如 table void(u8)[2]）中 [N] 修饰整个
                         * 函数指针（函数指针数组）→ 还原：vtype 包装成
                         * TYPE_ARRAY(ref=函数指针)，返回类型恢复为元素类型 */
                        CType *arr = type_new(cs, TYPE_ARRAY);
                        type->next = ret->ref;   /* 先：返回类型 = 元素类型 */
                        arr->ref = type_new(cs, type->kind);
                        memcpy(arr->ref, type, sizeof(CType));  /* 拷贝修正后的函数指针 */
                        arr->param_count = ret->param_count;
                        arr->size = ret->size;
                        *type = *arr;
                    } else {
                        type->next = ret;
                    }
                } else if (cur_tok(cs) == TOK_VOID) {
                    /* explicit "void" return: generic pointer */
                    CType *ret = type_new(cs, TYPE_VOID);
                    next_tok(cs);
                    type->next = ret;
                }
            }
            break;   /* 不 return：继续走数组/指针后缀循环（void[3]、void(i64) i64[2]） */
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

    /* Check for array modifiers.
     * NOTE: explicit `T*` named-pointer declaration was removed in 1.0.x
     * (see BNF <pointer-type>); pointers are now `void` / `void[n]` and
     * inferred from `&x`. Only the `[]` array suffix remains here. */
    while (cur_tok(cs) == TOK_LBRACKET) {
        if (cur_tok(cs) == TOK_LBRACKET) {
            next_tok(cs);
            /* 数组容量语法（BNF §3，2026-09-01 对齐 IR 前端）：
             *   [N]     —— 固定数组，容量 N
             *   [N...]  —— 动态数组，固定容量 N（1.0 不自动增长）  ← BNF 规范写法
             *   [N..]   —— 同上（.. 与 ... 等价）
             *   [...]   —— 动态数组未指定容量 → 默认 8 槽（与 IR 前端一致）
             *   [...N]  —— 历史兼容写法（A 方案早期实现），等同 [N...]
             * 注 1：1.0 语义下 [N] 与 [N...] 生成的 C 代码相同（都不增长），
             *       差异仅记录在源码层，供 2.0 ptr+len+cap 堆结构改造时区分。
             * 注 2：char[]（方括号内无点）由上层特判为 TYPE_STRING，不进入此分支；
             *       裸 []（如 void[]）也不进入本 if，array_size 保持 -1。 */
            int array_size = -1; /* 未指定 */
            if (cur_tok(cs) == TOK_INT_CONST) {
                array_size = (int)cs->parser.lex->tok_val.i;
                next_tok(cs);
                if (cur_tok(cs) == TOK_RANGE || cur_tok(cs) == TOK_ELLIPSIS) {
                    next_tok(cs);   /* [N..] / [N...]：容量即 N */
                }
            } else if (cur_tok(cs) == TOK_RANGE || cur_tok(cs) == TOK_ELLIPSIS) {
                next_tok(cs);
                if (cur_tok(cs) == TOK_INT_CONST) {
                    array_size = (int)cs->parser.lex->tok_val.i;  /* [...N]：历史兼容 */
                    next_tok(cs);
                } else {
                    array_size = 8;   /* [...]：[..] / [...] → 默认 8 槽 */
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
            } else if (cur_tok(cs) == TOK_IDENTIFIER) {
                /* link "lib" alias：显式别名 */
                char *alias = cs->parser.lex->tok_str;
                next_tok(cs);
                link_add_library(cs, lib_path, alias, lib_path);
            } else {
                link_add_library(cs, lib_path, lib_path, lib_path);
            }
        }
        skip_newlines(cs);
    }

    /* Parse top-level declarations（cooking 编译期块单独分派） */
    while (cur_tok(cs) != TOK_EOF) {
        if (cur_tok(cs) == TOK_COOKING) {
            parse_cooking_block(cs);
        } else if (cur_tok(cs) == TOK_ALIGN) {
            /* align n { ... }：对齐块——块体按普通声明处理（对齐留给 C 布局） */
            next_tok(cs);
            if (cur_tok(cs) == TOK_INT_CONST) next_tok(cs);
            if (cur_tok(cs) == TOK_LBRACE) next_tok(cs);
            skip_newlines(cs);
            while (cur_tok(cs) != TOK_RBRACE && cur_tok(cs) != TOK_EOF) {
                parse_declaration(cs);
                skip_newlines(cs);
            }
            if (cur_tok(cs) == TOK_RBRACE) next_tok(cs);
        } else {
            parse_declaration(cs);
            skip_newlines(cs);
        }
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

/* 初始化列表（嵌套递归）：已消费 {，元素逐项 parse_expression；遇 { 递归。
 * 定义在使用处（parse_declaration）前 */
static void parse_init_list(CompilerState *cs)
{
    cgen_raw("{");
    next_tok(cs);
    skip_newlines(cs);
    int k = 0;
    if (cur_tok(cs) != TOK_RBRACE) {
        for (;;) {
            if (k > 0) cgen_raw(", ");
            if (cur_tok(cs) == TOK_LBRACE) {
                parse_init_list(cs);
            } else {
                parse_expression(cs);
            }
            k++;
            if (cur_tok(cs) != TOK_COMMA) break;
            next_tok(cs);
            skip_newlines(cs);
        }
    }
    expect(cs, TOK_RBRACE);
    cgen_raw("}");
}

/* Heuristic C type for a type-inferred variable initializer.
 * Returns 1 if the RHS first token gave a usable hint. */
/* 推断初始化表达式的类型。
 * 契约：调用时 '=' 已被消费，cur_tok 指向 RHS 首 token
 * （parse_declaration 主路径与 for 循环 init 两处调用点保持一致）。 */
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
        case TOK_BITWISE_AND: {
            /* &x → 指向 x 的指针（peek 预判，不消费 &；
             * 与 IR 层 pt[] 模型语义对齐：& 表达式的类型 = 指向 x 的指针） */
            LexerState *lx = cs->parser.lex;
            lx->peek_valid = 0;
            lexer_peek(lx);
            if (lx->peek_tok == TOK_IDENTIFIER) {
                Symbol *s = sym_find(cs, lx->peek_str);
                if (s && s->type) {
                    out->kind = TYPE_POINTER;
                    out->size = 8;
                    CType *ref = type_new(cs, s->type->kind);
                    memcpy(ref, s->type, sizeof(CType));
                    ref->sym = s->type->sym;
                    out->ref = ref;
                    lx->peek_valid = 0;
                    return 1;
                }
            }
            lx->peek_valid = 0;
            out->kind = TYPE_POINTER;   /* 通用指针（无符号信息） */
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
        int init_seg[8] = {0};      /* 每个 init 表达式在 C 缓冲的段起点 */
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
                init_seg[nnames] = cgen_mark();
                parse_expression(cs); /* 值 emit 到 [seg, len) 段，生成时重排 */
            }
            nnames++;
            if (cur_tok(cs) == TOK_COMMA) next_tok(cs);
            skip_newlines(cs);
        }
        expect(cs, TOK_RBRACE);
        CType bt;
        parse_type(cs, &bt);
        /* 先收集每个 init 值段文本（段边界 = 相邻 init 起点 / 当前 len），
         * 再 truncate 掉原始位置，最后按 name 顺序重排输出 */
        char init_text[8][512];
        int had_any_init = 0;
        for (int i = 0; i < nnames; i++) {
            init_text[i][0] = '\0';
            if (has_init[i]) {
                had_any_init = 1;
                int nxt = (i + 1 < nnames && has_init[i + 1]) ? init_seg[i + 1]
                                                              : cgen_mark();
                int seg_len = nxt - init_seg[i];
                if (seg_len > 0 && seg_len < (int)sizeof(init_text[i])) {
                    memcpy(init_text[i], cgen_slice(init_seg[i]), (size_t)seg_len);
                    init_text[i][seg_len] = '\0';
                }
            }
        }
        if (had_any_init) cgen_truncate(init_seg[0]);  /* 清原始值段 */
        for (int i = 0; i < nnames; i++) {
            cgen_raw("%s%s %s", vis == VIS_CONST ? "const " : "",
                     c_type_name(&bt), names[i]);
            if (has_init[i] && init_text[i][0]) {
                char *p = init_text[i];
                while (*p == ' ' || *p == '\t') p++;   /* 段含缩进前导，trim */
                cgen_raw(" = %s", p);
            }
            cgen_line(";");
            /* 注册符号表（否则后续赋值被当推断声明 → redeclaration） */
            if (cs->parser.cur_func) {
                Symbol *vs = sym_push_local(cs, cs->parser.cur_func, names[i], &bt);
                vs->vis = vis;
            } else {
                Symbol *vs = sym_push(cs, SYM_VARIABLE, names[i], &bt);
                vs->vis = vis;
            }
        }
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
                /* optional param visibility prefix (§12.3 参数传递) */
                Visibility pv = VIS_DEFAULT;   /* 无前缀 = var */
                if (cur_tok(cs) == TOK_FLOW)        { pv = VIS_FLOW;   next_tok(cs); }
                else if (cur_tok(cs) == TOK_STATIC) { pv = VIS_STATIC; next_tok(cs); }
                else if (cur_tok(cs) == TOK_CONST)  { pv = VIS_CONST;  next_tok(cs); }
                else if (cur_tok(cs) == TOK_VAR)    { pv = VIS_DEFAULT; next_tok(cs); }
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
        param->vis = pv;   /* 记录参数可见性前缀（§12.3 M2 调用点检查用） */
        param->next = func_sym->params;
        func_sym->params = param;

                char one[256];
                if (ptype.kind == TYPE_FUNC) {
                    /* 函数指针参数：ret(*name)(params) */
                    char pstr[256];
                    c_type_params(&ptype, pstr, sizeof(pstr));
                    snprintf(one, sizeof(one), "%s%s(*%s)(%s)",
                             cparams[0] ? ", " : "",
                             c_type_name(ptype.next), pname,
                             pstr[0] ? pstr : "void");
                } else {
                    snprintf(one, sizeof(one), "%s%s %s%s",
                             cparams[0] ? ", " : "", c_type_name(&ptype), pname,
                             c_type_suffix(&ptype));
                }
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
            /* 返回类型挂到函数符号（堆拷贝）：return {..} 复合字面量生成用真实类型名。
             * 注意挂 func_sym->type->next——sym_push 已 memcpy 拷贝 ftype，ftype->next 无效 */
            CType *rtsave = nihao_malloc(cs, sizeof(CType));
            memcpy(rtsave, &ret_type, sizeof(CType));
            func_sym->type->next = rtsave;
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
    int inferred_eq = 0;   /* Name = expr：'=' 已被 infer 路径消费 */

    if (is_type_begin(cur_tok(cs)) || is_user_type_name(cs) || cur_tok(cs) == TOK_VOID) {
        parse_type(cs, &vtype);
    } else {
        /* type-inferred variable: Name = expr */
        if (cur_tok(cs) != TOK_ASSIGN) {
            nihao_error(cs, "expected type or '=' for variable '%s'", name);
            return;
        }
        next_tok(cs);               /* 消费 '='：infer 直接看 RHS 首 token */
        inferred_eq = 1;
        infer_init_type(cs, &vtype);
    }

    Symbol *var_sym;
    if (cs->parser.cur_func) {
        var_sym = sym_push_local(cs, cs->parser.cur_func, name, &vtype);
    } else {
        var_sym = sym_push(cs, SYM_VARIABLE, name, &vtype);
    }
    var_sym->vis = vis;

    /* emit declaration header */
    {
        int is_fptr = (vtype.kind == TYPE_FUNC ||
                       (vtype.kind == TYPE_ARRAY && vtype.ref &&
                        vtype.ref->kind == TYPE_FUNC));
        if (is_fptr) {
            /* 函数指针（数组）：C 为 ret(*name[suffix])(params)
             * 类型名包在名字两侧，数组后缀 [N] 插在 *name 后 */
            CType *ft = (vtype.kind == TYPE_FUNC) ? &vtype : vtype.ref;
            char pstr[512];
            c_type_params(ft, pstr, sizeof(pstr));
            cgen_raw("%s%s%s(*%s%s)(%s)", is_const ? "const " : "",
                     is_static ? "static " : "",
                     c_type_name(ft->next), name, c_type_suffix(&vtype),
                     pstr[0] ? pstr : "void");
        } else {
            cgen_raw("%s%s%s %s%s", is_const ? "const " : "",
                     is_static ? "static " : "",
                     c_type_name(&vtype), name, c_type_suffix(&vtype));
        }
    }

    /* initializer（inferred_eq：'=' 已消费，cur_tok 已是 RHS；否则 '=' 待消费） */
    if (inferred_eq || cur_tok(cs) == TOK_ASSIGN) {
        if (!inferred_eq) {
            next_tok(cs);
        }
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
        /* 数组/聚合初始化列表：= {e0, e1, ...}（原样输出给 C；嵌套 { } 递归） */
        if (cur_tok(cs) == TOK_LBRACE) {
            parse_init_list(cs);
        } else {
            parse_expression(cs);
        }
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
    TokenType t = cur_tok(cs);
    if (t == TOK_IDENTIFIER && strcmp(cs->parser.lex->tok_str, "_") == 0) {
        /* 通配符：匹配任意 __is_val（恒真）——文档 pattern 列表含 _ */
        next_tok(cs);
        cgen_raw("if (1)");
        if (cur_tok(cs) == TOK_LBRACE) {
            parse_statement(cs);
        } else {
            nihao_error(cs, "expected block after 'is _'");
        }
        return;
    }
    cgen_raw("if (__is_val");
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
    } else {
        nihao_error(cs, "expected block after 'is' pattern");
    }
}

void parse_statement(CompilerState *cs)
{
    TokenType tok = cur_tok(cs);

    /* 标签：name:（C 风格，lexer 注释 label suffix；peek 下一 token） */
    if (tok == TOK_IDENTIFIER) {
        LexerState *lx = cs->parser.lex;
        lx->peek_valid = 0;
        lexer_peek(lx);
        if (lx->peek_tok == TOK_COLON) {
            cgen_line("%s:;", lx->tok_str);
            next_tok(cs);
            next_tok(cs);           /* name : */
            return;
        }
        lx->peek_valid = 0;
    }

    switch (tok) {
        case TOK_GOTO:
            next_tok(cs);
            if (cur_tok(cs) == TOK_IDENTIFIER) {
                cgen_line("goto %s;", cs->parser.lex->tok_str);
                next_tok(cs);
            } else {
                nihao_error(cs, "expected label name after 'goto'");
            }
            break;

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
            /* 条件检查必须在 body 前（修复 do-while 语义 bug） */
            cgen_line("if (!__is_val) break;");
            parse_statement(cs);           /* body */
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
                    /* return {v1, v2, ...} compound literal——复合字面量类型
                     * 用当前函数返回类型名（原硬编码 (type__compound) 未定义） */
                    next_tok(cs);
                    const char *rt = "void";
                    Symbol *f = cs->parser.cur_func;
                    if (f && f->type && f->type->next) {
                        const char *tn = c_type_name(f->type->next);
                        if (tn && tn[0]) rt = tn;
                    }
                    cgen_raw("(%s)", rt);
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

        case TOK_SWITCH: {
            /* C 风格 switch：switch (expr) { case e: ... [default: ...] }
             * 生成 C 原生 switch，每个 case 后自动 break（NihaoC 语义：无 fallthrough）。
             * case 表达式须为编译期常量（int 字面量 / enum 常量）。 */
            next_tok(cs);
            expect(cs, TOK_LPAREN);
            cgen_raw("switch (");
            parse_expression(cs);
            cgen_raw(")");
            expect(cs, TOK_RPAREN);
            expect(cs, TOK_LBRACE);
            cgen_line("{");
            cgen_indent();
            skip_newlines(cs);
            while (cur_tok(cs) != TOK_RBRACE && cur_tok(cs) != TOK_EOF) {
                if (cur_tok(cs) == TOK_CASE) {
                    next_tok(cs);
                    cgen_raw("case ");
                    parse_expression(cs);
                    expect(cs, TOK_COLON);
                    cgen_line(":");
                    cgen_indent();
                    skip_newlines(cs);
                    while (cur_tok(cs) != TOK_CASE && cur_tok(cs) != TOK_DEFAULT &&
                           cur_tok(cs) != TOK_RBRACE) {
                        parse_statement(cs);
                        skip_newlines(cs);
                    }
                    cgen_line("break;");   /* NihaoC：case 自动跳出 */
                    cgen_dedent();
                } else if (cur_tok(cs) == TOK_DEFAULT) {
                    next_tok(cs);
                    expect(cs, TOK_COLON);
                    cgen_line("default:");
                    cgen_indent();
                    skip_newlines(cs);
                    while (cur_tok(cs) != TOK_RBRACE && cur_tok(cs) != TOK_EOF) {
                        parse_statement(cs);
                        skip_newlines(cs);
                    }
                    cgen_dedent();
                } else {
                    nihao_error(cs, "expected 'case' or 'default' in switch");
                    next_tok(cs);
                }
                skip_newlines(cs);
            }
            expect(cs, TOK_RBRACE);
            cgen_line("}");
            cgen_dedent();
            break;
        }

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
            /* 编译期块：static_assert 求值 + 编译期常量（parse_cooking_block） */
            parse_cooking_block(cs);
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

/* ============================================================
 * cooking 编译期（A 方案，与 IR 子集 PB-9 对齐）：
 *   cooking { static_assert(expr, "msg") / const NAME [TYPE] = expr / ... }
 * 常量折叠链 pc_*：int 字面量/一元/四则/移位/比较/相等/位运算/逻辑/括号/
 * enum 常量/可见性枚举/sizeof(类型)；编译期变量表 ct_vars（跨块共享）
 * ============================================================ */
static struct { const char *name; long long val; } ct_vars[64];
static int ct_vars_count;

static int ct_var_exist(const char *name)
{
    for (int i = 0; i < ct_vars_count; i++)
        if (strcmp(ct_vars[i].name, name) == 0) return 1;
    return 0;
}
static long long ct_var_find(const char *name)
{
    for (int i = 0; i < ct_vars_count; i++)
        if (strcmp(ct_vars[i].name, name) == 0) return ct_vars[i].val;
    return 0;
}

static long long pc_or(CompilerState *cs);
static long long pc_prim(CompilerState *cs)
{
    TokenType t = cur_tok(cs);
    if (t == TOK_INT_CONST) { long long v = cs->parser.lex->tok_val.i; next_tok(cs); return v; }
    if (t == TOK_TRUE) { next_tok(cs); return 1; }
    if (t == TOK_FALSE) { next_tok(cs); return 0; }
    if (t == TOK_LPAREN) {
        next_tok(cs);
        long long v = pc_or(cs);
        if (cur_tok(cs) == TOK_RPAREN) next_tok(cs);
        return v;
    }
    if (t == TOK__UNDEF || t == TOK__CONST || t == TOK__FLOW ||
        t == TOK__STATIC || t == TOK__VAR) {
        next_tok(cs);
        return (t == TOK__UNDEF) ? 0 : (t == TOK__CONST) ? 1 :
               (t == TOK__FLOW) ? 2 : (t == TOK__STATIC) ? 3 : 4;
    }
    if (t == TOK_SIZEOF) {
        next_tok(cs);
        if (cur_tok(cs) == TOK_LPAREN) next_tok(cs);
        if (is_type_begin(cur_tok(cs)) || is_user_type_name(cs)) {
            CType tmp;
            parse_type(cs, &tmp);
            if (cur_tok(cs) == TOK_RPAREN) next_tok(cs);
            return tmp.size ? (long long)tmp.size : 0;
        }
        while (cur_tok(cs) != TOK_RPAREN && cur_tok(cs) != TOK_NEWLINE &&
               cur_tok(cs) != TOK_EOF) next_tok(cs);
        if (cur_tok(cs) == TOK_RPAREN) next_tok(cs);
        return 8;   /* sizeof(expr)：8 字节槽 */
    }
    if (t == TOK_IDENTIFIER) {
        const char *name = cs->parser.lex->tok_str;
        if (ct_var_exist(name)) { next_tok(cs); return ct_var_find(name); }
        Symbol *s = sym_find(cs, name);
        if (s && s->kind == SYM_ENUM) { next_tok(cs); return (long long)s->addr; }
        nihao_error(cs, "constant expression: unknown identifier '%s'", name);
        next_tok(cs);
        return 0;
    }
    nihao_error(cs, "constant expression: unexpected token '%s'", token_name(t));
    next_tok(cs);
    return 0;
}
static long long pc_unary(CompilerState *cs)
{
    TokenType t = cur_tok(cs);
    if (t == TOK_MINUS) { next_tok(cs); return -pc_unary(cs); }
    if (t == TOK_LOGICAL_NOT) { next_tok(cs); return !pc_unary(cs); }
    if (t == TOK_BITWISE_NOT) { next_tok(cs); return ~pc_unary(cs); }
    return pc_prim(cs);
}
static long long pc_mul(CompilerState *cs)
{
    long long a = pc_unary(cs);
    for (;;) {
        TokenType t = cur_tok(cs);
        if (t == TOK_STAR) { next_tok(cs); a *= pc_unary(cs); }
        else if (t == TOK_SLASH) { next_tok(cs); long long d = pc_unary(cs); a = d ? a / d : 0; }
        else if (t == TOK_PERCENT) { next_tok(cs); long long d = pc_unary(cs); a = d ? a % d : 0; }
        else return a;
    }
}
static long long pc_add(CompilerState *cs)
{
    long long a = pc_mul(cs);
    for (;;) {
        TokenType t = cur_tok(cs);
        if (t == TOK_PLUS) { next_tok(cs); a += pc_mul(cs); }
        else if (t == TOK_MINUS) { next_tok(cs); a -= pc_mul(cs); }
        else return a;
    }
}
static long long pc_shift(CompilerState *cs)
{
    long long a = pc_add(cs);
    for (;;) {
        TokenType t = cur_tok(cs);
        if (t == TOK_LEFT_SHIFT) { next_tok(cs); a <<= pc_add(cs); }
        else if (t == TOK_RIGHT_SHIFT) { next_tok(cs); a >>= pc_add(cs); }
        else return a;
    }
}
static long long pc_rel(CompilerState *cs)
{
    long long a = pc_shift(cs);
    for (;;) {
        TokenType t = cur_tok(cs);
        if (t == TOK_LT) { next_tok(cs); a = a < pc_shift(cs); }
        else if (t == TOK_GT) { next_tok(cs); a = a > pc_shift(cs); }
        else if (t == TOK_LE) { next_tok(cs); a = a <= pc_shift(cs); }
        else if (t == TOK_GE) { next_tok(cs); a = a >= pc_shift(cs); }
        else return a;
    }
}
static long long pc_eq(CompilerState *cs)
{
    long long a = pc_rel(cs);
    for (;;) {
        TokenType t = cur_tok(cs);
        if (t == TOK_EQ) { next_tok(cs); a = a == pc_rel(cs); }
        else if (t == TOK_NE) { next_tok(cs); a = a != pc_rel(cs); }
        else return a;
    }
}
static long long pc_bitand(CompilerState *cs)
{
    long long a = pc_eq(cs);
    while (cur_tok(cs) == TOK_BITWISE_AND) { next_tok(cs); a &= pc_eq(cs); }
    return a;
}
static long long pc_bitxor(CompilerState *cs)
{
    long long a = pc_bitand(cs);
    while (cur_tok(cs) == TOK_BITWISE_XOR) { next_tok(cs); a ^= pc_bitand(cs); }
    return a;
}
static long long pc_bitor(CompilerState *cs)
{
    long long a = pc_bitxor(cs);
    while (cur_tok(cs) == TOK_BITWISE_OR) { next_tok(cs); a |= pc_bitxor(cs); }
    return a;
}
static long long pc_and(CompilerState *cs)
{
    long long a = pc_bitor(cs);
    while (cur_tok(cs) == TOK_LOGICAL_AND) { next_tok(cs); long long b = pc_bitor(cs); a = a && b; }
    return a;
}
static long long pc_or(CompilerState *cs)
{
    long long a = pc_and(cs);
    while (cur_tok(cs) == TOK_LOGICAL_OR) { next_tok(cs); long long b = pc_and(cs); a = a || b; }
    return a;
}

/* static_assert(expr, "msg")：编译期断言 */
static void parse_static_assert(CompilerState *cs)
{
    next_tok(cs);               /* static_assert */
    if (cur_tok(cs) == TOK_LPAREN) next_tok(cs);
    long long v = pc_or(cs);
    const char *msg = "";
    if (cur_tok(cs) == TOK_COMMA) {
        next_tok(cs);
        if (cur_tok(cs) == TOK_STRING_LITERAL) {
            msg = cs->parser.lex->tok_str;
            next_tok(cs);
        }
    }
    if (cur_tok(cs) == TOK_RPAREN) next_tok(cs);
    if (!v) {
        nihao_error(cs, "static_assert failed: %s", msg);
    }
}

/* cooking { ... }：编译期块——static_assert 求值 + 编译期常量声明 */
static void parse_cooking_block(CompilerState *cs)
{
    next_tok(cs);               /* cooking */
    if (cur_tok(cs) != TOK_LBRACE) {
        nihao_error(cs, "cooking must be followed by { }");
        return;
    }
    next_tok(cs);
    skip_newlines(cs);
    while (cur_tok(cs) != TOK_RBRACE && cur_tok(cs) != TOK_EOF) {
        if (cur_tok(cs) == TOK_IDENTIFIER &&
            strcmp(cs->parser.lex->tok_str, "static_assert") == 0) {
            parse_static_assert(cs);
        } else if (cur_tok(cs) == TOK_CONST) {
            /* 编译期常量：const NAME [TYPE] = expr → 存 ct_vars */
            next_tok(cs);
            if (cur_tok(cs) == TOK_IDENTIFIER) {
                const char *cname = cs->parser.lex->tok_str;
                next_tok(cs);
                if (is_type_token(cur_tok(cs))) next_tok(cs);
                if (cur_tok(cs) == TOK_ASSIGN) {
                    next_tok(cs);
                    long long v = pc_or(cs);
                    if (ct_var_exist(cname)) {
                        nihao_error(cs, "cooking const '%s' redefined", cname);
                    } else if (ct_vars_count < 64) {
                        ct_vars[ct_vars_count].name = cname;
                        ct_vars[ct_vars_count].val = v;
                        ct_vars_count++;
                    }
                } else {
                    nihao_error(cs, "cooking const: expected '='");
                }
            }
        } else {
            next_tok(cs);       /* 其他编译期 item 跳过 */
        }
        skip_newlines(cs);
    }
    if (cur_tok(cs) == TOK_RBRACE) next_tok(cs);
    skip_newlines(cs);
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

            /* cooking 编译期变量：运行时引用折叠为常量（PB-9，与 IR 子集对齐） */
            if (!sym && ct_var_exist(name)) {
                cgen_raw("%lld", ct_var_find(name));
                break;
            }

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
                /* M2 参数前缀检查：收集被调函数参数（声明序），对指针类实参按参数 vis 检查 */
                Symbol *callee = sym_find(cs, name);
                Symbol *oparams[32]; int nop = 0;
                if (callee && callee->kind == SYM_FUNCTION && callee->params) {
                    for (Symbol *pp = callee->params; pp && nop < 32; pp = pp->next)
                        oparams[nop++] = pp;
                    for (int _i = 0; _i < nop / 2; _i++) {  /* 反转（头插 → 倒序） */
                        Symbol *_t = oparams[_i];
                        oparams[_i] = oparams[nop - 1 - _i];
                        oparams[nop - 1 - _i] = _t;
                    }
                }
                Symbol *frozen_srcs[32]; int nf = 0;
                if (cur_tok(cs) != TOK_RPAREN) {
                    int arg_idx = 0;
                    for (;;) {
                        /* 先解析实参（vis_check_usable 此时源仍 VALID），再按参数 vis 检查。
                         * 顺序关键：flow→flow 转移(失效)须在实参被"使用"之后记录，
                         * 否则 parse_expression 内的 vis_check_usable 会把本次移动误判为 use-after-move。 */
                        Symbol *as = NULL;
                        if (cur_tok(cs) == TOK_IDENTIFIER && nop > 0 && arg_idx < nop) {
                            Symbol *s = sym_find(cs, cs->parser.lex->tok_str);
                            Symbol *pp = oparams[arg_idx];
                            if (s && s->kind == SYM_VARIABLE &&
                                vis_is_pointer_type(s->type) && pp &&
                                vis_is_pointer_type(pp->type))
                                as = s;
                        }
                        parse_expression(cs);
                        if (as) {
                            int borrowed = vis_check_call_arg(cs, oparams[arg_idx]->vis,
                                                             as, oparams[arg_idx]->name);
                            if (borrowed && nf < 32) frozen_srcs[nf++] = as;
                        }
                        arg_idx++;
                        if (cur_tok(cs) != TOK_COMMA) break;
                        next_tok(cs);
                        cgen_raw(", ");
                    }
                }
                cgen_raw(")");
                expect(cs, TOK_RPAREN);
                for (int _k = 0; _k < nf; _k++) vis_unfreeze(frozen_srcs[_k]);  /* 调用结束解冻借用 */
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
static void parse_deref_chain(CompilerState *cs, int line)
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
        /* .() : dereference pointer one level
         * 类型取自符号实际指针 ref（隐式推断/显式声明均记录在 sym->type）；
         * 无类型信息或指向 void（NihaoC 通用指针）时回退 void** */
        next_tok(cs);                   /* consume ) */
        CType *pt = (sym && sym->kind == SYM_VARIABLE) ? sym->type : NULL;
        if (pt && pt->kind == TYPE_POINTER && pt->ref &&
            pt->ref->kind != TYPE_VOID) {
            cgen_raw("(*(%s*)%s)", c_type_name(pt->ref), name);
        } else {
            cgen_raw("(*(void**)%s)", name);
        }
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
            if (cs->parser.lex->line_num != line) break;   /* 跨行不算后缀 ++ */
            cgen_raw("++");
            next_tok(cs);
        } else if (tok == TOK_DECREMENT) {
            if (cs->parser.lex->line_num != line) break;   /* 跨行不算后缀 -- */
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
            parse_deref_chain(cs, cs->parser.lex->line_num);
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
            if (cs->parser.lex->line_num != line) break;   /* 跨行不算后缀 ++ */
            cgen_raw("++");
            next_tok(cs);
        } else if (tok == TOK_DECREMENT) {
            if (cs->parser.lex->line_num != line) break;   /* 跨行不算后缀 -- */
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
