# NihaoC 阶段总结（2026-08-20）

> 会话恢复入口：读本文件 + `TODO.md` + `docs/BNF.md`。最后提交见 `git log -1`。

## 1. 双方案架构

| 方案 | 后端 | 说明 |
| ---- | ---- | ---- |
| **A 方案**（全量 transpiler） | `c`（→C 经 tcc）、`native`（→x86-64 AT&T 经 tcc） | parser.c 直接发 C 代码（cgen） |
| **B 方案**（IR 中间层） | `ir-c`（→C）、`ir-native`（→x86-64 Win64）、`ir-riscv64`、`ir-arm64`、**`ir-loongarch64`** | irparse.c 生成三地址码（ir.h），多后端发射 |

- 统一入口 `-backend=`：默认(c)/native/ir-c/ir-native/ir-riscv64/ir-arm64/ir-loongarch64；`xmake test --all` 全矩阵
- **四目标架构收官（2026-08-15）**：x86-64 / riscv64 / arm64 / loongarch64 全部经 TargetBackend 抽象（ir_backend.c 注册表），后三者只验汇编生成
- 源码：`ncc/`（内部活跃仓库 `D:\workspace\workbudy\ncc`，分支 feat/backend-ir）+ `NihaoC/ncc/`（发布副本，随 GitHub 推送同步）
- **同步流程**：改 `NihaoC/ncc/` → tar 同步到 `D:\workspace\workbudy\ncc` → 两边各自 commit（推送由用户决定；2026-08-18 起 credential.helper=wincred 可直推）

## 2. A 方案（c/native）现状

**全量 parser 测试 12P+4S 通过，0 FAIL**。已修复（历史）：
- 关键字内置函数（TOK_SIZEOF/TYPEOF/ALIGNOF/OFFSETOF/VISOF 分派）、可见性枚举、parse_is_stmt
- 语句边界行号（binop 链 12 层传起始行号，换行即语句边界）
- cgen 补 `#include <stddef.h>`（offsetof）
- 前缀 `++x/--x`、数组初始化列表、switch/case（ir_loop/ir_switch 四后端通用）
- **goto（2026-08-19）**：`goto name` + 标签 `name:`（C 风格，peek 检测），ir_goto.nc 四后端
- struct 返回生成 C bug、函数指针声明生成 bug、cooking 编译期（A 方案）

**已知 A 方案缺口**（见 TODO）：无（link 导入解析闭环；实际 -l 传 tcc 为规划特性）。

## 3. B 方案（IR）现状

**IR 用例 30 个（IR_SUBSET 25 + IR_ONLY 5），全矩阵 0 FAIL**。已完成的 PB：

| PB | 功能 | 用例 |
| -- | ---- | ---- |
| PB-1 | 类型系统：窄整数宽度（IR_TRUNC）、数组元素宽度（vetyp）、双向转换（IR_DTOI/ITOD + ir_coerce）、char[]/字符串（IR_LOAD8/STORE8 字节读写） | ir_narrow/narray/conv/str.nc |
| PB-2 | 一元/复合运算、++/--、前缀 | ir_expr.nc、ir_prefix.nc |
| PB-3 | 数组、切片（读/写/长度语义）、动态数组；**len()（8/19）** | ir_array/slice.nc |
| PB-4 | struct/union/enum、初始化列表、**嵌套 struct（8/19：链式访问+递归偏移 mslots）**、**整体赋值拷贝（8/19）**、**嵌套初始化列表（8/26：IR ir_agg_init 递归 + 全量 parse_init_list）** | ir_struct/nested.nc |
| PB-5/6 | 存储期 visof、内置函数 sizeof/malloc 等 | ir_vis/builtin.nc |
| PB-7/8 | 控制流 switch/is、多变量、函数指针（IR_CALLI）、struct 返回值（sret） | ir_switch/multi/fptr/mr.nc |
| PB-9 | 编译期 cooking：static_assert + 编译期变量表 + **编译期函数 cooking-call（8/19：宏式展开，临时 lexer 求值）** | ir_cook.nc |
| PB-12/13 | 指针 IR_ADDR/LOAD/STORE、**`.()` 解引用（8/18）**、浮点（x87 f64 + 调用 ABI）、**f32 严格宽度（8/20：IR_FTRUNC 存储截断）** | ir_ptr/float/fcall/conv.nc |
| PB-16 | 调用约定：struct 参数按值展开、return p 聚合返回（sret） | ir_sparam.nc |
| PB-17/18 | 嵌套调用（收集模式）、ir_to_c 类型化输出（0 warning） | — |
| PB-20 | **arm64 / riscv64 / loongarch64 后端**（四架构收官） | 汇编验证 |
| PB-23 | **CLI debug --ir（8/18：dump 三地址码 + 44 条指令名表）** | — |

