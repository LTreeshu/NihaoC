# NihaoC 阶段总结（2026-08-07）

> 会话恢复入口：读本文件 + `TODO.md` + `docs/BNF.md`。最后提交见 `git log -1`。

## 1. 双方案架构

| 方案 | 后端 | 说明 |
| ---- | ---- | ---- |
| **A 方案**（全量 transpiler） | `c`（→C 经 tcc）、`native`（→x86-64 AT&T 经 tcc） | parser.c 直接发 C 代码（cgen），native 走 tcc 内存编译 |
| **B 方案**（IR 中间层） | `ir-c`（→C）、`ir-native`（→x86-64） | irparse.c 生成三地址码（ir.h，30 条指令），ir_to_c / ir_to_native 双后端 |

- 统一入口 `-backend=`: 默认(c)/native/ir-c/ir-native；`xmake test --all` 四后端全矩阵
- 源码：`ncc/`（内部活跃仓库 `D:\workspace\workbudy\ncc`，分支 feat/backend-ir）+ `NihaoC/ncc/`（发布副本，随 GitHub 推送同步）
- **同步流程**：改 `NihaoC/ncc/` → tar 同步到 `D:\workspace\workbudy\ncc` → 两边各自 commit → NihaoC push（代理 127.0.0.1:7897）

## 2. A 方案（c/native）现状

**全量 parser 测试 16P+10S 通过**。近期修复：
- ✅ **关键字内置函数**（P1 关闭）：`parse_builtin_kw()` 分派 TOK_SIZEOF/TYPEOF/ALIGNOF/OFFSETOF/VISOF（原实现是 identifier 字符串比较，关键字 token 永远走不到）；可见性枚举 `_undef/_const/_flow/_static/_var` case；parse_is_stmt 可见性模式
- ✅ **语句边界行号**（重要）：函数体内无 NEWLINE token，`f()\n*p=1` 的 `*` 会被上句乘法吞掉 → binop 链 12 层全部传起始行号，换行即语句边界
- ✅ cgen 补 `#include <stddef.h>`（offsetof 宏）

**已知 A 方案缺口**（见 TODO P1）：前缀 `++x/--x`、数组初始化 `={1,2,3}`、switch/case、多变量声明（全量只支持带前缀形式，`{` 开头会当块语句）

## 3. B 方案（IR）现状

**测试：ir-c/ir-native 各 14P+12S 通过**。已完成 PB：

| PB | 功能 | 用例 |
| -- | ---- | ---- |
| PB-2 | 一元/复合运算、++/--、前缀 | ir_expr.nc、ir_prefix.nc |
| PB-3 | 数组：声明 N 槽、下标、初始化列表（剩余：切片/动态数组/类型宽度） | ir_array.nc |
| PB-4 | struct/union/enum：命名类型、成员访问、union 共享槽 0、初始化列表 | ir_struct.nc |
| PB-5 | 存储期 const/static/flow/var + visof（TOK_VISOF 关键字）+ is 可见性模式 | ir_vis.nc |
| PB-6 | 内置函数 sizeof/typeof/alignof/offsetof/malloc（类型大小表 + 外部 malloc 调用） | ir_builtin.nc |
| PB-7 | 控制流：for/break/continue/do/is 模式/switch | ir_switch.nc |
| PB-8 | 多变量声明 `var {a=0,b=1} i8`（&&/|| 短路层）、函数指针（IR_CALLI 间接调用）、multireturn 多返回值（隐藏 out-param 缓冲 + malloc） | ir_multi/fptr/mr.nc |
| PB-12 | IR_ADDR/LOAD/STORE 指针；IR_CALLI/IR_LD_ADDR | ir_ptr.nc |

**IR 数据模型**：全部值 8 字节 int64_t 槽（vreg）；ALLOCA 每槽独立 vreg，ADDR imm=k 直接引用槽 a+imm（不依赖 C 栈连续，ir-native 槽向下生长 slot=-8*(vreg+1)）；聚合=成员数槽。

**关键坑（反复踩过）**：
- **IrOp 枚举在 tcc 下底层 unsigned**——`op=-1` 哨兵判断 `op<0` 恒假 → 用布尔标志 is_compound
- 前向声明缺失 → x64 指针截断（ir_agg_decl 调 var_declare/ir_expr）
- `*p = 43` 中 ir_primary 赋值表达式块误吞 `=`（STAR 分支手动读标识符）
- 缓冲地址是"值"不是"槽"（multireturn 拷贝用 MOV 读值+ADD，勿用 ir_elem_addr）
- _mr_ret 隐藏参数必须注入 var 表头部（参数收集延迟注册）

## 4. 测试体系

```
xmake -r ncc          # 构建（NIHAO_TCC_DIR 指向 tcc）
xmake test --all      # 四后端全矩阵：c/native 16P、ir-c/ir-native 14P
```
- `tests/pos/*.nc` 全量用例（c/native）；`IR_ONLY` 集合 = IR 子集用例（ir-c/ir-native）；
  `IR_SUBSET` = 四后端通用
- 新用例须在 xmake.lua 注册 IR_ONLY 或 IR_SUBSET，否则自动当全量 target 编译失败
- A 方案指针用 `.()` 解引用语法（`p.(i32)=42`），IR 用 `*p`；指针声明用 `void` 类型

## 5. 下一步候选（见 TODO.md）

- PB-9 编译期（cooking/align/static_assert）
- PB-3 剩余（切片 `[a..b]`、动态数组 `[...]`）
- PB-13 浮点指令（f32/f64 + xmm ABI）
- PB-14 类型化内存访问（按类型宽度 load/store）
- A 方案 P1：switch/case、前缀 ++/--、数组初始化列表
- PA-9：-run/Linux 实测（无环境）
