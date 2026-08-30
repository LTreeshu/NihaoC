#include "ir_backend.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ============================================================
 * riscv64 后端（阶段 3：只验汇编生成，本机不汇编/运行）
 *
 * 约定（RV64I + D 浮点扩展，GAS 语法）：
 *  - 全栈槽：vreg N 在 s0-stride*(N+1)（槽向下生长）
 *  - 帧指针 s0（fp）；ra 保存在 8(sp)、s0 在 0(sp)
 *  - 调用约定：a0-a7 整数参数/返回（a0 返回），fa0-fa7 浮点参数
 *  - 缺失指令：RV64I 有 div/rem，无 IDIV 展开需求；浮点用 D 扩展
 * ============================================================ */

/* 当前函数帧大小（prologue 设置，IR_RET/epilogue 恢复 sp 用） */
static int g_rv_frame = 0;

static void rv_slot(NBuf *b, int vreg, const TargetBackend *tb)
{
    nb_put(b, "-%d(s0)", tb->slot_stride * (vreg + 1));
}

/* 装载 a=slot(x)，a1=slot(y) 到 a0/a1 */
static void rv_ld2(NBuf *b, const IrIns *in, const TargetBackend *tb)
{
    nb_put(b, "  ld a0, ");
    rv_slot(b, in->a, tb);
    nb_put(b, "\n  ld a1, ");
    rv_slot(b, in->b, tb);
    nb_put(b, "\n");
}

static void rv_store_a0(NBuf *b, int dst, const TargetBackend *tb)
{
    nb_put(b, "  sd a0, ");
    rv_slot(b, dst, tb);
    nb_put(b, "\n");
}

