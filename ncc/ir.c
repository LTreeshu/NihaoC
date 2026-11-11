/* ============================================================
 * ir.c - IR 程序构造与发射辅助
 * ============================================================ */
#include "ir.h"

IrProg *ir_prog_new(void)
{
    IrProg *p = nihao_malloc(g_cs, sizeof(IrProg));
    return p;
}

IrFn *ir_fn_new(IrProg *p, const char *name, int is_main)
{
    if (p->fn_count >= p->fn_cap) {
        p->fn_cap = p->fn_cap ? p->fn_cap * 2 : 8;
        p->fns = nihao_realloc(g_cs, p->fns, p->fn_cap * sizeof(IrFn));
    }
    IrFn *f = &p->fns[p->fn_count++];
    memset(f, 0, sizeof(IrFn));
    f->name = (char *)name;
    f->is_main = is_main;
    return f;
}

void ir_fn_end(IrFn *f)
{
    ir_emit(f, IR_END, -1, -1, -1, 0);
}

int ir_emit(IrFn *f, IrOp op, int dst, int a, int b, int64_t imm)
{
    if (f->ins_count >= f->ins_cap) {
        f->ins_cap = f->ins_cap ? f->ins_cap * 2 : 64;
        f->ins = nihao_realloc(g_cs, f->ins, f->ins_cap * sizeof(IrIns));
    }
    IrIns *in = &f->ins[f->ins_count++];
    memset(in, 0, sizeof(IrIns));
    in->op = op;
    in->dst = dst;
    in->a = a;
    in->b = b;
    in->imm = imm;
    return f->ins_count - 1;
}

int ir_new_vreg(IrFn *f)
{
    int vr = f->vreg_count++;
    /* vreg 类型表随 vreg_count 增长（默认 0=int；1=double） */
    f->vreg_type = nihao_realloc(g_cs, f->vreg_type, f->vreg_count * sizeof(int));
    f->vreg_type[vr] = 0;
    /* vreg 名字表同步增长（默认 NULL=无名；var_declare 后写入源码名） */
    f->vreg_name = nihao_realloc(g_cs, f->vreg_name, f->vreg_count * sizeof(char *));
    f->vreg_name[vr] = NULL;
    return vr;
}

int ir_new_label(IrFn *f)
{
    return f->label_count++;
}

int ir_add_string(IrProg *p, const char *data)
{
    /* PB-11：字符串池去重——相同字面量复用同一符号 */
    for (int i = 0; i < p->str_count; i++) {
        if (p->str_data[i] && strcmp(p->str_data[i], data) == 0)
            return i;
    }
    if (p->str_count >= p->str_cap) {
        p->str_cap = p->str_cap ? p->str_cap * 2 : 16;
        p->str_data = nihao_realloc(g_cs, p->str_data, p->str_cap * sizeof(char *));
        p->str_syms = nihao_realloc(g_cs, p->str_syms, p->str_cap * sizeof(char *));
    }
    p->str_data[p->str_count] = (char *)data;
    char sym[64];
    snprintf(sym, sizeof(sym), "__str_%d", p->str_count);
    p->str_syms[p->str_count] = nihao_malloc(g_cs, strlen(sym) + 1);
    strcpy(p->str_syms[p->str_count], sym);
    return p->str_count++;
}
/* 全局变量入池：顶层 var/const 声明的存储。符号名 __gN 与字符串池 __str_N 同风格，
 * 后端据此在 .data 段（交叉档）或文件作用域（ir-c）落一份真实存储。 */
int ir_add_global(IrProg *p, int64_t init, int is_double)
{
    if (p->glb_count >= p->glb_cap) {
        p->glb_cap = p->glb_cap ? p->glb_cap * 2 : 8;
        p->glb_syms = nihao_realloc(g_cs, p->glb_syms, p->glb_cap * sizeof(char *));
        p->glb_init = nihao_realloc(g_cs, p->glb_init, p->glb_cap * sizeof(int64_t));
        p->glb_is_double = nihao_realloc(g_cs, p->glb_is_double, p->glb_cap * sizeof(int));
    }
    char sym[64];
    snprintf(sym, sizeof(sym), "__g%d", p->glb_count);
    p->glb_syms[p->glb_count] = nihao_malloc(g_cs, strlen(sym) + 1);
    strcpy(p->glb_syms[p->glb_count], sym);
    p->glb_init[p->glb_count] = init;
    p->glb_is_double[p->glb_count] = is_double;
    return p->glb_count++;
}

/* 字符串池存的是解密后的原始字节，两个后端在序列化时必须自己再转义一次：把换行
 * 原样写进 `.string "a<换行>b"` 或 C 的 `"a<换行>b";` 会把一条语句劈成两行
 * （2026-11-11 实测：ir-c 产物是非法 C，交叉 .s 只靠汇编器容错才编得过）。
 * C 与 GAS 都接受八进制转义，故其余不可打印字节走 "\NNN"。 */
const char *ir_str_escaped(const char *s)
{
    static char bufs[2][1024];
    static int which;
    char *out = bufs[which];
    which ^= 1;
    char *o = out;
    const unsigned char *q = (const unsigned char *)s;
    for (; *q && (o - out) < 980; q++) {
        unsigned char c = *q;
        if (c == '\\' || c == '"') {
            *o++ = '\\';
            *o++ = (char)c;
        } else if (c == '\n') { *o++ = '\\'; *o++ = 'n'; }
        else if (c == '\t') { *o++ = '\\'; *o++ = 't'; }
        else if (c == '\r') { *o++ = '\\'; *o++ = 'r'; }
        else if (c < 0x20 || c == 0x7f) { o += sprintf(o, "\\%03o", c); }
        else *o++ = (char)c;
    }
    *o = 0;
    return out;
}
