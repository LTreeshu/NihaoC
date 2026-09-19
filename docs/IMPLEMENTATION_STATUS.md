# NihaoC 规范与编译器实现对应关系

本文档记录 NihaoC 语言规范（Chinese.md / English.md）中各项指针安全规则与编译器（ncc）实际实现的对应状态。

> 更新日期：2026-09-20。本表主体以 **1.0 冻结线（v1.0.2）** 的现态书写，行号基于该时点 `PA`/`main` 的 `parser.c` / `vis.c` / `irparse.c`，仅作导航用。
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
| `flow` 参数接收所有权 | 参数前缀被忽略 | parser.c:905–909、920 | ⚠️ 未实现 |
| `var` 参数冻结实参 | 参数前缀被忽略 | parser.c:905–909、920 | ⚠️ 未实现 |
| `const` 参数冻结实参 | 参数前缀被忽略 | parser.c:905–909、920 | ⚠️ 未实现 |

> 当前编译器解析函数参数上的属性前缀（`flow`/`var`/`const`），但在内部将所有参数统一视为 `var`（`VIS_DEFAULT`）。参数的所有权/借用检查不在 1.0 范围内，已由 2.0（PB 分支 PB-26）实现并验证，1.0 冻结线不移植。

---

## 检查范围

| 规范要求 | 编译器实现 | 对应代码 | 状态 |
|---------|----------|---------|------|
| 裸标识符赋值检查 | 完整实现 | parser.c:1049–1062（声明初始化）、2697–2710（表达式赋值） | ✅ 已实现 |
| 表达式级检查 | 仅检查裸标识符 | parser.c:1049–1062、2697–2710 | ⚠️ 部分实现 |
| IR 后端所有权检查 | 无 | irparse.c | ⚠️ 未实现（2.0 范围，PB-26/PB-29 已覆盖） |
| `?=` 安全赋值记号 | 已从语法移除（BNF v2.3）：A 后端 `case TOK_SAFE_ASSIGN` 显式拒绝并提示改用 `=`；IR 前端无该分支，词法 token 落入通用"unexpected token"报错 | lexer.c:613（词法保留）、parser.c:2714–2722（语法拒绝） | ✅ 已按规范移除（PA-20） |
| `?.` / `?(` 安全解引用记号 | 已从语法移除（BNF v2.3）：A 后端 postfix 循环遇 `TOK_SAFE_DOT` 显式拒绝并提示改用 `.()`；IR 前端同口径显式分支。`?(` 从未是 token（词法切成 `?` + `(`），无需处理 | token.h（`TOK_SAFE_DOT` 词法保留）、parser.c:2379–2385、irparse.c:1033–1039 | ✅ 已按规范移除（PA-21） |
| `.()` / `.(T)` 解引用安全检查 | 读侧 `vis_check_usable`；写穿侧 `vis_check_writable`（对 lhs 指针检查）；编译期宽度检查：`sizeof(T)` 超过该指针静态已知所指字节数即报错。所指字节数来自 `malloc(T)`/`malloc(T,n)` 初值或 `&x`（`sym->type->ref->size`），对指针本身重新赋值时清零（保守不检查）| parser.c:2080–2083（可见性）、2342–2354（越界检查）、2659 与 2736（写穿检查）、2661（重赋值撤销）、2167–2200 与 1047/1068（malloc 字节数记录）、ncc.h（`Symbol.pointee_bytes`）| ✅ 已实现（PA-21）。IR 前端仅支持裸 `p.()`，无 `.(T)` 与宽度检查 |
| 运行期空指针检查 | 无 | — | ⚠️ 未实现（非空由静态规则保证：指针声明必须初始化，§5.1；`malloc` 返回 `NULL` 的运行期检查属 2.0 范围）|

> `vis_check_assign()` 仅在赋值右侧为**单个裸标识符**时触发。`x = y + 1` 或 `x = malloc(...)` 等表达式形式的赋值不经过传递矩阵检查。IR 后端（irparse.c）仅记录可见性值用于 `visof()` 查询，不执行所有权/借用检查。

---

## 指针后缀链与切片（§5.1，BNF v2.5）

