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
 * 目的：验证 IR -> C 与 IR -> riscv64/loongarch64 汇编两条出码路线端到端可行
 *       （无机器码发射器的宿主上 ir-native 回退到 IR -> C）；
 *       全量语法迁移到 IR 是本骨架之后的迭代工作。
 * ============================================================ */
#include "ir.h"
#include "ir_backend.h"

static IrProg *P;
static IrFn *F;
static int *vt;             /* 局部变量: name 序号 -> vreg（ALLOCA 槽） */
static int *pt;             /* 变量指向类型（阶段 1 类型化指针，2026-08-31）：
                             *   -1    = 非指针
                             *   >= 0  = 指向聚合类型（agg_types 索引，供 -> 用）
                             *   < -1  = 指向标量（PT_SCALAR 编码，-2-code） */
/* 标量指针编码：code 与 vtype 同编码（0=i64 1=f64 2=i8 3=i16 4=i32 5=u8 6=u16 7=u32 8=f32）。
 * pt[] 用负值区与聚合索引（>=0）区分；-1 保留给"非指针"。 */
#define PT_SCALAR(code)   (-2 - (code))
#define PT_IS_SCALAR(p)   ((p) < -1)
#define PT_SCALAR_CODE(p) (-2 - (p))
static const char **vn;
static int *ve;             /* 数组元素数（0=标量）；数组元素槽 vreg = vt[i]+k */
static int *vetyp;          /* 数组元素类型编码（与 vtype 同编码；0=未记录）——元素截断/浮点标记 */
static int *vty;            /* 变量聚合类型索引（-1=标量/基本数组）；>=0 查 agg_types */
static int *vvis;           /* 变量可见性/存储期：0=var 1=const 2=flow 3=static 4=undef */
static int *vstate;        /* M2 借用状态：0=VALID 1=FROZEN 2=INVALID（所有权检查） */
static int *vborrow_src;   /* 本变量借用的源 vi（-1=无）；作用域退出时解冻源 */
static int *vptr;          /* 是否指针类（void）：1=是（仅指针类有所有权语义） */

/* 数组维度：vdim[vi][k] = 第 k 维长度（行主序）；vndim[vi] = 维数。
 * 0=标量；1=平铺一维（历史行为）；>=2=多维（PB-67）。
 * 槽模型下多维数组按行主序摊平成连续槽：扁平下标 = Σ idx[k] * stride[k]，
 * stride[k] = 第 k 维之后各维长度之积。 */
#define IR_MAX_DIMS 4
static int (*vdim)[IR_MAX_DIMS];
static int *vndim;

/* 变量存储形态（PB-68 指针类型后缀档）：
 *   0 = 帧槽/帧数组——元素就放在帧槽里，下标按「帧槽基址 + 偏移」寻址；
 *   1 = 指针值数组——槽里存的是一个地址（`void[N] = malloc(void[N])` 的退化形态），
 *       下标按「指针值 + 偏移」寻址。 */
#define VM_SLOT   0
#define VM_PTRARR 1
static int *vmode;
/* 指针来源（PB-81，待裁 33 选 (c) provenance）：
 *   1 = 指针值来自帧地址（`&聚合体变量`）——帧槽在汇编档向下生长、在 C 档向上；
 *   0 = 堆/池地址（malloc、字符串池）或未知（参数/跨函数返回）——两侧都向上。
 * 只有帧来源的指针做成员偏移时才改用方向感知的 IR_ELEM_ADDR（汇编减、C 加）。 */
static int *vfrm;
/* 数组元素字节步长（PB-82）：0=默认 8 字节槽；1/2/4/8 = 紧凑存储（char[9] 占 9 字节，
 * 块按 ceil(N*es/8) 槽分配）。动态字符串（vetyp=8）另按 1 字节指针寻址。 */
static int *velemsz;
/* PB-83：聚合体/紧凑数组的块槽数（ADDR_LO 用；0=标量槽） */
static int *vblk;
/* PB-83：聚合体**参数**（按值展平）——成员按「参数槽序号 base+fi」访问，
 * 不走字节布局（调用方按成员序传值，被调方把它们当参数槽本地副本）。 */
static int *vaparam;

/* PB-72：逻辑长度是否静态可知（A 侧 Symbol.len_known）。ve[] 的 0 既表示「标量」
 * 也表示「空动态字符串」，光看 ve>0 会把后者误判成不可知 ⇒ 单开一张标志表。
 * 只在数组声明 / 字符串字面量初值 / 切片赋值这些真正确定长度的地方置 1。 */
static int *vlen_known;

/* 可见性常量直接用 ncc.h 的 Visibility 枚举（PB-27.3+9：与 cgen 端 enum nihao_vis 数值统一：
 *   VIS_UNDEF=0, VIS_CONST=1, VIS_FLOW=2, VIS_STATIC=3, VIS_VAR=4）。 */

/* 前瞻第 offset 个 token（0=当前 token，1=下一个）——与 parser.c 的 peek_ahead 同语义。
 * 词法状态整体快照/还原；标识符串由 strdup 永久持有（还原后仍可读）。
 * lexer_peek 只看一个 token，判定「括号类型」`(i32)` 需要看两个 → 用这里的前瞻。 */
static TokenType ir_peek_ahead(CompilerState *cs, int offset)
{
    LexerState *lex = cs->parser.lex;
    LexerState save = *lex;
    TokenType t = lex->tok;
    for (int i = 0; i < offset; i++) {
        lexer_next(lex);
        t = lex->tok;
    }
    *lex = save;
    return t;
}

/* ---------- M2 所有权/借用静态检查（移植自 vis.c，基于 vi 索引） ---------- */
#define IR_VS_VALID   0
#define IR_VS_FROZEN  1
#define IR_VS_INVALID 2

static const char *ir_vis_str(int v)
{
    return v == VIS_CONST ? "const" : v == VIS_STATIC ? "static" :
           v == VIS_FLOW ? "flow" : "var";
}

/* 转移矩阵：src→dst 是否禁止（1=禁止）。对齐 vis_check_transfer */
static int ir_vis_transfer(int svis, int dvis)
{
    switch (svis) {
        case VIS_CONST:  return (dvis == VIS_CONST) ? 0 : 1;
        case VIS_STATIC: return (dvis == VIS_CONST || dvis == VIS_STATIC) ? 0 : 1;
        case VIS_FLOW:   return (dvis == VIS_CONST || dvis == VIS_FLOW ||
                                  dvis == VIS_VAR) ? 0 : 1;
        default:         return (dvis == VIS_CONST || dvis == VIS_VAR) ? 0 : 1; /* var */
    }
}

/* 声明/赋值转移检查：dst_vi 接收 src_vi 的所有权 */
static void ir_vis_check_assign(CompilerState *cs, int dst_vi, int src_vi)
{
    if (src_vi < 0 || dst_vi < 0 || src_vi == dst_vi) return;
    if (!vptr[src_vi]) return;                 /* 仅指针类有所有权语义 */
    if (vstate[src_vi] == IR_VS_INVALID) {
        nihao_error(cs, "%s is invalidated (ownership moved); cannot use it",
                    vn[src_vi]);
        return;
    }
    if (vstate[src_vi] == IR_VS_FROZEN &&
        (vvis[dst_vi] == VIS_FLOW || vvis[dst_vi] == VIS_STATIC)) {
        /* PB-84：被借用的源不能再转移所有权（flow/static 接收） */
        nihao_error(cs, "cannot transfer ownership of '%s': it is frozen by an active borrow",
                    vn[src_vi]);
        return;
    }
    if (ir_vis_transfer(vvis[src_vi], vvis[dst_vi])) {
        nihao_error(cs, "cannot assign %s (visibility %s) to %s (%s): "
                        "target lifetime shorter than source",
                    vn[src_vi], ir_vis_str(vvis[src_vi]),
                    vn[dst_vi], ir_vis_str(vvis[dst_vi]));
        return;
    }
    if (vvis[src_vi] == VIS_FLOW) {
        vstate[src_vi] = (vvis[dst_vi] == VIS_FLOW) ? IR_VS_INVALID : IR_VS_FROZEN;
    } else if (vvis[src_vi] == VIS_VAR) {
        if (vvis[dst_vi] == VIS_CONST || vvis[dst_vi] == VIS_VAR)
            vstate[src_vi] = IR_VS_FROZEN;
    }
    if (vstate[src_vi] == IR_VS_FROZEN) vborrow_src[dst_vi] = src_vi;
}

/* 读检查：所有权已转移（INVALID）则不可用 */
static void ir_vis_check_usable(CompilerState *cs, int vi)
{
    if (vi < 0) return;
    if (vstate[vi] == IR_VS_INVALID)
        nihao_error(cs, "%s is invalidated: its ownership has been moved", vn[vi]);
}

/* 写检查：被活动借用冻结（FROZEN）则不可写 */
static void ir_vis_check_writable(CompilerState *cs, int vi)
{
    if (vi < 0) return;
    if (vstate[vi] == IR_VS_FROZEN)
        nihao_error(cs, "%s is frozen by an active borrow (const/var)", vn[vi]);
    ir_vis_check_usable(cs, vi);
}

/* PB-90：吞掉当前物理行剩余 token（函数体内没有 NEWLINE token，按行号判断）。
 * 作为诊断后的恢复动作，避免残余 token 在下一语句里级联报错。 */
static void ir_skip_line(CompilerState *cs)
{
    int ln = cs->parser.lex->last_line_num;
    while (cur_tok(cs) != TOK_RBRACE && cur_tok(cs) != TOK_EOF &&
           cs->parser.lex->last_line_num == ln)
        next_tok(cs);
    skip_newlines(cs);
}
/* PB-90：const 变量的**自有存储**不可写（string/char[]/定长数组/聚合体内容）。
 * 指针变量的「指向内存」不受 const 限制（如 `const p void` 仍可写 `p.()`），
 * 故调用方只对非指针类变量调用本检查。 */
static int ir_check_const_store(CompilerState *cs, int vi)
{
    if (vi >= 0 && vvis[vi] == VIS_CONST) {
        nihao_error(cs, "ir: cannot modify the const '%s'", vn[vi]);
        return 0;
    }
    return 1;
}

/* 调用点参数检查：dst_vis = 被调函数参数可见性（来自 IrFn.param_vis，属被调函数
 * 作用域，调用方无法直接索引其 vi）。仅对指针类实参（vptr[src_vi]）生效。
 * 返回 1 表示发生借用（源被冻结），调用结束后须解冻源；0 表示转移/无变化。 */
static int ir_vis_check_arg(CompilerState *cs, int dst_vis, int src_vi)
{
    if (src_vi < 0) return 0;
    if (!vptr[src_vi]) return 0;                 /* 仅指针类有所有权语义 */
    if (vstate[src_vi] == IR_VS_INVALID) {
        nihao_error(cs, "%s is invalidated (ownership moved); cannot use it",
                    vn[src_vi]);
        return 0;
    }
    if (ir_vis_transfer(vvis[src_vi], dst_vis)) {
        nihao_error(cs, "cannot pass %s (visibility %s) to parameter (visibility %s): "
                        "target lifetime shorter than source",
                    vn[src_vi], ir_vis_str(vvis[src_vi]), ir_vis_str(dst_vis));
        return 0;
    }
    if (vvis[src_vi] == VIS_FLOW) {
        vstate[src_vi] = (dst_vis == VIS_FLOW) ? IR_VS_INVALID : IR_VS_FROZEN;
        return (dst_vis == VIS_FLOW) ? 0 : 1;   /* flow→flow: 转移(转移后不解冻)；flow→var/const: 借用(解冻) */
    } else if (vvis[src_vi] == VIS_VAR) {
        if (dst_vis == VIS_CONST || dst_vis == VIS_VAR) {
            vstate[src_vi] = IR_VS_FROZEN;
            return 1;
        }
    }
    return 0;
}
static int *vtype;          /* 变量数值类型：0=int(默认) 1=double(f64/f32 槽化) */
static int vn_count, vn_cap;

/* goto/label：标签名 → label id（per 函数，ir_func 进入时清零）。
 * 用独立编号空间（BASE 偏移）避免与 ir_new_label 的 if/while 分支冲突 */
#define IR_MAX_LABELS 32
#define IR_GOTO_LABEL_BASE 1000
static char g_label_names[IR_MAX_LABELS][64];
static int g_label_count;
static int ir_label_get(const char *name)
{
    for (int i = 0; i < g_label_count; i++)
        if (strcmp(g_label_names[i], name) == 0) return IR_GOTO_LABEL_BASE + i;
    if (g_label_count < IR_MAX_LABELS) {
        snprintf(g_label_names[g_label_count], 64, "%s", name);
        return IR_GOTO_LABEL_BASE + g_label_count++;
    }
    return IR_GOTO_LABEL_BASE;
}
/* vreg 是否编译期常量（查其最近一次定义是否 IR_CONST） */
static int vreg_const(IrFn *f, int vr, long long *out)
{
    for (int i = f->ins_count - 1; i >= 0; i--) {
        if (f->ins[i].dst == vr) {
            if (f->ins[i].op == IR_CONST) { *out = f->ins[i].imm; return 1; }
            return 0;
        }
    }
    return 0;
}
static int last_mr_buf = -1; /* 最近一次 sret（struct 返回）调用返回的缓冲地址 vreg（供声明拷贝） */

/* ============================================================
 * cooking 编译期变量表（PB-9 深化）：cooking 块内 `const NAME [TYPE] = expr`
 * 声明的编译期常量，供后续 static_assert / 编译期表达式使用（跨块共享）。
 * 编译期语义，不生成运行时代码。
 * ============================================================ */
static struct { const char *name; long long val; } ct_vars[64];
static int ct_vars_count;

static int ct_var_exist(const char *name)
{
    for (int i = 0; i < ct_vars_count; i++) {
        if (strcmp(ct_vars[i].name, name) == 0) return 1;
    }
    return 0;
}
static long long ct_var_find(const char *name)
{
    for (int i = 0; i < ct_vars_count; i++) {
        if (strcmp(ct_vars[i].name, name) == 0) return ct_vars[i].val;
    }
    return 0;
}

/* ---- 编译期函数（PB-9 深化：cooking-call 宏式展开）----
 * cooking 块内 `const NAME(p1, p2) = expr` 定义；调用 NAME(a, b) 时
 * 参数名替换为实参字面量 → 临时 lexer 求值（实参须编译期常量） */
#define IR_MAX_CTFUNC 32
#define IR_MAX_CTFPARAM 4
static long long ir_const_expr(CompilerState *cs);   /* ct_func_call 前向 */
static struct {
    const char *name;
    char params[IR_MAX_CTFPARAM][64];
    int param_count;
    char expr_src[512];
} ct_funcs[IR_MAX_CTFUNC];
static int ct_funcs_count;

static int ct_func_find(const char *name)
{
    for (int i = 0; i < ct_funcs_count; i++)
        if (strcmp(ct_funcs[i].name, name) == 0) return i;
    return -1;
}
static long long ct_func_call(CompilerState *cs, int cf, long long *args, int ac)
{
    /* 参数替换：expr_src 中参数名（词边界）→ 实参字面量 */
    char buf[512];
    int bi = 0;
    const char *src = ct_funcs[cf].expr_src;
    int i = 0;
    while (src[i] != '\0' && bi < (int)sizeof(buf) - 16) {
        if ((src[i] >= 'a' && src[i] <= 'z') || (src[i] >= 'A' && src[i] <= 'Z') ||
            src[i] == '_') {
            int j = i;
            while ((src[j] >= 'a' && src[j] <= 'z') || (src[j] >= 'A' && src[j] <= 'Z') ||
                   (src[j] >= '0' && src[j] <= '9') || src[j] == '_') j++;
            int plen = j - i;
            int pi = -1;
            for (int k = 0; k < ct_funcs[cf].param_count; k++)
                if ((int)strlen(ct_funcs[cf].params[k]) == plen &&
                    strncmp(src + i, ct_funcs[cf].params[k], (size_t)plen) == 0) {
                    pi = k; break;
                }
            if (pi >= 0 && pi < ac) {
                bi += snprintf(buf + bi, (int)sizeof(buf) - bi - 1, "%lld", args[pi]);
            } else {
                memcpy(buf + bi, src + i, (size_t)plen); bi += plen;
            }
            i = j;
        } else {
            buf[bi++] = src[i++];
        }
    }
    buf[bi] = '\0';
    /* 临时 lexer 求值（保存/恢复 cs->parser.lex） */
    LexerState tlex;
    LexerState *save = cs->parser.lex;
    cs->parser.lex = &tlex;
    lexer_init(cs, "<cooking-fn>", buf);
    lexer_next(&tlex);
    long long v = ir_const_expr(cs);
    cs->parser.lex = save;
    return v;
}

/* vreg 浮点类型辅助（PB-13：vreg_type 表在 IrFn，随 vreg_count 增长） */
static void ir_set_double(int vr)
{
    if (vr >= 0 && F && vr < F->vreg_count) F->vreg_type[vr] = 1;
}
static int ir_is_double(int vr)
{
    return vr >= 0 && F && vr < F->vreg_count && F->vreg_type[vr] == 1;
}

/* 混合类型提升：int → double（PB-浮点 ABI）。已 double 直接返回，否则发射 IR_ITOD */
static int ir_to_double(int vr)
{
    if (ir_is_double(vr)) return vr;
    int nd = ir_new_vreg(F);
    ir_emit(F, IR_ITOD, nd, vr, -1, 0);
    ir_set_double(nd);
    return nd;
}

/* ============================================================
 * 窄整数类型（PB-1）：变量 vtype 编码
 *   0=i64(默认) 1=f64/f32 2=i8 3=i16 4=i32 5=u8 6=u16 7=u32
 * 语义：槽仍 8 字节，赋值时截断 + 符号/零扩展存槽（读回即正确值）
 * ============================================================ */
static int ir_type_imm(int t)   /* TRUNC imm：0:i8 1:i16 2:i32 3:u8 4:u16 5:u32；-1=非窄 */
{
    switch (t) {
        case 2: return 0;
        case 3: return 1;
        case 4: return 2;
        case 5: return 3;
        case 6: return 4;
        case 7: return 5;
        default: return -1;
    }
}
static int ir_trunc_value(int vr, int vtype_code)   /* 窄类型截断（非窄原样返回） */
{
    int imm = ir_type_imm(vtype_code);
    if (imm < 0) return vr;
    int nv = ir_new_vreg(F);
    ir_emit(F, IR_TRUNC, nv, vr, -1, imm);
    return nv;
}

/* 赋值目标类型协调（PB-1 DTOI）：vr 协调到目标类型 vtype_code
 *  - 1 (float 目标) + int 源 → ITOD
 *  - int 目标 + double 源 → DTOI（截断向零）
 *  - 窄整数目标 → TRUNC */
static int ir_coerce(int vr, int vtype_code)
{
    if (vtype_code == 1) {
        if (!ir_is_double(vr)) return ir_to_double(vr);
        return vr;
    }
    if (vtype_code == 8) {
        /* f32 严格宽度：先提升 double（int 源）再 FTRUNC 单精度截断 */
        if (!ir_is_double(vr)) vr = ir_to_double(vr);
        int nv = ir_new_vreg(F);
        ir_emit(F, IR_FTRUNC, nv, vr, -1, 0);
        ir_set_double(nv);
        return nv;
    }
    if (ir_is_double(vr)) {
        int nv = ir_new_vreg(F);
        ir_emit(F, IR_DTOI, nv, vr, -1, 0);
        vr = nv;
    }
    return ir_trunc_value(vr, vtype_code);
}

/* 可见性常量（与 cgen 的 enum nihao_vis 对齐）：NH_UNDEF=0,NH_CONST,NH_FLOW,NH_STATIC,NH_VAR */

/* 前向声明（ir_agg_decl 在 var_declare/ir_expr 定义之前调用，
 * 隐式声明会导致 x64 指针参数截断，必须显式声明） */
static int var_declare(const char *name, int elems, int type_idx, int vis, int elem_bytes);
static int ir_expr(CompilerState *cs);
static int ir_scalar_code_of_token(TokenType tt);
static int64_t ir_load_width_imm(int code);   /* PB-75(b) */
static int ir_store_width(int code);           /* PB-75(b) */
static void ir_type_decl_named(CompilerState *cs, const char *tname);  /* PB-80 */
static long long type_size_token(TokenType t);   /* PB-82（链求值先于定义处使用） */
static void ir_block_copy_bytes(int dst_val, int src_val, int bytes);  /* PB-83 */
static int ir_block_slots(int vi);
static int ir_agg_base(int vi);
static int ir_ptr_byte_off(int base, int off);
static int ir_member_addr(CompilerState *cs, int vi, int ti, int fi, int off);
static int ir_member_vcode(int ti, int fi);
static int ir_member_swidth(int ti, int fi);
static int ir_bf_unit(int ti, int fi);
static int ir_bf_shift(int ti, int fi);
static void ir_copy_literal(CompilerState *cs, int dst, const char *str);

/* ---- struct/union/enum 类型表 ---- */
#define IR_MAX_AGG 16
#define IR_MAX_MEMB 32
typedef struct {
    const char *name;       /* 类型名（enum 时是枚举名） */
    int kind;               /* 0=struct 1=union 2=enum */
    const char *mnames[IR_MAX_MEMB];   /* 成员名（enum 为 variant 名） */
    long long mvals[IR_MAX_MEMB];      /* enum variant 值 */
    int mbits[IR_MAX_MEMB];            /* 位域宽度（0=非位域） */
    int mtype[IR_MAX_MEMB];            /* 成员聚合类型索引（-1=标量；嵌套用） */
    int moff[IR_MAX_MEMB];             /* 成员起始字节偏移（PB-83，C 布局；位域=存储单元起点） */
    int mbit[IR_MAX_MEMB];             /* 位域绝对位偏移（-1=非位域）（PB-83） */
    int misarr[IR_MAX_MEMB];           /* 成员带 `[...]` 后缀（含动态 `char[]`，PB-83） */
    int marr_len[IR_MAX_MEMB];         /* 数组成员的元素数（0=动态字符串成员） */
    int marr_esize[IR_MAX_MEMB];       /* 数组成员的元素字节宽（1/2/4/8） */
    int msize[IR_MAX_MEMB];            /* 标量成员的字节宽（1/2/4/8；0=聚合成员）（PB-74 补） */
    int mscode[IR_MAX_MEMB];           /* 标量成员类型码（0=i64 1=f64 2=i8 … 8=f32；-1=聚合成员）（PB-76 补） */
    int mdef_kind[IR_MAX_MEMB];        /* 成员默认值形态：0=无 1=整型 2=浮点 3=字符串（PB-79） */
    long long mdef_int[IR_MAX_MEMB];
    double mdef_dbl[IR_MAX_MEMB];
    const char *mdef_str[IR_MAX_MEMB];
    int mcount;
    int bytes;                         /* 实际字节大小（C 布局，PB-83） */
    int align;                         /* 类型对齐（成员最大对齐） */
    int mslots;                        /* 占用槽数 = ceil(bytes/8)（帧分配用） */
} IrAggType;
static IrAggType agg_types[IR_MAX_AGG];
static int agg_count;

/* 递归计算成员起始槽偏移与总槽数（嵌套 struct；union=最大成员槽数，enum=1 槽；
 * mslots>0 表示已计算（memset 清零后首次计算）） */
static int agg_align_up(int v, int al)
{
    if (al <= 1) return v;
    return (v + al - 1) / al * al;
}

/* 成员的大小与对齐（字节）：嵌套聚合取其 bytes/align；数组成员 len*es；
 * `[` 但无长度的动态字符串成员 = 指针（8/8）；标量取 msize。 */
static void agg_member_shape(int ti, int i, int *sz, int *al)
{
    IrAggType *a = &agg_types[ti];
    if (a->mtype[i] >= 0) {
        agg_compute_offsets(a->mtype[i]);
        *sz = agg_types[a->mtype[i]].bytes;
        *al = agg_types[a->mtype[i]].align;
        if (*al < 1) *al = 1;
        return;
    }
    if (a->misarr[i]) {
        if (a->marr_len[i] <= 0) {
            *sz = 8; *al = 8;          /* 动态字符串成员：指针 */
        } else {
            int es = a->marr_esize[i] > 0 ? a->marr_esize[i] : 1;
            *sz = a->marr_len[i] * es;
            *al = es > 8 ? 8 : es;
        }
        return;
    }
    int s = a->msize[i] > 0 ? a->msize[i] : 8;
    *sz = s;
    *al = s > 8 ? 8 : s;
}

/* PB-83：C 布局——struct 按成员对齐顺序排布（位域按同类型单元打包），
 * union 全部偏移 0、大小取最大成员；mslots 仅用于帧分配（ceil(bytes/8)）。 */
static int agg_compute_offsets(int ti)
{
    IrAggType *a = &agg_types[ti];
    if (a->mslots > 0) return a->mslots;
    if (a->kind == 2) {
        a->bytes = 8; a->align = 8; a->mslots = 1;
        return 1;
    }
    if (a->kind == 1) {
        int mx = 1, ma = 1;
        for (int i = 0; i < a->mcount; i++) {
            int sz = 0, al = 1;
            agg_member_shape(ti, i, &sz, &al);
            a->moff[i] = 0;
            a->mbit[i] = -1;
            if (sz > mx) mx = sz;
            if (al > ma) ma = al;
        }
        a->align = ma;
        a->bytes = agg_align_up(mx, ma);
        a->mslots = (a->bytes + 7) / 8;
        return a->mslots;
    }
    int cur = 0, malign = 1;
    int bf_start = -1, bf_sz = 0, bf_used = 0;   /* 当前位域单元 */
    for (int i = 0; i < a->mcount; i++) {
        if (a->mbits[i] > 0) {
            int usize = a->msize[i] > 0 ? a->msize[i] : 1;
            if (bf_start < 0 || bf_sz != usize ||
                bf_used + a->mbits[i] > usize * 8) {
                cur = agg_align_up(cur, usize);
                bf_start = cur; bf_sz = usize; bf_used = 0;
                cur += usize;
            }
            a->moff[i] = bf_start;
            a->mbit[i] = bf_start * 8 + bf_used;
            bf_used += a->mbits[i];
            if (usize > malign) malign = usize;
            continue;
        }
        bf_start = -1;
        int sz = 1, al = 1;
        agg_member_shape(ti, i, &sz, &al);
        cur = agg_align_up(cur, al);
        a->moff[i] = cur;
        a->mbit[i] = -1;
        cur += sz;
        if (al > malign) malign = al;
    }
    a->align = malign;
    a->bytes = agg_align_up(cur, malign);
    a->mslots = (a->bytes + 7) / 8;
    return a->mslots;
}

/* enum 常量查找 */
static int enum_const_find(const char *name, long long *val)
{
    for (int i = 0; i < agg_count; i++) {
        IrAggType *a = &agg_types[i];
        if (a->kind != 2) continue;
        for (int j = 0; j < a->mcount; j++) {
            if (strcmp(a->mnames[j], name) == 0) {
                *val = a->mvals[j];
                return 1;
            }
        }
    }
    return 0;
}

/* 聚合类型名查找（struct/union/enum） */
static int agg_type_find(const char *name)
{
    for (int i = 0; i < agg_count; i++)
        if (strcmp(agg_types[i].name, name) == 0) return i;
    return -1;
}

/* 成员名在聚合类型中的索引（struct 偏移 / union 均 0） */
static int agg_member_find(int ti, const char *mname)
{
    if (ti < 0 || ti >= agg_count) return -1;
    IrAggType *a = &agg_types[ti];
    for (int i = 0; i < a->mcount; i++)
        if (strcmp(a->mnames[i], mname) == 0) return i;
    return -1;
}

/* ---- 类型别名表（顶层 alias Name = Type，BNF §2.2）----
 * 别名只落「指向什么」：标量类型码或聚合类型索引。解析处遇到别名名时按此解引用。 */
#define IR_MAX_ALIAS 32
typedef struct {
    const char *name;
    int scalar_code;   /* 指向的标量类型码（-1 = 非标量） */
    int agg_ti;        /* 指向的聚合类型索引（-1 = 非聚合） */
} IrAliasType;
static IrAliasType ir_aliases[IR_MAX_ALIAS];
static int ir_alias_count;

static int alias_find(const char *name)
{
    for (int i = 0; i < ir_alias_count; i++)
        if (strcmp(ir_aliases[i].name, name) == 0) return i;
    return -1;
}

/* ---- 全局变量表（顶层 var/const 声明）----
 * 局部变量表是 per-function 的（var_reset），跨函数共享的顶层变量另存一份，
 * 存储落在 IrProg 的全局池里（后端在 .data 段 / 文件作用域落一份真实存储）。 */
#define IR_MAX_GLOBAL 64
typedef struct {
    const char *name;
    int sym_idx;      /* IrProg.glb_syms 下标 */
    int is_double;
    int type_code;    /* 窄整型码（0=i64 1=f64 2=i8 3=i16 4=i32 5=u8 6=u16 7=u32 8=f32） */
    int vis;          /* 可见性前缀（VIS_CONST = 只读；其余同局部变量语义） */
} IrGlobalVar;
static IrGlobalVar ir_globals[IR_MAX_GLOBAL];
static int ir_global_count;

static int global_find(const char *name)
{
    for (int i = 0; i < ir_global_count; i++)
        if (strcmp(ir_globals[i].name, name) == 0) return i;
    return -1;
}

/* ---- 位域（槽内低 N 位）---- */
static int ir_bitmask(int bits)
{
    long long m = (bits >= 64) ? -1 : ((1LL << bits) - 1);
    int v = ir_new_vreg(F);
    ir_emit(F, IR_CONST, v, -1, -1, m);
    return v;
}
/* 位域存储单元与位内偏移（PB-83：moff=单元字节起点、mbit=绝对位偏移） */
static int ir_bf_unit(int ti, int fi)
{
    int u = agg_types[ti].msize[fi];
    return (u == 1 || u == 2 || u == 4 || u == 8) ? u : 1;
}
static int ir_bf_shift(int ti, int fi)
{
    return agg_types[ti].mbit[fi] - agg_types[ti].moff[fi] * 8;
}
static int ir_bf_maskv(int bits, int shift)
{
    if (bits <= 0) return 0;
    if (bits >= 64) return shift == 0 ? -1 : ((-1LL) << shift);
    return (int)((((long long)1 << bits) - 1) << shift);
}

/* 读位域：按单元宽零扩展读 → 右移 → 掩码 */
static int ir_bf_load(int addr, int ti, int fi)
{
    int bits = agg_types[ti].mbits[fi];
    int unit = ir_bf_unit(ti, fi);
    int sh = ir_bf_shift(ti, fi);
    int vr = ir_new_vreg(F);
    if (unit == 8) ir_emit(F, IR_LOAD, vr, addr, -1, 0);
    else ir_emit(F, IR_LOAD, vr, addr, -1, unit);
    if (sh > 0) {
        int sc = ir_new_vreg(F);
        ir_emit(F, IR_CONST, sc, -1, -1, sh);
        int r1 = ir_new_vreg(F);
        ir_emit(F, IR_SHR, r1, vr, sc, 0);
        vr = r1;
    }
    if (bits > 0 && bits < 64) {
        int mv = ir_bitmask(bits);
        int r2 = ir_new_vreg(F);
        ir_emit(F, IR_AND, r2, vr, mv, 0);
        vr = r2;
    }
    return vr;
}

/* 写位域（RMW，按单元宽读写）：clear mask → or (v<<shift)&mask → STORE 单元 */
static void ir_bf_store(int addr, int v, int ti, int fi)
{
    int bits = agg_types[ti].mbits[fi];
    int unit = ir_bf_unit(ti, fi);
    int sh = ir_bf_shift(ti, fi);
    int w = (unit == 8) ? 0 : unit;
    int cur = ir_new_vreg(F);
    ir_emit(F, IR_LOAD, cur, addr, -1, w);
    int maskv = ir_bf_maskv(bits, sh);
    int mv = ir_new_vreg(F);
    ir_emit(F, IR_CONST, mv, -1, -1, maskv);
    int notm = ir_new_vreg(F);
    ir_emit(F, IR_NOT, notm, mv, -1, 0);
    int c1 = ir_new_vreg(F);
    ir_emit(F, IR_AND, c1, cur, notm, 0);
    int vv = v;
    if (sh > 0) {
        int sc = ir_new_vreg(F);
        ir_emit(F, IR_CONST, sc, -1, -1, sh);
        vv = ir_new_vreg(F);
        ir_emit(F, IR_SHL, vv, v, sc, 0);
    }
    int v1 = ir_new_vreg(F);
    ir_emit(F, IR_AND, v1, vv, mv, 0);
    int r = ir_new_vreg(F);
    ir_emit(F, IR_OR, r, c1, v1, 0);
    ir_emit(F, IR_STORE, -1, addr, r, w);
}

/* 聚合初始化列表（嵌套递归）：{e0, e1, ...} 或 {{...}, {...}}
 * ti=当前类型，base=当前类型在聚合变量中的槽起点；成员 k 的槽起点 =
 * base + moff[k]（union 共享 base）。聚合成员遇 { 递归填充。 */
/* PB-79：默认值落地。结构/联合定义处的 `= 常量` 在
 * ① 变量无初值声明、② 聚合初值列表未点名的成员 两处生效。
 * kind：0=无 1=整型 2=浮点 3=字符串字面量（存字符串池地址）。 */
static int agg_has_defaults(int ti)
{
    IrAggType *a = &agg_types[ti];
    for (int i = 0; i < a->mcount; i++)
        if (a->mdef_kind[i]) return 1;
    for (int i = 0; i < a->mcount; i++)
        if (a->mtype[i] >= 0 && agg_has_defaults(a->mtype[i])) return 1;
    return 0;
}

static void ir_member_default_store(CompilerState *cs, int vi, int ti, int fi, int off)
{
    IrAggType *a = &agg_types[ti];
    int v = -1;
    int w = ir_member_swidth(ti, fi);
    if (a->mdef_kind[fi] == 1) {
        v = ir_new_vreg(F);
        ir_emit(F, IR_CONST, v, -1, -1, a->mdef_int[fi]);
    } else if (a->mdef_kind[fi] == 2) {
        union { double d; int64_t i; } u;
        u.d = a->mdef_dbl[fi];
        v = ir_new_vreg(F);
        ir_emit(F, IR_CONST, v, -1, -1, u.i);
        ir_set_double(v);
        w = 0;              /* double 走 8 字节存储（后端按 vreg_type 发 double*） */
    } else if (a->mdef_kind[fi] == 3) {
        int si = ir_add_string(P, a->mdef_str[fi]);
        v = ir_new_vreg(F);
        IrIns *ld = &F->ins[ir_emit(F, IR_LD_ADDR, v, -1, -1, 0)];
        ld->sym = P->str_syms[si];
        w = 0;              /* 动态字符串成员槽里存池地址 */
    } else {
        return;
    }
    int addr = ir_new_vreg(F);
    addr = ir_ptr_byte_off(ir_agg_base(vi), off);
    ir_emit(F, IR_STORE, -1, addr, v, w);
    (void)cs;
}

/* 递归补默认值：struct 逐成员；union 只取首个带默认值的成员 */
static void ir_agg_defaults(CompilerState *cs, int vi, int ti, int base)
{
    IrAggType *a = &agg_types[ti];
    if (a->kind == 1) {
        for (int i = 0; i < a->mcount; i++)
            if (a->mdef_kind[i]) {
                ir_member_default_store(cs, vi, ti, i, base);
                return;
            }
        return;
    }
    for (int i = 0; i < a->mcount; i++) {
        int off = base + a->moff[i];
        if (a->mtype[i] >= 0) {
            if (agg_has_defaults(a->mtype[i]))
                ir_agg_defaults(cs, vi, a->mtype[i], off);
        } else if (a->mdef_kind[i]) {
            ir_member_default_store(cs, vi, ti, i, off);
        }
    }
}

static void ir_agg_init(CompilerState *cs, int vi, int ti, int base)
{
    IrAggType *a = &agg_types[ti];
    int k = 0;
    int seen[IR_MAX_MEMB];
    int any = 0;
    for (int i = 0; i < a->mcount; i++) seen[i] = 0;
    next_tok(cs);           /* { */
    if (cur_tok(cs) != TOK_RBRACE) {
        for (;;) {
            if (cur_tok(cs) == TOK_DOT) {
                /* PB-79：指定式初值 `.member = expr`（BNF §5 <designator>） */
                next_tok(cs);   /* . */
                if (cur_tok(cs) != TOK_IDENTIFIER) {
                    nihao_error(cs, "ir: expected member name after '.' in initializer");
                    break;
                }
                int fi = agg_member_find(ti, cs->parser.lex->tok_str);
                if (fi < 0) {
                    nihao_error(cs, "ir: no member '%s' in type '%s'",
                                cs->parser.lex->tok_str, a->name);
                    next_tok(cs);
                    break;
                }
                next_tok(cs);   /* field */
                expect(cs, TOK_ASSIGN);
                int off = base + ((a->kind == 1) ? 0 : a->moff[fi]);
                if (a->mtype[fi] >= 0 && cur_tok(cs) == TOK_LBRACE) {
                    ir_agg_init(cs, vi, a->mtype[fi], off);
                } else {
                    int v = ir_expr(cs);
                    int addr = ir_ptr_byte_off(ir_agg_base(vi), off);
                    ir_emit(F, IR_STORE, -1, addr, v,
                            ir_member_swidth(ti, fi));
                }
                seen[fi] = 1;
            } else {
                if (k >= a->mcount) break;
                int mt = a->mtype[k];
                int off = base + ((a->kind == 1) ? 0 : a->moff[k]);
                if (mt >= 0 && cur_tok(cs) == TOK_LBRACE) {
                    ir_agg_init(cs, vi, mt, off);
                } else {
                    int v = ir_expr(cs);
                    int addr = ir_ptr_byte_off(ir_agg_base(vi), off);
                    ir_emit(F, IR_STORE, -1, addr, v,
                            ir_member_swidth(ti, k));
                }
                seen[k] = 1;
                k++;
            }
            any = 1;
            if (cur_tok(cs) != TOK_COMMA) break;
            next_tok(cs);
        }
    }
    expect(cs, TOK_RBRACE);
    /* PB-79：未点名的成员补默认值；union 只在整份无初值时取首个默认成员
     * （union 成员共享槽 0，有用户初值后再补默认会把它覆盖掉） */
    if (a->kind == 1) {
        if (!any) ir_agg_defaults(cs, vi, ti, base);
        return;
    }
    for (int i = 0; i < a->mcount; i++) {
        if (seen[i]) continue;
        if (a->mtype[i] >= 0) {
            if (agg_has_defaults(a->mtype[i]))
                ir_agg_defaults(cs, vi, a->mtype[i], base + a->moff[i]);
        } else if (a->mdef_kind[i]) {
            ir_member_default_store(cs, vi, ti, i, base + a->moff[i]);
        }
    }
}

