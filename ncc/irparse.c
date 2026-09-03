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

/* 可见性常量（与 cgen 的 enum nihao_vis 对齐）：NH_UNDEF=0,NH_CONST,NH_FLOW,NH_STATIC,NH_VAR */
#define VIS_VAR   0
#define VIS_CONST 1
#define VIS_FLOW  2
#define VIS_STATIC 3
#define VIS_UNDEF 4

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
static int var_declare(const char *name, int elems, int type_idx, int vis);
static int ir_expr(CompilerState *cs);

/* ---- struct/union/enum 类型表 ---- */
#define IR_MAX_AGG 16
#define IR_MAX_MEMB 32
typedef struct {
    const char *name;       /* 类型名（enum 时是枚举名） */
    int kind;               /* 0=struct 1=union 2=enum */
    const char *mnames[IR_MAX_MEMB];   /* 成员名（enum 为 variant 名） */
    long long mvals[IR_MAX_MEMB];      /* enum variant 值 */
    int mbits[IR_MAX_MEMB];            /* 位域宽度（0=非位域；槽内低 N 位） */
    int mtype[IR_MAX_MEMB];            /* 成员聚合类型索引（-1=标量；嵌套用） */
    int moff[IR_MAX_MEMB];             /* 成员起始槽偏移（递归展开） */
    int mcount;
    int mslots;                        /* 类型总槽数（递归；union=1 enum=1） */
} IrAggType;
static IrAggType agg_types[IR_MAX_AGG];
static int agg_count;

/* 递归计算成员起始槽偏移与总槽数（嵌套 struct；union=最大成员槽数，enum=1 槽；
 * mslots>0 表示已计算（memset 清零后首次计算）） */
