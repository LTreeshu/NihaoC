/* ============================================================
 * ir.h - 三地址码 IR（方案 B：中间层）
 *
 * 设计原则（延续 TCC 单趟哲学）：
 *   - 无限虚拟寄存器（vreg），由后端负责映射
 *   - 每函数一个指令数组，label 为函数内编号
 *   - 调用参数通过连续 IR_PARAM 传递，后端负责对齐调用约定
 * ============================================================ */
#ifndef NIHAO_IR_H
#define NIHAO_IR_H

#include "ncc.h"

typedef enum {
    IR_NOP = 0,
    IR_CONST,       /* dst = imm                      */
    IR_MOV,         /* dst = a                        */
    IR_ADD, IR_SUB, IR_MUL, IR_DIV, IR_MOD,
    IR_SHL, IR_SHR, IR_AND, IR_OR,   /* 位运算（位域/位操作）：dst = a op b */
    IR_NEG,         /* dst = -a                       */
    IR_NOT,         /* dst = ~a（按位取反）            */
    IR_CMP_EQ, IR_CMP_NE, IR_CMP_LT, IR_CMP_LE,
    IR_CMP_GT, IR_CMP_GE,
    IR_FADD, IR_FSUB, IR_FMUL, IR_FDIV,   /* dst = a op b（double，xmm） */
    IR_FCMP,        /* dst = (a cmp b) double 比较；imm: 0=EQ 1=NE 2=LT 3=LE 4=GT 5=GE */
    IR_ITOD,        /* dst = (double)a（int64 → double 符号转换，混合类型提升） */
    IR_DTOI,        /* dst = (int64)a（double → int64 截断，配 ITOD 的反向转换） */
    IR_TRUNC,       /* dst = (intN)a 截断+符号扩展；imm: 0=i8 1=i16 2=i32 3=u8 4=u16 5=u32 */
    IR_JMP,         /* goto label                     */
    IR_JZ,          /* if dst == 0 goto label         */
    IR_JNZ,         /* if dst != 0 goto label         */
    IR_CALL,        /* dst = call fn(args via PARAM)  */
    IR_CALLI,       /* dst = call *(a)（间接调用，a 存函数地址 vreg） */
    IR_PARAM,       /* argument for the next CALL     */
    IR_RET,         /* return dst (-1 = bare return)  */
    IR_LABEL,
    IR_ALLOCA,      /* dst = address of a stack slot of size imm */
    IR_ADDR,        /* dst = address of local variable vreg a */
    IR_ELEM_ADDR,   /* dst = &base[idx]（元素地址，后端按自身布局方向） */
    IR_LOAD,        /* dst = *(a)                     */
    IR_STORE,       /* *(a) = b                       */
    IR_LD_ADDR,     /* dst = address of global sym    */
    IR_END
} IrOp;

typedef struct {
    IrOp op;
    int dst;            /* vreg 目标, -1 = 无 */
    int a, b;           /* vreg 源, -1 = 无 */
    int64_t imm;        /* 常量 / ALLOCA 大小 */
    int label;          /* 跳转目标 label */
    int fn;             /* CALL 目标函数号 */
    const char *sym;    /* LD_ADDR / 字符串字面量符号 */
} IrIns;

typedef struct {
    char *name;
    int is_main;
    int is_mr;          /* struct 返回（sret）：返回聚合值（隐藏 out-param 缓冲） */
    int param_count;
    int vreg_count;     /* 函数内使用的最大 vreg + 1 */
    int label_count;
    int ins_count;
    int ins_cap;
    IrIns *ins;
    int *vreg_type;     /* vreg -> 0=int 1=double（随 vreg_count 动态增长） */
    int ret_is_double;  /* 返回值为 f64/f32（PB-浮点 ABI） */
    int ret_agg_ti;     /* 返回聚合类型索引（struct 返回 = sret；-1=非聚合） */
    int param_types[32];/* 参数类型 0=int 1=double（param_count <= 32，与 pnames 对齐） */
    int param_agg_ti[32];/* 参数聚合类型（struct 参数按值展开；-1=标量；按展开后索引） */
} IrFn;

typedef struct {
    int fn_count;
    int fn_cap;
    IrFn *fns;
    /* 全局字符串池（字面量 -> 符号名） */
    int str_count;
    int str_cap;
    char **str_data;
    char **str_syms;
} IrProg;

IrProg *ir_prog_new(void);
IrFn *ir_fn_new(IrProg *p, const char *name, int is_main);
void ir_fn_end(IrFn *f);                 /* 追加 IR_END */
int ir_emit(IrFn *f, IrOp op, int dst, int a, int b, int64_t imm);
int ir_new_vreg(IrFn *f);
int ir_new_label(IrFn *f);
int ir_add_string(IrProg *p, const char *data);   /* 返回符号名序号 */

/* 后端入口（由 ncc 的 -backend=ir-c / ir-native 调用） */
int irgen_c_emit(IrProg *p, const char *outfile);        /* IR -> C */
int irgen_native_emit(IrProg *p, const char *outfile);   /* IR -> x86-64 asm */

#endif