/* 聚合体数组初值（PB-80）：`P arr[N] = { ... }`。
 * 内层花括号逐个元素走 ir_agg_init（元素内默认值/指定式照常）；未给初值的尾数元素补默认值。
 * 外层无花括号（`{1,2,3}`）按 C 的花括号省略规则、按槽序跨元素写入，**不补尾数默认**。
 * 平坦写入按「元素槽号 % mslots」取成员宽——嵌套聚合成员按整槽写（8 字节），属已知边界。 */
static void ir_aggarr_init(CompilerState *cs, int vi, int ti, int total)
{
    IrAggType *a = &agg_types[ti];
    int by = a->bytes > 0 ? a->bytes : 8;
    next_tok(cs);           /* { */
    int flat = (cur_tok(cs) != TOK_RBRACE && cur_tok(cs) != TOK_LBRACE);
    if (flat) {
        /* 扁平位置初值：按成员序跨元素展开（C 花括号省略），不补默认值，剩余成员清 0 */
        int g = 0;
        while (cur_tok(cs) != TOK_RBRACE && cur_tok(cs) != TOK_EOF &&
               g < total * a->mcount) {
            int v = ir_expr(cs);
            int e = g / a->mcount, mi = g % a->mcount;
            int addr = ir_ptr_byte_off(ir_agg_base(vi), e * by + a->moff[mi]);
            ir_emit(F, IR_STORE, -1, addr, v,
                    ir_member_swidth(ti, mi));
            g++;
            if (cur_tok(cs) != TOK_COMMA) break;
            next_tok(cs);
        }
        for (; g < total * a->mcount; g++) {
            int e = g / a->mcount, mi = g % a->mcount;
            int zv = ir_new_vreg(F);
            ir_emit(F, IR_CONST, zv, -1, -1, 0);
            int addr = ir_ptr_byte_off(ir_agg_base(vi), e * by + a->moff[mi]);
            ir_emit(F, IR_STORE, -1, addr, zv, ir_member_swidth(ti, mi));
        }
    } else {
        int e = 0;
        while (cur_tok(cs) != TOK_RBRACE && cur_tok(cs) != TOK_EOF && e < total) {
            if (cur_tok(cs) == TOK_LBRACE) {
                ir_agg_init(cs, vi, ti, e * by);
            } else {
                /* 内层花括号省略：该值落到元素首成员 */
                int v = ir_expr(cs);
                int addr = ir_ptr_byte_off(ir_agg_base(vi),
                                           e * by + (a->mcount ? a->moff[0] : 0));
                ir_emit(F, IR_STORE, -1, addr, v,
                        a->mcount ? ir_member_swidth(ti, 0) : 0);
            }
            e++;
            if (cur_tok(cs) != TOK_COMMA) break;
            next_tok(cs);
        }
        /* 尾数元素全默认值（PA-56 口径） */
        for (; e < total; e++) ir_agg_defaults(cs, vi, ti, e * by);
    }
    expect(cs, TOK_RBRACE);
}

/* 聚合类型声明：name Type [N]... [= {...}|= expr]
 * struct: elems=成员数（顺序槽）；union: 共享槽 0；enum: 单槽。
 * PB-80：聚合体数组——总槽数 = 元素数 × mslots，元素 k 的槽起点 = k*mslots。 */
static void ir_agg_decl(CompilerState *cs, const char *name, int ti, int vis)
{
    IrAggType *a = &agg_types[ti];
    int mslots = (a->kind == 2) ? 1 : a->mslots;
    int dims[IR_MAX_DIMS];
    int nd = 0, total = 1;
    while (cur_tok(cs) == TOK_LBRACKET && nd < IR_MAX_DIMS) {
        next_tok(cs);
        int n = 8;      /* `[...]` 默认容量 8（与 PB-67 同口径） */
        if (cur_tok(cs) == TOK_INT_CONST) {
            n = (int)cs->parser.lex->tok_val.i;
            next_tok(cs);
        } else if (cur_tok(cs) == TOK_ELLIPSIS) {
            next_tok(cs);           /* `...` = 默认容量 8 */
        }
        expect(cs, TOK_RBRACKET);
        dims[nd++] = n;
        total *= n;
    }
    int by = a->bytes > 0 ? a->bytes : 8;
    int elems = (int)(((long long)by * total + 7) / 8);
    int vi = var_declare(name, elems, ti, vis, 0);
    vblk[vi] = elems;
    if (nd > 0) {
        /* 聚合体数组：vndim 标记数组形态（表达式路径据此走元素成员寻址），
         * ve 记「元素个数」供 len()（var_declare 记的是槽数） */
        vndim[vi] = nd;
        for (int k = 0; k < nd; k++) vdim[vi][k] = dims[k];
        ve[vi] = total;
        vlen_known[vi] = 1;
    }
    if (cur_tok(cs) == TOK_ASSIGN) {
        next_tok(cs);
        if (cur_tok(cs) == TOK_LBRACE) {
            if (nd > 0) ir_aggarr_init(cs, vi, ti, total);
            else ir_agg_init(cs, vi, ti, 0);
        } else if (nd > 0) {
            nihao_error(cs, "ir: aggregate array '%s' must be initialized from "
                            "'{ ... }'", name);
            ir_expr(cs);
        } else {
            int v = ir_expr(cs);
            /* sret 调用（struct 返回函数）：缓冲地址是"值"（last_mr_buf），
             * 缓冲值 + k*8 → LOAD → 聚合槽 k（与 sret 声明同逻辑） */
            if (last_mr_buf >= 0) {
                /* PB-83：sret 缓冲（堆，向上）→ 聚合块（低地址起，向上）按字节复制 */
                ir_block_copy_bytes(ir_agg_base(vi), last_mr_buf, by);
                last_mr_buf = -1;
            } else {
                ir_emit(F, IR_MOV, vt[vi], v, -1, 0);
            }
        }
    } else {
        /* PB-79：无初值 → 定义处成员默认值（A 侧 PA-49 / PA-56 同口径）；
         * PB-80：聚合体数组逐元素回填 */
        if (nd > 0) {
            /* PB-84：成员默认值逐项展开的资源守卫（与 A 侧 DEFAULT_INIT_BUF 同口径）——
             * 超过 1024 个成员初始化点即报错，而不是产出超大展开。 */
            if ((long long)total * (a->mcount > 0 ? a->mcount : 1) > 1024) {
                nihao_error(cs, "ir: aggregate initializer is too large to expand "
                                "its member defaults");
            } else {
                for (int e = 0; e < total; e++)
                    ir_agg_defaults(cs, vi, ti, e * by);
            }
        } else {
            ir_agg_defaults(cs, vi, ti, 0);
        }
    }
}

/* ---- 循环上下文栈（break/continue 目标 label） ---- */
#define IR_MAX_LOOP 32
static int loop_end_label[IR_MAX_LOOP];    /* break 目标 */
static int loop_cont_label[IR_MAX_LOOP];   /* continue 目标 */
static int loop_depth;

/* ---- is 模式匹配：当前循环条件表达式的值 vreg（while/do 设置，体内 is 匹配） ---- */
static int is_val_vreg = -1;
/* PB-71：首匹配即止标记（A 侧 __is_matched，PA-16 / P4）。每轮 while 迭代重置为 0，
 * 每条 is 子句先检查它、匹配后在进块前置 1 ⇒ 同一轮里只有首个匹配的子句执行。
 * -1 = 当前不在 while 体内（与 is_val_vreg 同步保存/恢复，支持嵌套）。 */
static int is_matched_vreg = -1;

/* PB-75(b)：上一次解析的 malloc(T) 的标量元素码。malloc 参数是类型而非表达式，
 * 其类型信息没有随返回 vreg 传出；声明/赋值处靠这个全局给它记 pt[]「指向窄标量」
 * ——修复 D1（`malloc(i32)` 配 8 字节写越界）的第一步。
 * -2=未触发；-1=参数非标量（void 数组 / 聚合名）；其余=标量码（0=i64 2=i8 …）。 */
static int last_malloc_sc = -2;

/* PB-77：stdio 库化（2026-11-12 ltree 裁「双形态库化」）。print/puts 是 stdio 标准库的
 * 成员而非编译器内置名——`use stdio` 登记导入；未导入就使用它们时给一次温和提示
 * （不阻断编译：既有大量用例与 PA 结转用例未写 use stdio，硬拒绝会造成用例集分叉）。 */
static int stdio_imported = 0;
static int stdio_warned = 0;

/* PB-27.7：is 模式比较的类型感知——按 is_val_vreg 类型选 int (IR_CMP_*) 或
 * double (IR_FCMP) 指令；double 常量转位模式存 IR_CONST imm。 */
/* `is` 模式的整数字面量，可带一个前缀符号（BNF §6 <pattern> 的带符号字面量）。
 * 成功返回 1 并消费；失败返回 0，调用点按上下文给专属诊断。
 * 失败时符号可能已被消费，与 A 后端 parse_is_stmt 同口径（一态一诊断）。 */
static int ir_is_int(CompilerState *cs, long long *out)
{
    TokenType t = cur_tok(cs);
    if (t == TOK_INT_CONST) {
        *out = (long long)cs->parser.lex->tok_val.i;
        next_tok(cs);
        return 1;
    }
    if (t == TOK_MINUS || t == TOK_PLUS) {
        next_tok(cs);
        if (cur_tok(cs) != TOK_INT_CONST) return 0;
        *out = (t == TOK_MINUS) ? -(long long)cs->parser.lex->tok_val.i
                                :  (long long)cs->parser.lex->tok_val.i;
        next_tok(cs);
        return 1;
    }
    return 0;
}

static int emit_is_cmp_with_const(int is_val_vreg, IrOp int_cmp_op, long long const_val)
{
    int k = ir_new_vreg(F);
    int r = ir_new_vreg(F);
    if (ir_is_double(is_val_vreg)) {
        double dv = (double)const_val;
        long long bits;
        memcpy(&bits, &dv, sizeof(bits));
        ir_emit(F, IR_CONST, k, -1, -1, bits);
        ir_set_double(k);
        int fc = (int_cmp_op == IR_CMP_EQ) ? 0 :    /* FCMP_EQ */
                 (int_cmp_op == IR_CMP_LT) ? 2 :    /* FCMP_LT */
                 (int_cmp_op == IR_CMP_LE) ? 3 :    /* FCMP_LE */
                 (int_cmp_op == IR_CMP_GT) ? 4 :    /* FCMP_GT */
                 /* GE */ 5;                        /* FCMP_GE */
        ir_emit(F, IR_FCMP, r, is_val_vreg, k, fc);
    } else {
        ir_emit(F, IR_CONST, k, -1, -1, const_val);
        ir_emit(F, int_cmp_op, r, is_val_vreg, k, 0);
    }
    return r;
}

/* ---- 局部变量表 ---- */
static void var_reset(void)
{
    vn_count = 0;
    if (vn_cap == 0) {
        vn_cap = 32;
        vt = nihao_malloc(g_cs, vn_cap * sizeof(int));
        pt = nihao_malloc(g_cs, vn_cap * sizeof(int));
        vn = nihao_malloc(g_cs, vn_cap * sizeof(char *));
        ve = nihao_malloc(g_cs, vn_cap * sizeof(int));
        vetyp = nihao_malloc(g_cs, vn_cap * sizeof(int));
        vty = nihao_malloc(g_cs, vn_cap * sizeof(int));
        vvis = nihao_malloc(g_cs, vn_cap * sizeof(int));
        vstate = nihao_malloc(g_cs, vn_cap * sizeof(int));
        vborrow_src = nihao_malloc(g_cs, vn_cap * sizeof(int));
        vptr = nihao_malloc(g_cs, vn_cap * sizeof(int));
        vtype = nihao_malloc(g_cs, vn_cap * sizeof(int));
        vdim = nihao_malloc(g_cs, vn_cap * sizeof(*vdim));
        vndim = nihao_malloc(g_cs, vn_cap * sizeof(int));
        vmode = nihao_malloc(g_cs, vn_cap * sizeof(int));
        vfrm = nihao_malloc(g_cs, vn_cap * sizeof(int));
        velemsz = nihao_malloc(g_cs, vn_cap * sizeof(int));
        vblk = nihao_malloc(g_cs, vn_cap * sizeof(int));
        vaparam = nihao_malloc(g_cs, vn_cap * sizeof(int));
        vlen_known = nihao_malloc(g_cs, vn_cap * sizeof(int));
    }
}

static int var_find(const char *name)
{
    for (int i = 0; i < vn_count; i++)
        if (strcmp(vn[i], name) == 0) return i;
    return -1;
}

/* 按名字查用户函数（PB-16：调用点查参数聚合签名）；无返回 NULL */
static IrFn *ir_find_fn(const char *name)
{
    for (int fi = 0; fi < P->fn_count; fi++) {
        if (P->fns[fi].name && strcmp(P->fns[fi].name, name) == 0) {
            return &P->fns[fi];
        }
    }
    return NULL;
}

/* elems: 数组元素数 / struct 成员数（0=标量）。分配 elems 个连续 8 字节 ALLOCA 槽，
 * vt[i] 指向第 0 槽 vreg，元素/成员 k 槽 vreg = vt[i] + k。
 * type_idx: 聚合类型索引（-1=标量/基本类型）。vis: 可见性/存储期（VIS_*）。 */
static int var_declare(const char *name, int elems, int type_idx, int vis,
                       int elem_bytes)
{
    int i = var_find(name);
    if (i >= 0) return i;
    if (vn_count >= vn_cap) {
        vn_cap *= 2;
        vt = nihao_realloc(g_cs, vt, vn_cap * sizeof(int));
        pt = nihao_realloc(g_cs, pt, vn_cap * sizeof(int));
        vn = nihao_realloc(g_cs, vn, vn_cap * sizeof(char *));
        ve = nihao_realloc(g_cs, ve, vn_cap * sizeof(int));
        vetyp = nihao_realloc(g_cs, vetyp, vn_cap * sizeof(int));
        vty = nihao_realloc(g_cs, vty, vn_cap * sizeof(int));
        vvis = nihao_realloc(g_cs, vvis, vn_cap * sizeof(int));
        vstate = nihao_realloc(g_cs, vstate, vn_cap * sizeof(int));
        vborrow_src = nihao_realloc(g_cs, vborrow_src, vn_cap * sizeof(int));
        vptr = nihao_realloc(g_cs, vptr, vn_cap * sizeof(int));
        vtype = nihao_realloc(g_cs, vtype, vn_cap * sizeof(int));
        vdim = nihao_realloc(g_cs, vdim, vn_cap * sizeof(*vdim));
        vndim = nihao_realloc(g_cs, vndim, vn_cap * sizeof(int));
        vmode = nihao_realloc(g_cs, vmode, vn_cap * sizeof(int));
        vfrm = nihao_realloc(g_cs, vfrm, vn_cap * sizeof(int));
        velemsz = nihao_realloc(g_cs, velemsz, vn_cap * sizeof(int));
        vblk = nihao_realloc(g_cs, vblk, vn_cap * sizeof(int));
        vaparam = nihao_realloc(g_cs, vaparam, vn_cap * sizeof(int));
        vlen_known = nihao_realloc(g_cs, vlen_known, vn_cap * sizeof(int));
    }
    vn[vn_count] = name;
    pt[vn_count] = -1;
    ve[vn_count] = elems > 0 ? elems : 0;
    vetyp[vn_count] = 0;
    vty[vn_count] = type_idx;
    vvis[vn_count] = vis;
    vtype[vn_count] = 0;
    vndim[vn_count] = 0;
    vmode[vn_count] = VM_SLOT;
    vfrm[vn_count] = 0;
    velemsz[vn_count] = 0;
    vblk[vn_count] = 0;
    vaparam[vn_count] = 0;
    vlen_known[vn_count] = (elems > 0) ? 1 : 0;
    for (int k = 0; k < IR_MAX_DIMS; k++) vdim[vn_count][k] = 0;
    int base = -1;
    if (elems > 0 && type_idx >= 0) {
        /* 聚合体（struct/union/enum）：整块占 elems 个连续 8 字节槽（mslots，嵌套递归）。
         * 只给基槽发 IR_AGG_ALLOCA——汇编后端与 ALLOCA 一样跳过，成员槽由 vreg 编号
         * base+k 自然落在相邻帧槽；ir-c 把整块落成一条 C 数组 int64_t name[elems]，
         * 成员 vreg 映射为 name[k]。这样 `&agg + 8*k` 与直接成员访问（ADDR + k）落到
         * 同一存储，修复 `r->y` 写进聚合块之外的缺陷（PB-76(b)）。 */
        for (int k = 0; k < elems; k++) {
            int vr = ir_new_vreg(F);
            if (k == 0) {
                base = vr;
                ir_emit(F, IR_AGG_ALLOCA, vr, -1, -1, 8LL * elems);
            }
        }
    } else if (elems > 0) {
        /* 基本类型数组：PB-82 起按元素字节步长紧凑分配——
         * 块槽数 = ceil(elems × es / 8)（es=8 时与旧行为相同，es=1 时 char[N] 占 ceil(N/8) 槽）。
         * 仍为每个块槽分配一个 vreg（汇编帧按 vreg 计数），只有基槽发 ALLOCA。
         * 元素寻址由 velemsz[] 经 ELEM_ADDR 的宽度参数完成（见 ir_arr_const_addr / ir_elem_stride）。 */
        int es = elem_bytes > 0 ? elem_bytes : 8;
        int slots = (int)(((long long)elems * es + 7) / 8);
        for (int k = 0; k < slots; k++) {
            int vr = ir_new_vreg(F);
            if (k == 0) {
                base = vr;
                ir_emit(F, IR_ALLOCA, vr, -1, -1, 8LL * slots);
            }
        }
        velemsz[vn_count] = es;
    } else {
        for (int k = 0; k < (elems > 0 ? elems : 1); k++) {
            int vr = ir_new_vreg(F);
            ir_emit(F, IR_ALLOCA, vr, -1, -1, 8);
            if (k == 0) base = vr;
        }
    }
    vt[vn_count] = base;
    /* PB 阶段3 C 项：把源码变量名写入 IR，供 ir_to_c 输出保留可读名
     * （数组/聚合仅基 vreg 命名，元素/成员 vreg 仍平铺 tN，与声明一致） */
    if (base >= 0 && base < F->vreg_count)
        F->vreg_name[base] = vn[vn_count];
    return vn_count++;
}

/* ---- 全局变量读写（跨函数共享，落到 IrProg 全局池）---- */

/* 全局变量地址：LD_ADDR(__gN) */
static int ir_global_addr(int gi)
{
    int vr = ir_new_vreg(F);
    IrIns *in = &F->ins[ir_emit(F, IR_LD_ADDR, vr, -1, -1, 0)];
    in->sym = P->glb_syms[ir_globals[gi].sym_idx];
    return vr;
}

/* 读全局变量：LD_ADDR → LOAD */
static int ir_global_read(int gi)
{
    int addr = ir_global_addr(gi);
    int vr = ir_new_vreg(F);
    ir_emit(F, IR_LOAD, vr, addr, -1, 0);
    if (ir_globals[gi].is_double) ir_set_double(vr);
    return vr;
}

/* 写全局变量：LD_ADDR → STORE */
static void ir_global_write(int gi, int val)
{
    int addr = ir_global_addr(gi);
    ir_emit(F, IR_STORE, -1, addr, val, 0);
}

/* 把一段已发射的指令接到函数末尾（for 的 step：源码在 body 前，执行在 body 后） */
static void ir_ins_move_to_end(IrFn *f, const IrIns *saved, int n)
{
    for (int k = 0; k < n; k++) {
        int idx = ir_emit(f, saved[k].op, saved[k].dst, saved[k].a, saved[k].b,
                          saved[k].imm);
        f->ins[idx].label = saved[k].label;
        f->ins[idx].fn = saved[k].fn;
        f->ins[idx].sym = saved[k].sym;
    }
}

/* 数组元素寻址宽度：槽模型数组一律 8 字节；仅动态字符串（vetyp 码 8，
 * char[] 池地址）按 1 字节。vetyp 其他码只影响值截断（ir_coerce），不参与寻址 */
static int ir_elem_width(int code)
{
    return code == 8 ? 1 : 8;
}

/* PB-82：数组元素字节步长——动态字符串（vetyp=8，池地址）按 1；
 * 声明登记的 velemsz（紧凑数组）；否则 8（历史槽数组）。 */
static int ir_elem_stride(int vi)
{
    if (vetyp[vi] == 8) return 1;
    if (velemsz[vi] > 0) return velemsz[vi];
    return 8;
}

/* PB-82：紧凑数组块占用的槽数（es=8 的普通槽数组 = 元素数） */
static int ir_arr_slots(int vi)
{
    int es = ir_elem_stride(vi);
    if (es == 8) return ve[vi] > 0 ? ve[vi] : 1;
    return (ve[vi] * es + 7) / 8;
}

/* PB-83：变量的块槽数（聚合体用 vblk；紧凑数组按 ve*es 算；标量 0） */
static int ir_block_slots(int vi)
{
    if (vblk[vi] > 0) return vblk[vi];
    if (ve[vi] > 0) return ir_arr_slots(vi);
    return 1;
}

/* 聚合体块首地址（低地址端） */
static int ir_agg_base(int vi)
{
    int a = ir_new_vreg(F);
    ir_emit(F, IR_ADDR_LO, a, vt[vi], -1, ir_block_slots(vi));
    return a;
}

/* 块首 + 字节偏移（向上） */
static int ir_ptr_byte_off(int base, int off)
{
    if (off == 0) return base;
    int c = ir_new_vreg(F);
    ir_emit(F, IR_CONST, c, -1, -1, off);
    int r = ir_new_vreg(F);
    ir_emit(F, IR_ADD, r, base, c, 0);
    return r;
}

/* PB-83：按字节复制块（8 字节块优先，尾字节逐个）。src/dst 都是地址值 */
static void ir_block_copy_bytes(int dst_val, int src_val, int bytes)
{
    int o = 0;
    while (o + 8 <= bytes) {
        int so = ir_ptr_byte_off(src_val, o);
        int t = ir_new_vreg(F);
        ir_emit(F, IR_LOAD, t, so, -1, 0);
        int dof = ir_ptr_byte_off(dst_val, o);
        ir_emit(F, IR_STORE, -1, dof, t, 0);
        o += 8;
    }
    for (; o < bytes; o++) {
        int so = ir_ptr_byte_off(src_val, o);
        int t = ir_new_vreg(F);
        ir_emit(F, IR_LOAD8, t, so, -1, 0);
        int dof = ir_ptr_byte_off(dst_val, o);
        ir_emit(F, IR_STORE8, -1, dof, t, 0);
    }
}

/* PB-83：把字符串字面量（含结尾 NUL）逐字节写入 addr（切片字符串赋值） */
static void ir_copy_literal(CompilerState *cs, int dst, const char *str)
{
    int n = (int)strlen(str);
    for (int i = 0; i <= n; i++) {
        int c = ir_new_vreg(F);
        ir_emit(F, IR_CONST, c, -1, -1, (unsigned char)(i < n ? str[i] : 0));
        int a = ir_ptr_byte_off(dst, i);
        ir_emit(F, IR_STORE8, -1, a, c, 0);
    }
    (void)cs;
}

/* PB-89：string / char[] 绑定字面量 → 复制到可写内存（malloc(len+1) + 逐字节含 NUL），
 * 使元素写 `d[1] = x` 成立（此前直接指向只读字符串池，musl 下 SIGSEGV）。
 * 字符串模型暂不追踪释放（与既有动态字符串一致，所有权/释放仍属待定项）。 */
static int ir_strlit_copy(CompilerState *cs, const char *lit)
{
    int n = (int)strlen(lit);
    int sz = ir_new_vreg(F);
    ir_emit(F, IR_CONST, sz, -1, -1, (long long)n + 1);
    ir_emit(F, IR_PARAM, -1, sz, -1, 0);
    int p = ir_new_vreg(F);
    IrIns *ci = &F->ins[ir_emit(F, IR_CALL, p, -1, -1, 1)];
    ci->sym = "malloc";
    ci->fn = -1;
    for (int i = 0; i <= n; i++) {
        int cv = ir_new_vreg(F);
        ir_emit(F, IR_CONST, cv, -1, -1, (unsigned char)(i < n ? lit[i] : 0));
        int a = ir_ptr_byte_off(p, i);
        ir_emit(F, IR_STORE8, -1, a, cv, 0);
    }
    (void)cs;
    return p;
}

/* 聚合体成员地址：普通变量按字节布局；聚合参数按参数槽（base+fi） */
static int ir_member_addr(CompilerState *cs, int vi, int ti, int fi, int off)
{
    (void)cs; (void)ti;
    if (vaparam[vi]) {
        int a = ir_new_vreg(F);
        ir_emit(F, IR_ADDR, a, vt[vi], -1, fi);
        return a;
    }
    return ir_ptr_byte_off(ir_agg_base(vi), off);
}

/* PB-83：成员的类型码（动态字符串/嵌套聚合成员按“无标量类型”-1 → 整 8 字节） */
static int ir_member_vcode(int ti, int fi)
{
    IrAggType *a = &agg_types[ti];
    if (a->misarr[fi] && a->marr_len[fi] <= 0) return -1;   /* char[] 成员 = 指针 */
    if (a->mtype[fi] >= 0) return -1;
    return a->mscode[fi];
}

/* PB-83：成员写宽（动态字符串成员按 8 字节指针写） */
static int ir_member_swidth(int ti, int fi)
{
    int c = ir_member_vcode(ti, fi);
    if (c < 0) return 0;
    return ir_store_width(c);
}

/* 块首地址（字节块最低地址）：汇编取最低槽、C 取 &name[0]。
 * 紧凑块内一律 +k 向上寻址，与 C 字符串/数组语义一致（方向在后端内消化）。 */
static int ir_arr_lo(int vi)
{
    int a = ir_new_vreg(F);
    ir_emit(F, IR_ADDR_LO, a, vt[vi], -1, ir_arr_slots(vi));
    return a;
}

/* 常量下标的数组元素地址。步长 8 走历史槽寻址；紧凑数组走「块首 + idx*es」。 */
static int ir_arr_const_addr(int vi, long long idx)
{
    int es = ir_elem_stride(vi);
    int a;
    if (es == 8) {
        a = ir_new_vreg(F);
        ir_emit(F, IR_ADDR, a, vt[vi], -1, idx);
        return a;
    }
    int b = ir_arr_lo(vi);
    int c = ir_new_vreg(F);
    ir_emit(F, IR_CONST, c, -1, -1, idx * es);
    a = ir_new_vreg(F);
    ir_emit(F, IR_ADD, a, b, c, 0);
    return a;
}

/* 1 字节元素窄读：有符号（i8/char）→ IR_LOAD 负宽符号扩展；
 * 无符号/未知 → LOAD8 零扩展。此前一律 LOAD8，i8 元素读到 255 而非 -1。 */
static void ir_load1(int dst, int addr, int vcode)
{
    int64_t w = ir_load_width_imm(vcode);
    if (w < 0) ir_emit(F, IR_LOAD, dst, addr, -1, w);
    else ir_emit(F, IR_LOAD8, dst, addr, -1, 0);
}

/* 按元素步长读/写（1/2/4/8）。1 字节按元素类型定符号扩展；
 * 2/4 用窄存取（宽度与符号按元素类型码）；8 走完整槽。 */
static void ir_arr_load(int dst, int addr, int es, int vcode)
{
    if (es == 1) {
        ir_load1(dst, addr, vcode);
        return;
    }
    if (es == 2 || es == 4) {
        int64_t w = ir_load_width_imm(vcode);
        if (w == 0) w = es;
        ir_emit(F, IR_LOAD, dst, addr, -1, w);
        return;
    }
    ir_emit(F, IR_LOAD, dst, addr, -1, 0);
}

static void ir_arr_store(int addr, int v, int es)
{
    if (es == 1) {
        ir_emit(F, IR_STORE8, -1, addr, v, 0);
        return;
    }
    if (es == 2 || es == 4) {
        ir_emit(F, IR_STORE, -1, addr, v, es);
        return;
    }
    ir_emit(F, IR_STORE, -1, addr, v, 0);
}

/* 数组元素地址：&arr[0] + idx*width（width 编码进 ELEM_ADDR imm；imm=0 兼容=8）
 * is_ptr：动态字符串（vetyp=8 池地址）——槽里存的是指针值，取槽值（MOV）后
 * **向前偏移**（池内存向上布局；ELEM_ADDR 是后端栈布局方向，不适用） */
static int ir_elem_addr(CompilerState *cs, int vi, int base_vreg, int idx_vreg,
                        int width, int is_ptr)
{
    if (!is_ptr && width != 8 && vi >= 0) {
        /* PB-82：紧凑数组——块首 + idx*es（向上，无方向依赖） */
        int base = ir_arr_lo(vi);
        int wv = ir_new_vreg(F);
        ir_emit(F, IR_CONST, wv, -1, -1, width);
        int off = ir_new_vreg(F);
        ir_emit(F, IR_MUL, off, idx_vreg, wv, 0);
        int addr = ir_new_vreg(F);
        ir_emit(F, IR_ADD, addr, base, off, 0);
        return addr;
    }
    if (is_ptr) {
        int base = ir_new_vreg(F);
        ir_emit(F, IR_MOV, base, base_vreg, -1, 0);
        int addr = ir_new_vreg(F);
        if (width == 1) {
            ir_emit(F, IR_ADD, addr, base, idx_vreg, 0);
        } else {
            int wv = ir_new_vreg(F);
            ir_emit(F, IR_CONST, wv, -1, -1, width);
            int off = ir_new_vreg(F);
            ir_emit(F, IR_MUL, off, idx_vreg, wv, 0);
            ir_emit(F, IR_ADD, addr, base, off, 0);
        }
        return addr;
    }
    int base = ir_new_vreg(F);
    ir_emit(F, IR_ADDR, base, base_vreg, -1, 0);
    int addr = ir_new_vreg(F);
    ir_emit(F, IR_ELEM_ADDR, addr, base, idx_vreg, width == 8 ? 0 : width);
    return addr;
}

/* PB-83：从属查询内置实现（cur_tok = 内置名，随后是 '('）
 *   bits_mode=0：structof/holdof/unionof(Type, m, ptr) → ptr - 成员字节偏移
 *   bits_mode=1：bitoffsetof(Type, m) → 成员位偏移 */
static int ir_ownerof(CompilerState *cs, int bits_mode)
{
    next_tok(cs);           /* 内置名 */
    expect(cs, TOK_LPAREN);
    if (cur_tok(cs) != TOK_IDENTIFIER) {
        nihao_error(cs, "ir: %s: expected an aggregate type name",
                    bits_mode ? "bitoffsetof" : "structof");
        return ir_new_vreg(F);
    }
    int ti = agg_type_find(cs->parser.lex->tok_str);
    if (ti < 0) {
        nihao_error(cs, "ir: unknown aggregate type '%s'", cs->parser.lex->tok_str);
        ti = 0;
    }
    next_tok(cs);
    expect(cs, TOK_COMMA);
    if (cur_tok(cs) != TOK_IDENTIFIER) {
        nihao_error(cs, "ir: expected a member name");
        return ir_new_vreg(F);
    }
    int fi = agg_member_find(ti, cs->parser.lex->tok_str);
    if (fi < 0) {
        nihao_error(cs, "ir: no member '%s' in type '%s'",
                    cs->parser.lex->tok_str, agg_types[ti].name);
        fi = 0;
    }
    next_tok(cs);
    if (bits_mode) {
        expect(cs, TOK_RPAREN);
        long long bo = (agg_types[ti].mbit[fi] >= 0)
                       ? agg_types[ti].mbit[fi]
                       : (long long)agg_types[ti].moff[fi] * 8;
        int vr = ir_new_vreg(F);
        ir_emit(F, IR_CONST, vr, -1, -1, bo);
        return vr;
    }
    expect(cs, TOK_COMMA);
    int p = ir_expr(cs);
    expect(cs, TOK_RPAREN);
    int off = (agg_types[ti].kind == 1) ? 0 : agg_types[ti].moff[fi];
    if (off == 0) return p;
    int c = ir_new_vreg(F);
    ir_emit(F, IR_CONST, c, -1, -1, off);
    int r = ir_new_vreg(F);
    ir_emit(F, IR_SUB, r, p, c, 0);
    return r;
}

/* 数组成员的元素寻址：p.name[0] → 成员首槽地址 + idx*元素宽。
 * IR 槽模型下数组成员只占 1 个 8 字节槽（真实布局待裁定），越出首槽的元素
 * 没有地址可寻 → 常量下标越界时给专属诊断。返回元素地址 vreg（失败 -1）。
 * 调用方负责在调用前让 cur_tok 停在 '[' 上；本函数消费 '[ idx ]'。 */
static int ir_member_elem_addr(CompilerState *cs, int vi, int ti, int fi, int off,
                                const char *mname, int *esize_out)
{
    int elen = agg_types[ti].marr_len[fi];
    int esize = agg_types[ti].marr_esize[fi];
    if (elen <= 0) {
        nihao_error(cs, "ir: member '%s' is not an array", mname);
        return -1;
    }
    next_tok(cs);       /* [ */
    int idx = ir_expr(cs);
    expect(cs, TOK_RBRACKET);
    long long iv;
    if (vreg_const(F, idx, &iv)) {
        if (iv < 0 || iv >= elen) {
            nihao_error(cs, "ir: array index %lld is out of range for member '%s' "
                            "(%d elements)", iv, mname, elen);
        }
    }
    int base = ir_ptr_byte_off(ir_agg_base(vi), off);
    int iv2 = ir_new_vreg(F);
    ir_emit(F, IR_MOV, iv2, idx, -1, 0);
    int wv = ir_new_vreg(F);
    ir_emit(F, IR_CONST, wv, -1, -1, esize);
    int boff = ir_new_vreg(F);
    ir_emit(F, IR_MUL, boff, iv2, wv, 0);
    int addr = ir_new_vreg(F);
    ir_emit(F, IR_ADD, addr, base, boff, 0);
    if (esize_out) *esize_out = esize;
    return addr;
}

/* ---- 表达式：返回持有结果的 vreg ---- */

/* 多维数组的扁平下标：Σ idx[k] * stride[k]，stride[k] = 第 k 维之后各维长度之积。
 * 槽模型下多维数组按行主序摊平成连续槽（PB-67），故访问只按扁平下标算址。
 * nidx < 维数时结果是「子行首地址」（与 C/A 同口径：m[1] 是第 1 行的指针）。 */
static int ir_flat_index(CompilerState *cs, int vi, const int *idx, int nidx)
{
    int nd = vndim[vi];
    if (nd <= 0) {
        nihao_error(cs, "ir: '%s' is not a multi-dimensional array", vn[vi]);
        return ir_new_vreg(F);
    }
    if (nidx > nd) {
        nihao_error(cs, "ir: '%s' has %d dimension(s) but %d index(es) were given",
                    vn[vi], nd, nidx);
        return ir_new_vreg(F);
    }
    int flat = ir_new_vreg(F);
    ir_emit(F, IR_CONST, flat, -1, -1, 0);
    for (int k = 0; k < nidx; k++) {
        int stride = 1;
        for (int j = k + 1; j < nd; j++) stride *= vdim[vi][j];
        int sv = ir_new_vreg(F);
        ir_emit(F, IR_CONST, sv, -1, -1, stride);
        int prod = ir_new_vreg(F);
        ir_emit(F, IR_MUL, prod, idx[k], sv, 0);
        int sum = ir_new_vreg(F);
        ir_emit(F, IR_ADD, sum, flat, prod, 0);
        flat = sum;
    }
    return flat;
}

/* 聚合体数组元素的成员读：a[i].x / e[i][j].y（PB-80）。
 * 元素槽起点 = flat * mslots；成员再按 moff 加。地址用
 * ELEM_ADDR(ADDR(成员偏移), flat*mslots, 8)——方向由后端处理（C 向上 / 汇编向下）。 */
