# NihaoC 规范与编译器实现对应关系

本文档记录 NihaoC 语言规范（Chinese.md / English.md）中各项指针安全规则与编译器（ncc）实际实现的对应状态。

> 更新日期：2026-09-27（**PB-33 的 P4 落地**：`is` 首个匹配即止 + `while` 体外拒绝、`flow` 存储来源登记、`flow→flow` 转移的失效源豁免与冻结源拒绝，均由 `PA` 的 PA-16 / PA-32 / PA-33 移植进本分支 A 后端；「`is` 模式匹配」表与「存储期属性」表随之改状态，文末「2.0 线（PB）差异」两行由「未实现」翻为「A 后端已实现、IR 前端未实现」，`parser.c` 1088 行以后的锚点按 +9 ~ +50 逐行重算、`vis.c` 130 行以后 +4 / 194 行以后 +20。**同轮附带把「`is` 模式匹配」表 12 行、「检查范围」表 2 行、「存储期属性」表 2 行的锚点从漂移前的旧值改成本轮实测的行号**（旧值指向 `parse_decl_or_assign` 的初始化区，与本表主题无关，属既有锚点失准，非本轮引入）。上一轮：PB-35 交叉发射器验证）。本表主体以 **1.0 冻结线（v1.0.2）** 的现态书写，行号基于 **本（`PB`）分支** 的 `parser.c` / `irparse.c`（`vis.c` 两侧同源，行号与 `PA` 一致），仅作导航用。`PA`/`main` 的行号口径见 `PA` 分支同文件。
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
| 失效变量不可读 | `vis_check_usable()` | vis.c:127–140 | ✅ 已实现 |
| 冻结变量不可写 | `vis_check_writable()` | vis.c:143–152 | ✅ 已实现 |
| 赋值检查编排 | `vis_check_assign()` | vis.c:158–218 | ✅ 已实现 |

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
| 裸标识符赋值检查 | 完整实现 | `vis_check_assign()`（vis.c:158–218）的两处调用：parser.c:1128–1133（声明初始化）、parser.c:2695–2702（表达式赋值） | ✅ 已实现 |
| 表达式级检查 | 仅检查裸标识符 | 读侧：`vis_check_usable()` 在 parser.c:2109、2325 两处裸标识符路径调用；写侧：赋值右值须先过 `is_expr_continuer` 的裸标识符判定（parser.c:2695–2702），`x = y + 1` / `x = f()` 均不进矩阵 | ⚠️ 部分实现 |
| 整变量重绑定的作用域界定 | `lhs_bare_ident` 区分「改变量本身」与「改所指对象」，只有前者重登记存储来源 | 置位 parser.c:2504（`parse_postfix` 尾，无链式后缀）、清零 parser.c:2432（`(T)` 解引用链）、消费 parser.c:2704–2712 | ✅ 已实现（PB-33 P4） |
| IR 后端所有权检查 | 无 | irparse.c | ⚠️ 未实现（2.0 范围，PB-26/PB-29 已覆盖） |

> `vis_check_assign()` 仅在赋值右侧为**单个裸标识符**时触发。`x = y + 1` 或 `x = malloc(...)` 等表达式形式的赋值不经过传递矩阵检查。IR 后端（irparse.c）仅记录可见性值用于 `visof()` 查询，不执行所有权/借用检查。
>
> P4 起 `flow→flow` 自转移不再误报 use-after-move：`cs->parser.moved_src`（ncc.h:228）在转移发生的那条语句内豁免失效源的可读性检查（`vis_check_usable()` vis.c:127–140 的提前返回），下一条语句进入 `parse_statement()` 开头即清除（parser.c:1279）；同时把源符号标上 `ownership_transferred`（vis.c:210）让退出时的自动释放跳过它，避免双重释放。转移的源若正被 `const`/`var` 借用（`BS_FROZEN`），改为拒绝并报 `cannot transfer ownership of 'x': it is borrowed (frozen) by an active const/var`（vis.c:196–203）。

---

## `is` 模式匹配（§6.3，规范标题自 PA-42 起编号；BNF v2.2）

