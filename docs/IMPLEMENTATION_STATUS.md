# NihaoC 规范与编译器实现对应关系

本文档记录 NihaoC 语言规范（Chinese.md / English.md）中各项指针安全规则与编译器（ncc）实际实现的对应状态。

> 更新日期：2026-10-01（**PB-33 的 P7 落地**：`PA` 侧 **PA-57** 语法标准对齐轮的剩余合规项——语句终止符 `;` / `#` 改为包装层（新 `eat_stmt_terminator`，旧的一次性大函数改名 `parse_statement_impl`）、`(T)` 括号类型（`parse_type` 新档 + 新增有限前瞻 `peek_ahead` / `is_paren_type_ahead` + 四处分派）、`for` 头带类型声明的 init、一元 `+` 仅作字面量前缀、`use a.b.c` 点号多段与顶层 `use` / `link` / 终止符交错、`is` 整数模式两端可带符号 + 空区间编译期拒绝，以及**在册清单外的一条必要修正** `cgen_string_lit()`（字面量转义归一，原计划留 P9，`pos/literal_forms` 复测不过逼出，同时让 F2 / PA-50 在本分支结案）。**动手前逐形态复测**：四档数组容量（P5 核对项）、`void` 的 `pointee_type`（P5b）、`[[export "…"]]` 字符串写法在本分支**早已成立**，不是移植项；designator 初始化（PA-57⑥）连基础形态都没有，整条留 **P8**；`linkas` 不随本轮，留 **P10**。带来 7 份用例，并修正 `pos/ir_struct.nc` 的**用例自身错误**（原为 `PA` 的 PA-57 之前版本，打印 `struct copy bad`，因无 `.expect` 只进一致性档而长期静默 PASS）。新增「语句终止符与语法形态合规」一节。`parser.c` 3162→3292 行（净增 130），位移**非单调**（`parse_is_stmt` 整段替换、`parse_module` 两段循环合成一段），故本文件的 `parser.c` 锚点全部按新行号逐个 grep 复核，顺带改正一处既有失准锚点（`alignof` 标识符路径旧记 2357，实为现 2541；2357/2488 一直是 `vis_check_usable()`）。`cgen.c` 在 `cgen_truncate()` 后插 43 行（其后 `c_type_suffix` 253–269→296–312），`ncc.h` 在 371 行加一条原型（其后 `type_align` 421→422），`lexer.c` 的 `parse_number` 整数收尾 +5（261–266），`module.c` 的模块候选路径 +10（81–99）；`vis.c` / `type.c` / `irparse.c` 本轮零改动）
> 2026-10-01（**PB-33 的 P5b 落地**：切片读写链路 + `.(T)` 通用后缀链 + `PA-55` 的 `void` 槽静态还原。A 后端把解引用折进 `parse_postfix` 的通用循环、**删除**独立的 `parse_deref_chain`，并按 `PA` 现态重写声明体（先解析右值、后出声明头，声明头四档成形：函数指针 / 推断切片 `T *name` / 数组退化 `T (*name)[..]` / 普通）。新增「指针后缀链与切片」一节。同轮为使 `pos/slice_str_infer` 复测通过，把 `infer_init_type()` 的标识符分支（调用取返回类型、成员访问取成员类型、聚合体保留标签符号）一并移植——属在册清单外的必要修复，口径同 P5 的 `c_type_suffix` 先例。`parser.c` 2874→3162 行（净增 288），本轮位移**非单调**（解引用链整段替换使中段先增后减），故本文件的 `parser.c` 锚点不靠位移推算，全部按新行号逐个 grep 复核，并同步改正三处自 1.0 冻结线后从未重算的既有失准锚点。`ncc.h` 加 `pointee_bytes` / `pointee_type`（142 / 145）与七个 ParserState 字段（256–262），`cgen.c` 只把 `cgen_truncate()` 换成按残留文本重算行首缩进状态的版本。`vis.c` / `type.c` / `irparse.c` 本轮零改动）
> 2026-10-01（**PB-33 的 P6 落地**：编译器版本号改由 `xmake.lua` 构建期注入做单一真源——本分支原为**三套值并存**（`set_version("1.0.0")`、`ncc.h` 的 `0.1.0`、`ncc.c` 里 `nihao.toml` 脚手架再硬编码一次），现 `--version`、帮助横幅、verbose 启动行与脚手架版本全部派生自顶部那一处；`ncc.h` 在第 25 行处以 10 行替 2 行，**其后锚点整体 +8**，本文件五处 `ncc.h` 行号（`no_auto_free` / `len_known` 注释块 / `moved_src` / `while_depth` / `type_align` 原型）已按实测逐个复核）
> 2026-09-28（**PB-33 的 P5 落地**：数组与字符串初值口径的前端诊断与 `len()` 内置函数——定长 `char[n]` 配字面量的超容量/非 `char` 元素检查、多变量声明保留数组后缀、`len(x)` 逻辑长度登记与撤销、通用 `void` 指针裸下标拒绝、`c_type_suffix()` 改逐维输出，均由 `PA` 的 PA-25（第 ⑦ 项与 `void` 拒绝半段）/ PA-27 / PA-31 / PA-34 移植进本分支 A 后端；新增「数组与字符串初值口径」一节，`PA` 侧同主题的「指针后缀链与切片」整节（切片读写、数组退化、`.(T)` 通用后缀链、PA-55 静态还原）**未随本轮回灌**，登记为 **P5b**。**本轮 `parser.c` 全部锚点按实测重算**（净增 130 行；分段位移：528 行起 +20、753 起 +22、765 起 +25、775 起 +28、796 起 +47、806 起 +56、1083 起 +57、1087 起 +75、1143 起 +89、2123 起 +111、2439 起 +118、2461 起 +126、2684 起 +130；800–806 区间的两处行内替换在 ±1 内相互抵消，不单列）。`ncc.h` 133 行以后 +4（`moved_src` 228→232、`while_depth` 296→300），并改正一处既有失准锚点：`type_align` 原型由旧值 389 改为实测 401。`cgen.c` 只改 `c_type_suffix()` 一处（244–263），`vis.c` / `irparse.c` 本轮零改动故其锚点不动。上一轮：PB-33 的 P4（`is` 无 fallthrough + `flow` 存储来源与转移豁免，`PA` 的 PA-16 / PA-32 / PA-33）。本表主体以 **1.0 冻结线（v1.0.2）** 的现态书写，行号基于 **本（`PB`）分支** 的 `parser.c` / `irparse.c`（`vis.c` 两侧同源，行号与 `PA` 一致），仅作导航用。`PA`/`main` 的行号口径见 `PA` 分支同文件。
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
| `flow` 参数接收所有权 | 参数前缀被忽略 | parser.c:1149–1153、1164 | ⚠️ 未实现 |
| `var` 参数冻结实参 | 参数前缀被忽略 | parser.c:1149–1153、1164 | ⚠️ 未实现 |
| `const` 参数冻结实参 | 参数前缀被忽略 | parser.c:1149–1153、1164 | ⚠️ 未实现 |

> 当前编译器解析函数参数上的属性前缀（`flow`/`var`/`const`），但在内部将所有参数统一视为 `var`（`VIS_DEFAULT`）。参数的所有权/借用检查不在 1.0 范围内，已由 2.0（PB 分支 PB-26）实现并验证，1.0 冻结线不移植。

---

## 检查范围

| 规范要求 | 编译器实现 | 对应代码 | 状态 |
|---------|----------|---------|------|
| 裸标识符赋值检查 | 完整实现 | `vis_check_assign()`（vis.c:158–218）的两处调用：parser.c:1358–1362（声明初始化）、parser.c:3234–3246（表达式赋值） | ✅ 已实现 |
| 表达式级检查 | 仅检查裸标识符 | 读侧：`vis_check_usable()` 只在 parser.c:2488 的裸标识符路径调用一处——P5b 删掉独立的解引用链解析后，链上不再重复这一调用（可读性检查归主元解析）；写侧：赋值右值须先过 `is_expr_continuer` 的裸标识符判定（parser.c:3234–3246），`x = y + 1` / `x = f()` 均不进矩阵 | ⚠️ 部分实现 |
| 整变量重绑定的作用域界定 | `lhs_bare_ident` 区分「改变量本身」与「改所指对象」，只有前者重登记存储来源 | 置位 parser.c:2985（`parse_postfix` 尾，`lhs_bare_ident = !chained`）、解引用侧另置 `lhs_was_deref`（parser.c:2810），两者都在表达式解析结束时统一复位（parser.c:3286–3290）；消费 parser.c:3168（撤销 `len` 记录）、3175–3178（撤销所指记录）、3249–3254（重登记 `no_auto_free`） | ✅ 已实现（PB-33 P4；P5b 把解引用链的单独清零并入统一复位） |
| IR 后端所有权检查 | 无 | irparse.c | ⚠️ 未实现（2.0 范围，PB-26/PB-29 已覆盖） |