static int ir_agg_elem_read(CompilerState *cs, int vi)
{
    int ti = vty[vi];
    int idxs[IR_MAX_DIMS];
    int nidx = 0;
    while (cur_tok(cs) == TOK_LBRACKET && nidx < IR_MAX_DIMS) {
        next_tok(cs);       /* [ */
        idxs[nidx++] = ir_expr(cs);
        expect(cs, TOK_RBRACKET);
    }
    int flat = ir_flat_index(cs, vi, idxs, nidx);
    if (cur_tok(cs) != TOK_DOT) {
        nihao_error(cs, "ir: aggregate array element needs a member access ('.member')");
        return ir_new_vreg(F);
    }
    int off = 0;
    int last_fi = -1;
    int cur_ti = ti;
    for (;;) {
        next_tok(cs);       /* . */
        if (cur_tok(cs) != TOK_IDENTIFIER) {
            nihao_error(cs, "ir: expected member name after '.'");
            return ir_new_vreg(F);
        }
        int fi = agg_member_find(cur_ti, cs->parser.lex->tok_str);
        if (fi < 0) {
            nihao_error(cs, "ir: no member '%s' in type '%s'",
                        cs->parser.lex->tok_str, agg_types[cur_ti].name);
            return ir_new_vreg(F);
        }
        last_fi = fi;
        off += (agg_types[cur_ti].kind == 1) ? 0 : agg_types[cur_ti].moff[fi];
        int mt = agg_types[cur_ti].mtype[fi];
        next_tok(cs);       /* field */
        if (mt >= 0 && cur_tok(cs) == TOK_DOT) {
            cur_ti = mt;
            continue;
        }
        break;
    }
    int aggb = ir_agg_base(vi);
    int sc = ir_new_vreg(F);
    ir_emit(F, IR_CONST, sc, -1, -1, (long long)agg_types[ti].bytes);
    int scaled = ir_new_vreg(F);
    ir_emit(F, IR_MUL, scaled, flat, sc, 0);
    int base = ir_new_vreg(F);
    ir_emit(F, IR_ADD, base, aggb, scaled, 0);
    int addr = ir_ptr_byte_off(base, off);
    int vr = ir_new_vreg(F);
    int mcode = (agg_types[cur_ti].mbits[last_fi] > 0) ? 0
              : ir_member_vcode(cur_ti, last_fi);
    /* PB-84：位域按存储单元宽读（u8 单位=1 字节） */
                ir_emit(F, IR_LOAD, vr, addr, -1,
                        agg_types[ti].mbits[last_fi] > 0
                            ? (ir_bf_unit(ti, last_fi) == 8 ? 0 : ir_bf_unit(ti, last_fi))
                            : ir_load_width_imm(mcode));
    if (mcode == 1 || mcode == 8) ir_set_double(vr);
    return vr;
}

/* 数组初始化列表（PB-67 支持多维）：按维度递归填槽。
 * level = 当前维度下标，offset = 该维首元素的扁平下标。
 * 叶层逐个 STORE；复合层每个子列表对应一个元素。 */
static void ir_array_init_list(CompilerState *cs, int vi, int level, int offset)
{
    int nd = vndim[vi];
    int cnt = vdim[vi][level];
    if (level == nd - 1) {
        int k = 0;
        if (cur_tok(cs) != TOK_RBRACE) {
            for (;;) {
                int v = ir_expr(cs);
                v = ir_coerce(v, vetyp[vi] == 8 ? 2 : vetyp[vi]);   /* PB-1：元素类型协调（vetyp 8=char 元素，与 vtype 8=f32 编码冲突→按 i8 截断） */
                int addr = ir_arr_const_addr(vi, offset + k);
                ir_arr_store(addr, v, ir_elem_stride(vi));
                k++;
                if (cur_tok(cs) != TOK_COMMA) break;
                next_tok(cs);
            }
        }
        return;
    }
    int stride = 1;
    for (int j = level + 1; j < nd; j++) stride *= vdim[vi][j];
    int g = 0;
    if (cur_tok(cs) != TOK_RBRACE) {
        for (;;) {
            expect(cs, TOK_LBRACE);
            ir_array_init_list(cs, vi, level + 1, offset + g * stride);
            expect(cs, TOK_RBRACE);
            g++;
            if (cur_tok(cs) != TOK_COMMA) break;
            next_tok(cs);
        }
    }
}

/* ---- 后缀链求值（PB-68 指针类型后缀档）----
 *
 * 语言层的后缀不止一种：下标 arr[i]、切片 arr[a..b]、空下标 p[]（解一层引用）、
 * 解引用 p.() / p.(T)、数组型后缀 p.(T[N])（把通用指针定成「指向 T[N]」）。
 * 旧路径一次只处理一种后缀（`[` 之后直接返回），多步链与新形态都落不到。
 * 这里把整条链一次走完，链状态描述「当前对象」：
 *   val      当前值 vreg；
 *   kind     1 = 当前值是地址（可再解引用 / 可下标）；0 = 普通值；
 *   elem     元素/指向的标量类型码（-1 = 未知）；
 *   elem_ptr 1 = 元素本身是通用指针（解出来的值仍是地址）；
 *   addr     末步的可写地址（-1 = 无；赋值目标用）；
 *   width    元素寻址宽度（字节）：槽模型 8，字节寻址（动态字符串）1；
 *   is_slice 末步是切片（此时 val 就是切片首地址）。 */
typedef struct {
    int val;
    int kind;
    int elem;
    int elem_ptr;
    int addr;
    int width;
    int is_slice;
    int agg_ti;     /* PB-83：`.(AggType)` 定下的「指向聚合体」类型索引（-1=无） */
    int slice_lo;   /* PB-84：切片左界（常量；-1=未知） */
    int slice_hi;   /* PB-84：切片右界（常量；-1=未知） */
} IrChain;

/* 后缀链分组扫描：数出后缀「组」数，并标记新形态。
 * 组 = 连续下标组 | 一次 .() | 一次成员访问（. / ->，成员后的 [..] 算同一组）。
 * 旧路径只处理单组后缀；多组或新形态走链式求值。 */
static void ir_scan_postfix(CompilerState *cs, int start, int *groups, int *has_new)
{
    *groups = 0;
    *has_new = 0;
    int pos = start;
    for (;;) {
        TokenType t = ir_peek_ahead(cs, pos);
        if (t == TOK_LBRACKET) {
            if (ir_peek_ahead(cs, pos + 1) == TOK_RBRACKET) *has_new = 1;  /* 空下标 p[] */
            int depth = 0;
            do {
                TokenType u = ir_peek_ahead(cs, pos);
                if (u == TOK_EOF) break;
                if (u == TOK_LBRACKET) depth++;
                else if (u == TOK_RBRACKET) depth--;
                pos++;
            } while (depth > 0);
            (*groups)++;
            continue;
        }
        if (t == TOK_DOT_PAREN) {
            int p = pos + 1;
            if (ir_peek_ahead(cs, p) != TOK_RPAREN &&
                ir_peek_ahead(cs, p) != TOK_EOF) {
                p++;                                    /* 类型名 */
                if (ir_peek_ahead(cs, p) == TOK_LBRACKET) *has_new = 1;  /* 数组型后缀 .(T[N]) */
            }
            int depth = 1;
            pos = p;
            while (depth > 0) {
                TokenType v = ir_peek_ahead(cs, pos);
                if (v == TOK_EOF) break;
                if (v == TOK_LPAREN) depth++;
                else if (v == TOK_RPAREN) depth--;
                pos++;
            }
            (*groups)++;
            continue;
        }
        if (t == TOK_DOT || t == TOK_ARROW) {
            pos++;                                    /* . / -> */
            if (ir_peek_ahead(cs, pos) == TOK_IDENTIFIER) pos++;   /* 成员名 */
            if (ir_peek_ahead(cs, pos) == TOK_LBRACKET) {  /* 成员后的 [..] 同一组 */
                int depth = 0;
                do {
                    TokenType u = ir_peek_ahead(cs, pos);
                    if (u == TOK_EOF) break;
                    if (u == TOK_LBRACKET) depth++;
                    else if (u == TOK_RBRACKET) depth--;
                    pos++;
                } while (depth > 0);
            }
            (*groups)++;
            continue;
        }
        break;
    }
}

/* 这条后缀链是否要走链式求值：
 *   - 根变量是「指针值数组」（malloc 退化形态）→ 下标寻址语义与帧数组不同；
 *   - 链含新形态（空下标 p[] / 数组型后缀 .(T[N])）；
 *   - 链有两组以上后缀（多步链）。 */
/* PB-72：通用 void 指针的裸下标 / 切片——元素宽度静态未知，与 A 后端同判据拒绝
 * （A 侧判 psym->type->kind == TYPE_VOID）。两条例外：
 *   ① 空下标 `p[]` 是「省略类型解引用」（§5.1.1），合法；
 *   ② PB-68 的 VM_PTRARR（`void[N] = malloc(void[N])` 退化形态）ve[] 为 0 但可下标。
 * 调用时 cur_tok 必须是 `[`。 */
static void ir_check_void_subscript(CompilerState *cs, int vi)
{
    if (vptr[vi] && ve[vi] == 0 && vmode[vi] != VM_PTRARR &&
        ir_peek_ahead(cs, 1) != TOK_RBRACKET) {
        nihao_error(cs, "ir: cannot subscript the generic 'void' pointer '%s'; "
                        "write %s.(T)[i] to fix the element type first",
                    vn[vi], vn[vi]);
    }
}

static int ir_chain_needs_new_off(CompilerState *cs, int vi, int off)
{
    if (vmode[vi] == VM_PTRARR) return 1;
    int groups = 0, has_new = 0;
    ir_scan_postfix(cs, off, &groups, &has_new);
    return has_new || groups >= 2;
}

static int ir_chain_needs_new(CompilerState *cs, int vi)
{
    return ir_chain_needs_new_off(cs, vi, 0);
}

/* 链求值：从变量 vi 出发逐个消费后缀步。返回 1 成功，0 失败（已给诊断）。 */
/* 指针的成员偏移寻址（PB-81，待裁 33 选 (c) provenance）。
 * off = 槽偏移。帧来源指针（vfrm[vi]=1）：帧槽在汇编档向下、C 档向上（C 数组），
 * 用方向感知的 IR_ELEM_ADDR（idx=off、宽 8）；堆/池来源两侧都向上，仍用字节 ADD。 */
static int ir_ptr_slot_off(int vi, int base, int off)
{
    /* PB-83：聚合体/数组统一从块首（低地址）向上按字节排布，指针成员偏移
     * 一律“指针值 + 字节偏移”——不再依赖帧生长方向/来源（vfrm 仅留作登记）。 */
    (void)vi;
    return ir_ptr_byte_off(base, off);
}

static int ir_chain(CompilerState *cs, int vi, IrChain *ch)
{
    ch->val = vt[vi];
    ch->kind = 0;
    ch->elem = -1;
    ch->elem_ptr = 0;
    ch->addr = -1;
    ch->width = 8;
    ch->is_slice = 0;
    ch->agg_ti = -1;
    ch->slice_lo = -1;
    ch->slice_hi = -1;

    int base_vreg = vt[vi];   /* 当前寻址基址 */
    int ptr_base = 0;         /* 1 = 基址取「指针值」，0 = 取帧槽地址 */
    int root_nd = vndim[vi];  /* 根变量的维数（多维摊平用） */
    int step = 0;             /* 已走的后缀步数 */

    if (vmode[vi] == VM_PTRARR) {
        ptr_base = 1;         /* 槽里存的是地址 */
        ch->kind = 1;
        ch->elem = 0;
        ch->elem_ptr = 1;     /* 元素是通用指针 */
    } else if (vetyp[vi] == 8) {
        ptr_base = 1;         /* 动态字符串：槽里是池地址（字节寻址） */
        ch->kind = 1;
        ch->elem = 8;
        ch->width = 1;
    } else if (ve[vi] > 0 || root_nd >= 1) {
        ch->elem = vetyp[vi];  /* 帧数组：元素在帧槽里 */
        ch->width = ir_elem_stride(vi);
        ch->elem_ptr = vptr[vi];   /* void 元素数组：元素本身是通用指针 */
        if (ch->width != 8) {
            /* PB-82：紧凑帧数组从块首（最低地址）向上寻址 */
            base_vreg = ir_arr_lo(vi);
            ptr_base = 1;
        }
    } else if (vptr[vi]) {
        ptr_base = 1;         /* 普通通用指针变量 */
        ch->kind = 1;
        ch->elem = PT_IS_SCALAR(pt[vi]) ? PT_SCALAR_CODE(pt[vi]) : -1;
    } else {
        ch->elem = vtype[vi];  /* 标量槽 */
    }

    for (;;) {
        TokenType t = cur_tok(cs);
        if (t == TOK_LBRACKET) {
            next_tok(cs);
            if (cur_tok(cs) == TOK_RBRACKET) {
                /* 空下标 p[]：解一层引用（≡ p.()） */
                next_tok(cs);
                if (ch->kind != 1) {
                    nihao_error(cs, "ir: '[]' needs a pointer to dereference");
                    return 0;
                }
                int tgt = ch->val;
                int v = ir_new_vreg(F);
                ir_emit(F, IR_LOAD, v, tgt, -1, 0);
                ch->addr = tgt;
                ch->val = v;
                ch->kind = 1;
                ch->elem = -1;
                ch->elem_ptr = 0;
                ch->is_slice = 0;
                base_vreg = v;
                ptr_base = 1;
                step++;
                continue;
            }
            if (cur_tok(cs) == TOK_RANGE) {
                /* arr[..hi]：省略起点的切片 → 基址 */
                next_tok(cs);
                int hi_v = ir_expr(cs);
                long long hv;
                ch->slice_lo = 0;
                ch->slice_hi = vreg_const(F, hi_v, &hv) ? (int)hv : -1;
                expect(cs, TOK_RBRACKET);
                int addr = ir_elem_addr(cs, vi, base_vreg, 0, ch->width, ptr_base);
                ch->addr = addr;
                ch->val = addr;
                ch->kind = 1;
                ch->is_slice = 1;
                base_vreg = addr;
                ptr_base = 1;
                step++;
                continue;
            }
            int idx = ir_expr(cs);
            if (cur_tok(cs) == TOK_RANGE) {
                /* 切片 arr[lo..hi] → 元素地址（值即地址） */
                next_tok(cs);
                int hi_v = ir_expr(cs);
                long long lov, hiv;
                ch->slice_lo = vreg_const(F, idx, &lov) ? (int)lov : -1;
                ch->slice_hi = vreg_const(F, hi_v, &hiv) ? (int)hiv : -1;
                expect(cs, TOK_RBRACKET);
                int addr = ir_elem_addr(cs, vi, base_vreg, idx, ch->width, ptr_base);
                ch->addr = addr;
                ch->val = addr;
                ch->kind = 1;
                ch->is_slice = 1;
                base_vreg = addr;
                ptr_base = 1;
                step++;
                continue;
            }
            expect(cs, TOK_RBRACKET);
            /* 多维：首个下标步收连续下标并按行主序摊平（与旧路径同口径） */
            int idxs[IR_MAX_DIMS];
            int nidx = 1;
            idxs[0] = idx;
            if (step == 0) {
                while (cur_tok(cs) == TOK_LBRACKET && nidx < IR_MAX_DIMS) {
                    next_tok(cs);
                    int j = ir_expr(cs);
                    expect(cs, TOK_RBRACKET);
                    idxs[nidx++] = j;
                }
            }
            int addr;
            int partial = 0;
            if (nidx > 1 || root_nd >= 2) {
                int flat = ir_flat_index(cs, vi, idxs, nidx);
                addr = ir_elem_addr(cs, vi, base_vreg, flat, ch->width, ptr_base);
                partial = (nidx < root_nd);
            } else {
                addr = ir_elem_addr(cs, vi, base_vreg, idx, ch->width, ptr_base);
            }
            if (partial) {
                /* 子行首地址（m[1] 是第 1 行的指针） */
                ch->addr = addr;
                ch->val = addr;
                ch->kind = 1;
                ch->is_slice = 0;
                base_vreg = addr;
                ptr_base = 1;
                step++;
                continue;
            }
            int v = ir_new_vreg(F);
            if (ch->width == 1) ir_load1(v, addr, ch->elem);
            else ir_emit(F, IR_LOAD, v, addr, -1, 0);
            if (ch->elem == 1 || ch->elem == 8) ir_set_double(v);
            else if (ch->elem > 0) v = ir_coerce(v, ch->elem);
            ch->addr = addr;
            ch->val = v;
            ch->kind = ch->elem_ptr ? 1 : 0;
            /* PB-82：保留下标元素的类型码——链赋值按它取存储宽（char[9] 只写 1 字节）。
             * 指针元素（elem_ptr）的值是指针，类型码按整槽 0 处理。 */
            ch->elem = ch->elem_ptr ? 0 : ch->elem;
            ch->elem_ptr = 0;
            ch->is_slice = 0;
            base_vreg = v;
            ptr_base = ch->kind;
            step++;
            continue;
        }
        if (t == TOK_DOT_PAREN) {
            next_tok(cs);
            int tcode = -1;
            TokenType ttype = TOK_EOF;
            int arr_len = -1;
            if (cur_tok(cs) != TOK_RPAREN && cur_tok(cs) == TOK_IDENTIFIER) {
                int ati = agg_type_find(cs->parser.lex->tok_str);
                if (ati >= 0) {
                    next_tok(cs);
                    expect(cs, TOK_RPAREN);
                    if (ch->kind != 1) {
                        nihao_error(cs, "ir: '.()' with an aggregate type needs a pointer value");
                        return 0;
                    }
                    ch->agg_ti = ati;
                    ch->elem = -1;
                    ch->elem_ptr = 0;
                    ch->addr = -1;
                    ch->is_slice = 0;
                    ch->width = 8;
                    base_vreg = ch->val;
                    ptr_base = 1;
                    step++;
                    continue;
                }
            }
            if (cur_tok(cs) != TOK_RPAREN) {
                ttype = cur_tok(cs);
                if (ttype == TOK_VOID && ir_peek_ahead(cs, 1) == TOK_RPAREN) {
                    /* 码 0 碰撞修复：裸 `.(void)` 无意义；`.(void[N])` 是合法数组后缀 */
                    nihao_error(cs, "ir: '.()' dereference: 'void' has no size — use a scalar type");
                    return 0;
                }
                tcode = ir_scalar_code_of_token(ttype);
                if (tcode < 0) {
                    nihao_error(cs, "ir: '.()' dereference takes a scalar type or nothing, "
                                    "not '%s'", cs->parser.lex->tok_str);
                    return 0;
                }
                next_tok(cs);
                if (cur_tok(cs) == TOK_LBRACKET) {
                    /* 数组型后缀 .(T[N])：把通用指针定成「指向 T[N]」——
                     * 地址不变，元素类型改成 T（后续下标按 T 寻址）。 */
                    next_tok(cs);
                    arr_len = 0;
                    if (cur_tok(cs) == TOK_INT_CONST) {
                        arr_len = (int)cs->parser.lex->tok_val.i;
                        next_tok(cs);
                    }
                    expect(cs, TOK_RBRACKET);
                }
            }
            expect(cs, TOK_RPAREN);
            if (arr_len >= 0) {
                if (ch->kind != 1) {
                    nihao_error(cs, "ir: '.()' with an array type needs a pointer value");
                    return 0;
                }
                ch->elem = tcode;
                ch->elem_ptr = (ttype == TOK_VOID);   /* void 元素 = 通用指针 */
                ch->kind = 1;
                ch->addr = -1;
                ch->is_slice = 0;
                /* PB-82：元素宽按 T 的真实字节宽（char[9] → 1，i32[3] → 4） */
                ch->width = (int)type_size_token(ttype);
                if (!(ch->width == 1 || ch->width == 2 || ch->width == 4 || ch->width == 8))
                    ch->width = 8;
                base_vreg = ch->val;
                ptr_base = 1;
                step++;
                continue;
            }
            /* 普通解引用：读指针所指处的值 */
            if (ch->kind != 1) {
                nihao_error(cs, "ir: '.()' needs a pointer to dereference");
                return 0;
            }
            int tgt = ch->val;
            int v = ir_new_vreg(F);
            /* PB-82：指针元素类型已知时按元素宽读（切片变量 sl.() 等），否则整槽读 */
            {
                int wcode = (tcode >= 0) ? tcode : ch->elem;
                ir_emit(F, IR_LOAD, v, tgt, -1,
                        wcode >= 0 ? ir_load_width_imm(wcode) : 0);
            }
            if (tcode >= 0) {
                if (tcode == 1 || tcode == 8) ir_set_double(v);
                else if (tcode != 0) v = ir_coerce(v, tcode);
            } else if (ch->elem == 1 || ch->elem == 8) {
                ir_set_double(v);
            }
            ch->addr = tgt;
            ch->val = v;
            ch->kind = 0;
            ch->elem_ptr = 0;
            ch->elem = (tcode >= 0) ? tcode : ch->elem;
            ch->is_slice = 0;
            base_vreg = v;
            ptr_base = 0;
            step++;
            continue;
        }
        if (t == TOK_ARROW) {
            /* 指针成员访问 p->field：链上只在首步支持（成员类型取 pt[]） */
            if (step != 0 || pt[vi] < 0) {
                nihao_error(cs, "ir: '->' member access is only supported on a "
                                "pointer variable");
                return 0;
            }
            next_tok(cs);
            const char *fname = cs->parser.lex->tok_str;
            next_tok(cs);
            int ti = pt[vi];
            int off = 0;
            int last_fi = -1;
            for (;;) {
                int fi = agg_member_find(ti, fname);
                if (fi < 0) {
                    nihao_error(cs, "ir: no member '%s' in type '%s'", fname,
                                agg_types[ti].name);
                    return 0;
                }
                last_fi = fi;
                off += (agg_types[ti].kind == 1) ? 0 : agg_types[ti].moff[fi];
                int mt = agg_types[ti].mtype[fi];
                if (mt >= 0 && (cur_tok(cs) == TOK_DOT || cur_tok(cs) == TOK_ARROW)) {
                    ti = mt;
                    next_tok(cs);
                    fname = cs->parser.lex->tok_str;
                    next_tok(cs);
                    continue;
                }
                break;
            }
            int base = ir_new_vreg(F);
            ir_emit(F, IR_MOV, base, ch->val, -1, 0);
            int addr = ir_ptr_slot_off(vi, base, off);   /* PB-81：按指针来源选方向 */
            int v = ir_new_vreg(F);
            /* PB-76(b)：标量成员按成员类型窄读（符号扩展）；位域保持 8 字节 + AND */
            {
                int mcode = (agg_types[ti].mbits[last_fi] > 0) ? 0
                          : ir_member_vcode(ti, last_fi);
                /* PB-84：位域按存储单元宽读（u8 单位=1 字节） */
                ir_emit(F, IR_LOAD, v, addr, -1,
                        agg_types[ti].mbits[last_fi] > 0
                            ? (ir_bf_unit(ti, last_fi) == 8 ? 0 : ir_bf_unit(ti, last_fi))
                            : ir_load_width_imm(mcode));
                if (mcode == 1 || mcode == 8) ir_set_double(v);   /* PB-79：浮点成员读值标记 */
            }
            if (agg_types[ti].mbits[last_fi] > 0) {
                int sh = ir_bf_shift(ti, last_fi);
                if (sh > 0) {
                    int sc = ir_new_vreg(F);
                    ir_emit(F, IR_CONST, sc, -1, -1, sh);
                    int r1 = ir_new_vreg(F);
                    ir_emit(F, IR_SHR, r1, v, sc, 0);
                    v = r1;
                }
                int mv = ir_bitmask(agg_types[ti].mbits[last_fi]);
                int r2 = ir_new_vreg(F);
                ir_emit(F, IR_AND, r2, v, mv, 0);
                v = r2;
            }
            ch->addr = addr;
            ch->val = v;
            ch->kind = 0;
            ch->elem = (agg_types[ti].misarr[last_fi] &&
                        agg_types[ti].marr_len[last_fi] <= 0)
                           ? -1 : agg_types[ti].mscode[last_fi];
            ch->elem_ptr = 0;
            ch->is_slice = 0;
            ch->agg_ti = -1;
            base_vreg = v;
            ptr_base = 0;
            step++;
            continue;
        }
        if (t == TOK_DOT) {
            /* 成员访问 s.field（首步）或 `.(AggType).field`（PB-83） */
            if (vty[vi] < 0 && ch->agg_ti < 0) {
                nihao_error(cs, "ir: member access is only supported on an aggregate "
                                "variable");
                return 0;
            }
            next_tok(cs);
            const char *fname = cs->parser.lex->tok_str;
            next_tok(cs);
            int from_ptr = (ch->agg_ti >= 0);
            int ti = from_ptr ? ch->agg_ti : vty[vi];
            int off = 0;
            int last_fi = -1;
            for (;;) {
                int fi = agg_member_find(ti, fname);
                if (fi < 0) {
                    nihao_error(cs, "ir: no member '%s' in type '%s'", fname,
                                agg_types[ti].name);
                    return 0;
                }
                last_fi = fi;
                off += (agg_types[ti].kind == 1) ? 0 : agg_types[ti].moff[fi];
                int mt = agg_types[ti].mtype[fi];
                if (mt >= 0 && cur_tok(cs) == TOK_DOT) {
                    ti = mt;
                    next_tok(cs);
                    fname = cs->parser.lex->tok_str;
                    next_tok(cs);
                    continue;
                }
                break;
            }
            int esize = 0;
            int addr;
            int had_bracket = (cur_tok(cs) == TOK_LBRACKET);
            if (from_ptr) {
                if (cur_tok(cs) == TOK_LBRACKET) {
                    nihao_error(cs, "ir: array member through '.(AggType)' is not supported");
                    return 0;
                }
                addr = ir_ptr_byte_off(ch->val, off);
            } else if (cur_tok(cs) == TOK_LBRACKET) {
                addr = ir_member_elem_addr(cs, vi, ti, last_fi, off, fname, &esize);
                if (addr < 0) return 0;
            } else {
                addr = ir_member_addr(cs, vi, ti, last_fi, off);
            }
            if (!from_ptr && !had_bracket &&
                agg_types[ti].misarr[last_fi] && agg_types[ti].marr_len[last_fi] > 0) {
                /* PB-83：数组成员名退化为首元素地址（char[9] 成员 → 指针） */
                ch->addr = addr;
                ch->val = addr;
                ch->kind = 1;
                ch->elem = agg_types[ti].marr_esize[last_fi];
                ch->elem_ptr = 0;
                ch->width = agg_types[ti].marr_esize[last_fi];
                ch->is_slice = 0;
                ch->agg_ti = -1;
                base_vreg = addr;
                ptr_base = 1;
                step++;
                continue;
            }
            if (cur_tok(cs) != TOK_LBRACKET &&
                agg_types[ti].misarr[last_fi] && agg_types[ti].marr_len[last_fi] > 0) {
                /* PB-83：数组成员名退化为首元素地址 */
                return addr;
            }
            int v = ir_new_vreg(F);
            if (esize == 1) ir_load1(v, addr, ir_member_vcode(ti, last_fi));
            else {
                /* PB-76(b)：标量成员按成员类型窄读（符号扩展）；位域保持 8 字节 + AND */
                int mcode = (agg_types[ti].mbits[last_fi] > 0) ? 0
                          : ir_member_vcode(ti, last_fi);
                /* PB-84：位域按存储单元宽读（u8 单位=1 字节） */
                ir_emit(F, IR_LOAD, v, addr, -1,
                        agg_types[ti].mbits[last_fi] > 0
                            ? (ir_bf_unit(ti, last_fi) == 8 ? 0 : ir_bf_unit(ti, last_fi))
                            : ir_load_width_imm(mcode));
                if (mcode == 1 || mcode == 8) ir_set_double(v);   /* PB-79：浮点成员读值标记 */
            }
            if (agg_types[ti].mbits[last_fi] > 0) {
                int sh = ir_bf_shift(ti, last_fi);
                if (sh > 0) {
                    int sc = ir_new_vreg(F);
                    ir_emit(F, IR_CONST, sc, -1, -1, sh);
                    int r1 = ir_new_vreg(F);
                    ir_emit(F, IR_SHR, r1, v, sc, 0);
                    v = r1;
                }
                int mv = ir_bitmask(agg_types[ti].mbits[last_fi]);
                int r2 = ir_new_vreg(F);
                ir_emit(F, IR_AND, r2, v, mv, 0);
                v = r2;
            }
            ch->addr = addr;
            ch->val = v;
            ch->kind = 0;
            ch->elem = (agg_types[ti].misarr[last_fi] &&
                        agg_types[ti].marr_len[last_fi] <= 0)
                           ? -1 : agg_types[ti].mscode[last_fi];
            ch->elem_ptr = 0;
            ch->is_slice = 0;
            ch->agg_ti = -1;
            base_vreg = v;
            ptr_base = 0;
            step++;
            continue;
        }
        break;
    }
    return 1;
}

/* 链式赋值（PB-68）：<链> = e / <链> op= e / <链>++ / <链>--。
 * 链由 ir_chain 求值，末步给出可写地址与元素类型（元素类型用于截断协调）。 */
static void ir_chain_assign(CompilerState *cs, int vi)
{
    IrChain ch;
    if (!ir_chain(cs, vi, &ch)) {
        skip_newlines(cs);
        return;
    }
    if (ch.addr < 0) {
        nihao_error(cs, "ir: the left-hand side of the assignment is not a "
                        "writable location");
        skip_newlines(cs);
        return;
    }
    if (vvis[vi] == VIS_CONST && !vptr[vi] && vmode[vi] != VM_PTRARR) {
        /* PB-90：const 变量的自有存储不可写（指针指向内存不受限） */
        nihao_error(cs, "ir: cannot modify the const '%s'", vn[vi]);
        ir_skip_line(cs);
        return;
    }
    int sc = (ch.elem >= 0) ? ch.elem : vtype[vi];
    /* PB-82：链上元素的字节宽（动态字符串按 1；标量按类型码；未知按整槽 0） */
    int ldw = ir_load_width_imm(sc);
    int stw = ir_store_width(sc);
    if (ch.elem == 8 && vetyp[vi] == 8) { ldw = 0; stw = 1; }
    TokenType at = cur_tok(cs);
    int is_compound = 0;
    IrOp op = IR_NOP;
    if (at == TOK_PLUS_ASSIGN)         { op = IR_ADD; is_compound = 1; }
    else if (at == TOK_MINUS_ASSIGN)    { op = IR_SUB; is_compound = 1; }
    else if (at == TOK_STAR_ASSIGN)     { op = IR_MUL; is_compound = 1; }
    else if (at == TOK_SLASH_ASSIGN)    { op = IR_DIV; is_compound = 1; }
    else if (at == TOK_PERCENT_ASSIGN)  { op = IR_MOD; is_compound = 1; }
    if (at == TOK_INCREMENT || at == TOK_DECREMENT) {
        /* 后缀 ++ / -- */
        next_tok(cs);
        int one = ir_new_vreg(F);
        ir_emit(F, IR_CONST, one, -1, -1, 1);
        int cur = ir_new_vreg(F);
        ir_emit(F, IR_LOAD, cur, ch.addr, -1, ldw);
        int nv = ir_new_vreg(F);
        ir_emit(F, at == TOK_INCREMENT ? IR_ADD : IR_SUB, nv, cur, one, 0);
        nv = ir_coerce(nv, sc);
        ir_emit(F, IR_STORE, -1, ch.addr, nv, stw);
        skip_newlines(cs);
        return;
    }
    if (at != TOK_ASSIGN && !is_compound) {
        nihao_error(cs, "ir: expected '=' or 'op=' after the pointer expression");
        skip_newlines(cs);
        return;
    }
    next_tok(cs);
    if (ch.is_slice && cur_tok(cs) == TOK_STRING_LITERAL) {
        /* PB-83：切片右值是字符串字面量 → 逐字节复制（含 NUL）；PB-84：按容量校验 */
        const char *lit = nihao_strdup(g_cs, cs->parser.lex->tok_str);
        if (ch.slice_lo >= 0 && ch.slice_hi >= ch.slice_lo) {
            int cap = ch.slice_hi - ch.slice_lo + 1;
            int need = (int)strlen(lit) + 1;
            if (need > cap)
                nihao_error(cs, "ir: string needs %d bytes with terminator, slice holds %d",
                            need, cap);
        }
        next_tok(cs);
        ir_copy_literal(cs, ch.addr, lit);
        skip_newlines(cs);
        return;
    }
    int b;
    if (!is_compound && cur_tok(cs) == TOK_STRING_LITERAL && ch.elem < 0) {
        /* PB-89：动态字符串成员/通用指针赋字面量 → 可写副本 */
        const char *lit = nihao_strdup(g_cs, cs->parser.lex->tok_str);
        next_tok(cs);
        b = ir_strlit_copy(cs, lit);
    } else {
        b = ir_expr(cs);
    }
    int v;
    if (!is_compound) {
        v = b;
    } else {
        int cur = ir_new_vreg(F);
        ir_emit(F, IR_LOAD, cur, ch.addr, -1, ldw);
        int rhs = b;
        if (sc == 1 || sc == 8) {
            /* 浮点元素：LOAD 已是 double 位模式 */
            ir_set_double(cur);
            if (!ir_is_double(rhs)) rhs = ir_to_double(rhs);
            int nv = ir_new_vreg(F);
            ir_emit(F, op == IR_MUL ? IR_FMUL : op == IR_DIV ? IR_FDIV :
                        op == IR_SUB ? IR_FSUB : IR_FADD, nv, cur, rhs, 0);
            ir_set_double(nv);
            v = nv;
        } else {
            if (ir_is_double(cur)) { int t = ir_new_vreg(F); ir_emit(F, IR_DTOI, t, cur, -1, 0); cur = t; }
            if (ir_is_double(rhs)) { int t = ir_new_vreg(F); ir_emit(F, IR_DTOI, t, rhs, -1, 0); rhs = t; }
            int nv = ir_new_vreg(F);
            ir_emit(F, op, nv, cur, rhs, 0);
            v = nv;
        }
    }
    v = ir_coerce(v, sc);
    ir_emit(F, IR_STORE, -1, ch.addr, v, stw);
    skip_newlines(cs);
}

/* 基本类型字节大小（语言语义） */
static long long type_size_token(TokenType t)
{
    switch (t) {
        case TOK_BOOL: case TOK_CHAR: case TOK_U8: case TOK_I8:   return 1;
        case TOK_U16: case TOK_I16:                                 return 2;
        case TOK_U32: case TOK_I32: case TOK_F32: case TOK_FX32:    return 4;
        case TOK_U64: case TOK_I64: case TOK_F64: case TOK_FX64:    return 8;
        case TOK_STRING:                                            return 8;  /* 指针 */
        case TOK_VOID:                                              return 0;
        default:                                                    return -1;
    }
}

/* alignof（PB-74）：标量对齐 = 字节宽；void/string 是对齐 8 的指针。 */
static long long type_align_token(TokenType t)
{
    if (t == TOK_VOID || t == TOK_STRING) return 8;
    long long s = type_size_token(t);
    return s >= 0 ? s : -1;
}

/* 聚合体对齐 = 成员最大对齐（struct/union 同），递归嵌套；enum 按 int 4 字节。 */
static long long ir_agg_align(int ti)
{
    IrAggType *a = &agg_types[ti];
    if (a->kind == 2) return 4;                 /* enum */
    long long al = 1;
    for (int i = 0; i < a->mcount; i++) {
        long long ma;
        if (a->mtype[i] >= 0)            ma = ir_agg_align(a->mtype[i]);
        else if (a->marr_esize[i] > 0)   ma = a->marr_esize[i];
        else                             ma = a->msize[i] > 0 ? a->msize[i] : 1;
        if (ma > al) al = ma;
    }
    return al;
}

/* `.(T)` 的显式类型 → 标量类型码（0=i64 1=f64 2=i8 3=i16 4=i32 5=u8 6=u16 7=u32 8=f32）。
 * 与变量声明处那条 vt_code 链同源；返回 -1 表示不是标量类型（`.()` 无类型形态或聚合类型）。 */
static int ir_scalar_code_of_token(TokenType tt)
{
    if (!is_type_token(tt)) return -1;
    switch (tt) {
        case TOK_F32:                                     return 8;
        case TOK_F64:                                     return 1;
        case TOK_CHAR: case TOK_I8: case TOK_BOOL:        return 2;
        case TOK_I16:                                     return 3;
        case TOK_I32: case TOK_FX32:                      return 4;
        case TOK_U8:                                      return 5;
        case TOK_U16:                                     return 6;
        case TOK_U32:                                     return 7;
        default:                                          return 0;   /* i64/u64/string/void/聚合 */
    }
}

/* PB-75(b)：指针解引用的窄标量读宽——填进 IR_LOAD 的 imm：
 *   正 w = 零扩展读 w 字节（u8/u16/u32）；负 -w = 符号扩展读 w 字节（i8/i16/i32）；
 *   0 = 8 字节读（i64/f64/f32，保持旧行为）。读后值域已是正确 64 位，后续 ir_coerce 幂等。 */
/* PB-84：标量类型码 → 字节宽（读越界诊断用） */
static int ir_code_bytes(int code)
{
    switch (code) {
        case 2: case 5: return 1;
        case 3: case 6: return 2;
        case 4: case 7: case 8: return 4;
        default: return 8;
    }
}

static int64_t ir_load_width_imm(int code)
{
    switch (code) {
        case 2: return -1;   /* i8  符号扩展 */
        case 5: return  1;   /* u8  零扩展 */
        case 3: return -2;   /* i16 符号扩展 */
        case 6: return  2;   /* u16 零扩展 */
        case 4: return -4;   /* i32 符号扩展 */
        case 7: return  4;   /* u32 零扩展 */
        default: return 0;   /* 0=i64 1=f64 8=f32/指针等 → 8 字节 */
    }
}

/* PB-75(b)：窄标量存储宽（IR_STORE 的 imm）：写低 w 字节；0 = 8 字节。 */
static int ir_store_width(int code)
{
    switch (code) {
        case 2: case 5: return 1;
        case 3: case 6: return 2;
        case 4: case 7: return 4;
        default: return 0;
    }
}

/* 解析类型并返回字节大小（含聚合/数组）。
 * type_idx_out 非空时输出聚合类型索引（-1=基本类型）。失败返回 -1。 */