**回归扩容（2026-08-13/14/18/19）**：IR_SUBSET 9→25（批量对比发现法修复 3 个全量真 bug：跨行后缀 ++ 误吃 / multi-decl init 残留 / 动态字符串池寻址）。

**IR 数据模型**：8 字节槽（vreg），vreg_type 标记 double；变量 vtype 编码（0=i64 1=double 2=i8 3=i16 4=i32 5=u8 6=u16 7=u32 8=f32）；聚合=成员槽（嵌套递归 mslots）；ir_coerce 统一目标类型协调（ITOD/DTOI/TRUNC/FTRUNC）。

**关键坑（反复踩过）**：
- IrOp 枚举在 tcc 下底层 unsigned——`op=-1` 哨兵判断 `op<0` 恒假 → 用布尔标志
- x87：fsubp/fdivp 有方向需先压 b 再压 a；fistpq 默认最近舍入（3.7→4）→ fstcw/fldcw 切 RC=向零（0x0C00），用 rsp 动态让出临时区
- ir_to_c LOAD/STORE 须类型感知（double → `*(double*)`，硬编码 int64_t 会截断位模式）
- **指针值寻址（动态字符串/.()/is_ptr）用 MOV 勿 LOAD**（LOAD 多解引用一次 segfault）
- **ELEM_ADDR 是后端栈布局方向（x86 向下 subq）；池内存向上布局用 IR_ADD 向前**
- **goto label 独立编号空间（IR_GOTO_LABEL_BASE 1000）**——与 ir_new_label 冲突 duplicate label
- 一元负 double 不能发整数 IR_NEG（毁位模式）→ FSUB 0.0-a
- 多函数同一汇编文件 label 重名 → ir_backend 发射前加跨函数 lbl_base
- sret 缓冲地址是"值"不是"槽"；_mr_ret 注入 var 表头部
- ir_primary 的 IDENTIFIER 分支先 next_tok 消费——后续分支（ct_var 等）勿再 next_tok（双重消费吞 token）

## 4. 测试体系

```
xmake -r ncc          # 构建（NIHAO_TCC_DIR 指向 tcc）
xmake test --all      # 全矩阵：c/native 12P、IR 一致性 25P，0 FAIL
```
- `tests/pos/*.nc` 全量用例（c/native）；`IR_ONLY` = IR 子集（ir 后端）；`IR_SUBSET` = 四后端通用（一致性检查）
- 新用例须在 xmake.lua 注册 IR_ONLY 或 IR_SUBSET，否则自动当全量 target 编译失败
- 指针解引用：A 方案与 IR 统一用 `.()` 语法（`p.() = 42` / `y i32 = p.()`）；riscv64/arm64/loongarch64 只验汇编生成

## 5. 下一步候选（见 TODO.md）

- PB-3 剩余：动态数组增长、切片运行时边界长度
- 数据模型剩余：无（2026-08-27 union 嵌套完成，全部清零）
- IR 语法覆盖剩余：命名空间、部分 is 模式
- **PB-24 统一管线决策**（parser→IR 单管线 vs 双管线并存）
- A 方案：link 导入实际 -l 传递
- PA-9：-run/Linux 实测（无环境）
- 文档：Chinese.md/English.md 已核对 BNF v2.0；8/19-8/20 新语法（goto label/len/cooking 函数/嵌套/f32）待补