> `vis_check_assign()` 仅在赋值右侧为**单个裸标识符**时触发。`x = y + 1` 或 `x = malloc(...)` 等表达式形式的赋值不经过传递矩阵检查。IR 后端（irparse.c）仅记录可见性值用于 `visof()` 查询，不执行所有权/借用检查。
>
> P4 起 `flow→flow` 自转移不再误报 use-after-move：`cs->parser.moved_src`（ncc.h:245）在转移发生的那条语句内豁免失效源的可读性检查（`vis_check_usable()` vis.c:127–140 的提前返回），下一条语句进入 `parse_statement()` 开头即清除（parser.c:1622）；同时把源符号标上 `ownership_transferred`（vis.c:210）让退出时的自动释放跳过它，避免双重释放。转移的源若正被 `const`/`var` 借用（`BS_FROZEN`），改为拒绝并报 `cannot transfer ownership of 'x': it is borrowed (frozen) by an active const/var`（vis.c:196–203）。

---

## `is` 模式匹配（§6.3，规范标题自 PA-42 起编号；BNF v2.2）

| 规范要求 | 编译器实现 | 对应代码 | 状态 |
|---------|----------|---------|------|
| `is` 仅配合 `while`（块形式） | `parse_is_stmt()` 只在 while 体内调用；`cs->while_depth`（ncc.h:320）为 0 时 `TOK_IS` 分支先拒绝再按块展开 | `parse_is_stmt` parser.c:1528–1615；`TOK_IS` 守卫 parser.c:1813–1822；`while_depth` 增减 parser.c:1669/1671 | ✅ 已实现（P4 补拒绝路径） |
| `do` 不支持 `is` | `TOK_DO` 走 `parse_statement` 通用体，进体前把 `while_depth` 存零、出体恢复（P4） | parser.c:1685–1688（旧口径是"隐式不识别"，现改为显式清零后 `is` 会被 `TOK_IS` 守卫拒绝） | ✅ 已实现 |
| `is <int-literal>` / `is -<int>` / `is +<int>` | `== v` / `== -v` / `== +v`（前导 `+` 折叠进值，C 侧不再发 `+`） | 带符号前缀 parser.c:1552–1557，`-` 后非整数即报 `invalid 'is' pattern: '-' must be followed by an integer`（1558–1560）；取值 1562–1563、公式 `== lo` 在 1585 | ✅ 已实现（PB-33 P7 按 `PA` 的 PA-57 重写为带符号档；用例 `pos/is_pattern_forms`） |
| `is lo..hi` 闭区间，两端各可带符号 | `>= lo && __is_val <= hi` | parser.c:1565–1583（比较式在 1582；上界非整数报 `invalid 'is' range: expected an integer upper bound`，1572–1573） | ✅ 已实现（P7 补上界符号与前缀校验） |
| `is <visibility-enum>` | 比较 `NH_*` 常量 | 关键字 token 分支 parser.c:1596–1603；`_flow` 等标识符写法分支 parser.c:1587–1593 | ✅ 已实现 |
| `is <enum-variant>` / 已知常量 | `TOK_IDENTIFIER` 分支按值比较 `== pat` | parser.c:1587–1595（兜底 `else cgen_raw(" == %s", pat)` 在 1594） | ✅ 已实现 |
| `is _` 通配符恒匹配 | 恒真分支 `(1)`（IR 侧不发比较与 JZ） | parser.c:1538–1547（恒真式在 1541）、irparse.c:2036–2039 | ✅ 已实现（v1.0.2 补全） |
| `is <identifier>` 按值比较（BNF v2.11 定案，绑定/解构留 2.0） | 生成 `__is_val == pat`，不引入新绑定 | parser.c:1587–1595 | ✅ 与规范一致；变量绑定属 2.0（PB-27 类型感知后再议） |
| 多个 `is-clause` 首个匹配即止（无 fallthrough） | 每子句守卫 `if (!__is_matched && … && (__is_matched = 1))`，置位写在条件里（块体由 `parse_statement` 整体输出，无法在其 `{` 后插语句）；`__is_matched` 在 while 脚手架声明并每轮迭代重置 | 守卫 parser.c:1541 / 1549 / 1608，声明 parser.c:1658，每轮重置 parser.c:1668；**IR 前端未实现**（仍并列 `if` / 独立比较跳转，irparse.c:2147–2151，与 `PA` 同口径留 2.0） | ✅ 已实现（PB-33 P4 移植 `PA` 的 PA-16；用例 `pos/is_no_fallthrough`、`err/is_outside_while`、`err/is_in_do_body`） |
| `is <pat> => <stmt>` 单语句 | 已移除（BNF v2.2 / PA-13），双前端仅接受块形式 | parser.c:1610–1613（非 `{` 即报 `expected '{' after 'is' pattern`——P7 起文案与 `PA` 逐字相同，旧写法无用例依赖）、irparse.c:2147–2151 | ✅ 已按规范移除 |
| 反向范围 `lo > hi` 编译期校验 | 取完两端后比较 `lo > hi` 并报 `empty 'is' range %lld..%lld: lo must be <= hi`，不再静默生成永不匹配的分支 | parser.c:1578–1581 | ✅ 已实现（PB-33 P7 移植 `PA` 的 PA-57；用例 `err/is_empty_range`。IR 早有同类检查但文案不同，见下方差异表） |
| 结构体解构 / ADT 变体解构 | 预留语法，未实现 | — | ⚠️ 预留（依赖类型系统，PB-27.10/11） |

---

## 存储期属性（§11.1）

| 规范要求 | 编译器实现 | 对应代码 | 状态 |
|---------|----------|---------|------|
| `const` 模块级静态 / 块级自动 | C 后端自然实现 | parser.c:1265–1266（`is_const` / `is_static` 由声明前缀决定） | ✅ 隐式实现 |
| `flow` 动态分配 + 自动释放 | 函数退出与块退出两处 free 循环 | parser.c:1494–1501（函数级 locals）、parser.c:1891–1902（块级 `scope_start..` 段） | ✅ 已实现 |
| `flow` 初值/重绑定的存储来源 | `no_auto_free`（ncc.h:135）：右值首 token 是字符串字面量 / `&x` / `{` 时不登记堆所有权，退出时不 free | 声明点 parser.c:1309–1313、整变量重绑定 parser.c:3247–3255，两处消费点即上两行 free 循环的条件 | ✅ 已实现（PB-33 P4；用例 `pos/flow_no_free`） |
| `flow` 返回值所有权转移 | `ownership_transferred` 标志 | parser.c:1787–1795（return 路径置位）、vis.c:206–211（`flow→flow` 转移时置位） | ✅ 已实现 |
| `static` 静态存储 | C `static` 关键字 | cgen | ✅ 已实现 |

> `const` 的存储期区分（模块级静态 / 块级自动）由 C 后端编译器的自然语义实现——ncc 不显式区分两种 `const`，但生成的 C 代码中，文件作用域 `const` 自然获得静态存储期，块作用域 `const` 自然获得自动存储期。`vis_check_transfer()` 对两种 `const` 一视同仁。

---

## 数组与字符串初值口径（§5.1.1 / §4.2 / §2.3，自 PB-33 的 P5 起）

> 本节随 **PB-33 的 P5**（2026-09-28）建立，只覆盖已回灌的**数组容量诊断与 `len()` 登记**。`PA` 侧同主题还有一整节「指针后缀链与切片」（切片读 / 切片写 / 数组退化 / `.(T)` 处于后缀链任意位置 / `PA-55` 的 `void` 槽静态还原）**已于 2026-10-01 随 P5b 回灌**，见下文「指针后缀链与切片」一节。

