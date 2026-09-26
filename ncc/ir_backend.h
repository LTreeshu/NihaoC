#ifndef IR_BACKEND_H
#define IR_BACKEND_H

#include "ncc.h"
#include "ir.h"

/* ============================================================
 * 目标后端抽象接口（方案 B 多平台设计，阶段 1）
 *
 * 借鉴：
 *  - LLVM TargetLowering：调用约定/参数寄存器抽象为接口字段
 *  - tcc *-gen.c：每平台一个后端文件做指令选择 + 发射
 *
 * IR（ir.h 三地址码）保持平台无关语义；栈帧布局、调用约定、
 * 指令选择、浮点策略全部由 TargetBackend 实现负责。
 *
 * 注意：函数指针类型必须 typedef 到结构体外（tcc 0.9.27 对
 * 结构体内联函数指针声明解析有 bug）。
 * ============================================================ */

/* 汇编输出缓冲（后端共享） */
typedef struct {
    char *buf;
    size_t len, cap;
} NBuf;

void nb_init(NBuf *b);
void nb_put(NBuf *b, const char *fmt, ...);

typedef struct TargetBackend TargetBackend;

typedef void (*FnPrologue)(NBuf *b, const struct TargetBackend *tb,
                           const IrFn *f, int frame);
typedef void (*FnEmitIns)(NBuf *b, const IrIns *in, const struct TargetBackend *tb,
                          const IrProg *p, const IrFn *f, int i);
typedef void (*FnEpilogue)(NBuf *b, const struct TargetBackend *tb,
                           const IrFn *f, int need_ret);

struct TargetBackend {
    const char *name;         /* "riscv64" | "loongarch64"（IR 线只留这两档） */

    /* ---- 栈帧布局 ---- */
    int slot_stride;          /* 槽间距（字节）：8 */
    int stack_dir;            /* -1 向下（riscv64/loongarch64 均向下） */
    int callee_align;         /* 栈对齐：16 */
    int frame_extra;          /* 额外帧开销（两档都是 ra + fp 各 8 字节） */
    /* 单帧可寻址上限（字节）。两档发射器都用「帧指针 ±12 位立即数」寻址槽位，
     * 超界会发出发汇编器拒绝的指令；0 = 不限（发射器自己能处理超界偏移）。 */
    int max_frame;

    /* ---- 调用约定（参数寄存器表，超出走栈） ---- */
    const char **int_arg_regs;   /* riscv "a0"… / loongarch "$a0"… */
    int int_arg_count;
    const char **fp_arg_regs;    /* riscv "fa0"… / loongarch "$fa0"… */
    int fp_arg_count;
    const char *ret_reg;         /* "a0" / "$a0" */
    const char *fp_ret_reg;      /* "fa0" / "$fa0" */

    /* ---- 指令发射钩子 ---- */
    FnPrologue fn_prologue;      /* 函数序言（帧 + 参数装载） */
    FnEmitIns  emit_ins;         /* 每条 IR 指令发射 */
    FnEpilogue fn_epilogue;      /* 函数尾声（need_ret=1 补隐式返回） */
    FnEmitIns  expand;           /* 缺失指令 expand（NULL=不支持） */
};

/* 后端注册表 */
const TargetBackend *backend_find(const char *name);
int backend_count(void);
const char *backend_name(int idx);

/* 统一发射入口（骨架：字符串池/函数循环/帧计算/三钩子调度） */
int irgen_backend_emit(IrProg *p, const char *outfile, const char *backend);

/* 各后端实例（定义于各自 .c） */
extern const TargetBackend riscv64_backend;      /* 阶段 3 */
extern const TargetBackend loongarch64_backend;  /* 阶段 4 */

#endif /* IR_BACKEND_H */