| 规范要求 | 编译器实现 | 对应代码 | 状态 |
|---------|----------|---------|------|
| `is` 仅配合 `while`（块形式） | `parse_is_stmt()` 只在 while 体内调用；`cs->while_depth`（ncc.h:296）为 0 时 `TOK_IS` 分支先拒绝再按块展开 | `parse_is_stmt` parser.c:1203–1270；`TOK_IS` 守卫 parser.c:1451–1460；`while_depth` 增减 parser.c:1326/1328 | ✅ 已实现（P4 补拒绝路径） |
| `do` 不支持 `is` | `TOK_DO` 走 `parse_statement` 通用体，进体前把 `while_depth` 存零、出体恢复（P4） | parser.c:1341–1345（旧口径是"隐式不识别"，现改为显式清零后 `is` 会被 `TOK_IS` 守卫拒绝） | ✅ 已实现 |
| `is <int-literal>` / `is -<int>` | `== v` / `== -v` | 负数分支 parser.c:1225–1230（比较式 1228），正字面量 parser.c:1231–1233 + 1241–1243 | ✅ 已实现 |
| `is lo..hi` 闭区间 | `>= lo && __is_val <= hi` | parser.c:1234–1240（比较式在 1238） | ✅ 已实现 |
| `is <visibility-enum>` | 比较 `NH_*` 常量 | 关键字 token 分支 parser.c:1253–1260；`_flow` 等标识符写法分支 parser.c:1246–1250 | ✅ 已实现 |
| `is <enum-variant>` / 已知常量 | `TOK_IDENTIFIER` 分支按值比较 `== pat` | parser.c:1244–1252（兜底 `else cgen_raw(" == %s", pat)` 在 1251） | ✅ 已实现 |
| `is _` 通配符恒匹配 | 恒真分支 `(1)`（IR 侧不发比较与 JZ） | parser.c:1213–1222（恒真式在 1216）、irparse.c:2036–2039 | ✅ 已实现（v1.0.2 补全） |
| `is <identifier>` 按值比较（BNF v2.11 定案，绑定/解构留 2.0） | 生成 `__is_val == pat`，不引入新绑定 | parser.c:1244–1252 | ✅ 与规范一致；变量绑定属 2.0（PB-27 类型感知后再议） |
| 多个 `is-clause` 首个匹配即止（无 fallthrough） | 每子句守卫 `if (!__is_matched && … && (__is_matched = 1))`，置位写在条件里（块体由 `parse_statement` 整体输出，无法在其 `{` 后插语句）；`__is_matched` 在 while 脚手架声明并每轮迭代重置 | 守卫 parser.c:1216 / 1224 / 1265，声明 parser.c:1315，每轮重置 parser.c:1325；**IR 前端未实现**（仍并列 `if` / 独立比较跳转，irparse.c:2147–2151，与 `PA` 同口径留 2.0） | ✅ 已实现（PB-33 P4 移植 `PA` 的 PA-16；用例 `pos/is_no_fallthrough`、`err/is_outside_while`、`err/is_in_do_body`） |
| `is <pat> => <stmt>` 单语句 | 已移除（BNF v2.2 / PA-13），双前端仅接受块形式 | parser.c:1267–1271（非 `{` 即报 `expected block after 'is' pattern`）、irparse.c:2147–2151 | ✅ 已按规范移除 |
| 反向范围 `lo > hi` 编译期校验 | 无 | parser.c:1234–1240 | ⚠️ 未实现（2.0 范围，PB-27.4 已覆盖） |
| 结构体解构 / ADT 变体解构 | 预留语法，未实现 | — | ⚠️ 预留（依赖类型系统，PB-27.10/11） |

---

## 存储期属性（§11.1）

