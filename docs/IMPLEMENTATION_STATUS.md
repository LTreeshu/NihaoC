# NihaoC 规范与编译器实现对应关系

本文档记录 NihaoC 语言规范（Chinese.md / English.md）中各项指针安全规则与编译器（ncc）实际实现的对应状态。

> 更新日期：2026-09-21。本表主体以 **1.0 冻结线（v1.0.2）** 的现态书写，行号基于 **本（`PB`）分支** 的 `parser.c` / `irparse.c`（`vis.c` 两侧同源，行号与 `PA` 一致），仅作导航用。`PA`/`main` 的行号口径见 `PA` 分支同文件。
> **PB（2.0 线）差异集中在文末「2.0 线（PB）差异」一节**：`vis.c` 的矩阵实现两侧同源（行号一致），`parser.c` / `irparse.c` 行号与部分状态不同。
>
> 本文件是**实现状态层**文档：`BNF.md` / `Chinese.md` / `English.md` / 两份语法元素表只写设计，不写落地情况；状态、行号、待办一律记在这里或 `TODO-PA.md` / `TODO-PB.md`。

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
| 裸标识符赋值检查 | 完整实现 | parser.c:1026–1033（声明初始化）、2393–2400（表达式赋值） | ✅ 已实现 |
| 表达式级检查 | 仅检查裸标识符 | parser.c:1026–1033、2393–2400 | ⚠️ 部分实现 |
| IR 后端所有权检查 | 无 | irparse.c | ⚠️ 未实现（2.0 范围，PB-26/PB-29 已覆盖） |

> `vis_check_assign()` 仅在赋值右侧为**单个裸标识符**时触发。`x = y + 1` 或 `x = malloc(...)` 等表达式形式的赋值不经过传递矩阵检查。IR 后端（irparse.c）仅记录可见性值用于 `visof()` 查询，不执行所有权/借用检查。

---

## `is` 模式匹配（§6.3，规范标题自 PA-42 起编号；BNF v2.2）

| 规范要求 | 编译器实现 | 对应代码 | 状态 |
|---------|----------|---------|------|
| `is` 仅配合 `while`（块形式） | `parse_is_stmt` 只在 while 体内调用 | parser.c:1102–1168（调用点 1336） | ✅ 已实现 |
| `do` 不支持 `is` | `TOK_DO` 走 `parse_statement` 通用体，不识别 is-clause | parser.c:1223 | ✅ 已实现（隐式） |
| `is <int-literal>` / `is -<int>` | `== v` / `== -v` | parser.c:1121–1133 | ✅ 已实现 |
| `is lo..hi` 闭区间 | `>= lo && __is_val <= hi` | parser.c:1128–1138 | ✅ 已实现 |
| `is <visibility-enum>` | 比较 `NH_*` 常量 | parser.c:1140–1156 | ✅ 已实现 |
| `is <enum-variant>` / 已知常量 | `TOK_IDENTIFIER` 分支按值比较 `== pat` | parser.c:1140–1147 | ✅ 已实现 |
| `is _` 通配符恒匹配 | 恒真分支 `if (1)`（IR 侧不发比较与 JZ） | parser.c:1109–1119、irparse.c:2029–2032 | ✅ 已实现（v1.0.2 补全） |
| `is <identifier>` 按值比较（BNF v2.11 定案，绑定/解构留 2.0） | 生成 `__is_val == pat`，不引入新绑定 | parser.c:1140–1147 | ✅ 与规范一致；变量绑定属 2.0（PB-27 类型感知后再议） |
| 多个 `is-clause` 首个匹配即止（无 fallthrough） | 各 is-clause 生成**并列** `if`，条件重叠时连续执行 | parser.c:1163–1167、irparse.c:2140–2144 | ⚠️ **未实现**（R 规则缺口；`is _` 恒匹配更易触发，需改控制流生成，未纳入 v1.0.2；已登记 `TODO-PA.md` PA-16 待决策） |
| `is <pat> => <stmt>` 单语句 | 已移除（BNF v2.2 / PA-13），双前端仅接受块形式 | parser.c:1163–1167、irparse.c:2140–2144 | ✅ 已按规范移除 |
| 反向范围 `lo > hi` 编译期校验 | 无 | parser.c:1128–1138 | ⚠️ 未实现（2.0 范围，PB-27.4 已覆盖） |
| 结构体解构 / ADT 变体解构 | 预留语法，未实现 | — | ⚠️ 预留（依赖类型系统，PB-27.10/11） |