static void rv_emit_ins(NBuf *b, const IrIns *in, const TargetBackend *tb,
                        const IrProg *p, const IrFn *f, int i)
{
    (void)p; (void)f; (void)i;
    switch (in->op) {
        case IR_CONST:
            nb_put(b, "  li a0, %lld\n", (long long)in->imm);
            rv_store_a0(b, in->dst, tb);
            break;
        case IR_MOV:
            nb_put(b, "  ld a0, ");
            rv_slot(b, in->a, tb);
            nb_put(b, "\n");
            rv_store_a0(b, in->dst, tb);
            break;
        case IR_ADD: case IR_SUB: case IR_MUL:
            rv_ld2(b, in, tb);
            nb_put(b, in->op == IR_ADD ? "  add a0, a0, a1\n" :
                      in->op == IR_SUB ? "  sub a0, a0, a1\n" : "  mul a0, a0, a1\n");
            rv_store_a0(b, in->dst, tb);
            break;
        case IR_SHL: case IR_SHR: case IR_AND: case IR_OR:
            rv_ld2(b, in, tb);
            nb_put(b, in->op == IR_SHL ? "  sll a0, a0, a1\n" :
                      in->op == IR_SHR ? "  srl a0, a0, a1\n" :
                      in->op == IR_AND ? "  and a0, a0, a1\n" : "  or a0, a0, a1\n");
            rv_store_a0(b, in->dst, tb);
            break;
        case IR_DIV: case IR_MOD:
            rv_ld2(b, in, tb);
            nb_put(b, in->op == IR_DIV ? "  div a0, a0, a1\n" : "  rem a0, a0, a1\n");
            rv_store_a0(b, in->dst, tb);
            break;
        case IR_ITOD:
            /* int64 槽 → a0 → fcvt.d.l → fa0 → fsd 槽（D 扩展） */
            nb_put(b, "  ld a0, ");
            rv_slot(b, in->a, tb);
            nb_put(b, "\n  fcvt.d.l fa0, a0\n  fsd fa0, ");
            rv_slot(b, in->dst, tb);
            nb_put(b, "\n");
            break;
        case IR_DTOI:
            /* double → int64 截断（D 扩展 fcvt.l.d，向零舍入） */
            nb_put(b, "  fld fa0, ");
            rv_slot(b, in->a, tb);
            nb_put(b, "\n  fcvt.l.d a0, fa0\n  sd a0, ");
            rv_slot(b, in->dst, tb);
            nb_put(b, "\n");
            break;
        case IR_TRUNC:
            /* 窄整数截断 + 符号/零扩展：slli 对齐高位 + srai/srli（imm 编码） */
            {
                int sh = (in->imm == 0 || in->imm == 3) ? 56 :
                         (in->imm == 1 || in->imm == 4) ? 48 : 32;
                nb_put(b, "  ld a0, ");
                rv_slot(b, in->a, tb);
                nb_put(b, "\n  slli a0, a0, %d\n  %s a0, a0, %d\n  sd a0, ",
                       sh, in->imm >= 3 ? "srli" : "srai", sh);
                rv_slot(b, in->dst, tb);
                nb_put(b, "\n");
            }
            break;
        case IR_FADD: case IR_FSUB: case IR_FMUL: case IR_FDIV:
            /* D 扩展：fld fa0/fa1 → op.d → fsd（riscv 浮点方向明确，无 x87 坑） */
            nb_put(b, "  fld fa0, ");
            rv_slot(b, in->a, tb);
            nb_put(b, "\n  fld fa1, ");
            rv_slot(b, in->b, tb);
            nb_put(b, "\n  %s.d fa0, fa0, fa1\n  fsd fa0, ",
                   in->op == IR_FADD ? "fadd" :
                   in->op == IR_FSUB ? "fsub" :
                   in->op == IR_FMUL ? "fmul" : "fdiv");
            rv_slot(b, in->dst, tb);
            nb_put(b, "\n");
            break;
        case IR_FCMP:
            /* 浮点比较：fld fa0/fa1 → feq/flt/fle.d → 0/1（GT/GE 换操作数） */
            nb_put(b, "  fld fa0, ");
            rv_slot(b, in->a, tb);
            nb_put(b, "\n  fld fa1, ");
            rv_slot(b, in->b, tb);
            nb_put(b, "\n  ");
            nb_put(b, in->imm == 0 ? "feq.d a0, fa0, fa1\n" :
                      in->imm == 1 ? "feq.d a0, fa0, fa1\n  xori a0, a0, 1\n" :
                      in->imm == 2 ? "flt.d a0, fa0, fa1\n" :
                      in->imm == 3 ? "fle.d a0, fa0, fa1\n" :
                      in->imm == 4 ? "flt.d a0, fa1, fa0\n" :
                                    "fle.d a0, fa1, fa0\n");
            rv_store_a0(b, in->dst, tb);
            break;
        case IR_NEG:
            nb_put(b, "  ld a0, ");
            rv_slot(b, in->a, tb);
            nb_put(b, "\n  neg a0, a0\n");
            rv_store_a0(b, in->dst, tb);
            break;
        case IR_NOT:
            nb_put(b, "  ld a0, ");
            rv_slot(b, in->a, tb);
            nb_put(b, "\n  not a0, a0\n");
            rv_store_a0(b, in->dst, tb);
            break;
        case IR_CMP_EQ: case IR_CMP_NE: case IR_CMP_LT:
        case IR_CMP_LE: case IR_CMP_GT: case IR_CMP_GE:
            rv_ld2(b, in, tb);
            switch (in->op) {
                case IR_CMP_EQ: nb_put(b, "  xor a0, a0, a1\n  seqz a0, a0\n"); break;
                case IR_CMP_NE: nb_put(b, "  xor a0, a0, a1\n  snez a0, a0\n"); break;
                case IR_CMP_LT: nb_put(b, "  slt a0, a0, a1\n"); break;
                case IR_CMP_LE: nb_put(b, "  slt a0, a1, a0\n  xori a0, a0, 1\n"); break;
                case IR_CMP_GT: nb_put(b, "  slt a0, a1, a0\n"); break;
                case IR_CMP_GE: nb_put(b, "  slt a0, a0, a1\n  xori a0, a0, 1\n"); break;
                default: break;
            }
            rv_store_a0(b, in->dst, tb);
            break;
        case IR_JMP:
            nb_put(b, "  j .L%d\n", (int)in->label);
            break;
        case IR_JZ: case IR_JNZ:
            nb_put(b, "  ld a0, ");
            rv_slot(b, in->a, tb);
            nb_put(b, "\n  %s a0, .L%d\n",
                   in->op == IR_JZ ? "beqz" : "bnez", (int)in->label);
            break;
        case IR_LABEL:
            nb_put(b, ".L%d:\n", (int)in->label);
            break;
        case IR_LD_ADDR:
            nb_put(b, "  la a0, %s\n", in->sym);
            rv_store_a0(b, in->dst, tb);
            break;
        case IR_ADDR:
            /* 地址 = s0 - stride*(a+imm+1)（槽向下生长，槽号偏移 imm） */
            nb_put(b, "  addi a0, s0, -%d\n",
                   tb->slot_stride * (in->a + (int)in->imm + 1));
            rv_store_a0(b, in->dst, tb);
            break;
        case IR_ELEM_ADDR:
            /* 元素地址 = 基址 - idx*width（槽向下；imm=宽度，0 兼容=8） */
            nb_put(b, "  ld a0, ");
            rv_slot(b, in->a, tb);
            nb_put(b, "\n  ld a1, ");
            rv_slot(b, in->b, tb);
            nb_put(b, "\n  slli a1, a1, %d\n  sub a0, a0, a1\n",
                   in->imm == 1 ? 0 : in->imm == 2 ? 1 :
                   in->imm == 4 ? 2 : 3);
            rv_store_a0(b, in->dst, tb);
            break;
        case IR_LOAD:
            nb_put(b, "  ld a0, ");
            rv_slot(b, in->a, tb);
            nb_put(b, "\n  ld a0, 0(a0)\n");
            rv_store_a0(b, in->dst, tb);
            break;
        case IR_STORE:
            nb_put(b, "  ld a0, ");
            rv_slot(b, in->a, tb);
            nb_put(b, "\n  ld a1, ");
            rv_slot(b, in->b, tb);
            nb_put(b, "\n  sd a1, 0(a0)\n");
            break;
        case IR_LOAD8:
            nb_put(b, "  ld a0, ");
            rv_slot(b, in->a, tb);
            nb_put(b, "\n  lbu a0, 0(a0)\n");
            rv_store_a0(b, in->dst, tb);
            break;
        case IR_STORE8:
            nb_put(b, "  ld a0, ");
            rv_slot(b, in->a, tb);
            nb_put(b, "\n  ld a1, ");
            rv_slot(b, in->b, tb);
            nb_put(b, "\n  sb a1, 0(a0)\n");
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
            /* 参数装载分流（PB-浮点 ABI）：int → a0-a7（ld），double → fa0-fa7（fld） */
            int ii = 0, fi = 0;
            for (int k = 0; k < got; k++) {
                int vr = temp[start + k];
                if (f->vreg_type && vr < f->vreg_count && f->vreg_type[vr] == 1) {
                    if (fi < tb->fp_arg_count) {
                        nb_put(b, "  fld %s, ", tb->fp_arg_regs[fi++]);
                        rv_slot(b, vr, tb);
                        nb_put(b, "\n");
                    }
                } else {
                    if (ii < tb->int_arg_count) {
                        nb_put(b, "  ld %s, ", tb->int_arg_regs[ii++]);
                        rv_slot(b, vr, tb);
                        nb_put(b, "\n");
                    }
                }
            }
            if (in->op == IR_CALL) {
                nb_put(b, "  call %s\n", in->sym);
            } else {
                nb_put(b, "  ld a0, ");
                rv_slot(b, in->a, tb);
                nb_put(b, "\n  jalr ra, a0\n");
            }
            /* 返回值存储：目标函数浮点返回 → fa0（fsd），否则 a0（sd） */
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
                    nb_put(b, "  fsd fa0, ");
                    rv_slot(b, in->dst, tb);
                    nb_put(b, "\n");
                    break;
                }
            }
            rv_store_a0(b, in->dst, tb);
            break;
        }
        case IR_RET:
            if (in->a >= 0) {
                if (f->vreg_type && in->a < f->vreg_count &&
                    f->vreg_type[in->a] == 1) {
                    nb_put(b, "  fld fa0, ");
                    rv_slot(b, in->a, tb);
                    nb_put(b, "\n");
                } else {
                    nb_put(b, "  ld a0, ");
                    rv_slot(b, in->a, tb);
                    nb_put(b, "\n");
                }
            } else {
                nb_put(b, "  li a0, 0\n");
            }
            nb_put(b, "  ld ra, 8(sp)\n  ld s0, 0(sp)\n  addi sp, sp, %d\n  ret\n",
                   g_rv_frame);
            break;
        default:
            break;
    }
}