| 规范要求 | 编译器实现 | 对应代码 | 状态 |
|---------|----------|---------|------|
| `const` 模块级静态 / 块级自动 | C 后端自然实现 | parser.c:1009–1011 | ✅ 隐式实现 |
| `flow` 动态分配 + 自动释放 | 函数退出与块退出两处 free 循环 | parser.c:1167–1176（函数级 locals）、parser.c:1528–1539（块级 `scope_start..` 段） | ✅ 已实现 |
| `flow` 初值/重绑定的存储来源 | `no_auto_free`（ncc.h:127）：右值首 token 是字符串字面量 / `&x` / `{` 时不登记堆所有权，退出时不 free | 声明点 parser.c:1088–1096、整变量重绑定 parser.c:2704–2712，两处消费点即上两行 free 循环的条件 | ✅ 已实现（PB-33 P4；用例 `pos/flow_no_free`） |
| `flow` 返回值所有权转移 | `ownership_transferred` 标志 | parser.c:1423–1433（return 路径置位）、vis.c:206–211（`flow→flow` 转移时置位） | ✅ 已实现 |
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
| tests/pos/is_no_fallthrough.nc | 多 `is` 子句首个匹配即止，无 fallthrough（P4） | ✅ c / native 逐行比对；IR 侧 SKIP（仍有 fallthrough） |
| tests/err/is_outside_while.nc | `is` 在循环体外拒绝（P4） | ✅ |
| tests/err/is_in_do_body.nc | `is` 在 `do` 体内拒绝（P4） | ✅ |
| tests/pos/flow_no_free.nc | 字面量 / `&x` / `{…}` 初值的 `flow` 不自动 free（P4） | ✅ |
| tests/pos/flow_move.nc | `flow→flow` 转移，本语句读右值放行、接收方接管（P4） | ✅ 无 `.expect`，走跨后端一致性档 |
| tests/err/flow_move_frozen.nc | 冻结源转交所有权拒绝（P4） | ✅ |

> 上表为 1.0 发布集（`tests/pos` + `tests/err`，c/native 双后端）。`tests/pos/ir_*.nc` 属 2.0 IR 线（PB 分支），不在 1.0 发布门禁内。`is` 模式匹配由 `tests/pos/pattern.nc` 覆盖，该用例含 `is _` 通配符分支（v1.0.2 / PA-15 起，四后端输出一致）；`is <identifier>` 的变量绑定形态无用例——BNF v2.11 已把它定为**按值比较**（见上表），变量绑定与解构属 2.0 范围。

### 本分支回灌基线与逐轮复跑（PB-33；2026-09-26 起逐轮追加）

**回灌前（commit `1f83523`，2026-09-26 实测复跑）**

| 项目 | 现值 | 说明 |
| --- | --- | --- |
| `xmake test --all` c / native | **各 19P / 0F / 6S** | 6 条 SKIP 全是 `IR_ONLY`（`ir_builtin` / `ir_cook` / `ir_mr` / `ir_slice` / `ir_sparam` / `struct_param_prefix`） |
| `xmake test --all` ir-c / ir-native | **各 13P / 0F / 8S** | 8 条 SKIP 是「IR 子集未覆盖」的全量用例（`borrow` / `features` / `flow` / `malloc_demo` / `mathmod` / `pattern` / `transfer` / `use_mod`） |
| 跨后端一致性 | **36P / 0F** | |
| `examples/`（c 与 native 各跑一遍） | **6/7** | 唯一失败 `06_cooking.nc`，原因是 A 后端缺 `<ct-func-def>` / `<ct-func-call>`（PB-33 表 **P11**），`ir-c` 侧可编译 |

**P1 落地后（`?=` / `?.` 语法移除 + PB-31 两份元素表归一，2026-09-26 实测复跑）**

| 项目 | 现值 | 说明 |
| --- | --- | --- |
| `xmake test --all` c / native | **各 21P / 0F / 6S** | 各 +2 PASS：新增 `err/safe_assign_removed`、`err/safe_dot_removed` |
| `xmake test --all` ir-c / ir-native | **各 15P / 0F / 8S** | 各 +2 PASS 且 SKIP 数不变——`xmake.lua:152` 新增 `IR_ERR_COVERED` 白名单后，两份用例在 IR 侧由「跳过」变为「实跑并通过」 |
| 跨后端一致性 | **36P / 0F** | 不变 |
| `examples/` | **6/7** | 不变，仍缺 **P11** |

**P2 落地后（`structof` / `unionof` / `holdof` / `bitoffsetof` 进 A 后端，2026-09-26 实测复跑）**

| 项目 | 现值 | 说明 |
| --- | --- | --- |
| `xmake test --all` c / native | **各 23P / 0F / 6S** | 各 +2 PASS：新增 `pos/ownerof`（三参反推首地址 + 三档位偏移，输出逐行比对 `.expect`）、`err/structof_bad_member` |
| `xmake test --all` ir-c / ir-native | **各 15P / 0F / 10S** | 两份用例在 IR 侧各落一条 SKIP（`pos` 走「IR 子集未覆盖」、`err` 走「IR 前端暂未覆盖」）——**A 后端已实现、IR 前端仍拒绝**，与 `PA` 的 IR 现态同口径，故刻意**不入** `IR_ERR_COVERED` 白名单 |
| 跨后端一致性 | **36P / 0F** | 不变 |
| `examples/` | **6/7** | 不变（native 逐条复跑），仍缺 **P11** |