| 规范要求 | 编译器实现 | 对应代码 | 状态 |
|---------|----------|---------|------|
| 定长字符数组的字符串初值 `s char[n] = "abc"` | 按声明容量保留**数组存储**（`char s[9] = "abc";`，不退化为指针）；`strlen+1 > n` 报 `string needs N bytes with terminator, array 's' holds M`，元素非 `char` 报 `cannot initialize array 's' with a string literal; its elements are not 'char', use a value list {v0, v1, ...}`，两条诊断都在消费 `'='` 之后、输出 ` = ` 之前发出 | 数组存储声明 parser.c:1419–1421（声明头的普通分支，即既有 `c_type_suffix` 调用点）、`str_lit_len` 采集 parser.c:1325–1326、两条诊断 parser.c:1371–1380 | ✅ 已实现（PB-33 P5 移植 `PA` 的 PA-31，c/native）。**IR 前端两条诊断均无**（`irparse.c` 不做容量检查），用例不入 `IR_SUBSET` / `IR_ERR_COVERED` |
| 四档数组容量写法 `[N]` / `[N...]` / `[...]` / `[...N]` | 本分支 2026-09-01 即与 IR 前端一致（省略容量取默认容量 8），P5 只做**量测复核**、代码零改动；`len()` 取声明容量 | 容量解析 parser.c:410–449 | ✅ 已实现（`PA` 的 **PA-51** 在本分支为核对项而非移植项；用例 `pos/array_capacity_tiers.nc` 逐档验证容量与 `len()`） |
| 多维数组声明 `T[a][b]` | `c_type_suffix()` 由「只输出最外一维」改为**逐维输出**（`int32_t m[2][3]`）：只发一维会让 `i32[2][3]` 出成 `int m[2]`，而 `len(m)` 按 §2.3 报各维乘积 6，两者不一致即是错码而非错诊断 | cgen.c:296–312（`c_type_suffix`）、parser.c:624（`type_array_count` 求各维乘积） | ✅ 已实现（PB-33 P5，即 `PA` 的 PA-25 第 ⑦ 项）；PA-25 ①–⑥（通用后缀链与切片）已随 **P5b** 落地，见下文同名一节 |
| 多变量声明带数组类型 `var {aa = "aa", …} char[3]` | 改前该分支只发 `c_type_name` + 变量名，数组后缀被静默丢弃（生成 `char aa = "aa";`）；改后与单变量分支同口径：定长数组保留数组存储并登记声明容量，动态 `char[]` 仍退化为 `char*` 并登记字面量长度；三条诊断（初值不是字符串字面量 / 元素非 `char` / 超容量），后两条与单变量共用文案 | parser.c:975（`arr_cap` 求值）、995–1012（三条诊断）、1013–1015（接 `c_type_suffix`）、1030–1038（登记 `len_known`） | ✅ 已实现（PB-33 P5 移植 `PA` 的 PA-34，c/native）。**IR 前端不支持该声明形态**，用例不入白名单 |
| `len(x)` 逻辑长度 | 编译期常量：定长数组=各维元素个数乘积，动态字符串 `char[]` / 推断字符串=字面量长度；长度在声明处登记到符号表，整变量重新赋值（LHS 为裸标识符）即撤销记录，让后续 `len()` 保守报错而非返回过期值；实参非标识符、或静态不可知时即时报错（`len expects an identifier` / `len: logical length of 'n' is not statically known`，后者另出 `0` 兜底以免产物 C 空表达式）。**切片变量 `hi-lo` 一档随 P5b 落地**：切片读两侧同为字面量时把 `hi-lo` 记进 `slice_len`（parser.c:2902–2905、2932–2933），声明处据此登记（parser.c:1429–1431） | 求值与报错 parser.c:2502–2522（`parse_primary` 的标识符内置函数表，置于 `sizeof` 之前）、登记 parser.c:1428–1441（单变量）/ 1030–1038（多变量）、撤销 parser.c:3166–3169（`parse_assign`）、字段 ncc.h:146–149（`len_known` / `logical_len`）与 256–262（ParserState 的 `slice_len_known` / `slice_len`） | ✅ 已实现（PB-33 P5 移植 `PA` 的 PA-27 的标识符与数组/字符串部分 + P5b 补切片档，c/native）。**IR 前端 `len()` 走 `ve[]` 表**，静态不可知时返回 0 而非报错，故 `err/len_unknown` 刻意不入 `IR_ERR_COVERED`（IR 侧 SKIP） |
| 通用 `void` 指针裸下标 `p[i]` | 元素宽度静态未知，裸下标无法翻成合法 C（改前直接产出 `p[i]` 交给 tcc，报语法错误）。改后 `parse_postfix` 在主元解析**之前**记下裸标识符符号（`base_sym`，因为宽度信息只在「前缀仍是裸标识符」时可知），`[` 分支在**非链式**且该符号类型为 `TYPE_VOID` 时报 `cannot subscript the generic 'void' pointer 'p'; write p.(T)[i] to fix the element type first` | `base_sym` 采集 parser.c:2842–2846、拒绝 parser.c:2883–2889 | ✅ 已实现（拒绝半段来自 PB-33 P5 移植 `PA` 的 PA-25，`PA-55` 的「声明处静态还原所指类型」与 `.(T)` 通用后缀链由 **P5b** 补齐，见下文「指针后缀链与切片」一节）。`err/void_subscript.nc` 覆盖拒绝，`pos/void_slot_infer`、`err/void_member_unknown` 随 P5b 带来 |

---

## 指针后缀链与切片（§5.1，BNF v2.5 / v2.7，自 PB-33 的 P5b 起）

> 本节由 **PB-33 的 P5b**（2026-10-01）建立，移植 `PA` 侧同名整节（PA-25 ①–⑥ / PA-26 / PA-27 / PA-55）。P5 时解引用仍走独立的 `parse_deref_chain`，本轮把它折进 `parse_postfix` 通用循环并**删除该函数**，同时按 `PA` 现态重写声明体（先解析右值、后出声明头）。`irparse.c` 本轮零改动，下表全部只在 c / native 生效。

| 规范要求 | 编译器实现 | 对应代码 | 状态 |
|---------|----------|---------|------|
| `.(T)` 可处于后缀链任意位置，其后继续下标 / 成员 / 切片 | 通用 postfix 循环：每个算子把「已生成的前缀 C 文本」整体包成 `(*(<T>*)(prefix))` 再续链；前缀经 `cgen_mark` / `cgen_slice` / `cgen_truncate` 回取，缩进按 `%*s` 重发 | parser.c:2744–2756（`cgen_take_prefix`）、2779–2804（`deref_prefix`）、2807–2833（`deref_step`）、2836–2986（`parse_postfix`）；回退后须按残留文本重算行首缩进状态（cgen.c:124–130） | ✅ 已实现（P5b 移植 `PA` 的 PA-25 ①–⑥，`parse_deref_chain` 已删除）。**IR 前端无对应路径**，属 2.0 |
| 数组类型化解引用 `p.(char[9])[i]` | 数组 cast 生成 `char (*)[9]` 而非 `char**`，下标按整数组步长推进；NihaoC 的 `void` 槽位实为指针，故元素类型 `void` 在 C 里渲染成 `void*` | parser.c:2761–2775（`c_cast_name`） | ✅ 已实现（PA-25） |
| `.()` 越界检查（§12.1） | 读取宽度超过指针当前所指对象的**静态字节数**即报 `'.(T)' reads N bytes but the pointer target holds only M bytes`；前缀不是裸标识符（链式 / 下标之后）时字节数不可静态得知，保守不检查 | 检查 parser.c:2819–2831、字段 ncc.h:142（`pointee_bytes`）、记录 parser.c:828–835（`record_init_pointee`）、撤销 parser.c:3173–3178 | ✅ 已实现（PA-25；用例 `err/deref_bounds`） |
| 空下标 `p[]` = 省略类型的 `p.()`（一层解引用） | `[` 紧跟 `]` 时走 `deref_prefix`；多级链由此统一写作 `p[][].(i32)` | parser.c:2878–2881 | ✅ 已实现（PA-25） |
| 通用 `void` 槽的静态还原（`PA-55`） | 声明处由 `malloc(T)` / `&x` 定下所指类型，省略类型的裸 `.()` / `p[]` / `q->m` 据此还原成 `(*(T*)(p))`；所指类型推不出时 `->m` 与带索引下标各给专属报错，裸 `.()` 才退回 `void**` 读回指针值 | 来源登记 parser.c:1319–1323（`&x` 前瞻：`peek_ahead_ident` parser.c:129 + `ctype_persist` parser.c:140）、828–835、2595–2637（`malloc(T)` 的字节数与元素类型）；还原 parser.c:2793–2799（`deref_prefix` 的 `void` 分支）、2949–2971（`->` 分支）；字段 ncc.h:143–145 | ✅ 已实现（P5b；用例 `pos/void_slot_infer`、`err/void_member_unknown`；带索引下标 `p[i]` / 切片 `p[a..b]` 在 `void` 槽上仍拒绝，口径见「数组与字符串初值口径」表末行） |
| 切片读 `p[a..b]` | 生成「指向第 a 号元素的指针」`&p[a]`，长度不进入值本身（C 无切片类型）；上下界同为字面量时另记录 `hi-lo` 供 `len(切片变量)` 取用；`[..b]` 省略起点等价 `[0..b]`，长度记为 `b` | parser.c:2890–2905（`[..b]`）、2911–2933（`[a..b]`）、2902–2905 与 2932–2933（`rhs_was_slice` / `slice_lmark` / `slice_len_known` 登记） | ✅ 已实现（PA-25 / PA-27；用例 `pos/deref_slice`、`pos/len_builtin`） |
| 切片读赋给固定数组 `T[n] x = p[a..b]` | 按**声明容量**逐元素复制（`memcpy`），非别名 | parser.c:1445–1451（复制分支）、1381–1387（先解析右值并回取其文本） | ✅ 已实现（PA-25） |
| 切片写 `p[a..b] = {v0, v1, …}` 与 `p[a..b] = "abc"` | 值列表逐元素写回（逗号表达式，一条 C 语句）；字符串右值按**字节**整段 `memcpy`（含结尾 `\0`），闭区间右界 `b` 即最后一个可写字节，`strlen > b-a` 时报 `string needs N bytes with terminator, slice holds M`；其余右值形态专属报错 | parser.c:3180–3230（3186–3204 为字符串分支；3180 的 `slice_lmark == lmark` 门控保证只有「赋值左侧整体就是这次切片读」才走写回） | ✅ 已实现（PA-25 / PA-26；用例 `err/slice_str_overflow`）。字符串右值的转义归一随 **P7** 落地：该分支现改用 `cgen_string_lit()` 输出（parser.c:3198），口径见下文「语句终止符与语法形态合规」一节 |
| `T[n] x = malloc(T[n])` 声明退化为指针 | 生成 `T (*x)…`（仅省略最前维度：`void[4][5]` → `void* (*x)[5]`），`malloc` 字节数按元素个数乘算 | parser.c:691–705（`c_decay_suffix`）、1412–1417（退化声明头）、2595–2637（字节数乘算） | ✅ 已实现（PA-25） |
| 切片变量 `s = a[lo..hi]`（类型推断声明） | 声明后退化为**元素指针** `E *s = &a[lo]`（视图，不复制），`s.()` 取首元素，`len(s)` 取 `hi-lo` | parser.c:1406–1411（推断切片头）、1429–1431（长度登记）、2789–2792（数组槽裸 `.()` 按最深元素解引用） | ✅ 已实现（PA-27） |
| 推断声明 `v = s.m` / `v = f(a)` 的取值 | 右值为成员访问时取**成员**类型（数组成员按退化处理成 `T*`）、为调用时取被调（或函数指针）的返回类型；聚合体变量本身作右值时保留标签符号，避免把变量名当类型名输出 | parser.c:741–794（`infer_init_type` 的 `TOK_IDENTIFIER` 分支）、成员查询助手 `agg_member_sym` 前向声明 parser.c:708 | ✅ 已实现（P5b 随 `pos/slice_str_infer` 复测失败而移植 `PA` 的 PA-26 分支，属在册清单外的必要修复；口径同 P5 的 `c_type_suffix` 先例）。**IR 前端无成员类型推断路径** |