/* riscv64 帧布局：ra 在 8(sp)、s0 在 0(sp)，槽区从 16(sp) 起 */
static void rv_fn_prologue(NBuf *b, const TargetBackend *tb,
                           const IrFn *f, int frame)
{
    g_rv_frame = frame;
    nb_put(b, "  addi sp, sp, -%d\n", frame);
    nb_put(b, "  sd ra, 8(sp)\n");
    nb_put(b, "  sd s0, 0(sp)\n");
    nb_put(b, "  addi s0, sp, %d\n", frame);
    /* 参数装载（PB-浮点 ABI）：int → a0-a7（sd 槽），double → fa0-fa7（fsd 槽） */
    int ii = 0, fi = 0;
    for (int i = 0; i < f->param_count; i++) {
        if (f->param_types[i] == 1) {
            if (fi < tb->fp_arg_count) {
                nb_put(b, "  fsd %s, ", tb->fp_arg_regs[fi++]);
                rv_slot(b, i, tb);
                nb_put(b, "\n");
            }
        } else {
            if (ii < tb->int_arg_count) {
                nb_put(b, "  sd %s, ", tb->int_arg_regs[ii++]);
                rv_slot(b, i, tb);
                nb_put(b, "\n");
            }
        }
    }
}

/* epilogue：显式 ret 已由 IR_RET 发射（含帧恢复）；
 * need_ret=1（隐式返回）时补完整返回序列 */