static long long type_size_of(CompilerState *cs, int *type_idx_out)
{
    TokenType t = cur_tok(cs);
    long long sz = type_size_token(t);
    int ti = -1;
    if (sz >= 0) {
        next_tok(cs);
    } else if (t == TOK_IDENTIFIER) {
        ti = agg_type_find(cs->parser.lex->tok_str);
        if (ti < 0) {
            nihao_error(cs, "ir: unknown type '%s'", cs->parser.lex->tok_str);
            next_tok(cs);
            return -1;
        }
        next_tok(cs);
        IrAggType *a = &agg_types[ti];
        /* enum 4 字节；struct/union 按 8 字节槽模型 = 成员数*8（union 仍按成员数计） */
        sz = (a->kind == 2) ? 4 : (long long)a->mcount * 8;
    } else {
        nihao_error(cs, "ir: expected a type name");
        return -1;
    }
    /* 数组 type[N] */
    if (cur_tok(cs) == TOK_LBRACKET) {
        next_tok(cs);
        long long n = 1;
        if (cur_tok(cs) == TOK_INT_CONST) {
            n = cs->parser.lex->tok_val.i;
            next_tok(cs);
        }
        expect(cs, TOK_RBRACKET);
        sz = sz * n;
    }
    if (type_idx_out) *type_idx_out = ti;
    return sz;
}

/* malloc(T) / malloc(T[N]) 的字节数（PB-68）：void 按通用指针 8 字节计，
 * 数组后缀逐维连乘（A 侧同口径：`malloc(void[3])` → 3 * sizeof(void*) = 24，
 * `malloc(void[4][5])` → 20 * sizeof(void*) = 160）。 */
static long long ir_malloc_size(CompilerState *cs)
{
    long long sz;
    if (cur_tok(cs) == TOK_VOID) {
        sz = 8;
        next_tok(cs);
    } else {
        sz = type_size_of(cs, NULL);
        if (sz < 0) sz = 8;
    }
    while (cur_tok(cs) == TOK_LBRACKET) {
        next_tok(cs);
        long long n = 1;
        if (cur_tok(cs) == TOK_INT_CONST) {
            n = cs->parser.lex->tok_val.i;
            next_tok(cs);
        }
        expect(cs, TOK_RBRACKET);
        sz *= n;
    }
    return sz;
}

static int ir_primary(CompilerState *cs)
{
    TokenType t = cur_tok(cs);
    if (t == TOK_PLUS) {
        /* PB-70：BNF §1.3 的 <int-literal> / <float-literal> 自带符号，正号是字面量的一部分。
         * 只在紧跟数字字面量时消费，其余位置的 '+' 仍是二元运算符（与 A 后端 parse_primary 同口径，
         * 不放宽成一元加）。 */
        TokenType nt = ir_peek_ahead(cs, 1);
        if (nt == TOK_INT_CONST || nt == TOK_FLOAT_CONST) {
            next_tok(cs);             /* '+' */
            return ir_primary(cs);
        }
    }
    if (t == TOK_TRUE || t == TOK_FALSE) {
        /* PB-70：布尔字面量（BNF <bool-literal>）——槽模型下按 1 / 0 的整型常量落 */
        int vr = ir_new_vreg(F);
        ir_emit(F, IR_CONST, vr, -1, -1, (t == TOK_TRUE) ? 1 : 0);
        next_tok(cs);
        return vr;
    }
    if (t == TOK_MINUS) {
        /* 一元负号：int → IR_NEG；double → 0.0 - a（FSUB，整数取反会毁位模式） */
        next_tok(cs);
        int a = ir_primary(cs);
        int vr = ir_new_vreg(F);
        if (ir_is_double(a)) {
            int zero = ir_new_vreg(F);
            union { double d; int64_t i; } u;
            u.d = 0.0;
            ir_emit(F, IR_CONST, zero, -1, -1, u.i);
            ir_set_double(zero);
            ir_emit(F, IR_FSUB, vr, zero, a, 0);
            ir_set_double(vr);
        } else {
            ir_emit(F, IR_NEG, vr, a, -1, 0);
        }
        return vr;
    }
    if (t == TOK_LOGICAL_NOT) {
        /* 逻辑非 !x -> (x == 0)；double 用 FCMP(EQ, 0.0) */
        next_tok(cs);
        int a = ir_primary(cs);
        int vr = ir_new_vreg(F);
        if (ir_is_double(a)) {
            int z = ir_new_vreg(F);
            union { double d; int64_t i; } u;
            u.d = 0.0;
            ir_emit(F, IR_CONST, z, -1, -1, u.i);
            ir_set_double(z);
            ir_emit(F, IR_FCMP, vr, a, z, 0);
        } else {
            int z = ir_new_vreg(F);
            ir_emit(F, IR_CONST, z, -1, -1, 0);
            ir_emit(F, IR_CMP_EQ, vr, a, z, 0);
        }
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
        /* 一元解引用 *p 已从 1.0.x/2.0 指针语法移除（解引用统一 .()/.(T)/->）。
         * 乘法 * 在 ir_term 处理，此处遇 * 即报错并吞掉该 token 防 fallthrough 死循环。 */
        nihao_error(cs, "ir: unary dereference '*p' removed; use p.() instead");
        next_tok(cs);
        return ir_new_vreg(F);
    }
    if (t == TOK_STRUCTOF || t == TOK_HOLDOF || t == TOK_UNIONOF ||
        t == TOK_BITOFFSETOF) {
        /* PB-83：从属查询三关键字（holdof 是标识符，走 identifier 分支） */
        return ir_ownerof(cs, t == TOK_BITOFFSETOF);
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
            if (cur_tok(cs) == TOK_DOT && vty[vi] >= 0) {
                /* PB-83：&聚合体.成员 → 成员字节地址（ownerof / 动态字符串成员取址） */
                int ti2 = vty[vi];
                int off2 = 0, lfi = -1;
                for (;;) {
                    next_tok(cs);   /* . */
                    if (cur_tok(cs) != TOK_IDENTIFIER) {
                        nihao_error(cs, "ir: expected member name after '.'");
                        return ir_new_vreg(F);
                    }
                    int fi2 = agg_member_find(ti2, cs->parser.lex->tok_str);
                    if (fi2 < 0) {
                        nihao_error(cs, "ir: no member '%s' in type '%s'",
                                    cs->parser.lex->tok_str, agg_types[ti2].name);
                        return ir_new_vreg(F);
                    }
                    lfi = fi2;
                    off2 += (agg_types[ti2].kind == 1) ? 0
                                                        : agg_types[ti2].moff[fi2];
                    int mt2 = agg_types[ti2].mtype[fi2];
                    next_tok(cs);   /* field */
                    if (mt2 >= 0 && cur_tok(cs) == TOK_DOT) {
                        ti2 = mt2;
                        continue;
                    }
                    break;
                }
                (void)lfi;
                return ir_member_addr(cs, vi, ti2, lfi, off2);
            }
            if (ve[vi] > 0 && ir_elem_stride(vi) != 8)   /* PB-82：紧凑数组 = 块首 */
                return ir_arr_lo(vi);
            if (vty[vi] >= 0) return ir_agg_base(vi);    /* PB-83：聚合体 = 块首 */
            int vr = ir_new_vreg(F);
            ir_emit(F, IR_ADDR, vr, vt[vi], -1, 0);
            return vr;
        }
        nihao_error(cs, "ir: '&' requires a variable identifier");
        next_tok(cs);
        return ir_new_vreg(F);
    }
    if (t == TOK_INCREMENT || t == TOK_DECREMENT) {
        /* 前缀 ++x / --x：x 自增/减 1，表达式值为新值（写回变量槽） */
        int is_inc = (t == TOK_INCREMENT);
        next_tok(cs);
        if (cur_tok(cs) != TOK_IDENTIFIER) {
            nihao_error(cs, "ir: prefix '++'/'--' requires a variable");
            return ir_new_vreg(F);
        }
        const char *pname = cs->parser.lex->tok_str;
        int pvi = var_find(pname);
        next_tok(cs);
        if (pvi < 0) {
            nihao_error(cs, "ir: undeclared variable '%s'", pname);
            return ir_new_vreg(F);
        }
        ir_check_const_store(cs, pvi);   /* PB-90：const 不可 ++/-- */
        int one = ir_new_vreg(F);
        ir_emit(F, IR_CONST, one, -1, -1, 1);
        int nv = ir_new_vreg(F);
        if (vtype[pvi] == 1 || vtype[pvi] == 8) {
            /* 浮点变量（f64/f32）：1 转 double 位模式 + FADD/FSUB */
            union { double d; int64_t i; } u;
            u.d = 1.0;
            one = ir_new_vreg(F);
            ir_emit(F, IR_CONST, one, -1, -1, u.i);
            ir_set_double(one);
            ir_emit(F, is_inc ? IR_FADD : IR_FSUB, nv, vt[pvi], one, 0);
            ir_set_double(nv);
        } else {
            ir_emit(F, is_inc ? IR_ADD : IR_SUB, nv, vt[pvi], one, 0);
        }
        ir_emit(F, IR_MOV, vt[pvi], nv, -1, 0);   /* 写回 */
        return nv;
    }
    if (t == TOK_INT_CONST) {
        int vr = ir_new_vreg(F);
        ir_emit(F, IR_CONST, vr, -1, -1, cs->parser.lex->tok_val.i);
        next_tok(cs);
        return vr;
    }
    if (t == TOK_FLOAT_CONST) {
        /* 浮点字面量：CONST imm 存 double 位模式，vreg 标记为 double */
        int vr = ir_new_vreg(F);
        union { double d; int64_t i; } u;
        u.d = cs->parser.lex->tok_val.f;
        ir_emit(F, IR_CONST, vr, -1, -1, u.i);
        ir_set_double(vr);
        next_tok(cs);
        return vr;
    }
    if (t == TOK__UNDEF || t == TOK__CONST || t == TOK__FLOW ||
        t == TOK__STATIC || t == TOK__VAR) {
        /* 可见性枚举常量：_undef/_const/_flow/_static/_var → NH_* 值 */
        long long vv = (t == TOK__UNDEF) ? VIS_UNDEF :
                       (t == TOK__CONST) ? VIS_CONST :
                       (t == TOK__FLOW)  ? VIS_FLOW :
                       (t == TOK__STATIC)? VIS_STATIC : VIS_VAR;
        int vr = ir_new_vreg(F);
        ir_emit(F, IR_CONST, vr, -1, -1, vv);
        next_tok(cs);
        return vr;
    }
    if (t == TOK_CHAR_CONST) {
        /* 字符字面量 'A' → ASCII 码（lexer tok_val.i 已是 unsigned char） */
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
    if (t == TOK_VISOF) {
        /* visof(x)：编译期可见性查询 → NH_* 常量（visof 是关键字 TOK_VISOF） */
        next_tok(cs);
        expect(cs, TOK_LPAREN);
        int vv = VIS_UNDEF;
        if (cur_tok(cs) == TOK_IDENTIFIER) {
            int vi = var_find(cs->parser.lex->tok_str);
            if (vi >= 0) vv = vvis[vi];
            next_tok(cs);
        }
        expect(cs, TOK_RPAREN);
        int vr = ir_new_vreg(F);
        ir_emit(F, IR_CONST, vr, -1, -1, vv);
        return vr;
    }
    if (t == TOK_SIZEOF || t == TOK_TYPEOF || t == TOK_ALIGNOF) {
        /* sizeof(type/expr) / typeof（映射为 sizeof）/ alignof（PB-74 按类型真实对齐，
         * 不再恒返 8） */
        next_tok(cs);
        expect(cs, TOK_LPAREN);
        long long sz;
        if (t == TOK_ALIGNOF) {
            TokenType tt = cur_tok(cs);
            long long al = type_align_token(tt);
            if (al >= 0) {
                next_tok(cs);
            } else if (tt == TOK_IDENTIFIER) {
                int ti = agg_type_find(cs->parser.lex->tok_str);
                if (ti < 0) {
                    nihao_error(cs, "ir: unknown type '%s'", cs->parser.lex->tok_str);
                    next_tok(cs);
                    al = 1;
                } else {
                    next_tok(cs);
                    al = ir_agg_align(ti);
                }
            } else {
                nihao_error(cs, "ir: expected a type name after 'alignof'");
                al = 1;
            }
            if (cur_tok(cs) == TOK_LBRACKET) {   /* T[N]：数组对齐 = 元素对齐，吞掉后缀 */
                next_tok(cs);
                if (cur_tok(cs) == TOK_INT_CONST) next_tok(cs);
                expect(cs, TOK_RBRACKET);
            }
            sz = al;
        } else if (is_type_token(cur_tok(cs)) || cur_tok(cs) == TOK_IDENTIFIER) {
            sz = type_size_of(cs, NULL);
        } else {
            ir_expr(cs);            /* sizeof(expr)：表达式值为 8 字节 */
            sz = 8;
        }
        expect(cs, TOK_RPAREN);
        int vr = ir_new_vreg(F);
        ir_emit(F, IR_CONST, vr, -1, -1, sz);
        return vr;
    }
    if (t == TOK_OFFSETOF) {
        /* offsetof(Type, member) -> 成员字节偏移（IR 槽模型 = 成员序*8） */
        next_tok(cs);
        expect(cs, TOK_LPAREN);
        int ti = -1;
        type_size_of(cs, &ti);
        expect(cs, TOK_COMMA);
        long long off = -1;
        if (cur_tok(cs) == TOK_IDENTIFIER) {
            const char *mname = cs->parser.lex->tok_str;
            next_tok(cs);
            int fi = (ti >= 0) ? agg_member_find(ti, mname) : -1;
            if (fi >= 0)
                off = (agg_types[ti].kind == 1) ? 0 : (long long)fi * 8;  /* union 0 */
            else
                nihao_error(cs, "ir: offsetof: no member '%s'", mname);
        }
        expect(cs, TOK_RPAREN);
        int vr = ir_new_vreg(F);
        ir_emit(F, IR_CONST, vr, -1, -1, off < 0 ? 0 : off);
        return vr;
    }
    if (t == TOK_IDENTIFIER) {
        const char *name = cs->parser.lex->tok_str;
        /* len(x)：内置——数组/动态字符串/切片变量的逻辑长度（ve 表） */
        if (strcmp(name, "len") == 0) {
            LexerState *lx0 = cs->parser.lex;
            lx0->peek_valid = 0;
            lexer_peek(lx0);
            if (lx0->peek_tok == TOK_LPAREN) {
                next_tok(cs);           /* len */
                next_tok(cs);           /* ( */
                long long vlen = 0;
                if (cur_tok(cs) == TOK_IDENTIFIER) {
                    const char *lname = cs->parser.lex->tok_str;
                    int vi2 = var_find(lname);
                    /* PB-72：与 A 后端同判据（Symbol.len_known）——标量与长度未登记的
                     * 变量都必须即时报错，此前一律静默给 0（台账 E-c 桶）。 */
                    if (vi2 < 0 || !vlen_known[vi2]) {
                        nihao_error(cs, "ir: len: logical length of '%s' is not statically known",
                                    lname);
                    } else {
                        vlen = ve[vi2];
                    }
                    next_tok(cs);
                }
                expect(cs, TOK_RPAREN);
                int vr = ir_new_vreg(F);
                ir_emit(F, IR_CONST, vr, -1, -1, vlen);
                return vr;
            }
            lx0->peek_valid = 0;
        }
        /* PB-83：holdof 是标识符形态的从属查询（structof/unionof/bitoffsetof 是关键字） */
        if (strcmp(name, "holdof") == 0) {
            LexerState *lx0 = cs->parser.lex;
            lx0->peek_valid = 0;
            lexer_peek(lx0);
            if (lx0->peek_tok == TOK_LPAREN)
                return ir_ownerof(cs, 0);
            lx0->peek_valid = 0;
        }
        /* enum 常量优先（编译期值） */
        {
            long long eval;
            if (var_find(name) < 0 && enum_const_find(name, &eval)) {
                int vr = ir_new_vreg(F);
                ir_emit(F, IR_CONST, vr, -1, -1, eval);
                next_tok(cs);
                return vr;
            }
        }
        /* malloc(Type) / malloc(Type[N])：动态分配 → 调用外部 malloc(字节数)
         * malloc 是普通标识符（非关键字），参数是类型，在 LPAREN 分支特判 */
        next_tok(cs);
        if (cur_tok(cs) == TOK_LPAREN) {
            /* 函数指针间接调用：fp(args)（name 是变量且后跟 (） */
            int fvi = var_find(name);
            if (fvi >= 0) {
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
                int addr = ir_new_vreg(F);
                ir_emit(F, IR_MOV, addr, vt[fvi], -1, 0);   /* 读函数指针值 */
                int vr = ir_new_vreg(F);
                ir_emit(F, IR_CALLI, vr, addr, -1, nargs);
                return vr;
            }
            /* 函数调用：malloc 特判（参数是类型，非表达式） */
            if (strcmp(name, "malloc") == 0) {
                next_tok(cs);
                /* PB-75(b)：记录 malloc 的标量元素码（声明/赋值处据此给 pt[] 记指向类型） */
                if (cur_tok(cs) == TOK_VOID || cur_tok(cs) == TOK_IDENTIFIER) {
                    last_malloc_sc = -1;
                } else if (is_type_token(cur_tok(cs))) {
                    last_malloc_sc = ir_scalar_code_of_token(cur_tok(cs));
                } else {
                    last_malloc_sc = -1;
                }
                long long sz = ir_malloc_size(cs);
                expect(cs, TOK_RPAREN);
                int szv = ir_new_vreg(F);
                ir_emit(F, IR_CONST, szv, -1, -1, sz < 0 ? 8 : sz);
                ir_emit(F, IR_PARAM, -1, szv, -1, 0);
                int vr = ir_new_vreg(F);
                IrIns *in = &F->ins[ir_emit(F, IR_CALL, vr, -1, -1, 1)];
                in->sym = "malloc";
                in->fn = -1;
                return vr;
            }
            next_tok(cs);
            int argn = 0;
            int call_first_str = 0;   /* PB-64：print 的首个实参是否为格式串 */
            int arglist[64];
            /* PB-16 struct 参数：按展开后参数索引查目标函数聚合签名 */
            IrFn *tfn = ir_find_fn(name);
            int argi = (tfn && tfn->is_mr) ? 1 : 0;   /* mr 时 0 是 _mr_ret 缓冲 */
            /* M2 调用点参数前缀检查：call_pi = 用户参数序号（与 tfn->param_vis 对齐）；
             * call_borrow_src 记录被冻结的源，调用结束后解冻（借用仅持续调用期间） */
            int call_pi = 0;
            int call_borrow_src[32];
            int call_borrow_n = 0;
            if (cur_tok(cs) != TOK_RPAREN) {
                for (;;) {
                    int pti = (tfn && argi >= 0 && argi < 32) ? tfn->param_agg_ti[argi] : -1;
                    if (pti >= 0 && cur_tok(cs) == TOK_IDENTIFIER) {
                        /* 聚合实参：struct 变量（成员槽值）或 mr 调用（缓冲 LOAD） */
                        const char *an = cs->parser.lex->tok_str;
                        int avi = var_find(an);
                        LexerState *lx = cs->parser.lex;
                        lx->peek_valid = 0;
                        lexer_peek(lx);
                        TokenType nxt = lx->peek_tok;
                        lx->peek_valid = 0;
                        if (avi >= 0 && vty[avi] >= 0 && nxt != TOK_LPAREN) {
                            next_tok(cs);
                            IrAggType *aa = &agg_types[vty[avi]];
                            int chunks = aa->bytes > 0 ? (aa->bytes + 7) / 8 : 1;
                            int base = ir_agg_base(avi);
                            for (int k = 0; k < chunks && argn < 64; k++) {
                                int a2 = ir_ptr_byte_off(base, (long long)k * 8);
                                int lv = ir_new_vreg(F);
                                ir_emit(F, IR_LOAD, lv, a2, -1, 0);
                                arglist[argn++] = lv;
                            }
                        } else {
                            /* mr 调用（struct 返回）等：ir_expr 解析 → 从缓冲 LOAD 成员 */
                            int a = ir_expr(cs);
                            (void)a;
                            if (last_mr_buf >= 0) {
                                IrAggType *ra = &agg_types[pti];
                                int chunks = ra->bytes > 0 ? (ra->bytes + 7) / 8 : 1;
                                int base = ir_new_vreg(F);
                                ir_emit(F, IR_MOV, base, last_mr_buf, -1, 0);
                                for (int k = 0; k < chunks && argn < 64; k++) {
                                    int addr = ir_ptr_byte_off(base, (long long)k * 8);
                                    int lv = ir_new_vreg(F);
                                    ir_emit(F, IR_LOAD, lv, addr, -1, 0);
                                    arglist[argn++] = lv;
                                }
                                last_mr_buf = -1;
                            } else {
                                nihao_error(cs, "ir: struct argument must be a struct variable or struct-returning call");
                                if (argn < 64) arglist[argn++] = ir_new_vreg(F);
                            }
                        }
                        argi += agg_types[pti].bytes > 0
                                    ? (agg_types[pti].bytes + 7) / 8 : 1;
                    } else {
                        /* 标量实参：M2 参数前缀检查（flow→失效, var/const→冻结） */
                        int src_vi = -1;
                        if (cur_tok(cs) == TOK_IDENTIFIER) {
                            int sv = var_find(cs->parser.lex->tok_str);
                            if (sv >= 0) src_vi = sv;
                        }
                        if (argi == 0) call_first_str = (cur_tok(cs) == TOK_STRING_LITERAL);
                        int a = ir_expr(cs);
                        if (argn < 64) arglist[argn++] = a;
                        if (tfn && src_vi >= 0 && call_pi >= 0 && call_pi < 32) {
                            int pdv = tfn->param_vis[call_pi];
                            if (pdv != VIS_UNDEF) {
                                int borrowed = ir_vis_check_arg(cs, pdv, src_vi);
                                if (borrowed && call_borrow_n < 32)
                                    call_borrow_src[call_borrow_n++] = src_vi;
                            }
                        }
                        argi++;
                        call_pi++;
                    }
                    if (cur_tok(cs) != TOK_COMMA) break;
                    next_tok(cs);
                }
            }
            expect(cs, TOK_RPAREN);
            /* M2：调用结束，解冻被本次调用借用的源（借用仅持续调用期间） */
            for (int _k = 0; _k < call_borrow_n; _k++)
                if (call_borrow_src[_k] >= 0) vstate[call_borrow_src[_k]] = IR_VS_VALID;
            /* sret 调用（struct 返回）：目标函数是 mr → malloc 缓冲 → PARAM 缓冲+参数 →
             * CALL → 返回缓冲地址（调用方拷贝到聚合槽） */
            int is_mr_call = 0;
            int mr_agg_ti = -1;
            for (int fi = 0; fi < P->fn_count; fi++) {
                if (P->fns[fi].is_mr && P->fns[fi].name &&
                    strcmp(P->fns[fi].name, name) == 0) {
                    is_mr_call = 1;
                    mr_agg_ti = P->fns[fi].ret_agg_ti;
                    break;
                }
            }
            if (is_mr_call) {
                /* 缓冲大小 = 返回聚合类型成员数（struct 返回用 ret_agg_ti） */
                int ti = (mr_agg_ti >= 0) ? mr_agg_ti : agg_type_find("__mr");
                long long sz = (ti >= 0) ? (long long)agg_types[ti].bytes : 8;
                int szv = ir_new_vreg(F);
                ir_emit(F, IR_CONST, szv, -1, -1, sz);
                ir_emit(F, IR_PARAM, -1, szv, -1, 0);
                int buf = ir_new_vreg(F);
                IrIns *mi = &F->ins[ir_emit(F, IR_CALL, buf, -1, -1, 1)];
                mi->sym = "malloc";
                mi->fn = -1;
                /* 缓冲指针必须是第一个 PARAM（对应被调 _mr_ret） */
                ir_emit(F, IR_PARAM, -1, buf, -1, 0);
                for (int k = 0; k < argn; k++) {
                    ir_emit(F, IR_PARAM, -1, arglist[k], -1, 0);
                }
                int vr = ir_new_vreg(F);
                IrIns *ci = &F->ins[ir_emit(F, IR_CALL, vr, -1, -1, argn + 1)];
                ci->sym = name;
                ci->fn = -1;
                last_mr_buf = buf;      /* 调用方据此拷贝 */
                return vr;
            }
            /* PB-64：内置名 `print` 下降到 printf（A 后端同口径）。此前它被当外部符号
             * 原样发出，产物 C 里是裸 `print(...)`，tcc 只报 undefined symbol——
             * 台账里 flow 族四份用例就卡在这一条上。 */
            /* PB-77：print/puts 是 stdio 库成员；未 use stdio 时给一次温和提示（不阻断编译） */
            if (!stdio_imported && !stdio_warned &&
                (strcmp(name, "print") == 0 || strcmp(name, "puts") == 0)) {
                nihao_warning(cs, "print/puts come from the stdio library; add 'use stdio'");
                stdio_warned = 1;
            }
            int print_fmt = -1;
            if (strcmp(name, "print") == 0 && argn >= 1) {
                name = "printf";
                if (!call_first_str) {
                    /* print(e) → printf("%lld\n", (long long)e)：格式化整型，
                     * 浮点实参先 DTOI，与 A 侧那个 (long long) 转换等价 */
                    if (ir_is_double(arglist[0])) {
                        int dt = ir_new_vreg(F);
                        ir_emit(F, IR_DTOI, dt, arglist[0], -1, 0);
                        arglist[0] = dt;
                    }
                    int si = ir_add_string(P, "%lld\n");
                    print_fmt = ir_new_vreg(F);
                    IrIns *ai = &F->ins[ir_emit(F, IR_LD_ADDR, print_fmt, -1, -1, 0)];
                    ai->sym = P->str_syms[si];
                }
            }
            /* 非 mr 普通调用：按序发射 PARAM + CALL */
            if (print_fmt >= 0) ir_emit(F, IR_PARAM, -1, print_fmt, -1, 0);
            for (int k = 0; k < argn; k++) {
                ir_emit(F, IR_PARAM, -1, arglist[k], -1, 0);
            }
            int vr = ir_new_vreg(F);
            IrIns *in = &F->ins[ir_emit(F, IR_CALL, vr, -1, -1, argn + (print_fmt >= 0 ? 1 : 0))];
            in->sym = name;         /* 外部符号（puts / printf 等） */
            in->fn = -1;
            /* PB-浮点 ABI：目标函数浮点返回 → 标记 dst vreg（后端按此存储 xmm0/fa0） */
            for (int fi = 0; fi < P->fn_count; fi++) {
                if (P->fns[fi].name && strcmp(P->fns[fi].name, name) == 0 &&
                    P->fns[fi].ret_is_double) {
                    ir_set_double(vr);
                    break;
                }
            }
            return vr;
        }
        int vi = var_find(name);
        if (vi < 0) {
            /* 顶层全局变量（跨函数共享）：LD_ADDR → LOAD。
             * 注意：name 已被上面 next_tok 消费，这里不要再推进 token！ */
            int gi = global_find(name);
            if (gi >= 0) return ir_global_read(gi);
            /* cooking 编译期变量：运行时引用折叠为常量（PB-9 深化）。
             * 注意：name 已被上面 next_tok 消费，这里不要再推进 token！ */
            if (ct_var_exist(name)) {
                int vr = ir_new_vreg(F);
                ir_emit(F, IR_CONST, vr, -1, -1, ct_var_find(name));
                return vr;
            }
            /* 可能是函数名引用（取函数地址，供函数指针赋值） */
            for (int fi = 0; fi < P->fn_count; fi++) {
                if (P->fns[fi].name && strcmp(P->fns[fi].name, name) == 0) {
                    int vr = ir_new_vreg(F);
                    ir_emit(F, IR_LD_ADDR, vr, -1, -1, 0);
                    F->ins[F->ins_count - 1].sym = name;
                    return vr;
                }
            }
            nihao_error(cs, "ir: undeclared variable '%s'", name);
            return ir_new_vreg(F);
        }
        /* PB-80：聚合体数组元素成员读 a[i].x / e[i][j].y（vndim>0 且带聚合类型） */
        if (vty[vi] >= 0 && vndim[vi] >= 1 && cur_tok(cs) == TOK_LBRACKET)
            return ir_agg_elem_read(cs, vi);
        /* PB-68：多步后缀链与新形态（空下标 p[]、数组型后缀 .(T[N])）走链式求值；
         * 单组旧形态仍走下面各自的旧路径（行为不变）。 */
        if ((cur_tok(cs) == TOK_LBRACKET || cur_tok(cs) == TOK_DOT_PAREN ||
             cur_tok(cs) == TOK_ARROW || cur_tok(cs) == TOK_DOT) &&
            ir_chain_needs_new(cs, vi)) {
            IrChain ch;
            if (!ir_chain(cs, vi, &ch)) return ir_new_vreg(F);
            return ch.val;
        }
        if (cur_tok(cs) == TOK_LBRACKET) {
            /* 数组下标 arr[idx] -> LOAD(&arr[0] + idx*8)；切片 arr[lo..hi] -> 地址 */
            ir_check_void_subscript(cs, vi);   /* PB-72 */
            next_tok(cs);
            if (cur_tok(cs) == TOK_RANGE) {
                /* arr[..hi]：省略起始的切片 → &arr[0]（紧凑数组取块首） */
                next_tok(cs);
                ir_expr(cs);            /* hi 忽略 */
                expect(cs, TOK_RBRACKET);
                if (ir_elem_stride(vi) != 8) return ir_arr_lo(vi);
                int vr = ir_new_vreg(F);
                ir_emit(F, IR_ADDR, vr, vt[vi], -1, 0);
                return vr;
            }
            int idx = ir_expr(cs);
            if (cur_tok(cs) == TOK_RANGE) {
                /* 切片 arr[lo..hi] → 返回 &arr[lo]（长度信息忽略，留 TODO） */
                next_tok(cs);
                ir_expr(cs);            /* hi 忽略 */
                expect(cs, TOK_RBRACKET);
                return ir_elem_addr(cs, vi, vt[vi], idx, ir_elem_stride(vi), vetyp[vi] == 8);
            }
            expect(cs, TOK_RBRACKET);
            /* 多维（PB-67）：继续收 [j]…，按行主序摊平成一个扁平下标 */
            int idxs[IR_MAX_DIMS];
            int nidx = 0;
            idxs[nidx++] = idx;
            while (cur_tok(cs) == TOK_LBRACKET && nidx < IR_MAX_DIMS) {
                next_tok(cs);
                int j = ir_expr(cs);
                expect(cs, TOK_RBRACKET);
                idxs[nidx++] = j;
            }
            if (nidx > 1 || vndim[vi] >= 2) {
                int flat = ir_flat_index(cs, vi, idxs, nidx);
                int addr = ir_elem_addr(cs, vi, vt[vi], flat, ir_elem_stride(vi), vetyp[vi] == 8);
                if (nidx < vndim[vi]) return addr;   /* 只给部分下标 → 子行首地址 */
                int vr2 = ir_new_vreg(F);
                ir_arr_load(vr2, addr, ir_elem_stride(vi), vetyp[vi]);
                if (vetyp[vi] == 1) ir_set_double(vr2);
                return vr2;
            }
            int addr = ir_elem_addr(cs, vi, vt[vi], idx, ir_elem_stride(vi), vetyp[vi] == 8);
            int vr = ir_new_vreg(F);
            ir_arr_load(vr, addr, ir_elem_stride(vi), vetyp[vi]);
            if (vetyp[vi] == 1) ir_set_double(vr);   /* PB-1：浮点元素标记（运算/比较用） */
            return vr;
        }
        if (cur_tok(cs) == TOK_SAFE_DOT) {
            /* '?.' 已从语法移除，解引用检查统一由 '.()' 承担（BNF v2.3 / PA-21，与 A 后端同口径） */
            nihao_error(cs, "ir: '?.' is not part of the grammar; use '.()' "
                            "(dereference performs the visibility and bounds checks)");
            next_tok(cs);
            return ir_new_vreg(F);
        }
        if (cur_tok(cs) == TOK_DOT_PAREN) {
            /* 指针解引用 p.() → LOAD(*p)（p 变量存目标地址值；全量同语法。
             * 注意 DOT_PAREN 已含 '('——token 流 p, DOT_PAREN, ')'）。
             * 指向浮点（f64/f32）时标记结果 vreg 为 double，避免后续赋值/运算
             * 误把位模式当整数转换（ITOD）——pt[] 记录指针指向的标量类型。 */
            next_tok(cs);
            /* PB-63：`.(T)` 显式类型——此前只支持空括号 `.()`，读到类型名就
             * 在 expect(')') 处断掉（`expected ')', got 'i32'`），台账里 6 份用例卡在这 */
            int tcode = -1;
            if (cur_tok(cs) != TOK_RPAREN) {
                if (cur_tok(cs) == TOK_VOID && ir_peek_ahead(cs, 1) == TOK_RPAREN) {
                    /* 码 0 碰撞修复：裸 `.(void)` 无意义；`.(void[N])` 是合法数组后缀 */
                    nihao_error(cs, "ir: '.()' dereference: 'void' has no size — use a scalar type");
                    tcode = 0;
                } else {
                    tcode = ir_scalar_code_of_token(cur_tok(cs));
                }
                if (tcode < 0) {
                    nihao_error(cs, "ir: '.()' dereference takes a scalar type or nothing, "
                                    "not '%s'", cs->parser.lex->tok_str);
                    tcode = 0;
                }
                next_tok(cs);
            }
            expect(cs, TOK_RPAREN);
            ir_vis_check_usable(cs, vi);   /* M2：读前检查所有权是否已转移（use-after-move） */
            int addr = ir_new_vreg(F);
            ir_emit(F, IR_MOV, addr, vt[vi], -1, 0);    /* p 槽值 = 目标地址（MOV 勿 LOAD） */
            int vr = ir_new_vreg(F);
            /* PB-75(b)：窄标量按指向类型宽读（符号/零扩展进 imm），修 D1 堆越界 */
            {
                int rc = (tcode >= 0) ? tcode :
                         (PT_IS_SCALAR(pt[vi]) ? PT_SCALAR_CODE(pt[vi]) : -1);
                ir_emit(F, IR_LOAD, vr, addr, -1, ir_load_width_imm(rc));
            }
            if (tcode >= 0 && PT_IS_SCALAR(pt[vi])) {
                int pc0 = PT_SCALAR_CODE(pt[vi]);
                if (ir_code_bytes(tcode) > ir_code_bytes(pc0)) {
                    nihao_error(cs, "ir: '.()' dereference reads %d bytes from a %d-byte allocation",
                                ir_code_bytes(tcode), ir_code_bytes(pc0));
                }
            }
            if (tcode >= 0) {
                /* 宽度按 T：浮点标记照 T，窄整型走 TRUNC（与声明处一致） */
                if (tcode == 1 || tcode == 8) ir_set_double(vr);
                else if (tcode != 0) vr = ir_coerce(vr, tcode);
            } else if (PT_IS_SCALAR(pt[vi])) {
                int sc = PT_SCALAR_CODE(pt[vi]);
                if (sc == 1 || sc == 8) ir_set_double(vr);
            }
            return vr;
        }
        if (cur_tok(cs) == TOK_ARROW) {
            /* 指针成员访问 p->field：地址 = p 槽值（目标地址）+ off*8。
             * pt[vi] 记录指向的聚合类型（`p = &agg` 时写入） */
            next_tok(cs);
            const char *fname = cs->parser.lex->tok_str;
            next_tok(cs);
            if (pt[vi] < 0) {
                if (vptr[vi])
                    nihao_error(cs, "ir: cannot access a member of the generic 'void' pointer");
                else
                    nihao_error(cs, "ir: '%s' is not a pointer to aggregate (assign &var first)", name);
                /* 吞掉本行剩余 token，避免 '->' 级联误报 */
                while (cur_tok(cs) != TOK_NEWLINE && cur_tok(cs) != TOK_RBRACE &&
                       cur_tok(cs) != TOK_EOF) next_tok(cs);
                return ir_new_vreg(F);
            }
            int ti = pt[vi];
            int off = 0;
            int last_fi = -1;
            for (;;) {
                int fi = agg_member_find(ti, fname);
                if (fi < 0) {
                    nihao_error(cs, "ir: no member '%s' in type '%s'", fname,
                                agg_types[ti].name);
                    return ir_new_vreg(F);
                }
                last_fi = fi;
                off += (agg_types[ti].kind == 1) ? 0 : agg_types[ti].moff[fi];
                int mt = agg_types[ti].mtype[fi];
                if (mt >= 0 && (cur_tok(cs) == TOK_DOT || cur_tok(cs) == TOK_ARROW)) {
                    /* 聚合成员：继续链（嵌套 struct） */
                    ti = mt;
                    next_tok(cs);
                    fname = cs->parser.lex->tok_str;
                    next_tok(cs);
                    continue;
                }
                break;
            }
            int base = ir_new_vreg(F);
            ir_emit(F, IR_MOV, base, vt[vi], -1, 0);    /* p 槽值 = 目标地址（MOV 勿 LOAD） */
            /* off 为槽偏移：帧来源指针用方向感知的 ELEM_ADDR，堆/池来源用字节 ADD（PB-81） */
            int addr = ir_ptr_slot_off(vi, base, off);
            if (agg_types[ti].misarr[last_fi] && agg_types[ti].marr_len[last_fi] > 0) {
                /* PB-83：数组成员名退化为首元素地址 */
                return addr;
            }
            int vr = ir_new_vreg(F);
            /* PB-76(b)：标量成员按成员类型窄读（符号扩展）；位域成员保持 8 字节读 + AND */
            {
                int mcode = (agg_types[ti].mbits[last_fi] > 0) ? 0
                          : ir_member_vcode(ti, last_fi);
                /* PB-84：位域按存储单元宽读（u8 单位=1 字节） */
                ir_emit(F, IR_LOAD, vr, addr, -1,
                        agg_types[ti].mbits[last_fi] > 0
                            ? (ir_bf_unit(ti, last_fi) == 8 ? 0 : ir_bf_unit(ti, last_fi))
                            : ir_load_width_imm(mcode));
                if (mcode == 1 || mcode == 8) ir_set_double(vr);  /* PB-79：浮点成员读值标记 */
            }
            /* 位域成员：AND 掩码取低 N 位 */
            if (agg_types[ti].mbits[last_fi] > 0) {
                int sh = ir_bf_shift(ti, last_fi);
                if (sh > 0) {
                    int sc = ir_new_vreg(F);
                    ir_emit(F, IR_CONST, sc, -1, -1, sh);
                    int r1 = ir_new_vreg(F);
                    ir_emit(F, IR_SHR, r1, vr, sc, 0);
                    vr = r1;
                }
                int mv = ir_bitmask(agg_types[ti].mbits[last_fi]);
                int r2 = ir_new_vreg(F);
                ir_emit(F, IR_AND, r2, vr, mv, 0);
                vr = r2;
            }
            return vr;
        }
        if (cur_tok(cs) == TOK_DOT) {
            /* 成员访问 s.field / s.a.x 链式（嵌套）→ LOAD(&s + off*8) */
            next_tok(cs);
            const char *fname = cs->parser.lex->tok_str;
            next_tok(cs);
            if (vty[vi] < 0) {
                nihao_error(cs, "ir: '%s' is not an aggregate variable", name);
                return ir_new_vreg(F);
            }
            int ti = vty[vi];
            int off = 0;
            int last_fi = -1;
            for (;;) {
                int fi = agg_member_find(ti, fname);
                if (fi < 0) {
                    nihao_error(cs, "ir: no member '%s' in type '%s'", fname,
                                agg_types[ti].name);
                    return ir_new_vreg(F);
                }
                last_fi = fi;
                off += (agg_types[ti].kind == 1) ? 0 : agg_types[ti].moff[fi];
                int mt = agg_types[ti].mtype[fi];
                if (mt >= 0 && cur_tok(cs) == TOK_DOT) {
                    /* 聚合成员：继续链（嵌套 struct） */
                    ti = mt;
                    next_tok(cs);
                    fname = cs->parser.lex->tok_str;
                    next_tok(cs);
                    continue;
                }
                break;
            }
            int esize = 0;
            int addr;
            int had_bracket = (cur_tok(cs) == TOK_LBRACKET);
            if (had_bracket) {
                /* 数组成员读 p.name[0] */
                addr = ir_member_elem_addr(cs, vi, ti, last_fi, off, fname, &esize);
                if (addr < 0) return ir_new_vreg(F);
            } else {
                addr = ir_member_addr(cs, vi, ti, last_fi, off);
            }
            if (!had_bracket &&
                agg_types[ti].misarr[last_fi] && agg_types[ti].marr_len[last_fi] > 0) {
                /* PB-83：数组成员名退化为首元素地址 */
                return addr;
            }
            int vr = ir_new_vreg(F);
            if (esize == 1) ir_load1(vr, addr, ir_member_vcode(ti, last_fi));
            else {
                /* PB-76(b)：标量成员按成员类型窄读（符号扩展）；位域保持 8 字节 + AND */
                int mcode = (agg_types[ti].mbits[last_fi] > 0) ? 0
                          : ir_member_vcode(ti, last_fi);
                /* PB-84：位域按存储单元宽读（u8 单位=1 字节） */
                ir_emit(F, IR_LOAD, vr, addr, -1,
                        agg_types[ti].mbits[last_fi] > 0
                            ? (ir_bf_unit(ti, last_fi) == 8 ? 0 : ir_bf_unit(ti, last_fi))
                            : ir_load_width_imm(mcode));
                if (mcode == 1 || mcode == 8) ir_set_double(vr);  /* PB-79：浮点成员读值标记 */
            }
            /* 位域成员：AND 掩码取低 N 位 */
            if (agg_types[ti].mbits[last_fi] > 0) {
                int sh = ir_bf_shift(ti, last_fi);
                if (sh > 0) {
                    int sc = ir_new_vreg(F);
                    ir_emit(F, IR_CONST, sc, -1, -1, sh);
                    int r1 = ir_new_vreg(F);
                    ir_emit(F, IR_SHR, r1, vr, sc, 0);
                    vr = r1;
                }
                int mv = ir_bitmask(agg_types[ti].mbits[last_fi]);
                int r2 = ir_new_vreg(F);
                ir_emit(F, IR_AND, r2, vr, mv, 0);
                vr = r2;
            }
            return vr;
        }
        /* 赋值表达式（表达式级）：x = e / x op= e / x++ / x--，返回新值。
         * NihaoC 支持 `while x += 1 { is -1 {...} }`——条件值供 is 匹配。 */
        {
            TokenType nt = cur_tok(cs);
            /* 不用 -1 哨兵（IrOp 枚举在 tcc 下底层 unsigned，`op < 0` 恒假） */
            int is_compound = 0;
            int incr = 0;
            IrOp op = IR_NOP;
            if (nt == TOK_PLUS_ASSIGN) { op = IR_ADD; is_compound = 1; }
            else if (nt == TOK_MINUS_ASSIGN) { op = IR_SUB; is_compound = 1; }
            else if (nt == TOK_STAR_ASSIGN) { op = IR_MUL; is_compound = 1; }
            else if (nt == TOK_SLASH_ASSIGN) { op = IR_DIV; is_compound = 1; }
            else if (nt == TOK_PERCENT_ASSIGN) { op = IR_MOD; is_compound = 1; }
            else if (nt == TOK_INCREMENT) { op = IR_ADD; incr = 1; is_compound = 1; }
            else if (nt == TOK_DECREMENT) { op = IR_SUB; incr = 1; is_compound = 1; }
            if (nt == TOK_ASSIGN || nt == TOK_PLUS_ASSIGN || nt == TOK_MINUS_ASSIGN ||
                nt == TOK_STAR_ASSIGN || nt == TOK_SLASH_ASSIGN || nt == TOK_PERCENT_ASSIGN ||
                nt == TOK_INCREMENT || nt == TOK_DECREMENT) {
                next_tok(cs);
                int rhs;
                if (incr) {
                    int one = ir_new_vreg(F);
                    ir_emit(F, IR_CONST, one, -1, -1, 1);
                    rhs = one;
                } else {
                    rhs = ir_expr(cs);
                }
                int nv;
                if (!is_compound) nv = rhs;
                else {
                    nv = ir_new_vreg(F);
                    ir_emit(F, op, nv, vt[vi], rhs, 0);
                }
                nv = ir_coerce(nv, vtype[vi]);   /* PB-1：目标类型协调（DTOI/ITOD/TRUNC） */
                ir_emit(F, IR_MOV, vt[vi], nv, -1, 0);
                return nv;
            }
        }
        if (ve[vi] > 0 && vty[vi] < 0 && vetyp[vi] != 8 && vmode[vi] == VM_SLOT) {
            /* PB-82：定长数组名退化为首元素地址（puts(char[N])、切片 rhs 等） */
            if (ir_elem_stride(vi) != 8) return ir_arr_lo(vi);
            int av = ir_new_vreg(F);
            ir_emit(F, IR_ADDR, av, vt[vi], -1, 0);
            return av;
        }
        int vr = ir_new_vreg(F);
        ir_emit(F, IR_MOV, vr, vt[vi], -1, 0);
        if (vtype[vi] == 1 || vtype[vi] == 8) ir_set_double(vr);   /* 浮点变量读值标记（f32 亦 double 槽） */
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
        /* PB-13：任一侧为 double → 浮点运算（% 无浮点版）；混合类型先提升 int 侧 */
        if ((ir_is_double(a) || ir_is_double(b)) && op != IR_MOD) {
            if (!ir_is_double(a)) a = ir_to_double(a);
            if (!ir_is_double(b)) b = ir_to_double(b);
            IrOp fop = (op == IR_MUL) ? IR_FMUL :
                       (op == IR_DIV) ? IR_FDIV : IR_FMUL;
            ir_emit(F, fop, vr, a, b, 0);
            ir_set_double(vr);
        } else {
            ir_emit(F, op, vr, a, b, 0);
        }
        a = vr;
    }
    return a;
}