> A 后端不把切片长度编码进值本身：`p[a..b]` 生成的是「指向第 a 号元素的指针」，`b` 只在与 `lo` 同为字面量时被记录下来供 `len(切片变量)` 求 `hi-lo`（parser.c:2890–2933），随后即丢弃；因此接收方为固定数组时按**声明容量**复制、写回时按右值元素个数决定，`b` 越界仍属运行期未定义行为（与 §5.1.2 一致）。

---

## 语句终止符与语法形态合规（§1.4 / §3 / §6 / §10，自 PB-33 的 P7 起）

> 本节由 **PB-33 的 P7**（2026-10-01）建立，移植 `PA` 侧 **PA-57**（语法标准对齐轮）的剩余合规项。**动手前逐形态复测**：PA-57 的 11 处修复里，四档数组容量（P5 已核对）、`void` 的 `Symbol.pointee_type`（P5b 已带）、`[[export "…"]]` 的字符串写法**在本分支早已成立**；designator 初始化（PA-57⑥）本分支连基础形态都没有，整条留 **P8**；`linkas` 的专属诊断留 **P10**。`irparse.c` / `vis.c` / `type.c` 本轮零改动，下表全部只在 c / native 生效。

| 规范要求 | 编译器实现 | 对应代码 | 状态 |
|---------|----------|---------|------|
| 语句终止符 `;` / `#`（§1.4；`#` 为兼容写法，单独出现即空语句） | 改成**包装层**处理：`parse_statement()` 进门先吃掉连续终止符（空语句），出口再吃掉尾随的 `;` / `#`，于是每条语句分支不再各自处理；原先的一次性大函数改名 `parse_statement_impl` | parser.c:11–14（`eat_stmt_terminator`）、16–25（包装层）、1617（`parse_statement_impl`） | ✅ 已实现（P7；用例 `pos/stmt_terminators`） |
| 括号类型 `(T)`（§3，`<type-name> ::= "(" "<type-name>` ")"，括号只分组，其后 `[N]` 仍由后缀循环接住） | `parse_type` 新增 `TOK_LPAREN` 档；为避免 `f(GREEN)`、`add(i32 a, b)` 这类调用被误读成「括号类型声明」，新增有限前瞻 `peek_ahead`（词法状态快照，整份源码常驻且 token 字符串新分配，故可安全还原）与 `is_paren_type_ahead`（`(` 后只认基本类型 token 且紧跟 `)`），四处「像调用还是声明」的分派公用它 | parser.c:390–399（`parse_type`）、106–116（`peek_ahead`）、121–125（`is_paren_type_ahead`）；调用点 1130（函数 vs 变量）、1269–1270（声明的类型档）、1717–1719（`for` 头 init）、1934–1935（语句内 `name Type` 分派） | ✅ 已实现（P7；用例 `pos/type_forms`） |
| `for` 头带类型声明 `for i i32 = 0; …`（§6，`<for-init> ::= "<identifier>" "=" "<expr>` | `<var-decl>`） | init 新增声明档：`cgen_mark()` → `parse_declaration` → `cgen_take_prefix` 回取文本、去掉收尾 `;` 与空白，再拼 `for (` ——因此 `cgen_raw("for (")` 从公共前置下放到各分支内。推断档 `for i = 0` 同时改为按 `PA` 用 `infer_init_type` 输出声明 | parser.c:1691–1748（`case TOK_FOR`；推断档 1703–1716、声明档 1717–1730） | ✅ 已实现（P7；用例 `pos/for_init_forms`）。推断档里的常量可见性仍保留**本分支的 `VIS_VAR` 口径**：`Visibility` 枚举两侧不兼容，不随本轮收敛 |
| 一元 `+` 仅作字面量前缀（§1.3；其余位置的 `+` 仍是二元运算符，不放宽成一元加） | `parse_primary` 头部：只有 `+` 后紧跟整/浮字面量时才消费并递归（C 侧不发正号） | parser.c:2429–2440 | ✅ 已实现（P7；用例 `pos/literal_forms`） |
| 二进制字面量 `0b…` 的**值**（§1.3） | 词法器早已能扫 `0b` / `0B`，但整数收尾用 `strtoll(buf, NULL, 0)` ——base 0 只自动识别 `0x`，`0b1010` 会被截成 **0**（静默错值，不报错）。改为二进制档显式 `strtoll(buf + 2, NULL, 2)` | lexer.c:261–266（`parse_number` 的整数收尾） | ✅ 已实现（P7；`pos/literal_forms` 复测不过即此条，`PA` 侧同源缺陷由 PA-57 修出） |
| 顶层 `use` / `link` / 终止符可任意交错，且 `use a.b.c` 点号多段（§10，`<use-decl> ::= "use" "<identifier>` { "." "<identifier>" } | 两段顺序循环（先 `use` 后 `link`）合成一个交错循环，含 `;` / `#` 分支与点号累加（诊断 `expected module name after '.' in 'use'`）；模块名保持点号形式供符号查找，查文件时另按目录层级试 `std/io.nc`（点号原样拼出的 `stdlib/a.b.nc` 保留一次尝试，兼容单段命名） | parser.c:481–543（交错循环；点号档 496–513）、547–550（顶层声明循环的终止符分支）；module.c:81–99（候选路径，新增把点号换成 `/` 的 `slashed` 形式） | ✅ 已实现（P7；用例 `pos/module_top_forms`，无 `func main` → 门禁按 `(module)` 档编译，不进一致性档） |
| 产物 C 的字符串字面量转义归一（实测表 **F2**，`PA` 的 PA-50） | 词法器把 `\n` / `\t` 解码成真字节，直接写进产物 C 得到内含换行的字面量——tcc 容忍、gcc / clang / MSVC 拒绝。新增 `cgen_string_lit()` 按 C 转义序列重新编码（`\\` `\"` 与七个控制符走短转义，其余 `<0x20` 与 `0x7f` 走八进制，`>=0x80` 的 UTF-8 字节原样保留，输出缓冲 1024 字节溢出前先 flush），`parse_primary` 的字面量出口与切片写的字符串出口两处改走它 | cgen.c:136–174（`cgen_string_lit`）、ncc.h:371（原型）、parser.c:2471、3198 | ✅ 已实现（**在册清单外的必要修正**：原计划留 P9，但 `pos/literal_forms` 复测不过——产物 C 出 `warning: unknown escape sequence '\q'` + `error: ';' expected`。改后门禁全部 **57 份产物 C 过 `clang -fsyntax-only`**，F2 在本分支结案；口径同 P5 的 `c_type_suffix` 与 P5b 的 `infer_init_type` 先例） |
| 复测为「本分支早已具备」的三项 | 四档数组容量写法与 `len()`（P5 的核对项）、`void` 的 `Symbol.pointee_type`（P5b）、`[[export "…"]]` 的字符串字面量写法（属性循环直接吞掉字符串参数，对 C 输出无效，不报错） | parser.c:410–449（容量）、828–835（`record_init_pointee`）、854–869（属性列表，864 处吞字符串参数） | ✅ 无需移植（口径同 **PA-51** 的「每轮动手前先在 PB 复测」教训） |
| designator 初始化与聚合成员默认值（PA-57⑥，§5.1.1）| 本分支**零形态**：无 `Symbol.def_init`、无 `member_default_text()` / `default_init_text()`、无 `MAX_INIT_DESIGATORS`，因此也没有「成员默认值被当类型名前缀输出」的 F1 缺陷修复 | — | ⚠️ 未落地，整条登记为 **P8**（含 `pos/struct_member_default` / `pos/array_member_default` / `pos/designator_init` / `err/default_init_overflow`） |

