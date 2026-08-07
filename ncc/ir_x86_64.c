#include "ir_backend.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ============================================================
 * x86-64 后端（阶段 1：从 ir_to_native.c 迁移，符合 TargetBackend 接口）
 *
 * 约定：
 *  - 全栈槽：vreg N 在 rbp-stride*(N+1)（槽向下生长）
 *  - 调用约定：Windows x64 简化版（rcx/rdx/r8/r9 + 影子空间 32）
 *  - 浮点：x87 浮点栈（tcc 汇编器不支持 SSE movsd）
 * ============================================================ */

/* vreg -> 栈槽地址（-stride*(vreg+1)） */
static void x64_slot(NBuf *b, int vreg, const TargetBackend *tb)
{
    nb_put(b, "-%d(%%rbp)", tb->slot_stride * (vreg + 1));
}

static const char *setcc_ins(IrOp op)
{
    switch (op) {
        case IR_CMP_EQ: return "sete";
        case IR_CMP_NE: return "setne";
        case IR_CMP_LT: return "setl";
        case IR_CMP_LE: return "setle";
        case IR_CMP_GT: return "setg";
        case IR_CMP_GE: return "setge";
        default:        return "sete";
    }
}

static void x64_emit_ins(NBuf *b, const IrIns *in, const TargetBackend *tb,
                         const IrProg *p, const IrFn *f, int i)
{
    (void)f; (void)i;
    switch (in->op) {
        case IR_CONST:
            /* imm 可能 >32 位（double 位模式）→ imm64 先到寄存器 */
            nb_put(b, "  movq $%lld, %%rax\n  movq %%rax, ",
                   (long long)in->imm);
            x64_slot(b, in->dst, tb);
            nb_put(b, "\n");
            break;
        case IR_MOV:
            nb_put(b, "  movq ");
            x64_slot(b, in->a, tb);
            nb_put(b, ", %%rax\n  movq %%rax, ");
            x64_slot(b, in->dst, tb);
            nb_put(b, "\n");
            break;
        case IR_ADD: case IR_SUB:
            nb_put(b, "  movq ");
            x64_slot(b, in->a, tb);
            nb_put(b, ", %%rax\n  %sq ", in->op == IR_ADD ? "add" : "sub");
            x64_slot(b, in->b, tb);
            nb_put(b, ", %%rax\n  movq %%rax, ");
            x64_slot(b, in->dst, tb);
            nb_put(b, "\n");
            break;
        case IR_FADD: case IR_FSUB: case IR_FMUL: case IR_FDIV:
            /* x87 浮点栈（tcc 汇编器不支持 SSE movsd）。
             * fsubp/fdivp 语义 st0 = st0 op st1（有方向），
             * 需 st0=a → 先压 b 再压 a；faddp/fmulp 交换律无顺序。 */
            if (in->op == IR_FSUB || in->op == IR_FDIV) {
                nb_put(b, "  fldl ");
                x64_slot(b, in->b, tb);
                nb_put(b, "\n  fldl ");
                x64_slot(b, in->a, tb);
            } else {
                nb_put(b, "  fldl ");
                x64_slot(b, in->a, tb);
                nb_put(b, "\n  fldl ");
                x64_slot(b, in->b, tb);
            }
            nb_put(b, "\n  %sp %%st, %%st(1)\n  fstpl ",
                   in->op == IR_FADD ? "fadd" :
                   in->op == IR_FSUB ? "fsub" :
                   in->op == IR_FMUL ? "fmul" : "fdiv");
            x64_slot(b, in->dst, tb);
            nb_put(b, "\n");
            break;
        case IR_FCMP:
            /* 栈序：fldl b; fldl a → st0=a, st1=b；
             * fcomip %st(1)：比较 st0(a) 与 st1(b)，CF=1 iff a<b，弹出 st0 */
            nb_put(b, "  fldl ");
            x64_slot(b, in->b, tb);
            nb_put(b, "\n  fldl ");
            x64_slot(b, in->a, tb);
            nb_put(b, "\n  fcomip %%st(1), %%st\n  fstp %%st\n  ");
            nb_put(b, in->imm == 0 ? "sete" : in->imm == 1 ? "setne" :
                      in->imm == 2 ? "setb" : in->imm == 3 ? "setbe" :
                      in->imm == 4 ? "seta" : "setae");
            nb_put(b, " %%al\n  movzbq %%al, %%rax\n  movq %%rax, ");
            x64_slot(b, in->dst, tb);
            nb_put(b, "\n");
            break;
        case IR_MUL:
            nb_put(b, "  movq ");
            x64_slot(b, in->a, tb);
            nb_put(b, ", %%rax\n  imulq ");
            x64_slot(b, in->b, tb);
            nb_put(b, ", %%rax\n  movq %%rax, ");
            x64_slot(b, in->dst, tb);
            nb_put(b, "\n");
            break;
        case IR_DIV: case IR_MOD:
            nb_put(b, "  movq ");
            x64_slot(b, in->a, tb);
            nb_put(b, ", %%rax\n  cqto\n  idivq ");
            x64_slot(b, in->b, tb);
            nb_put(b, "\n");
            nb_put(b, in->op == IR_DIV ? "  movq %%rax, " : "  movq %%rdx, ");
            x64_slot(b, in->dst, tb);
            nb_put(b, "\n");
            break;
        case IR_NEG:
            nb_put(b, "  movq ");
            x64_slot(b, in->a, tb);
            nb_put(b, ", %%rax\n  negq %%rax\n  movq %%rax, ");
            x64_slot(b, in->dst, tb);
            nb_put(b, "\n");
            break;
        case IR_NOT:
            nb_put(b, "  movq ");
            x64_slot(b, in->a, tb);
            nb_put(b, ", %%rax\n  notq %%rax\n  movq %%rax, ");
            x64_slot(b, in->dst, tb);
            nb_put(b, "\n");
            break;
        case IR_CMP_EQ: case IR_CMP_NE: case IR_CMP_LT:
        case IR_CMP_LE: case IR_CMP_GT: case IR_CMP_GE:
            nb_put(b, "  movq ");
            x64_slot(b, in->a, tb);
            nb_put(b, ", %%rax\n  cmpq ");
            x64_slot(b, in->b, tb);
            nb_put(b, ", %%rax\n  %s %%al\n  movzbq %%al, %%rax\n  movq %%rax, ",
                   setcc_ins(in->op));
            x64_slot(b, in->dst, tb);
            nb_put(b, "\n");
            break;
        case IR_JMP:
            nb_put(b, "  jmp .L%d\n", (int)in->label);
            break;
        case IR_JZ: case IR_JNZ:
            nb_put(b, "  cmpq $0, ");
            x64_slot(b, in->a, tb);
            nb_put(b, "\n  %s .L%d\n",
                   in->op == IR_JZ ? "je" : "jne", (int)in->label);
            break;
        case IR_LABEL:
            nb_put(b, ".L%d:\n", (int)in->label);
            break;
        case IR_LD_ADDR:
            nb_put(b, "  leaq %s(%%rip), %%rax\n  movq %%rax, ", in->sym);
            x64_slot(b, in->dst, tb);
            nb_put(b, "\n");
            break;
        case IR_ADDR:
            /* dst = 槽 a+imm 的地址（槽向下生长 -8,-16,...，imm=槽号偏移） */
            nb_put(b, "  leaq ");
            x64_slot(b, in->a + (int)in->imm, tb);
            nb_put(b, ", %%rax\n  movq %%rax, ");
            x64_slot(b, in->dst, tb);
            nb_put(b, "\n");
            break;
        case IR_ELEM_ADDR:
            /* 元素地址 = 基址 - idx*8（槽向下生长：slot(a+k) = slot(a) - 8k） */
            nb_put(b, "  movq ");
            x64_slot(b, in->a, tb);
            nb_put(b, ", %%rax\n  movq ");
            x64_slot(b, in->b, tb);
            nb_put(b, ", %%rcx\n  shlq $3, %%rcx\n  subq %%rcx, %%rax\n  movq %%rax, ");
            x64_slot(b, in->dst, tb);
            nb_put(b, "\n");
            break;
        case IR_LOAD:
            nb_put(b, "  movq ");
            x64_slot(b, in->a, tb);
            nb_put(b, ", %%rax\n  movq (%%rax), %%rax\n  movq %%rax, ");
            x64_slot(b, in->dst, tb);
            nb_put(b, "\n");
            break;
        case IR_STORE:
            nb_put(b, "  movq ");
            x64_slot(b, in->a, tb);
            nb_put(b, ", %%rax\n  movq ");
            x64_slot(b, in->b, tb);
            nb_put(b, ", %%rcx\n  movq %%rcx, (%%rax)\n");
            break;
        case IR_CALL: case IR_CALLI: {
            /* 取本 CALL 前最近的 nargs 个 PARAM */
            int nargs = (int)in->imm;
            int temp[64];
            int tc = 0;
            for (int k = 0; k < i && tc < 64; k++) {
                if (f->ins[k].op == IR_PARAM) temp[tc++] = f->ins[k].a;
            }
            int start = tc > nargs ? tc - nargs : 0;
            int got = tc - start;
            const char *regs[4] = { "%rcx", "%rdx", "%r8", "%r9" };
            for (int k = 0; k < got && k < 4; k++) {
                nb_put(b, "  movq ");
                x64_slot(b, temp[start + k], tb);
                nb_put(b, ", %s\n", regs[k]);
            }
            if (got > 4) {
                /* 多余参数压栈（从右到左） */
                for (int k = got - 1; k >= 4; k--) {
                    nb_put(b, "  pushq ");
                    x64_slot(b, temp[start + k], tb);
                    nb_put(b, "\n");
                }
            }
            nb_put(b, "  subq $32, %%rsp\n");   /* shadow space */
            if (in->op == IR_CALL) {
                /* 用户函数直接 call；外部符号（puts 等 DLL 导入）经 __imp_ 间接调用 */
                int is_user_fn = 0;
                for (int k = 0; k < p->fn_count; k++) {
                    if (p->fns[k].name && in->sym &&
                        strcmp(p->fns[k].name, in->sym) == 0) {
                        is_user_fn = 1;
                        break;
                    }
                }
                if (is_user_fn) {
                    nb_put(b, "  call %s\n", in->sym);
                } else {
#ifdef _WIN32
                    nb_put(b, "  call *__imp_%s(%%rip)\n", in->sym);
#else
                    nb_put(b, "  call %s\n", in->sym);
#endif
                }
            } else {
                /* 间接调用：call *%rax */
                nb_put(b, "  movq ");
                x64_slot(b, in->a, tb);
                nb_put(b, ", %%rax\n  call *%%rax\n");
            }
            nb_put(b, "  addq $%d, %%rsp\n", 32 + (got > 4 ? 8 * (got - 4) : 0));
            nb_put(b, "  movq %%rax, ");
            x64_slot(b, in->dst, tb);
            nb_put(b, "\n");
            break;
        }
        case IR_RET:
            if (in->a >= 0) {
                nb_put(b, "  movq ");
                x64_slot(b, in->a, tb);
                nb_put(b, ", %%rax\n");
            } else {
                nb_put(b, "  xorl %%eax, %%eax\n");
            }
            nb_put(b, "  leave\n  ret\n");
            break;
        default:
            break;
    }
}