**P3 落地后（`alignof` 改由编译期自算并输出字面量，2026-09-26 实测复跑）**

| 项目 | 现值 | 说明 |
| --- | --- | --- |
| `xmake test --all` c / native | **各 24P / 0F / 6S** | 各 +1 PASS：新增 `pos/alignof_builtin`（9 项断言：标量 / `void` 通用指针 / 结构体 / 嵌套结构体 / union / `i32[4]` / `char[8]`，用 `puts` 输出后逐行比对 `.expect`） |
| `xmake test --all` ir-c / ir-native | **各 15P / 0F / 11S** | PASS 不变，SKIP +1——该用例**刻意不入** `IR_SUBSET`：IR 前端按 8 字节槽模型对任何 `alignof` 固定返回 8，`alignof(u8)`/`alignof(i32)` 两条断言必错，与 `PA` 的处置一致 |
| 跨后端一致性 | **36P / 0F** | 不变 |
| `examples/` | **6/7** | 不变（native 逐条复跑），仍缺 **P11** |

**PB-34 落地后（IR 机器码后端裁剪为 riscv64 + loongarch64，2026-09-26 实测复跑；非 PB-33 移植轮，沿用同一复跑口径）**

| 项目 | 现值 | 说明 |
| --- | --- | --- |
| `xmake test --all` c / native | **各 24P / 0F / 6S** | 与 P3 后**逐字相同**（本轮不碰 A 后端） |
| `xmake test --all` ir-c / ir-native | **各 15P / 0F / 11S** | 数值不变，但 **ir-native 的含义变了**：本机 Windows x86-64 无 IR 机器码发射器，该档现在走 IR→C→`tcc`，即门禁里 ir-native 一列与 ir-c 跑的是同一条出码路线（差异只剩 `irparse.c` 之外的 C 落地器）。`xmake.lua` 里原「非 Windows 主机整体跳过 ir-native」段随 x86 发射器一并删除，故 **Linux 侧数值待有环境时复跑补记**（原先该处 ir-native 整档跳过） |
| 跨后端一致性 | **36P / 0F** | 不变 |
| `examples/` | native **6/7**、ir-native **7/7** | ir-native 一档本轮首次逐条量测：含 `06_cooking.nc` 全过 → **P11 的缺口纯粹在 A 后端**，IR 前端早有编译期函数 |

**PB-35 落地后（两档保留发射器首次过外部汇编器，2026-09-27 实测复跑；交叉档不在门禁矩阵，故量测另立一张表）**

| 项目 | 现值 | 说明 |
| --- | --- | --- |
| `xmake test --all` c / native | **各 24P / 0F / 6S** | 与 PB-34 后**逐字相同**（本轮只碰两档交叉发射器与共享骨架，A 后端与 IR 前端不动） |
| `xmake test --all` ir-c / ir-native | **各 15P / 0F / 11S** | 不变（`xmake.lua` 零改动） |
| 跨后端一致性 | **36P / 0F** | 不变 |
| `examples/` | c / native **6/7**、ir-native **7/7** | 复跑确认不变 |
| 交叉档离线量测 | **riscv64 与 loongarch64 各 46/46 过汇编、各 45/46 过链接** | 语料 `tests/pos/*.nc`(47) + `examples/*.nc`(7)，出码 46 份（另 8 份不出码：7 份卡在 IR 前端既有缺口，`pos/ir_slice.nc` 由本轮新增的帧护栏可读地拒绝）。汇编器 = msys2 `riscv64-unknown-elf-as`、zig 0.15.2（LLVM 19 LoongArch MC）；链接 = `riscv64-unknown-elf-gcc -specs=nosys.specs -static`、`zig cc -target loongarch64-linux-gnu`。**两侧唯一的链接失败同为 `pos/use_mod.nc`**（`undefined reference to 'add'`——跨模块符号要等多文件链接才闭合，与发射器无关）。**执行验证做不到**：本机只有 `qemu-system-*`（整机模拟）而无 `qemu-user`，也无可用的 loongarch64 GNU 工具链，「能链接」即本机天花板，语义正确性仍未证 |