> 本轮带来的用例全部从 `PA` 逐字复制（`.expect` 走子串匹配，A 后端诊断文案与 `PA` 相同故可直接用）；`irparse.c` 一行未改，故七份用例在 IR 侧全部 SKIP。唯一例外是 `err/is_empty_range`：IR 前端**本就有**同类检查（irparse.c:2266–2268），但文案不同，入白名单会因子串匹配失败——这是「入白名单需要的不只是能力，还需要同文案」的又一例。

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
| F8 | 四种数组容量写法逐个编译：`x i32[5]` / `x i32[5...]` / `x i32[...]` / `x i32[...5]`（**A 后端 c/native**） | `[5]` → `int32_t x[5]` 正常；`[5...]` → **前端拒绝**（`expected ']', got 'TOK_ELLIPSIS'`）；`[...]` → 退化为 `int32_t* x`，**没有默认容量 8**；`[...5]`（设计标为已废弃的反序写法）→ `int32_t x[5]` 正常。`[5..]` / `[..5]` / `[..]` 三种点号写法一律拒绝 | **A 后端与设计的四档写法不一致**：规范里的 `[N...]` 在 1.0 发布线根本写不出来，反而是「已废弃」的 `[...N]` 可用；`[...]` 的默认容量 8 只在 IR 槽模型里成立（`tests/pos/ir_slice.nc:11` 注释即按 8 槽写）。待办：`TODO-PA.md`（PA-51）。**本分支不成立**——四档写法 2026-09-01 即已对齐 IR 前端，`pos/array_capacity_tiers.nc` 逐档量过，见文末差异表「四档数组容量写法」行 |
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
| tests/pos/str_array_init.nc | 定长 `char[n]` 配字符串字面量 → 数组存储、可原地改写、`len()` 取声明容量，含「刚好放下」边界（P5） | ✅ c / native 逐行比对；IR 侧 SKIP（无容量检查） |
| tests/err/str_array_overflow.nc | 字面量含结尾 `\0` 超过声明容量时前端报错（P5） | ✅ |
| tests/err/str_array_bad_elem.nc | 非 `char` 元素数组用字符串字面量初始化时前端报错（P5） | ✅ |
| tests/pos/multi_arr_decl.nc | 多变量声明的定长数组按容量分配数组存储、`len()` 取容量，动态 `char[]` 退化为指针且 `len()` 取字面量长，标量形态不受影响（P5） | ✅ c / native 逐行比对；IR 侧 SKIP（不支持该形态） |
| tests/err/multi_arr_overflow.nc | 多变量声明的定长数组超容量时报与单变量同文案的容量错误（P5） | ✅ |
| tests/err/multi_arr_nostr.nc | 定长数组类型的多变量声明用非字符串字面量初值时报错，不再静默丢弃数组后缀（P5） | ✅ |
| tests/pos/array_capacity_tiers.nc | BNF §3 四档容量写法 `[N]` / `[N...]` / `[...]` / `[...N]` 逐个验证容量与 `len()`，含省略容量取默认 8 与多维 `i32[2][3]`（P5 核对项，非移植项） | ✅ c / native 逐行比对；IR 侧 SKIP |
| tests/err/len_unknown.nc | `len()` 静态不可知时前端报错而非返回过期值（P5；整变量重赋值后撤销登记） | ✅ |
| tests/err/void_subscript.nc | 通用 `void` 指针裸下标前端拒绝并给出 `p.(T)[i]` 修复写法（P5，仅拒绝半段） | ✅ |
| tests/pos/stmt_terminators.nc | 语句终止符 `;` / `#`：语句后连续终止符、单独成句的空语句、块内与顶层混用（P7） | ✅ c / native 逐行比对；IR 侧 SKIP |
| tests/pos/type_forms.nc | `(T)` 括号类型：声明处的括号分组、`(T)[N]` 组合，且 `f(GREEN)` / `add(i32 a, b)` 等调用形态不被误读成括号类型声明（P7） | ✅ c / native 逐行比对；IR 侧 SKIP |
| tests/pos/for_init_forms.nc | `for` 头的三档 init：推断 `i = 0`、带类型声明 `i i32 = 0`、普通表达式（P7） | ✅ c / native 逐行比对；IR 侧 SKIP |
| tests/pos/literal_forms.nc | 字面量档：`0b1010` 的**值**、`0x…`、一元 `+`、浮点，以及字符串转义在产物 C 重新编码（P7，含 `cgen_string_lit`）| ✅ c / native 逐行比对；IR 侧 SKIP |
| tests/pos/is_pattern_forms.nc | `is` 的整数 / 带符号 / 闭区间 / 可见性枚举 / 标识符 / `_` 通配各模式一轮跑完（P7） | ✅ c / native 逐行比对；IR 侧 SKIP |
| tests/err/is_empty_range.nc | 反向区间 `is 5..1` 编译期拒绝并给出 `empty 'is' range … lo must be <= hi`（P7） | ✅ |
| tests/pos/module_top_forms.nc | 顶层 `use a.b` / `link` / 终止符任意交错，且点号模块名按目录层级找到 `std/io.nc`（P7） | ✅ 无 `func main`，按模块编译（门禁记 `(module)`），不进一致性档 |
| tests/pos/ir_struct.nc | 结构体 / union / enum 基础形态（2.0 IR 线用例）。**本轮改的是用例自身**：原内容是 `PA` 在 PA-57 之前的版本，末行打印 `struct copy bad`，且因无 `.expect` 只进一致性档而长期静默 PASS（P7 修正） | ✅ 四后端 PASS（现逐行比对 8 行期望输出）；副作用：一致性计数 37→36 |

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

**P5 落地后（数组/字符串初值诊断 + `len()` 内置函数 + `c_type_suffix` 逐维输出，2026-09-28 实测复跑）**

| 项目 | 现值 | 说明 |
| --- | --- | --- |
| `xmake test --all` c / native | **各 38P / 0F / 6S** | 各 +9 PASS：3 份 pos（`str_array_init` / `multi_arr_decl` / `array_capacity_tiers`，均带 `.expect` 逐行比对）+ 6 份 err（`str_array_overflow` / `str_array_bad_elem` / `multi_arr_overflow` / `multi_arr_nostr` / `len_unknown` / `void_subscript`）。SKIP 仍是 6 条 `IR_ONLY`，未变 |
| `xmake test --all` ir-c / ir-native | **各 15P / 0F / 26S** | PASS 不变、SKIP +9：九份用例**刻意不入** `IR_SUBSET` / `IR_ERR_COVERED`。逐条实测（2026-09-28，命令级 `-backend=ir-c`）：`err/str_array_overflow` 在 IR 侧 **rc=0 编译通过**（只有 tcc 的两条 assignment warning），即 IR 前端根本没有容量检查，入白名单会掩盖缺口；其余五条 err 均 rc=1 但**失败点全在 tcc**（`undefined symbol 'print'`——IR 前端未内建 `print`；`error: lvalue expected`——多变量声明形态），没有一条给出与 A 后端同文案的前端诊断，故不满足共用 `.expect` 的条件；三份 pos 里 `pos/str_array_init` 同样只卡在 `print`、`pos/multi_arr_decl` 卡在 `lvalue expected`，`pos/array_capacity_tiers` 则被 IR 前端**解析层**拒绝（多维声明 `m i32[2][3]` 报 `expected '=', got '['` + `ir: unexpected token ']' in expression`，该行即本轮 A 后端新修的 `c_type_suffix` 逐维输出口径） |
| 跨后端一致性 | **37P / 0F** | **不变**——本轮三份 pos 全带 `.expect`，按 `xmake.lua` 一致性档的口径只计入各后端 PASS 列、不进跨后端比对（与 P4 的 `pos/flow_move`「无 `.expect` 才进比对」互为镜像） |
| `examples/` | c / native **6/7**、ir-native **7/7** | 复跑确认不变，仍缺 **P11**（cooking 编译期函数在 A 后端） |

**P7 落地后（PA-57 剩余语法合规项 + `cgen_string_lit()` 转义归一，2026-10-01 实测复跑）**