/* 函数序言：帧设置 + 参数装载（rcx/rdx/r8/r9 → vreg 0..nparam-1） */
static void x64_fn_prologue(NBuf *b, const TargetBackend *tb,
                            const IrFn *f, int frame)
{
    nb_put(b, "  pushq %%rbp\n");
    nb_put(b, "  movq %%rsp, %%rbp\n");
    nb_put(b, "  subq $%d, %%rsp\n", frame);
    for (int i = 0; i < f->param_count && i < tb->int_arg_count; i++) {
        nb_put(b, "  movq %s, ", tb->int_arg_regs[i]);
        x64_slot(b, i, tb);
        nb_put(b, "\n");
    }
}

/* 函数尾声：need_ret=1 时补隐式返回 */
static void x64_fn_epilogue(NBuf *b, const TargetBackend *tb,
                            const IrFn *f, int need_ret)
{
    (void)tb; (void)f;
    if (need_ret) {
        nb_put(b, "  xorl %%eax, %%eax\n");
        nb_put(b, "  leave\n  ret\n");
    }
}

static const char *x64_int_arg_regs[] = { "%rcx", "%rdx", "%r8", "%r9", NULL };
static const char *x64_fp_arg_regs[]  = { "%xmm0", "%xmm1", "%xmm2", "%xmm3", NULL };

const TargetBackend x86_64_backend = {
    .name         = "x86-64",
    .asm_syntax   = "att",
    .slot_stride  = 8,
    .stack_dir    = -1,
    .callee_align = 16,
    .frame_extra  = 0,            /* 影子空间在 CALL 时动态 sub，非帧内 */
    .int_arg_regs = x64_int_arg_regs,
    .int_arg_count = 4,
    .fp_arg_regs  = x64_fp_arg_regs,
    .fp_arg_count = 4,
    .ret_reg      = "%rax",
    .fp_ret_reg   = "%xmm0",
    .fn_prologue  = x64_fn_prologue,
    .emit_ins     = x64_emit_ins,
    .fn_epilogue  = x64_fn_epilogue,
    .expand       = NULL,
};

/* 兼容入口（ncc.c 现有调用） */
int irgen_native_emit(IrProg *p, const char *outfile)
{
    return irgen_backend_emit(p, outfile, "x86-64");
}