static int agg_compute_offsets(int ti)
{
    IrAggType *a = &agg_types[ti];
    if (a->mslots > 0 || a->kind == 2) return a->kind == 2 ? 1 : a->mslots;
    if (a->kind == 1) {
        /* union：成员共享起始槽 0；总槽数 = 最大成员槽数（嵌套聚合成员，
         * 如 union { a Point b Point } = 2 槽——防链式访问越界） */
        int mx = 1;
        for (int i = 0; i < a->mcount; i++) {
            int ms = (a->mtype[i] >= 0) ? agg_compute_offsets(a->mtype[i]) : 1;
            if (ms > mx) mx = ms;
        }
        a->mslots = mx;
        return mx;
    }
    int off = 0;
    for (int i = 0; i < a->mcount; i++) {
        a->moff[i] = off;
        int ms = (a->mtype[i] >= 0) ? agg_compute_offsets(a->mtype[i]) : 1;
        off += ms;
    }
    a->mslots = off;
    return off;
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

/* ---- 位域（槽内低 N 位）---- */
static int ir_bitmask(int bits)
{
    long long m = (bits >= 64) ? -1 : ((1LL << bits) - 1);
    int v = ir_new_vreg(F);
    ir_emit(F, IR_CONST, v, -1, -1, m);
    return v;
}
/* 读位域：LOAD → AND mask（bits<=0 时原样 LOAD） */
static int ir_bf_load(int addr, int bits)
{
    int vr = ir_new_vreg(F);
    ir_emit(F, IR_LOAD, vr, addr, -1, 0);
    if (bits > 0) {
        int mv = ir_bitmask(bits);
        int r2 = ir_new_vreg(F);
        ir_emit(F, IR_AND, r2, vr, mv, 0);
        vr = r2;
    }
    return vr;
}
/* 写位域（RMW）：读槽 → AND ~mask → OR (v & mask) → STORE；bits<=0 直接 STORE */
static void ir_bf_store(int addr, int v, int bits)
{
    if (bits <= 0) {
        ir_emit(F, IR_STORE, -1, addr, v, 0);
        return;
    }
    int cur = ir_new_vreg(F);
    ir_emit(F, IR_LOAD, cur, addr, -1, 0);
    int mask = ir_bitmask(bits);
    int notm = ir_new_vreg(F);
    ir_emit(F, IR_NOT, notm, mask, -1, 0);
    int c1 = ir_new_vreg(F);
    ir_emit(F, IR_AND, c1, cur, notm, 0);
    int v1 = ir_new_vreg(F);
    ir_emit(F, IR_AND, v1, v, mask, 0);
    int r = ir_new_vreg(F);
    ir_emit(F, IR_OR, r, c1, v1, 0);
    ir_emit(F, IR_STORE, -1, addr, r, 0);
}

/* 聚合初始化列表（嵌套递归）：{e0, e1, ...} 或 {{...}, {...}}
 * ti=当前类型，base=当前类型在聚合变量中的槽起点；成员 k 的槽起点 =
 * base + moff[k]（union 共享 base）。聚合成员遇 { 递归填充。 */
static void ir_agg_init(CompilerState *cs, int vi, int ti, int base)
{
    IrAggType *a = &agg_types[ti];
    int k = 0;
    next_tok(cs);           /* { */
    if (cur_tok(cs) != TOK_RBRACE) {
        for (;;) {
            if (k >= a->mcount) break;
            int mt = a->mtype[k];
            int off = base + ((a->kind == 1) ? 0 : a->moff[k]);
            if (mt >= 0 && cur_tok(cs) == TOK_LBRACE) {
                ir_agg_init(cs, vi, mt, off);
            } else {
                int v = ir_expr(cs);
                int addr = ir_new_vreg(F);
                ir_emit(F, IR_ADDR, addr, vt[vi], -1, off);
                ir_emit(F, IR_STORE, -1, addr, v, 0);
            }
            k++;
            if (cur_tok(cs) != TOK_COMMA) break;
            next_tok(cs);
        }
    }
    expect(cs, TOK_RBRACE);
}

/* 聚合类型声明：name Type [= {...}|= expr]
 * struct: elems=成员数（顺序槽）；union: 共享槽 0；enum: 单槽 */
static void ir_agg_decl(CompilerState *cs, const char *name, int ti, int vis)
{
    IrAggType *a = &agg_types[ti];
    int elems = (a->kind == 2) ? 1 : a->mslots;
    int vi = var_declare(name, elems, ti, vis);
    if (cur_tok(cs) == TOK_ASSIGN) {
        next_tok(cs);
        if (cur_tok(cs) == TOK_LBRACE) {
            ir_agg_init(cs, vi, ti, 0);
        } else {
            int v = ir_expr(cs);
            /* sret 调用（struct 返回函数）：缓冲地址是"值"（last_mr_buf），
             * 缓冲值 + k*8 → LOAD → 聚合槽 k（与 sret 声明同逻辑） */
            if (elems > 1 && last_mr_buf >= 0) {
                int base = ir_new_vreg(F);
                ir_emit(F, IR_MOV, base, last_mr_buf, -1, 0);
                for (int k = 0; k < elems; k++) {
                    int off = ir_new_vreg(F);
                    ir_emit(F, IR_CONST, off, -1, -1, (long long)k * 8);
                    int addr = ir_new_vreg(F);
                    ir_emit(F, IR_ADD, addr, base, off, 0);
                    int lv = ir_new_vreg(F);
                    ir_emit(F, IR_LOAD, lv, addr, -1, 0);
                    ir_emit(F, IR_MOV, vt[vi] + k, lv, -1, 0);
                }
                last_mr_buf = -1;
            } else {
                ir_emit(F, IR_MOV, vt[vi], v, -1, 0);
            }
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
static int var_declare(const char *name, int elems, int type_idx, int vis)
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
    }
    vn[vn_count] = name;
    pt[vn_count] = -1;
    ve[vn_count] = elems > 0 ? elems : 0;
    vetyp[vn_count] = 0;
    vty[vn_count] = type_idx;
    vvis[vn_count] = vis;
    vtype[vn_count] = 0;
    int base = -1;
    if (elems > 0 && type_idx < 0) {
        /* 基本类型数组：分配 elems 个连续 vreg（native 槽字节连续），
         * 但只发基槽 ALLOCA（imm=8*elems → ir-c 生成 C 数组 t{base}[elems]，
         * 元素槽 vreg 不单独声明，ADDR(imm=k) 生成 &t{base}[k]） */
        for (int k = 0; k < elems; k++) {
            int vr = ir_new_vreg(F);
            if (k == 0) {
                base = vr;
                ir_emit(F, IR_ALLOCA, vr, -1, -1, 8LL * elems);
            }
        }
    } else {
        for (int k = 0; k < (elems > 0 ? elems : 1); k++) {
            int vr = ir_new_vreg(F);
            ir_emit(F, IR_ALLOCA, vr, -1, -1, 8);
            if (k == 0) base = vr;
        }
    }
    vt[vn_count] = base;
    return vn_count++;
}

/* 数组元素寻址宽度：槽模型数组一律 8 字节；仅动态字符串（vetyp 码 8，
 * char[] 池地址）按 1 字节。vetyp 其他码只影响值截断（ir_coerce），不参与寻址 */
static int ir_elem_width(int code)
{
    return code == 8 ? 1 : 8;
}

/* 数组元素地址：&arr[0] + idx*width（width 编码进 ELEM_ADDR imm；imm=0 兼容=8）
 * is_ptr：动态字符串（vetyp=8 池地址）——槽里存的是指针值，取槽值（MOV）后
 * **向前偏移**（池内存向上布局；ELEM_ADDR 是后端栈布局方向，不适用） */
static int ir_elem_addr(CompilerState *cs, int base_vreg, int idx_vreg, int width, int is_ptr)
{
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

/* ---- 表达式：返回持有结果的 vreg ---- */

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

static int ir_primary(CompilerState *cs)
{
    TokenType t = cur_tok(cs);
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
        /* sizeof(type/expr) / typeof（映射为 sizeof）/ alignof（IR 槽模型返回 8） */
        next_tok(cs);
        expect(cs, TOK_LPAREN);
        long long sz;
        if (is_type_token(cur_tok(cs)) || cur_tok(cs) == TOK_IDENTIFIER) {
            sz = type_size_of(cs, NULL);
        } else {
            ir_expr(cs);            /* sizeof(expr)：表达式值为 8 字节 */
            sz = 8;
        }
        expect(cs, TOK_RPAREN);
        if (t == TOK_ALIGNOF) sz = 8;   /* IR 8 字节槽对齐 */
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
                    int vi2 = var_find(cs->parser.lex->tok_str);
                    if (vi2 >= 0) vlen = ve[vi2];
                    next_tok(cs);
                }
                expect(cs, TOK_RPAREN);
                int vr = ir_new_vreg(F);
                ir_emit(F, IR_CONST, vr, -1, -1, vlen);
                return vr;
            }
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
                long long sz = type_size_of(cs, NULL);
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
                            int mc = agg_types[vty[avi]].mcount;
                            for (int k = 0; k < mc && argn < 64; k++) {
                                arglist[argn++] = vt[avi] + k;
                            }
                        } else {
                            /* mr 调用（struct 返回）等：ir_expr 解析 → 从缓冲 LOAD 成员 */
                            int a = ir_expr(cs);
                            (void)a;
                            if (last_mr_buf >= 0) {
                                int mc = agg_types[pti].mcount;
                                int base = ir_new_vreg(F);
                                ir_emit(F, IR_MOV, base, last_mr_buf, -1, 0);
                                for (int k = 0; k < mc && argn < 64; k++) {
                                    int off = ir_new_vreg(F);
                                    ir_emit(F, IR_CONST, off, -1, -1, (long long)k * 8);
                                    int addr = ir_new_vreg(F);
                                    ir_emit(F, IR_ADD, addr, base, off, 0);
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
                        argi += agg_types[pti].mcount;
                    } else {
                        /* 标量实参：M2 参数前缀检查（flow→失效, var/const→冻结） */
                        int src_vi = -1;
                        if (cur_tok(cs) == TOK_IDENTIFIER) {
                            int sv = var_find(cs->parser.lex->tok_str);
                            if (sv >= 0) src_vi = sv;
                        }
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
                long long sz = (ti >= 0) ? (long long)agg_types[ti].mcount * 8 : 8;
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
            /* 非 mr 普通调用：按序发射 PARAM + CALL */
            for (int k = 0; k < argn; k++) {
                ir_emit(F, IR_PARAM, -1, arglist[k], -1, 0);
            }
            int vr = ir_new_vreg(F);
            IrIns *in = &F->ins[ir_emit(F, IR_CALL, vr, -1, -1, argn)];
            in->sym = name;         /* 外部符号（puts 等） */
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
        if (cur_tok(cs) == TOK_LBRACKET) {
            /* 数组下标 arr[idx] -> LOAD(&arr[0] + idx*8)；切片 arr[lo..hi] -> 地址 */
            next_tok(cs);
            if (cur_tok(cs) == TOK_RANGE) {
                /* arr[..hi]：省略起始的切片 → &arr[0] */
                next_tok(cs);
                ir_expr(cs);            /* hi 忽略 */
                expect(cs, TOK_RBRACKET);
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
                return ir_elem_addr(cs, vt[vi], idx, ir_elem_width(vetyp[vi]), vetyp[vi] == 8);
            }
            expect(cs, TOK_RBRACKET);
            int addr = ir_elem_addr(cs, vt[vi], idx, ir_elem_width(vetyp[vi]), vetyp[vi] == 8);
            int vr = ir_new_vreg(F);
            if (vetyp[vi] == 8) {
                /* 动态字符串元素（char，1 字节）：IR_LOAD8 字节读（0-255） */
                ir_emit(F, IR_LOAD8, vr, addr, -1, 0);
            } else {
                ir_emit(F, IR_LOAD, vr, addr, -1, 0);
            }
            if (vetyp[vi] == 1) ir_set_double(vr);   /* PB-1：浮点元素标记（运算/比较用） */
            return vr;
        }
        if (cur_tok(cs) == TOK_DOT_PAREN) {
            /* 指针解引用 p.() → LOAD(*p)（p 变量存目标地址值；全量同语法。
             * 注意 DOT_PAREN 已含 '('——token 流 p, DOT_PAREN, ')'）。
             * 指向浮点（f64/f32）时标记结果 vreg 为 double，避免后续赋值/运算
             * 误把位模式当整数转换（ITOD）——pt[] 记录指针指向的标量类型。 */
            next_tok(cs);
            expect(cs, TOK_RPAREN);
            ir_vis_check_usable(cs, vi);   /* M2：读前检查所有权是否已转移（use-after-move） */
            int addr = ir_new_vreg(F);
            ir_emit(F, IR_MOV, addr, vt[vi], -1, 0);    /* p 槽值 = 目标地址（MOV 勿 LOAD） */
            int vr = ir_new_vreg(F);
            ir_emit(F, IR_LOAD, vr, addr, -1, 0);       /* 读目标内容 */
            if (PT_IS_SCALAR(pt[vi])) {
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
                nihao_error(cs, "ir: '%s' is not a pointer to aggregate (assign &var first)", name);
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
            int addr;
            if (off == 0) {
                addr = base;
            } else {
                /* off 为槽偏移，IR_ADD 是字节加法 → 乘 8（与 IR_ADDR 槽语义区分） */
                int ov = ir_new_vreg(F);
                ir_emit(F, IR_CONST, ov, -1, -1, off * 8);
                addr = ir_new_vreg(F);
                ir_emit(F, IR_ADD, addr, base, ov, 0);
            }
            int vr = ir_new_vreg(F);
            ir_emit(F, IR_LOAD, vr, addr, -1, 0);
            /* 位域成员：AND 掩码取低 N 位 */
            if (agg_types[ti].mbits[last_fi] > 0) {
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
            int addr = ir_new_vreg(F);
            ir_emit(F, IR_ADDR, addr, vt[vi], -1, off);
            int vr = ir_new_vreg(F);
            ir_emit(F, IR_LOAD, vr, addr, -1, 0);
            /* 位域成员：AND 掩码取低 N 位 */
            if (agg_types[ti].mbits[last_fi] > 0) {
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
                        next_tok(cs);
                        /* 截取表达式源文本（mark buf_ptr → 行尾） */
                        LexerState *lx = cs->parser.lex;
                        char *mark = lx->buf_ptr;
                        while (cur_tok(cs) != TOK_NEWLINE && cur_tok(cs) != TOK_RBRACE &&
                               cur_tok(cs) != TOK_EOF) next_tok(cs);
                        char *end = lx->buf_ptr;
                        int n = (int)(end - mark);
                        if (n > 0 && n < (int)sizeof(ct_funcs[cf].expr_src)) {
                            memcpy(ct_funcs[cf].expr_src, mark, (size_t)n);
                            ct_funcs[cf].expr_src[n] = '\0';
                            ct_funcs[cf].name = cname;
                            ct_funcs[cf].param_count = pc;
                            ct_funcs_count++;
                        } else {
                            nihao_error(cs, "ir: cooking function body too long");
                        }
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
            next_tok(cs);       /* 其他编译期 item（cooking-call 等）跳过 */
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
        minits[mc] = ir_expr(cs);
        mc++;
        if (cur_tok(cs) != TOK_COMMA) break;
        next_tok(cs);
        skip_newlines(cs);
    }
    expect(cs, TOK_RBRACE);
    /* 类型：基本类型或聚合类型，可带 [N] 数组 */
    int elems = 0, ti = -1;
    if (is_type_token(cur_tok(cs))) {
        next_tok(cs);
    } else if (cur_tok(cs) == TOK_IDENTIFIER) {
        ti = agg_type_find(cs->parser.lex->tok_str);
        if (ti >= 0) {
            next_tok(cs);
            elems = (agg_types[ti].kind == 2) ? 1 : agg_types[ti].mcount;
        } else {
            nihao_error(cs, "ir: unknown type '%s' in multi-declaration",
                        cs->parser.lex->tok_str);
            next_tok(cs);
        }
    } else {
        nihao_error(cs, "ir: expected type after multi-variable declaration");
    }
    if (cur_tok(cs) == TOK_LBRACKET) {
        next_tok(cs);
        if (cur_tok(cs) == TOK_INT_CONST) {
            elems = (int)cs->parser.lex->tok_val.i;
            next_tok(cs);
        }
        expect(cs, TOK_RBRACKET);
    }
    for (int k = 0; k < mc; k++) {
        int vi = var_declare(mnames[k], elems, ti, vis);
        ir_emit(F, IR_MOV, vt[vi], minits[k], -1, 0);
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

static void ir_stmt(CompilerState *cs)
{
    TokenType t = cur_tok(cs);
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
        is_val_vreg = c;                /* 体内 is 匹配条件值 */
        ir_block(cs);
        is_val_vreg = save_is;
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
        typedef struct { int vi, kind, val, has_val; } ForStep;
        ForStep fs = { -1, 0, -1, 0 };
        next_tok(cs);
        if (cur_tok(cs) == TOK_IDENTIFIER) {
            const char *vname = cs->parser.lex->tok_str;
            next_tok(cs);
            expect(cs, TOK_ASSIGN);
            int vi = var_find(vname);
            if (vi < 0) vi = var_declare(vname, 0, -1, VIS_VAR);
            int v = ir_expr(cs);
            ir_emit(F, IR_MOV, vt[vi], v, -1, 0);
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
        /* 解析 step（只记录，不发射写回） */
        if (cur_tok(cs) == TOK_IDENTIFIER) {
            const char *sname = cs->parser.lex->tok_str;
            next_tok(cs);
            TokenType st = cur_tok(cs);
            fs.vi = var_find(sname);
            if (fs.vi >= 0) {
                if (st == TOK_INCREMENT) { fs.kind = 0; next_tok(cs); }
                else if (st == TOK_DECREMENT) { fs.kind = 1; next_tok(cs); }
                else if (st == TOK_ASSIGN) {
                    fs.kind = 4; next_tok(cs);
                    fs.val = ir_expr(cs); fs.has_val = 1;
                } else if (st == TOK_PLUS_ASSIGN) {
                    fs.kind = 2; next_tok(cs);
                    fs.val = ir_expr(cs); fs.has_val = 1;
                } else if (st == TOK_MINUS_ASSIGN) {
                    fs.kind = 3; next_tok(cs);
                    fs.val = ir_expr(cs); fs.has_val = 1;
                }
            }
        }
        /* NihaoC for 语法：for init; cond; step { body } —— step 后无分号 */
        ir_block(cs);
        /* body 后发射 step */
        ir_emit(F, IR_LABEL, -1, -1, -1, 0);
        F->ins[F->ins_count - 1].label = l_cont;
        if (fs.vi >= 0) {
            if (fs.kind == 0 || fs.kind == 1) {
                IrOp sop = (fs.kind == 0) ? IR_ADD : IR_SUB;
                int one = ir_new_vreg(F);
                ir_emit(F, IR_CONST, one, -1, -1, 1);
                int sv = ir_new_vreg(F);
                ir_emit(F, sop, sv, vt[fs.vi], one, 0);
                ir_emit(F, IR_MOV, vt[fs.vi], sv, -1, 0);
            } else if (fs.kind == 2 || fs.kind == 3) {
                IrOp sop = (fs.kind == 2) ? IR_ADD : IR_SUB;
                int sv = ir_new_vreg(F);
                ir_emit(F, sop, sv, vt[fs.vi], fs.val, 0);
                ir_emit(F, IR_MOV, vt[fs.vi], sv, -1, 0);
            } else if (fs.kind == 4 && fs.has_val) {
                ir_emit(F, IR_MOV, vt[fs.vi], fs.val, -1, 0);
            }
        }
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
        /* is 模式匹配：is pat { ... }，匹配 while/do 循环条件值 is_val_vreg
         * pat: <int> | -<int> | <int>..<int>（闭区间） */
        next_tok(cs);
        if (is_val_vreg < 0) {
            nihao_error(cs, "ir: 'is' pattern match only valid inside while/do loop body");
            skip_newlines(cs);
            return;
        }
        int l_done = ir_new_label(F);
        TokenType pt = cur_tok(cs);
        if (pt == TOK_IDENTIFIER && strcmp(cs->parser.lex->tok_str, "_") == 0) {
            /* 通配符：匹配任意 is_val（恒真），不 emit 条件跳转（文档 pattern 列表含 _） */
            next_tok(cs);
        } else if (pt == TOK_MINUS) {
            next_tok(cs);
            if (cur_tok(cs) == TOK_INT_CONST) {
                long long v = -(long long)cs->parser.lex->tok_val.i;
                next_tok(cs);
                int k = ir_new_vreg(F);
                ir_emit(F, IR_CONST, k, -1, -1, v);
                int eq = ir_new_vreg(F);
                ir_emit(F, IR_CMP_EQ, eq, is_val_vreg, k, 0);
                ir_emit(F, IR_JZ, -1, eq, -1, 0);
                F->ins[F->ins_count - 1].label = l_done;
            }
        } else if (pt == TOK_INT_CONST) {
            long long lo = (long long)cs->parser.lex->tok_val.i;
            next_tok(cs);
            if (cur_tok(cs) == TOK_RANGE) {
                next_tok(cs);
                if (cur_tok(cs) == TOK_INT_CONST) {
                    long long hi = (long long)cs->parser.lex->tok_val.i;
                    next_tok(cs);
                    int klo = ir_new_vreg(F);
                    ir_emit(F, IR_CONST, klo, -1, -1, lo);
                    int ge = ir_new_vreg(F);
                    ir_emit(F, IR_CMP_GE, ge, is_val_vreg, klo, 0);
                    ir_emit(F, IR_JZ, -1, ge, -1, 0);
                    F->ins[F->ins_count - 1].label = l_done;
                    int khi = ir_new_vreg(F);
                    ir_emit(F, IR_CONST, khi, -1, -1, hi);
                    int le = ir_new_vreg(F);
                    ir_emit(F, IR_CMP_LE, le, is_val_vreg, khi, 0);
                    ir_emit(F, IR_JZ, -1, le, -1, 0);
                    F->ins[F->ins_count - 1].label = l_done;
                }
            } else {
                int k = ir_new_vreg(F);
                ir_emit(F, IR_CONST, k, -1, -1, lo);
                int eq = ir_new_vreg(F);
                ir_emit(F, IR_CMP_EQ, eq, is_val_vreg, k, 0);
                ir_emit(F, IR_JZ, -1, eq, -1, 0);
                F->ins[F->ins_count - 1].label = l_done;
            }
        } else if (pt == TOK__FLOW || pt == TOK__STATIC || pt == TOK__CONST ||
                   pt == TOK__VAR || pt == TOK__UNDEF) {
            /* 可见性模式：is _flow / is _static 等（BNF <visibility-enum>）——
             * 匹配循环条件值 == 可见性常量（VIS_CONST=1 FLOW=2 STATIC=3 VAR=0 UNDEF=4） */
            int visv = (pt == TOK__FLOW) ? VIS_FLOW :
                       (pt == TOK__STATIC) ? VIS_STATIC :
                       (pt == TOK__CONST) ? VIS_CONST :
                       (pt == TOK__VAR) ? VIS_VAR : VIS_UNDEF;
            next_tok(cs);
            int k = ir_new_vreg(F);
            ir_emit(F, IR_CONST, k, -1, -1, visv);
            int eq = ir_new_vreg(F);
            ir_emit(F, IR_CMP_EQ, eq, is_val_vreg, k, 0);
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
                int eq = ir_new_vreg(F);
                ir_emit(F, IR_CMP_EQ, eq, is_val_vreg, pv, 0);
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
                int k = ir_new_vreg(F);
                ir_emit(F, IR_CONST, k, -1, -1, cval);
                int eq = ir_new_vreg(F);
                ir_emit(F, IR_CMP_EQ, eq, is_val_vreg, k, 0);
                ir_emit(F, IR_JZ, -1, eq, -1, 0);
                F->ins[F->ins_count - 1].label = l_done;
            }
        } else if (pt == TOK__UNDEF || pt == TOK__CONST || pt == TOK__FLOW ||
                   pt == TOK__STATIC || pt == TOK__VAR) {
            /* 可见性模式：is _static { }（比较 is_val == NH_STATIC） */
            long long vv = (pt == TOK__UNDEF) ? VIS_UNDEF :
                           (pt == TOK__CONST) ? VIS_CONST :
                           (pt == TOK__FLOW)  ? VIS_FLOW :
                           (pt == TOK__STATIC)? VIS_STATIC : VIS_VAR;
            next_tok(cs);
            int k = ir_new_vreg(F);
            ir_emit(F, IR_CONST, k, -1, -1, vv);
            int eq = ir_new_vreg(F);
            ir_emit(F, IR_CMP_EQ, eq, is_val_vreg, k, 0);
            ir_emit(F, IR_JZ, -1, eq, -1, 0);
            F->ins[F->ins_count - 1].label = l_done;
        } else {
            nihao_error(cs, "ir: bad 'is' pattern");
            next_tok(cs);
            skip_newlines(cs);
            return;
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
        if (cur_tok(cs) == TOK_NEWLINE || cur_tok(cs) == TOK_RBRACE) {
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
                        int off = ir_new_vreg(F);
                        ir_emit(F, IR_CONST, off, -1, -1, (long long)k * 8);
                        int addr = ir_new_vreg(F);
                        ir_emit(F, IR_ADD, addr, vt[retvi], off, 0);
                        ir_emit(F, IR_STORE, -1, addr, v, 0);
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
                    int mcount = agg_types[vty[rvi]].mcount;
                    for (int k = 0; k < mcount; k++) {
                        int off = ir_new_vreg(F);
                        ir_emit(F, IR_CONST, off, -1, -1, (long long)k * 8);
                        int addr = ir_new_vreg(F);
                        ir_emit(F, IR_ADD, addr, vt[retvi], off, 0);
                        ir_emit(F, IR_STORE, -1, addr, vt[rvi] + k, 0);
                    }
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
            if (vi < 0) vi = var_declare(name, 0, -1, VIS_VAR);
            if (vty[vi] >= 0 && agg_types[vty[vi]].kind != 2 &&
                cur_tok(cs) == TOK_IDENTIFIER) {
                /* 聚合整体赋值：逐成员拷贝（struct 按展开槽数（嵌套递归），
                 * union 共享槽拷 1） */
                int src = var_find(cs->parser.lex->tok_str);
                if (src >= 0 && vty[src] >= 0 &&
                    agg_types[vty[src]].kind == agg_types[vty[vi]].kind) {
                    int n = agg_types[vty[vi]].kind == 1 ? 1 :
                            agg_types[vty[vi]].mslots;
                    next_tok(cs);   /* src name */
                    for (int k = 0; k < n; k++)
                        ir_emit(F, IR_MOV, vt[vi] + k, vt[src] + k, -1, 0);
                    skip_newlines(cs);
                    return;
                }
            }
            if (cur_tok(cs) == TOK_IDENTIFIER) {
                /* 切片 rhs：s = arr[lo..hi]——记录逻辑长度（常量边界）+ 指针 */
                LexerState *lx2 = cs->parser.lex;
                lx2->peek_valid = 0;
                lexer_peek(lx2);
                if (lx2->peek_tok == TOK_LBRACKET) {
                    const char *arrn = lx2->tok_str;
                    int avi = var_find(arrn);
                    if (avi >= 0) {
                        next_tok(cs);   /* arr */
                        next_tok(cs);   /* [ */
                        int lo = ir_expr(cs);
                        if (cur_tok(cs) == TOK_RANGE) {
                            next_tok(cs);
                            int hi = ir_expr(cs);
                            expect(cs, TOK_RBRACKET);
                            long long lov, hiv;
                            if (vreg_const(F, lo, &lov) && vreg_const(F, hi, &hiv))
                                ve[vi] = (int)(hiv - lov);
                            int addr = ir_elem_addr(cs, vt[avi], lo,
                                                    ir_elem_width(vetyp[avi]),
                                                    vetyp[avi] == 8);
                            ir_emit(F, IR_MOV, vt[vi], addr, -1, 0);
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
                            skip_newlines(cs);
                            return;
                        }
                    }
                }
                lx2->peek_valid = 0;
                /* 非标量/聚合目标：& 未消费，回退通用 ir_expr 解析 */
            }
            int vr = ir_expr(cs);
            /* PB-1 补缺（2026-09-01）：窄整数目标此前只在「声明初始化」路径经
             * ir_coerce 截断，普通赋值直接落槽 —— `b u8 = 0; b = 300` 在 IR 后端
             * 得到 300 而 A 方案（C 语义）得到 44。此处统一补目标类型协调。 */
            vr = ir_coerce(vr, vtype[vi]);
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
            next_tok(cs);           /* .( */
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
                    ir_emit(F, IR_LOAD, cur, addr, -1, 0);
                    v = ir_expr(cs);
                    int sc = (PT_IS_SCALAR(pt[vi])) ? PT_SCALAR_CODE(pt[vi]) : vtype[vi];
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
                    v = ir_coerce(v, (PT_IS_SCALAR(pt[vi])) ? PT_SCALAR_CODE(pt[vi]) : vtype[vi]);
                }
                ir_emit(F, IR_STORE, -1, addr, v, 0);
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
                nihao_error(cs, "ir: '%s' is not a pointer to aggregate (assign &var first)", name);
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
                int b = ir_expr(cs);
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
                        if (off > 0) {
                            int ov = ir_new_vreg(F);
                            ir_emit(F, IR_CONST, ov, -1, -1, off * 8);
                            int a2 = ir_new_vreg(F);
                            ir_emit(F, IR_ADD, a2, caddr, ov, 0);
                            caddr = a2;
                        }
                        cur = ir_bf_load(caddr, bfbits);
                    } else {
                        /* 普通成员当前值：LOAD(p 值 + off*8) */
                        int base = ir_new_vreg(F);
                        ir_emit(F, IR_MOV, base, vt[vi], -1, 0);
                        int caddr;
                        if (off == 0) {
                            caddr = base;
                        } else {
                            int ov = ir_new_vreg(F);
                            ir_emit(F, IR_CONST, ov, -1, -1, off * 8);
                            caddr = ir_new_vreg(F);
                            ir_emit(F, IR_ADD, caddr, base, ov, 0);
                        }
                        cur = ir_new_vreg(F);
                        ir_emit(F, IR_LOAD, cur, caddr, -1, 0);
                    }
                    nv = ir_new_vreg(F);
                    ir_emit(F, aop, nv, cur, b, 0);
                }
                /* STORE 到 p 值 + off*8 */
                int base = ir_new_vreg(F);
                ir_emit(F, IR_MOV, base, vt[vi], -1, 0);
                int addr;
                if (off == 0) {
                    addr = base;
                } else {
                    int ov = ir_new_vreg(F);
                    ir_emit(F, IR_CONST, ov, -1, -1, off * 8);
                    addr = ir_new_vreg(F);
                    ir_emit(F, IR_ADD, addr, base, ov, 0);
                }
                ir_bf_store(addr, nv, bfbits);
                skip_newlines(cs);
            } else {
                nihao_error(cs, "ir: expected '=' after '->' member access");
                skip_newlines(cs);
            }
        } else if (nt == TOK_DOT) {
            /* 成员赋值 s.field = e / s.field op= e */
            next_tok(cs);           /* name */
            int vi = var_find(name);
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
                int b = ir_expr(cs);
                int nv;
                int bfbits = agg_types[ti].mbits[fi];
                if (!is_compound) {
                    nv = b;
                } else {
                    int cur;
                    if (bfbits > 0) {
                        /* 位域复合赋值：基于位域值运算（LOAD+AND 取低 N 位） */
                        int caddr = ir_new_vreg(F);
                        ir_emit(F, IR_ADDR, caddr, vt[vi], -1, off);
                        cur = ir_bf_load(caddr, bfbits);
                    } else {
                        cur = ir_new_vreg(F);
                        ir_emit(F, IR_MOV, cur, vt[vi] + off, -1, 0);
                    }
                    nv = ir_new_vreg(F);
                    ir_emit(F, aop, nv, cur, b, 0);
                }
                int addr = ir_new_vreg(F);
                ir_emit(F, IR_ADDR, addr, vt[vi], -1, off);
                ir_bf_store(addr, nv, bfbits);
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
        } else if (is_type_token(nt)) {
            /* 声明: name i32 [N] = expr | = {e0, e1, ...} | name 函数指针类型 = fn */
            int decl_float = (nt == TOK_F64 || nt == TOK_F32);
            /* PB-1 窄整数类型编码（0=i64 1=float 2=i8 3=i16 4=i32 5=u8 6=u16 7=u32）；
             * f32 严格宽度（2026-08-19）：F32 → 码 8（标记 f32，赋值时 FTRUNC） */
            int vt_code = decl_float ? (nt == TOK_F32 ? 8 : 1) :
                          (nt == TOK_CHAR || nt == TOK_I8) ? 2 :
                          (nt == TOK_I16) ? 3 : (nt == TOK_I32) ? 4 :
                          (nt == TOK_U8)  ? 5 : (nt == TOK_U16) ? 6 :
                          (nt == TOK_U32) ? 7 : 0;
            next_tok(cs);           /* name */
            next_tok(cs);           /* 类型 */
            int elems = 0;
            int is_arr = 0;         /* 出现过 [..]（数组/字符串；标量 char 非数组） */
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
            } else if (cur_tok(cs) == TOK_LBRACKET) {
                next_tok(cs);
                is_arr = 1;         /* char[] 动态字符串与数组共用此标记 */
                if (cur_tok(cs) == TOK_RANGE || cur_tok(cs) == TOK_ELLIPSIS) {
                    /* 纯动态数组 [...] / [..]：固定默认容量 8 槽（增长语义留 TODO） */
                    elems = 8;
                    next_tok(cs);
                    if (cur_tok(cs) == TOK_INT_CONST) {
                        /* [...N] / [..N]：历史兼容写法（A 方案早期实现），
                         * 等同 BNF 规范的 [N...] —— 2026-09-01 与 parser.c 对齐 */
                        elems = (int)cs->parser.lex->tok_val.i;
                        next_tok(cs);
                    }
                } else if (cur_tok(cs) == TOK_INT_CONST) {
                    elems = (int)cs->parser.lex->tok_val.i;
                    next_tok(cs);
                    if (cur_tok(cs) == TOK_RANGE || cur_tok(cs) == TOK_ELLIPSIS) {
                        /* N... 动态数组：初始容量 N 个槽（增长忽略） */
                        next_tok(cs);
                    }
                }
                expect(cs, TOK_RBRACKET);
            }
            expect(cs, TOK_ASSIGN); /* 消费 =（expect 已推进） */
            int src_vi = -1;
            if (cur_tok(cs) == TOK_IDENTIFIER) {
                int sv = var_find(cs->parser.lex->tok_str);
                if (sv >= 0) src_vi = sv;   /* RHS 是裸变量引用 → 走所有权检查 */
            }
            int vi = var_declare(name, elems, -1, decl_vis);
            if (nt == TOK_VOID) vptr[vi] = 1;   /* 指针类标记（仅 void 触发所有权语义） */
            if (src_vi >= 0) ir_vis_check_assign(cs, vi, src_vi);  /* M2 转移检查 */
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
                /* 数组初始化列表 {e0, e1, ...}：逐个 STORE 到元素槽 */
                next_tok(cs);
                int k = 0;
                if (cur_tok(cs) != TOK_RBRACE) {
                    for (;;) {
                        int v = ir_expr(cs);
                        v = ir_coerce(v, vetyp[vi] == 8 ? 2 : vetyp[vi]);   /* PB-1：元素类型协调（vetyp 8=char 元素，与 vtype 8=f32 编码冲突→按 i8 截断） */
                        int addr = ir_new_vreg(F);
                        ir_emit(F, IR_ADDR, addr, vt[vi], -1, k);
                        ir_emit(F, IR_STORE, -1, addr, v, 0);
                        k++;
                        if (cur_tok(cs) != TOK_COMMA) break;
                        next_tok(cs);
                    }
                }
                expect(cs, TOK_RBRACE);
            } else if (cur_tok(cs) == TOK_STRING_LITERAL && elems > 0) {
                /* 字符串数组初始化 s char[N] = "hello"：逐字节 STORE（含 NUL，
                 * 截断到容量 elems）；char 元素 vetyp=2（i8），字节值天然规范 */
                const char *str = cs->parser.lex->tok_str;
                size_t slen = strlen(str);
                next_tok(cs);
                int cap = (int)slen + 1;
                if (cap > elems) cap = elems;
                for (int k = 0; k < cap; k++) {
                    int cv = ir_new_vreg(F);
                    ir_emit(F, IR_CONST, cv, -1, -1,
                            (unsigned char)(k < (int)slen ? str[k] : 0));
                    int addr = ir_new_vreg(F);
                    ir_emit(F, IR_ADDR, addr, vt[vi], -1, k);
                    ir_emit(F, IR_STORE, -1, addr, cv, 0);
                }
            } else if (vetyp[vi] == 8 && cur_tok(cs) == TOK_STRING_LITERAL) {
                /* 动态字符串 char[] = "lit"：记录逻辑长度（len() 用），存池地址 */
                ve[vi] = (int)strlen(cs->parser.lex->tok_str);
                int vr = ir_expr(cs);
                ir_emit(F, IR_MOV, vt[vi], vr, -1, 0);
            } else {
                int vr = ir_expr(cs);
                vr = ir_coerce(vr, vtype[vi]);   /* PB-1：目标类型协调（DTOI/ITOD/TRUNC） */
                ir_emit(F, IR_MOV, vt[vi], vr, -1, 0);
            }
            skip_newlines(cs);
        } else if (nt == TOK_LBRACKET) {
            /* 数组元素赋值 arr[idx] = expr | 切片赋值 arr[lo..hi] = {v0, v1, ...} */
            next_tok(cs);           /* name */
            next_tok(cs);           /* [ */
            int vi = var_find(name);
            if (vi < 0) {
                nihao_error(cs, "ir: undeclared variable '%s'", name);
                skip_newlines(cs);
                return;
            }
            int idx = ir_expr(cs);   /* lo 或单下标 */
            if (cur_tok(cs) == TOK_RANGE) {
                /* 切片赋值：arr[lo..hi] = {v0, v1, ...}（逐个 STORE；hi 边界留校验） */
                next_tok(cs);
                int hi = ir_expr(cs);
                (void)hi;
                expect(cs, TOK_RBRACKET);
                expect(cs, TOK_ASSIGN);
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
                            int addr = ir_elem_addr(cs, vt[vi], ik, ir_elem_width(vetyp[vi]), vetyp[vi] == 8);
                            if (vetyp[vi] == 8) ir_emit(F, IR_STORE8, -1, addr, v, 0);
                            else ir_emit(F, IR_STORE, -1, addr, v, 0);
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
            int addr = ir_elem_addr(cs, vt[vi], idx, ir_elem_width(vetyp[vi]), vetyp[vi] == 8);
            if (vetyp[vi] == 8) ir_emit(F, IR_STORE8, -1, addr, v, 0);
            else ir_emit(F, IR_STORE, -1, addr, v, 0);
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
static void ir_func(CompilerState *cs)
{
    /* func name ( params ) ret */
    const char *fname = cs->parser.lex->tok_str;
    int is_main = strcmp(fname, "main") == 0;
    next_tok(cs);
    F = ir_fn_new(P, fname, is_main);
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
    }
    /* 注册变量：mr 时 _mr_ret 必须是第 0 槽（调用约定：缓冲指针为第一参数）。
     * PB-16 struct 参数按值展开：聚合参数占 mcount 个虚拟参数槽（成员槽 vreg
     * 连续，prologue 按展开索引入槽） */
    int vpi = 0;
    if (F->is_mr) {
        var_declare("_mr_ret", 0, -1, VIS_VAR);
        F->param_agg_ti[0] = -1;
        vpi = 1;
    }
    for (int pi = 0; pi < nparam; pi++) {
        F->param_vis[pi] = pvis[pi];     /* M2 调用点检查用（声明序，与 param 对齐） */
        int ti = ptype_ti[pi];
        if (ti >= 0) {
            int mcount = agg_types[ti].mcount;
            int vi = var_declare(pnames[pi], mcount, ti, pvis[pi]);
            for (int k = 0; k < mcount; k++) {
                F->param_types[vpi + k] = 0;   /* 成员统一 8 字节槽 */
            }
            F->param_agg_ti[vpi] = ti;         /* 记录聚合起始（其余展开槽 -1） */
            for (int k = 1; k < mcount; k++) F->param_agg_ti[vpi + k] = -1;
            (void)vi;
            vpi += mcount;
        } else {
            int vi = var_declare(pnames[pi], 0, -1, pvis[pi]);
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
    F->param_count = vpi;
    ir_block(cs);
    ir_fn_end(F);
}

/* 顶层类型定义：Name struct { fields } / Name union { fields } / Name enum { A, B } */
static void ir_type_decl(CompilerState *cs)
{
    const char *tname = cs->parser.lex->tok_str;
    next_tok(cs);
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
            /* 成员类型：自定义聚合类型（IDENTIFIER）记录索引，否则跳过基本类型；
             * 注意：struct 体内换行不产生 NEWLINE（brace_depth>0），
             * 遇非类型 token（下一成员名/冒号/赋值）即成员结束。 */
            a->mtype[a->mcount] = -1;
            if (cur_tok(cs) == TOK_IDENTIFIER) {
                int ti2 = agg_type_find(cs->parser.lex->tok_str);
                if (ti2 >= 0) {
                    a->mtype[a->mcount] = ti2;
                    next_tok(cs);
                }
            } else {
                while (cur_tok(cs) != TOK_RBRACE && cur_tok(cs) != TOK_EOF &&
                       cur_tok(cs) != TOK_COLON && cur_tok(cs) != TOK_ASSIGN) {
                    if (is_type_token(cur_tok(cs)) ||
                        cur_tok(cs) == TOK_LBRACKET || cur_tok(cs) == TOK_RBRACKET) {
                        next_tok(cs);
                        continue;
                    }
                    break;
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
            if (cur_tok(cs) == TOK_ASSIGN) {       /* 默认值：仅支持简单常量 */
                next_tok(cs);
                if (cur_tok(cs) == TOK_MINUS) next_tok(cs);
                if (cur_tok(cs) == TOK_INT_CONST || cur_tok(cs) == TOK_IDENTIFIER ||
                    cur_tok(cs) == TOK_STRING_LITERAL) {
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
        while (cur_tok(cs) == TOK_IDENTIFIER || cur_tok(cs) == TOK_DOT) {
            next_tok(cs);
        }
        skip_newlines(cs);
    }

    /* 函数与类型定义列表 */
    while (cur_tok(cs) != TOK_EOF) {
        skip_newlines(cs);
        if (cur_tok(cs) == TOK_FUNC) {
            next_tok(cs);
            ir_func(cs);
        } else if (cur_tok(cs) == TOK_IDENTIFIER) {
            /* 可能是类型定义：Name struct/union/enum */
            LexerState *lx = cs->parser.lex;
            lx->peek_valid = 0;
            lexer_peek(lx);
            TokenType nxt = lx->peek_tok;
            lx->peek_valid = 0;
            if (nxt == TOK_STRUCT || nxt == TOK_UNION || nxt == TOK_ENUM) {
                ir_type_decl(cs);
            } else {
                nihao_error(cs, "ir: unexpected top-level identifier '%s'",
                            cs->parser.lex->tok_str);
                next_tok(cs);
            }
        } else if (cur_tok(cs) == TOK_COOKING) {
            /* 编译期块：执行 static_assert，其余跳过 */
            ir_cooking(cs);
        } else if (cur_tok(cs) == TOK_USE) {
            /* use 模块导入：IR 单文件模型——解析声明并跳过（模块内容需内联） */
            next_tok(cs);
            while (cur_tok(cs) == TOK_IDENTIFIER || cur_tok(cs) == TOK_DOT) {
                next_tok(cs);
            }
            if (cur_tok(cs) == TOK_NEWLINE) next_tok(cs);
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
        static const char *op_names[] = {
            "NOP", "CONST", "MOV", "ADD", "SUB", "MUL", "DIV", "MOD",
            "SHL", "SHR", "AND", "OR", "NEG", "NOT",
            "CMP_EQ", "CMP_NE", "CMP_LT", "CMP_LE", "CMP_GT", "CMP_GE",
            "FADD", "FSUB", "FMUL", "FDIV", "FCMP", "ITOD", "DTOI",
            "FTRUNC", "TRUNC", "JMP", "JZ", "JNZ", "CALL", "CALLI", "PARAM", "RET",
            "LABEL", "ALLOCA", "ADDR", "ELEM_ADDR", "LOAD", "STORE",
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

    char out[1024];
    snprintf(out, sizeof(out), "%s.%s", cs->output_file ? cs->output_file : "a.out",
             backend == 2 ? "c" : "s");
    if (backend == 2) {
        if (irgen_c_emit(P, out) != 0) return -1;
    } else if (backend == 4 || backend == 5 || backend == 6) {
        /* riscv64/arm64/loongarch64：只生成汇编（本机 tcc 是 x86-64，交叉汇编留外部工具） */
        const char *bn = (backend == 4) ? "riscv64" : (backend == 5) ? "arm64" : "loongarch64";
        if (irgen_backend_emit(P, out, bn) != 0) return -1;
        if (cs->verbose) printf("%s asm written to %s (cross, not assembled)\n", bn, out);
        return 0;
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
