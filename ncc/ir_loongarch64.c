#include "ir_backend.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ============================================================
 * loongarch64 后端（阶段 4；2026-09-27 起本机用 LLVM 的 LoongArch MC 层
 * 汇编验证，见 TODO-PB 的 PB-35——之前只验「生成了文本」，从未验过语法）
 *
 * 约定（LoongArch64 基础指令集 + D 扩展，GAS 语法，寄存器带 $ 前缀）：
 *  - 全栈槽：vreg N 在 $s0 负偏移 stride*(N+1)（槽向下生长）
 *  - 访存操作数是「基址在前」的三操作数形式：`ld.d $a0, $s0, -8`，
 *    不是 AT&T 的 `-8($s0)`；偏移量必须在 ±2047 之内（超界的发射路径未实现）
 *  - 帧指针 $s0（fp）；$ra 保存在 8(sp)、$s0 在 0(sp)
 *  - 调用约定：$a0-$a7 整数参数/返回（$a0 返回），$fa0-$fa7 浮点参数
 *  - 立即数：li.d 伪指令（汇编器展开，支持任意 64 位）
 *  - 浮点：int→double 是 movgr2fr.d 再 ffint.d.l（两个操作数都是浮点寄存器）；
 *    double→int 是 ftintrz.l.d（向零舍入，同样 f→f）再 movfr2gr.d；
 *    比较是 fcmp.<cnd>.d $fcc0, $fa0, $fa1 写条件码，再 movcf2gr + andi 取 0/1；
 *    gt/ge 通过交换操作数用 clt/cle 表达
 *  - LA64 无 neg/not 单指令：sub.d/nor 组合
 *  - 整数逻辑与移位无 .d 后缀的写法分两套：and/or/xor/nor/slt/sltui 不带 .d，
 *    移位 slli.d/srli.d/srai.d 与算术 add.d/sub.d/mul.d/div.d/mod.d 带 .d
 * ============================================================ */

/* 当前函数帧大小（prologue 设置，IR_RET/epilogue 恢复 sp 用） */
static int g_la_frame = 0;

/* 槽操作数：LoongArch 是「基址在前」的三操作数语法（`st.d $a0, $s0, -8`），
 * 不是 AT&T 的 `-8($s0)` 形式——后者 GAS 与 LLVM 都拒绝，实测见 TODO-PB 的 PB-35。 */
static void la_slot(NBuf *b, int vreg, const TargetBackend *tb)
{
    nb_put(b, "$s0, -%d", tb->slot_stride * (vreg + 1));
}

/* 装载 a=slot(x)，a1=slot(y) 到 $a0/$a1 */
static void la_ld2(NBuf *b, const IrIns *in, const TargetBackend *tb)
{
    nb_put(b, "  ld.d $a0, ");
    la_slot(b, in->a, tb);
    nb_put(b, "\n  ld.d $a1, ");
    la_slot(b, in->b, tb);
    nb_put(b, "\n");
}

static void la_store_a0(NBuf *b, int dst, const TargetBackend *tb)
{
    nb_put(b, "  st.d $a0, ");
    la_slot(b, dst, tb);
    nb_put(b, "\n");
}

