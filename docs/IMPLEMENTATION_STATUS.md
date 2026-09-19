# NihaoC 规范与编译器实现对应关系

本文档记录 NihaoC 语言规范（Chinese.md / English.md）中各项指针安全规则与编译器（ncc）实际实现的对应状态。

> 更新日期：2026-09-19。本表主体以 **1.0 冻结线（v1.0.2）** 的现态书写，行号基于该时点 `PA`/`main` 的 `parser.c` / `vis.c` / `irparse.c`，仅作导航用。
> **PB（2.0 线）差异集中在文末「2.0 线（PB）差异」一节**：`vis.c` 的矩阵实现两侧同源（行号一致），`parser.c` / `irparse.c` 行号与部分状态不同。

---

## 指针传递矩阵（§12.1）

| 规范要求 | 编译器实现 | 对应代码 | 状态 |
|---------|----------|---------|------|
| 4×4 传递矩阵 | `vis_check_transfer()` | vis.c:70–86 | ✅ 已实现 |
| 源状态变更（冻结/失效/保持） | `vis_update_source()` | vis.c:89–113 | ✅ 已实现 |
| 仅指针类型受约束 | `vis_is_pointer_type()` 守卫 | vis.c:55–60 | ✅ 已实现 |
| 借用作用域释放 | `vis_unfreeze_borrows()` | vis.c:117–124 | ✅ 已实现 |
| 失效变量不可读 | `vis_check_usable()` | vis.c:127–136 | ✅ 已实现 |
| 冻结变量不可写 | `vis_check_writable()` | vis.c:139–148 | ✅ 已实现 |
| 赋值检查编排 | `vis_check_assign()` | vis.c:154–198 | ✅ 已实现 |

---

## 函数参数传递（§12.2）

| 规范要求 | 编译器实现 | 对应代码 | 状态 |
|---------|----------|---------|------|
| `flow` 参数接收所有权 | 参数前缀被忽略 | parser.c:864–868、879 | ⚠️ 未实现 |
| `var` 参数冻结实参 | 参数前缀被忽略 | parser.c:864–868、879 | ⚠️ 未实现 |
| `const` 参数冻结实参 | 参数前缀被忽略 | parser.c:864–868、879 | ⚠️ 未实现 |

> 当前编译器解析函数参数上的属性前缀（`flow`/`var`/`const`），但在内部将所有参数统一视为 `var`（`VIS_DEFAULT`）。参数的所有权/借用检查不在 1.0 范围内，已由 2.0（PB 分支 PB-26）实现并验证，1.0 冻结线不移植。

---

## 检查范围

| 规范要求 | 编译器实现 | 对应代码 | 状态 |
|---------|----------|---------|------|
| 裸标识符赋值检查 | 完整实现 | parser.c:1024–1034（声明初始化）、2432–2443（表达式赋值） | ✅ 已实现 |
| 表达式级检查 | 仅检查裸标识符 | parser.c:1024–1034、2432–2443 | ⚠️ 部分实现 |
| IR 后端所有权检查 | 无 | irparse.c | ⚠️ 未实现（2.0 范围，PB-26/PB-29 已覆盖） |
| `?=` 安全赋值记号 | 已从语法移除（BNF v2.3）：A 后端 `case TOK_SAFE_ASSIGN` 显式拒绝并提示改用 `=`；IR 前端无该分支，词法 token 落入通用"unexpected token"报错 | lexer.c:613（词法保留）、parser.c:2448–2457（语法拒绝） | ✅ 已按规范移除（PA-20） |
| `?.` / `?(` 安全解引用记号 | 已从语法移除（BNF v2.3）：A 后端 postfix 循环遇 `TOK_SAFE_DOT` 显式拒绝并提示改用 `.()`；IR 前端同口径显式分支。`?(` 从未是 token（词法切成 `?` + `(`），无需处理 | token.h（`TOK_SAFE_DOT` 词法保留）、parser.c:2186–2190、irparse.c:1033–1039 | ✅ 已按规范移除（PA-21） |
| `.()` / `.(T)` 解引用安全检查 | 读侧 `vis_check_usable`；写穿侧 `vis_check_writable`（对 lhs 指针检查）；编译期宽度检查：`sizeof(T)` 超过该指针静态已知所指字节数即报错。所指字节数来自 `malloc(T)`/`malloc(T,n)` 初值或 `&x`（`sym->type->ref->size`），对指针本身重新赋值时清零（保守不检查）| parser.c:2066（可见性）、2088–2099（越界检查）、2425（写穿检查）、2427（重赋值撤销）、1962–1980（malloc 字节数记录）、ncc.h（`Symbol.pointee_bytes`）| ✅ 已实现（PA-21）。IR 前端仅支持裸 `p.()`，无 `.(T)` 与宽度检查 |
| 运行期空指针检查 | 无 | — | ⚠️ 未实现（非空由静态规则保证：指针声明必须初始化，§5.1；`malloc` 返回 `NULL` 的运行期检查属 2.0 范围）|