| 规范要求 | 编译器实现 | 对应代码 | 状态 |
|---------|----------|---------|------|
| `.(T)` 可处于后缀链任意位置，其后继续下标/成员/切片 | 通用 postfix 循环：每个算子把「已生成的前缀 C 文本」整体包裹后继续 | parser.c:2278–2292（前缀回取）、2313–2356（`deref_prefix` / `deref_step`）、2359 起（`parse_postfix`） | ✅ 已实现（v1.0.2 / PA-25，c/native）。IR 前端属 2.0 待做 |
| 数组类型化解引用 `p.(char[9])[i]` | 数组 cast 生成 `char (*)[9]`（而非 `char**`），下标按整数组步长推进 | parser.c:2295–2309（`c_cast_name`） | ✅ 已实现（PA-25） |
| 空下标 `p[]` = 省略类型的 `p.()`（一层解引用） | postfix `[` 紧跟 `]` 时走 `deref_prefix`；`p.().()` 多级链由此统一写作 `p[][].(i32)` | parser.c:2401–2404 | ✅ 已实现（PA-25） |
| 通用 `void` 指针裸下标/切片 | 前端报错并给出修复写法 `p.(T)[i]`（元素宽度静态未知，无法翻译成合法 C） | parser.c:2406–2412 | ✅ 已实现（PA-25，§5.1.2） |
| 切片读 `p[a..b]` | 生成「指向第 a 号元素的指针」`&p[a]`，长度不进入值本身（C 无数组切片类型）；`[..b]` 省略起点等价 `[0..b]` | parser.c:2413–2445 | ✅ 已实现（PA-25） |
| 切片读赋给固定数组 `T[n] x = p[a..b]` | 按**声明容量**逐元素复制（`memcpy`），非别名 | parser.c:1063–1069、1103–1110 | ✅ 已实现（PA-25） |
| 切片写 `p[a..b] = {v0, v1, …}` | 逐元素写回，生成逗号表达式一条 C 语句；右值非花括号列表时报错 | parser.c:2663–2694 | ✅ 已实现（PA-25）。§5.1.4 的字符串右值形式尚未支持（登记 PA-26） |
| `T[n] x = malloc(T[n])` 声明退化为指针 | 生成 `T (*x)…`（仅省略最前维度：`void[4][5]` → `void* (*x)[5]`），`malloc` 字节数按元素个数乘算 | parser.c:542–556（`c_decay_suffix`）、1088–1093、2177–2188 | ✅ 已实现（PA-25） |
| 多维数组声明 `T[a][b]` | C 后缀按源码顺序输出全部维度（`int32_t m[2][3]`） | cgen.c（`c_type_suffix`） | ✅ 已实现（PA-25） |

> A 后端不记录切片长度：`p[a..b]` 的 `b` 被解析后丢弃，边界由接收方（数组声明容量 / 写回元素个数）决定，因此 `b` 越界属运行期未定义行为，与 §5.1.2 一致。`len()` 对切片变量的支持属另一条缺口（`TODO-PA.md`）。

---

## 内置查询函数（§2.3，BNF v2.4）

A 后端把 `sizeof` / `typeof` / `alignof` / `offsetof` / `bitoffsetof` / `visof` / `structof` / `unionof` / `holdof` 作为**关键字 token**处理，统一由 `parse_builtin_kw()` 分派（`parse_primary` 的 `switch` 进入，parser.c:2041）；标识符路径的内置函数表看不到这些关键字。