static void la_emit_ins(NBuf *b, const IrIns *in, const TargetBackend *tb,
                        const IrProg *p, const IrFn *f, int i)
{
    (void)p; (void)f; (void)i;
    switch (in->op) {
        case IR_CONST:
            nb_put(b, "  li.d $a0, %lld\n", (long long)in->imm);
            la_store_a0(b, in->dst, tb);
            break;
        case IR_MOV:
            nb_put(b, "  ld.d $a0, ");
            la_slot(b, in->a, tb);
            nb_put(b, "\n");
            la_store_a0(b, in->dst, tb);
            break;
        case IR_ADD: case IR_SUB: case IR_MUL:
            la_ld2(b, in, tb);
            nb_put(b, in->op == IR_ADD ? "  add.d $a0, $a0, $a1\n" :
                      in->op == IR_SUB ? "  sub.d $a0, $a0, $a1\n" : "  mul.d $a0, $a0, $a1\n");
            la_store_a0(b, in->dst, tb);
            break;
        case IR_SHL: case IR_SHR: case IR_AND: case IR_OR:
            la_ld2(b, in, tb);
            nb_put(b, in->op == IR_SHL ? "  sll.d $a0, $a0, $a1\n" :
                      in->op == IR_SHR ? "  srl.d $a0, $a0, $a1\n" :
                      in->op == IR_AND ? "  and $a0, $a0, $a1\n" : "  or $a0, $a0, $a1\n");
            la_store_a0(b, in->dst, tb);
            break;
        case IR_DIV: case IR_MOD:
            la_ld2(b, in, tb);
            nb_put(b, in->op == IR_DIV ? "  div.d $a0, $a0, $a1\n" : "  mod.d $a0, $a0, $a1\n");
            la_store_a0(b, in->dst, tb);
            break;
        case IR_ITOD:
            /* int64 槽 → $a0 → movgr2fr.d → $fa0 → ffint.d.l（f→f）→ fst.d 槽 */
            nb_put(b, "  ld.d $a0, ");
            la_slot(b, in->a, tb);
            nb_put(b, "\n  movgr2fr.d $fa0, $a0\n  ffint.d.l $fa0, $fa0\n  fst.d $fa0, ");
            la_slot(b, in->dst, tb);
            nb_put(b, "\n");
            break;
        case IR_DTOI:
            /* double → int64 截断：ftintrz.l.d 仍是 f→f，再 movfr2gr.d 回整数 */
            nb_put(b, "  fld.d $fa0, ");
            la_slot(b, in->a, tb);
            nb_put(b, "\n  ftintrz.l.d $fa0, $fa0\n  movfr2gr.d $a0, $fa0\n");
            la_store_a0(b, in->dst, tb);
            break;
        case IR_FTRUNC:
            /* f32 严格宽度：double → float 截断再回 double（fcvt.s.d + fcvt.d.s） */
            nb_put(b, "  fld.d $fa0, ");
            la_slot(b, in->a, tb);
            nb_put(b, "\n  fcvt.s.d $fa0, $fa0\n  fcvt.d.s $fa0, $fa0\n  fst.d $fa0, ");
            la_slot(b, in->dst, tb);
            nb_put(b, "\n");
            break;
        case IR_TRUNC:
            /* 窄整数截断 + 符号/零扩展：slli.d 对齐高位 + srai.d/srli.d（imm 编码） */
            {
                int sh = (in->imm == 0 || in->imm == 3) ? 56 :
                         (in->imm == 1 || in->imm == 4) ? 48 : 32;
                nb_put(b, "  ld.d $a0, ");
                la_slot(b, in->a, tb);
                nb_put(b, "\n  slli.d $a0, $a0, %d\n  %s.d $a0, $a0, %d\n",
                       sh, in->imm >= 3 ? "srli" : "srai", sh);
                la_store_a0(b, in->dst, tb);
            }
            break;
        case IR_FADD: case IR_FSUB: case IR_FMUL: case IR_FDIV:
            /* D 扩展：fld.d $fa0/$fa1 → op.d → fst.d */
            nb_put(b, "  fld.d $fa0, ");
            la_slot(b, in->a, tb);
            nb_put(b, "\n  fld.d $fa1, ");
            la_slot(b, in->b, tb);
            nb_put(b, "\n  %s.d $fa0, $fa0, $fa1\n  fst.d $fa0, ",
                   in->op == IR_FADD ? "fadd" :
                   in->op == IR_FSUB ? "fsub" :
                   in->op == IR_FMUL ? "fmul" : "fdiv");
            la_slot(b, in->dst, tb);
            nb_put(b, "\n");
            break;
        case IR_FCMP:
            /* 浮点比较：fcmp.<cnd>.d 写 $fcc0 → movcf2gr 取回整数 → andi 保 0/1。
             * gt/ge 无独立条件码，交换两个操作数用 clt/cle 表达。 */
            nb_put(b, "  fld.d $fa0, ");
            la_slot(b, in->a, tb);
            nb_put(b, "\n  fld.d $fa1, ");
            la_slot(b, in->b, tb);
            switch (in->imm) {
                case 0: nb_put(b, "\n  fcmp.ceq.d $fcc0, $fa0, $fa1\n"); break;
                case 1: nb_put(b, "\n  fcmp.ceq.d $fcc0, $fa0, $fa1\n  movcf2gr $a0, $fcc0\n"
                                   "  xori $a0, $a0, 1\n  andi $a0, $a0, 1\n");
                        la_store_a0(b, in->dst, tb);
                        break;
                case 2: nb_put(b, "\n  fcmp.clt.d $fcc0, $fa0, $fa1\n"); break;
                case 3: nb_put(b, "\n  fcmp.cle.d $fcc0, $fa0, $fa1\n"); break;
                case 4: nb_put(b, "\n  fcmp.clt.d $fcc0, $fa1, $fa0\n"); break;
                default: nb_put(b, "\n  fcmp.cle.d $fcc0, $fa1, $fa0\n"); break;
            }
            if (in->imm != 1) {
                nb_put(b, "  movcf2gr $a0, $fcc0\n  andi $a0, $a0, 1\n");
                la_store_a0(b, in->dst, tb);
            }
            break;
        case IR_NEG:
            nb_put(b, "  ld.d $a0, ");
            la_slot(b, in->a, tb);
            nb_put(b, "\n  sub.d $a0, $zero, $a0\n");
            la_store_a0(b, in->dst, tb);
            break;
        case IR_NOT:
            nb_put(b, "  ld.d $a0, ");
            la_slot(b, in->a, tb);
            nb_put(b, "\n  nor $a0, $a0, $zero\n");
            la_store_a0(b, in->dst, tb);
            break;
        case IR_CMP_EQ: case IR_CMP_NE: case IR_CMP_LT:
        case IR_CMP_LE: case IR_CMP_GT: case IR_CMP_GE:
            la_ld2(b, in, tb);
            switch (in->op) {
                /* 逻辑与比较类指令在 LoongArch 无 .d 后缀（64 位即默认宽度） */
                case IR_CMP_EQ: nb_put(b, "  xor $a0, $a0, $a1\n  sltui $a0, $a0, 1\n"); break;
                case IR_CMP_NE: nb_put(b, "  xor $a0, $a0, $a1\n  sltu $a0, $zero, $a0\n"); break;
                case IR_CMP_LT: nb_put(b, "  slt $a0, $a0, $a1\n"); break;
                case IR_CMP_LE: nb_put(b, "  slt $a0, $a1, $a0\n  xori $a0, $a0, 1\n"); break;
                case IR_CMP_GT: nb_put(b, "  slt $a0, $a1, $a0\n"); break;
                case IR_CMP_GE: nb_put(b, "  slt $a0, $a0, $a1\n  xori $a0, $a0, 1\n"); break;
                default: break;
            }
            la_store_a0(b, in->dst, tb);
            break;
        case IR_JMP:
            nb_put(b, "  b .L%d\n", (int)in->label);
            break;
        case IR_JZ: case IR_JNZ:
            nb_put(b, "  ld.d $a0, ");
            la_slot(b, in->a, tb);
            nb_put(b, "\n  %s $a0, .L%d\n",
                   in->op == IR_JZ ? "beqz" : "bnez", (int)in->label);
            break;
        case IR_LABEL:
            nb_put(b, ".L%d:\n", (int)in->label);
            break;
        case IR_LD_ADDR:
            nb_put(b, "  la $a0, %s\n", in->sym);
            la_store_a0(b, in->dst, tb);
            break;
        case IR_ADDR:
            /* 地址 = $s0 - stride*(a+imm+1)（槽向下生长，槽号偏移 imm） */
            nb_put(b, "  addi.d $a0, $s0, -%d\n",
                   tb->slot_stride * (in->a + (int)in->imm + 1));
            la_store_a0(b, in->dst, tb);
            break;
        case IR_ELEM_ADDR:
            /* 元素地址 = 基址 - idx*width（槽向下；imm=宽度，0 兼容=8） */
            nb_put(b, "  ld.d $a0, ");
            la_slot(b, in->a, tb);
            nb_put(b, "\n  ld.d $a1, ");
            la_slot(b, in->b, tb);
            nb_put(b, "\n  slli.d $a1, $a1, %d\n  sub.d $a0, $a0, $a1\n",
                   in->imm == 1 ? 0 : in->imm == 2 ? 1 :
                   in->imm == 4 ? 2 : 3);
            la_store_a0(b, in->dst, tb);
            break;
        case IR_LOAD:
            nb_put(b, "  ld.d $a0, ");
            la_slot(b, in->a, tb);
            nb_put(b, "\n  ld.d $a0, $a0, 0\n");
            la_store_a0(b, in->dst, tb);
            break;
        case IR_STORE:
            nb_put(b, "  ld.d $a0, ");
            la_slot(b, in->a, tb);
            nb_put(b, "\n  ld.d $a1, ");
            la_slot(b, in->b, tb);
            nb_put(b, "\n  st.d $a1, $a0, 0\n");
            break;
        case IR_LOAD8:
            nb_put(b, "  ld.d $a0, ");
            la_slot(b, in->a, tb);
            nb_put(b, "\n  ld.bu $a0, $a0, 0\n");
            la_store_a0(b, in->dst, tb);
            break;
        case IR_STORE8:
            nb_put(b, "  ld.d $a0, ");
            la_slot(b, in->a, tb);
            nb_put(b, "\n  ld.d $a1, ");
            la_slot(b, in->b, tb);
            nb_put(b, "\n  st.b $a1, $a0, 0\n");
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
            /* 参数装载分流：int → $a0-$a7（ld.d），double → $fa0-$fa7（fld.d） */
            int ii = 0, fi = 0;
            for (int k = 0; k < got; k++) {
                int vr = temp[start + k];
                if (f->vreg_type && vr < f->vreg_count && f->vreg_type[vr] == 1) {
                    if (fi < tb->fp_arg_count) {
                        nb_put(b, "  fld.d %s, ", tb->fp_arg_regs[fi++]);
                        la_slot(b, vr, tb);
                        nb_put(b, "\n");
                    }
                } else {
                    if (ii < tb->int_arg_count) {
                        nb_put(b, "  ld.d %s, ", tb->int_arg_regs[ii++]);
                        la_slot(b, vr, tb);
                        nb_put(b, "\n");
                    }
                }
            }
            if (in->op == IR_CALL) {
                nb_put(b, "  bl %s\n", in->sym);
            } else {
                nb_put(b, "  ld.d $a0, ");
                la_slot(b, in->a, tb);
                nb_put(b, "\n  jirl $ra, $a0, 0\n");
            }
            /* 返回值存储：目标函数浮点返回 → $fa0（fst.d），否则 $a0（st.d） */
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
                    nb_put(b, "  fst.d $fa0, ");
                    la_slot(b, in->dst, tb);
                    nb_put(b, "\n");
                    break;
                }
            }
            la_store_a0(b, in->dst, tb);
            break;
        }
        case IR_RET:
            if (in->a >= 0) {
                if (f->vreg_type && in->a < f->vreg_count &&
                    f->vreg_type[in->a] == 1) {
                    nb_put(b, "  fld.d $fa0, ");
                    la_slot(b, in->a, tb);
                    nb_put(b, "\n");
                } else {
                    nb_put(b, "  ld.d $a0, ");
                    la_slot(b, in->a, tb);
                    nb_put(b, "\n");
                }
            } else {
                nb_put(b, "  ori $a0, $zero, 0\n");
            }
            nb_put(b, "  ld.d $ra, $sp, 8\n  ld.d $s0, $sp, 0\n  addi.d $sp, $sp, %d\n  jirl $zero, $ra, 0\n",
                   g_la_frame);
            break;
        default:
            break;
    }
}

