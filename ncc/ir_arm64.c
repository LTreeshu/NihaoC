#include "ir_backend.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ============================================================
 * arm64 后端（PB-20：AArch64 + AAPCS64，只验汇编生成，交叉汇编器
 * clang -target aarch64 验证语法）
 *
 * 约定：
 *  - 全栈槽：vreg N 在 x29 - stride*(N+1)（槽向下生长）
 *  - 帧：stp x29,x30,[sp,#-16]!；mov x29,sp；sub sp,sp,#frame
 *  - 调用约定：x0-x7 整数参数/返回（x0 返回），d0-d7 浮点参数（d0 返回）
 *  - 临时寄存器：x9 作槽地址（CALL 装载后不再用，call 后重新计算）；x0 作值
 *  - 立即数：movz/movk 4 段（imm 是 int64 位模式，浮点常量同样适用）；
 *    槽偏移 >4095 时 movz/movk x9 + sub
 *  - 浮点比较：fcmp d0,d1 → cset（EQ=eq NE=ne LT=mi LE=ls GT=hi GE=ge）
 * ============================================================ */

/* 当前函数帧大小（prologue 设置，epilogue 恢复 sp 用） */
static int g_a64_frame = 0;

/* 生成"槽地址到 x9"（处理 >4095 偏移） */
static void a64_slot_addr(NBuf *b, int vreg, const TargetBackend *tb)
{
    int off = tb->slot_stride * (vreg + 1);
    if (off <= 4095) {
        nb_put(b, "  sub x9, x29, #%d\n", off);
    } else {
        nb_put(b, "  movz x9, #%d\n", off & 0xFFFF);
        if (off > 0xFFFF) nb_put(b, "  movk x9, #%d, lsl #16\n", (off >> 16) & 0xFFFF);
        nb_put(b, "  sub x9, x29, x9\n");
    }
}

/* 槽 → x0 */
static void a64_ld_x0(NBuf *b, int vreg, const TargetBackend *tb)
{
    a64_slot_addr(b, vreg, tb);
    nb_put(b, "  ldr x0, [x9]\n");
}

/* x0 → 槽 */
static void a64_st_x0(NBuf *b, int vreg, const TargetBackend *tb)
{
    a64_slot_addr(b, vreg, tb);
    nb_put(b, "  str x0, [x9]\n");
}

/* 槽 → 任意 GPR（x0-x7 参数装载用） */
static void a64_ld_reg(NBuf *b, const char *reg, int vreg, const TargetBackend *tb)
{
    a64_slot_addr(b, vreg, tb);
    nb_put(b, "  ldr %s, [x9]\n", reg);
}

/* 槽 → 浮点寄存器 dN */
static void a64_ld_fp(NBuf *b, const char *reg, int vreg, const TargetBackend *tb)
{
    a64_slot_addr(b, vreg, tb);
    nb_put(b, "  ldr %s, [x9]\n", reg);
}

static void a64_emit_ins(NBuf *b, const IrIns *in, const TargetBackend *tb,
                         const IrProg *p, const IrFn *f, int i)
{
    (void)p; (void)f; (void)i;
    switch (in->op) {
        case IR_CONST:
            nb_put(b, "  movz x0, #%d\n", (int)((long long)in->imm & 0xFFFF));
            nb_put(b, "  movk x0, #%d, lsl #16\n", (int)(((long long)in->imm >> 16) & 0xFFFF));
            nb_put(b, "  movk x0, #%d, lsl #32\n", (int)(((long long)in->imm >> 32) & 0xFFFF));
            nb_put(b, "  movk x0, #%d, lsl #48\n", (int)(((long long)in->imm >> 48) & 0xFFFF));
            a64_st_x0(b, in->dst, tb);
            break;
        case IR_MOV:
            a64_ld_x0(b, in->a, tb);
            a64_st_x0(b, in->dst, tb);
            break;
        case IR_ADD: case IR_SUB: case IR_MUL:
            a64_ld_x0(b, in->a, tb);
            a64_ld_reg(b, "x1", in->b, tb);
            nb_put(b, in->op == IR_ADD ? "  add x0, x0, x1\n" :
                      in->op == IR_SUB ? "  sub x0, x0, x1\n" : "  mul x0, x0, x1\n");
            a64_st_x0(b, in->dst, tb);
            break;
        case IR_DIV: case IR_MOD:
            a64_ld_x0(b, in->a, tb);
            a64_ld_reg(b, "x1", in->b, tb);
            if (in->op == IR_DIV) {
                nb_put(b, "  sdiv x0, x0, x1\n");
            } else {
                /* 余数：sdiv x2, x0, x1; msub x0, x2, x1, x0 */
                nb_put(b, "  sdiv x2, x0, x1\n  msub x0, x2, x1, x0\n");
            }
            a64_st_x0(b, in->dst, tb);
            break;
        case IR_ITOD:
            a64_ld_x0(b, in->a, tb);
            nb_put(b, "  scvtf d0, x0\n");
            a64_slot_addr(b, in->dst, tb);
            nb_put(b, "  str d0, [x9]\n");
            break;
        case IR_DTOI:
            a64_slot_addr(b, in->a, tb);
            nb_put(b, "  ldr d0, [x9]\n");
            nb_put(b, "  fcvtzs x0, d0\n");   /* 向零截断（与 C 一致） */
            a64_st_x0(b, in->dst, tb);
            break;
        case IR_TRUNC:
            /* 窄整数截断 + 符号/零扩展（AArch64 原生 sxtb/sxth/sxtw/uxtb/uxth/uxtw） */
            a64_ld_x0(b, in->a, tb);
            nb_put(b, "  %s x0, w0\n",
                   in->imm == 0 ? "sxtb" : in->imm == 1 ? "sxth" :
                   in->imm == 2 ? "sxtw" : in->imm == 3 ? "uxtb" :
                   in->imm == 4 ? "uxth" : "uxtw");
            a64_st_x0(b, in->dst, tb);
            break;
        case IR_FADD: case IR_FSUB: case IR_FMUL: case IR_FDIV:
            a64_ld_fp(b, "d0", in->a, tb);
            a64_ld_fp(b, "d1", in->b, tb);
            nb_put(b, in->op == IR_FADD ? "  fadd d0, d0, d1\n" :
                      in->op == IR_FSUB ? "  fsub d0, d0, d1\n" :
                      in->op == IR_FMUL ? "  fmul d0, d0, d1\n" : "  fdiv d0, d0, d1\n");
            a64_slot_addr(b, in->dst, tb);
            nb_put(b, "  str d0, [x9]\n");
            break;
        case IR_FCMP:
            a64_ld_fp(b, "d0", in->a, tb);
            a64_ld_fp(b, "d1", in->b, tb);
            nb_put(b, "  fcmp d0, d1\n  cset x0, %s\n",
                   in->imm == 0 ? "eq" : in->imm == 1 ? "ne" :
                   in->imm == 2 ? "mi" : in->imm == 3 ? "ls" :
                   in->imm == 4 ? "hi" : "ge");
            a64_st_x0(b, in->dst, tb);
            break;
        case IR_NEG:
            a64_ld_x0(b, in->a, tb);
            nb_put(b, "  neg x0, x0\n");
            a64_st_x0(b, in->dst, tb);
            break;
        case IR_NOT:
            a64_ld_x0(b, in->a, tb);
            nb_put(b, "  mvn x0, x0\n");
            a64_st_x0(b, in->dst, tb);
            break;
        case IR_CMP_EQ: case IR_CMP_NE: case IR_CMP_LT:
        case IR_CMP_LE: case IR_CMP_GT: case IR_CMP_GE:
            a64_ld_x0(b, in->a, tb);
            a64_ld_reg(b, "x1", in->b, tb);
            nb_put(b, "  cmp x0, x1\n  cset x0, %s\n",
                   in->op == IR_CMP_EQ ? "eq" : in->op == IR_CMP_NE ? "ne" :
                   in->op == IR_CMP_LT ? "lt" : in->op == IR_CMP_LE ? "le" :
                   in->op == IR_CMP_GT ? "gt" : "ge");
            a64_st_x0(b, in->dst, tb);
            break;
        case IR_JMP:
            nb_put(b, "  b .L%d\n", (int)in->label);
            break;
        case IR_JZ: case IR_JNZ:
            a64_ld_x0(b, in->a, tb);
            nb_put(b, "  %s x0, .L%d\n",
                   in->op == IR_JZ ? "cbz" : "cbnz", (int)in->label);
            break;
        case IR_LABEL:
            nb_put(b, ".L%d:\n", (int)in->label);
            break;
        case IR_LD_ADDR:
            nb_put(b, "  adrp x0, %s\n  add x0, x0, :lo12:%s\n", in->sym, in->sym);
            a64_st_x0(b, in->dst, tb);
            break;
        case IR_ADDR:
            /* 地址 = 槽 a+imm 的地址（x9 → x0） */
            a64_slot_addr(b, in->a + (int)in->imm, tb);
            nb_put(b, "  mov x0, x9\n");
            a64_st_x0(b, in->dst, tb);
            break;
        case IR_ELEM_ADDR:
            /* 元素地址 = 基地址值 - idx*8（槽向下，与 x86/riscv 同方向） */
            a64_ld_x0(b, in->a, tb);
            a64_ld_reg(b, "x1", in->b, tb);
            nb_put(b, "  sub x0, x0, x1, lsl #3\n");
            a64_st_x0(b, in->dst, tb);
            break;
        case IR_LOAD:
            a64_ld_x0(b, in->a, tb);           /* 地址值 */
            nb_put(b, "  ldr x0, [x0]\n");
            a64_st_x0(b, in->dst, tb);
            break;
        case IR_STORE:
            a64_ld_x0(b, in->a, tb);           /* 地址值 */
            a64_ld_reg(b, "x1", in->b, tb);    /* 值 */
            nb_put(b, "  str x1, [x0]\n");
            break;
        case IR_CALL: case IR_CALLI: {
            int nargs = (int)in->imm;
            int temp[64];
            int tc = 0;
            for (int k = 0; k < i && tc < 64; k++) {
                if (f->ins[k].op == IR_PARAM) temp[tc++] = f->ins[k].a;
            }
            int start = tc > nargs ? tc - nargs : 0;
            int got = tc - start;
            /* 参数装载分流：int → x0-x7，double → d0-d7（AAPCS） */
            int ii = 0, fi = 0;
            for (int k = 0; k < got; k++) {
                int vr = temp[start + k];
                if (f->vreg_type && vr < f->vreg_count && f->vreg_type[vr] == 1) {
                    if (fi < tb->fp_arg_count) {
                        a64_ld_fp(b, tb->fp_arg_regs[fi++], vr, tb);
                    }
                } else {
                    if (ii < tb->int_arg_count) {
                        a64_ld_reg(b, tb->int_arg_regs[ii++], vr, tb);
                    }
                }
            }
            if (in->op == IR_CALL) {
                nb_put(b, "  bl %s\n", in->sym);
            } else {
                a64_ld_x0(b, in->a, tb);
                nb_put(b, "  blr x0\n");
            }
            /* 返回值存储：目标函数浮点返回 → d0，否则 x0 */
            if (in->op == IR_CALL) {
                int callee_dbl = 0;
                for (int k = 0; k < p->fn_count; k++) {
                    if (p->fns[k].name && in->sym &&
                        strcmp(p->fns[k].name, in->sym) == 0) {
                        callee_dbl = p->fns[k].ret_is_double;
                        break;
                    }
                }
                if (callee_dbl) {
                    a64_slot_addr(b, in->dst, tb);
                    nb_put(b, "  str d0, [x9]\n");
                    break;
                }
            }
            a64_st_x0(b, in->dst, tb);
            break;
        }
        case IR_RET:
            if (in->a >= 0) {
                if (f->vreg_type && in->a < f->vreg_count &&
                    f->vreg_type[in->a] == 1) {
                    a64_slot_addr(b, in->a, tb);
                    nb_put(b, "  ldr d0, [x9]\n");
                } else {
                    a64_ld_x0(b, in->a, tb);
                }
            } else {
                nb_put(b, "  mov x0, xzr\n");
            }
            nb_put(b, "  ldp x29, x30, [sp], #16\n  ret\n");
            break;
        default:
            break;
    }
}

/* AArch64 帧布局：x29/x30 在 [sp,#16]! 压栈后，槽区在 x29 之下 */
static void a64_fn_prologue(NBuf *b, const TargetBackend *tb,
                            const IrFn *f, int frame)
{
    g_a64_frame = frame;
    nb_put(b, "  stp x29, x30, [sp, #-16]!\n");
    nb_put(b, "  mov x29, sp\n");
    if (frame <= 4095) {
        nb_put(b, "  sub sp, sp, #%d\n", frame);
    } else {
        nb_put(b, "  movz x9, #%d\n", frame & 0xFFFF);
        if (frame > 0xFFFF) nb_put(b, "  movk x9, #%d, lsl #16\n", (frame >> 16) & 0xFFFF);
        nb_put(b, "  sub sp, sp, x9\n");
    }
    /* 参数入槽：int → x0-x7（str），double → d0-d7（str d） */
    int ii = 0, fi = 0;
    for (int i = 0; i < f->param_count; i++) {
        if (f->param_types[i] == 1) {
            if (fi < tb->fp_arg_count) {
                a64_slot_addr(b, i, tb);
                nb_put(b, "  str %s, [x9]\n", tb->fp_arg_regs[fi++]);
            }
        } else {
            if (ii < tb->int_arg_count) {
                a64_slot_addr(b, i, tb);
                nb_put(b, "  str %s, [x9]\n", tb->int_arg_regs[ii++]);
            }
        }
    }
}

static void a64_fn_epilogue(NBuf *b, const TargetBackend *tb,
                            const IrFn *f, int need_ret)
{
    (void)tb;
    if (need_ret) {
        if (f->ret_is_double) {
            nb_put(b, "  fmov d0, xzr\n");   /* 浮点隐式返回 0.0 */
        } else {
            nb_put(b, "  mov x0, xzr\n");
        }
        nb_put(b, "  add sp, sp, #%d\n", g_a64_frame);
        nb_put(b, "  ldp x29, x30, [sp], #16\n  ret\n");
    }
}

static const char *a64_int_arg_regs[] = { "x0", "x1", "x2", "x3", "x4", "x5", "x6", "x7", NULL };
static const char *a64_fp_arg_regs[]  = { "d0", "d1", "d2", "d3", "d4", "d5", "d6", "d7", NULL };

const TargetBackend arm64_backend = {
    .name         = "arm64",
    .asm_syntax   = "att",
    .slot_stride  = 8,
    .stack_dir    = -1,
    .callee_align = 16,
    .frame_extra  = 16,           /* x29 + x30 保存 */
    .int_arg_regs = a64_int_arg_regs,
    .int_arg_count = 8,
    .fp_arg_regs  = a64_fp_arg_regs,
    .fp_arg_count = 8,
    .ret_reg      = "x0",
    .fp_ret_reg   = "d0",
    .fn_prologue  = a64_fn_prologue,
    .emit_ins     = a64_emit_ins,
    .fn_epilogue  = a64_fn_epilogue,
    .expand       = NULL,
};
