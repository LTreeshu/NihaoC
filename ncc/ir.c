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