**P4 落地后（`is` 无 fallthrough + `while` 体外拒绝、`flow` 存储来源、`flow→flow` 转移豁免与冻结源拒绝，2026-09-27 实测复跑）**

| 项目 | 现值 | 说明 |
| --- | --- | --- |
| `xmake test --all` c / native | **各 29P / 0F / 6S** | 各 +5 PASS：`pos/is_no_fallthrough`、`pos/flow_no_free`、`err/flow_move_frozen`、`err/is_outside_while`、`err/is_in_do_body` 逐行比对 `.expect`；`pos/flow_move.nc` **不占 PASS 位**——`xmake.lua` 的一致性防线只把**无 `.expect`** 的用例收进跨后端比对而不计入 PASS 列，`PA` 侧该用例同样无 `.expect`，照搬不改 |
| `xmake test --all` ir-c / ir-native | **各 15P / 0F / 17S** | PASS 不变、SKIP +6：六份用例**刻意不入** `IR_SUBSET` / `IR_ERR_COVERED`，与 `PA` 的「只修 A 后端、IR 留 2.0」口径一致。逐条实测（2026-09-27，命令级）：`pos/is_no_fallthrough` 在 ir-c **能编译运行但输出与 `.expect` 不符**（每个 `is` 子句打两行，即 IR 前端仍生成并列 `if`、有 fallthrough）；`err/is_outside_while`、`err/is_in_do_body` 在 ir-c 已被拒绝，但文案带前端前缀 `ir: 'is' pattern match only valid inside while loop body`，与 A 后端同句无 `ir: ` 前缀不一致，故不满足共用 `.expect` 的入白名单条件；`pos/flow_no_free`、`pos/flow_move`、`err/flow_move_frozen` 三份在 ir-c **连解析都过不了**（`p.(i32) = v` 形态报 `expected ')', got 'i32'` + `ir: expected '=' or 'op=' after p.()`），属 IR 前端的带类型解引用赋值缺口，记在 `TODO-PB.md` PB-33 表 P4 行的「顺带发现」，本轮不扩大范围 |
| 跨后端一致性 | **37P / 0F** | +1，即新增的 `pos/flow_move`（四后端输出一致） |
| `examples/` | c / native **6/7**、ir-native **7/7** | 复跑确认不变，仍缺 **P11** |

> 数字与 `PA` 分支现值（c/native 56P/0F/4S、ir 8P/0F/49S、examples 7/7）**不可直接对比**：两条线的用例集与 skip 名单不同（`PA` 自 merge-base `7d4c652` 起新增 43 份用例，P1 + P2 + P3 + P4 已带进 11 份、余 32 份未进）。此后每完成一条 PB-33 移植项就复跑本表并在其下追加一行，**数字回落即回归信号**。
>
> 基线复跑同轮做符号级普查，核出两类既有事实（均 2026-09-26 实测，`git grep <symbol> PB -- ncc`）：① `PA` 侧 **PA-51** 的四档数组容量写法本分支**早已实现**（`parser.c:344–365`，2026-09-01 即对齐 IR 前端，`[...]` 取默认容量 8），故 PB-33 表把它从移植项降为**核对项**；② `PA-49` / `PA-50` / `PA-55` / `PA-57` / `PA-58` / `PA-59` 的实现符号 `def_init`、`member_default_text`、`default_init_text`、`cgen_string_lit`、`eat_stmt_terminator`、`is_paren_type_ahead`、`parse_linkas_decl`、`skip_cooking_item`、`ct_capture_body`、`MAX_INIT_DESIGATORS`，以及 `xmake.lua` 的产物 C `-fsyntax-only` 抽查，在本分支**全部零命中**（`ct_funcs` 仅存在于 `irparse.c`，`parser.c` 无）——即 P7 ~ P11 五行的缺口是实测而非推测。

---

## 2.0 线（PB）差异

