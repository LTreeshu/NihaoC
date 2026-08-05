/* ============================================================
 * irparse.c - IR 前端（方案 B 骨架）
 *
 * 解析 NihaoC 最小子集并直接发射三地址码 IR（单趟，无 AST）：
 *   module / use / func name(params) ret { ... }
 *   局部变量声明 (name i32 = expr) 与赋值
 *   if / else / while / return
 *   表达式: 整数常量、标识符、字符串字面量、+ - * /、比较、括号
 *   调用: puts("...")（外部 C 符号）
 *
 * 目的：验证 IR -> C 与 IR -> x86-64 双后端端到端可行；
 *       全量语法迁移到 IR 是本骨架之后的迭代工作。
 * ============================================================ */
#include "ir.h"

static IrProg *P;
static IrFn *F;
static int *vt;             /* 局部变量: name 序号 -> vreg（ALLOCA 槽） */
static const char **vn;
static int vn_count, vn_cap;

/* ---- 局部变量表 ---- */
static void var_reset(void)
{
    vn_count = 0;
    if (vn_cap == 0) {
        vn_cap = 32;
        vt = nihao_malloc(g_cs, vn_cap * sizeof(int));
        vn = nihao_malloc(g_cs, vn_cap * sizeof(char *));
    }
}

static int var_find(const char *name)
{
    for (int i = 0; i < vn_count; i++)
        if (strcmp(vn[i], name) == 0) return i;
    return -1;
}

static int var_declare(const char *name)
{
    int i = var_find(name);
    if (i >= 0) return i;
    if (vn_count >= vn_cap) {
        vn_cap *= 2;
        vt = nihao_realloc(g_cs, vt, vn_cap * sizeof(int));
        vn = nihao_realloc(g_cs, vn, vn_cap * sizeof(char *));
    }
    vn[vn_count] = name;
    /* ALLOCA 一个 8 字节槽 */
    int vr = ir_new_vreg(F);
    ir_emit(F, IR_ALLOCA, vr, -1, -1, 8);
    vt[vn_count] = vr;
    return vn_count++;
}

/* ---- 表达式：返回持有结果的 vreg ---- */
static int ir_expr(CompilerState *cs);