> `vis_check_assign()` 仅在赋值右侧为**单个裸标识符**时触发。`x = y + 1` 或 `x = malloc(...)` 等表达式形式的赋值不经过传递矩阵检查。IR 后端（irparse.c）仅记录可见性值用于 `visof()` 查询，不执行所有权/借用检查。

---

## `is` 模式匹配（§6.1，BNF v2.2）

| 规范要求 | 编译器实现 | 对应代码 | 状态 |
|---------|----------|---------|------|
| `is` 仅配合 `while`（块形式） | A 后端 `while_depth` 计数守卫：`case TOK_IS` 在循环体外直接报错；IR 前端 `is_val_vreg < 0` 守卫 | parser.c:1216–1218、1341–1350、irparse.c:2021–2024 | ✅ 已实现（v1.0.2 补 A 后端守卫，PA-18） |
| `do` 不支持 `is` | A 后端进入 `do` 体前把 `while_depth` 清零、退出恢复；IR 前端 `do` 分支置 `is_val_vreg = -1`。嵌套在 `while` 内的 `do` 同样拒绝，不会静默匹配外层条件值 | parser.c:1232–1235、1341–1350、irparse.c:1896、1909 | ✅ 已实现（双前端显式拒绝，v1.0.2 补 IR 侧，PA-19） |
| `is <int-literal>` / `is -<int>` | `== v` / `== -v` | parser.c:1121–1133 | ✅ 已实现 |
| `is lo..hi` 闭区间 | `>= lo && __is_val <= hi` | parser.c:1128–1138 | ✅ 已实现 |
| `is <visibility-enum>` | 比较 `NH_*` 常量 | parser.c:1140–1156 | ✅ 已实现 |
| `is <enum-variant>` / 已知常量 | `TOK_IDENTIFIER` 分支按值比较 `== pat` | parser.c:1140–1147 | ✅ 已实现 |
| `is _` 通配符恒匹配 | 恒真分支 `if (1)`（IR 侧不发比较与 JZ） | parser.c:1109–1119、irparse.c:2029–2032 | ✅ 已实现（v1.0.2 补全） |
| `is <identifier>` 变量绑定 | 按**值**比较，非绑定 | parser.c:1140–1147 | ⚠️ 语义差异（2.0 范围，PB-27 类型感知后统一） |
| 多个 `is-clause` 首个匹配即止（无 fallthrough） | 各 is-clause 生成**并列** `if`，条件重叠时连续执行 | parser.c:1163–1167、irparse.c:2140–2144 | ⚠️ **未实现**（R 规则缺口；`is _` 恒匹配更易触发，需改控制流生成，未纳入 v1.0.2；已登记 `TODO-PA.md` PA-16 待决策） |
| `is <pat> => <stmt>` 单语句 | 已移除（BNF v2.2 / PA-13），双前端仅接受块形式 | parser.c:1163–1167、irparse.c:2140–2144 | ✅ 已按规范移除 |
| 反向范围 `lo > hi` 编译期校验 | 无 | parser.c:1128–1138 | ⚠️ 未实现（2.0 范围，PB-27.4 已覆盖） |
| 结构体解构 / ADT 变体解构 | 预留语法，未实现 | — | ⚠️ 预留（依赖类型系统，PB-27.10/11） |

