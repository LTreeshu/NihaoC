# 历史遗留：早期 x86-64 直接代码生成器（codegen.c）

> **状态**：已删除（2026-09-01 清理，B 方案阶段 1 后）。本文档为设计思路归档，
> 供未来"直接机器码发射"类工作参考。删除 commit 见 NihaoC PB 分支 git 历史。

## 1. 背景与设计意图

NihaoC 早期（M0-M2 阶段）曾尝试 **parser → 直接生成本机 x86-64 机器码** 的
"直通后端"：parser.c 解析 AST 的同时调用 codegen.c 的发射器，把指令字节写入
Section 缓冲区，最后由 linker.c 打包成文件。这是当时"自研后端"路线的最初形态，
目标是不依赖外部工具链（tcc 尚未引入）独立产出可执行文件。

该路线最终被放弃，原因见 §6。codegen.c 在项目中保留了长期死代码状态，
直到 2026-09-01 归档后删除。

## 2. 管线形态（当时的调用链）

```
parser.c
  compile_file_full()            —— 旧编译入口（static，无人调用）
    ├─ codegen_init()            —— 初始化 Section/值栈/寄存器分配/relocs
    ├─ parse_function_full()     —— 函数级：序言 + 语句 + 尾声
    │    └─ gen_function_prologue_full / epilogue_full
    ├─ parse_statement_full()    —— 语句级分派
    │    ├─ gen_if_statement / gen_while_loop / gen_for_loop
    │    ├─ gen_do_while_loop / gen_return_statement
    └─ codegen_optimize()        —— 优化占位（no-op stub）
    └─ linker_generate_executable_full() / linker_generate_object()
         —— text/data/rodata Section → raw binary 文件
```

## 3. 架构组件（实现细节）

### 3.1 Section 模型

```c
typedef struct {
    unsigned char *data;      /* 字节缓冲，realloc 倍增（起始 4096） */
    int data_allocated;
    int data_size;
    char name[32];            /* ".text" / ".data" / ".bss" / ".rodata" */
    int sh_type;              /* 1=SHT_PROGBITS, 8=SHT_NOBITS */
    int sh_flags;             /* text=6 (ALLOC|EXEC), data=3 (ALLOC|WRITE), rodata=2 */
    int sh_addr;
    int sh_addralign;         /* 16 */
} Section;
```

四个标准段在 `codegen_init` 中创建；`cur_text_section` 指向当前代码段。
这是对标 ELF section 的最小骨架，仅保留了后续写 ELF 头所需的最基本字段。

### 3.2 字节发射器

```c
codegen_emit_byte(Section *sec, unsigned char b)   /* 单字节 */
codegen_emit_int32(Section *sec, int32_t val)      /* 小端 4 字节 */
```

所有机器码以硬编码字节序列方式逐字节写入（无汇编器、无指令编码抽象）。
典型发射序列（函数序言）：

```c
codegen_emit_byte(sec, 0x55);       /* push rbp */
codegen_emit_byte(sec, 0x48);       /* mov rbp, rsp */
codegen_emit_byte(sec, 0x89);
codegen_emit_byte(sec, 0xe5);
codegen_emit_byte(sec, 0x48);       /* sub rsp, imm8/imm32（按栈帧大小选择） */
codegen_emit_byte(sec, 0x83);       /*   0x83=imm8 短形式，0x81=imm32 长形式 */
codegen_emit_byte(sec, 0xec);
```

### 3.3 值栈（虚拟操作数栈）

```c
typedef struct {
    CType *type;
    unsigned short r;        /* 寄存器编号或 VT_CONST */
    union { int64_t i; double f; void *ptr; } val;
    int sym;                 /* 符号索引 */
} SValue;

CodeGenState:
    SValue *vstack;          /* 256 项，vtop 栈顶 */
```

参考 tcc 的 `vstack`/`SValue` 设计（tcc 的表达式求值基于值栈而非显式寄存器分配），
本实现预留了同样的数据结构但从未接入表达式求值。

### 3.4 寄存器分配雏形

```c
int reg_count;               /* = 8 */
int reg_alloc[8];            /* 简单分配器：每个寄存器一个占用标记 */
```

最简占用标记方案，无 live range / spill（对应后来 PB-15 决策中"全栈槽保底"
路线的对立面——本文件是"寄存器直分"路线的早期尝试，最终被全栈槽方案取代）。

### 3.5 Relocations（跳转补丁占位）

```c
int *relocs;                 /* 256 项容量 */
int reloc_count;
```

条件跳转发射时先写 `rel32 = 0` 占位，注释设想"二遍扫描或 relocation 系统"
回填真实偏移（见 §4.2）。同样未完成。

## 4. 指令模板（已实现的骨架）

### 4.1 函数序言 / 尾声