static int ir_primary(CompilerState *cs)
{
    TokenType t = cur_tok(cs);
    if (t == TOK_MINUS) {
        /* 一元负号 -x -> IR_NEG */
        next_tok(cs);
        int a = ir_primary(cs);
        int vr = ir_new_vreg(F);
        ir_emit(F, IR_NEG, vr, a, -1, 0);
        return vr;
    }
    if (t == TOK_LOGICAL_NOT) {
        /* 逻辑非 !x -> (x == 0) */
        next_tok(cs);
        int a = ir_primary(cs);
        int z = ir_new_vreg(F);
        ir_emit(F, IR_CONST, z, -1, -1, 0);
        int vr = ir_new_vreg(F);
        ir_emit(F, IR_CMP_EQ, vr, a, z, 0);
        return vr;
    }
    if (t == TOK_BITWISE_NOT) {
        /* 位非 ~x -> IR_NOT */
        next_tok(cs);
        int a = ir_primary(cs);
        int vr = ir_new_vreg(F);
        ir_emit(F, IR_NOT, vr, a, -1, 0);
        return vr;
    }
    if (t == TOK_STAR) {
        /* 一元解引用 *p -> LOAD */
        next_tok(cs);
        int a = ir_primary(cs);
        int vr = ir_new_vreg(F);
        ir_emit(F, IR_LOAD, vr, a, -1, 0);
        return vr;
    }
    if (t == TOK_BITWISE_AND) {
        /* 取地址 &x -> ADDR（当前仅支持局部变量） */
        next_tok(cs);
        if (cur_tok(cs) == TOK_IDENTIFIER) {
            const char *name = cs->parser.lex->tok_str;
            int vi = var_find(name);
            if (vi < 0) {
                nihao_error(cs, "ir: cannot take address of undeclared '%s'", name);
                return ir_new_vreg(F);
            }
            next_tok(cs);
            int vr = ir_new_vreg(F);
            ir_emit(F, IR_ADDR, vr, vt[vi], -1, 0);
            return vr;
        }
        nihao_error(cs, "ir: '&' requires a variable identifier");
        next_tok(cs);
        return ir_new_vreg(F);
    }
    if (t == TOK_INT_CONST) {
        int vr = ir_new_vreg(F);
        ir_emit(F, IR_CONST, vr, -1, -1, cs->parser.lex->tok_val.i);
        next_tok(cs);
        return vr;
    }
    if (t == TOK_STRING_LITERAL) {
        int si = ir_add_string(P, cs->parser.lex->tok_str);
        int vr = ir_new_vreg(F);
        ir_emit(F, IR_LD_ADDR, vr, -1, -1, 0);
        F->ins[F->ins_count - 1].sym = P->str_syms[si];
        next_tok(cs);
        return vr;
    }
    if (t == TOK_IDENTIFIER) {
        const char *name = cs->parser.lex->tok_str;
        next_tok(cs);
        if (cur_tok(cs) == TOK_LPAREN) {
            /* 函数调用 */
            next_tok(cs);
            int nargs = 0;
            if (cur_tok(cs) != TOK_RPAREN) {
                for (;;) {
                    int a = ir_expr(cs);
                    ir_emit(F, IR_PARAM, -1, a, -1, 0);
                    nargs++;
                    if (cur_tok(cs) != TOK_COMMA) break;
                    next_tok(cs);
                }
            }
            expect(cs, TOK_RPAREN);
            int vr = ir_new_vreg(F);
            IrIns *in = &F->ins[ir_emit(F, IR_CALL, vr, -1, -1, nargs)];
            in->sym = name;         /* 外部符号（puts 等） */
            in->fn = -1;
            return vr;
        }
        int vi = var_find(name);
        if (vi < 0) {
            nihao_error(cs, "ir: undeclared variable '%s'", name);
            return ir_new_vreg(F);
        }
        int vr = ir_new_vreg(F);
        ir_emit(F, IR_MOV, vr, vt[vi], -1, 0);
        return vr;
    }
    if (t == TOK_LPAREN) {
        next_tok(cs);
        int vr = ir_expr(cs);
        expect(cs, TOK_RPAREN);
        return vr;
    }
    nihao_error(cs, "ir: unexpected token '%s' in expression", token_name(t));
    next_tok(cs);
    return ir_new_vreg(F);
}

static int ir_binop(CompilerState *cs, IrOp op)
{
    int a = ir_primary(cs);
    (void)op;
    return a;
}

/* 简单优先级：add > mul > cmp（子集够用）。
 * 函数体内换行不产生 NEWLINE token（lexer 仅顶层发 NEWLINE），
 * 语句边界用"运算符与表达式首 token 是否同行"判定：换行即停止表达式。 */
static int ir_mul(CompilerState *cs, int line)
{
    int a = ir_primary(cs);
    for (;;) {
        TokenType t = cur_tok(cs);
        IrOp op;
        if (cs->parser.lex->last_line_num != line) break;
        if (t == TOK_STAR) op = IR_MUL;
        else if (t == TOK_SLASH) op = IR_DIV;
        else if (t == TOK_PERCENT) op = IR_MOD;
        else break;
        next_tok(cs);
        int b = ir_primary(cs);
        int vr = ir_new_vreg(F);
        ir_emit(F, op, vr, a, b, 0);
        a = vr;
    }
    return a;
}

static int ir_add(CompilerState *cs, int line)
{
    int a = ir_mul(cs, line);
    for (;;) {
        TokenType t = cur_tok(cs);
        IrOp op;
        if (cs->parser.lex->last_line_num != line) break;
        if (t == TOK_PLUS) op = IR_ADD;
        else if (t == TOK_MINUS) op = IR_SUB;
        else break;
        next_tok(cs);
        int b = ir_mul(cs, line);
        int vr = ir_new_vreg(F);
        ir_emit(F, op, vr, a, b, 0);
        a = vr;
    }
    return a;
}