static int ir_add(CompilerState *cs, int line)
{
    /* 阶段 1 指针算术：peek 预判左操作数是指向标量的指针变量（pt 记录），
     * 加法右操作数缩放 ×8（IR 槽模型：一个槽 = 8 字节，p+1 = 下一个槽，
     * 与数组寻址 &arr[k]=base+k*8 一致；指向宽度不缩放——槽统一 8 字节）。
     * 例：p = &x; q = p + 2  →  q = p + 16 字节 */
    int pvi = -1;
    {
        LexerState *lx = cs->parser.lex;
        lx->peek_valid = 0;
        lexer_peek(lx);
        if (lx->peek_tok == TOK_IDENTIFIER) {
            int tv = var_find(lx->peek_str);
            if (tv >= 0 && PT_IS_SCALAR(pt[tv])) pvi = tv;
        }
        lx->peek_valid = 0;
    }
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
        if (pvi >= 0 && op == IR_ADD && !ir_is_double(a) && !ir_is_double(b)) {
            /* 指针 + k：k × 8（槽宽）。只对首个加法项生效（p+k 后不再是"指针语义"，
             * 链式 p+k+1 保持字节语义——文档化，与 C 的连续指针算术不同） */
            int wv = ir_new_vreg(F);
            ir_emit(F, IR_CONST, wv, -1, -1, 8);
            int off = ir_new_vreg(F);
            ir_emit(F, IR_MUL, off, b, wv, 0);
            b = off;
            pvi = -1;   /* 只缩放一次 */
        }
        int vr = ir_new_vreg(F);
        /* PB-13：任一侧为 double → 浮点运算；混合类型先提升 int 侧 */
        if (ir_is_double(a) || ir_is_double(b)) {
            if (!ir_is_double(a)) a = ir_to_double(a);
            if (!ir_is_double(b)) b = ir_to_double(b);
            ir_emit(F, (op == IR_ADD) ? IR_FADD : IR_FSUB, vr, a, b, 0);
            ir_set_double(vr);
        } else {
            ir_emit(F, op, vr, a, b, 0);
        }
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
        /* PB-13：任一侧为 double → 浮点比较（imm 编码 0=EQ 1=NE 2=LT 3=LE 4=GT 5=GE）；
         * 混合类型先提升 int 侧 */
        if (ir_is_double(a) || ir_is_double(b)) {
            if (!ir_is_double(a)) a = ir_to_double(a);
            if (!ir_is_double(b)) b = ir_to_double(b);
            int fcmp = (op == IR_CMP_EQ) ? 0 : (op == IR_CMP_NE) ? 1 :
                       (op == IR_CMP_LT) ? 2 : (op == IR_CMP_LE) ? 3 :
                       (op == IR_CMP_GT) ? 4 : 5;
            ir_emit(F, IR_FCMP, vr, a, b, fcmp);
        } else {
            ir_emit(F, op, vr, a, b, 0);
        }
        a = vr;
    }
}

/* a && b：短路（a==0 跳过 b 求值）；结果 = b != 0 */
static int ir_logical_and(CompilerState *cs, int line)
{
    int a = ir_cmp(cs, line);
    for (;;) {
        TokenType t = cur_tok(cs);
        if (cs->parser.lex->last_line_num != line) return a;
        if (t != TOK_LOGICAL_AND) return a;
        next_tok(cs);
        int b = ir_cmp(cs, line);
        int res = ir_new_vreg(F);
        int l_false = ir_new_label(F);
        int l_done = ir_new_label(F);
        ir_emit(F, IR_JZ, -1, a, -1, 0);
        F->ins[F->ins_count - 1].label = l_false;
        int zero = ir_new_vreg(F);
        ir_emit(F, IR_CONST, zero, -1, -1, 0);
        int nb = ir_new_vreg(F);
        ir_emit(F, IR_CMP_NE, nb, b, zero, 0);
        ir_emit(F, IR_MOV, res, nb, -1, 0);
        ir_emit(F, IR_JMP, -1, -1, -1, 0);
        F->ins[F->ins_count - 1].label = l_done;
        ir_emit(F, IR_LABEL, -1, -1, -1, 0);
        F->ins[F->ins_count - 1].label = l_false;
        ir_emit(F, IR_CONST, res, -1, -1, 0);
        ir_emit(F, IR_LABEL, -1, -1, -1, 0);
        F->ins[F->ins_count - 1].label = l_done;
        a = res;
    }
}

/* a || b：短路（a!=0 跳过 b 求值）；结果 = b != 0 */
static int ir_logical_or(CompilerState *cs, int line)
{
    int a = ir_logical_and(cs, line);
    for (;;) {
        TokenType t = cur_tok(cs);
        if (cs->parser.lex->last_line_num != line) return a;
        if (t != TOK_LOGICAL_OR) return a;
        next_tok(cs);
        int b = ir_logical_and(cs, line);
        int res = ir_new_vreg(F);
        int l_true = ir_new_label(F);
        int l_done = ir_new_label(F);
        ir_emit(F, IR_JNZ, -1, a, -1, 0);
        F->ins[F->ins_count - 1].label = l_true;
        int zero = ir_new_vreg(F);
        ir_emit(F, IR_CONST, zero, -1, -1, 0);
        int nb = ir_new_vreg(F);
        ir_emit(F, IR_CMP_NE, nb, b, zero, 0);
        ir_emit(F, IR_MOV, res, nb, -1, 0);
        ir_emit(F, IR_JMP, -1, -1, -1, 0);
        F->ins[F->ins_count - 1].label = l_done;
        ir_emit(F, IR_LABEL, -1, -1, -1, 0);
        F->ins[F->ins_count - 1].label = l_true;
        ir_emit(F, IR_CONST, res, -1, -1, 1);
        ir_emit(F, IR_LABEL, -1, -1, -1, 0);
        F->ins[F->ins_count - 1].label = l_done;
        a = res;
    }
}

static int ir_expr(CompilerState *cs)
{
    /* 表达式首 token 的行号 = 语句起始行，用于块内换行边界判定 */
    int line = cs->parser.lex->last_line_num;
    int a = ir_logical_or(cs, line);
    if (cur_tok(cs) != TOK_QUESTION) return a;
    /* 三元条件 c ? a : b：
     *   JZ c, L_else → a → MOV r, a → JMP L_end → L_else: b → MOV r, b → L_end
     * 类型：两分支任一 double → r 标 double 且另一分支 ITOD 提升；
     *      真分支 int / 假分支 double（不匹配方向）→ 报错（语义验证器不静默错值） */
    next_tok(cs);
    int l_else = ir_new_label(F);
    int l_end = ir_new_label(F);
    ir_emit(F, IR_JZ, -1, a, -1, 0);
    F->ins[F->ins_count - 1].label = l_else;
    int tv = ir_expr(cs);           /* 真分支（支持嵌套三元/完整表达式） */
    int r = ir_new_vreg(F);
    if (ir_is_double(tv)) ir_set_double(r);
    ir_emit(F, IR_MOV, r, tv, -1, 0);
    ir_emit(F, IR_JMP, -1, -1, -1, 0);
    F->ins[F->ins_count - 1].label = l_end;
    ir_emit(F, IR_LABEL, -1, -1, -1, 0);
    F->ins[F->ins_count - 1].label = l_else;
    expect(cs, TOK_COLON);
    int fv = ir_expr(cs);           /* 假分支 */
    if (ir_is_double(fv) && !ir_is_double(tv)) {
        nihao_error(cs, "ir: ternary branch type mismatch (true=int, false=double)");
        return r;
    }
    if (ir_is_double(fv)) ir_set_double(r);   /* tv 也是 double（上面已排除不匹配） */
    if (!ir_is_double(fv) && ir_is_double(r)) fv = ir_to_double(fv);
    ir_emit(F, IR_MOV, r, fv, -1, 0);
    ir_emit(F, IR_LABEL, -1, -1, -1, 0);
    F->ins[F->ins_count - 1].label = l_end;
    return r;
}

/* ---- 语句 ---- */
static void ir_stmt(CompilerState *cs);
static void ir_stmt_impl(CompilerState *cs);

/* ============================================================
 * 编译期常量求值（PB-9：static_assert / cooking）
 * 直接递归下降求值，不生成 IR。支持：int 字面量、一元 -/!、
 * 四则/取模、比较、&&/||、括号、enum 常量、sizeof(type)、
 * visof(x)、可见性枚举 _const 等。
 * ============================================================ */
static long long ir_const_or(CompilerState *cs);
static long long ir_const_prim(CompilerState *cs)
{
    TokenType t = cur_tok(cs);
    if (t == TOK_INT_CONST) {
        long long v = cs->parser.lex->tok_val.i;
        next_tok(cs);
        return v;
    }
    if (t == TOK_TRUE) { next_tok(cs); return 1; }
    if (t == TOK_FALSE) { next_tok(cs); return 0; }
    if (t == TOK_LPAREN) {
        next_tok(cs);
        long long v = ir_const_or(cs);
        expect(cs, TOK_RPAREN);
        return v;
    }
    if (t == TOK_SIZEOF) {
        next_tok(cs);
        expect(cs, TOK_LPAREN);
        long long sz = type_size_of(cs, NULL);
        expect(cs, TOK_RPAREN);
        return sz < 0 ? 0 : sz;
    }
    if (t == TOK_VISOF) {
        next_tok(cs);
        expect(cs, TOK_LPAREN);
        long long v = VIS_UNDEF;
        if (cur_tok(cs) == TOK_IDENTIFIER) {
            int vi = var_find(cs->parser.lex->tok_str);
            if (vi >= 0) v = vvis[vi];
            next_tok(cs);
        }
        expect(cs, TOK_RPAREN);
        return v;
    }
    if (t == TOK__UNDEF || t == TOK__CONST || t == TOK__FLOW ||
        t == TOK__STATIC || t == TOK__VAR) {
        next_tok(cs);
        return (t == TOK__UNDEF) ? VIS_UNDEF :
               (t == TOK__CONST) ? VIS_CONST :
               (t == TOK__FLOW)  ? VIS_FLOW :
               (t == TOK__STATIC)? VIS_STATIC : VIS_VAR;
    }
    if (t == TOK_IDENTIFIER) {
        const char *name = cs->parser.lex->tok_str;
        long long eval = 0;
        /* 编译期函数调用（cooking-call）：NAME(args) → 宏式展开求值 */
        {
            int cf = ct_func_find(name);
            if (cf >= 0) {
                LexerState *lx = cs->parser.lex;
                lx->peek_valid = 0;
                lexer_peek(lx);
                if (lx->peek_tok == TOK_LPAREN) {
                    next_tok(cs);
                    next_tok(cs);           /* name ( */
                    long long args[IR_MAX_CTFPARAM];
                    int ac = 0;
                    if (cur_tok(cs) != TOK_RPAREN) {
                        for (;;) {
                            if (ac >= IR_MAX_CTFPARAM) {
                                nihao_error(cs, "ir: too many args to cooking function");
                                break;
                            }
                            args[ac++] = ir_const_expr(cs);
                            if (cur_tok(cs) != TOK_COMMA) break;
                            next_tok(cs);
                        }
                    }
                    expect(cs, TOK_RPAREN);
                    return ct_func_call(cs, cf, args, ac);
                }
                lx->peek_valid = 0;
            }
        }
        if (ct_var_exist(name)) {        /* cooking 编译期变量优先 */
            next_tok(cs);
            return ct_var_find(name);
        }
        if (enum_const_find(name, &eval)) {
            next_tok(cs);
            return eval;
        }
        nihao_error(cs, "ir: constant expression: unknown identifier '%s'", name);
        next_tok(cs);
        return 0;
    }
    nihao_error(cs, "ir: constant expression: unexpected token '%s'", token_name(t));
    next_tok(cs);
    return 0;
}

static long long ir_const_unary(CompilerState *cs)
{
    TokenType t = cur_tok(cs);
    if (t == TOK_MINUS) { next_tok(cs); return -ir_const_unary(cs); }
    if (t == TOK_LOGICAL_NOT) { next_tok(cs); return !ir_const_unary(cs); }
    if (t == TOK_BITWISE_NOT) { next_tok(cs); return ~ir_const_unary(cs); }
    return ir_const_prim(cs);
}

static long long ir_const_mul(CompilerState *cs)
{
    long long a = ir_const_unary(cs);
    for (;;) {
        TokenType t = cur_tok(cs);
        if (t == TOK_STAR) { next_tok(cs); a = a * ir_const_unary(cs); }
        else if (t == TOK_SLASH) { next_tok(cs); long long d = ir_const_unary(cs); a = d ? a / d : 0; }
        else if (t == TOK_PERCENT) { next_tok(cs); long long d = ir_const_unary(cs); a = d ? a % d : 0; }
        else return a;
    }
}

static long long ir_const_add(CompilerState *cs)
{
    long long a = ir_const_mul(cs);
    for (;;) {
        TokenType t = cur_tok(cs);
        if (t == TOK_PLUS) { next_tok(cs); a = a + ir_const_mul(cs); }
        else if (t == TOK_MINUS) { next_tok(cs); a = a - ir_const_mul(cs); }
        else return a;
    }
}

static long long ir_const_rel(CompilerState *cs)
{
    long long a = ir_const_add(cs);
    for (;;) {
        TokenType t = cur_tok(cs);
        if (t == TOK_LT) { next_tok(cs); a = (a < ir_const_add(cs)); }
        else if (t == TOK_GT) { next_tok(cs); a = (a > ir_const_add(cs)); }
        else if (t == TOK_LE) { next_tok(cs); a = (a <= ir_const_add(cs)); }
        else if (t == TOK_GE) { next_tok(cs); a = (a >= ir_const_add(cs)); }
        else return a;
    }
}

static long long ir_const_eq(CompilerState *cs)
{
    long long a = ir_const_rel(cs);
    for (;;) {
        TokenType t = cur_tok(cs);
        if (t == TOK_EQ) { next_tok(cs); a = (a == ir_const_rel(cs)); }
        else if (t == TOK_NE) { next_tok(cs); a = (a != ir_const_rel(cs)); }
        else return a;
    }
}

static long long ir_const_and(CompilerState *cs)
{
    long long a = ir_const_eq(cs);
    while (cur_tok(cs) == TOK_LOGICAL_AND) { next_tok(cs); a = a && ir_const_eq(cs); }
    return a;
}

static long long ir_const_or(CompilerState *cs)
{
    long long a = ir_const_and(cs);
    while (cur_tok(cs) == TOK_LOGICAL_OR) { next_tok(cs); a = a || ir_const_and(cs); }
    return a;
}

static long long ir_const_expr(CompilerState *cs)
{
    return ir_const_or(cs);
}

/* static_assert(expr, "msg")：编译期断言 */
static void ir_static_assert(CompilerState *cs)
{
    next_tok(cs);               /* static_assert */
    expect(cs, TOK_LPAREN);
    long long v = ir_const_expr(cs);
    expect(cs, TOK_COMMA);
    const char *msg = "";
    if (cur_tok(cs) == TOK_STRING_LITERAL) {
        msg = cs->parser.lex->tok_str;
        next_tok(cs);
    }
    expect(cs, TOK_RPAREN);
    if (!v) {
        nihao_error(cs, "ir: static_assert failed: %s", msg);
    }
}

/* 跳过当前 cooking item 的剩余部分：词法器在括号与花括号内不发换行 token，故以 item
 * 的起始行界定，换到下一行、遇到 `;` / `#` 终止符或块尾即停。与 A 后端 skip_cooking_item
 * 同口径；用 last_line_num（当前 token 所在行）而非 line_num（缓冲区位置，滞后一个 token）。 */
static void ir_skip_cooking_item(CompilerState *cs, int start_line)
{
    while (cur_tok(cs) != TOK_SEMICOLON && cur_tok(cs) != TOK_POUND &&
           cur_tok(cs) != TOK_RBRACE && cur_tok(cs) != TOK_EOF &&
           cs->parser.lex->last_line_num == start_line)
        next_tok(cs);
}

/* 截取编译期函数体源文本：mark 是 '=' 之后的缓冲区位置，扫到行尾 / `;` / `#` / `}` 为止。
 * 必须按字符扫而非按 token 流末位定位——旧写法在 next_tok 消费掉 `=` 之后才取 mark，
 * 那时词法器已扫过体的首个 token，首 token 被静默丢掉（`x * x` 变成 `* x`）；而其
 * end = buf_ptr 又要等 token 流走到换行/块尾，花括号内不发换行 token，于是一路吞掉
 * 同块后续 item。返回长度，空体返回 0，超长返回 -1。 */
static int ir_capture_body(CompilerState *cs, const char *mark, char *dst, int dst_sz)
{
    const char *p = mark;
    const char *end = cs->parser.lex->buf_end;
    while (p < end && *p != '\0' && *p != '\n' && *p != ';' && *p != '#' && *p != '}') p++;
    while (p > mark && (p[-1] == ' ' || p[-1] == '\t' || p[-1] == '\r')) p--;
    while (p > mark && (*mark == ' ' || *mark == '\t')) mark++;
    int n = (int)(p - mark);
    if (n <= 0) { dst[0] = '\0'; return 0; }
    if (n >= dst_sz) return -1;
    memcpy(dst, mark, (size_t)n);
    dst[n] = '\0';
    return n;
}

/* cooking { ... }：编译期块——执行 static_assert；`const NAME [TYPE] = expr`
 * 声明编译期变量（ct_vars 表，跨块共享）；其余 item 跳过 */
static void ir_cooking(CompilerState *cs)
{
    next_tok(cs);               /* cooking */
    expect(cs, TOK_LBRACE);
    skip_newlines(cs);
    while (cur_tok(cs) != TOK_RBRACE && cur_tok(cs) != TOK_EOF) {
        if (cur_tok(cs) == TOK_IDENTIFIER &&
            strcmp(cs->parser.lex->tok_str, "static_assert") == 0) {
            ir_static_assert(cs);
        } else if (cur_tok(cs) == TOK_CONST) {
            /* 编译期常量：const NAME [TYPE] = expr → 存 ct_vars */
            next_tok(cs);
            if (cur_tok(cs) != TOK_IDENTIFIER) {
                nihao_error(cs, "ir: cooking const: expected name");
                while (cur_tok(cs) != TOK_NEWLINE && cur_tok(cs) != TOK_RBRACE &&
                       cur_tok(cs) != TOK_EOF) next_tok(cs);
            } else {
                const char *cname = cs->parser.lex->tok_str;
                next_tok(cs);
                if (is_type_token(cur_tok(cs))) next_tok(cs);   /* 可选类型 */
                if (cur_tok(cs) == TOK_LPAREN) {
                    /* 编译期函数定义：const NAME(p1, p2) = expr（宏式展开） */
                    int cf = ct_funcs_count;
                    next_tok(cs);
                    int pc = 0;
                    while (cur_tok(cs) != TOK_RPAREN && cur_tok(cs) != TOK_EOF &&
                           pc < IR_MAX_CTFPARAM) {
                        if (cur_tok(cs) == TOK_IDENTIFIER) {
                            snprintf(ct_funcs[cf].params[pc], 64, "%s",
                                     cs->parser.lex->tok_str);
                            pc++;
                            next_tok(cs);
                        } else {
                            next_tok(cs);
                        }
                        if (cur_tok(cs) == TOK_COMMA) next_tok(cs);
                    }
                    expect(cs, TOK_RPAREN);
                    if (cur_tok(cs) == TOK_ASSIGN) {
                        /* mark 取在消费 `=` 之前：此刻 buf_ptr 正好停在 '=' 之后 */
                        char *mark = cs->parser.lex->buf_ptr;
                        int body_line = cs->parser.lex->line_num;
                        int n = ir_capture_body(cs, mark, ct_funcs[cf].expr_src,
                                                (int)sizeof(ct_funcs[cf].expr_src));
                        if (n > 0) {
                            ct_funcs[cf].name = cname;
                            ct_funcs[cf].param_count = pc;
                            ct_funcs_count++;
                        } else if (n == 0) {
                            nihao_error(cs, "ir: cooking function '%s' has empty body", cname);
                        } else {
                            nihao_error(cs, "ir: cooking function body too long");
                        }
                        next_tok(cs);               /* '=' → 进入体，按行跳到行尾 */
                        ir_skip_cooking_item(cs, body_line);
                    } else {
                        nihao_error(cs, "ir: cooking function: expected '='");
                    }
                } else if (cur_tok(cs) == TOK_ASSIGN) {
                    next_tok(cs);
                    long long v = ir_const_expr(cs);
                    if (ct_var_exist(cname)) {
                        nihao_error(cs, "ir: cooking const '%s' redefined", cname);
                    } else if (ct_vars_count < 64) {
                        ct_vars[ct_vars_count].name = cname;
                        ct_vars[ct_vars_count].val = v;
                        ct_vars_count++;
                    }
                } else {
                    nihao_error(cs, "ir: cooking const: expected '='");
                }
            }
        } else {
            /* PB-84：不支持的 cooking item（如裸 var-decl）即时报错并只跳过该 item */
            int cline = cs->parser.lex->line_num;
            nihao_error(cs, "ir: unsupported cooking item");
            ir_skip_cooking_item(cs, cline);
        }
        skip_newlines(cs);
    }
    expect(cs, TOK_RBRACE);
    skip_newlines(cs);
}

/* align N { ... }：对齐块——IR 8 字节槽模型下无实际意义，跳过块体 */
static void ir_align_block(CompilerState *cs)
{
    next_tok(cs);               /* align */
    if (cur_tok(cs) == TOK_INT_CONST) next_tok(cs);   /* N */
    expect(cs, TOK_LBRACE);
    int depth = 1;
    while (depth > 0 && cur_tok(cs) != TOK_EOF) {
        if (cur_tok(cs) == TOK_LBRACE) depth++;
        else if (cur_tok(cs) == TOK_RBRACE) depth--;
        if (depth > 0) next_tok(cs);
    }
    expect(cs, TOK_RBRACE);
    skip_newlines(cs);
}

/* 多变量声明：var {a = e0, b = e1, ...} Type [N]
 * 仅在前缀 var/const/static/flow 后出现（无前缀 { 是块语句）。 */
static void ir_multi_decl(CompilerState *cs, int vis)
{
    next_tok(cs);   /* { */
    const char *mnames[32];
    int minits[32];
    int misstr[32];      /* PB-62：该名的初值是否字符串字面量（定长数组只收这个形态） */
    int mslen[32];       /* 字面量长度（不含结尾 NUL） */
    const char *mstrs[32];  /* PB-82：字面量文本（紧凑数组逐字节写入用） */
    int mc = 0;
    skip_newlines(cs);
    while (cur_tok(cs) != TOK_RBRACE && cur_tok(cs) != TOK_EOF && mc < 32) {
        if (cur_tok(cs) != TOK_IDENTIFIER) {
            nihao_error(cs, "ir: expected variable name in multi-declaration");
            next_tok(cs);
            continue;
        }
        mnames[mc] = cs->parser.lex->tok_str;
        next_tok(cs);
        expect(cs, TOK_ASSIGN);
        misstr[mc] = (cur_tok(cs) == TOK_STRING_LITERAL);
        mslen[mc] = misstr[mc] ? (int)strlen(cs->parser.lex->tok_str) : 0;
        mstrs[mc] = misstr[mc] ? nihao_strdup(g_cs, cs->parser.lex->tok_str) : NULL;
        minits[mc] = ir_expr(cs);
        mc++;
        if (cur_tok(cs) != TOK_COMMA) break;
        next_tok(cs);
        skip_newlines(cs);
    }
    expect(cs, TOK_RBRACE);
    /* 类型：基本类型或聚合类型，可带 [N] 数组 */
    int elems = 0, ti = -1, arr_cap = 0, elt_char = 0, has_bracket = 0;
    int mdelem = 8;
    if (is_type_token(cur_tok(cs))) {
        elt_char = (cur_tok(cs) == TOK_CHAR || cur_tok(cs) == TOK_I8);
        mdelem = (int)type_size_token(cur_tok(cs));
        if (!(mdelem == 1 || mdelem == 2 || mdelem == 4 || mdelem == 8)) mdelem = 8;
        next_tok(cs);
    } else if (cur_tok(cs) == TOK_IDENTIFIER) {
        ti = agg_type_find(cs->parser.lex->tok_str);
        if (ti >= 0) {
            next_tok(cs);
            elems = (agg_types[ti].kind == 2) ? 1 : agg_types[ti].mslots;
        } else {
            nihao_error(cs, "ir: unknown type '%s' in multi-declaration",
                        cs->parser.lex->tok_str);
            next_tok(cs);
        }
    } else {
        nihao_error(cs, "ir: expected type after multi-variable declaration");
    }
    if (cur_tok(cs) == TOK_LBRACKET) {
        has_bracket = 1;
        next_tok(cs);
        if (cur_tok(cs) == TOK_INT_CONST) {
            elems = (int)cs->parser.lex->tok_val.i;
            arr_cap = elems;
            next_tok(cs);
        } else if (cur_tok(cs) == TOK_ELLIPSIS) {
            next_tok(cs);           /* `...` = 默认容量 8（PB-67 同口径） */
        }
        expect(cs, TOK_RBRACKET);
    }
    /* PB-62：定长数组的多变量声明只支持字符串字面量初值——此前直接 IR_MOV，
     * 等于静默丢掉 `[N]` 生成 `int x = 1` 这类错误 C（台账 E-c 桶） */
    if (arr_cap > 0) {
        for (int k = 0; k < mc; k++) {
            if (!misstr[k]) {
                nihao_error(cs, "ir: '%s' has fixed array type but no string literal "
                                "initializer; use a single-variable declaration for this form",
                            mnames[k]);
                continue;
            }
            if (!elt_char) {
                nihao_error(cs, "ir: cannot initialize array '%s' with a string literal; "
                                "its elements are not 'char', use a value list {v0, v1, ...}",
                            mnames[k]);
            }
            if (mslen[k] + 1 > arr_cap) {
                nihao_error(cs, "ir: string needs %d bytes with terminator, "
                                "array '%s' holds %d",
                            mslen[k] + 1, mnames[k], arr_cap);
            }
        }
    }
    for (int k = 0; k < mc; k++) {
        int vi = var_declare(mnames[k], elems, ti, vis,
                             (elems > 0 && ti < 0) ? mdelem : 0);
        if (arr_cap > 0 && misstr[k]) {
            /* PB-82：定长 char[N] 多变量声明——逐字节写入（含 NUL），紧凑存储 */
            const char *lit = mstrs[k];
            int cap = mslen[k] + 1;
            if (cap > arr_cap) cap = arr_cap;
            for (int j = 0; j < cap; j++) {
                int cv = ir_new_vreg(F);
                ir_emit(F, IR_CONST, cv, -1, -1,
                        (unsigned char)(j < mslen[k] ? lit[j] : 0));
                int addr = ir_arr_const_addr(vi, j);
                ir_arr_store(addr, cv, 1);
            }
        } else if (has_bracket && arr_cap == 0 && misstr[k]) {
            /* PB-82：动态 `char[]` 多变量声明——登记长度与字节寻址（与单变量同口径）
             * PB-89：字面量复制到可写内存（元素写须成立） */
            vetyp[vi] = 8;
            ve[vi] = mslen[k];
            vlen_known[vi] = 1;
            int cp = (vvis[vi] == VIS_CONST) ? minits[k]
                                             : ir_strlit_copy(cs, mstrs[k]);
            ir_emit(F, IR_MOV, vt[vi], cp, -1, 0);
        } else {
            ir_emit(F, IR_MOV, vt[vi], minits[k], -1, 0);
        }
    }
    skip_newlines(cs);
}

static void ir_block(CompilerState *cs)
{
    expect(cs, TOK_LBRACE);
    int scope_start = vn_count;   /* 本块内新声明的变量起点（作用域退出解冻借用源） */
    skip_newlines(cs);
    while (cur_tok(cs) != TOK_RBRACE && cur_tok(cs) != TOK_EOF) {
        ir_stmt(cs);
        skip_newlines(cs);
    }
    expect(cs, TOK_RBRACE);
    /* 解冻：本块内声明的借用变量的源（借用随作用域结束失效，对齐 vis_unfreeze_borrows） */
    for (int vi = scope_start; vi < vn_count; vi++)
        if (vborrow_src[vi] >= 0) vstate[vborrow_src[vi]] = IR_VS_VALID;
    skip_newlines(cs);
}

/* if / else 语句（BNF §6 <if-stmt>）：`else` 后可接块或另一条 if（else if 链） */
static void ir_if_stmt(CompilerState *cs)
{
    next_tok(cs);                 /* if */
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
        if (cur_tok(cs) == TOK_IF) ir_if_stmt(cs);
        else ir_block(cs);
    }
    ir_emit(F, IR_LABEL, -1, -1, -1, 0);
    F->ins[F->ins_count - 1].label = l_end;
}

/* BNF §1.4 / §6 <empty-stmt> ::= ";" | "#"：语句尾的可选终止符。
 * 与 A 后端 eat_stmt_terminator 同口径——只吃一个，连续的空语句由 ir_stmt_impl 入口处理。 */
static void ir_eat_stmt_terminator(CompilerState *cs)
{
    if (cur_tok(cs) == TOK_SEMICOLON || cur_tok(cs) == TOK_POUND) next_tok(cs);
    skip_newlines(cs);
}

/* PB-70：终止符包装层——语句本体与「尾随终止符」分开，避免在几十个出口各补一次。 */
static void ir_stmt(CompilerState *cs)
{
    ir_stmt_impl(cs);
    ir_eat_stmt_terminator(cs);
}