static void rv_fn_epilogue(NBuf *b, const TargetBackend *tb,
                           const IrFn *f, int need_ret)
{
    (void)tb;
    if (need_ret) {
        if (f->ret_is_double) {
            nb_put(b, "  fmv.d.x fa0, zero\n");   /* 浮点隐式返回 0.0 */
        } else {
            nb_put(b, "  li a0, 0\n");
        }
        nb_put(b, "  ld ra, 8(sp)\n  ld s0, 0(sp)\n  addi sp, sp, %d\n  ret\n",
               g_rv_frame);
    }
}

static const char *rv_int_arg_regs[] = { "a0", "a1", "a2", "a3", "a4", "a5", "a6", "a7", NULL };
static const char *rv_fp_arg_regs[]  = { "fa0", "fa1", "fa2", "fa3", "fa4", "fa5", "fa6", "fa7", NULL };

const TargetBackend riscv64_backend = {
    .name         = "riscv64",
    .asm_syntax   = "att",
    .slot_stride  = 8,
    .stack_dir    = -1,
    .callee_align = 16,
    .frame_extra  = 16,           /* ra + s0 保存 */
    .int_arg_regs = rv_int_arg_regs,
    .int_arg_count = 8,
    .fp_arg_regs  = rv_fp_arg_regs,
    .fp_arg_count = 8,
    .ret_reg      = "a0",
    .fp_ret_reg   = "fa0",
    .fn_prologue  = rv_fn_prologue,
    .emit_ins     = rv_emit_ins,
    .fn_epilogue  = rv_fn_epilogue,
    .expand       = NULL,
};