static int ir_cmp(CompilerState *cs, int line)
{
    int a = ir_add(cs, line);
    for (;;) {
        TokenType t = cur_tok(cs);
        IrOp op;
        if (cs->parser.lex->last_line_num != line) return a;
        switch (t) {
            case TOK_LT:  op = IR_CMP_LT; break;
            case TOK_LE:  op = IR_CMP_LE; break;
            case TOK_GT:  op = IR_CMP_GT; break;
            case TOK_GE:  op = IR_CMP_GE; break;
            case TOK_EQ:  op = IR_CMP_EQ; break;
            case TOK_NE:  op = IR_CMP_NE; break;
            default: return a;
        }
        next_tok(cs);
        int b = ir_add(cs, line);
        int vr = ir_new_vreg(F);
        ir_emit(F, op, vr, a, b, 0);
        a = vr;
    }
}

static int ir_expr(CompilerState *cs)
{
    /* 表达式首 token 的行号 = 语句起始行，用于块内换行边界判定 */
    return ir_cmp(cs, cs->parser.lex->last_line_num);
}

/* ---- 语句 ---- */
static void ir_stmt(CompilerState *cs);

static void ir_block(CompilerState *cs)
{
    expect(cs, TOK_LBRACE);
    skip_newlines(cs);
    while (cur_tok(cs) != TOK_RBRACE && cur_tok(cs) != TOK_EOF) {
        ir_stmt(cs);
        skip_newlines(cs);
    }
    expect(cs, TOK_RBRACE);
    skip_newlines(cs);
}