以下为 PB 与上表（1.0 现态）不同的条目，行号基于 PB `8079ad0`，**2026-09-26 已按 P1 / P2 / P3 三轮改动逐个复核**（`parser.c` 在 1603 行以后因插入 `agg_member_sym` 与两段内置函数体整体后移：`vis_check_call_arg` 调用点 2125→2253、`?.` 拒绝块 2169→2297、`?=` 拒绝 case 2513→2641；`irparse.c` 在 1195 行以后整体 +7；P3 对 `parser.c` 只是**行内**替换（1640、2109 两处分派的右侧表达式），行号未动，`type.c` 在 65 行以后插入 37 行、`ncc.h` 在 389 行加一条原型；**PB-34**（IR 机器码后端裁剪）对 `parser.c` / `type.c` / `irparse.c` 本节所引锚点**零位移**——它只碰 `irparse.c` 尾部（3408 行起新函数与 `ir_compile` 的发码尾段）、`ncc.c` 的 CLI 分派、`ir_backend.{c,h}` / `ir.h` 注册表与两个整文件删除；**P4**（2026-09-27）对 `parser.c` 的位移从 1088 行起算：`no_auto_free` 登记 +9，`parse_is_stmt` +3（只有注释增长，三处子句守卫是行内替换），`parse_statement` 入口 +3、while 脚手架 +5、`do` 体清零 +4、`TOK_IS` 守卫 +7，`parse_postfix` 的 `lhs_bare_ident` +3，`parse_assign` 的重登记 +9（净增 50 行）——即 1173 行以后的锚点按位置逐行重算，位移自 +9 起、文件末尾达 +50。`vis.c` 的 `vis_check_usable()` 内插 4 行转移豁免（其后锚点整体 +4：`vis_check_writable()` 139→143、`vis_check_assign()` 起点 154→158），`vis_check_assign()` 内再插冻结源拒绝与 `moved_src` / `ownership_transferred` 两段共 16 行（该函数终点 198→218，其后累计 +20）；`irparse.c` **零改动**故其锚点全部不变）：