static void ir_stmt_impl(CompilerState *cs)
{
    TokenType t = cur_tok(cs);
    /* PB-70：空语句 `;` / `#` 可单独成句 */
    if (t == TOK_SEMICOLON || t == TOK_POUND) {
        next_tok(cs);
        skip_newlines(cs);
        return;
    }
    /* 声明前缀：const/static/flow/var 记录可见性后继续解析声明 */
    int decl_vis = VIS_VAR;
    int has_prefix = 0;
    if (t == TOK_CONST)   { decl_vis = VIS_CONST;  has_prefix = 1; next_tok(cs); t = cur_tok(cs); }
    else if (t == TOK_FLOW)   { decl_vis = VIS_FLOW;   has_prefix = 1; next_tok(cs); t = cur_tok(cs); }
    else if (t == TOK_STATIC) { decl_vis = VIS_STATIC; has_prefix = 1; next_tok(cs); t = cur_tok(cs); }
    else if (t == TOK_VAR)    { decl_vis = VIS_VAR;    has_prefix = 1; next_tok(cs); t = cur_tok(cs); }
    /* 标签：name:（C 风格，lexer 注释 label suffix；peek 下一 token） */
    if (t == TOK_IDENTIFIER && !has_prefix) {
        LexerState *lx = cs->parser.lex;
        lx->peek_valid = 0;
        lexer_peek(lx);
        if (lx->peek_tok == TOK_COLON) {
            int lid = ir_label_get(lx->tok_str);
            next_tok(cs);
            next_tok(cs);           /* name : */
            ir_emit(F, IR_LABEL, -1, -1, -1, 0);
            F->ins[F->ins_count - 1].label = lid;
            skip_newlines(cs);
            return;
        }
        lx->peek_valid = 0;
    }
    if (t == TOK_GOTO) {
        /* goto name → IR_JMP（label 首引用即分配，无需先定义） */
        next_tok(cs);
        if (cur_tok(cs) == TOK_IDENTIFIER) {
            int lid = ir_label_get(cs->parser.lex->tok_str);
            next_tok(cs);
            ir_emit(F, IR_JMP, -1, -1, -1, 0);
            F->ins[F->ins_count - 1].label = lid;
        } else {
            nihao_error(cs, "ir: expected label name after 'goto'");
        }
        skip_newlines(cs);
        return;
    }
    if (t == TOK_IF) {
        ir_if_stmt(cs);
    } else if (t == TOK_SWITCH) {
        /* C 风格：switch (expr) { case e: stmts... [default: stmts] }
         * 单遍布局（延迟绑定）：
         *   v = expr
         *   L_c1: c1 = (v==e1); JZ c1, L_c2; stmts1; JMP L_end
         *   L_c2: c2 = (v==e2); JZ c2, L_def/L_end; stmts2; JMP L_end
         *   ...
         *   L_end:
         * break 跳出 switch（break=loop_end 压栈）；continue 非法（cont=-1 哨兵） */
        next_tok(cs);
        expect(cs, TOK_LPAREN);
        int v = ir_expr(cs);
        expect(cs, TOK_RPAREN);
        expect(cs, TOK_LBRACE);
        int l_end = ir_new_label(F);
        int l_cur = ir_new_label(F);
        int pending_jz = -1;            /* 未绑定跳转目标的 JZ 指令索引 */
        loop_end_label[loop_depth] = l_end;
        loop_cont_label[loop_depth] = -1;   /* switch 内 continue 非法 */
        loop_depth++;
        for (;;) {
            skip_newlines(cs);
            TokenType st = cur_tok(cs);
            if (st == TOK_RBRACE) break;
            if (pending_jz >= 0) {      /* 绑定上一个不匹配跳转到当前检查入口 */
                F->ins[pending_jz].label = l_cur;
                pending_jz = -1;
            }
            if (st == TOK_CASE) {
                next_tok(cs);
                ir_emit(F, IR_LABEL, -1, -1, -1, 0);
                F->ins[F->ins_count - 1].label = l_cur;
                int e = ir_expr(cs);
                expect(cs, TOK_COLON);
                int c = ir_new_vreg(F);
                ir_emit(F, IR_CMP_EQ, c, v, e, 0);
                ir_emit(F, IR_JZ, -1, c, -1, 0);
                pending_jz = F->ins_count - 1;
                skip_newlines(cs);
                while (cur_tok(cs) != TOK_CASE && cur_tok(cs) != TOK_DEFAULT &&
                       cur_tok(cs) != TOK_RBRACE) {
                    ir_stmt(cs);
                    skip_newlines(cs);
                }
                ir_emit(F, IR_JMP, -1, -1, -1, 0);
                F->ins[F->ins_count - 1].label = l_end;
                l_cur = ir_new_label(F);
            } else if (st == TOK_DEFAULT) {
                next_tok(cs);
                expect(cs, TOK_COLON);
                ir_emit(F, IR_LABEL, -1, -1, -1, 0);
                F->ins[F->ins_count - 1].label = l_cur;
                skip_newlines(cs);
                while (cur_tok(cs) != TOK_RBRACE) {
                    ir_stmt(cs);
                    skip_newlines(cs);
                }
                break;
            } else {
                nihao_error(cs, "ir: expected 'case' or 'default' in switch");
                next_tok(cs);
                skip_newlines(cs);
                continue;
            }
        }
        if (pending_jz >= 0)            /* 无 default 时最后一个 case 不匹配 → 结束 */
            F->ins[pending_jz].label = l_end;
        expect(cs, TOK_RBRACE);
        ir_emit(F, IR_LABEL, -1, -1, -1, 0);
        F->ins[F->ins_count - 1].label = l_end;
        loop_depth--;
    } else if (t == TOK_WHILE || t == TOK_DO) {
        /* NihaoC: while/do 均为前测循环（do 是 while 的别名关键字）。
         * 条件值存 is_val_vreg，体内 `is pat { }` 匹配该值。 */
        int is_do = (t == TOK_DO);
        next_tok(cs);
        int l_loop = ir_new_label(F);
        int l_end = ir_new_label(F);
        loop_end_label[loop_depth] = l_end;
        loop_cont_label[loop_depth] = l_loop;
        loop_depth++;
        ir_emit(F, IR_LABEL, -1, -1, -1, 0);
        F->ins[F->ins_count - 1].label = l_loop;
        int c = ir_expr(cs);
        ir_emit(F, IR_JZ, -1, c, -1, 0);
        F->ins[F->ins_count - 1].label = l_end;
        int save_is = is_val_vreg;
        int save_matched = is_matched_vreg;
        /* PB-27.1：do 循环不设置 is_val_vreg（do 不支持 is 模式匹配，
         * 对齐 2026-09-01 is 规范：is 仅配合 while）。
         * 若 do 体内出现 is，会触发 is_val_vreg<0 报错（下方 §"ir: do loop does not support 'is' pattern"）。 */
        if (!is_do) {
            is_val_vreg = c;            /* 仅 while 设置条件值 */
            /* PB-71：每轮迭代重置首匹配标记（与 A 侧 `__is_matched = 0;` 同位置：
             * 条件检查之后、body 之前）。槽只分配一次，重置在循环体内。 */
            is_matched_vreg = ir_new_vreg(F);
            int zero = ir_new_vreg(F);
            ir_emit(F, IR_CONST, zero, -1, -1, 0);
            ir_emit(F, IR_MOV, is_matched_vreg, zero, -1, 0);
        }
        ir_block(cs);
        is_val_vreg = save_is;
        is_matched_vreg = save_matched;
        ir_emit(F, IR_JMP, -1, -1, -1, 0);
        F->ins[F->ins_count - 1].label = l_loop;
        ir_emit(F, IR_LABEL, -1, -1, -1, 0);
        F->ins[F->ins_count - 1].label = l_end;
        loop_depth--;
        (void)is_do;
    } else if (t == TOK_FOR) {
        /* for init; cond; step { body }
         * IR 布局：cond 检查 → body → L_cont(step) → JMP cond
         * 源码 step 在 body 前解析，但指令需在 body 后发射 → 先记录后重放 */
        next_tok(cs);
        /* ---- for-init（BNF §6）：<identifier> "=" <expr> | <var-decl>（name Type
         * / name (Type)）| 任意表达式 ---- */
        if (cur_tok(cs) == TOK_IDENTIFIER) {
            TokenType nt = ir_peek_ahead(cs, 1);
            /* 「括号类型」`(i32)` 与函数调用 `f(x)` 的区分：括号内是类型名且紧跟 ) */
            int paren_type = (nt == TOK_LPAREN) &&
                             is_type_token(ir_peek_ahead(cs, 2)) &&
                             ir_peek_ahead(cs, 3) == TOK_RPAREN;
            if (nt == TOK_ASSIGN || is_type_token(nt) || paren_type) {
                const char *vname = cs->parser.lex->tok_str;
                next_tok(cs);           /* name */
                int vcode = 0;
                int assign_form = 0;    /* 1 = `name = expr`（无类型，沿用已有变量类型） */
                if (nt == TOK_ASSIGN) {
                    next_tok(cs);       /* = */
                    assign_form = 1;
                } else if (paren_type) {
                    next_tok(cs);       /* ( */
                    vcode = ir_scalar_code_of_token(cur_tok(cs));
                    next_tok(cs);       /* 类型名 */
                    expect(cs, TOK_RPAREN);
                } else {
                    vcode = ir_scalar_code_of_token(cur_tok(cs));
                    next_tok(cs);       /* 类型名 */
                }
                int vi = var_find(vname);
                if (vi < 0) vi = var_declare(vname, 0, -1, VIS_VAR, 0);
                if (vcode != 0) {
                    vtype[vi] = vcode;
                    if (vcode == 1 || vcode == 8) ir_set_double(vt[vi]);
                }
                /* 初值：赋值形式即当前位置；声明形式可选 `= expr` */
                if (assign_form || cur_tok(cs) == TOK_ASSIGN) {
                    if (!assign_form) next_tok(cs);   /* 消费 = */
                    int v = ir_expr(cs);
                    v = ir_coerce(v, vtype[vi]);
                    ir_emit(F, IR_MOV, vt[vi], v, -1, 0);
                }
            } else {
                ir_expr(cs);            /* 表达式形式 */
            }
        } else {
            ir_expr(cs);
        }
        expect(cs, TOK_SEMICOLON);
        int l_cond = ir_new_label(F);
        int l_end = ir_new_label(F);
        int l_cont = ir_new_label(F);
        loop_end_label[loop_depth] = l_end;
        loop_cont_label[loop_depth] = l_cont;
        loop_depth++;
        ir_emit(F, IR_LABEL, -1, -1, -1, 0);
        F->ins[F->ins_count - 1].label = l_cond;
        int c = ir_expr(cs);
        ir_emit(F, IR_JZ, -1, c, -1, 0);
        F->ins[F->ins_count - 1].label = l_end;
        expect(cs, TOK_SEMICOLON);   /* cond 与 step 之间的 ; */
        /* ---- for-step：任意表达式（赋值 / i++ / i-- / 函数调用）----
         * 源码里 step 写在 body 之前，但语义上每轮迭代在 body 之后执行
         * → 解析时把发出的指令摘下来，body 之后再接回去。 */
        int step_mark = F->ins_count;
        ir_expr(cs);
        IrIns step_ins[64];
        int step_n = F->ins_count - step_mark;
        if (step_n > 64) step_n = 64;
        for (int k = 0; k < step_n; k++) step_ins[k] = F->ins[step_mark + k];
        F->ins_count = step_mark;
        /* NihaoC for 语法：for init; cond; step { body } —— step 后无分号 */
        ir_block(cs);
        /* body 后发射 step */
        ir_emit(F, IR_LABEL, -1, -1, -1, 0);
        F->ins[F->ins_count - 1].label = l_cont;
        ir_ins_move_to_end(F, step_ins, step_n);
        ir_emit(F, IR_JMP, -1, -1, -1, 0);
        F->ins[F->ins_count - 1].label = l_cond;
        ir_emit(F, IR_LABEL, -1, -1, -1, 0);
        F->ins[F->ins_count - 1].label = l_end;
        loop_depth--;
    } else if (t == TOK_BREAK) {
        next_tok(cs);
        if (loop_depth > 0) {
            ir_emit(F, IR_JMP, -1, -1, -1, 0);
            F->ins[F->ins_count - 1].label = loop_end_label[loop_depth - 1];
        } else {
            nihao_error(cs, "ir: 'break' outside loop");
        }
        skip_newlines(cs);
    } else if (t == TOK_CONTINUE) {
        next_tok(cs);
        if (loop_depth > 0) {
            int tgt = loop_cont_label[loop_depth - 1];
            if (tgt < 0) {
                nihao_error(cs, "ir: 'continue' not allowed inside switch");
            } else {
                ir_emit(F, IR_JMP, -1, -1, -1, 0);
                F->ins[F->ins_count - 1].label = tgt;
            }
        } else {
            nihao_error(cs, "ir: 'continue' outside loop");
        }
        skip_newlines(cs);
    } else if (t == TOK_IS) {
        /* is 模式匹配：is pat { ... }，匹配 while 循环条件值 is_val_vreg
         * （PB-27.1：do 不支持 is，仅 while 设 is_val_vreg）
         * pat: <int> | ±<int> | <int>..<int>（闭区间，两端可带符号） | <ident> | _ 通配 | _vis 可见性枚举 */
        next_tok(cs);
        if (is_val_vreg < 0) {
            /* PB-27.1：is 仅配合 while（do 不设 is_val_vreg + 循环外也是此错误） */
            nihao_error(cs, "ir: 'is' pattern match only valid inside while loop body");
            skip_newlines(cs);
            return;
        }
        int l_done = ir_new_label(F);
        /* PB-71：本轮已有子句匹配过 ⇒ 整条跳过（A 侧守卫的 `!__is_matched &&` 部分） */
        if (is_matched_vreg >= 0) {
            ir_emit(F, IR_JNZ, -1, is_matched_vreg, -1, 0);
            F->ins[F->ins_count - 1].label = l_done;
        }
        TokenType pt = cur_tok(cs);
        if (pt == TOK_IDENTIFIER && strcmp(cs->parser.lex->tok_str, "_") == 0) {
            /* PB-27.5：通配符 `_`：匹配任意 is_val（恒真），不 emit 条件跳转 */
            next_tok(cs);
        } else if (pt == TOK_MINUS || pt == TOK_PLUS || pt == TOK_INT_CONST) {
            /* PB-70：整数字面量可带前缀符号，区间两端都可带（`is +5` / `is -9..-5`） */
            long long lo = 0;
            if (!ir_is_int(cs, &lo)) {
                nihao_error(cs, "ir: bad 'is' pattern (expected integer after '%c')",
                            (pt == TOK_MINUS) ? '-' : '+');
                skip_newlines(cs);
                return;
            }
            if (cur_tok(cs) == TOK_RANGE) {
                next_tok(cs);
                long long hi = 0;
                if (!ir_is_int(cs, &hi)) {
                    /* PB-27.2：畸形模式 `is 5..` 后必须跟整数字面量，否则报错 */
                    nihao_error(cs, "ir: invalid 'is' range: expected an integer upper bound");
                    skip_newlines(cs);
                    return;
                }
                /* PB-27.4：反向范围 `is 5..2` 编译期检查 lo<=hi，否则报错 */
                if (lo > hi) {
                    nihao_error(cs, "ir: empty 'is' range (%lld..%lld, lo > hi)", lo, hi);
                    skip_newlines(cs);
                    return;
                }
                int ge = emit_is_cmp_with_const(is_val_vreg, IR_CMP_GE, lo);
                ir_emit(F, IR_JZ, -1, ge, -1, 0);
                F->ins[F->ins_count - 1].label = l_done;
                int le = emit_is_cmp_with_const(is_val_vreg, IR_CMP_LE, hi);
                ir_emit(F, IR_JZ, -1, le, -1, 0);
                F->ins[F->ins_count - 1].label = l_done;
            } else {
                int eq = emit_is_cmp_with_const(is_val_vreg, IR_CMP_EQ, lo);
                ir_emit(F, IR_JZ, -1, eq, -1, 0);
                F->ins[F->ins_count - 1].label = l_done;
            }
        } else if (pt == TOK__FLOW || pt == TOK__STATIC || pt == TOK__CONST ||
                   pt == TOK__VAR || pt == TOK__UNDEF) {
            /* 可见性模式：is _flow / is _static 等（BNF <visibility-enum>）——
             * 匹配循环条件值 == 可见性常量（VIS_UNDEF=0, VIS_CONST=1, VIS_FLOW=2,
             * VIS_STATIC=3, VIS_VAR=4；PB-27.3 数值与 cgen NH_ 统一） */
            int visv = (pt == TOK__FLOW) ? VIS_FLOW :
                       (pt == TOK__STATIC) ? VIS_STATIC :
                       (pt == TOK__CONST) ? VIS_CONST :
                       (pt == TOK__VAR) ? VIS_VAR : VIS_UNDEF;
            next_tok(cs);
            int eq = emit_is_cmp_with_const(is_val_vreg, IR_CMP_EQ, (long long)visv);
            ir_emit(F, IR_JZ, -1, eq, -1, 0);
            F->ins[F->ins_count - 1].label = l_done;
        } else if (pt == TOK_IDENTIFIER) {
            /* 普通标识符模式：比较 is_val == 标识符值（如枚举/变量） */
            const char *pat = cs->parser.lex->tok_str;
            int pvi = var_find(pat);
            long long cval = 0;
            int has_c = 0;
            if (pvi >= 0) {
                int pv = ir_new_vreg(F);
                ir_emit(F, IR_MOV, pv, vt[pvi], -1, 0);
                /* PB-27.7：is_val_vreg 类型感知（与变量模式同） */
                int eq;
                if (ir_is_double(is_val_vreg)) {
                    int fc = ir_new_vreg(F);
                    ir_emit(F, IR_FCMP, fc, is_val_vreg, pv, 0);  /* FCMP_EQ */
                    eq = fc;
                } else {
                    eq = ir_new_vreg(F);
                    ir_emit(F, IR_CMP_EQ, eq, is_val_vreg, pv, 0);
                }
                ir_emit(F, IR_JZ, -1, eq, -1, 0);
                F->ins[F->ins_count - 1].label = l_done;
                next_tok(cs);
            } else if (enum_const_find(pat, &cval)) {
                has_c = 1;
                next_tok(cs);
            } else {
                nihao_error(cs, "ir: unknown identifier '%s' in 'is' pattern", pat);
                next_tok(cs);
                skip_newlines(cs);
                return;
            }
            if (has_c) {
                /* PB-27.7：常量比较类型感知（用 helper） */
                int eq = emit_is_cmp_with_const(is_val_vreg, IR_CMP_EQ, cval);
                ir_emit(F, IR_JZ, -1, eq, -1, 0);
                F->ins[F->ins_count - 1].label = l_done;
            }
        } else {
            nihao_error(cs, "ir: bad 'is' pattern");
            next_tok(cs);
            skip_newlines(cs);
            return;
        }
        /* PB-71：模式已匹配 ⇒ 进块前置位，使本轮后续子句全部跳过
         * （A 侧把置位放在守卫条件里 `&& (__is_matched = 1)`，IR 无短路表达式，
         * 改为在条件跳转之后、块体之前发一条 MOV，等价）。 */
        if (is_matched_vreg >= 0) {
            int one = ir_new_vreg(F);
            ir_emit(F, IR_CONST, one, -1, -1, 1);
            ir_emit(F, IR_MOV, is_matched_vreg, one, -1, 0);
        }
        if (cur_tok(cs) == TOK_LBRACE) {
            ir_block(cs);
        } else {
            /* 规范 2026-09-01 移除 `=>` 箭头形式：is 仅块形式，且只能用于 while 体内
             * （<is-stmt> 从 <statement> 删除，do 不支持 is）。其余 token 一律报错，
             * 避免静默接受已废弃语法。c/native（parser.c）与 IR（irparse.c）两前端均已对齐。 */
            nihao_error(cs, "ir: 'is' pattern must be followed by a block");
        }
        ir_emit(F, IR_LABEL, -1, -1, -1, 0);
        F->ins[F->ins_count - 1].label = l_done;
        skip_newlines(cs);
    } else if (t == TOK_RETURN) {
        next_tok(cs);
        if (cur_tok(cs) == TOK_NEWLINE || cur_tok(cs) == TOK_RBRACE ||
            cur_tok(cs) == TOK_SEMICOLON || cur_tok(cs) == TOK_POUND) {
            /* PB-70：`return;` / `return #` 也是无值返回（终止符不是表达式） */
            ir_emit(F, IR_RET, -1, -1, -1, 0);
        } else if (cur_tok(cs) == TOK_LBRACE) {
            /* return {e0, e1, ...}：聚合返回 → STORE 到 *_mr_ret（sret） */
            int retvi = F->is_mr ? var_find("_mr_ret") : -1;
            if (retvi < 0) {
                nihao_error(cs, "ir: aggregate return outside struct-returning function");
                while (cur_tok(cs) != TOK_RBRACE && cur_tok(cs) != TOK_EOF) next_tok(cs);
                expect(cs, TOK_RBRACE);
            } else {
                next_tok(cs);
                int k = 0;
                if (cur_tok(cs) != TOK_RBRACE) {
                    for (;;) {
                        int v = ir_expr(cs);
                        IrAggType *ra2 = &agg_types[F->ret_agg_ti];
                        int offb = (ra2->kind == 1) ? 0 :
                                   (k < ra2->mcount ? ra2->moff[k] : k * 8);
                        int addr = ir_ptr_byte_off(vt[retvi], offb);
                        ir_emit(F, IR_STORE, -1, addr, v,
                                k < ra2->mcount
                                    ? ir_store_width(ra2->mscode[k]) : 0);
                        k++;
                        if (cur_tok(cs) != TOK_COMMA) break;
                        next_tok(cs);
                    }
                }
                expect(cs, TOK_RBRACE);
            }
            ir_emit(F, IR_RET, -1, -1, -1, 0);   /* bare return */
        } else if (cur_tok(cs) == TOK_IDENTIFIER && F->is_mr) {
            /* return p（聚合变量）：逐字段 STORE 到 *_mr_ret 缓冲（PB-16） */
            const char *rname = cs->parser.lex->tok_str;
            int rvi = var_find(rname);
            if (rvi >= 0 && vty[rvi] >= 0) {
                int retvi = var_find("_mr_ret");
                if (retvi >= 0) {
                    next_tok(cs);
                    /* PB-83：聚合块（低地址起，向上）→ sret 缓冲（堆，向上） */
                    ir_block_copy_bytes(vt[retvi], ir_agg_base(rvi),
                                        agg_types[vty[rvi]].bytes);
                    ir_emit(F, IR_RET, -1, -1, -1, 0);
                } else {
                    int vr = ir_expr(cs);
                    ir_emit(F, IR_RET, -1, vr, -1, 0);
                }
            } else {
                int vr = ir_expr(cs);
                ir_emit(F, IR_RET, -1, vr, -1, 0);
            }
        } else {
            int vr = ir_expr(cs);
            ir_emit(F, IR_RET, -1, vr, -1, 0);
        }
        skip_newlines(cs);
    } else if (t == TOK_STAR) {
        /* 解引用写 *p = e / *p op= e 已从 1.0.x/2.0 指针语法移除（统一 p.()= / p.() op=）。
         * 吞掉 * 与后续行，避免解析器在该 token 上死循环。 */
        nihao_error(cs, "ir: dereference-write '*p = e' removed; use p.() = e instead");
        next_tok(cs);
        skip_newlines(cs);
        return;
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
            if (vi < 0) {
                /* 顶层全局变量赋值：写全局池，不在当前函数里开同名局部 */
                int gi = global_find(name);
                if (gi >= 0) {
                    if (ir_globals[gi].vis == VIS_CONST) {
                        nihao_error(cs, "ir: cannot assign to the const global '%s'",
                                    name);
                        ir_expr(cs);
                        skip_newlines(cs);
                        return;
                    }
                    int gv = ir_expr(cs);
                    gv = ir_coerce(gv, ir_globals[gi].type_code);
                    ir_global_write(gi, gv);
                    skip_newlines(cs);
                    return;
                }
            }
            if (vi >= 0 && vvis[vi] == VIS_CONST) {
                /* PB-90：const 变量不可重绑定 */
                nihao_error(cs, "ir: cannot assign to the const '%s'", name);
                ir_skip_line(cs);
                return;
            }
            int fresh = 0;
            if (vi < 0) { vi = var_declare(name, 0, -1, VIS_VAR, 0); fresh = 1; }
            if (vty[vi] >= 0 && agg_types[vty[vi]].kind != 2 &&
                cur_tok(cs) == TOK_IDENTIFIER) {
                /* 聚合整体赋值：逐成员拷贝（struct 按展开槽数（嵌套递归），
                 * union 共享槽拷 1） */
                int src = var_find(cs->parser.lex->tok_str);
                if (src >= 0 && vty[src] >= 0 &&
                    agg_types[vty[src]].kind == agg_types[vty[vi]].kind) {
                    next_tok(cs);   /* src name */
                    /* PB-83：整块按字节复制（低地址端对齐，两后端同为向上） */
                    ir_block_copy_bytes(ir_agg_base(vi), ir_agg_base(src),
                                        agg_types[vty[vi]].bytes);
                    skip_newlines(cs);
                    return;
                }
            }
            if (cur_tok(cs) == TOK_IDENTIFIER) {
                /* 切片 rhs：s = arr[lo..hi] / s = arr[..hi]——记录逻辑长度（常量边界）+ 指针 */
                LexerState *lx2 = cs->parser.lex;
                lx2->peek_valid = 0;
                lexer_peek(lx2);
                if (lx2->peek_tok == TOK_LBRACKET) {
                    const char *arrn = lx2->tok_str;
                    int avi = var_find(arrn);
                    if (avi >= 0) {
                        next_tok(cs);   /* arr */
                        next_tok(cs);   /* [ */
                        int open_lo = 0;    /* arr[..hi]：省略起始 */
                        int lo = -1;
                        if (cur_tok(cs) == TOK_RANGE) {
                            open_lo = 1;
                        } else {
                            lo = ir_expr(cs);
                        }
                        if (cur_tok(cs) == TOK_RANGE) {
                            next_tok(cs);
                            int hi = ir_expr(cs);
                            expect(cs, TOK_RBRACKET);
                            long long hiv;
                            int addr;
                            if (open_lo) {
                                /* arr[..hi] → &arr[0]，长度 = hi */
                                if (vreg_const(F, hi, &hiv)) {
                                    ve[vi] = (int)hiv;
                                    vlen_known[vi] = 1;   /* PB-72 */
                                }
                                addr = ir_new_vreg(F);
                                ir_emit(F, IR_ADDR, addr, vt[avi], -1, 0);
                            } else {
                                long long lov;
                                if (vreg_const(F, lo, &lov) && vreg_const(F, hi, &hiv)) {
                                    ve[vi] = (int)(hiv - lov);
                                    vlen_known[vi] = 1;   /* PB-72 */
                                }
                                addr = ir_elem_addr(cs, avi, vt[avi], lo,
                                                        ir_elem_stride(avi),
                                                        vetyp[avi] == 8);
                            }
                            ir_emit(F, IR_MOV, vt[vi], addr, -1, 0);
                            /* PB-82：切片变量的元素类型（供 sl.() 窄读）；帧数组来源供方向判断 */
                            if (vty[avi] < 0) pt[vi] = PT_SCALAR(vetyp[avi]);
                            if (vetyp[avi] != 8) vfrm[vi] = 1;
                            skip_newlines(cs);
                            return;
                        }
                        nihao_error(cs, "ir: expected '..' in slice assignment rhs");
                        skip_newlines(cs);
                        return;
                    }
                    lx2->peek_valid = 0;
                }
                lx2->peek_valid = 0;
            }
            if (cur_tok(cs) == TOK_BITWISE_AND) {
                /* p = &x（指针记录，阶段 1 类型化指针）：
                 *   &聚合 → pt[vi] = 聚合索引（供 -> 成员访问）
                 *   &标量 → pt[vi] = PT_SCALAR(code)（供 *p 类型化解引用/算术）
                 * peek 预判目标变量（peek_str 是 strdup 永久拷贝）——
                 * 未声明/枚举时不消费 &，回退通用 ir_expr 解析。 */
                LexerState *lx2 = cs->parser.lex;
                lx2->peek_valid = 0;
                lexer_peek(lx2);
                if (lx2->peek_tok == TOK_IDENTIFIER) {
                    int avi = var_find(lx2->peek_str);
                    if (avi >= 0) {
                        if (vty[avi] >= 0 && agg_types[vty[avi]].kind != 2) {
                            pt[vi] = vty[avi];              /* 聚合指针 */
                        } else if (vty[avi] < 0) {
                            pt[vi] = PT_SCALAR(vtype[avi]); /* 标量指针（新） */
                        } else {
                            avi = -1;   /* 枚举：不记录，回退通用 */
                        }
                        if (avi >= 0) {
                            next_tok(cs);   /* & */
                            next_tok(cs);   /* name */
                            int addr = ir_new_vreg(F);
                            ir_emit(F, IR_ADDR, addr, vt[avi], -1, 0);
                            ir_emit(F, IR_MOV, vt[vi], addr, -1, 0);
                            vfrm[vi] = 1;   /* PB-81：指针值来自帧地址 */
                            skip_newlines(cs);
                            return;
                        }
                    }
                }
                lx2->peek_valid = 0;
                /* 非标量/聚合目标：& 未消费，回退通用 ir_expr 解析 */
            }
            if (cur_tok(cs) == TOK_STRING_LITERAL &&
                (fresh || vetyp[vi] == 8)) {
                /* s = "abc"（类型由字面量推断）：逻辑长度 = 字面量长（len() 用），
                 * 槽值 = 池地址，vetyp=8 标记 byte 寻址（与 char[] 同语义） */
                ve[vi] = (int)strlen(cs->parser.lex->tok_str);
                vlen_known[vi] = 1;   /* PB-72 */
                if (fresh) vetyp[vi] = 8;
            }
            int str_copy = (cur_tok(cs) == TOK_STRING_LITERAL &&
                            vetyp[vi] == 8 && vvis[vi] != VIS_CONST);
            const char *str_lit = str_copy
                ? nihao_strdup(g_cs, cs->parser.lex->tok_str) : NULL;
            int vfrm_src = -1;      /* PB-81：`r = s` 的指针来源传播 */
            if (cur_tok(cs) == TOK_IDENTIFIER) {
                int s = var_find(cs->parser.lex->tok_str);
                if (s >= 0) vfrm_src = vfrm[s];
            }
            int vr;
            if (str_copy) {
                /* PB-89：动态字符串赋字面量 → 可写副本（元素写须成立） */
                next_tok(cs);
                vr = ir_strlit_copy(cs, str_lit);
            } else {
                vr = ir_expr(cs);
            }
            /* PB-1 补缺（2026-09-01）：窄整数目标此前只在「声明初始化」路径经
             * ir_coerce 截断，普通赋值直接落槽 —— `b u8 = 0; b = 300` 在 IR 后端
             * 得到 300 而 A 方案（C 语义）得到 44。此处统一补目标类型协调。 */
            vr = ir_coerce(vr, vtype[vi]);
            ir_emit(F, IR_MOV, vt[vi], vr, -1, 0);
            if (vfrm_src > 0) vfrm[vi] = 1;
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
            if (vi >= 0 && vvis[vi] == VIS_CONST) {
                /* PB-90：const 变量不可修改 */
                nihao_error(cs, "ir: cannot modify the const '%s'", name);
                ir_skip_line(cs);
                return;
            }
            if (vi < 0) {
                int gi = global_find(name);
                if (gi >= 0) {
                    int cur = ir_global_read(gi);
                    int b = ir_expr(cs);
                    int nv = ir_new_vreg(F);
                    ir_emit(F, op, nv, cur, b, 0);
                    nv = ir_coerce(nv, ir_globals[gi].type_code);
                    ir_global_write(gi, nv);
                    skip_newlines(cs);
                    return;
                }
                nihao_error(cs, "ir: undeclared variable '%s'", name);
                skip_newlines(cs);
                return;
            }
            int b = ir_expr(cs);
            int vr = ir_new_vreg(F);
            if (ir_is_double(vt[vi])) {
                /* 浮点复合赋值：目标为 double → 发浮点运算（对齐 ir_expr 二进制 op） */
                int fb = ir_is_double(b) ? b : ir_to_double(b);
                IrOp fop = IR_FADD;
                if (op == IR_SUB) fop = IR_FSUB;
                else if (op == IR_MUL) fop = IR_FMUL;
                else if (op == IR_DIV) fop = IR_FDIV;
                /* IR_MOD 无浮点操作码（语言层禁用 %= 浮点），退化用 FADD 不应到达 */
                ir_emit(F, fop, vr, vt[vi], fb, 0);
                ir_set_double(vr);                 /* 结果标记 double，使 ir_coerce 走 no-op（f32→FTRUNC） */
                vr = ir_coerce(vr, vtype[vi]);      /* f64→no-op；f32→FTRUNC 单精度截断 */
            } else {
                ir_emit(F, op, vr, vt[vi], b, 0);
                vr = ir_coerce(vr, vtype[vi]);   /* PB-1 补缺：窄整数目标类型截断 */
            }
            ir_emit(F, IR_MOV, vt[vi], vr, -1, 0);
            skip_newlines(cs);
        } else if (nt == TOK_INCREMENT || nt == TOK_DECREMENT) {
            /* 后缀自增/自减 x++ / x-- */
            IrOp op = (nt == TOK_INCREMENT) ? IR_ADD : IR_SUB;
            next_tok(cs);           /* name */
            next_tok(cs);           /* ++ / -- */
            int vi = var_find(name);
            if (vi < 0) {
                int gi = global_find(name);
                if (gi >= 0) {
                    int one = ir_new_vreg(F);
                    ir_emit(F, IR_CONST, one, -1, -1, 1);
                    int cur = ir_global_read(gi);
                    int gv = ir_new_vreg(F);
                    ir_emit(F, op, gv, cur, one, 0);
                    gv = ir_coerce(gv, ir_globals[gi].type_code);
                    ir_global_write(gi, gv);
                    skip_newlines(cs);
                    return;
                }
                nihao_error(cs, "ir: undeclared variable '%s'", name);
                skip_newlines(cs);
                return;
            }
            int one = ir_new_vreg(F);
            ir_emit(F, IR_CONST, one, -1, -1, 1);
            int vr = ir_new_vreg(F);
            if (ir_is_double(vt[vi])) {
                /* 浮点自增/自减：1 提为 double 后走浮点运算；结果标记 double 后 ir_coerce */
                int fone = ir_to_double(one);
                ir_emit(F, op == IR_ADD ? IR_FADD : IR_FSUB, vr, vt[vi], fone, 0);
                ir_set_double(vr);
                vr = ir_coerce(vr, vtype[vi]);      /* f64→no-op；f32→FTRUNC */
            } else {
                ir_emit(F, op, vr, vt[vi], one, 0);
                vr = ir_coerce(vr, vtype[vi]);   /* PB-1 补缺：窄整数目标类型截断 */
            }
            ir_emit(F, IR_MOV, vt[vi], vr, -1, 0);
            skip_newlines(cs);
        } else if (nt == TOK_DOT_PAREN) {
            /* 指针解引用 p.() = e（STORE）/ p.() op= e（复合 RMW，2.0 对齐 .() 复合解引用）。
             * 地址 = p 槽值（目标地址），LOAD 当前值 → 类型协调 → op → 按指向类型截断 → STORE。 */
            next_tok(cs);           /* name */
            int vi = var_find(name);
            if (vi < 0) {
                nihao_error(cs, "ir: undeclared variable '%s'", name);
                skip_newlines(cs);
                return;
            }
            ir_vis_check_writable(cs, vi);  /* M2：写前检查是否被活动借用冻结 */
            if (ir_chain_needs_new(cs, vi)) {   /* PB-68：多步链 / 新形态 */
                ir_chain_assign(cs, vi);
                return;
            }
            next_tok(cs);           /* .( */
            int wcode = -1;         /* PB-63：`.(T)` 的显式类型码，优先于 pt[] 推出来的指向类型 */
            if (cur_tok(cs) != TOK_RPAREN) {
                if (cur_tok(cs) == TOK_VOID && ir_peek_ahead(cs, 1) == TOK_RPAREN) {
                    /* 码 0 碰撞修复：裸 `.(void)` 无意义；`.(void[N])` 是合法数组后缀 */
                    nihao_error(cs, "ir: '.()' dereference: 'void' has no size — use a scalar type");
                    wcode = 0;
                } else {
                    wcode = ir_scalar_code_of_token(cur_tok(cs));
                }
                if (wcode < 0) {
                    nihao_error(cs, "ir: '.()' dereference takes a scalar type or nothing, "
                                    "not '%s'", cs->parser.lex->tok_str);
                    wcode = 0;
                }
                next_tok(cs);
            }
            expect(cs, TOK_RPAREN);
            TokenType at = cur_tok(cs);
            if (at == TOK_ASSIGN || at == TOK_PLUS_ASSIGN || at == TOK_MINUS_ASSIGN ||
                at == TOK_STAR_ASSIGN || at == TOK_SLASH_ASSIGN || at == TOK_PERCENT_ASSIGN) {
                next_tok(cs);
                int addr = ir_new_vreg(F);
                ir_emit(F, IR_MOV, addr, vt[vi], -1, 0);    /* p 槽值 = 目标地址（MOV 勿 LOAD） */
                int v;
                if (at != TOK_ASSIGN) {
                    /* 复合 p.() op= e：LOAD 当前值 → 按指向类型协调 → op → 截断 → STORE */
                    int cur = ir_new_vreg(F);
                    int sc = (wcode >= 0) ? wcode :
                             ((PT_IS_SCALAR(pt[vi])) ? PT_SCALAR_CODE(pt[vi]) : vtype[vi]);
                    ir_emit(F, IR_LOAD, cur, addr, -1, ir_load_width_imm(sc));   /* PB-75(b) */
                    v = ir_expr(cs);
                    IrOp op;
                    switch (at) {
                        case TOK_PLUS_ASSIGN:    op = IR_ADD; break;
                        case TOK_MINUS_ASSIGN:   op = IR_SUB; break;
                        case TOK_STAR_ASSIGN:    op = IR_MUL; break;
                        case TOK_SLASH_ASSIGN:   op = IR_DIV; break;
                        default:                 op = IR_MOD; break;
                    }
                    if (sc == 1 || sc == 8) {
                        /* 浮点指向：LOAD 已是 double 位模式，直接标记（勿 ITOD）；rhs 提 double */
                        ir_set_double(cur);
                        if (!ir_is_double(v)) v = ir_to_double(v);
                        IrOp fop = (op == IR_MUL) ? IR_FMUL : (op == IR_DIV) ? IR_FDIV :
                                   (op == IR_SUB) ? IR_FSUB : IR_FADD;
                        int nv = ir_new_vreg(F);
                        ir_emit(F, fop, nv, cur, v, 0);
                        ir_set_double(nv);
                        v = nv;
                    } else {
                        if (ir_is_double(cur)) { int t = ir_new_vreg(F); ir_emit(F, IR_DTOI, t, cur, -1, 0); cur = t; }
                        if (ir_is_double(v))   { int t = ir_new_vreg(F); ir_emit(F, IR_DTOI, t, v, -1, 0);   v = t; }
                        int nv = ir_new_vreg(F);
                        ir_emit(F, op, nv, cur, v, 0);
                        v = nv;
                    }
                    v = ir_coerce(v, sc);   /* 按指向类型截断（窄型 TRUNC / double→int 兜底） */
                } else {
                    v = ir_expr(cs);
                    v = ir_coerce(v, (wcode >= 0) ? wcode :
                             ((PT_IS_SCALAR(pt[vi])) ? PT_SCALAR_CODE(pt[vi]) : vtype[vi]));
                }
                {
                    int scw = (wcode >= 0) ? wcode :
                              ((PT_IS_SCALAR(pt[vi])) ? PT_SCALAR_CODE(pt[vi]) : vtype[vi]);
                    ir_emit(F, IR_STORE, -1, addr, v, ir_store_width(scw));   /* PB-75(b) */
                }
                skip_newlines(cs);
            } else {
                nihao_error(cs, "ir: expected '=' or 'op=' after p.()");
                skip_newlines(cs);
            }
        } else if (nt == TOK_ARROW) {
            /* 指针成员赋值 p->field = e / p->field op= e（地址 = p 值 + off*8） */
            next_tok(cs);           /* name */
            int vi = var_find(name);
            if (vi < 0) {
                nihao_error(cs, "ir: undeclared variable '%s'", name);
                skip_newlines(cs);
                return;
            }
            if (pt[vi] < 0) {
                if (vptr[vi])
                    nihao_error(cs, "ir: cannot access a member of the generic 'void' pointer");
                else
                    nihao_error(cs, "ir: '%s' is not a pointer to aggregate (assign &var first)", name);
                while (cur_tok(cs) != TOK_NEWLINE && cur_tok(cs) != TOK_RBRACE &&
                       cur_tok(cs) != TOK_EOF) next_tok(cs);
                skip_newlines(cs);
                return;
            }
            next_tok(cs);           /* -> */
            const char *fname = cs->parser.lex->tok_str;
            int ti = pt[vi];
            int off = 0;
            int fi = -1;
            for (;;) {
                fi = agg_member_find(ti, fname);
                if (fi < 0) {
                    nihao_error(cs, "ir: no member '%s' in type '%s'", fname,
                                agg_types[ti].name);
                    skip_newlines(cs);
                    return;
                }
                off += (agg_types[ti].kind == 1) ? 0 : agg_types[ti].moff[fi];
                int mt = agg_types[ti].mtype[fi];
                next_tok(cs);           /* field */
                if (mt >= 0 && (cur_tok(cs) == TOK_DOT || cur_tok(cs) == TOK_ARROW)) {
                    ti = mt;            /* 嵌套：继续链 */
                    next_tok(cs);       /* . / -> */
                    fname = cs->parser.lex->tok_str;
                    continue;
                }
                break;
            }
            TokenType at = cur_tok(cs);
            int is_compound = 0;
            IrOp aop = IR_NOP;
            if (at == TOK_PLUS_ASSIGN) { aop = IR_ADD; is_compound = 1; }
            else if (at == TOK_MINUS_ASSIGN) { aop = IR_SUB; is_compound = 1; }
            else if (at == TOK_STAR_ASSIGN) { aop = IR_MUL; is_compound = 1; }
            else if (at == TOK_SLASH_ASSIGN) { aop = IR_DIV; is_compound = 1; }
            else if (at == TOK_PERCENT_ASSIGN) { aop = IR_MOD; is_compound = 1; }
            if (at == TOK_ASSIGN || is_compound) {
                next_tok(cs);
                int b;
                if (!is_compound && cur_tok(cs) == TOK_STRING_LITERAL &&
                    agg_types[ti].misarr[fi] && agg_types[ti].marr_len[fi] <= 0) {
                    /* PB-89：动态字符串成员赋字面量 → 可写副本 */
                    const char *lit = nihao_strdup(g_cs, cs->parser.lex->tok_str);
                    next_tok(cs);
                    b = ir_strlit_copy(cs, lit);
                } else {
                    b = ir_expr(cs);
                }
                int nv;
                int bfbits = agg_types[ti].mbits[fi];
                if (!is_compound) {
                    nv = b;
                } else {
                    int cur;
                    if (bfbits > 0) {
                        /* 位域复合赋值：基于位域值运算（LOAD+AND 取低 N 位） */
                        int caddr = ir_new_vreg(F);
                        ir_emit(F, IR_MOV, caddr, vt[vi], -1, 0);
                        if (off > 0)
                            caddr = ir_ptr_slot_off(vi, caddr, off);   /* PB-81 */
                        cur = ir_bf_load(caddr, ti, fi);
                    } else {
                        /* 普通成员当前值：LOAD(p 值 + off*8)，方向按来源（PB-81） */
                        int base = ir_new_vreg(F);
                        ir_emit(F, IR_MOV, base, vt[vi], -1, 0);
                        int caddr = ir_ptr_slot_off(vi, base, off);
                        cur = ir_new_vreg(F);
                        ir_emit(F, IR_LOAD, cur, caddr, -1,
                                ir_load_width_imm(agg_types[ti].mscode[fi]));   /* PB-76(b) */
                    }
                    nv = ir_new_vreg(F);
                    ir_emit(F, aop, nv, cur, b, 0);
                }
                /* STORE 到 p 值 + off*8（方向按来源，PB-81） */
                int base = ir_new_vreg(F);
                ir_emit(F, IR_MOV, base, vt[vi], -1, 0);
                int addr = ir_ptr_slot_off(vi, base, off);
                if (bfbits > 0) {
                    ir_bf_store(addr, nv, ti, fi);
                } else {
                    /* PB-76(b)：非位域标量成员按成员宽窄写（位域仍走 RMW） */
                    ir_emit(F, IR_STORE, -1, addr, nv, ir_member_swidth(ti, fi));
                }
                skip_newlines(cs);
            } else {
                nihao_error(cs, "ir: expected '=' after '->' member access");
                skip_newlines(cs);
            }
        } else if (nt == TOK_DOT) {
            /* 成员赋值 s.field = e / s.field op= e */
            next_tok(cs);           /* name */
            int vi = var_find(name);
            if (vi >= 0 && vvis[vi] == VIS_CONST) {
                /* PB-90：const 聚合体的自有存储不可写 */
                nihao_error(cs, "ir: cannot modify the const '%s'", name);
                ir_skip_line(cs);
                return;
            }
            if (vi < 0 || vty[vi] < 0) {
                nihao_error(cs, "ir: '%s' is not an aggregate variable", name);
                skip_newlines(cs);
                return;
            }
            next_tok(cs);           /* . */
            const char *fname = cs->parser.lex->tok_str;
            int ti = vty[vi];
            int off = 0;
            int fi = -1;
            for (;;) {
                fi = agg_member_find(ti, fname);
                if (fi < 0) {
                    nihao_error(cs, "ir: no member '%s' in type '%s'", fname,
                                agg_types[ti].name);
                    skip_newlines(cs);
                    return;
                }
                off += (agg_types[ti].kind == 1) ? 0 : agg_types[ti].moff[fi];
                int mt = agg_types[ti].mtype[fi];
                next_tok(cs);           /* field */
                if (mt >= 0 && cur_tok(cs) == TOK_DOT) {
                    ti = mt;            /* 嵌套：继续链 */
                    next_tok(cs);       /* . */
                    fname = cs->parser.lex->tok_str;
                    continue;
                }
                break;
            }
            /* 数组成员元素写 p.name[0] = e：元素地址 = 成员首槽 + idx*元素宽 */
            int esize = 0;
            int elem_addr = -1;
            if (cur_tok(cs) == TOK_LBRACKET) {
                elem_addr = ir_member_elem_addr(cs, vi, ti, fi, off, fname, &esize);
                if (elem_addr < 0) {
                    skip_newlines(cs);
                    return;
                }
            }
            TokenType at = cur_tok(cs);
            /* 注意：不用 -1 哨兵判断——IrOp 枚举在 tcc 下底层为 unsigned，
             * `aop < 0` 恒假会导致纯赋值误入复合路径。用布尔标志。 */
            int is_compound = 0;
            IrOp aop = IR_NOP;
            if (at == TOK_PLUS_ASSIGN) { aop = IR_ADD; is_compound = 1; }
            else if (at == TOK_MINUS_ASSIGN) { aop = IR_SUB; is_compound = 1; }
            else if (at == TOK_STAR_ASSIGN) { aop = IR_MUL; is_compound = 1; }
            else if (at == TOK_SLASH_ASSIGN) { aop = IR_DIV; is_compound = 1; }
            else if (at == TOK_PERCENT_ASSIGN) { aop = IR_MOD; is_compound = 1; }
            if (at == TOK_ASSIGN || is_compound) {
                next_tok(cs);
                int b;
                if (!is_compound && cur_tok(cs) == TOK_STRING_LITERAL &&
                    agg_types[ti].misarr[fi] && agg_types[ti].marr_len[fi] <= 0) {
                    /* PB-89：动态字符串成员赋字面量 → 可写副本 */
                    const char *lit = nihao_strdup(g_cs, cs->parser.lex->tok_str);
                    next_tok(cs);
                    b = ir_strlit_copy(cs, lit);
                } else {
                    b = ir_expr(cs);
                }
                int nv;
                int bfbits = agg_types[ti].mbits[fi];
                if (!is_compound) {
                    nv = b;
                } else {
                    int cur;
                    if (elem_addr >= 0) {
                        /* 数组成员复合赋值：当前值取元素地址处的内容 */
                        cur = ir_new_vreg(F);
                        if (esize == 1) ir_load1(cur, elem_addr, ir_member_vcode(ti, fi));
                        else ir_emit(F, IR_LOAD, cur, elem_addr, -1, 0);
                    } else if (bfbits > 0) {
                        /* 位域复合赋值：基于位域值运算（存储单元 LOAD + 移位 + AND） */
                        int caddr = ir_member_addr(cs, vi, ti, fi, off);
                        cur = ir_bf_load(caddr, ti, fi);
                    } else {
                        /* PB-83：成员当前值从字节地址按成员宽读取 */
                        cur = ir_new_vreg(F);
                        int mc0 = agg_types[ti].mscode[fi];
                        int a0 = ir_member_addr(cs, vi, ti, fi, off);
                        ir_emit(F, IR_LOAD, cur, a0, -1, ir_load_width_imm(mc0));
                        if (mc0 == 1 || mc0 == 8) ir_set_double(cur);
                    }
                    nv = ir_new_vreg(F);
                    ir_emit(F, aop, nv, cur, b, 0);
                }
                if (elem_addr >= 0) {
                    /* 数组成员元素写：1 字节元素用 STORE8，其余 8 字节槽 */
                    if (esize == 1) ir_emit(F, IR_STORE8, -1, elem_addr, nv, 0);
                    else ir_emit(F, IR_STORE, -1, elem_addr, nv, 0);
                } else {
                    int addr = ir_member_addr(cs, vi, ti, fi, off);
                    if (bfbits > 0) {
                        ir_bf_store(addr, nv, ti, fi);
                    } else {
                        ir_emit(F, IR_STORE, -1, addr, nv,
                                ir_member_swidth(ti, fi));
                    }
                }
                skip_newlines(cs);
            } else {
                nihao_error(cs, "ir: expected '=' after member access");
                skip_newlines(cs);
            }
        } else if (nt == TOK_IDENTIFIER) {
            /* 可能为聚合类型声明 p Person（peek 只给类型，直接消费判断） */
            next_tok(cs);           /* name */
            const char *tname = cs->parser.lex->tok_str;
            int ti = agg_type_find(tname);
            if (ti >= 0) {
                next_tok(cs);       /* 类型名 */
                ir_agg_decl(cs, name, ti, decl_vis);
                skip_newlines(cs);
                return;
            }
            nihao_error(cs, "ir: unexpected '%s %s' in statement", name, tname);
            skip_newlines(cs);
            return;
        } else if (nt == TOK_STRUCT || nt == TOK_UNION || nt == TOK_ENUM) {
            /* PB-80：函数体内的类型定义（用例把 `S union { ... }` 写在 main 里） */
            const char *tname2 = cs->parser.lex->tok_str;   /* 类型名（当前 token） */
            next_tok(cs);           /* name → struct/union/enum */
            ir_type_decl_named(cs, tname2);
            skip_newlines(cs);
            return;
        } else if (is_type_token(nt) ||
                   (nt == TOK_LPAREN && is_type_token(ir_peek_ahead(cs, 2)) &&
                    ir_peek_ahead(cs, 3) == TOK_RPAREN)) {
            /* 声明: name i32 [N] = expr | = {e0, e1, ...} | name 函数指针类型 = fn
             * PB-70：括号类型 `(T)`（BNF §3 <type-name> ::= … | "(" <type-name> ")"）
             * 归一成内层类型 token 后走同一条声明路径，数组后缀照常接在 `)` 之后。 */
            int paren_type = (nt == TOK_LPAREN);
            if (paren_type) nt = ir_peek_ahead(cs, 2);
            int decl_float = (nt == TOK_F64 || nt == TOK_F32);
            /* PB-1 窄整数类型编码（0=i64 1=float 2=i8 3=i16 4=i32 5=u8 6=u16 7=u32）；
             * f32 严格宽度（2026-08-19）：F32 → 码 8（标记 f32，赋值时 FTRUNC）；
             * fx32 / fx64 按同宽整型（BNF v2.11 定案）：FX32 → 码 4 截断、FX64 → 码 0 */
            int vt_code = decl_float ? (nt == TOK_F32 ? 8 : 1) :
                          (nt == TOK_CHAR || nt == TOK_I8) ? 2 :
                          (nt == TOK_I16) ? 3 : (nt == TOK_I32) ? 4 :
                          (nt == TOK_FX32) ? 4 :
                          (nt == TOK_U8)  ? 5 : (nt == TOK_U16) ? 6 :
                          (nt == TOK_U32) ? 7 : 0;
            next_tok(cs);           /* name */
            if (paren_type) {
                next_tok(cs);       /* ( */
                next_tok(cs);       /* 内层类型 */
                next_tok(cs);       /* ) */
            } else {
                next_tok(cs);       /* 类型 */
            }
            int elems = 0;
            int is_arr = 0;         /* 出现过 [..]（数组/字符串；标量 char 非数组） */
            int dims[IR_MAX_DIMS];  /* 各维长度（PB-67：多维数组） */
            int nd = 0;
            if (cur_tok(cs) == TOK_LPAREN) {
                /* 函数指针类型 void(params) [ret]：跳过参数列表与可选返回类型 */
                next_tok(cs);
                int pd = 1;
                while (pd > 0 && cur_tok(cs) != TOK_EOF) {
                    if (cur_tok(cs) == TOK_LPAREN) pd++;
                    else if (cur_tok(cs) == TOK_RPAREN) pd--;
                    next_tok(cs);
                }
                if (is_type_token(cur_tok(cs)) || cur_tok(cs) == TOK_IDENTIFIER) {
                    next_tok(cs);   /* 返回类型 */
                }
            } else {
                /* 数组维度后缀（可多维，PB-67）：逐段解析 [..]，记录各维长度。
                 * 槽模型下多维按行主序摊平成连续槽，总槽数 = 各维之积。
                 * PB-68（指针类型后缀档）：`void` 基类型的裸 `[]` 不是数组维而是
                 * 「指针级」（A 侧 `void[]` → void**、`void[][]` → void***），
                 * 固定维 `[N]` 才是数组维。 */
                while (cur_tok(cs) == TOK_LBRACKET) {
                    next_tok(cs);
                    is_arr = 1;     /* char[] 动态字符串与数组共用此标记 */
                    int cnt = 0;
                    int dyn = 0;     /* 1 = 动态后缀（[..] / [...] / [N...]） */
                    if (cur_tok(cs) == TOK_RANGE || cur_tok(cs) == TOK_ELLIPSIS) {
                        /* 纯动态数组 [...] / [..]：固定默认容量 8 槽（增长语义留 TODO） */
                        cnt = 8;
                        dyn = 1;
                        next_tok(cs);
                        if (cur_tok(cs) == TOK_INT_CONST) {
                            /* [...N] / [..N]：历史兼容写法（A 方案早期实现），
                             * 等同 BNF 规范的 [N...] —— 2026-09-01 与 parser.c 对齐 */
                            cnt = (int)cs->parser.lex->tok_val.i;
                            next_tok(cs);
                        }
                    } else if (cur_tok(cs) == TOK_INT_CONST) {
                        cnt = (int)cs->parser.lex->tok_val.i;
                        next_tok(cs);
                        if (cur_tok(cs) == TOK_RANGE || cur_tok(cs) == TOK_ELLIPSIS) {
                            /* N... 动态数组：初始容量 N 个槽（增长忽略） */
                            dyn = 1;
                            next_tok(cs);
                        }
                    }
                    expect(cs, TOK_RBRACKET);
                    if (nt == TOK_VOID && cnt == 0 && !dyn) {
                        /* 裸 []：通用指针的下一级（指针级，不是数组维）——
                         * 槽模型下不占维，解引用层数由后缀链动态决定 */
                        continue;
                    }
                    if (nd < IR_MAX_DIMS) dims[nd++] = cnt;
                    elems = cnt;
                }
                if (nd > 1) {
                    /* 多维：总槽数 = 各维之积（行主序摊平） */
                    long long total = 1;
                    for (int k = 0; k < nd; k++) {
                        if (dims[k] <= 0) {
                            nihao_error(cs, "ir: array dimension %d must have a "
                                            "positive length", k + 1);
                            dims[k] = 1;
                        }
                        total *= dims[k];
                    }
                    elems = (int)total;
                }
            }
            expect(cs, TOK_ASSIGN); /* 消费 =（expect 已推进） */
            int src_vi = -1;
            int call_ret_vis = -1;          /* PB-27：RHS 是函数调用时记录 callee->ret_vis */
            const char *call_name = NULL;    /* PB-27：函数名（错误消息用） */
            if (cur_tok(cs) == TOK_IDENTIFIER) {
                /* peek 下一 token：若是 ( 则视为函数调用 RHS（PB-27 返回值所有权检查），
                 * 否则视为裸变量引用（PB-26 转移检查） */
                LexerState *lx = cs->parser.lex;
                lx->peek_valid = 0;
                lexer_peek(lx);
                TokenType after = lx->peek_tok;
                lx->peek_valid = 0;
                if (after == TOK_LPAREN) {
                    call_name = cs->parser.lex->tok_str;
                    IrFn *tfn = ir_find_fn(call_name);
                    if (tfn) call_ret_vis = tfn->ret_vis;
                    /* tfn==NULL：前向引用，跳过检查（同 PB-26 var 路径的容错） */
                } else {
                    int sv = var_find(cs->parser.lex->tok_str);
                    if (sv >= 0) src_vi = sv;   /* RHS 是裸变量引用 → 走所有权检查 */
                }
            }
            /* PB-68 指针类型后缀档：`void[N] = malloc(void[N])` 退化为「指针值数组」
             * （A 侧同形态：`void*(*p) = malloc(N * sizeof(void*))`）——槽里存的是
             * 堆地址而不是帧数组基址，故按标量指针槽声明，下标走「指针值 + 偏移」。 */
            int is_ptrarr = (nt == TOK_VOID && elems > 0 &&
                             call_name && strcmp(call_name, "malloc") == 0);
            int decl_es = (int)type_size_token(nt);
            if (!(decl_es == 1 || decl_es == 2 || decl_es == 4 || decl_es == 8))
                decl_es = 8;
            int vi = var_declare(name, is_ptrarr ? 0 : elems, -1, decl_vis,
                                 (elems > 0 && !is_ptrarr) ? decl_es : 0);
            /* 记录维度（PB-67）：只有多维（nd>=2）进维度表；一维与动态字符串
             * （char[] / void[]）沿用历史单维寻址路径，行为不变 */
            if (nd >= 2) {
                vndim[vi] = nd;
                for (int k = 0; k < IR_MAX_DIMS; k++)
                    vdim[vi][k] = (k < nd) ? dims[k] : 0;
            }
            if (nt == TOK_VOID) vptr[vi] = 1;   /* 指针类标记（仅 void 触发所有权语义） */
            if (is_ptrarr) vmode[vi] = VM_PTRARR;
            if (src_vi >= 0) {
                ir_vis_check_assign(cs, vi, src_vi);  /* M2 转移检查（裸变量 RHS） */
            } else if ((call_ret_vis == VIS_CONST || call_ret_vis == VIS_FLOW ||
                       call_ret_vis == VIS_STATIC) && nt == TOK_VOID) {
                /* PB-27：函数调用 RHS → 接收变量（void* 指针类）按 ret_vis→decl_vis 检查。
                 * 仅当 callee 显式声明了非 var 返回前缀（VIS_CONST/VIS_FLOW/VIS_STATIC）才检查——
                 * 无前缀或外部符号（call_ret_vis==VIS_VAR 或 < 0，如 malloc/create 默认返回 var 但
                 * 实质是 fresh value，不参与所有权转移）。
                 * 转移矩阵（ir_vis_transfer）: flow→{flow,var,const}, static→{static,const},
                 *   var→{var,const}, const→{const}；其他组合均禁止。 */
                if (ir_vis_transfer(call_ret_vis, decl_vis)) {
                    nihao_error(cs, "cannot assign %s() return (visibility %s) to '%s' (%s): "
                                    "target lifetime shorter than source",
                                call_name ? call_name : "(call)",
                                ir_vis_str(call_ret_vis), name, ir_vis_str(decl_vis));
                }
            }
            if (elems > 0) {
                vetyp[vi] = vt_code;    /* 数组元素类型（元素截断/浮点标记用） */
            } else if (vt_code == 2 && elems == 0 && is_arr) {
                /* char[] 动态字符串：池地址（字节布局）——寻址宽度 1（码 8 标记） */
                vetyp[vi] = 8;
            } else if (vt_code == 1) {
                vtype[vi] = 1;          /* f64/f32 变量 → double 槽 */
                ir_set_double(vt[vi]);  /* 槽 vreg 类型同步（ir_to_c 声明 double） */
            } else if (vt_code == 8) {
                vtype[vi] = 8;          /* f32 严格宽度：槽 double + 赋值 FTRUNC */
                ir_set_double(vt[vi]);
            } else {
                vtype[vi] = vt_code;    /* 窄整数：赋值时截断（IR_TRUNC） */
            }
            if (cur_tok(cs) == TOK_LBRACE) {
                /* 数组初始化列表 {e0, e1, ...}：逐个 STORE 到元素槽；
                 * 多维（PB-67）按维度递归填槽（行主序摊平） */
                next_tok(cs);
                if (vndim[vi] >= 2) {
                    ir_array_init_list(cs, vi, 0, 0);
                } else {
                    int k = 0;
                    if (cur_tok(cs) != TOK_RBRACE) {
                        for (;;) {
                            int v = ir_expr(cs);
                            v = ir_coerce(v, vetyp[vi] == 8 ? 2 : vetyp[vi]);   /* PB-1：元素类型协调（vetyp 8=char 元素，与 vtype 8=f32 编码冲突→按 i8 截断） */
                            int addr = ir_arr_const_addr(vi, k);
                            ir_arr_store(addr, v, ir_elem_stride(vi));
                            k++;
                            if (cur_tok(cs) != TOK_COMMA) break;
                            next_tok(cs);
                        }
                    }
                }
                expect(cs, TOK_RBRACE);
            } else if (cur_tok(cs) == TOK_STRING_LITERAL && elems > 0) {
                /* 字符串数组初始化 s char[N] = "hello"：逐字节 STORE（含 NUL，
                 * 截断到容量 elems）；char 元素 vetyp=2（i8），字节值天然规范 */
                const char *str = cs->parser.lex->tok_str;
                size_t slen = strlen(str);
                next_tok(cs);
                /* PB-62：与 A 后端同判据。此前元素非 char 也照收、放不下就静默截断，
                 * 于是非法程序在 IR 线既不出诊断也不报错（台账 E-c 桶） */
                if (vt_code != 2) {
                    nihao_error(cs, "ir: cannot initialize array '%s' with a string literal; "
                                    "its elements are not 'char', use a value list {v0, v1, ...}",
                                name);
                } else if ((int)slen + 1 > elems) {
                    nihao_error(cs, "ir: string needs %d bytes with terminator, "
                                    "array '%s' holds %d",
                                (int)slen + 1, name, elems);
                }
                int cap = (int)slen + 1;
                if (cap > elems) cap = elems;
                for (int k = 0; k < cap; k++) {
                    int cv = ir_new_vreg(F);
                    ir_emit(F, IR_CONST, cv, -1, -1,
                            (unsigned char)(k < (int)slen ? str[k] : 0));
                    int addr = ir_arr_const_addr(vi, k);
                    ir_arr_store(addr, cv, 1);
                }
            } else if (vetyp[vi] == 8 && cur_tok(cs) == TOK_STRING_LITERAL) {
                /* PB-89：char[] 绑定字面量 → 复制到可写内存（元素写须成立） */
                const char *lit = nihao_strdup(g_cs, cs->parser.lex->tok_str);
                ve[vi] = (int)strlen(lit);
                vlen_known[vi] = 1;   /* PB-72 */
                int vr;
                if (vvis[vi] == VIS_CONST) {
                    vr = ir_expr(cs);           /* PB-90：const 绑只读池，无需副本 */
                } else {
                    next_tok(cs);
                    vr = ir_strlit_copy(cs, lit);
                }
                ir_emit(F, IR_MOV, vt[vi], vr, -1, 0);
            } else if (nt == TOK_STRING && cur_tok(cs) == TOK_STRING_LITERAL) {
                /* PB-70：声明型 `s string = "lit"` 也要登记逻辑长度。
                 * PB-89：同样复制到可写内存（string 元素写成立）。 */
                const char *lit = nihao_strdup(g_cs, cs->parser.lex->tok_str);
                ve[vi] = (int)strlen(lit);
                vlen_known[vi] = 1;   /* PB-72 */
                int vr;
                if (vvis[vi] == VIS_CONST) {
                    vr = ir_expr(cs);           /* PB-90：const 绑只读池，无需副本 */
                } else {
                    next_tok(cs);
                    vr = ir_strlit_copy(cs, lit);
                }
                ir_emit(F, IR_MOV, vt[vi], vr, -1, 0);
            } else {
                /* 切片读入声明数组：cp i32[3] = arr[1..4] —— 按目标容量复制
                 * （A 侧语义 memcpy(&cp, &arr[lo], elems*sizeof(T))：复制个数取
                 * 声明容量而非切片长度） */
                int copied = 0;
                if (elems > 0 && cur_tok(cs) == TOK_IDENTIFIER) {
                    LexerState *lx = cs->parser.lex;
                    lx->peek_valid = 0;
                    lexer_peek(lx);
                    TokenType after = lx->peek_tok;
                    lx->peek_valid = 0;
                    if (after == TOK_LBRACKET) {
                        int avi = var_find(cs->parser.lex->tok_str);
                        if (avi >= 0 && ve[avi] > 0) {
                            next_tok(cs);   /* arr */
                            next_tok(cs);   /* [ */
                            int lo = ir_expr(cs);
                            if (cur_tok(cs) != TOK_RANGE) {
                                nihao_error(cs, "ir: array '%s' must be initialized "
                                                "from a slice 'arr[lo..hi]'", name);
                                skip_newlines(cs);
                                return;
                            }
                            next_tok(cs);   /* .. */
                            ir_expr(cs);    /* hi：复制个数取目标容量，边界忽略 */
                            expect(cs, TOK_RBRACKET);
                            int swid = ir_elem_stride(avi);
                            int sis8 = (swid == 1);
                            for (int k = 0; k < elems; k++) {
                                int ik = ir_new_vreg(F);
                                if (k == 0) {
                                    ir_emit(F, IR_MOV, ik, lo, -1, 0);
                                } else {
                                    int kv = ir_new_vreg(F);
                                    ir_emit(F, IR_CONST, kv, -1, -1, k);
                                    ir_emit(F, IR_ADD, ik, lo, kv, 0);
                                }
                                int saddr = ir_elem_addr(cs, avi, vt[avi], ik, swid, sis8);
                                int val = ir_new_vreg(F);
                                ir_arr_load(val, saddr, swid, vetyp[avi]);
                                int daddr = ir_arr_const_addr(vi, k);
                                ir_arr_store(daddr, val, ir_elem_stride(vi));
                            }
                            copied = 1;
                        }
                    } else if (after == TOK_DOT_PAREN) {
                        /* PB-68：指针后缀链切片读入 buf T[N] = p.(T[M])[lo..hi]。
                         * 源地址由链求值给出（指针值寻址），复制个数仍取目标容量。 */
                        int avi = var_find(cs->parser.lex->tok_str);
                        if (avi >= 0 && ir_chain_needs_new_off(cs, avi, 1)) {
                            next_tok(cs);   /* p */
                            IrChain ch;
                            if (!ir_chain(cs, avi, &ch)) {
                                skip_newlines(cs);
                                return;
                            }
                            if (ch.is_slice) {
                                int s8 = (ch.width == 1);
                                for (int k = 0; k < elems; k++) {
                                    int saddr = ch.val;
                                    if (k > 0) {
                                        int kv = ir_new_vreg(F);
                                        ir_emit(F, IR_CONST, kv, -1, -1, k * ch.width);
                                        saddr = ir_new_vreg(F);
                                        ir_emit(F, IR_ADD, saddr, ch.val, kv, 0);
                                    }
                                    int val = ir_new_vreg(F);
                                    if (s8) ir_emit(F, IR_LOAD8, val, saddr, -1, 0);
                                    else ir_emit(F, IR_LOAD, val, saddr, -1, 0);
                                    int daddr = ir_arr_const_addr(vi, k);
                                    ir_arr_store(daddr, val, ir_elem_stride(vi));
                                }
                            } else {
                                int vr = ir_coerce(ch.val, vtype[vi]);
                                ir_emit(F, IR_MOV, vt[vi], vr, -1, 0);
                            }
                            copied = 1;
                        }
                    }
                }
                if (!copied) {
                    /* PB-77：声明处 `p void = &x` / `q void = &pt` 也要记指向类型——
                     * 赋值语句路径早已登记（供 `->`/`.()`），声明路径此前漏掉，
                     * 于是 `q->x` 报 "not a pointer to aggregate"。
                     * 口径与赋值路径相同：&聚合 → 聚合索引；&标量 → PT_SCALAR(code)。 */
                    if (cur_tok(cs) == TOK_BITWISE_AND) {
                        LexerState *lx2 = cs->parser.lex;
                        lx2->peek_valid = 0;
                        lexer_peek(lx2);
                        if (lx2->peek_tok == TOK_IDENTIFIER &&
                            ir_peek_ahead(cs, 2) != TOK_DOT &&
                            ir_peek_ahead(cs, 2) != TOK_LBRACKET) {
                            int avi = var_find(lx2->peek_str);
                            if (avi >= 0) {
                                if (vty[avi] >= 0 &&
                                    agg_types[vty[avi]].kind != 2) {
                                    pt[vi] = vty[avi];
                                } else if (vty[avi] < 0) {
                                    /* PB-82：数组取元素类型码（i32[3] → 4），标量取 vtype */
                                    pt[vi] = PT_SCALAR(ve[avi] > 0 ? vetyp[avi]
                                                                   : vtype[avi]);
                                }
                                vfrm[vi] = 1;   /* PB-81：帧地址来源 */
                            }
                        }
                        lx2->peek_valid = 0;
                    }
                    int vr = ir_expr(cs);
                    vr = ir_coerce(vr, vtype[vi]);   /* PB-1：目标类型协调（DTOI/ITOD/TRUNC） */
                    ir_emit(F, IR_MOV, vt[vi], vr, -1, 0);
                    /* PB-75(b)：`p void = malloc(i32)` 此前丢指向类型——RHS 解析完后
                     * 补记 pt[]，让 .() 读/写按元素宽（修 PB-48 D1 堆越界） */
                    if (nt == TOK_VOID && call_name &&
                        strcmp(call_name, "malloc") == 0 && last_malloc_sc >= 0) {
                        pt[vi] = PT_SCALAR(last_malloc_sc);
                    }
                    last_malloc_sc = -2;
                }
            }
            skip_newlines(cs);
        } else if (nt == TOK_LBRACKET) {
            /* 数组元素赋值 arr[idx] = expr | 切片赋值 arr[lo..hi] = {v0, v1, ...} */
            next_tok(cs);           /* name */
            int vi = var_find(name);
            if (vi < 0) {
                nihao_error(cs, "ir: undeclared variable '%s'", name);
                skip_newlines(cs);
                return;
            }
            if (!vptr[vi] && vmode[vi] != VM_PTRARR && !ir_check_const_store(cs, vi)) {
                /* PB-90：const 数组/字符串的自有存储不可写 */
                ir_skip_line(cs);
                return;
            }
            if (ir_chain_needs_new(cs, vi)) {   /* PB-68：多步链 / 新形态 */
                ir_chain_assign(cs, vi);
                return;
            }
            ir_check_void_subscript(cs, vi);   /* PB-72 */
            next_tok(cs);           /* [ */
            int idx = ir_expr(cs);   /* lo 或单下标 */
            if (cur_tok(cs) == TOK_RANGE) {
                /* 切片赋值：arr[lo..hi] = {v0, v1, ...}（逐个 STORE；hi 边界留校验） */
                next_tok(cs);
                int hi = ir_expr(cs);
                (void)hi;
                expect(cs, TOK_RBRACKET);
                expect(cs, TOK_ASSIGN);
                if (cur_tok(cs) == TOK_STRING_LITERAL) {
                    /* PB-83：切片字符串赋值（数组侧，逐字节）；PB-84：容量校验 */
                    const char *lit = nihao_strdup(g_cs, cs->parser.lex->tok_str);
                    long long lov, hiv;
                    if (vreg_const(F, idx, &lov) && vreg_const(F, hi, &hiv)) {
                        int cap = (int)(hiv - lov) + 1;
                        int need = (int)strlen(lit) + 1;
                        if (need > cap)
                            nihao_error(cs, "ir: string needs %d bytes with terminator, "
                                            "slice holds %d", need, cap);
                    }
                    next_tok(cs);
                    int d = ir_elem_addr(cs, vi, vt[vi], idx, ir_elem_stride(vi),
                                         vetyp[vi] == 8);
                    ir_copy_literal(cs, d, lit);
                    skip_newlines(cs);
                    return;
                }
                if (cur_tok(cs) == TOK_LBRACE) {
                    next_tok(cs);
                    skip_newlines(cs);
                    int k = 0;
                    if (cur_tok(cs) != TOK_RBRACE) {
                        for (;;) {
                            int v = ir_expr(cs);
                            v = ir_coerce(v, vetyp[vi] == 8 ? 2 : vetyp[vi]);
                            int offk = ir_new_vreg(F);
                            ir_emit(F, IR_CONST, offk, -1, -1, k);
                            int ik = ir_new_vreg(F);
                            ir_emit(F, IR_ADD, ik, idx, offk, 0);
                            int addr = ir_elem_addr(cs, vi, vt[vi], ik, ir_elem_stride(vi), vetyp[vi] == 8);
                            ir_arr_store(addr, v, ir_elem_stride(vi));
                            k++;
                            if (cur_tok(cs) != TOK_COMMA) break;
                            next_tok(cs);
                            skip_newlines(cs);
                        }
                    }
                    expect(cs, TOK_RBRACE);
                } else {
                    nihao_error(cs, "ir: slice assignment expects { v0, v1, ... }");
                }
                skip_newlines(cs);
                return;
            }
            expect(cs, TOK_RBRACKET);
            expect(cs, TOK_ASSIGN);
            int v = ir_expr(cs);
            v = ir_coerce(v, vetyp[vi] == 8 ? 2 : vetyp[vi]);   /* PB-1：元素类型协调（vetyp 8=char 元素，与 vtype 8=f32 编码冲突→按 i8 截断） */
            int addr = ir_elem_addr(cs, vi, vt[vi], idx, ir_elem_stride(vi), vetyp[vi] == 8);
            ir_arr_store(addr, v, ir_elem_stride(vi));
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
        if (!ir_check_const_store(cs, vi)) {   /* PB-90：const 不可 ++/-- */
            ir_skip_line(cs);
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
        /* 一元解引用 *p（读/写）已从 1.0.x/2.0 指针语法移除（统一 .()/.(T)/->）。
         * 吞掉 * 与后续行，防解析死循环。 */
        nihao_error(cs, "ir: unary dereference '*p' removed; use p.() instead");
        next_tok(cs);
        skip_newlines(cs);
        return;
    } else if (t == TOK_COOKING) {
        /* 函数内编译期块：执行 static_assert，其余跳过 */
        ir_cooking(cs);
    } else if (t == TOK_LBRACE && has_prefix) {
        /* 多变量声明：var {a=0, b=1} i8（有前缀的 { 必是多变量，非块语句） */
        ir_multi_decl(cs, decl_vis);
    } else if (t == TOK_LBRACE) {
        ir_block(cs);
    } else {
        /* 表达式语句（如 puts 调用） */
        ir_expr(cs);
        skip_newlines(cs);
    }
}

/* ---- 函数 ---- */

/* <bracket-attr-list> ::= "[[" <bracket-attr> { "," <bracket-attr> } "]]"（BNF §7）。
 * 词法器把 `[[` / `]]` 当作两个独立 token，故先前瞻一个 `[` 才确认这是属性表而不是下标。
 * 判据与 A 后端 parse_declaration 的属性分支逐字对应：同一组名字、同一条兜底诊断、
 * export 的段名字面量同样只消费不落码。 */
static int ir_bracket_attr_list(CompilerState *cs)
{
    LexerState *lx = cs->parser.lex;
    int mask = 0;

    lx->peek_valid = 0;
    lexer_peek(lx);
    if (lx->peek_tok != TOK_LBRACKET) {
        lx->peek_valid = 0;
        nihao_error(cs, "ir: '[' at top level must be doubled: '[[ ... ]]'");
        return -1;
    }
    lx->peek_valid = 0;
    next_tok(cs);       /* [ */
    next_tok(cs);       /* [ */
    while (cur_tok(cs) != TOK_RBRACKET && cur_tok(cs) != TOK_EOF) {
        if (cur_tok(cs) == TOK_IDENTIFIER) {
            const char *a = lx->tok_str;
            if (strcmp(a, "inline") == 0) mask |= IR_ATTR_INLINE;
            else if (strcmp(a, "weak") == 0) mask |= IR_ATTR_WEAK;
            else if (strcmp(a, "local") == 0) mask |= IR_ATTR_LOCAL;
            else if (strcmp(a, "used") == 0) mask |= IR_ATTR_USED;
            else if (strcmp(a, "unused") == 0) mask |= IR_ATTR_UNUSED;
            else if (strcmp(a, "export") != 0)
                nihao_error(cs, "ir: unknown bracket attribute '%s'; the grammar "
                            "allows local / inline / weak / used / unused / "
                            "export \"<section>\"", a);
            next_tok(cs);
            if (cur_tok(cs) == TOK_STRING_LITERAL) next_tok(cs);   /* export 的段名 */
        } else {
            next_tok(cs);
        }
        if (cur_tok(cs) == TOK_COMMA) next_tok(cs);
    }
    expect(cs, TOK_RBRACKET);
    expect(cs, TOK_RBRACKET);
    return mask;
}

/* 属性表后跟的不是 func：报一条专属诊断，再把这条声明的本行余下部分消费掉，
 * 避免逐 token 落到通用分支级联出第二条、第三条错。 */
static void ir_skip_rest_of_line(CompilerState *cs)
{
    while (cur_tok(cs) != TOK_NEWLINE && cur_tok(cs) != TOK_EOF) {
        next_tok(cs);
    }
    if (cur_tok(cs) == TOK_NEWLINE) next_tok(cs);
}

static void ir_func_named(CompilerState *cs, const char *fname, int pre_vis, int attrs)
{
    /* func name ( params ) ret
     * pre_vis：名字前的 return-attr 前缀（flow/static/const/var，BNF §7）；
     *           -1 = 无前缀（`func` 关键字形态）。 */
    int is_main = strcmp(fname, "main") == 0;
    F = ir_fn_new(P, fname, is_main);
    F->attrs = attrs;
    var_reset();
    for (int _i = 0; _i < 32; _i++) F->param_vis[_i] = VIS_VAR;  /* 默认 var（无前缀） */
    g_label_count = 0;      /* goto/label 表 per 函数清零 */

    expect(cs, TOK_LPAREN);
    skip_newlines(cs);
    int nparam = 0;
    const char *pnames[32];
    int ptypes[32] = {0};   /* 参数类型编码：0=i64 1=double 2=i8 3=i16 4=i32 5=u8 6=u16 7=u32 */
    int ptype_ti[32];       /* 参数聚合类型索引（PB-16 struct 参数；-1=标量） */
    int pvis[32];           /* 参数可见性前缀：0=var 1=const 2=flow 3=static（§12.3 M2） */
    int pptr[32];           /* 参数是否指针类（void 类型）：仅其有所有权语义 */
    for (int _i = 0; _i < 32; _i++) { ptype_ti[_i] = -1; pvis[_i] = VIS_VAR; pptr[_i] = 0; }
    if (cur_tok(cs) != TOK_RPAREN) {
        for (;;) {
            /* 可选参数可见性前缀 flow/var/const/static（§12.3 参数传递规则） */
            int pv = VIS_VAR;
            if (cur_tok(cs) == TOK_FLOW)        { pv = VIS_FLOW;   next_tok(cs); }
            else if (cur_tok(cs) == TOK_CONST)  { pv = VIS_CONST;  next_tok(cs); }
            else if (cur_tok(cs) == TOK_VAR)    { pv = VIS_VAR;    next_tok(cs); }
            else if (cur_tok(cs) == TOK_STATIC) { pv = VIS_STATIC; next_tok(cs); }

            /* param: name Type（先收集，注册延后——mr 隐藏参数须在头部） */
            if (cur_tok(cs) != TOK_IDENTIFIER) break;
            pnames[nparam] = cs->parser.lex->tok_str;
            pvis[nparam] = pv;
            next_tok(cs);
            TokenType pt = cur_tok(cs);
            ptypes[nparam] =
                (pt == TOK_F32 || pt == TOK_F64) ? 1 :
                (pt == TOK_CHAR || pt == TOK_I8) ? 2 :
                (pt == TOK_I16) ? 3 : (pt == TOK_I32) ? 4 :
                (pt == TOK_FX32) ? 4 :
                (pt == TOK_U8)  ? 5 : (pt == TOK_U16) ? 6 :
                (pt == TOK_U32) ? 7 : 0;
            if (pt == TOK_VOID) {
                pptr[nparam] = 1;       /* void = 通用指针：指针类（所有权语义） */
            } else if (pt == TOK_IDENTIFIER) {
                int ti = agg_type_find(cs->parser.lex->tok_str);
                if (ti >= 0) {
                    ptype_ti[nparam] = ti;   /* struct 参数（按值展开） */
                    ptypes[nparam] = 0;
                }
            }
            nparam++;
            next_tok(cs);           /* 跳过类型 */
            if (cur_tok(cs) != TOK_COMMA) break;
            next_tok(cs);
        }
    }
    expect(cs, TOK_RPAREN);
    skip_newlines(cs);
    if (cur_tok(cs) != TOK_LBRACE) {
        /* PB-27：返回类型可见性前缀 flow/var/const/static（§12.3 后半段）
         *   flow  create_ptr() flow  void { ... return ptr }  // 转移所有权
         *   static get()      static void { return pool }    // 共享引用
         *   var/const: 借用（无前缀 = var 借用） */
        int rv = VIS_VAR;
        if (cur_tok(cs) == TOK_FLOW)        { rv = VIS_FLOW;   next_tok(cs); }
        else if (cur_tok(cs) == TOK_CONST)  { rv = VIS_CONST;  next_tok(cs); }
        else if (cur_tok(cs) == TOK_VAR)    { rv = VIS_VAR;    next_tok(cs); }
        else if (cur_tok(cs) == TOK_STATIC) { rv = VIS_STATIC; next_tok(cs); }
        else if (pre_vis >= 0)              { rv = pre_vis; }
        F->ret_vis = rv;
        /* struct 返回 = sret：具名聚合类型 → 隐藏 out-param 机制
         * （返回聚合值经 _mr_ret 缓冲指针，调用方 malloc+拷贝） */
        if (cur_tok(cs) == TOK_IDENTIFIER) {
            int rti = agg_type_find(cs->parser.lex->tok_str);
            if (rti >= 0) {
                F->is_mr = 1;
                F->ret_agg_ti = rti;
            }
        }
        F->ret_is_double =
            (cur_tok(cs) == TOK_F32 || cur_tok(cs) == TOK_F64) ? 1 : 0;
        next_tok(cs);           /* 其他返回类型 */
    } else if (pre_vis >= 0) {
        F->ret_vis = pre_vis;  /* 无显式返回类型，但名字前有 return-attr 前缀 */
    }
    /* 注册变量：mr 时 _mr_ret 必须是第 0 槽（调用约定：缓冲指针为第一参数）。
     * PB-16 struct 参数按值展开：聚合参数占 mcount 个虚拟参数槽（成员槽 vreg
     * 连续，prologue 按展开索引入槽） */
    int vpi = 0;
    if (F->is_mr) {
        var_declare("_mr_ret", 0, -1, VIS_VAR, 0);
        F->param_agg_ti[0] = -1;
        vpi = 1;
    }
    /* 两遍注册（0.2.2 回归修复）：第一遍只登记 ABI 参数槽（聚合按 8 字节块），
     * 保证前 vpi 个 vreg 恰为参数槽；第二遍再为聚合参数建字节布局块并搬入——
     * 块变量必须落在参数区之后，否则后续标量参数会被绑进块成员里。 */
    int aggn = 0, agg_pi[32], agg_ti[32], agg_st[32];
    for (int pi = 0; pi < nparam; pi++) {
        F->param_vis[pi] = pvis[pi];     /* M2 调用点检查用（声明序，与 param 对齐） */
        int ti = ptype_ti[pi];
        if (ti >= 0) {
            int chunks = agg_types[ti].bytes > 0 ? (agg_types[ti].bytes + 7) / 8 : 1;
            if (aggn < 32) {
                agg_pi[aggn] = pi; agg_ti[aggn] = ti; agg_st[aggn] = vpi; aggn++;
            }
            for (int k = 0; k < chunks; k++) {
                /* 名字必须堆分配：var_declare 存的是指针，栈缓冲会在下一轮被覆写，
                 * 三轮声明撞成同名 → 只分配一个槽（0.2.2 实测踩到）。 */
                char *pn = nihao_malloc(g_cs, 64);
                snprintf(pn, 64, "__parg%d_%d", pi, k);
                var_declare(pn, 0, -1, VIS_VAR, 0);
                F->param_types[vpi + k] = 0;
            }
            F->param_agg_ti[vpi] = ti;
            for (int k = 1; k < chunks; k++) F->param_agg_ti[vpi + k] = -1;
            vpi += chunks;
        } else {
            int vi = var_declare(pnames[pi], 0, -1, pvis[pi], 0);
            if (pptr[pi]) vptr[vi] = 1;       /* 指针类参数（void）：标记所有权语义 */
            F->param_types[vpi] = ptypes[pi];
            F->param_agg_ti[vpi] = -1;
            if (ptypes[pi] == 1) {
                vtype[vi] = 1;          /* 浮点参数 → double 槽 */
                ir_set_double(vt[vi]);
            } else {
                vtype[vi] = ptypes[pi]; /* 窄整数参数记录类型（值已符号扩展，入槽无需截断） */
            }
            vpi++;
        }
    }
    /* 第二遍：聚合参数块（在参数区之后分配），把 ABI 块搬入字节布局块 */
    for (int a = 0; a < aggn; a++) {
        int pi = agg_pi[a], ti = agg_ti[a], pstart = agg_st[a];
        int chunks = agg_types[ti].bytes > 0 ? (agg_types[ti].bytes + 7) / 8 : 1;
        int vi = var_declare(pnames[pi], chunks, ti, pvis[pi], 0);
        vblk[vi] = chunks;
        for (int k = 0; k < chunks; k++) {
            int addr = ir_ptr_byte_off(ir_agg_base(vi), (long long)k * 8);
            ir_emit(F, IR_STORE, -1, addr, pstart + k, 0);
        }
    }
    F->param_count = vpi;
    ir_block(cs);
    ir_fn_end(F);
}

/* `func` 关键字形态的函数定义（return-attr 为 func，无所有权语义） */
static void ir_func(CompilerState *cs, int attrs)
{
    const char *fname = cs->parser.lex->tok_str;
    next_tok(cs);
    ir_func_named(cs, fname, -1, attrs);
}

/* ---- 顶层声明：alias / 全局变量 / 带前缀的函数定义 ---- */

/* 顶层 alias：alias Name = Type（BNF §2.2） */
static void ir_alias_decl(CompilerState *cs)
{
    next_tok(cs);                 /* alias */
    if (cur_tok(cs) != TOK_IDENTIFIER) {
        nihao_error(cs, "ir: expected a name after 'alias'");
        ir_skip_rest_of_line(cs);
        return;
    }
    const char *aname = cs->parser.lex->tok_str;
    next_tok(cs);
    expect(cs, TOK_ASSIGN);
    int sc = -1, ati = -1;
    if (is_type_token(cur_tok(cs))) {
        sc = ir_scalar_code_of_token(cur_tok(cs));
        next_tok(cs);
    } else if (cur_tok(cs) == TOK_IDENTIFIER) {
        const char *tn = cs->parser.lex->tok_str;
        int ti = agg_type_find(tn);
        if (ti >= 0) {
            ati = ti;
        } else {
            int ai = alias_find(tn);
            if (ai >= 0) {
                sc = ir_aliases[ai].scalar_code;
                ati = ir_aliases[ai].agg_ti;
            } else {
                nihao_error(cs, "ir: alias '%s' refers to unknown type '%s'",
                            aname, tn);
                next_tok(cs);
                ir_skip_rest_of_line(cs);
                return;
            }
        }
        next_tok(cs);
    } else {
        nihao_error(cs, "ir: expected a type after 'alias %s ='", aname);
        ir_skip_rest_of_line(cs);
        return;
    }
    if (ir_alias_count >= IR_MAX_ALIAS) {
        nihao_error(cs, "ir: too many type aliases");
        ir_skip_rest_of_line(cs);
        return;
    }
    ir_aliases[ir_alias_count].name = aname;
    ir_aliases[ir_alias_count].scalar_code = sc;
    ir_aliases[ir_alias_count].agg_ti = ati;
    ir_alias_count++;
}

/* 顶层变量声明：name <type> [= <常量表达式>] → 落进全局变量池（跨函数共享） */
static void ir_top_var(CompilerState *cs, const char *name, int vis)
{
    int tcode = 0;
    int is_double = 0;
    if (is_type_token(cur_tok(cs))) {
        tcode = ir_scalar_code_of_token(cur_tok(cs));
        is_double = (tcode == 1 || tcode == 8);
        next_tok(cs);
    } else if (cur_tok(cs) == TOK_IDENTIFIER) {
        const char *tn = cs->parser.lex->tok_str;
        int ai = alias_find(tn);
        int ti = agg_type_find(tn);
        if (ai >= 0) {
            tcode = ir_aliases[ai].scalar_code;
            is_double = (tcode == 1 || tcode == 8);
        } else if (ti >= 0) {
            nihao_error(cs, "ir: aggregate global '%s' is not supported yet", name);
            ir_skip_rest_of_line(cs);
            return;
        } else {
            nihao_error(cs, "ir: unknown type '%s' in global declaration", tn);
            ir_skip_rest_of_line(cs);
            return;
        }
        next_tok(cs);
    } else {
        nihao_error(cs, "ir: expected a type in the global declaration of '%s'",
                    name);
        ir_skip_rest_of_line(cs);
        return;
    }
    if (cur_tok(cs) == TOK_LBRACKET) {
        nihao_error(cs, "ir: global arrays are not supported yet");
        ir_skip_rest_of_line(cs);
        return;
    }
    long long init = 0;
    if (cur_tok(cs) == TOK_ASSIGN) {
        next_tok(cs);
        if (is_double && cur_tok(cs) == TOK_FLOAT_CONST) {
            union { double d; int64_t i; } u;
            u.d = cs->parser.lex->tok_val.f;
            init = u.i;
            next_tok(cs);
        } else {
            init = ir_const_expr(cs);
        }
    }
    if (ir_global_count >= IR_MAX_GLOBAL) {
        nihao_error(cs, "ir: too many global variables");
        ir_skip_rest_of_line(cs);
        return;
    }
    int sym = ir_add_global(P, (int64_t)init, is_double);
    ir_globals[ir_global_count].name = name;
    ir_globals[ir_global_count].sym_idx = sym;
    ir_globals[ir_global_count].is_double = is_double;
    ir_globals[ir_global_count].type_code = tcode;
    ir_globals[ir_global_count].vis = vis;
    ir_global_count++;
}

/* 顶层带前缀的声明：<return-attr> <identifier> ...
 * 名字后紧跟 '(' → 函数定义；否则 → 全局变量声明。 */
static void ir_top_decl(CompilerState *cs, int vis, int attrs)
{
    (void)attrs;   /* 属性表对全局变量在 IR 落地上无影响（导出段名等留给后端） */
    if (cur_tok(cs) != TOK_IDENTIFIER) {
        nihao_error(cs, "ir: expected a name after the declaration prefix");
        ir_skip_rest_of_line(cs);
        return;
    }
    const char *name = cs->parser.lex->tok_str;
    next_tok(cs);
    if (cur_tok(cs) == TOK_LPAREN) {
        ir_func_named(cs, name, vis, attrs);
        return;
    }
    ir_top_var(cs, name, vis);
}

/* 顶层类型定义：Name struct { fields } / Name union { fields } / Name enum { A, B } */
/* PB-80：名字已消费、cur_tok 停在 struct/union/enum 上（顶层与函数体内共用） */
static void ir_type_decl_named(CompilerState *cs, const char *tname)
{
    TokenType kw = cur_tok(cs);
    next_tok(cs);   /* struct/union/enum */
    if (agg_count >= IR_MAX_AGG) {
        nihao_error(cs, "ir: too many aggregate types");
        return;
    }
    IrAggType *a = &agg_types[agg_count];
    memset(a, 0, sizeof(*a));
    a->name = tname;
    a->kind = (kw == TOK_STRUCT) ? 0 : (kw == TOK_UNION) ? 1 : 2;
    if (a->kind == 2) {
        /* enum { A, B = 5, C }：值递增，可显式指定 */
        expect(cs, TOK_LBRACE);
        long long val = 0;
        skip_newlines(cs);
        while (cur_tok(cs) != TOK_RBRACE && cur_tok(cs) != TOK_EOF) {
            if (cur_tok(cs) != TOK_IDENTIFIER) {
                nihao_error(cs, "ir: expected enum variant name");
                next_tok(cs);
                continue;
            }
            a->mnames[a->mcount] = cs->parser.lex->tok_str;
            next_tok(cs);
            if (cur_tok(cs) == TOK_ASSIGN) {
                next_tok(cs);
                if (cur_tok(cs) == TOK_INT_CONST) {
                    val = cs->parser.lex->tok_val.i;
                    next_tok(cs);
                }
            }
            a->mvals[a->mcount] = val;
            a->mcount++;
            val++;
            if (cur_tok(cs) == TOK_COMMA) next_tok(cs);
            skip_newlines(cs);
        }
        expect(cs, TOK_RBRACE);
    } else {
        /* struct/union { [vis] name Type [:bits] [= def] }
         * IR 8 字节槽模型：成员按声明序分配槽（union 共享槽 0）；
         * 位宽/默认值解析后忽略（不生成指令）。 */
        expect(cs, TOK_LBRACE);
        skip_newlines(cs);
        while (cur_tok(cs) != TOK_RBRACE && cur_tok(cs) != TOK_EOF) {
            if (cur_tok(cs) == TOK_CONST || cur_tok(cs) == TOK_FLOW ||
                cur_tok(cs) == TOK_STATIC || cur_tok(cs) == TOK_VAR) {
                next_tok(cs);
            }
            if (cur_tok(cs) != TOK_IDENTIFIER) {
                nihao_error(cs, "ir: expected member name");
                next_tok(cs);
                skip_newlines(cs);
                continue;
            }
            a->mnames[a->mcount] = cs->parser.lex->tok_str;
            next_tok(cs);
            /* 成员类型：自定义聚合类型（IDENTIFIER）记录索引，否则基本类型
             * （可带数组后缀 [N]，记录元素数与元素宽）。
             * 注意：struct 体内换行不产生 NEWLINE（brace_depth>0），
             * 遇非类型 token（下一成员名/冒号/赋值）即成员结束。 */
            a->mtype[a->mcount] = -1;
            a->marr_len[a->mcount] = 0;
            a->marr_esize[a->mcount] = 0;
            a->msize[a->mcount] = 0;
            a->mscode[a->mcount] = -1;
            a->mbit[a->mcount] = -1;
            a->misarr[a->mcount] = 0;
            if (cur_tok(cs) == TOK_IDENTIFIER) {
                int ti2 = agg_type_find(cs->parser.lex->tok_str);
                if (ti2 >= 0) {
                    a->mtype[a->mcount] = ti2;
                    next_tok(cs);
                } else {
                    /* PB-88：成员缺类型（`age` 后直接接下一成员名）——与 A 后端同判据 */
                    int ln = cs->parser.lex->last_line_num;
                    nihao_error(cs, "ir: unknown type '%s'", cs->parser.lex->tok_str);
                    while (cur_tok(cs) != TOK_RBRACE && cur_tok(cs) != TOK_EOF &&
                           cs->parser.lex->last_line_num == ln)
                        next_tok(cs);
                }
            } else if (is_type_token(cur_tok(cs))) {
                int esize = (int)type_size_token(cur_tok(cs));
                a->mscode[a->mcount] = ir_scalar_code_of_token(cur_tok(cs));   /* PB-76 */
                next_tok(cs);
                if (cur_tok(cs) == TOK_LBRACKET) {
                    /* 数组成员 name char[32]：记录元素数与元素宽。
                     * 槽模型下该成员仍只占 1 个 8 字节槽（真实布局待裁定），
                     * 元素寻址越出首槽时由 ir_member_elem_addr 诊断。 */
                    next_tok(cs);
                    long long n = 0;
                    if (cur_tok(cs) == TOK_INT_CONST) {
                        n = cs->parser.lex->tok_val.i;
                        next_tok(cs);
                    }
                    expect(cs, TOK_RBRACKET);
                    a->misarr[a->mcount] = 1;
                    a->marr_len[a->mcount] = (int)n;
                    a->marr_esize[a->mcount] = esize > 0 ? esize : 1;
                } else {
                    /* PB-74：标量成员也记下字节宽（alignof 与真实布局用） */
                    a->msize[a->mcount] = esize > 0 ? esize : 8;
                }
            }
            if (cur_tok(cs) == TOK_COLON) {        /* 位域 :N——记录宽度（槽内低 N 位） */
                next_tok(cs);
                if (cur_tok(cs) == TOK_INT_CONST) {
                    long long bw = cs->parser.lex->tok_val.i;
                    a->mbits[a->mcount] = (bw > 0 && bw < 64) ? (int)bw : 0;
                    next_tok(cs);
                }
            }
            if (cur_tok(cs) == TOK_ASSIGN) {
                /* PB-79：默认值——整型 / 浮点 / 字符串字面量 / 枚举常量（可带负号）。
                 * 此前只「解析后忽略」，默认值从不生效。 */
                int mi = a->mcount;
                next_tok(cs);
                int neg = 0;
                if (cur_tok(cs) == TOK_MINUS) { neg = 1; next_tok(cs); }
                if (cur_tok(cs) == TOK_INT_CONST) {
                    long long iv = cs->parser.lex->tok_val.i;
                    a->mdef_int[mi] = neg ? -iv : iv;
                    a->mdef_kind[mi] = 1;
                    next_tok(cs);
                } else if (cur_tok(cs) == TOK_FLOAT_CONST) {
                    double dv = cs->parser.lex->tok_val.f;
                    a->mdef_dbl[mi] = neg ? -dv : dv;
                    a->mdef_kind[mi] = 2;
                    next_tok(cs);
                } else if (!neg && cur_tok(cs) == TOK_STRING_LITERAL) {
                    a->mdef_str[mi] = nihao_strdup(g_cs, cs->parser.lex->tok_str);
                    a->mdef_kind[mi] = 3;
                    next_tok(cs);
                } else if (!neg && cur_tok(cs) == TOK_IDENTIFIER) {
                    long long ev = 0;
                    if (enum_const_find(cs->parser.lex->tok_str, &ev)) {
                        a->mdef_int[mi] = ev;
                        a->mdef_kind[mi] = 1;
                    } else {
                        nihao_error(cs, "ir: member default value: unknown identifier '%s'",
                                    cs->parser.lex->tok_str);
                    }
                    next_tok(cs);
                } else {
                    nihao_error(cs, "ir: member default value: only simple constant supported");
                }
            }
            a->mcount++;
            skip_newlines(cs);
        }
        expect(cs, TOK_RBRACE);
    }
    agg_compute_offsets(agg_count);   /* 嵌套：递归算成员偏移/总槽数 */
    agg_count++;
}

static void ir_type_decl(CompilerState *cs)
{
    const char *tname = cs->parser.lex->tok_str;
    next_tok(cs);   /* name */
    ir_type_decl_named(cs, tname);
}

static void ir_parse_top_level(CompilerState *cs);
static void ir_use_decl(CompilerState *cs);
static void ir_use_module(CompilerState *cs, const char *name);
static void ir_parse_module_source(CompilerState *cs, const char *filename);

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
    while (cur_tok(cs) == TOK_USE) ir_use_decl(cs);

    ir_parse_top_level(cs);
    return 0;
}