| 规范要求 | 编译器实现 | 对应代码 | 状态 |
|---------|----------|---------|------|
| `structof(T, m, p)` / `unionof(T, m, p)` / `holdof(T, m, p)` 从属查询 | 生成 `((void*)((char*)(p) - offsetof(T, m)))`，即反推成员所属聚合体首地址；`T` 必须是聚合体，`m` 必须是其成员，`structof` 仅接受 `struct`、`unionof` 仅接受 `union`、`holdof` 两者通用 | parser.c:1607–1613（成员查找）、1686–1723（三参分派与诊断） | ✅ 已实现（v1.0.2 / PA-22，c/native）。IR 前端属 2.0 布局待做，`ir-*` 经 `IR_SUBSET`/`IR_ERR_SKIP` 跳过 |
| `bitoffsetof(T, m)` 位域位偏移 | 按声明顺序的位布局：以前置非位域成员为锚点，用 C 自身 `offsetof`/`sizeof` 求存储单元起点，再累加整单元与单元内位；对齐以基类型 `sizeof` 近似（基本整型 `align == size`）；目标非位域、基类型宽度未知、同一组成员存储类型不一致均报错 | parser.c:1725–1803 | ✅ 已实现（v1.0.2 / PA-22，c/native）。IR 前端未实现 |
| `alignof(T)` 类型对齐 | 生成 `_Alignof(T)` | parser.c:1641–1648 | ⚠️ **有缺陷**：tcc 0.9.27 不提供 `_Alignof`，`c` 后端链接期报 `undefined symbol '_Alignof'`（PA-22 探针发现，已登记 `TODO-PA.md` PA-24 待决策）|
| 聚合体成员列表分隔符 | 成员以**空白**分隔（`Point struct { x i32 y i32 }`），逗号被拒绝（`expected member name, got ','`） | parser.c:103（成员解析） | ⚠️ **文档示例不一致**：Chinese.md / English.md §14.1 若干示例用逗号分隔成员，无法编译（已登记 `TODO-PA.md` PA-23 待决策）|

---

## `is` 模式匹配（§6.1，BNF v2.2）

| 规范要求 | 编译器实现 | 对应代码 | 状态 |
|---------|----------|---------|------|
| `is` 仅配合 `while`（块形式） | A 后端 `while_depth` 计数守卫：`case TOK_IS` 在循环体外直接报错；IR 前端 `is_val_vreg < 0` 守卫 | parser.c:1298–1300、1423–1430、irparse.c:2021–2024 | ✅ 已实现（v1.0.2 补 A 后端守卫，PA-18） |
| `do` 不支持 `is` | A 后端进入 `do` 体前把 `while_depth` 清零、退出恢复；IR 前端 `do` 分支置 `is_val_vreg = -1`。嵌套在 `while` 内的 `do` 同样拒绝，不会静默匹配外层条件值 | parser.c:1313–1317、1423–1430、irparse.c:1896、1909 | ✅ 已实现（双前端显式拒绝，v1.0.2 补 IR 侧，PA-19） |
| `is <int-literal>` / `is -<int>` | `== v` / `== -v` | parser.c:1203–1221 | ✅ 已实现 |
| `is lo..hi` 闭区间 | `>= lo && __is_val <= hi` | parser.c:1212–1218 | ✅ 已实现 |
| `is <visibility-enum>` | 比较 `NH_*` 常量 | parser.c:1231–1238 | ✅ 已实现 |
| `is <enum-variant>` / 已知常量 | `TOK_IDENTIFIER` 分支按值比较 `== pat` | parser.c:1222–1230 | ✅ 已实现 |
| `is _` 通配符恒匹配 | 恒真分支 `if (1)`（IR 侧不发比较与 JZ） | parser.c:1191–1200、irparse.c:2029–2032 | ✅ 已实现（v1.0.2 补全） |
| `is <identifier>` 变量绑定 | 按**值**比较，非绑定 | parser.c:1222–1230 | ⚠️ 语义差异（2.0 范围，PB-27 类型感知后统一） |
| 多个 `is-clause` 首个匹配即止（无 fallthrough） | 各 is-clause 生成**并列** `if`，条件重叠时连续执行 | parser.c:1243–1249、irparse.c:2140–2144 | ⚠️ **未实现**（R 规则缺口；`is _` 恒匹配更易触发，需改控制流生成，未纳入 v1.0.2；已登记 `TODO-PA.md` PA-16 待决策） |
| `is <pat> => <stmt>` 单语句 | 已移除（BNF v2.2 / PA-13），双前端仅接受块形式 | parser.c:1245–1249、irparse.c:2140–2144 | ✅ 已按规范移除 |
| 反向范围 `lo > hi` 编译期校验 | 无 | parser.c:1212–1218 | ⚠️ 未实现（2.0 范围，PB-27.4 已覆盖） |
| 结构体解构 / ADT 变体解构 | 预留语法，未实现 | — | ⚠️ 预留（依赖类型系统，PB-27.10/11） |

---

## 存储期属性（§11.1）