| 项目 | 现值 | 说明 |
| --- | --- | --- |
| `xmake test --all` c / native | **各 53P / 0F / 6S** | 各 +8 PASS（改前 P5b 为 45P）：7 份新用例（6 份 pos 带 `.expect` + 1 份 err）+ `pos/ir_struct.nc` **新占一格 PASS**——它原先无 `.expect`、只在一致性档比对，补上 `.expect` 后按 `xmake.lua` 口径转入各后端的 PASS 列。SKIP 仍是 6 条 `IR_ONLY` |
| `xmake test --all` ir-c / ir-native | **各 16P / 0F / 40S** | +1 PASS 即 `pos/ir_struct.nc`（IR 侧本就能编译运行，原先不计 PASS 位）；SKIP +7（七份新用例一律不入 `IR_SUBSET` / `IR_ERR_COVERED`）。逐条实测：六份 pos 在 irparse 解析层被拒或不在 IR 子集；`err/is_empty_range` 在 ir-c **也已被拒绝**，但文案是 `ir: invalid range in 'is' pattern (5..1, lo > hi)`，与共用 `.expect` 的子串匹配口径不符，故刻意留 SKIP
| 跨后端一致性 | **36P / 0F** | −1：`pos/ir_struct.nc` 从比对档移出（补 `.expect` 的直接后果，`PA` 侧同口径）；本轮其余新用例全部带 `.expect`，按口径不进比对档，与 P4 的 `pos/flow_move`（无 `.expect` 才进比对）互为镜像
| `examples/` | c / native **6/7**、ir-native **7/7** | 不变，唯一失败仍是 `06_cooking.nc`（**P11** 的缺口，与本轮无关） |
| 产物 C 语法体检 | **57 份全过 `clang -fsyntax-only`** | 门禁全部产物（含本轮新用例与四档）逐份交外部编译器做语法检查，为 **P9** 的抽查接线给出「当前零红」基线；同时即 **F2**（PA-50）在本分支结案的证据。改前同一批产物里，`pos/literal_forms` 一样的写法会触发 `unknown escape sequence` 与 `error: ';' expected`

> 数字与 `PA` 分支现值（c/native 56P/0F/4S、ir 8P/0F/49S、examples 7/7）**不可直接对比**：两条线的用例集与 skip 名单不同（`PA` 自 merge-base `7d4c652` 起新增 43 份用例，P1 ~ P6 + P5b + P7 已带进 **34 份**、余 **9 份**未进；P5b 带来 7 份、P7 带来 7 份（6 份 pos 带 `.expect` + 1 份 err），均不增加一致性计数；P7 另把 `pos/ir_struct.nc` 换成 `PA` 修正版并补 `.expect`，它从比对档移入各后端 PASS 档，故一致性 37→36）。此后每完成一条 PB-33 移植项就复跑本表并在其下追加一行，**数字回落即回归信号**。
>
> 基线复跑同轮做符号级普查，核出两类既有事实（均 2026-09-26 实测，`git grep <symbol> PB -- ncc`）：① `PA` 侧 **PA-51** 的四档数组容量写法本分支**早已实现**（`parser.c:410–449`，2026-09-01 即对齐 IR 前端，`[...]` 取默认容量 8），故 PB-33 表把它从移植项降为**核对项**；② `PA-49` / `PA-50` / `PA-55` / `PA-57` / `PA-58` / `PA-59` 的实现符号 `def_init`、`member_default_text`、`default_init_text`、`cgen_string_lit`、`eat_stmt_terminator`、`is_paren_type_ahead`、`parse_linkas_decl`、`skip_cooking_item`、`ct_capture_body`、`MAX_INIT_DESIGATORS`，以及 `xmake.lua` 的产物 C `-fsyntax-only` 抽查，在本分支**全部零命中**（`ct_funcs` 仅存在于 `irparse.c`，`parser.c` 无）——即 P7 ~ P11 五行的缺口是实测而非推测。**2026-10-01 随 P7 复核**：其中 `cgen_string_lit` / `eat_stmt_terminator` / `is_paren_type_ahead` 三个符号已落地，剩余 `def_init` / `member_default_text` / `default_init_text` / `MAX_INIT_DESIGATORS`（**P8**）、`parse_linkas_decl` / `skip_cooking_item`（**P10**）、`ct_capture_body`（**P11**）仍零命中；`xmake.lua` 的抽查也仍未接线，但本轮已手工量到 **57/57 全过 `clang -fsyntax-only`**，故 **P9** 只剩接线一件事。

---

## 2.0 线（PB）差异

以下为 PB 与上表（1.0 现态）不同的条目，行号基于 PB `8079ad0`，**每轮移植后逐个复核**（P1 / P2 / P3 于 2026-09-26、P4 于 2026-09-27、P5 于 2026-09-28）（`parser.c` 在 1603 行以后因插入 `agg_member_sym` 与两段内置函数体整体后移：`vis_check_call_arg` 调用点 2125→2253、`?.` 拒绝块 2169→2297、`?=` 拒绝 case 2513→2641；`irparse.c` 在 1195 行以后整体 +7；P3 对 `parser.c` 只是**行内**替换（1640、2109 两处分派的右侧表达式），行号未动，`type.c` 在 65 行以后插入 37 行、`ncc.h` 在 389 行加一条原型；**PB-34**（IR 机器码后端裁剪）对 `parser.c` / `type.c` / `irparse.c` 本节所引锚点**零位移**——它只碰 `irparse.c` 尾部（3408 行起新函数与 `ir_compile` 的发码尾段）、`ncc.c` 的 CLI 分派、`ir_backend.{c,h}` / `ir.h` 注册表与两个整文件删除；**P4**（2026-09-27）对 `parser.c` 的位移从 1088 行起算：`no_auto_free` 登记 +9，`parse_is_stmt` +3（只有注释增长，三处子句守卫是行内替换），`parse_statement` 入口 +3、while 脚手架 +5、`do` 体清零 +4、`TOK_IS` 守卫 +7，`parse_postfix` 的 `lhs_bare_ident` +3，`parse_assign` 的重登记 +9（净增 50 行）——即 1173 行以后的锚点按位置逐行重算，位移自 +9 起、文件末尾达 +50。`vis.c` 的 `vis_check_usable()` 内插 4 行转移豁免（其后锚点整体 +4：`vis_check_writable()` 139→143、`vis_check_assign()` 起点 154→158），`vis_check_assign()` 内再插冻结源拒绝与 `moved_src` / `ownership_transferred` 两段共 16 行（该函数终点 198→218，其后累计 +20）；`irparse.c` **零改动**故其锚点全部不变；**P5**（2026-09-28）对 `parser.c` 是**分段位移**（净增 130 行，分段值见文首更新日期一行），本表与正文各表的 `parser.c` 锚点已按新行号整体重算并逐条 grep 复核；`ncc.h` 在 133 行以后 +4（`moved_src` 228→232、`while_depth` 296→300），另改正一处既有失准锚点（`type_align` 原型 389→401）；`cgen.c` 只改 `c_type_suffix()` 一个函数（244–263），`vis.c` / `type.c` / `irparse.c` 本轮零改动；**P6**（2026-10-01）只碰构建配置与版本宏——`xmake.lua` 顶部加 4 行、`ncc.c` 一处**行内**替换（行号未动），`ncc.h` 在第 25 行处以 10 行替 2 行故其后**整体 +8**，本文件五处 `ncc.h` 锚点相应更正（`no_auto_free` 127→135、`len_known` 注释块 133–136→141–144、`moved_src` 232→240、`while_depth` 300→308、`type_align` 原型 401→409）；**P5b**（2026-10-01）对 `parser.c` 净增 **288 行**（2874→3162），且位移**非单调**——`parse_deref_chain` 整段删除与 `parse_postfix` / 四个解引用助手整段重写使中段（约 2429–2611）先加后减，声明体「先右值后声明头」重排又让 1116–1162 段净减 32 行再分段回升，故本轮不列位移表而把本文件的 `parser.c` 锚点全部按新行号逐个 grep 复核，顺带改正三处**既有失准锚点**（「`is` 模式匹配」的 `is lo..hi` 行与「存储期属性」的 `is_const`/`is_static` 行、§12.2 的 pv 解析行——自 1.0 冻结线后从未随 P1~P5 重算）。`ncc.h` 在 140 行后加 5 行（`pointee_bytes` 142、`pointee_type` 145，`len_known` / `logical_len` 141–144→148–149）、在 250 行后加 7 行（ParserState 的 `malloc_bytes` … `slice_len`，256–262），`moved_src` 240→245、`while_depth` 308→320、`type_align` 原型 409→421；`cgen.c` 只把 `cgen_truncate()` 由一行截断改为按残留文本重算 `at_line_start`（124→124–130，其后 `c_type_suffix` 244–263→253–269）；**P7**（2026-10-01）对 `parser.c` 净增 **130 行**（3162→3292），位移同样**非单调**——`parse_is_stmt` 的整数模式整段替换（1472–1521 段重写）与 `parse_module` 的两段顺序循环合成一段（430–445 段）使得中段先减后增，故本轮不列位移表，本文件的 `parser.c` 锚点再次全部按新行号逐个 grep 复核，顺带改正一处**既有失准锚点**（`alignof` 标识符路径旧记 2357，实为现 2541）。`cgen.c` 在 `cgen_truncate()` 之后插入 `cgen_string_lit()`（136–174，故其后 `c_type_suffix` 253–269→296–312，`cgen_truncate` 自身 124–130 未动）；`ncc.h` 在 371 行加一条原型，故其后 `type_align` 421→422，其余 `ncc.h` 锚点不动；`lexer.c`（261–266）与 `module.c`（81–99）是本轮新引的两处锚点，此前本文件无其行号；`vis.c` / `type.c` / `irparse.c` 本轮零改动：