---

## 存储期属性（§11.1）

| 规范要求 | 编译器实现 | 对应代码 | 状态 |
|---------|----------|---------|------|
| `const` 模块级静态 / 块级自动 | C 后端自然实现 | parser.c:1009–1011 | ✅ 隐式实现 |
| `flow` 动态分配 + 自动释放 | 块退出时自动 free | parser.c:1418–1430 | ✅ 已实现 |
| `flow` 返回值所有权转移 | `ownership_transferred` 标志 | parser.c:1307–1317 | ✅ 已实现 |
| `static` 静态存储 | C `static` 关键字 | cgen | ✅ 已实现 |

> `const` 的存储期区分（模块级静态 / 块级自动）由 C 后端编译器的自然语义实现——ncc 不显式区分两种 `const`，但生成的 C 代码中，文件作用域 `const` 自然获得静态存储期，块作用域 `const` 自然获得自动存储期。`vis_check_transfer()` 对两种 `const` 一视同仁。

---

## 测试覆盖

| 测试文件 | 测试规则 | 状态 |
|---------|---------|------|
| tests/err/m2a_flow_static.nc | `flow`→`static` 禁止 | ✅ |
| tests/err/m2b_const_flow.nc | `const`→`flow` 禁止 | ✅ |
| tests/err/m2c_frozen.nc | 冻结源不可写 | ✅ |
| tests/err/m2d_invalid.nc | 失效源不可读 | ✅ |
| tests/err/is_outside_while.nc | 循环体外 `is` 前端拒绝 | ✅（v1.0.2 / PA-18 新增，四后端） |
| tests/err/is_in_do_body.nc | `do` 体内 `is` 前端拒绝 | ✅（v1.0.2 / PA-19 新增，四后端） |
| tests/err/safe_assign_removed.nc | `?=` 已移除，赋值处拒绝 | ✅（v1.0.2 / PA-20 新增，四后端） |
| tests/err/safe_dot_removed.nc | `?.` 已移除，postfix 处拒绝 | ✅（v1.0.2 / PA-21 新增，四后端） |
| tests/err/deref_bounds.nc | `.(i64)` 读取宽度超过所指对象字节数 | ✅（v1.0.2 / PA-21 新增，c/native；IR 前端无 `.(T)`，经 `IR_ERR_SKIP` 跳过） |
| tests/pos/borrow.nc | `flow`→`var` 借用 + 解冻 | ✅ |
| tests/pos/flow.nc | `flow` 块级自动释放 | ✅ |
| tests/pos/transfer.nc | `flow` 返回值所有权转移 | ✅ |

> 上表为 1.0 发布集（`tests/pos` + `tests/err`，c/native 双后端）。`tests/pos/ir_*.nc` 属 2.0 IR 线（PB 分支），不在 1.0 发布门禁内。err 用例自 v1.0.2（PA-19）起在四个后端下均执行，`m2a`~`m2d` 四条 M2 静态检查用例与 `deref_bounds`（`.(T)` 类型化解引用）共五条经 `xmake.lua` 的 `IR_ERR_SKIP` 在 IR 后端跳过（IR 前端无所有权/借用检查，且只支持裸 `p.()`）。`is` 模式匹配由 `tests/pos/pattern.nc` 覆盖，该用例含 `is _` 通配符分支（v1.0.2 / PA-15 起，四后端输出一致）；`is <identifier>` 变量绑定仍无用例（对应上表 ⚠️ 项）。

---

## 2.0 线（PB）差异