/* ---- PB-73：模块加载（把模块顶层声明内联进同一个 IrProg，与 A 后端 module_import 同语义） ---- */

static char **ir_imported;      /* 已导入模块名（点号形式；循环守卫 + 去重） */
static int ir_imported_count;
static int ir_imported_cap;

static int ir_module_seen(const char *name)
{
    for (int i = 0; i < ir_imported_count; i++)
        if (strcmp(ir_imported[i], name) == 0) return 1;
    return 0;
}

static void ir_module_mark(const char *name)
{
    if (ir_module_seen(name)) return;
    if (ir_imported_count >= ir_imported_cap) {
        ir_imported_cap = ir_imported_cap ? ir_imported_cap * 2 : 8;
        ir_imported = nihao_realloc(g_cs, ir_imported, ir_imported_cap * sizeof(char *));
    }
    ir_imported[ir_imported_count++] = nihao_strdup(g_cs, name);
}

/* 按 use 名解析模块文件：cwd / 源文件目录 / stdlib 三层候选，点号名按目录层级（a.b → a/b.nc）。
 * 命中返回 1 并写出路径；不命中返回 0（外部 / 内建模块：无可内联的东西）。 */
static int ir_module_resolve(const char *name, const char *input_file, char *out, size_t outsz)
{
    char slashed[512];
    size_t si = 0;
    for (const char *q = name; *q && si + 1 < sizeof(slashed); q++)
        slashed[si++] = (*q == '.') ? '/' : *q;
    slashed[si] = '\0';

    char dirbuf[512] = "";
    if (input_file) {
        const char *slash = strrchr(input_file, '/');
        const char *bslash = strrchr(input_file, '\\');
        const char *sep = (bslash && (!slash || bslash > slash)) ? bslash : slash;
        if (sep) {
            int len = (int)(sep - input_file);
            if (len > 0 && len < (int)sizeof(dirbuf) - 1) {
                memcpy(dirbuf, input_file, len);
                dirbuf[len] = '\0';
            }
        }
    }

    char cands[3][512];
    int n = 0;
    snprintf(cands[n++], 512, "%s.nc", slashed);
    if (dirbuf[0]) snprintf(cands[n++], 512, "%s/%s.nc", dirbuf, slashed);
    snprintf(cands[n++], 512, "stdlib/%s.nc", slashed);

    for (int i = 0; i < n; i++) {
        FILE *probe = fopen(cands[i], "rb");
        if (!probe) continue;
        fclose(probe);
        snprintf(out, outsz, "%s", cands[i]);
        return 1;
    }
    return 0;
}