| 条目 | 1.0 线（上表） | 2.0 线（PB）现态 |
|------|---------------|-----------------|
| §12.2 `flow`/`var`/`const` 参数前缀 | 前缀被忽略，统一 `VIS_DEFAULT`（parser.c:1149–1153、1164） | **已实现**：`param->vis = pv` 记录前缀（parser.c:1164），调用点 `vis_check_call_arg()` 执行 M2 检查（parser.c:2708）；IR 前端由 `vvis` 状态机等价实现（irparse.c，PB-26） |
| 返回值可见性前缀（§12.3 后半） | 无 | **已实现**：`ret_vis` 调用点检查（parser.c:1339–1355）+ IR 侧 PB-29（含 PB-29.1 六条禁止路径 err 用例） |
| 检查范围表「IR 后端所有权检查」 | ⚠️ 未实现 | **已实现**（PB-26/M2 移植进 irparse.c，err 用例 m2a~m2d 在 ir-c/ir-native 下同样拒绝） |
| `is` 各模式行号 | 见「`is` 模式匹配」表（本分支锚点） | 本分支 `parse_is_stmt` parser.c:1528–1615（P4 改的是子句守卫与 while 脚手架，P7 改的是整段整数模式与收尾文案）：`_` 通配符 1538–1547、共用守卫头 1549、带符号前缀 1552–1560、区间与空区间校验 1565–1583、标识符/枚举档 1587–1603、`invalid 'is' pattern` 1605、`expected '{' after 'is' pattern` 1613；`PA` 同函数在其 parser.c:1625 起。`is` 块由 irparse.c:2241 起的 `_` 分支处理 |
| `is _` 通配符 | v1.0.2 补全（PA-15） | PB-27.5 先于 1.0 线完成（parser.c + irparse.c 双前端） |
| 反向范围 `lo > hi` 校验 | ⚠️ 未实现 | **两侧都已实现**：A 后端自 P7（parser.c:1578–1581，`empty 'is' range 5..1: lo must be <= hi`，用例 `err/is_empty_range`）；IR 前端更早（irparse.c:2266–2268）。但两句文案不同——IR 侧是 `ir: invalid range in 'is' pattern (5..1, lo > hi)`，与共用 `.expect` 的字串匹配口径不符，故该用例**刻意不入** `IR_ERR_COVERED`，IR 侧 SKIP |
| `__is_val` 类型 | `int` 固定 | **类型感知**，等于 `while` 条件表达式类型（PB-27.7） |
| 循环体外使用 `is` | A 后端按 `if` 展开，不报错 | **两侧都拒绝**（P4 起）：A 后端 parser.c:1813–1822 报 `'is' pattern match only valid inside while loop body`，IR 前端同句带 `ir: ` 前缀（PB-27.1）。`do` 体亦拒绝——A 后端 parser.c:1685–1688 进体前清零 `while_depth` |
| 多个 `is-clause` 无 fallthrough | ⚠️ 未实现（PA-16 登记待决策） | **A 后端已实现**（P4 移植 `PA` 的 PA-16 决策「只修 A 后端、IR 留 2.0」：`__is_matched` 守卫，见「`is` 模式匹配」表）；**IR 前端仍未实现**，实测 ir-c 下 `pos/is_no_fallthrough` 每个子句打两行输出 |
| `flow` 存储来源与转移豁免（§11.1 / §14.2） | 1.0 表只写「块退出自动 free」与 `ownership_transferred` | **P4 补齐三条**：① `no_auto_free`（ncc.h:135）让字符串字面量 / `&x` / `{…}` 初值的 `flow` 不被 free（改前实测 rc=127 静默崩溃）；② `moved_src`（ncc.h:245）豁免转移那条语句内对失效源的读取（改前实测两条误报 `'a' is invalidated`）；③ 冻结源转交所有权改为拒绝（vis.c:196–200）。IR 侧 ①③ 无用武之地（irparse.c 不做自动释放与冻结检查，见「检查范围」表），②的等价物在 IR 前端亦未实现 |
| `?=` 安全赋值 / `?.` 安全解引用 | 1.0 线已移除（BNF v2.3 / PA-20、PA-21），`token.h` 保留 token | **2026-09-26 起与本分支设计层归一（PB-33 的 P1）**：语法层拒绝——A 后端 parser.c:3158–3163（`'?='`，诊断后 fall through 到普通 `=`）、parser.c:2856–2860（`'?.'`，诊断后补吞 `(` 按 `.()` 展开，顺带删去失效的 `(void)op;`）、is_expr_continuer 的 parser.c:585–598 的 `switch` 判据去掉 SAFE_*；IR 前端 irparse.c:1195–1201 的 `ir_primary` 补同文案拒绝分支。`TOK_SAFE_ASSIGN` / `TOK_SAFE_DOT` 仍留词法层，用例 `err/safe_assign_removed` / `err/safe_dot_removed` 四后端一致拒绝（文案与 `PA` 逐字相同；恢复路径两侧略异，同一 `?.` 用例 `PA` 的 c 后端多 1 条级联、本分支只报 1 条） |
| 从属查询与位域偏移内置函数（§2.3，BNF v2.4） | `PA` 侧同文件该节标 ✅ 已实现（v1.0.2 / PA-22，仅 c/native；IR 前端未实现）；**本文件此前无该节** | **2026-09-26 随 PB-33 的 P2 落到本分支 A 后端**：`parse_builtin_kw` 新增 `structof`/`unionof`/`holdof(Type, member, ptr)` → `((void*)((char*)(ptr) - offsetof(Type, member)))`（parser.c:2079–2113；`structof` 只认 struct、`unionof` 只认 union、`holdof` 通用；成员不存在 / 类型非聚合体 / 缺第三参各有专属诊断）与 `bitoffsetof(Type, member)` → 按声明序折成编译期常量表达式（parser.c:2119–2193；以前置非位域成员为锚点，用 C 的 `offsetof`/`sizeof` 定位存储单元起点，目标非位域、基类型宽度未知、同组存储类型不一致均报错）；成员查询助手 `agg_member_sym()` 在 parser.c:1997，`parse_primary` 的分派 case 在 parser.c:2443–2447。**IR 前端仍拒绝**（`ir: unexpected token 'structof' in expression`，与 `PA` 的 IR 现态逐字相同）——8 字节槽模型无真实布局，属模型边界而非移植缺口。用例 `pos/ownerof.nc` / `err/structof_bad_member.nc` 自 `PA` 带来，c/native PASS、IR 侧 SKIP |
| `alignof(T)` 类型对齐（§2.3，BNF `<builtin-call>`） | `PA` 侧同文件该节标 ✅ 已实现（v1.0.2 / PA-24：编译期自算后只输出整数字面量）；**本文件此前无该节** | **2026-09-26 随 PB-33 的 P3 落到本分支 A 后端**：新增 `type_align()`（type.c:71–102，原型 ncc.h:422——旧值 389 为既有失准锚点，P5 轮按实测改正），编译期递归求对齐——数组/别名经 `ref` 取元素对齐、`struct`/`union` 取最宽**非位域**成员（递归深度上限 16）、`void` 按 §5.1 通用指针计 8、其余取 `CType.align` 为 0 时回落 `type_default_align(kind)`；`parser.c` 两处分派由 `cgen_raw("_Alignof(%s)", …)` 改为 `cgen_raw("%u", type_align(&tmp))`（parser.c:2034 关键字路径、2541 标识符路径——此前记为 2357，属既有失准锚点，2357 一直是 `vis_check_usable()` 那处，P7 按新行号改正；后者按 `PB-6` 已记录的既有事实**实际不可达**——`alignof` 是关键字 token，改它只为与 `PA` 同源，不新增覆盖主张）。改前实测：`c` / `native` 链接期 `tcc: error: undefined symbol '_Alignof'`，`ir-c` 因固定返 8 而九项断言全错。**IR 前端未改**（`irparse.c:909` 仍按槽模型固定返回 8，与 `PA` 的 IR 现态一致）。用例 `pos/alignof_builtin.nc` 自 `PA` 原样带来（含其注释里的 PA-24 溯源，可与 PB-33 表 P3 行对号），**未列入** `IR_SUBSET` |
| IR 机器码后端档位（B 方案出码面） | c / native 之外的四档发射器（x86-64/riscv64/arm64/loongarch64），`ir-native` 硬绑 x86-64 | **2026-09-27（PB-34，ltree 裁定）起本分支只留 riscv64 + loongarch64 两档发射器**：`ir_x86_64.c`（518 行）与 `ir_arm64.c`（355 行）整文件删除，注册表 `ir_backend.c:49–52` 只剩两项，`ir_backend.h` 的两条 extern 与 `ir.h` 的 `irgen_native_emit` 声明一并摘掉。`ir_compile` 发码尾段改按宿主分派——新 `ir_host_asm_backend()`（irparse.c:3406–3417，靠 `__riscv`/`__loongarch_lp64` 判定）非空时出 `.s` 交 tcc 汇编，为空时（含本机 Windows x86-64）走 `irgen_c_emit` 那条既有尾巴即 **IR→C→宿主 cc**（irparse.c:3453–3473，`-v` 打一行回退说明）；`ir-riscv64` 仍编号 4，`ir-loongarch64` 由 6 改 5。CLI 的 `-backend=<be>` 与 `-backend <be>` 两写法改共用 `set_backend()`（ncc.c:232–259，调用点 314、318），六档全收，已删的 `ir-arm64`/`ir-x86_64` 给专属诊断而非 `unknown backend`；`xmake.lua` 里「非 Windows 主机整体跳过 ir-native」段（起因是 x86 发射器的 ELF 在 Linux 运行时 segfault，2026-08-31 WSL 实测）随发射器一并删除。**门禁四档数值与裁剪前逐字一致**（24P / 24P / 15P / 15P，一致性 36P/0F），但本机 ir-native 一档已等价 IR→C；`examples/` 本轮逐条量到 ir-native **7/7** 而 c/native **6/7**，即 **P11** 的 cooking 缺口纯粹在 A 后端。`PA`（1.0 冻结线）四档发射器**未动**。**2026-09-27（跨零点）PB-35 续**：两档产物首次过真汇编器，暴露 loongarch64 发射器**此前 0/34 能汇编**（AT&T 式 `-8($s0)` 访存、`xor.d`/`slt.d`/`sltui r,r,r`、`mov.d`、`fcmp.eq.d` 一类写法在 GAS 与 LLVM 里都不存在），已按 LoongArch64 正确形态重写（`la_slot()` ir_loongarch64.c:31–35 出基址在前的三操作数、`IR_ITOD`/`IR_DTOI` 走 `movgr2fr.d`+`ffint.d.l` / `ftintrz.l.d`+`movfr2gr.d` 两步、`IR_FCMP` 走 `fcmp.<cnd>.d $fcc0`+`movcf2gr`+`andi`（ir_loongarch64.c:86–100 与 134–156）、整数比较去 `.d` 后缀（174 附近）），文件头注释记下这些语法事实。两档共有的另一条是**帧越界**：槽位靠帧指针 12 位带符号立即数寻址，`pos/ir_slice.nc` 实测帧 2208 字节 → `addi sp,sp,-2208` / `sd a0,-2056(s0)` 全被拒绝。解法是集中式护栏而非各发射器自查——`TargetBackend` 新增 `max_frame`（ir_backend.h:48–50，两档均填 2047：ir_riscv64.c:348、ir_loongarch64.c:376），`irgen_backend_emit` 在算完帧大小处先拒后发（ir_backend.c:106–115，诊断含函数名 / 实测帧 / 上限 / 后端名）。改后量测见上表 |
| 数组与字符串初值口径 + `len()`（§5.1.1 / §4.2 / §2.3，`PA` 的 PA-25⑦ / PA-27 / PA-31 / PA-34） | `PA` 侧该主题占「指针后缀链与切片」「内置查询与输出内建」两节 | **P5（2026-09-28）回灌「诊断 + 逻辑长度登记」一档，P5b（2026-10-01）回灌其余全部**：切片读 `p[a..b]`（`&p[a]` 视图）、`T[n] x = p[a..b]` 按声明容量 `memcpy`、切片写 `p[a..b] = {…}` / `= "abc"`、`T[n] x = malloc(T[n])` 的数组退化、切片变量 `E *s = &a[lo]`、`.(T)` 处于后缀链任意位置（`parse_deref_chain` 已删除、折进 `parse_postfix` 通用循环，声明体改成**先解析右值、后出声明头**）、`PA-55` 的 `void` 槽静态还原，以及 `infer_init_type()` 的调用/成员类型推断——逐条见上文「指针后缀链与切片」表。`PA` 侧同主题四份依赖切片的用例（`pos/deref_slice` / `pos/len_builtin` / `pos/slice_str_infer` / `pos/void_slot_infer`）与三份 err（`err/deref_bounds` / `err/slice_str_overflow` / `err/void_member_unknown`，另 `err/void_subscript` 已在 P5 带入）全部带来并 PASS；产物 C 与 `PA` 逐字节相同（四个切片用例逐个 `diff` 为空）。**IR 前端本轮零改动**：七份新用例实测全部在 irparse 解析层被拒，故 `IR_SUBSET` / `IR_ERR_COVERED` 名单**一个都没加**，ir-c / ir-native 各 33 份 SKIP |
| §3 四档数组容量写法（`PA` 的 **PA-51**，即上文实测表 **F8 / F9** 两行） | F8/F9 记的是 `[N...]` 写不出来、`[...]` 无默认容量 8、`i32[...] = {…}` 生成非法 C | **该缺陷在本分支不成立**：`parser.c:410–449` 四档全可写、省略容量取默认 8，P5 的 `pos/array_capacity_tiers.nc` 已在 c / native 逐档量过（含多维 `i32[2][3]`）。F8/F9 是**沿用的 1.0 线（`PA`）实测快照**，不可当作本分支现状读；`PA` 侧的修复状态见 `PA` 分支同文件 |
| 语句终止符 / 括号类型 / `for` 声明式 init / 一元 `+` / `use` 点号多段（`PA` 的 **PA-57** 剩余项）| 本文件正文各表此前只按「换行即语句边界」书写，无 `;` / `#` 的语法地位、无 `(T)` 括号类型、`for` 头只有推断与表达式两档 | **P7（2026-10-01）已回灌**：`eat_stmt_terminator`（parser.c:11–14）+ `parse_statement` 包装层（16–25，旧的一次性大函数改名 `parse_statement_impl`，1617）；`(T)` 括号类型（`parse_type` 390–399 + `peek_ahead` 106–116 + `is_paren_type_ahead` 121–125）与四处分派 1130 / 1269–1270 / 1717–1719 / 1934–1935；`for` 头带类型声明 1691–1748；一元 `+` 2429–2440；`use` 点号多段与顶层交错 481–543、547–550，配合 `module.c:81–99` 的点号→目录层级候选路径。**IR 前端零改动**，六份 pos 用例一律不入 `IR_SUBSET`。**designator 初始化（PA-57⑥）本分支连基础形态都没有**（`Symbol.def_init` / `member_default_text()` / `default_init_text()` / `MAX_INIT_DESIGATORS` 全零命中），整条留 **P8**；`linkas` 也不随本轮，留 **P10** |
| 产物 C 的字符串字面量转义（实测表 **F2** / `PA` 的 PA-50）| F2 行记的是「生成的 C 含真实 0x0A 字节，tcc 容忍而 gcc/clang/MSVC 拒绝」，并注明「当前门禁全在 tcc 下跑，故未暴露」 | **P7 已在本分支落地**（原计划留 P9，`pos/literal_forms` 复测不过逼出）：新增 `cgen_string_lit()`（cgen.c:136–174，原型 ncc.h:371），`parse_primary` 的字面量出口（parser.c:2471）与切片写的字符串出口（3198）改走它；改后**门禁全部 57 份产物 C 过 `clang -fsyntax-only`**，即 F2 在本分支结案。P9 只剩把该抽查接进 `xmake.lua` |
| `pos/ir_struct.nc` 的用例自身 | 1.0 发布集不含 `ir_*` | **P7 修正**：本分支该用例是 `PA` 在 PA-57 **之前**的版本，末行打印 `struct copy bad`（成员赋值未生效）；因它原先**没有** `.expect`，只进四后端一致性档，四端同样错误即判定一致 → **长期静默 PASS**。换成 `PA` 修正版 + 补 `.expect`（8 行输出）后四后端逐行比对 PASS，代价是它移出一致性档（37→36）——`xmake.lua` 的计数口径如此，与 `PA` 现态同口径 |
| 实测表 **F1**（结构体成员默认值 lowering） | F1 行记的是 `PA` 线缺陷：`P struct { x i32 = 7  y i32 }` 生成 `typedef struct P { 7int32_t x; … }`，tcc 报 `invalid number` | **2026-10-01 在本分支写探针复测（PB-32 的 F1 部分）：同样成立，形态逐字相同**——前端照收，产物出成 `typedef struct P { 7int32_t x; int32_t y; } P;`（探针 `ncc/build/p7t/f1_probe.nc`，gitignore），失败落在 tcc `error: invalid number`。即本分支缺的是**整条成员默认值机制**（`Symbol.def_init` / `member_default_text()` / `default_init_text()` / `MAX_INIT_DESIGATORS` 零命中）而不止 designator 一档，登记为 **P8**；同轮的 F2 一条已随 P7 的 `cgen_string_lit()` 结案，见上表 |

| 测试覆盖 | 1.0 门禁 12P/0F/5S | `xmake test --all` 全矩阵（c/native/ir-c/ir-native），见 `ROADMAP.md` 里程碑「验收」行 |