| 条目 | 1.0 线（上表） | 2.0 线（PB）现态 |
|------|---------------|-----------------|
| §12.2 `flow`/`var`/`const` 参数前缀 | 前缀被忽略，统一 `VIS_DEFAULT`（parser.c:864–868、879） | **已实现**：`param->vis = pv` 记录前缀（parser.c:934），调用点 `vis_check_call_arg()` 执行 M2 检查（parser.c:2284）；IR 前端由 `vvis` 状态机等价实现（irparse.c，PB-26） |
| 返回值可见性前缀（§12.3 后半） | 无 | **已实现**：`ret_vis` 调用点检查（parser.c:1114）+ IR 侧 PB-29（含 PB-29.1 六条禁止路径 err 用例） |
| 检查范围表「IR 后端所有权检查」 | ⚠️ 未实现 | **已实现**（PB-26/M2 移植进 irparse.c，err 用例 m2a~m2d 在 ir-c/ir-native 下同样拒绝） |
| `is` 各模式行号 | 见「`is` 模式匹配」表（本分支锚点） | 本分支 `parse_is_stmt` parser.c:1203–1270（P4 改的是子句守卫与 while 脚手架，函数边界未动）：`_` 通配符 1213–1222、共用守卫头 1224、`invalid 'is' pattern` 1262、`expected block after 'is' pattern` 1270；`PA` 同函数在其 parser.c:1625 起。`is` 块由 irparse.c:2241 起的 `_` 分支处理 |
| `is _` 通配符 | v1.0.2 补全（PA-15） | PB-27.5 先于 1.0 线完成（parser.c + irparse.c 双前端） |
| 反向范围 `lo > hi` 校验 | ⚠️ 未实现 | **IR 前端已实现**（irparse.c:2266–2268 编译期报错）；A 后端 `parser.c` 仍不校验 |
| `__is_val` 类型 | `int` 固定 | **类型感知**，等于 `while` 条件表达式类型（PB-27.7） |
| 循环体外使用 `is` | A 后端按 `if` 展开，不报错 | **两侧都拒绝**（P4 起）：A 后端 parser.c:1451–1460 报 `'is' pattern match only valid inside while loop body`，IR 前端同句带 `ir: ` 前缀（PB-27.1）。`do` 体亦拒绝——A 后端 parser.c:1341–1345 进体前清零 `while_depth` |
| 多个 `is-clause` 无 fallthrough | ⚠️ 未实现（PA-16 登记待决策） | **A 后端已实现**（P4 移植 `PA` 的 PA-16 决策「只修 A 后端、IR 留 2.0」：`__is_matched` 守卫，见「`is` 模式匹配」表）；**IR 前端仍未实现**，实测 ir-c 下 `pos/is_no_fallthrough` 每个子句打两行输出 |
| `flow` 存储来源与转移豁免（§11.1 / §14.2） | 1.0 表只写「块退出自动 free」与 `ownership_transferred` | **P4 补齐三条**：① `no_auto_free`（ncc.h:127）让字符串字面量 / `&x` / `{…}` 初值的 `flow` 不被 free（改前实测 rc=127 静默崩溃）；② `moved_src`（ncc.h:228）豁免转移那条语句内对失效源的读取（改前实测两条误报 `'a' is invalidated`）；③ 冻结源转交所有权改为拒绝（vis.c:196–200）。IR 侧 ①③ 无用武之地（irparse.c 不做自动释放与冻结检查，见「检查范围」表），②的等价物在 IR 前端亦未实现 |
| `?=` 安全赋值 / `?.` 安全解引用 | 1.0 线已移除（BNF v2.3 / PA-20、PA-21），`token.h` 保留 token | **2026-09-26 起与本分支设计层归一（PB-33 的 P1）**：语法层拒绝——A 后端 parser.c:2676–2682（`'?='`，诊断后 fall through 到普通 `=`）、parser.c:2328–2339（`'?.'`，诊断后补吞 `(` 按 `.()` 展开，顺带删去失效的 `(void)op;`）、is_expr_continuer 的 parser.c:492/498 两处判据去掉 SAFE_*；IR 前端 irparse.c:1195–1201 的 `ir_primary` 补同文案拒绝分支。`TOK_SAFE_ASSIGN` / `TOK_SAFE_DOT` 仍留词法层，用例 `err/safe_assign_removed` / `err/safe_dot_removed` 四后端一致拒绝（文案与 `PA` 逐字相同；恢复路径两侧略异，同一 `?.` 用例 `PA` 的 c 后端多 1 条级联、本分支只报 1 条） |
| 从属查询与位域偏移内置函数（§2.3，BNF v2.4） | `PA` 侧同文件该节标 ✅ 已实现（v1.0.2 / PA-22，仅 c/native；IR 前端未实现）；**本文件此前无该节** | **2026-09-26 随 PB-33 的 P2 落到本分支 A 后端**：`parse_builtin_kw` 新增 `structof`/`unionof`/`holdof(Type, member, ptr)` → `((void*)((char*)(ptr) - offsetof(Type, member)))`（parser.c:1713–1750；`structof` 只认 struct、`unionof` 只认 union、`holdof` 通用；成员不存在 / 类型非聚合体 / 缺第三参各有专属诊断）与 `bitoffsetof(Type, member)` → 按声明序折成编译期常量表达式（parser.c:1752–1830；以前置非位域成员为锚点，用 C 的 `offsetof`/`sizeof` 定位存储单元起点，目标非位域、基类型宽度未知、同组存储类型不一致均报错）；成员查询助手 `agg_member_sym()` 在 parser.c:1634，`parse_primary` 的分派 case 在 parser.c:2066。**IR 前端仍拒绝**（`ir: unexpected token 'structof' in expression`，与 `PA` 的 IR 现态逐字相同）——8 字节槽模型无真实布局，属模型边界而非移植缺口。用例 `pos/ownerof.nc` / `err/structof_bad_member.nc` 自 `PA` 带来，c/native PASS、IR 侧 SKIP |
| `alignof(T)` 类型对齐（§2.3，BNF `<builtin-call>`） | `PA` 侧同文件该节标 ✅ 已实现（v1.0.2 / PA-24：编译期自算后只输出整数字面量）；**本文件此前无该节** | **2026-09-26 随 PB-33 的 P3 落到本分支 A 后端**：新增 `type_align()`（type.c:71–102，原型 ncc.h:389），编译期递归求对齐——数组/别名经 `ref` 取元素对齐、`struct`/`union` 取最宽**非位域**成员（递归深度上限 16）、`void` 按 §5.1 通用指针计 8、其余取 `CType.align` 为 0 时回落 `type_default_align(kind)`；`parser.c` 两处分派由 `cgen_raw("_Alignof(%s)", …)` 改为 `cgen_raw("%u", type_align(&tmp))`（parser.c:1671 关键字路径、2109 标识符路径；后者按 `PB-6` 已记录的既有事实**实际不可达**——`alignof` 是关键字 token，改它只为与 `PA` 同源，不新增覆盖主张）。改前实测：`c` / `native` 链接期 `tcc: error: undefined symbol '_Alignof'`，`ir-c` 因固定返 8 而九项断言全错。**IR 前端未改**（`irparse.c:909` 仍按槽模型固定返回 8，与 `PA` 的 IR 现态一致）。用例 `pos/alignof_builtin.nc` 自 `PA` 原样带来（含其注释里的 PA-24 溯源，可与 PB-33 表 P3 行对号），**未列入** `IR_SUBSET` |
| IR 机器码后端档位（B 方案出码面） | c / native 之外的四档发射器（x86-64/riscv64/arm64/loongarch64），`ir-native` 硬绑 x86-64 | **2026-09-27（PB-34，ltree 裁定）起本分支只留 riscv64 + loongarch64 两档发射器**：`ir_x86_64.c`（518 行）与 `ir_arm64.c`（355 行）整文件删除，注册表 `ir_backend.c:49–52` 只剩两项，`ir_backend.h` 的两条 extern 与 `ir.h` 的 `irgen_native_emit` 声明一并摘掉。`ir_compile` 发码尾段改按宿主分派——新 `ir_host_asm_backend()`（irparse.c:3406–3417，靠 `__riscv`/`__loongarch_lp64` 判定）非空时出 `.s` 交 tcc 汇编，为空时（含本机 Windows x86-64）走 `irgen_c_emit` 那条既有尾巴即 **IR→C→宿主 cc**（irparse.c:3453–3473，`-v` 打一行回退说明）；`ir-riscv64` 仍编号 4，`ir-loongarch64` 由 6 改 5。CLI 的 `-backend=<be>` 与 `-backend <be>` 两写法改共用 `set_backend()`（ncc.c:232–259，调用点 314、318），六档全收，已删的 `ir-arm64`/`ir-x86_64` 给专属诊断而非 `unknown backend`；`xmake.lua` 里「非 Windows 主机整体跳过 ir-native」段（起因是 x86 发射器的 ELF 在 Linux 运行时 segfault，2026-08-31 WSL 实测）随发射器一并删除。**门禁四档数值与裁剪前逐字一致**（24P / 24P / 15P / 15P，一致性 36P/0F），但本机 ir-native 一档已等价 IR→C；`examples/` 本轮逐条量到 ir-native **7/7** 而 c/native **6/7**，即 **P11** 的 cooking 缺口纯粹在 A 后端。`PA`（1.0 冻结线）四档发射器**未动**。**2026-09-27（跨零点）PB-35 续**：两档产物首次过真汇编器，暴露 loongarch64 发射器**此前 0/34 能汇编**（AT&T 式 `-8($s0)` 访存、`xor.d`/`slt.d`/`sltui r,r,r`、`mov.d`、`fcmp.eq.d` 一类写法在 GAS 与 LLVM 里都不存在），已按 LoongArch64 正确形态重写（`la_slot()` ir_loongarch64.c:31–35 出基址在前的三操作数、`IR_ITOD`/`IR_DTOI` 走 `movgr2fr.d`+`ffint.d.l` / `ftintrz.l.d`+`movfr2gr.d` 两步、`IR_FCMP` 走 `fcmp.<cnd>.d $fcc0`+`movcf2gr`+`andi`（ir_loongarch64.c:86–100 与 134–156）、整数比较去 `.d` 后缀（174 附近）），文件头注释记下这些语法事实。两档共有的另一条是**帧越界**：槽位靠帧指针 12 位带符号立即数寻址，`pos/ir_slice.nc` 实测帧 2208 字节 → `addi sp,sp,-2208` / `sd a0,-2056(s0)` 全被拒绝。解法是集中式护栏而非各发射器自查——`TargetBackend` 新增 `max_frame`（ir_backend.h:48–50，两档均填 2047：ir_riscv64.c:348、ir_loongarch64.c:376），`irgen_backend_emit` 在算完帧大小处先拒后发（ir_backend.c:106–115，诊断含函数名 / 实测帧 / 上限 / 后端名）。改后量测见上表 |

| 测试覆盖 | 1.0 门禁 12P/0F/5S | `xmake test --all` 全矩阵（c/native/ir-c/ir-native），见 `ROADMAP.md` 里程碑「验收」行 |