/* `use <name>` 声明：累积点号多段名后触发导入（源文件头与顶层循环两处共用）。 */
static void ir_use_decl(CompilerState *cs)
{
    next_tok(cs);                 /* use */
    char use_name[256];
    use_name[0] = '\0';
    if (cur_tok(cs) == TOK_IDENTIFIER) {
        snprintf(use_name, sizeof(use_name), "%s", cs->parser.lex->tok_str);
        next_tok(cs);
        while (cur_tok(cs) == TOK_DOT) {
            next_tok(cs);
            if (cur_tok(cs) != TOK_IDENTIFIER) break;
            size_t len = strlen(use_name);
            if (len + 1 + strlen(cs->parser.lex->tok_str) < sizeof(use_name))
                snprintf(use_name + len, sizeof(use_name) - len, ".%s",
                         cs->parser.lex->tok_str);
            next_tok(cs);
        }
    }
    if (use_name[0]) ir_use_module(cs, use_name);
    skip_newlines(cs);
}

/* 递归导入一个模块：解析其头部（module / 嵌套 use），再把顶层声明喂进同一个 IrProg。
 * 保存 / 还原 lexer（与 A 侧同款），错误定位自动落在模块自己的文件名与行号上。 */
static void ir_parse_module_source(CompilerState *cs, const char *filename)
{
    size_t src_size = 0;
    char *source = load_source_file(filename, &src_size);
    if (!source) return;

    LexerState *saved_lex = cs->parser.lex;
    LexerState *mod_lex = nihao_malloc(cs, sizeof(LexerState));
    memset(mod_lex, 0, sizeof(*mod_lex));
    cs->parser.lex = mod_lex;
    lexer_init(cs, filename, source);

    next_tok(cs);
    skip_newlines(cs);
    if (cur_tok(cs) == TOK_MODULE) {
        next_tok(cs);
        next_tok(cs);             /* 模块名 */
        skip_newlines(cs);
    }
    while (cur_tok(cs) == TOK_USE) ir_use_decl(cs);
    ir_parse_top_level(cs);

    cs->parser.lex = saved_lex;
}

static void ir_use_module(CompilerState *cs, const char *name)
{
    if (strcmp(name, "stdio") == 0 || strncmp(name, "stdio.", 6) == 0) {
        stdio_imported = 1;       /* PB-77：stdio 是内建标准库模块，use 即登记导入 */
        return;
    }
    if (ir_module_seen(name)) return;
    char path[512];
    if (ir_module_resolve(name, cs->input_file, path, sizeof(path))) {
        ir_module_mark(name);     /* 先标记：循环 import 靠它挡 */
        ir_parse_module_source(cs, path);
    }
    /* 解析不到文件 = 外部 / 内建模块：无可内联，静默跳过 */
}

/* 顶层声明列表（PB-73：抽成可重入，模块递归导入复用）。 */
static void ir_parse_top_level(CompilerState *cs)
{
    for (;;) {
        skip_newlines(cs);
        if (cur_tok(cs) == TOK_EOF) break;
        int attrs = 0;
        if (cur_tok(cs) == TOK_LBRACKET) {
            /* <bracket-attr-list>（BNF §7）：属性表可与其声明分行 */
            attrs = ir_bracket_attr_list(cs);
            if (attrs < 0) {
                /* 单个 `[` 不是属性表：整行消费，一条错不级联成两三条 */
                ir_skip_rest_of_line(cs);
                continue;
            }
            skip_newlines(cs);
        }
        if (cur_tok(cs) == TOK_FUNC) {
            next_tok(cs);
            ir_func(cs, attrs);
        } else if (cur_tok(cs) == TOK_ALIAS) {
            /* 顶层类型别名：alias Name = Type */
            ir_alias_decl(cs);
        } else if (cur_tok(cs) == TOK_FLOW || cur_tok(cs) == TOK_CONST ||
                   cur_tok(cs) == TOK_STATIC || cur_tok(cs) == TOK_VAR) {
            /* 顶层声明前缀（return-attr / 可见性）：名字后是 '(' → 函数定义，
             * 否则 → 全局变量声明 */
            int vis = (cur_tok(cs) == TOK_FLOW) ? VIS_FLOW :
                      (cur_tok(cs) == TOK_CONST) ? VIS_CONST :
                      (cur_tok(cs) == TOK_STATIC) ? VIS_STATIC : VIS_VAR;
            next_tok(cs);
            ir_top_decl(cs, vis, attrs);
        } else if (cur_tok(cs) == TOK_IDENTIFIER) {
            /* 可能是类型定义：Name struct/union/enum；也可能是顶层变量：name <type> */
            LexerState *lx = cs->parser.lex;
            lx->peek_valid = 0;
            lexer_peek(lx);
            TokenType nxt = lx->peek_tok;
            lx->peek_valid = 0;
            if (nxt == TOK_STRUCT || nxt == TOK_UNION || nxt == TOK_ENUM) {
                ir_type_decl(cs);
            } else if (is_type_token(nxt)) {
                const char *name = cs->parser.lex->tok_str;
                next_tok(cs);
                ir_top_var(cs, name, VIS_VAR);
            } else {
                nihao_error(cs, "ir: unexpected top-level identifier '%s'",
                            cs->parser.lex->tok_str);
                next_tok(cs);
            }
        } else if (cur_tok(cs) == TOK_COOKING) {
            /* 编译期块：执行 static_assert，其余跳过 */
            ir_cooking(cs);
        } else if (cur_tok(cs) == TOK_USE) {
            /* use 模块导入：把模块顶层声明内联进同一 IrProg（PB-73） */
            ir_use_decl(cs);
        } else if (cur_tok(cs) == TOK_LINK) {
            /* link "lib" [as] alias：静态库导入——IR 单文件模型解析跳过 */
            next_tok(cs);
            if (cur_tok(cs) == TOK_STRING_LITERAL) next_tok(cs);
            if (cur_tok(cs) == TOK_AS) next_tok(cs);
            if (cur_tok(cs) == TOK_IDENTIFIER) next_tok(cs);
            if (cur_tok(cs) == TOK_NEWLINE) next_tok(cs);
        } else if (cur_tok(cs) == TOK_ALIGN) {
            /* 对齐块：IR 槽模型下跳过 */
            ir_align_block(cs);
        } else if (cur_tok(cs) == TOK_SEMICOLON || cur_tok(cs) == TOK_POUND) {
            /* PB-70：顶层终止符 / 空声明（`module main;`、`use stdio;`、`Cfg struct {…};`、`}#`） */
            next_tok(cs);
            skip_newlines(cs);
        } else {
            nihao_error(cs, "ir: unsupported top-level token '%s'",
                        token_name(cur_tok(cs)));
            next_tok(cs);
        }
    }
}

/* ir-native 的宿主机器码后端：2026-09-26 裁定 IR 发射器只留 riscv64 与 loongarch64，
 * 其余宿主返回 NULL，由 ir_compile 改走 IR→C→宿主 cc（机器码由 C 路线代劳）。 */
static const char *ir_host_asm_backend(void)
{
#if defined(__riscv) && (__riscv_xlen == 64)
    return "riscv64";
#elif defined(__loongarch_lp64) || defined(__loongarch64__)
    return "loongarch64";
#else
    return NULL;
#endif
}

/* 后端统一入口 */
int ir_compile(CompilerState *cs, const char *filename, int backend, int verbose)
{
    (void)verbose;
    if (ir_parse_file(cs, filename) != 0) return -1;
    if (cs->error_count > 0) return -1;

    /* verbose: dump IR 指令序列（调试用） */
    if (verbose) {
        static const char *op_names[] = {
            "NOP", "CONST", "MOV", "ADD", "SUB", "MUL", "DIV", "MOD",
            "SHL", "SHR", "AND", "OR", "NEG", "NOT",
            "CMP_EQ", "CMP_NE", "CMP_LT", "CMP_LE", "CMP_GT", "CMP_GE",
            "FADD", "FSUB", "FMUL", "FDIV", "FCMP", "ITOD", "DTOI",
            "FTRUNC", "TRUNC", "JMP", "JZ", "JNZ", "CALL", "CALLI", "PARAM", "RET",
            "LABEL", "ALLOCA", "AGG_ALLOCA", "ADDR", "ADDR_LO", "ELEM_ADDR", "LOAD", "STORE",
            "LOAD8", "STORE8", "LD_ADDR", "END",
        };
        for (int fi = 0; fi < P->fn_count; fi++) {
            IrFn *f = &P->fns[fi];
            printf("IR fn: %s (params=%d, vregs=%d)\n",
                   f->name, f->param_count, f->vreg_count);
            for (int i = 0; i < f->ins_count; i++) {
                IrIns *in = &f->ins[i];
                int opi = (int)in->op;
                const char *on = (opi >= 0 && opi < (int)(sizeof(op_names) /
                                 sizeof(op_names[0]))) ? op_names[opi] : "?";
                printf("  %2d: %-8s dst=%2d a=%2d b=%2d imm=%lld%s%s\n",
                       i, on,
                       in->dst, in->a, in->b, (long long)in->imm,
                       in->sym ? " sym=" : "", in->sym ? in->sym : "");
            }
        }
    }

    /* ir-native 在无机器码发射器的宿主上回退到 IR→C，与 ir-c 同一条尾巴 */
    const char *host_asm = (backend == 3) ? ir_host_asm_backend() : NULL;
    int via_c = (backend == 2) || (backend == 3 && !host_asm);

    char out[1024];
    snprintf(out, sizeof(out), "%s.%s", cs->output_file ? cs->output_file : "a.out",
             via_c ? "c" : "s");
    if (via_c) {
        if (backend == 3 && cs->verbose)
            printf("ir-native: no IR asm emitter for this host, taking the IR->C route\n");
        if (irgen_c_emit(P, out) != 0) return -1;
    } else if (backend == 4 || backend == 5) {
        /* riscv64/loongarch64 交叉档：只生成汇编（宿主工具链汇编不了别家指令集） */
        const char *bn = (backend == 4) ? "riscv64" : "loongarch64";
        if (irgen_backend_emit(P, out, bn) != 0) return -1;
        if (cs->verbose) printf("%s asm written to %s (cross, not assembled)\n", bn, out);
        return 0;
    } else {
        if (irgen_backend_emit(P, out, host_asm) != 0) return -1;
    }

    /* 用 tcc 汇编/编译 + 链接为可执行 */
    char cmd[1600];
    snprintf(cmd, sizeof(cmd), "tcc \"%s\" -o \"%s\"", out,
             cs->output_file ? cs->output_file : "a.out");
    if (cs->verbose) printf("Invoking: %s\n", cmd);
    int rc = system(cmd);
    return rc == 0 ? 0 : -1;
}