static void ir_stmt(CompilerState *cs)
{
    TokenType t = cur_tok(cs);
    if (t == TOK_IF) {
        next_tok(cs);
        int c = ir_expr(cs);
        int l_else = ir_new_label(F);
        int l_end = ir_new_label(F);
        ir_emit(F, IR_JZ, -1, c, -1, 0);
        F->ins[F->ins_count - 1].label = l_else;
        ir_block(cs);
        ir_emit(F, IR_JMP, -1, -1, -1, 0);
        F->ins[F->ins_count - 1].label = l_end;
        ir_emit(F, IR_LABEL, -1, -1, -1, 0);
        F->ins[F->ins_count - 1].label = l_else;
        if (cur_tok(cs) == TOK_ELSE) {
            next_tok(cs);
            ir_block(cs);
        }
        ir_emit(F, IR_LABEL, -1, -1, -1, 0);
        F->ins[F->ins_count - 1].label = l_end;
    } else if (t == TOK_WHILE) {
        next_tok(cs);
        int l_loop = ir_new_label(F);
        int l_end = ir_new_label(F);
        ir_emit(F, IR_LABEL, -1, -1, -1, 0);
        F->ins[F->ins_count - 1].label = l_loop;
        int c = ir_expr(cs);
        ir_emit(F, IR_JZ, -1, c, -1, 0);
        F->ins[F->ins_count - 1].label = l_end;
        ir_block(cs);
        ir_emit(F, IR_JMP, -1, -1, -1, 0);
        F->ins[F->ins_count - 1].label = l_loop;
        ir_emit(F, IR_LABEL, -1, -1, -1, 0);
        F->ins[F->ins_count - 1].label = l_end;
    } else if (t == TOK_RETURN) {
        next_tok(cs);
        if (cur_tok(cs) == TOK_NEWLINE || cur_tok(cs) == TOK_RBRACE) {
            ir_emit(F, IR_RET, -1, -1, -1, 0);
        } else {
            int vr = ir_expr(cs);
            ir_emit(F, IR_RET, -1, vr, -1, 0);
        }
        skip_newlines(cs);
    } else if (t == TOK_IDENTIFIER) {
        const char *name = cs->parser.lex->tok_str;
        /* peek 下一个 token 分派：赋值 / 声明(name i32 = expr) / 表达式 */
        LexerState *lex = cs->parser.lex;
        lex->peek_valid = 0;
        lexer_peek(lex);
        TokenType nt = lex->peek_tok;
        lex->peek_valid = 0;
        if (nt == TOK_ASSIGN) {
            next_tok(cs);           /* name */
            next_tok(cs);           /* = */
            int vi = var_find(name);
            if (vi < 0) vi = var_declare(name);
            int vr = ir_expr(cs);
            ir_emit(F, IR_MOV, vt[vi], vr, -1, 0);
            skip_newlines(cs);
        } else if (nt == TOK_PLUS_ASSIGN || nt == TOK_MINUS_ASSIGN ||
                   nt == TOK_STAR_ASSIGN || nt == TOK_SLASH_ASSIGN ||
                   nt == TOK_PERCENT_ASSIGN) {
            /* 复合赋值 x op= expr */
            IrOp op;
            switch (nt) {
                case TOK_PLUS_ASSIGN:    op = IR_ADD; break;
                case TOK_MINUS_ASSIGN:   op = IR_SUB; break;
                case TOK_STAR_ASSIGN:    op = IR_MUL; break;
                case TOK_SLASH_ASSIGN:   op = IR_DIV; break;
                default:                 op = IR_MOD; break;
            }
            next_tok(cs);           /* name */
            next_tok(cs);           /* op= */
            int vi = var_find(name);
            if (vi < 0) {
                nihao_error(cs, "ir: undeclared variable '%s'", name);
                skip_newlines(cs);
                return;
            }
            int b = ir_expr(cs);
            int vr = ir_new_vreg(F);
            ir_emit(F, op, vr, vt[vi], b, 0);
            ir_emit(F, IR_MOV, vt[vi], vr, -1, 0);
            skip_newlines(cs);
        } else if (nt == TOK_INCREMENT || nt == TOK_DECREMENT) {
            /* 后缀自增/自减 x++ / x-- */
            IrOp op = (nt == TOK_INCREMENT) ? IR_ADD : IR_SUB;
            next_tok(cs);           /* name */
            next_tok(cs);           /* ++ / -- */
            int vi = var_find(name);
            if (vi < 0) {
                nihao_error(cs, "ir: undeclared variable '%s'", name);
                skip_newlines(cs);
                return;
            }
            int one = ir_new_vreg(F);
            ir_emit(F, IR_CONST, one, -1, -1, 1);
            int vr = ir_new_vreg(F);
            ir_emit(F, op, vr, vt[vi], one, 0);
            ir_emit(F, IR_MOV, vt[vi], vr, -1, 0);
            skip_newlines(cs);
        } else if (is_type_token(nt)) {
            /* 声明: name i32 = expr */
            next_tok(cs);           /* name */
            next_tok(cs);           /* 类型 */
            expect(cs, TOK_ASSIGN); /* 消费 =（expect 已推进） */
            int vi = var_declare(name);
            int vr = ir_expr(cs);
            ir_emit(F, IR_MOV, vt[vi], vr, -1, 0);
            skip_newlines(cs);
        } else {
            /* 表达式语句（如 puts(...) 调用） */
            ir_expr(cs);
            skip_newlines(cs);
        }
    } else if (t == TOK_INCREMENT || t == TOK_DECREMENT) {
        /* 前缀 ++x / --x */
        IrOp op = (t == TOK_INCREMENT) ? IR_ADD : IR_SUB;
        next_tok(cs);
        if (cur_tok(cs) != TOK_IDENTIFIER) {
            nihao_error(cs, "ir: '++'/'--' requires a variable");
            skip_newlines(cs);
            return;
        }
        const char *name = cs->parser.lex->tok_str;
        int vi = var_find(name);
        if (vi < 0) {
            nihao_error(cs, "ir: undeclared variable '%s'", name);
            skip_newlines(cs);
            return;
        }
        next_tok(cs);
        int one = ir_new_vreg(F);
        ir_emit(F, IR_CONST, one, -1, -1, 1);
        int vr = ir_new_vreg(F);
        ir_emit(F, op, vr, vt[vi], one, 0);
        ir_emit(F, IR_MOV, vt[vi], vr, -1, 0);
        skip_newlines(cs);
    } else if (t == TOK_STAR) {
        /* *p = expr（STORE）或 *p 表达式（LOAD） */
        next_tok(cs);
        int addr = ir_primary(cs);
        if (cur_tok(cs) == TOK_ASSIGN) {
            next_tok(cs);
            int vr = ir_expr(cs);
            ir_emit(F, IR_STORE, -1, addr, vr, 0);
        } else {
            int vr = ir_new_vreg(F);
            ir_emit(F, IR_LOAD, vr, addr, -1, 0);
        }
        skip_newlines(cs);
    } else if (t == TOK_LBRACE) {
        ir_block(cs);
    } else {
        /* 表达式语句（如 puts 调用） */
        ir_expr(cs);
        skip_newlines(cs);
    }
}