| 规范要求 | 编译器实现 | 对应代码 | 状态 |
|---------|----------|---------|------|
| `const` 模块级静态 / 块级自动 | C 后端自然实现 | parser.c:781、1009 | ✅ 隐式实现 |
| `flow` 动态分配 + 自动释放 | 块退出时自动 free | parser.c:1500–1512 | ✅ 已实现 |
| `flow` 返回值所有权转移 | `ownership_transferred` 标志 | parser.c:1395–1405 | ✅ 已实现 |
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
| tests/err/structof_bad_member.nc | `structof(T, m, p)` 的 `m` 不在 `T` 中 | ✅（v1.0.2 / PA-22 新增，c/native 拒绝；IR 前端无该内置函数，经 `IR_ERR_SKIP` 跳过） |
| tests/pos/ownerof.nc | `structof`/`unionof`/`holdof` 三参反推首地址 + `bitoffsetof` 位偏移 | ✅（v1.0.2 / PA-22 新增，c/native；IR 侧未覆盖，跳过） |
| tests/err/void_subscript.nc | 通用 `void` 指针裸下标前端拒绝，提示改写 `p.(T)[i]` | ✅（v1.0.2 / PA-25 新增，c/native；IR 前端无 `.(T)`，经 `IR_ERR_SKIP` 跳过） |
| tests/pos/deref_slice.nc | 多级 `[]`/`.()` 链、数组指针 `.(char[9])[i]`、切片读复制、切片写逐元素回写、`T[n]=malloc(T[n])` 退化、多维声明 | ✅（v1.0.2 / PA-25 新增，c/native；IR 侧未覆盖，跳过） |
| tests/pos/borrow.nc | `flow`→`var` 借用 + 解冻 | ✅ |
| tests/pos/flow.nc | `flow` 块级自动释放 | ✅ |
| tests/pos/transfer.nc | `flow` 返回值所有权转移 | ✅ |

> 上表为 1.0 发布集（`tests/pos` + `tests/err`，c/native 双后端）。`tests/pos/ir_*.nc` 属 2.0 IR 线（PB 分支），不在 1.0 发布门禁内。err 用例自 v1.0.2（PA-19）起在四个后端下均执行，`m2a`~`m2d` 四条 M2 静态检查用例与 `deref_bounds`（`.(T)` 类型化解引用）、`structof_bad_member`（从属查询内置函数）、`void_subscript`（通用 `void` 指针裸下标检查）共七条经 `xmake.lua` 的 `IR_ERR_SKIP` 在 IR 后端跳过（IR 前端无所有权/借用检查，只支持裸 `p.()`，且未实现从属/位域内置函数与 `.(T)` 相关检查）。`ownerof`（从属查询 + 位域偏移）与 `deref_slice`（后缀链解引用 + 切片 + 数组退化）未列入 `IR_SUBSET`，在 IR 后端按「IR 子集未覆盖」自动跳过。`is` 模式匹配由 `tests/pos/pattern.nc` 覆盖，该用例含 `is _` 通配符分支（v1.0.2 / PA-15 起，四后端输出一致）；`is <identifier>` 变量绑定仍无用例（对应上表 ⚠️ 项）。门禁（v1.0.2 / PA-25 后）：`xmake test --all` → c / native 各 **21 PASS / 0 FAIL / 5 SKIP**，ir-c / ir-native 各 **5 PASS / 0 FAIL / 17 SKIP**。

---

## 2.0 线（PB）差异

以下为 PB 与上表（1.0 现态）不同的条目，行号基于 PB `8079ad0`：