/* loongarch64 帧布局：$ra 在 8(sp)、$s0 在 0(sp)，槽区从 16(sp) 起 */
static void la_fn_prologue(NBuf *b, const TargetBackend *tb,
                           const IrFn *f, int frame)
{
    g_la_frame = frame;
    nb_put(b, "  addi.d $sp, $sp, -%d\n", frame);
    nb_put(b, "  st.d $ra, $sp, 8\n");
    nb_put(b, "  st.d $s0, $sp, 0\n");
    nb_put(b, "  addi.d $s0, $sp, %d\n", frame);
    /* 参数装载：int → $a0-$a7（st.d 槽），double → $fa0-$fa7（fst.d 槽） */
    int ii = 0, fi = 0;
    for (int i = 0; i < f->param_count; i++) {
        if (f->param_types[i] == 1) {
            if (fi < tb->fp_arg_count) {
                nb_put(b, "  fst.d %s, ", tb->fp_arg_regs[fi++]);
                la_slot(b, i, tb);
                nb_put(b, "\n");
            }
        } else {
            if (ii < tb->int_arg_count) {
                nb_put(b, "  st.d %s, ", tb->int_arg_regs[ii++]);
                la_slot(b, i, tb);
                nb_put(b, "\n");
            }
        }
    }
}

