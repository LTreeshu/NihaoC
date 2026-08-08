# NihaoC 阶段总结（2026-08-08）

> 会话恢复入口：读本文件 + `TODO.md` + `docs/BNF.md`。最后提交见 `git log -1`。

## 1. 双方案架构

| 方案 | 后端 | 说明 |
| ---- | ---- | ---- |
| **A 方案**（全量 transpiler） | `c`（→C 经 tcc）、`native`（→x86-64 AT&T 经 tcc） | parser.c 直接发 C 代码（cgen） |
| **B 方案**（IR 中间层） | `ir-c`（→C）、`ir-native`（→x86-64 Win64）、`ir-riscv64`、`ir-arm64` | irparse.c 生成三地址码（ir.h），多后端发射 |

- 统一入口 `-backend=`：默认(c)/native/ir-c/ir-native/ir-riscv64/ir-arm64；`xmake test --all` 全矩阵
- 源码：`ncc/`（内部活跃仓库 `D:\workspace\workbudy\ncc`，分支 feat/backend-ir）+ `NihaoC/ncc/`（发布副本，随 GitHub 推送同步）
- **同步流程**：改 `NihaoC/ncc/` → tar 同步到 `D:\workspace\workbudy\ncc` → 两边各自 commit（推送由用户决定）

## 2. A 方案（c/native）现状

**全量 parser 测试 12P+19S 通过，0 FAIL**。已修复（历史）：
- 关键字内置函数（TOK_SIZEOF/TYPEOF/ALIGNOF/OFFSETOF/VISOF 分派）、可见性枚举、parse_is_stmt
- 语句边界行号（binop 链 12 层传起始行号，换行即语句边界）
- cgen 补 `#include <stddef.h>`（offsetof）
- 前缀 `++x/--x`、数组初始化列表、switch/case（P0 三项已闭合，ir_loop/ir_switch 四后端通用）

**已知 A 方案缺口**（见 TODO）：struct 返回生成 C bug（`type__compound` 未定义）、函数指针声明生成 bug、link 导入未实现、cooking 顶层块（parser.c 注释掉）。

## 3. B 方案（IR）现状

**约 20 个 IR 用例全过，0 FAIL**。已完成的 PB：

| PB | 功能 | 用例 |
| -- | ---- | ---- |
| PB-1 | 类型系统：窄整数宽度（IR_TRUNC）、数组元素宽度（vetyp）、双向转换（IR_DTOI/ITOD + ir_coerce）、char[]/字符串 | ir_narrow/narray/conv/str.nc |
| PB-2 | 一元/复合运算、++/--、前缀 | ir_expr.nc、ir_prefix.nc |
| PB-3/4 | 数组、struct/union/enum、初始化列表 | ir_array/struct.nc |
| PB-5/6 | 存储期 visof、内置函数 sizeof/malloc 等 | ir_vis/builtin.nc |
| PB-7/8 | 控制流 switch/is、多变量、函数指针、struct 返回值（sret） | ir_switch/multi/fptr/mr.nc |
| PB-9 | 编译期 cooking（static_assert + **编译期变量表 ct_vars**，运行时引用折叠） | ir_cook.nc |
| PB-12/13 | 指针 IR_ADDR/LOAD/STORE、浮点（x87 f64 运算 + **调用 ABI**：xmm/d0/fa0 参数、IR_ITOD/DTOI） | ir_ptr/float/fcall/conv.nc |
| PB-16 | 调用约定：struct 参数按值展开（param_agg_ti）、return p 聚合返回（sret） | ir_sparam.nc |
| PB-17/18 | 嵌套调用（收集模式）、ir_to_c 类型化输出（0 warning） | — |
| PB-20 | **arm64 后端 ir_arm64.c（AAPCS64）**，riscv64 后端（RV64I+D） | 三架构汇编验证 |

**IR 数据模型**：8 字节槽（vreg），vreg_type 标记 double；变量 vtype 编码（0=i64 1=float 2=i8 3=i16 4=i32 5=u8 6=u16 7=u32）；聚合=成员数槽；ir_coerce 统一目标类型协调（ITOD/DTOI/TRUNC）。

**关键坑（反复踩过）**：
- IrOp 枚举在 tcc 下底层 unsigned——`op=-1` 哨兵判断 `op<0` 恒假 → 用布尔标志
- x87：fsubp/fdivp 有方向需先压 b 再压 a；fistpq 默认最近舍入（3.7→4）→ fstcw/fldcw 切 RC=向零（0x0C00），用 rsp 动态让出临时区
- ir_to_c LOAD/STORE 须类型感知（double → `*(double*)`，硬编码 int64_t 会截断位模式）
- 一元负 double 不能发整数 IR_NEG（毁位模式）→ FSUB 0.0-a
- 多函数同一汇编文件 label 重名 → ir_backend 发射前加跨函数 lbl_base
- sret 缓冲地址是"值"不是"槽"；_mr_ret 注入 var 表头部
- ir_primary 的 IDENTIFIER 分支先 next_tok 消费——后续分支（ct_var 等）勿再 next_tok（双重消费吞 token）

## 4. 测试体系

```
xmake -r ncc          # 构建（NIHAO_TCC_DIR 指向 tcc）
xmake test --all      # 全矩阵：c/native 12P、ir 后端 ~20 用例，0 FAIL
```
- `tests/pos/*.nc` 全量用例（c/native）；`IR_ONLY` = IR 子集（ir 后端）；`IR_SUBSET` = 四后端通用（一致性检查：ir-c 与 ir-native 输出必须一致）
- 新用例须在 xmake.lua 注册 IR_ONLY 或 IR_SUBSET，否则自动当全量 target 编译失败
- A 方案指针用 `.()` 解引用语法（`p.(i32)=42`），IR 用 `*p`；riscv64/arm64 后端只验汇编生成

## 5. 下一步候选（见 TODO.md）

- PB-3 剩余：切片 `[a..b]`、动态数组 `[...]`（IR 与全量）
- PB-9 剩余：编译期函数调用（cooking-call 执行）
- x86-64 SysV 变体（验汇编）、loongarch 后端
- A 方案：struct 返回 cgen bug、函数指针 cgen bug
- PA-9：-run/Linux 实测（无环境）
- 文档：Chinese.md/English.md 示例已核对符合 BNF v2.0（含规划特性）