| 条目 | 1.0 线（上表） | 2.0 线（PB）现态 |
|------|---------------|-----------------|
| §12.2 `flow`/`var`/`const` 参数前缀 | 前缀被忽略，统一 `VIS_DEFAULT`（parser.c:905–909、920） | **已实现**：`param->vis = pv` 记录前缀（parser.c:934），调用点 `vis_check_call_arg()` 执行 M2 检查（parser.c:2125）；IR 前端由 `vvis` 状态机等价实现（irparse.c，PB-26） |
| 返回值可见性前缀（§12.3 后半） | 无 | **已实现**：`ret_vis` 调用点检查（parser.c:1105）+ IR 侧 PB-29（含 PB-29.1 六条禁止路径 err 用例） |
| 检查范围表「IR 后端所有权检查」 | ⚠️ 未实现 | **已实现**（PB-26/M2 移植进 irparse.c，err 用例 m2a~m2d 在 ir-c/ir-native 下同样拒绝） |
| `is` 各模式行号 | parser.c:1184–1250 | parser.c:1194–1258（`_` 通配符 1201/`if (1)` 1204；错误信息 `expected block after 'is' pattern` 1258）；`is` 块由 irparse.c:2234 起的 `_` 分支处理 |
| `is _` 通配符 | v1.0.2 补全（PA-15） | PB-27.5 先于 1.0 线完成（parser.c + irparse.c 双前端） |
| 反向范围 `lo > hi` 校验 | ⚠️ 未实现 | **IR 前端已实现**（irparse.c:2259–2261 编译期报错）；A 后端 `parser.c` 仍不校验 |
| `__is_val` 类型 | `int` 固定 | **类型感知**，等于 `while` 条件表达式类型（PB-27.7） |
| 循环体外使用 `is` | A 后端 `while_depth` 守卫即时报错（parser.c:1423–1430，v1.0.2 / PA-18）+ IR 前端 `is_val_vreg < 0` 报错 | 双前端均拒绝：IR 前端由 PB-27.1 先落地，**A 后端守卫尚未从 1.0 线回灌**（PB `parser.c` 的 `case TOK_IS` 仍为通用分支、无守卫） |
| 多个 `is-clause` 无 fallthrough | ⚠️ 未实现（PA-16 登记待决策） | **同样未实现**（并列 `if` / 独立比较跳转），待与 PA-16 一并决策 |
| `?=` 安全赋值记号 | 已从语法移除，A 后端显式拒绝（PA-20，BNF v2.3） | **仍接受**：PB `parser.c` 在声明（498）、语句窥探（1544）、赋值（2505）三处把 `?=` 与 `=` 同路处理，`token.h:112` 注释亦未更新；移除需回灌 PB（与 `while_depth` 守卫同批） |
| `?.` 安全解引用记号 | 已从语法移除，A/IR 双前端显式拒绝（PA-21，BNF v2.3） | **仍作为 `.(` 的别名接受**：PB `parser.c` / `irparse.c` 的 postfix 分支把 `TOK_SAFE_DOT` 与 `TOK_DOT_PAREN` 同路处理，`token.h` 注释亦未更新；移除需回灌 PB |
| `.()` / `.(T)` 越界检查 | A 后端编译期宽度检查（PA-21） | **未实现**：PB `irparse.c` 仍只支持裸 `p.()`、无 `.(T)`；PB `parser.c` 解引用链无宽度检查。检查规则回灌 PB 前，2.0 线文档不得声称已具备该检查 |
| 从属查询 / 位域偏移内置函数（§2.3，BNF v2.4） | A 后端已实现三参 `structof`/`unionof`/`holdof` 与 `bitoffsetof`（PA-22，见上表） | **未接入**：PB `parse_primary` 只把 `sizeof`/`typeof`/`alignof`/`offsetof`/`visof` 分派给 `parse_builtin_kw()`（PB parser.c:1905–1909），`structof`/`unionof`/`holdof`/`bitoffsetof` 落入默认分支；PB `irparse.c` 亦无对应实现。1.0 线的 PA-22 实现需回灌 PB 后 2.0 线才可声称支持 |
| 指针后缀链与切片（§5.1，BNF v2.5） | A 后端已实现 `.(T)` 通用后缀链、`p[]` 空下标、切片读/写、`T[n]=malloc(T[n])` 退化、多维声明（PA-25，见上表） | **未接入**：PB `parser.c` 解引用仍是独立的 `parse_deref_chain` 路径（无 `[]` 空下标、无切片读/写回、无数组退化），PB `irparse.c` 同样无对应实现。1.0 线的 PA-25 代码生成需回灌 PB 后 2.0 线才可声称支持 |
| 测试覆盖 | 1.0 门禁 21P/0F/5S（含 PA-18 ~ PA-22、PA-25 七条 err 用例；IR 侧 5P/0F/17S） | `xmake test --all` 全矩阵（c/native/ir-c/ir-native），见 `ROADMAP.md` 里程碑「验收」行 |