/* epilogue：显式 ret 已由 IR_RET 发射（含帧恢复）；
 * need_ret=1（隐式返回）时补完整返回序列 */
static void la_fn_epilogue(NBuf *b, const TargetBackend *tb,
                           const IrFn *f, int need_ret)
{
    (void)tb;
    if (need_ret) {
        if (f->ret_is_double) {
            /* $zero 的 64 位全零按 IEEE754 即 +0.0，经 movgr2fr.d 送进 $fa0 */
            nb_put(b, "  ori $a0, $zero, 0\n  movgr2fr.d $fa0, $a0\n");
        } else {
            nb_put(b, "  ori $a0, $zero, 0\n");
        }
        nb_put(b, "  ld.d $ra, $sp, 8\n  ld.d $s0, $sp, 0\n  addi.d $sp, $sp, %d\n  jirl $zero, $ra, 0\n",
               g_la_frame);
    }
}

static const char *la_int_arg_regs[] = { "$a0", "$a1", "$a2", "$a3", "$a4", "$a5", "$a6", "$a7", NULL };
static const char *la_fp_arg_regs[]  = { "$fa0", "$fa1", "$fa2", "$fa3", "$fa4", "$fa5", "$fa6", "$fa7", NULL };

const TargetBackend loongarch64_backend = {
    .name         = "loongarch64",
    .slot_stride  = 8,
    .stack_dir    = -1,
    .callee_align = 16,
    .frame_extra  = 16,           /* $ra + $s0 保存 */
    .max_frame    = 2047,         /* addi.d/ld.d/st.d 只用 12 位带符号立即数寻址（PB-35） */
    .int_arg_regs = la_int_arg_regs,
    .int_arg_count = 8,
    .fp_arg_regs  = la_fp_arg_regs,
    .fp_arg_count = 8,
    .ret_reg      = "$a0",
    .fp_ret_reg   = "$fa0",
    .fn_prologue  = la_fn_prologue,
    .emit_ins     = la_emit_ins,
    .fn_epilogue  = la_fn_epilogue,
    .expand       = NULL,
};