/* ---- 函数 ---- */
static void ir_func(CompilerState *cs)
{
    /* func name ( params ) ret */
    const char *fname = cs->parser.lex->tok_str;
    int is_main = strcmp(fname, "main") == 0;
    next_tok(cs);
    F = ir_fn_new(P, fname, is_main);
    var_reset();

    expect(cs, TOK_LPAREN);
    skip_newlines(cs);
    int nparam = 0;
    if (cur_tok(cs) != TOK_RPAREN) {
        for (;;) {
            /* param: name i32 */
            if (cur_tok(cs) != TOK_IDENTIFIER) break;
            const char *pname = cs->parser.lex->tok_str;
            next_tok(cs);
            next_tok(cs);           /* 跳过类型 */
            var_declare(pname);
            nparam++;
            if (cur_tok(cs) != TOK_COMMA) break;
            next_tok(cs);
        }
    }
    expect(cs, TOK_RPAREN);
    skip_newlines(cs);
    if (cur_tok(cs) != TOK_LBRACE) next_tok(cs);   /* 跳过返回类型 */
    F->param_count = nparam;
    ir_block(cs);
    ir_fn_end(F);
}

int ir_parse_file(CompilerState *cs, const char *filename)
{
    size_t src_size = 0;
    char *source = load_source_file(filename, &src_size);
    if (!source) return -1;

    LexerState *lex = nihao_malloc(cs, sizeof(LexerState));
    cs->parser.lex = lex;
    lexer_init(cs, filename, source);

    P = ir_prog_new();
    next_tok(cs);       /* 懒加载 lexer：读取首个 token */
    skip_newlines(cs);

    /* module / use 行 */
    if (cur_tok(cs) == TOK_MODULE) {
        next_tok(cs);
        next_tok(cs);   /* 模块名 */
        skip_newlines(cs);
    }
    while (cur_tok(cs) == TOK_USE) {
        next_tok(cs);
        next_tok(cs);
        skip_newlines(cs);
    }

    /* 函数列表 */
    while (cur_tok(cs) != TOK_EOF) {
        if (cur_tok(cs) == TOK_FUNC) {
            next_tok(cs);
            ir_func(cs);
        } else {
            nihao_error(cs, "ir: unsupported top-level token '%s'",
                        token_name(cur_tok(cs)));
            next_tok(cs);
        }
    }
    return 0;
}

/* 后端统一入口 */
int ir_compile(CompilerState *cs, const char *filename, int backend, int verbose)
{
    (void)verbose;
    if (ir_parse_file(cs, filename) != 0) return -1;
    if (cs->error_count > 0) return -1;

    /* verbose: dump IR 指令序列（调试用） */
    if (verbose) {
        for (int fi = 0; fi < P->fn_count; fi++) {
            IrFn *f = &P->fns[fi];
            printf("IR fn: %s (params=%d, vregs=%d)\n",
                   f->name, f->param_count, f->vreg_count);
            for (int i = 0; i < f->ins_count; i++) {
                IrIns *in = &f->ins[i];
                printf("  %2d: op=%-3d dst=%2d a=%2d b=%2d imm=%lld%s%s\n",
                       i, (int)in->op,
                       in->dst, in->a, in->b, (long long)in->imm,
                       in->sym ? " sym=" : "", in->sym ? in->sym : "");
            }
        }
    }

    char out[1024];
    snprintf(out, sizeof(out), "%s.%s", cs->output_file ? cs->output_file : "a.out",
             backend == 2 ? "c" : "s");
    if (backend == 2) {
        if (irgen_c_emit(P, out) != 0) return -1;
    } else {
        if (irgen_native_emit(P, out) != 0) return -1;
    }

    /* 用 tcc 汇编/编译 + 链接为可执行 */
    char cmd[1600];
    snprintf(cmd, sizeof(cmd), "tcc \"%s\" -o \"%s\"", out,
             cs->output_file ? cs->output_file : "a.out");
    if (cs->verbose) printf("Invoking: %s\n", cmd);
    int rc = system(cmd);
    return rc == 0 ? 0 : -1;
}