---

## 存储期属性（§11.1）

| 规范要求 | 编译器实现 | 对应代码 | 状态 |
|---------|----------|---------|------|
| `const` 模块级静态 / 块级自动 | C 后端自然实现 | parser.c:1009–1011 | ✅ 隐式实现 |
| `flow` 动态分配 + 自动释放 | 块退出时自动 free | parser.c:1405–1416 | ✅ 已实现 |
| `flow` 返回值所有权转移 | `ownership_transferred` 标志 | parser.c:1307–1317 | ✅ 已实现 |
| `static` 静态存储 | C `static` 关键字 | cgen | ✅ 已实现 |

> `const` 的存储期区分（模块级静态 / 块级自动）由 C 后端编译器的自然语义实现——ncc 不显式区分两种 `const`，但生成的 C 代码中，文件作用域 `const` 自然获得静态存储期，块作用域 `const` 自然获得自动存储期。`vis_check_transfer()` 对两种 `const` 一视同仁。

---

## 语法固定轮实测记录（2026-09-21，BNF v2.11）

> 本轮把 `BNF.md` / `Chinese.md` / `English.md` 统一为**纯设计层**文档：所有"哪个后端已落地、待办、行号、测试计数"的陈述从这三份文档移出，集中到本文件。
> **下表实测数据采自 1.0 冻结线（`PA` / `main`，v1.0.2）的 A 后端（backend `c`）**，在本分支（PB / 2.0 线）逐条复测前只作为设计裁定的实现参照；复测任务见 `TODO-PB.md`。样本与生成产物在 `ncc/build/probe1.nc` ~ `probe9.nc`（gitignore，不入库）。

| 编号 | 探针写法 | 实测结果 | 定性 |
|------|---------|---------|------|
| F1 | `P struct { x i32 = 7  y i32 }` | 前端接受，生成 `typedef struct P { 7int32_t x; int32_t y; } P;`，tcc 报 `error: invalid number` | **A 后端代码生成缺陷**（成员默认值被当作类型名前缀输出）。设计侧 `<field-decl> ::= … [ "=" <expr> ]` 意图明确，产生式保留；修好前该写法属"语法可写、不可编译"。待办：`TODO-PA.md`（PA-49） |
| F2 | `print("%d\n", i)` | 生成的 C 里字符串常量**含真实 0x0A 字节**（`printf("` + 换行 + `", i)`），而非 `\n` 两字符 | **A 后端代码生成缺陷（可移植性）**。tcc 作为扩展容忍，gcc/clang/MSVC 会报 unclosed string literal；当前门禁全在 tcc 下跑，故未暴露。待办：`TODO-PA.md`（PA-50） |
| F3 | `switch (1) { case 1: … case 2: … default: … }` | 只执行命中的那个 case；生成的 C 每个 case 体末尾自动插 `break;` | 实现即为**不穿透**。设计已在 BNF §6 / 中英 §6.2 固定为「case 不穿透，1.x 无显式穿透写法」，无需改代码 |
| F4 | `for i = 0; i < 3; bump(&cnt) { … }` | 编译通过，原样生成 `for (int32_t i = 0; i < 3; bump(&cnt))` | 实现比旧产生式宽。设计已按实测放宽：`<for-step> ::= <expr>`（1.x 不做形态限制，文档不再承诺"只允许赋值/++/--"） |
| F5 | `a fx32 = 1` / `b fx64 = 2` / `c string = "abc"` | 分别生成 `int32_t a`、`int64_t b`、`char* c`，编译运行均通过 | `fx32` / `fx64` **语法与类型系统已接通但无定点语义**（按同宽整型下降，小数点位置未定义，2.0 定案）；`string` → `char*` 与 `char[]` 同型口径一致 |
| F6 | `short s = 1` / `int i` / `long l` / `float f` / `double d` 写在类型位置 | A 后端逐个报 `unexpected token 'short' in expression` … `'double' in expression`，共 5 error 后终止 | 与本轮设计裁定一致：基本类型收敛为 **16 个**，C 风格别名不再是类型（`short`→`i16`、`int`→`i32`、`long`→`i64`、`float`→`f32`、`double`→`f64`）。两前端与全部 `tests/` `examples/` 均无消费点，故属"文档追平实现"而非行为变更 |
| F7 | `while w -= 1 { is 3 {…} is 1..5 {…} }` 重叠模式 | A 后端只执行首个匹配子句（`__is_matched` 每轮迭代重置） | 与"首个匹配即止、无 fallthrough"设计一致。IR 前端仍生成并列 `if`，见「`is` 模式匹配」表与下文 2.0 差异表 |
| F8 | 四种数组容量写法逐个编译：`x i32[5]` / `x i32[5...]` / `x i32[...]` / `x i32[...5]`（**A 后端 c/native**） | `[5]` → `int32_t x[5]` 正常；`[5...]` → **前端拒绝**（`expected ']', got 'TOK_ELLIPSIS'`）；`[...]` → 退化为 `int32_t* x`，**没有默认容量 8**；`[...5]`（设计标为已废弃的反序写法）→ `int32_t x[5]` 正常。`[5..]` / `[..5]` / `[..]` 三种点号写法一律拒绝 | **A 后端与设计的四档写法不一致**：规范里的 `[N...]` 在 1.0 发布线根本写不出来，反而是「已废弃」的 `[...N]` 可用；`[...]` 的默认容量 8 只在 IR 槽模型里成立（`tests/pos/ir_slice.nc:11` 注释即按 8 槽写）。待办：`TODO-PA.md`（PA-51） |
| F9 | `a i32[...] = {1, 2, 3}`（A 后端） | 生成 `int32_t* a = {1, 2, 3};`，tcc 报 `'}' expected (got ",")` | **A 后端代码生成缺陷**：省略容量的数组类型退化为指针后仍按聚合初值输出，生成非法 C。与 F8 同属 `[...]` 一档，修 F8 时需一并给出诊断口径。待办：`TODO-PA.md`（PA-51） |