- **简单版**（gen_function_prologue/epilogue）：`push rbp; mov rbp,rsp; sub rsp,N; leave; ret`
  —— 栈帧大小按 `(local_count + 1) * 16` 对齐 16 字节，local_count 来自 Symbol。
- **完整版**（gen_function_prologue_full/epilogue_full）：增加 callee-saved
  寄存器保存（rbx, r12, r13——注释注明 r14/r15 简化未存）；
  栈帧 `local_count*8 + saved*8 + 8`；`sub rsp` 按大小选 imm8/imm32。

### 4.2 控制流（全部为 placeholder 结构）

| 结构 | 发射序列 | 状态 |
| ---- | ---- | ---- |
| if | `test eax,eax; jz rel32(0)`（条件值假定在 eax）| else/end label 偏移占位，未回填 |
| while | 同上 + 循环回跳注释掉 | 未完成 |
| for | 同上 | 未完成 |
| do-while | `test eax,eax; jnz rel32(0)` | 未完成 |
| return | `mov rsp,rbp; pop rbp; ret` | 简化版（未处理返回值） |

label 用全局计数器 `label_counter`/`new_label()` 编号，跳转偏移通过
"先写 0 占位 → 二遍扫描回填"模式补丁——注释明确写了该设想但未实现
（`/* placeholder: else_lbl offset */`）。gen_if/gen_while/gen_for 为纯 stub
（仅分配 label + verbose 打印）。

### 4.3 局限

- 控制流跳转偏移永远为 0（placeholder 未回填）→ 生成代码**不可正确执行**
- 表达式求值、条件计算未接入值栈 → 实际只发射了函数骨架
- linker 产物为 **raw binary 拼接**（text+rodata+data 顺序写入），
  非 ELF/PE 格式，无法被 OS 加载
- `codegen_optimize` 为 no-op stub（peephole/DCE/寄存器合并均未实现）

## 5. 与本项目最终路线的演进关系

| 路线 | 做法 | 与本文件关系 |
| ---- | ---- | ---- |
| **A 方案（1.0 产品线）** | parser → **C 文本** → tcc（cgen.c + native.c）| 取代：借 tcc 的 x86-64/arm64/riscv64 C 后端获得多平台，放弃自研字节发射 |
| **B 方案（2.0 演进线）** | irparse → **IR 中间层** → ir_to_c / ir_x86_64.c 等（汇编文本）| 取代：平台中立 IR + 每平台独立汇编生成（借鉴 tcc *-gen.c 与 LLVM TargetLowering）|

关键转折：**tcc 引入后**，A 方案获得"编译到 C 再交 tcc"的极简多平台能力，
自研本机后端（字节发射 + 链接器）不再必要；B 方案则选择 IR 中间层 + 汇编文本
（而非直接字节发射），把平台差异隔离在每后端 .c 中。

## 6. 潜在参考价值（未来若做直接机器码）

1. **字节发射 + placeholder 回填模式**：若未来 B 方案要做 **JIT**（在内存中
   直接生成机器码执行，类似 -run 的 tcc 内部机制），本文件的
   `codegen_emit_byte/emit_int32` + "占位 → 回填"跳转补丁流程是可复用骨架
   （ir_x86_64.c 当前输出汇编文本，需额外汇编步骤；JIT 需字节发射）。
2. **SValue 值栈 / reg_alloc 雏形**：若做寄存器分配（PB-15 长期项）或
   表达式栈式求值，本文件的数据结构提供了最简起点（tcc 同款思路）。
3. **Section 模型**：若实现 ELF/PE 输出（B 方案阶段 3 之后），
   最小 Section 骨架 + sh_type/sh_flags/addralign 字段可作起始参考。
4. **反面教材**：控制流与表达式求值未打通就先行发射字节，导致大量
   placeholder 悬空——教训是后端开发应先有完整指令选择与 label 回填设计，
   再动字节发射（B 方案 IR 层正是按此顺序：先 IR 指令集，后 per-backend 发射）。

## 7. 删除清单（2026-09-01）

| 文件 | 删除内容 |
| ---- | ---- |
| codegen.c | 整个文件（507 行）|
| parser.c | compile_file_full / parse_function_full / parse_statement_full（2440-2635）|
| linker.c | linker_generate_object / linker_generate_executable_full |
| ncc.h | SValue / Section / CodeGenState 结构、CompilerState.codegen 字段、全部 gen_*/codegen_* 声明、parse_*_full 声明、type_check_statement / verify_types 声明 |
| type.c | type_check_statement（placeholder 空函数）|
| vis.c | verify_types（类型一致性 pass，仅旧入口调用）|
| ncc.c | compile_file 中的 codegen_init(cs) 遗留调用 |
| xmake.lua | add_files 移除 codegen.c |

保留：linker_init / link_add_library（`link` 指令解析，活跃）；g_cs（IR/native 层使用）。