以下为 PB 与上表（1.0 现态）不同的条目，行号基于 PB `8079ad0`：

| 条目 | 1.0 线（上表） | 2.0 线（PB）现态 |
|------|---------------|-----------------|
| §12.2 `flow`/`var`/`const` 参数前缀 | 前缀被忽略，统一 `VIS_DEFAULT`（parser.c:864–868、879） | **已实现**：`param->vis = pv` 记录前缀（parser.c:934），调用点 `vis_check_call_arg()` 执行 M2 检查（parser.c:2125）；IR 前端由 `vvis` 状态机等价实现（irparse.c，PB-26） |
| 返回值可见性前缀（§12.3 后半） | 无 | **已实现**：`ret_vis` 调用点检查（parser.c:1105）+ IR 侧 PB-29（含 PB-29.1 六条禁止路径 err 用例） |
| 检查范围表「IR 后端所有权检查」 | ⚠️ 未实现 | **已实现**（PB-26/M2 移植进 irparse.c，err 用例 m2a~m2d 在 ir-c/ir-native 下同样拒绝） |
| `is` 各模式行号 | parser.c:1102–1168 | parser.c:1194–1258（`_` 通配符 1201/`if (1)` 1204；错误信息 `expected block after 'is' pattern` 1258）；`is` 块由 irparse.c:2234 起的 `_` 分支处理 |
| `is _` 通配符 | v1.0.2 补全（PA-15） | PB-27.5 先于 1.0 线完成（parser.c + irparse.c 双前端） |
| 反向范围 `lo > hi` 校验 | ⚠️ 未实现 | **IR 前端已实现**（irparse.c:2259–2261 编译期报错）；A 后端 `parser.c` 仍不校验 |
| `__is_val` 类型 | `int` 固定 | **类型感知**，等于 `while` 条件表达式类型（PB-27.7） |
| 循环体外使用 `is` | A 后端 `while_depth` 守卫即时报错（parser.c:1341–1350，v1.0.2 / PA-18）+ IR 前端 `is_val_vreg < 0` 报错 | 双前端均拒绝：IR 前端由 PB-27.1 先落地，**A 后端守卫尚未从 1.0 线回灌**（PB `parser.c` 的 `case TOK_IS` 仍为通用分支、无守卫） |
| 多个 `is-clause` 无 fallthrough | ⚠️ 未实现（PA-16 登记待决策） | **同样未实现**（并列 `if` / 独立比较跳转），待与 PA-16 一并决策 |
| `?=` 安全赋值记号 | 已从语法移除，A 后端显式拒绝（PA-20，BNF v2.3） | **仍接受**：PB `parser.c` 在声明（498）、语句窥探（1544）、赋值（2505）三处把 `?=` 与 `=` 同路处理，`token.h:112` 注释亦未更新；移除需回灌 PB（与 `while_depth` 守卫同批） |
| `?.` 安全解引用记号 | 已从语法移除，A/IR 双前端显式拒绝（PA-21，BNF v2.3） | **仍作为 `.(` 的别名接受**：PB `parser.c` / `irparse.c` 的 postfix 分支把 `TOK_SAFE_DOT` 与 `TOK_DOT_PAREN` 同路处理，`token.h` 注释亦未更新；移除需回灌 PB |
| `.()` / `.(T)` 越界检查 | A 后端编译期宽度检查（PA-21） | **未实现**：PB `irparse.c` 仍只支持裸 `p.()`、无 `.(T)`；PB `parser.c` 解引用链无宽度检查。检查规则回灌 PB 前，2.0 线文档不得声称已具备该检查 |
| 测试覆盖 | 1.0 门禁 17P/0F/5S（含 PA-18 ~ PA-21 五条 err 用例，四后端执行；IR 侧 5P/0F/13S） | `xmake test --all` 全矩阵（c/native/ir-c/ir-native），见 `ROADMAP.md` 里程碑「验收」行 |