> 本轮固定下来的其余裁定（`#` 为兼容语句终止符、`register` / `restrict` / `volatile` 与五个 C 风格别名仅词法保留、内置名不参与用户作用域解析、`<statement>` 含 `<label-def>`、指针必须初始化）均为设计层收口，无需实现变更，故不单列实测行。PB 线若发现与本文各行的状态差异，回填到「2.0 线（PB）差异」一节。

探针复现：`ncc/build/probe1.nc` ~ `probe9.nc`（gitignore，不入库），命令 `./ncc.exe build probeN.nc -o probeN.exe` 后查看同目录 `probeN.exe.c`。

---

## 测试覆盖

| 测试文件 | 测试规则 | 状态 |
|---------|---------|------|
| tests/err/m2a_flow_static.nc | `flow`→`static` 禁止 | ✅ |
| tests/err/m2b_const_flow.nc | `const`→`flow` 禁止 | ✅ |
| tests/err/m2c_frozen.nc | 冻结源不可写 | ✅ |
| tests/err/m2d_invalid.nc | 失效源不可读 | ✅ |
| tests/pos/borrow.nc | `flow`→`var` 借用 + 解冻 | ✅ |
| tests/pos/flow.nc | `flow` 块级自动释放 | ✅ |
| tests/pos/transfer.nc | `flow` 返回值所有权转移 | ✅ |

> 上表为 1.0 发布集（`tests/pos` + `tests/err`，c/native 双后端）。`tests/pos/ir_*.nc` 属 2.0 IR 线（PB 分支），不在 1.0 发布门禁内。`is` 模式匹配由 `tests/pos/pattern.nc` 覆盖，该用例含 `is _` 通配符分支（v1.0.2 / PA-15 起，四后端输出一致）；`is <identifier>` 的变量绑定形态无用例——BNF v2.11 已把它定为**按值比较**（见上表），变量绑定与解构属 2.0 范围。

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
| 循环体外使用 `is` | A 后端按 `if` 展开，不报错 | IR 前端报错拒绝（PB-27.1）；A 后端与 1.0 相同仍按 `if` 展开 |
| 多个 `is-clause` 无 fallthrough | ⚠️ 未实现（PA-16 登记待决策） | **同样未实现**（并列 `if` / 独立比较跳转），待与 PA-16 一并决策 |
| 测试覆盖 | 1.0 门禁 12P/0F/5S | `xmake test --all` 全矩阵（c/native/ir-c/ir-native），见 `ROADMAP.md` 里程碑「验收」行 |
