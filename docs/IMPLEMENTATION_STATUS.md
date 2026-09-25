# NihaoC 规范与编译器实现对应关系

本文档记录 NihaoC 语言规范（Chinese.md / English.md）中各项指针安全规则与编译器（ncc）实际实现的对应状态。

> 更新日期：2026-09-25（2026-09-24 的 PA-49 / PA-50 代码轮 + 同日 BNF v2.11 语法标准对齐轮 **PA-57** + 同日聚合数组默认值轮 **PA-56** + 次日凌晨的未实现形态专属诊断轮 **PA-52 / PA-59** + 同日的 cooking 编译期函数实现轮 **PA-58**）。本表主体以 **1.0 冻结线（v1.0.2 之后的 `PA`）** 的现态书写，行号基于该时点 `PA`/`main` 的 `parser.c` / `vis.c` / `irparse.c`，仅作导航用。
> **行号漂移口径**：PA-49/PA-50 落地后 `parser.c` 行号已全量重基（2026-09-21 版之前的引用一律加 0/1/2/7/31 的偏移）；**PA-57 轮 `parser.c` 再净增 290 行**，偏移随位置递增（文件头 +19、`parse_type` 起 +61 ~ +85、`parse_module` 起 +105、`parse_declaration` 起 +138 ~ +158、`parse_statement` 起 +175 ~ +194、`parse_primary` 起 +196 ~ +212、`parse_postfix` 起 +224、`parse_assign` 起 +237，其后一律 +237）；**PA-56 轮 `parser.c` 再净增 82 行**（3259 → 3341），偏移随位置递增（`parse_init_list` 起 +16、`parse_declaration` 起 +102、`parse_postfix` / `parse_primary` / `parse_assign` 及其后 +82）。本文件正文凡未标注「PA-56」的行号仍是**上一基线**值，按上述分档粗换算即可；PA-57 / PA-56 新增与改写的行号已按当前文件逐条核对；**2026-09-25 未实现形态专属诊断轮（PA-52 / PA-59）`parser.c` 再净增 51 行**（3341 → 3392），偏移随位置递增（`parse_linkas_decl` / `parse_module` 起 +11、`skip_cooking_item` 及其后 +19）。本行之后凡引用 `parser.c` 的既有行号按此换算，本轮新写入的引用（449–458、2476–2485、2514–2519）已按当前文件核对。本行之前引用的 2476–2485 / 2514–2519 两段已被下一轮改写，见紧随其后的 PA-58 行。另注：本文若干区间写作 `A–B` 且 `B < A`，那是历次「只重基区间首数」的部分重基留下的已知痕迹（`A` 可用、`B` 为更早基线），不是排印错误，读者以 `A` 为准。
> **2026-09-25 同日的 cooking 编译期函数实现轮（PA-58）`parser.c` 再净增 183 行**（3392 → 3575）：`ct_funcs` 表与 `ct_func_find` / `ct_func_call` 插在 `pc_or` 前向声明之后（现 2321–2396），故 `pc_prim` 起 +64；`pc_prim` 的编译期函数调用形态在 2431–2463；`skip_cooking_item` 现 2589–2599（原引用 2476–2485）、其后新增 `ct_capture_body` 2601–2618；`parse_cooking_block` 的 `<ct-func-def>` 定义分支在 2648–2704（原 2514–2519 的「未实现」诊断由它替换）。同轮 `skip_cooking_item` 的按行判据由 `line_num` 改为 `last_line_num`（见 PA-58 条目）。
> **PB（2.0 线）差异集中在文末「2.0 线（PB）差异」一节**：`vis.c` 的矩阵实现两侧同源（行号一致），`parser.c` / `irparse.c` 行号与部分状态不同（该节右侧列的 PB 行号不受上述 PA 偏移影响）。
>
> 本文件是**实现状态层**文档：`BNF.md` / `Chinese.md` / `English.md` / 两份语法元素表只写设计，不写落地情况；状态、行号、待办一律记在这里或 `TODO-PA.md` / `TODO-PB.md`。


---

## 指针传递矩阵（§12.1，仅限同一作用域内变量之间的赋值）

| 规范要求 | 编译器实现 | 对应代码 | 状态 |
|---------|----------|---------|------|
| 4×4 传递矩阵 | `vis_check_transfer()` | vis.c:70–86 | ✅ 已实现 |
| 源状态变更（冻结/失效/保持） | `vis_update_source()` | vis.c:89–113 | ✅ 已实现 |
| 仅指针类型受约束 | `vis_is_pointer_type()` 守卫 | vis.c:55–60 | ✅ 已实现 |
| 借用作用域释放 | `vis_unfreeze_borrows()` | vis.c:117–124 | ✅ 已实现 |
| 失效变量不可读 | `vis_check_usable()` | vis.c:127–140 | ✅ 已实现 |
| 冻结变量不可写 | `vis_check_writable()` | vis.c:143–152 | ✅ 已实现 |
| 赋值检查编排 | `vis_check_assign()` | vis.c:158–218 | ✅ 已实现 |
| `flow → flow`（同一指针的所有权转移，§12.1 允许） | 声明式 `flow neu void = ptr`（parser.c:1177–1158）与赋值式 `neu = ptr`（parser.c:2950–2924）两种写法均可书写：`vis_check_assign` 把源置 `BS_INVALID` 的同时登记 `ParserState.moved_src`，`vis_check_usable` 只对**本语句自身的**转移源放行，下一条语句进入 `parse_statement` 即清除（parser.c:1440–1395）——源在转移语句内可读、语句结束后失效。转移同时置源的 `ownership_transferred`，失效源不再参与 §11.1 的自动释放，释放责任移交接收方。源已被 `const`/`var` 借用（`BS_FROZEN`）时转交所有权即时报 `cannot transfer ownership of 'ptr': it is borrowed (frozen) by an active const/var`，与 §14.2 分析表一致 | parser.c:1186、1393–1395、2226、2911，vis.c:134、194–201、205–211 | ✅ 已实现（v1.0.2 / PA-33 修复检查顺序缺陷） |

> 本矩阵不适用于函数返回值接收：返回值没有可冻结/可解冻的源变量，规则由 §7.3 单独约束且更严格，见下文「调用方接收规则（§7.3）」。

---

## 函数参数传递（§12.2）

| 规范要求 | 编译器实现 | 对应代码 | 状态 |
|---------|----------|---------|------|
| `flow` 参数接收所有权 | 参数前缀被忽略 | parser.c:1018–991、1002 | ⚠️ 未实现 |
| `var` 参数冻结实参 | 参数前缀被忽略 | parser.c:1018–991、1002 | ⚠️ 未实现 |
| `const` 参数冻结实参 | 参数前缀被忽略 | parser.c:1018–991、1002 | ⚠️ 未实现 |

> 当前编译器解析函数参数上的属性前缀（`flow`/`var`/`const`），但在内部将所有参数统一视为 `var`（`VIS_DEFAULT`）。参数的所有权/借用检查不在 1.0 范围内，已由 2.0（PB 分支 PB-26）实现并验证，1.0 冻结线不移植。

---

## 调用方接收规则（§7.3）

| 规范要求 | 编译器实现 | 对应代码 | 状态 |
|---------|----------|---------|------|
| `flow` 返回值只能由 `flow` 接收（禁 `static`/`const`/`var`） | 调用点不比较函数返回前缀与接收变量前缀 | — | ⚠️ 未实现（2.0 范围，PB-29 已实现） |
| `static` 返回值可由 `static`/`const` 接收（禁 `flow`/`var`） | 同上 | — | ⚠️ 未实现（2.0 范围） |
| `const` 返回值只能由 `const` 接收（禁 `static`/`flow`/`var`） | 同上 | — | ⚠️ 未实现（2.0 范围） |

> §7.3 与 §12.1 的关系已在 PA-29 收口：**§12.1 矩阵只约束同一作用域内变量之间的赋值**，返回值接收另由 §7.3 约束且更严格——`flow → const` 作为赋值合法（源变量仍在作用域内可冻结），把 `flow` 返回值收进 `const` 变量则非法（无人负责释放，且临时量随语句结束失效）。§12.1 的 `func` 说明与 §7.3 表均注明 `func` 不是变量存储期、不能作接收属性；`const` 行的禁止列已补上 `var`，与 §12.1 禁止 `const → var` 对齐。
>
> 1.0 冻结线不移植该检查。探针验证：`const q void = create_buf(4)`、`var r void = create_buf(4)`、`flow t void = get_ver()`（`get_ver` 返回 `const`）在 A 后端（c / native）均静默编译并正常运行。2.0 线由 PB-29 的 `ret_vis` 调用点检查实现（含六条禁止路径 err 用例）。

---

## 检查范围

| 规范要求 | 编译器实现 | 对应代码 | 状态 |
|---------|----------|---------|------|
| 裸标识符赋值检查 | 完整实现 | parser.c:1176–1158（声明初始化）、2901–2914（表达式赋值） | ✅ 已实现 |
| 表达式级检查 | 仅检查裸标识符 | parser.c:1176–1158、2901–2914 | ⚠️ 部分实现 |
| IR 后端所有权检查 | 无 | irparse.c | ⚠️ 未实现（2.0 范围，PB-26/PB-29 已覆盖） |
| `?=` 安全赋值记号 | 已从语法移除（BNF v2.3）：A 后端 `case TOK_SAFE_ASSIGN` 显式拒绝并提示改用 `=`；IR 前端无该分支，词法 token 落入通用"unexpected token"报错 | lexer.c:613（词法保留）、parser.c:2975–2935（语法拒绝） | ✅ 已按规范移除（PA-20） |
| `?.` / `?(` 安全解引用记号 | 已从语法移除（BNF v2.3）：A 后端 postfix 循环遇 `TOK_SAFE_DOT` 显式拒绝并提示改用 `.()`；IR 前端同口径显式分支。`?(` 从未是 token（词法切成 `?` + `(`），无需处理 | token.h（`TOK_SAFE_DOT` 词法保留）、parser.c:2596–2555、irparse.c:1033–1039 | ✅ 已按规范移除（PA-21） |
| `.()` / `.(T)` 解引用安全检查 | 读侧 `vis_check_usable`；写穿侧 `vis_check_writable`（对 lhs 指针检查）；编译期宽度检查：`sizeof(T)` 超过该指针静态已知所指字节数即报错。所指字节数来自 `malloc(T)`/`malloc(T,n)` 初值或 `&x`（`sym->type->ref->size`），对指针本身重新赋值时清零（保守不检查）| parser.c:2271–2227（可见性）、2512–2524（越界检查）、2845 与 2949（写穿检查）、2847（重赋值撤销）、2333–2366 与 1140/1144（malloc 字节数记录）、ncc.h（`Symbol.pointee_bytes`；2026-09-24 / PA-55 起另有 `Symbol.pointee_type`，`void` 槽的所指类型由字节数记账升级为类型记账，见「语法标准对齐轮实现状态」表）| ✅ 已实现（PA-21）。IR 前端仅支持裸 `p.()`，无 `.(T)` 与宽度检查 |
| 运行期空指针检查 | 无 | — | ⚠️ 未实现（非空由静态规则保证：指针声明必须初始化，§5.1；`malloc` 返回 `NULL` 的运行期检查属 2.0 范围）|

> `vis_check_assign()` 仅在赋值右侧为**单个裸标识符**时触发。`x = y + 1` 或 `x = malloc(...)` 等表达式形式的赋值不经过传递矩阵检查。IR 后端（irparse.c）仅记录可见性值用于 `visof()` 查询，不执行所有权/借用检查。

---

## 指针后缀链与切片（§5.1，BNF v2.5 / v2.7）

| 规范要求 | 编译器实现 | 对应代码 | 状态 |
|---------|----------|---------|------|
| `.(T)` 可处于后缀链任意位置，其后继续下标/成员/切片 | 通用 postfix 循环：每个算子把「已生成的前缀 C 文本」整体包裹后继续 | parser.c:2491–2458（前缀回取）、2470–2517（`deref_prefix` / `deref_step`）、2520 起（`parse_postfix`） | ✅ 已实现（v1.0.2 / PA-25，c/native）。IR 前端属 2.0 待做 |
| 数组类型化解引用 `p.(char[9])[i]` | 数组 cast 生成 `char (*)[9]`（而非 `char**`），下标按整数组步长推进 | parser.c:2508–2475（`c_cast_name`） | ✅ 已实现（PA-25） |
| 空下标 `p[]` = 省略类型的 `p.()`（一层解引用） | postfix `[` 紧跟 `]` 时走 `deref_prefix`；`p.().()` 多级链由此统一写作 `p[][].(i32)` | parser.c:2618–2574 | ✅ 已实现（PA-25） |
| 通用 `void` 指针裸下标/切片 | **分两种情形**（2026-09-24 / PA-55 起）：① 空下标 `p[]`（等价省略类型的 `p.()`）在声明处已静态定下所指类型时（`p void = &x` / `= malloc(T)` / `= &arr`）按该类型还原成有效读写；② 带索引的 `p[i]` 与切片 `p[a..b]` 在 `void` 槽上**仍一律前端报错**并给出修复写法 `p.(T)[i]`（元素宽度虽可知，但该路径未接静态还原），所指类型推不出时裸 `.()` / `->` 也报错 | parser.c:2756–2762（`deref_prefix` 的 void 还原）、2838–2845（空下标分派）、2846–2851（带索引/切片的拒绝）、837–844（来源登记） | ✅ 已实现（PA-25 的拒绝路径 + PA-55 的静态还原路径，§5.1.2）。`tests/pos/void_slot_infer.nc` 覆盖 ①、`tests/err/void_subscript.nc` 覆盖 ②、`tests/err/void_member_unknown.nc` 覆盖「推不出所指类型的 `->`」 |
| 切片读 `p[a..b]` | 生成「指向第 a 号元素的指针」`&p[a]`，长度不进入值本身（C 无数组切片类型）；上下界同为字面量时另记录 `hi-lo` 供 `len(切片变量)` 取用（PA-27）；`[..b]` 省略起点等价 `[0..b]`，长度记为 `b` | parser.c:2630–2626 | ✅ 已实现（PA-25 / PA-27） |
| 切片读赋给固定数组 `T[n] x = p[a..b]` | 按**声明容量**逐元素复制（`memcpy`），非别名 | parser.c:1206–1181、1229–1236 | ✅ 已实现（PA-25） |
| 定长字符数组的字符串初值 `s char[n] = "abc"` | 按声明容量分配**数组存储**（不退化为指针），字面量含结尾 `\0` 整段写入；`strlen+1 > n` 时前端即时报 `string needs N bytes with terminator, array 's' holds M`；元素类型非 `char` 的定长数组用字面量初始化即时报 `cannot initialize array 's' with a string literal; ...`；`len(s)` 取声明容量 | parser.c:1190–1174（初值形态判定与两条诊断）、1203–1207（数组形态声明） | ✅ 已实现（v1.0.2 / PA-31，c/native）。IR 前端两项诊断均无，属 2.0 待对齐 |
| 切片写 `p[a..b] = {v0, v1, …}` 与 `p[a..b] = "abc"` | 值列表逐元素写回（逗号表达式，一条 C 语句）；字符串右值按**字节**整段 `memcpy`（含结尾 `\0`），闭区间右界 `b` 即最后一个可写字节，`strlen > b-a` 时前端报错；两者之外的右值形态报错 | parser.c:2896–2898（2840–2857 为字符串右值分支） | ✅ 已实现（PA-25；§5.1.4 的字符串右值形式与容量诊断由 PA-26 补齐） |
| `T[n] x = malloc(T[n])` 声明退化为指针 | 生成 `T (*x)…`（仅省略最前维度：`void[4][5]` → `void* (*x)[5]`），`malloc` 字节数按元素个数乘算 | parser.c:549–556（`c_decay_suffix`）、1197–1202、2334–2345 | ✅ 已实现（PA-25） |
| 切片变量 `s = a[lo..hi]`（类型推断声明） | 推断自源数组，声明后退化为**元素指针** `E *s = &a[lo]`（视图，不复制），`s.()` 取首元素 | parser.c:1231–1205（指针形态声明）、2480–2484（数组槽裸 `.()` 按最深元素解引用） | ✅ 已实现（PA-27） |
| 推断声明 `v = s.m` / `v = f(a)` 的取值 | 右值为成员访问时取**成员**类型（数组成员按退化处理成 `T*`）、为调用时取被调（或函数指针）的返回类型；聚合体变量本身作右值时保留标签符号，避免把变量名当类型名输出 | parser.c:599–645（`infer_init_type` 的标识符分支） | ✅ 已实现（v1.0.2 / PA-26，c/native）。IR 前端无成员类型推断路径，属 2.0 待回灌 |
| 多维数组声明 `T[a][b]` | C 后缀按源码顺序输出全部维度（`int32_t m[2][3]`） | cgen.c（`c_type_suffix`） | ✅ 已实现（PA-25） |
| 多变量声明带数组类型 `var {aa = "aa", …} char[3]` | 与单变量分支同口径：定长数组保留数组存储（`char aa[3] = "aa";`）并登记声明容量供 `len()` 取用，动态 `char[]` 仍退化为 `char*` 并登记字面量长度；初值不是字符串字面量、元素非 `char`、字面量含 NUL 超容量三种形态即时报错（后两条诊断与单变量共用文案） | parser.c:810–881（多变量分支：815 求 `arr_cap`、835–852 三条诊断、853–855 接 `c_type_suffix`、869–878 登记 `len_known`） | ✅ 已实现（v1.0.2 / PA-34，c/native）。IR 前端不实现多变量声明的数组检查，属 2.0 待回灌；`tests/pos/multi_arr_decl.nc` 与 `tests/err/multi_arr_overflow.nc`、`tests/err/multi_arr_nostr.nc` 覆盖 |

> A 后端不把切片长度编码进值本身：`p[a..b]` 生成的是「指向第 a 号元素的指针」，`b` 只在与 `lo` 同为字面量时被记录下来供 `len(切片变量)` 求 `hi-lo`（parser.c:2635–2598、2606–2617），随后即丢弃；因此接收方为固定数组时按**声明容量**复制、写回时按右值元素个数决定，`b` 越界仍属运行期未定义行为（与 §5.1.2 一致）。

---

## 内置查询与输出内建（§2.3 / §2.3.1，BNF v2.4 / v2.6）

A 后端把 `sizeof` / `typeof` / `alignof` / `offsetof` / `bitoffsetof` / `visof` / `structof` / `unionof` / `holdof` 作为**关键字 token**处理，统一由 `parse_builtin_kw()` 分派（`parse_primary` 的 `switch` 进入，parser.c:2232）；标识符路径的内置函数表看不到这些关键字。`len` 与之相反，走 `parse_primary` 的标识符内置函数表（parser.c:2287–2261）。

| 规范要求 | 编译器实现 | 对应代码 | 状态 |
|---------|----------|---------|------|
| `len(x)` 逻辑长度 | 编译期常量：数组=各维元素个数乘积；动态字符串 `char[]` / 推断字符串=字面量长度；切片变量=`hi-lo`（两侧须为字面量）。长度在声明处登记到符号表，整变量重新赋值后撤销记录（保守报错而非返回过期值）；静态不可知或非标识符实参即时报错 | parser.c:1250–1235（登记）、2231–2252（求值与报错）、2830–2833（重赋值撤销）、ncc.h（`Symbol.len_known` / `logical_len`） | ✅ 已实现（v1.0.2 / PA-27，c/native）。IR 前端 `len()` 走 `ve[]` 表，静态不可知时返回 0 而非报错（`tests/err/len_unknown.nc` 经 `IR_ERR_SKIP` 跳过），2.0 待对齐 |
| `structof(T, m, p)` / `unionof(T, m, p)` / `holdof(T, m, p)` 从属查询 | 生成 `((void*)((char*)(p) - offsetof(T, m)))`，即反推成员所属聚合体首地址；`T` 必须是聚合体，`m` 必须是其成员，`structof` 仅接受 `struct`、`unionof` 仅接受 `union`、`holdof` 两者通用 | parser.c:1798–1757（成员查找）、1821–1858（三参分派与诊断） | ✅ 已实现（v1.0.2 / PA-22，c/native）。IR 前端属 2.0 布局待做，`ir-*` 经 `IR_SUBSET`/`IR_ERR_SKIP` 跳过 |
| `bitoffsetof(T, m)` 位域位偏移 | 按声明顺序的位布局：以前置非位域成员为锚点，用 C 自身 `offsetof`/`sizeof` 求存储单元起点，再累加整单元与单元内位；对齐以基类型 `sizeof` 近似（基本整型 `align == size`）；目标非位域、基类型宽度未知、同一组成员存储类型不一致均报错 | parser.c:1916–1947 | ✅ 已实现（v1.0.2 / PA-22，c/native）。IR 前端未实现 |
| `alignof(T)` 类型对齐 | 编译期由编译器算出对齐值并把**字面量**输出到 C（不再依赖 C 的 `_Alignof`）：数组递归取元素对齐、结构体/联合体取最宽非位域成员对齐、`void` 按通用指针计 8、其余类型取声明对齐（`parse_type` 的栈上 `CType.align` 为 0 时回落到 `type_default_align(kind)`）| parser.c:1832–1792 与 2261–2267（两处分派）、type.c:71–102（`type_align`）、ncc.h:400（原型）| ✅ 已实现（v1.0.2 / PA-24，c/native）。IR 前端仍按 IR 槽模型对任何 `alignof` 固定返回 8（`irparse.c:759`），2.0 布局待对齐 |
| 聚合体成员列表分隔符 | 成员以**空白**分隔（`Point struct { x i32 y i32 }`），逗号被拒绝（`expected member name, got ','`） | parser.c:105（成员解析） | ✅ 与 BNF 一致（v1.0.2 / PA-23）：文档 4 处示例原用逗号分隔，已改回空白分隔，语法与实现均未改动 |
| `print` 两种形态（§2.3.1） | 实参首 token 为字符串字面量 → **C `printf` 直通**（格式符与参数须自行匹配，不自动换行，多余参数不拼接）；否则 → `printf("%lld\n", (long long)expr)`，即按十进制整数打印该值并换行（对指针实参打印其地址整数值） | parser.c:2419–2391 | ✅ 已实现且文档已定义（v1.0.2 / PA-30 补 §2.3.1 口径）。IR 前端**未内建** `print`（直出成未定义符号，链接期报错），2.0 待做 |
| `puts` 输出字符串（§2.3.1） | C `puts` 直通：实参须为字符串指针，自动补换行；把整型（`p.(i32)`）传给 `puts` 会按地址解引用，运行期崩溃或乱码 | parser.c:2439–2406（`puts`/`printf` 普通直通调用） | ✅ 双前端一致（v1.0.2 / PA-30 补文档口径；文档 5 处 `puts(整型)` 示例已改为 `print(整型)`） |

---

## 条件与分支（§6，BNF `<if-stmt>`）

| 规范要求 | 编译器实现 | 对应代码 | 状态 |
|---------|----------|---------|------|
| `if <expr> <block> [else (if-stmt \| block)]`——`else if` 递归形式 | A 后端 `parse_if_stmt` 用 `while (cur_tok == TOK_ELSE)` 循环消化整条 `else if` 链；IR 前端 `ir_if_stmt` 在 `else` 后 peek 到 `if` 时递归自身，多级分支共用同一条 `JZ`/`JMP` 标签链 | parser.c:1763–1738、irparse.c:1771–1793 | ✅ 双前端一致（v1.0.2 / PA-28 补 IR 侧；此前 `else if` 在 ir-c / ir-native 报 `expected '{', got 'if'`） |
| `else` 分支必须是块 | IR 前端 `ir_block` 严格要求 `{`；A 后端语法上对 `else` 后接非块语句不报错，但生成的 C 缺少分隔符（`elseputs(...)`）而延迟到 C 编译期失败 | parser.c:1779–1736、irparse.c:1759–1769（`ir_block`） | ✅ 有效语义与 BNF 一致（只有块形式可用）；A 后端该分支的宽松属代码生成缺陷，无文档承诺该写法，未纳入本版 |
| `switch` 的 case 不穿透（BNF v2.11 / 中英 §6.2） | 每个 case 体末尾自动插入 `break;`，无 `fallthrough` 关键字 | 实测见上文 F3 | ✅ 与规范一致（1.x 不提供显式穿透写法） |
| `<for-step> ::= <expr>`（BNF v2.11 放宽） | step 位置接受任意表达式并原样下沉到 C 的第三段，不限赋值/`++`/`--` | 实测见上文 F4 | ✅ 与放宽后的规范一致（1.x 不加形态限制） |
| 指针必须初始化、语言层无空指针字面量（§5.1） | 与上文「运行期空指针检查」行同口径：非空由静态规则（声明必须初始化）保证 | 见「检查范围」表末行 | ✅ 已实现；`malloc` 返回 `NULL` 的运行期检查属 2.0 |

---

## `is` 模式匹配（§6.3，规范标题自 PA-42 起编号；BNF v2.2）

| 规范要求 | 编译器实现 | 对应代码 | 状态 |
|---------|----------|---------|------|
| `is` 仅配合 `while`（块形式） | A 后端 `while_depth` 计数守卫：`case TOK_IS` 在循环体外直接报错；IR 前端 `is_val_vreg < 0` 守卫 | parser.c:1489–1444、1558–1565、irparse.c:2021–2024 | ✅ 已实现（v1.0.2 补 A 后端守卫，PA-18） |
| `do` 不支持 `is` | A 后端进入 `do` 体前把 `while_depth` 清零、退出恢复；IR 前端 `do` 分支置 `is_val_vreg = -1`。嵌套在 `while` 内的 `do` 同样拒绝，不会静默匹配外层条件值 | parser.c:1504–1461、1558–1565、irparse.c:1896、1909 | ✅ 已实现（双前端显式拒绝，v1.0.2 补 IR 侧，PA-19） |
| `is <int-literal>` / `is -<int>` | `== v` / `== -v` | parser.c:1388–1359 | ✅ 已实现 |
| `is lo..hi` 闭区间 | `>= lo && __is_val <= hi` | parser.c:1397–1356 | ✅ 已实现 |
| `is <visibility-enum>` | 比较 `NH_*` 常量 | parser.c:1416–1376 | ✅ 已实现 |
| `is <enum-variant>` / 已知常量 | `TOK_IDENTIFIER` 分支按值比较 `== pat` | parser.c:1407–1368 | ✅ 已实现 |
| `is _` 通配符恒匹配 | 恒真分支（A 后端 `if (!__is_matched && (1) && …)`，IR 侧不发比较与 JZ） | parser.c:1376–1338、irparse.c:2029–2032 | ✅ 已实现（v1.0.2 补全） |
| `is <identifier>` 按值比较（BNF v2.11 定案，绑定/解构留 2.0） | 生成 `__is_val == pat`，不引入新绑定 | parser.c:1407–1368 | ✅ 与规范一致；变量绑定属 2.0（PB-27 类型感知后再议） |
| 多个 `is-clause` 首个匹配即止（无 fallthrough） | A 后端：`while` 声明 `int __is_matched`（parser.c:1478）并在每轮迭代清零（parser.c:1488），每个子句的守卫为 `if (!__is_matched && <pat> && (__is_matched = 1))`（parser.c:1379、1340），故同一条 `while` 体内至多执行一个 `is-clause` 块，前子句不匹配时后续子句照常执行。IR 前端：各 is-clause 仍是**并列**独立 `if`（块末无跳到合并出口的 `jmp`），条件重叠时连续执行 | parser.c:1379、1340、1431、1441、irparse.c:2140–2144 | ✅ A 后端已实现（v1.0.2 / PA-16，`tests/pos/is_no_fallthrough.nc` 覆盖）；⚠️ IR 前端未实现（1.0 线的 2.0 预览与 PB 线同样待对齐） |
| `is <pat> => <stmt>` 单语句 | 已移除（BNF v2.2 / PA-13），双前端仅接受块形式 | parser.c:1430–1387、irparse.c:2140–2144 | ✅ 已按规范移除 |
| 反向范围 `lo > hi` 编译期校验 | `is lo..hi` 两侧皆字面量时比较，`lo > hi` 即时报 `empty 'is' range %lld..%lld: lo must be <= hi` | parser.c:1561–1580（1574 报错） | ✅ 已实现（2026-09-24 / PA-57，A 后端；`tests/err/is_empty_range.nc` 覆盖，入 `IR_ERR_SKIP`：PA 线 IR 前端仍不校验） |
| 结构体解构 / ADT 变体解构 | 预留语法，未实现 | — | ⚠️ 预留（依赖类型系统，PB-27.10/11） |

---

## 存储期属性（§11.1）

| 规范要求 | 编译器实现 | 对应代码 | 状态 |
|---------|----------|---------|------|
| `const` 模块级静态 / 块级自动 | C 后端自然实现 | parser.c:884、1091 | ✅ 隐式实现 |
| `flow` 动态分配 + 自动释放 | 块/函数退出时对**堆所有权**的 `flow` 自动 free；是否属于堆所有权由存储来源登记决定（`Symbol.no_auto_free`，零初始化默认保持释放），`ownership_transferred`（返回值转移与 `flow → flow` 转移后的失效源）与 `no_auto_free`（非堆来源）都会豁免 | parser.c:1162–1139（声明期登记）、2915–2923（整变量重绑定重登记）、1285–1292（函数退出）、1644–1656（块退出） | ✅ 已实现（v1.0.2 / PA-32 收紧为「只发给堆右值」）。IR 前端不做所有权与自动释放（2.0） |
| `flow` 返回值所有权转移 | `ownership_transferred` 标志 | parser.c:1586–1549 | ✅ 已实现 |
| `static` 静态存储 | C `static` 关键字 | cgen | ✅ 已实现 |
| `flow` 绑定非堆右值（字符串字面量 / `&x` / 聚合初值） | 声明期登记存储来源并豁免自动释放：`flow s char[] = "abc"` 生成 `char* s = "abc";` 且退出点不再发 `free(s)`；`flow p void = &n` 同样不释放（原实现两处都会对静态只读段／栈地址执行 `free`，运行期中止且无任何输出） | parser.c:1162–1139（来源登记）、1285–1292 与 1644–1656（两处豁免）、ncc.h（`Symbol.no_auto_free`） | ✅ 已修复（v1.0.2 / PA-32，c/native；`tests/pos/flow_no_free.nc` 固化）。中英 §11.1 已定案四类右值口径。IR 前端无自动释放路径，2.0 待回灌 |
| `flow` 堆所有权被重绑定后的旧堆块 | 整变量重绑定按新右值重判来源，不在重绑定点插入释放：`flow q void = malloc(i32)` 后 `q = &n` → 旧块**泄漏**（保守选择，避免经别名双释放） | parser.c:2963–2923 | ⚠️ 已知限制（v1.0.2 / PA-32 决策②刻意保留）：精确回收属 2.0 所有权转移分析，中英 §11.1 已写明 |

> `const` 的存储期区分（模块级静态 / 块级自动）由 C 后端编译器的自然语义实现——ncc 不显式区分两种 `const`，但生成的 C 代码中，文件作用域 `const` 自然获得静态存储期，块作用域 `const` 自然获得自动存储期。`vis_check_transfer()` 对两种 `const` 一视同仁。

---

## 语法固定轮实测记录（2026-09-21，BNF v2.11；F1 / F2 与 F8 / F9 / F10 均已于 2026-09-24 修复，F10 为同日普查新增）

> 本轮把 `BNF.md` / `Chinese.md` / `English.md` 统一为**纯设计层**文档：所有"哪个后端已落地、待办、行号、测试计数"的陈述从这三份文档移出，集中到本文件。下表是固定设计前对 v1.0.2 A 后端（backend `c`）逐条探针得到的实现状态，样本与生成产物在 `ncc/build/probe1.nc` ~ `probe9.nc`（gitignore，不入库）。

| 编号 | 探针写法 | 实测结果 | 定性 |
|------|---------|---------|------|
| F1 | `P struct { x i32 = 7  y i32 }` | 前端接受，生成 `typedef struct P { 7int32_t x; int32_t y; } P;`，tcc 报 `error: invalid number` | **A 后端代码生成缺陷**（成员默认值被当作类型名前缀输出）。设计侧 `<field-decl> ::= … [ "=" <expr> ]` 意图明确，产生式保留；修好前该写法属"语法可写、不可编译"。→ **已修复（2026-09-24，PA-49）**：初值表达式文本在聚合体内截回、挂到成员符号，变量声明处展开为 C 指定初始化器（`P p = { .x = 7 };`），`tests/pos/struct_member_default.nc` 固化。**2026-09-24 / PA-56 追加**：同口径推广到**聚合类型数组变量**，`a P[3]` → `P a[3] = { { .x = 7, .y = 8 }, { .x = 7, .y = 8 }, { .x = 7, .y = 8 } };`，带初值时元素内省略成员与尾数元素各自补默认（`tests/pos/array_member_default.nc`）；展开文本超出缓冲上限时即时报 `aggregate initializer is too large to expand its member defaults`（`tests/err/default_init_overflow.nc`） |
| F2 | `print("%d\n", i)` | 生成的 C 里字符串常量**含真实 0x0A 字节**（`printf("` + 换行 + `", i)`），而非 `\n` 两字符 | **A 后端代码生成缺陷（可移植性）**。tcc 作为扩展容忍，gcc/clang/MSVC 会报 unclosed string literal；当前门禁全在 tcc 下跑，故未暴露。→ **已修复（2026-09-24，PA-50）**：字面量经 `cgen_string_lit()` 按 C 转义序列重新编码后落地，`xmake test` 的 c 后端阶段新增非 tcc 编译器 `-fsyntax-only` 抽查（见「测试覆盖」末段） |
| F3 | `switch (1) { case 1: … case 2: … default: … }` | 只执行命中的那个 case；生成的 C 每个 case 体末尾自动插 `break;` | 实现即为**不穿透**。设计已在 BNF §6 / 中英 §6.2 固定为「case 不穿透，1.x 无显式穿透写法」，无需改代码 |
| F4 | `for i = 0; i < 3; bump(&cnt) { … }` | 编译通过，原样生成 `for (int32_t i = 0; i < 3; bump(&cnt))` | 实现比旧产生式宽。设计已按实测放宽：`<for-step> ::= <expr>`（1.x 不做形态限制，文档不再承诺"只允许赋值/++/--"） |
| F5 | `a fx32 = 1` / `b fx64 = 2` / `c string = "abc"` | 分别生成 `int32_t a`、`int64_t b`、`char* c`，编译运行均通过 | `fx32` / `fx64` **语法与类型系统已接通但无定点语义**（按同宽整型下降，小数点位置未定义，2.0 定案）；`string` → `char*` 与 `char[]` 同型口径一致 |
| F6 | `short s = 1` / `int i` / `long l` / `float f` / `double d` 写在类型位置 | A 后端逐个报 `unexpected token 'short' in expression` … `'double' in expression`，共 5 error 后终止 | 与本轮设计裁定一致：基本类型收敛为 **16 个**，C 风格别名不再是类型（`short`→`i16`、`int`→`i32`、`long`→`i64`、`float`→`f32`、`double`→`f64`）。两前端与全部 `tests/` `examples/` 均无消费点，故属"文档追平实现"而非行为变更 |
| F7 | `while w -= 1 { is 3 {…} is 1..5 {…} }` 重叠模式 | A 后端只执行首个匹配子句（`__is_matched` 每轮迭代重置） | 与"首个匹配即止、无 fallthrough"设计一致。IR 前端仍生成并列 `if`，见「`is` 模式匹配」表与下文 2.0 差异表 |
| F8 | 四种数组容量写法逐个编译：`x i32[5]` / `x i32[5...]` / `x i32[...]` / `x i32[...5]`（**A 后端 c/native**） | `[5]` → `int32_t x[5]` 正常；`[5...]` → **前端拒绝**（`expected ']', got 'TOK_ELLIPSIS'`）；`[...]` → 退化为 `int32_t* x`，**没有默认容量 8**；`[...5]`（设计标为已废弃的反序写法）→ `int32_t x[5]` 正常。`[5..]` / `[..5]` / `[..]` 三种点号写法一律拒绝 | **A 后端与设计的四档写法不一致**：规范里的 `[N...]` 在 1.0 发布线根本写不出来，反而是「已废弃」的 `[...N]` 可用；`[...]` 的默认容量 8 只在 IR 槽模型里成立（`tests/pos/ir_slice.nc:11` 注释即按 8 槽写）。→ **已修复（2026-09-24，PA-51 / PA-57）**：`parse_type` 的数组后缀循环按 v2.11 四档逐档消化，省略容量落 `NH_DEFAULT_ARRAY_CAP = 8`（`ncc.h:43`），`tests/pos/array_capacity_tiers.nc` 固化七种写法与多维形态 |
| F9 | `a i32[...] = {1, 2, 3}`（A 后端） | 生成 `int32_t* a = {1, 2, 3};`，tcc 报 `'}' expected (got ",")` | **A 后端代码生成缺陷**：省略容量的数组类型退化为指针后仍按聚合初值输出，生成非法 C。与 F8 同属 `[...]` 一档，修 F8 时需一并给出诊断口径。→ **已修复（2026-09-24，PA-51 / PA-57）**：现生成 `int32_t a[8] = {1, 2, 3};`（真数组存储，`len(a) == 8`），`tests/pos/array_capacity_tiers.nc` 的 `cap2` 分支即该形态 |
| F10 | PA-50 修好后，用 clang 对全部门禁产物 C 跑 `-fsyntax-only`（2026-09-24 一次性普查） | 47 份产物 C 中 45 份通过；`ir_builtin.nc`、`ir_struct.nc` 被拒：`incompatible integer to pointer conversion`（`p.(i32) = 42` 下降为 `*(void**)(p) = 42`；`Person p = {100, 25, 90}` 把 `100` 赋给 `char[]` 成员） | **A 后端的可移植性缺口（与 F2 不同源）**：tcc 对 int↔指针隐式转换只警告，clang/gcc/MSVC 判为错误。tcc 下用例语义正常，故不阻塞 1.0。→ **已修复（2026-09-24，PA-55 / PA-57）**：`ir_builtin` 侧是真实的 lowering 缺陷（新增 `Symbol.pointee_type`，解引用按声明处登记的所指类型还原，见下文「`void` 通用槽的所指类型」行），`ir_struct` 侧是用例初值写错（`100` 交给 `char[]` 成员）；`xmake.lua` 的 `STRICT_SKIP` 名单已整体删除，抽查恢复全覆盖（c 后端实际编译的 56 份产物 C 全部通过 clang `-fsyntax-only`） |

探针复现：`ncc/build/probe1.nc` ~ `probe9.nc`（gitignore，不入库），命令 `./ncc.exe build probeN.nc -o probeN.exe` 后查看同目录 `probeN.exe.c`。

---

## 语法标准对齐轮实现状态（2026-09-24，BNF v2.11 逐产生式普查 / PA-57）

> 上一节是 2026-09-21 固定设计时的探针台账；本节是同日 PA-57 轮按 v2.11 **定稿后**的产生式逐条普查 A 后端得到的实现状态，行号已按当前 `parser.c` / `lexer.c` / `module.c` 核对。设计文档（`BNF.md` / 中英规范 / 两份元素表）在本轮一字未改。

| BNF 产生式 | 编译器实现 | 对应代码 | 状态 |
|-----------|----------|---------|------|
| §1.4 `<empty-stmt> ::= ";" \| "#"`（语句级与顶层声明级） | `eat_stmt_terminator()` 在 `parse_statement` 外层的包装里吞掉语句尾的 `;` / `#`；顶层声明循环与 `parse_module` 的 use/link 循环各带独立分支并随后 `skip_newlines`，故 `module main;`、struct 定义后 `};`、函数后 `}#`、以及单独成句的 `;` / `#` 都是合法空语句 | parser.c:11–25（`eat_stmt_terminator` + 包装）、478–546（use/link 循环）、1686–1688 与 1731（顶层声明循环） | ✅ 已实现（PA-57，c/native）。`tests/pos/stmt_terminators.nc` 覆盖；IR 前端不解析顶层终止符，用例未入 `IR_SUBSET` 自动跳过 |
| §1.3 `<int-literal> ::= ["+" \| "-"] <digits>` | 一元正号不作为表达式运算符，而是在 `parse_primary` 与字面量一起消化（`pos = +5`、`neg + +7` 可写）；`0x` 走 `strtoll(base 0)`，`0b` 显式按基 2 解析 | parser.c:2602–2607（前导 `+`）、lexer.c:261–262（`0b` 取值） | ✅ 已实现（PA-57）。此前 `0b1011` 被 `strtoll(…, 0)` 截断成 0，且全仓 `tests/` 与 `examples/` 无一例用 `0b`；现由 `tests/pos/literal_forms.nc` 断言值 |
| §3 `<type-name> ::= … \| "(" <type-name> ")"` | `parse_type` 补 `case TOK_LPAREN` 递归解析内层类型，括号类型之后仍可接数组后缀（`a (i32)[3]`）；「声明符括号」与「括号类型」的歧义由 `is_paren_type_ahead()` 前瞻判定，函数声明 `pair(x i32)` 与分组表达式 `(a+b)` 不受影响 | parser.c:121–127（消歧）、395 起（`case TOK_LPAREN`）、1266（声明位）、1712–1714 与 1928–1930（`for` 初异位） | ✅ 已实现（PA-57）。`tests/pos/type_forms.nc` 覆盖 |
| §3 `<array-size>` 四档（`[N]` / `[N..]` `[N...]` / `[..]` `[...]` / `[..N]` `[...N]`） | 数组后缀循环逐档消化：有 `<dim>` 取该容量，`..` / `...` 只作「动态/省略」标记；**省略容量落显式默认容量 8**（不再退化为指针），已废弃的反序写法继续可用；多维各维独立生效，`len()` 取乘积 | parser.c:410–448（四档循环）、ncc.h:43（`NH_DEFAULT_ARRAY_CAP`） | ✅ 已实现（PA-51 / PA-57，闭合 F8/F9）。`tests/pos/array_capacity_tiers.nc` 覆盖七种写法 + `i32[2][3]`；IR 侧的 `[...]` 8 槽模型本就成立，本用例未入 `IR_SUBSET` |
| §5 `<aggregate-init>` 的 `<designator> ::= "." <id> "=" <expr>` | `parse_init_list` 用三 token 前瞻（`.` IDENT `=`）识别指定式，被点名成员记在**本层局部数组**（上限 `MAX_INIT_DESIGATORS 32`；PA-56 起由 `ParserState` 字段改为各层记账，修掉嵌套层互相覆写的隐患）；`member_default_text()` **按成员名过滤**拼接默认值——未点名的取 §3.1 默认值、点名的不重复拼，故支持乱序与部分指定 | parser.c:635–643（识别与记录）、911–935（按名过滤）、1533（声明点调用） | ✅ 已实现（PA-57，与 PA-49 的成员默认值协同）。`tests/pos/designator_init.nc` 覆盖乱序 / 部分 / 全指定与位置式共存 |
| §6 `<for-init> ::= <id> "=" <expr> \| <var-decl>` | `TOK_FOR` 分支前瞻判定初始化位是否为声明，是则走声明生成并用 `cgen_take_prefix()` 回取文本拼进 C 的 for 头，故 `for i i32 = 0; …`、`for m (i32) = 0; …`、`for k = 2; …`（推断）三形态皆可写 | parser.c:1709–1727 | ✅ 已实现（PA-57）。`tests/pos/for_init_forms.nc` 另覆盖 `<for-step>` 的 `i++` / 赋值 / 函数调用三种表达式 |
| §6 `<pattern>` 的带符号整型与负区间 | `is +5` 与 `is -3` 按符号+字面量整体解析；`is -9..-5` 的负上下界可写（此前负数被切成两段 token） | parser.c:1545–1580 | ✅ 已实现（PA-57）。`tests/pos/is_pattern_forms.nc` 覆盖负字面量 / 带符号字面量 / 负区间 / 枚举变体 / 裸标识符按值 / `_` 与首匹配独占 |
| §6 `<pattern>` 闭区间 `lo <= hi` 的编译期校验 | `is lo..hi` 在两侧都是字面量时编译期比较，`lo > hi` 即时报 `empty 'is' range %lld..%lld: lo must be <= hi`（此前静默生成永假条件） | parser.c:1561–1580（含 1574 的报错） | ✅ A 后端已实现（PA-57）；`tests/err/is_empty_range.nc` + `.expect` 覆盖，入 `IR_ERR_SKIP`（PA 的 IR 前端不做该校验，属 2.0 待对齐口径） |
| `void` 通用槽的所指类型（§5.1 / §5.1.2 的省略类型解引用） | 声明期把「所指类型」登记进符号表：`malloc(T)` 的 `T`、`&x` 的 `x` 类型（数组取最深元素类型）→ `Symbol.pointee_type`，并顺带补 `pointee_bytes`；此后裸 `p.()`、空下标 `p[]`、`->m` 都按该类型还原成 `(*(T*)(p))`。对指针本身重新赋值时撤销记录（`lhs_was_deref` 区分 `p.(T) = v` 与 `p = v`），静态不可知即不检查；`->` 遇到推不出所指类型的 `void` 槽即时报错而非产非法 C | parser.c:837–844（`record_init_pointee`）、2756–2762（`deref_prefix` 的 void 分支）、2917–2930（`->` 的 void 分支与报错）、3132–3135（重赋值撤销）、ncc.h:144（`Symbol.pointee_type`） | ✅ 已实现（PA-55 / PA-57，闭合 F10）。`tests/pos/void_slot_infer.nc` 覆盖四类来源、`tests/err/void_member_unknown.nc` 覆盖报错路径（后者入 `IR_ERR_SKIP`） |
| §2 `<top-level>` 的 use / link 交错 与 `<use-decl> ::= "use" <id> { "." <id> }` | `parse_module` 把 `use` 与 `link` 合入同一分派循环（BNF 允许任意交错；此前两条各占独立循环，`link` 写在 `use` 之前会被整条跳过），`use` 支持点号多段名；`module_import` 把点号名按目录层级找文件（`std/io.nc`），并保留点号原样候选以兼容名为 `a.b` 的单段模块 | parser.c:478–546（合并循环 + 点号拼接）、module.c:83–104（点号→目录候选） | ✅ 已实现（PA-57）。`tests/pos/module_top_forms.nc`（模块形态，无 `func main`，走 `-c` 编译）覆盖 `use sys.path` + `link "kernel32" Kernel32` + `use stdio` 交错与 `[[export ".mydata"]]` |
| §7 `<bracket-attr> ::= "export" [<string-literal>]` | `[[ … ]]` 属性循环在属性名之后消化一个可选字符串字面量（`export` 对 C 产物无副作用，仅记属性） | parser.c:864–875 | ✅ 本轮**只补覆盖、未改代码**：`[[export ".section"]]` 形态此前无用例，现由 `tests/pos/module_top_forms.nc` 固化。注意 `docs/BNF.md:409` 的例子注释写法与产生式不一致，见 `TODO-PA.md` **PA-60** |
| §8 `<cooking-item>` 四形态 | A 后端落地三形态：`<static-assert>`、`const NAME <type> = <expr>`、**`<ct-func-def>` + `<ct-func-call>`**（`const sq(x) = x * x` 定义、`sq(5)` / `sq(sq(2))` 调用，2026-09-25 / PA-58 起，形参上限 4、体源文本上限 512 字节，与 IR 侧同；展开后超上限即报错不截断）；块内裸 `<var-decl>`（`K i32 = 3`）仍只报专属诊断不落地 | parser.c:2321–2396（`ct_funcs` / `ct_func_find` / `ct_func_call`）、2431–2463（`pc_prim` 的调用形态）、2648–2704（`parse_cooking_block` 的定义分支） | ✅ 三形态已实现（`examples/06_cooking.nc` 四后端可编译）；裸 `<var-decl>` 仍是 ⚠️ 仅诊断，登记为 `TODO-PA.md` **PA-59**（结案口径：1.x 不落地该形态）。`static_assert` 与 `align n { }` 复验可用 |

> 本表各行与 `TODO-PA.md` 的 PA-57 条目 ①~⑫ 一一对应；`is` 区间校验一项 PB 的 IR 前端早已实现（见文末差异表），1.0 线这次是补齐 A 后端，回灌方向为 PA → PB 的 `parser.c`。

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
| tests/pos/len_builtin.nc | `len()` 三类取值：数组容量（多维取乘积）、动态字符串取字面量长度、切片取 `hi-lo` | ✅（v1.0.2 / PA-27 新增，c/native；未列入 `IR_SUBSET`，IR 后端自动跳过） |
| tests/err/len_unknown.nc | `len()` 实参逻辑长度静态不可知时前端报错 | ✅（v1.0.2 / PA-27 新增，c/native；IR 前端返回 0 不报错，经 `IR_ERR_SKIP` 跳过） |
| tests/pos/elseif_chain.nc | `else if` 链（含链中命中、末尾无 `else`、全不命中、循环体内、`else` 普通块） | ✅（v1.0.2 / PA-28 新增，四后端输出一致） |
| tests/pos/alignof_builtin.nc | `alignof()` 编译期对齐：标量、`void` 通用指针=8、结构体/联合体取最宽成员、嵌套结构体、数组取元素对齐 | ✅（v1.0.2 / PA-24 新增，c/native；未列入 `IR_SUBSET`，IR 后端自动跳过） |
| tests/pos/slice_str_infer.nc | 切片赋值的字符串右值（含偏移切片与结尾 NUL）、`talk = xiaoming.say` 取成员类型、`result = calc(10, 20)` 取函数指针返回类型 | ✅（v1.0.2 / PA-26 新增，c/native；未列入 `IR_SUBSET`，IR 后端自动跳过） |
| tests/err/slice_str_overflow.nc | 字符串右值放不下切片区间时前端报错（`strlen+1 > b-a+1`） | ✅（v1.0.2 / PA-26 新增，c/native；IR 前端无 `.(T)`，经 `IR_ERR_SKIP` 跳过） |
| tests/pos/print_forms.nc | `print` 两种形态（`print("fmt %s\n", s)` 的 printf 直通与 `print(expr)` 的整型打印）+ `puts` 字符串直通 | ✅（v1.0.2 / PA-30 新增，c/native；IR 前端未内建 `print`，未列入 `IR_SUBSET` 自动跳过） |
| tests/pos/is_no_fallthrough.nc | 多个 `is-clause` 首个匹配即止：重叠模式（`is 3` 与 `is 1..5`）只执行前者、不匹配时后续子句照常执行、标记每轮迭代重置 | ✅（v1.0.2 / PA-16 新增，c/native；IR 前端未实现该语义，未列入 `IR_SUBSET` 自动跳过） |
| tests/pos/str_array_init.nc | 定长 `char[n]` 配字符串字面量 → 数组存储、可原地改写、`len()` 取声明容量；刚好放下（`strlen+1 == n`）的边界 | ✅（v1.0.2 / PA-31 新增，c/native；未列入 `IR_SUBSET`，IR 后端自动跳过） |
| tests/err/str_array_overflow.nc | 字面量含结尾 `\0` 超过声明容量时前端报错 | ✅（v1.0.2 / PA-31 新增，c/native；IR 前端无容量检查，经 `IR_ERR_SKIP` 跳过） |
| tests/err/str_array_bad_elem.nc | 非 `char` 元素数组用字符串字面量初始化时前端报错 | ✅（v1.0.2 / PA-31 新增，c/native；IR 前端无该检查，经 `IR_ERR_SKIP` 跳过） |
| tests/pos/borrow.nc | `flow`→`var` 借用 + 解冻 | ✅ |
| tests/pos/flow.nc | `flow` 块级自动释放 | ✅ |
| tests/pos/flow_no_free.nc | `flow` 绑定字面量／`&x`／重绑定为非堆来源时不再生成 `free`，`malloc` 堆所有权仍正常 | ✅（v1.0.2 / PA-32 新增，c/native；未列入 `IR_SUBSET`，IR 后端自动跳过） |
| tests/pos/flow_move.nc | `flow → flow` 所有权转移的声明式与赋值式两种写法均可编译运行，堆对象只被释放一次 | ✅（v1.0.2 / PA-33 新增，c/native；未列入 `IR_SUBSET`，IR 后端自动跳过） |
| tests/err/flow_move_frozen.nc | 源被 `var` 借用（冻结）时转交 `flow` 所有权报编译期错误（§14.2 分析表口径） | ✅（v1.0.2 / PA-33 新增，c/native；入 `IR_ERR_SKIP`，IR 前端不做所有权/借用检查） |
| tests/pos/multi_arr_decl.nc | 多变量声明的定长数组按声明容量分配数组存储、`len()` 取容量，动态 `char[]` 退化为指针且 `len()` 取字面量长，标量多变量声明不受影响 | ✅（v1.0.2 / PA-34 新增，c/native；未列入 `IR_SUBSET`，IR 后端自动跳过） |
| tests/err/multi_arr_overflow.nc | 多变量声明的定长数组初值含 NUL 超声明容量时报与单变量同文案的容量错误 | ✅（v1.0.2 / PA-34 新增，c/native；入 `IR_ERR_SKIP`） |
| tests/err/multi_arr_nostr.nc | 定长数组类型的多变量声明用非字符串字面量初值时报错，不再静默丢弃数组后缀 | ✅（v1.0.2 / PA-34 新增，c/native；入 `IR_ERR_SKIP`） |
| tests/pos/transfer.nc | `flow` 返回值所有权转移 | ✅ |
| tests/pos/struct_member_default.nc | 聚合成员默认值：无初值全取默认、位置初值只覆盖前若干员、union 首个默认成员且用户初值优先；同表固化字面量转义（`\n` / `\t` / `\"` / `\\`）在产物 C 中的形态 | ✅（2026-09-24 / PA-49 + PA-50 新增，c/native；未列入 `IR_SUBSET`，IR 后端自动跳过） |
| tests/pos/stmt_terminators.nc | §1.4 `<empty-stmt>`：`module main;` / `use stdio;` / struct 后 `};` / 函数后 `}#` / 语句尾 `;` 与 `#` / 独立成句的 `;` 与 `#` 全部可用 | ✅（2026-09-24 / PA-57 新增，c/native；未列入 `IR_SUBSET`，IR 后端自动跳过） |
| tests/pos/literal_forms.nc | §1.3 字面量：十/十六/二进制整数十种带符号形态（含 `0b1011 == 11`）、浮点的 e 计数法与带符号形态、字符转义 `'A' '\n' '\0' '\\' '\''`、字符串转义配 `len()`、`bool` | ✅（2026-09-24 / PA-57 新增，c/native；二进制取值此前被 `strtoll(base 0)` 截断成 0，本用例是该缺陷的回归开关） |
| tests/pos/type_forms.nc | §3 `<type-name>` 的括号类型（`v (i32) = 7`、`a (i32)[3]`）、带符号字面量在表达式里混用（`neg + +7`）、取址后裸 `.()`、`->` 成员读写 | ✅（2026-09-24 / PA-57 新增，c/native；未列入 `IR_SUBSET`，IR 后端自动跳过） |
| tests/pos/array_capacity_tiers.nc | §3 `<array-size>` 四档七种写法（`[N]` / `[N..]` `[N...]` / `[..]` `[...]` / `[..N]` `[...N]`）各自可写、省略容量取默认容量 8、`len()` 取声明容量、多维 `i32[2][3]` 取乘积 | ✅（2026-09-24 / PA-51 + PA-57 新增，c/native；IR 侧 `[...]` 的 8 槽模型另有 `ir_slice.nc`，本用例未入 `IR_SUBSET`） |
| tests/pos/designator_init.nc | §5 `<designator>` 指定式初值：乱序、部分指定、与 §3.1 成员默认值共存（未点名的取默认值），以及与位置式初值互不干扰 | ✅（2026-09-24 / PA-57 新增，c/native；未列入 `IR_SUBSET`，IR 后端自动跳过） |
| tests/pos/for_init_forms.nc | §6 `<for-init>` 的三种声明形态（`i i32 = 0` / `k = 2` 推断 / `m (i32) = 0` 括号类型）× `<for-step>` 的 `i++` / `i--` / 赋值 / 函数调用 | ✅（2026-09-24 / PA-57 新增，c/native；未列入 `IR_SUBSET`，IR 后端自动跳过） |
| tests/pos/is_pattern_forms.nc | §6 `<pattern>` 六形态：`is -3`、`is +5`、`is -9..-5` 负区间、枚举变体、裸标识符按值比较、`is _`；并断言多个 `is-clause` 只执行首个匹配者 | ✅（2026-09-24 / PA-57 新增，c/native；与 `is_no_fallthrough.nc` 互补，未列入 `IR_SUBSET`） |
| tests/pos/void_slot_infer.nc | `void` 通用槽的所指类型由声明处静态确定后，裸 `.()` 读写、空下标 `p[]` 读写、`->` 成员读写、`malloc(T)`、`&arr` 取最深元素五类形态全部按该类型还原 | ✅（2026-09-24 / PA-55 + PA-57 新增，c/native；IR 前端只支持裸 `p.()`，未列入 `IR_SUBSET`） |
| tests/pos/module_top_forms.nc | §2 `<top-level>`：模块形态（无 `func main`，走 `-c`）下 `use sys.path`（点号多段名 + 目录层级找文件）与 `link "kernel32" Kernel32` 任意交错、`[[export ".mydata"]]` 带字面量参数、`[[inline]] func` | ✅（2026-09-24 / PA-57 新增，c/native 的 module 编译模式；无运行期输出，故不参与 `.expect` 比对） |
| tests/err/is_empty_range.nc | §6 `<pattern>` 的闭区间在编译期校验 `lo <= hi`，`is 5..1` 报 `empty 'is' range` | ✅（2026-09-24 / PA-57 新增，c/native；入 `IR_ERR_SKIP`，PA 的 IR 前端不做该校验） |
| tests/err/void_member_unknown.nc | `void` 槽的所指类型静态不可知（经另一个 `void` 变量转手）时写 `q->x` 即时报错并提示 `q.(T).x`，不再退化成非法 C | ✅（2026-09-24 / PA-55 + PA-57 新增，c/native；入 `IR_ERR_SKIP`） |
| tests/pos/ir_struct.expect | **本轮新增期望文件**（非新用例）：`ir_struct.nc` 此前无 `.expect`，其 struct 整体拷贝断言从不参与比对（实际一直打印 `struct copy bad`）；补齐后 8 行断言在 c/native/ir-c/ir-native 四后端一致，该用例由「跨后端一致性」段升格为按期望比对的 PASS（IR 线 PASS 由 6 变 7 的来源） | ✅（2026-09-24 / PA-55 + PA-57 新增，四后端一致） |
| tests/err/default_init_overflow.nc | 成员默认值的逐项展开超出缓冲上限（`DEFAULT_INIT_BUF 8192`）时前端即时报 `aggregate initializer is too large to expand its member defaults (8192 byte buffer)`，不产出截断的初始化器 | ✅（2026-09-24 / PA-56 新增，c/native；入 `IR_ERR_SKIP`，IR 前端无该机制） |
| tests/pos/array_member_default.nc | §3.1 × §5.1.2 聚合类型**数组变量**的成员默认值逐项展开：无初值全元素默认、位置初值时元素内省略成员与尾数元素各自补默认、逐元素指定式各层独立记账、`[...]` 默认容量档、多维按乘积展开、扁平初值不追加、`union` 元素数组只取首个默认成员 | ✅（2026-09-24 / PA-56 新增，c/native；未列入 `IR_SUBSET`，IR 后端自动跳过） |
| tests/err/linkas_not_implemented.nc | §2.1 `<linkas-decl>` 在 1.x 无实现：即时报 `linkas is not implemented in 1.x; static-library export naming comes with the 2.0 module system`，且整条声明（记号 + 库名字面量）一次消费，不再对字符串字面量补第二条通用错 | ✅（2026-09-25 / PA-52 新增，**四后端均执行**：IR 前端既有文案 `ir: unsupported top-level token 'linkas'` 亦含 `linkas`，故 `.expect` 取该子串，与 `safe_assign_removed` 同口径） |
| tests/err/cooking_bare_var.nc | §8 `<cooking-item>` 的未落地形态（裸 `<var-decl>`）即时报 `unsupported cooking item 'K'; 1.x provides only 'const NAME = expr', 'static_assert(...)' in a cooking block`，不再静默丢弃后延迟到 tcc 报 `'K' undeclared` | ✅（2026-09-25 / PA-59 新增，入 `IR_ERR_SKIP`：IR 前端仍逐 token 跳过、只在引用处报 `undeclared variable`） |
| tests/pos/cooking_ct_func.nc | §8 `<ct-func-def>` / `<ct-func-call>`：多形参（`add3` / `area`）、嵌套与组合（`sq(sq(2))` / `sq(area(2, 3))`）、带运算与一元的实参（`sq(1 + 2)` / `sq(-3)`）、实参取编译期变量并把结果再存为编译期变量供运行时声明引用 | ✅（2026-09-25 / PA-58 新增，**未列入 `IR_SUBSET`**：IR 侧同一用例的跨块嵌套调用 `sq(sq(2))` 会报 `ir: constant expression: unexpected token '*'`（其编译期函数体定位方式所致，见文末「2.0 线（PB）差异」表），故 IR 双后端按「IR 子集未覆盖」跳过） |
| tests/pos/ir_cook.nc | 原 `IR_ONLY`（全量 parser 不认 cooking 函数）→ PA-58 起 A 后端可编译运行，移入 `IR_SUBSET`，四后端执行且跨后端一致性段按期望比对（`consistent, 4 backends`） | ✅（2026-09-25 / PA-58 同轮改列，未改用例正文） |

> 上表为 1.0 发布集（`tests/pos` + `tests/err`，c/native 双后端）。`tests/pos/ir_*.nc` 属 2.0 IR 线（PB 分支），不在 1.0 发布门禁内。err 用例自 v1.0.2（PA-19）起在四个后端下均执行，`m2a`~`m2d` 四条 M2 静态检查用例与 `deref_bounds`（`.(T)` 类型化解引用）、`structof_bad_member`（从属查询内置函数）、`void_subscript`（通用 `void` 指针裸下标检查）、`void_member_unknown`（通用 `void` 槽推不出所指类型时的 `->` 成员访问拒绝，2026-09-24 / PA-57 新增）、`is_empty_range`（`is` 闭区间的 `lo <= hi` 编译期校验，2026-09-24 / PA-57 新增）、`len_unknown`（`len()` 静态不可知的报错口径）与 `slice_str_overflow`（切片赋值的字符串右值容量检查）、`str_array_overflow`（定长字符数组的字符串初值超容量）与 `str_array_bad_elem`（非 `char` 元素数组用字面量初始化）与 `flow_move_frozen`（冻结源转交所有权的检查）与 `multi_arr_overflow`（多变量声明的定长数组超容量）与 `multi_arr_nostr`（定长数组类型的多变量声明缺字符串初值）与 `default_init_overflow`（成员默认值展开的缓冲上限诊断，IR 前端整条默认值机制均无，2026-09-24 / PA-56 新增）与 `cooking_bare_var`（cooking 块内未落地 item 的即时诊断，IR 前端仍静默跳过且其编译期函数已实现，2026-09-25 / PA-59 新增）共**十八条**经 `xmake.lua` 的 `IR_ERR_SKIP` 在 IR 后端跳过（IR 前端无所有权/借用检查，只支持裸 `p.()`，未实现从属/位域内置函数与 `.(T)` 相关检查，也不做 `is` 区间校验）。`ownerof`（从属查询 + 位域偏移）、`deref_slice`（后缀链解引用 + 切片 + 数组退化）、`len_builtin`（`len()` 三类取值）、`alignof_builtin`（`alignof()` 编译期对齐）、`slice_str_infer`（字符串右值切片赋值 + 成员/调用类型推断）、`print_forms`（`print` 两种形态，IR 前端未内建 `print`）与 `is_no_fallthrough`（首个匹配即止，IR 前端未实现）、`str_array_init`（定长字符数组的字符串初值，IR 前端无容量检查）与 `flow_no_free`（`flow` 绑定非堆右值时豁免自动释放，IR 前端无所有权与释放路径）与 `flow_move`（`flow → flow` 所有权转移，IR 前端无所有权检查故语义无从校验）与 `multi_arr_decl`（多变量声明的定长数组存储与 `len()` 登记，IR 前端不支持该声明形态）与 `struct_member_default`（成员默认值与字面量转义，A 后端 `cgen` 专属路径），以及 2026-09-24 / PA-57 新增的 `stmt_terminators` / `literal_forms` / `type_forms` / `array_capacity_tiers` / `designator_init` / `for_init_forms` / `is_pattern_forms` / `void_slot_infer` / `module_top_forms` 九条，加上 PA-56 新增的 `array_member_default`（聚合类型数组的元素默认值逐项展开，A 后端专属路径），以及 PA-58 新增的 `cooking_ct_func`（cooking 编译期函数的多形参 / 嵌套 / 实参带运算形态，IR 前端受其体截取偏移缺陷所阻，见文末差异表），共十一条，未列入 `IR_SUBSET`，在 IR 后端按「IR 子集未覆盖」自动跳过（**PA-57 轮刻意不扩 IR 子集**：IR 线属 2.0 预览，扩子集需在 PB 分支验证四后端一致；PA-58 轮只把 `ir_cook` 由 `IR_ONLY` 改列入 `IR_SUBSET`——A 后端补齐 cooking 编译期函数后该用例四后端一致，属既有用例的改列，不是新增 IR 覆盖）。`is` 模式匹配由 `tests/pos/pattern.nc` 覆盖，该用例含 `is _` 通配符分支（v1.0.2 / PA-15 起，四后端输出一致）；`is <identifier>` 的变量绑定形态无用例——BNF v2.11 已把它定为**按值比较**（见上表），变量绑定与解构属 2.0 范围。门禁（2026-09-24 / PA-57 复跑 `xmake test --all`）：c / native 各 **51 PASS / 0 FAIL / 5 SKIP**，ir-c / ir-native 各 **7 PASS / 0 FAIL / 45 SKIP**；**同日 PA-56 轮再复跑为 c / native 各 53 PASS / 0 FAIL / 5 SKIP、ir-c / ir-native 各 7 PASS / 0 FAIL / 47 SKIP**（+2 = `tests/pos/array_member_default.nc` 不入 `IR_SUBSET`、`tests/err/default_init_overflow.nc` 入 `IR_ERR_SKIP`）；**2026-09-25 未实现形态专属诊断轮（PA-52 / PA-59）再复跑为 c / native 各 55 PASS / 0 FAIL / 5 SKIP、ir-c / ir-native 各 8 PASS / 0 FAIL / 48 SKIP**（+2 条 err 用例：`linkas_not_implemented` 四后端均 PASS、`cooking_bare_var` 在 IR 侧跳过），examples **6/7** 不变（`06_cooking.nc` 仍失败于 `<ct-func-def>` 未实现＝PA-58，但本轮其报错由 1 条误导文案变成逐行的「未实现」清单）；**同日 cooking 编译期函数实现轮（PA-58）再复跑为 c / native 各 56 PASS / 0 FAIL / 4 SKIP、ir-c / ir-native 各 8 PASS / 0 FAIL / 49 SKIP**（`ir_cook` 改列入 `IR_SUBSET` 后在 c / native 由 SKIP 转 PASS，故 c / native 各 +1 PASS / −1 SKIP；新增的 `cooking_ct_func` 只在 c / native 执行，故 IR 侧各 +1 SKIP），examples 由 **6/7** 变为 **7/7**（`06_cooking.nc` 自本轮起四后端均可编译运行，见 `TODO-PA.md` **PA-58**）。（上一基线是 PA-49 + PA-50 轮的 c/native 39P/0F/5S、IR 线 6P/0F/34S；本轮 +12 的来源是 11 条新用例 + `ir_struct` 补齐 `.expect` 后从「跨后端一致性」段升格为按期望比对的 PASS，IR 线的 45 SKIP = 原 34 + 11 条新用例。）

> **产物 C 的可移植性抽查（2026-09-24 / PA-50 起）**：`xmake test` 在 c 后端每个 pos 用例编译成功后，追加一次 `clang -fsyntax-only`（探测顺序 clang → gcc → cc，探不到可用编译器时整段跳过、不影响门禁），把"产物 C 只能被 tcc 吃下"这类缺陷变成 FAIL。**2026-09-24 / PA-57 起该抽查为全覆盖**：PA-50 落地时用于排除 `ir_builtin` / `ir_struct` 的 `STRICT_SKIP` 名单已随 F10 修复**整体删除**（原因见上表 F10 与 `TODO-PA.md` PA-55），当前 c 后端实际编译的 56 份产物 C（61 个 pos 用例减去 5 条 `IR_ONLY`）全部通过。注意本机 `gcc`（msys64）无 include 路径、通不过探针，`clang`（Swift 工具链）可用——抽查实际由 clang 执行。IR→C 后端（`ir_to_c.c`）与汇编后端的 `.string` 仍原样输出字面量字节，该路径的转义与抽查属 2.0（PB-33）。

---

## 2.0 线（PB）差异

以下为 PB 与上表（1.0 现态）不同的条目，行号基于 PB `8079ad0`：

| 条目 | 1.0 线（上表） | 2.0 线（PB）现态 |
|------|---------------|-----------------|
| §12.2 `flow`/`var`/`const` 参数前缀 | 前缀被忽略，统一 `VIS_DEFAULT`（parser.c:1018–991、1002） | **已实现**：`param->vis = pv` 记录前缀（parser.c:1001），调用点 `vis_check_call_arg()` 执行 M2 检查（parser.c:2220）；IR 前端由 `vvis` 状态机等价实现（irparse.c，PB-26） |
| 返回值可见性前缀（§7.3 接收规则，另见 §12.2「返回值」） | 无（见上方「调用方接收规则（§7.3）」表） | **已实现**：`ret_vis` 调用点检查（parser.c:1181）+ IR 侧 PB-29（含 PB-29.1 六条禁止路径 err 用例） |
| 检查范围表「IR 后端所有权检查」 | ⚠️ 未实现 | **已实现**（PB-26/M2 移植进 irparse.c，err 用例 m2a~m2d 在 ir-c/ir-native 下同样拒绝） |
| `is` 各模式行号 | parser.c:1366–1435 | parser.c:1239–1303（`_` 通配符 1237/`if (1)` 1240；错误信息 `expected block after 'is' pattern` 1294）；`is` 块由 irparse.c:2234 起的 `_` 分支处理 |
| `is _` 通配符 | v1.0.2 补全（PA-15） | PB-27.5 先于 1.0 线完成（parser.c + irparse.c 双前端） |
| 反向范围 `lo > hi` 校验 | **A 后端已实现**（2026-09-24 / PA-57，parser.c:1561–1580）；PA 侧 IR 前端仍不校验（该用例入 `IR_ERR_SKIP`） | **IR 前端先落地**（irparse.c:2259–2261 编译期报错）；**A 后端实现尚未从 1.0 线回灌**，PB `parser.c` 的 `is` 区间分支仍无 `lo <= hi` 比较，`tests/err/is_empty_range.nc` 可作回归用例 |
| `__is_val` 类型 | `int` 固定 | **类型感知**，等于 `while` 条件表达式类型（PB-27.7） |
| 结构体数组成员 | **A 后端可编译可运行**：`T struct { n char[8] a i32 }` 与省略长度的 `name char[]` 均接受（PA-23 探针） | **IR 前端拒绝**：任何数组类型成员都报 `ir: expected member name`（`char[8]` 与 `char[]` 同），标量成员正常。标量-only 结构体两前端一致 |
| `print` 内建（§2.3.1） | A 后端两种形态（v1.0.2 / PA-30 已在文档定稿口径）：首实参为字符串字面量 → 转发 C `printf(...)`（格式符须自配，不自动换行/拼接）；否则 → `printf("%lld\n", (long long)expr)` 按整数打印（`print(200)` 输出 `200`，`print(指针)` 输出地址整数值） | **IR 前端未内建 `print`**：作为未知函数直出，链接期报 `undefined symbol 'print'`（ir-native 为 `__imp_print`）；IR 侧仅 `puts` 可用，故 `IR_SUBSET` 用例一律用 `puts`。PB 侧文档尚无 §2.3.1 口径，`tests/pos/print_forms.nc` 可作 2.0 接入时的回归用例 |
| 循环体外使用 `is` | A 后端 `while_depth` 守卫即时报错（parser.c:1614–1574，v1.0.2 / PA-18）+ IR 前端 `is_val_vreg < 0` 报错 | 双前端均拒绝：IR 前端由 PB-27.1 先落地，**A 后端守卫尚未从 1.0 线回灌**（PB `parser.c` 的 `case TOK_IS` 仍为通用分支、无守卫） |
| 多个 `is-clause` 无 fallthrough | 1.0 线 **A 后端已实现**（v1.0.2 / PA-16：`while` 每轮迭代清零 `__is_matched`，子句守卫 `!__is_matched && <pat> && (__is_matched = 1)`）；PA 侧 IR 前端**仍未实现**（属 2.0 预览，按决策不在冻结线改动） | **两前端均未实现**：A 后端各子句生成并列 `if`，IR 侧每子句独立比较 + 跳转、块末无合并出口 jmp。可直接移植 1.0 的 `__is_matched` 标记方案（`tests/pos/is_no_fallthrough.nc` 可作回归用例） |
| `?=` 安全赋值记号 | 已从语法移除，A 后端显式拒绝（PA-20，BNF v2.3） | **仍接受**：PB `parser.c` 在声明（498）、语句窥探（1544）、赋值（2505）三处把 `?=` 与 `=` 同路处理，`token.h:112` 注释亦未更新；移除需回灌 PB（与 `while_depth` 守卫同批） |
| `?.` 安全解引用记号 | 已从语法移除，A/IR 双前端显式拒绝（PA-21，BNF v2.3） | **仍作为 `.(` 的别名接受**：PB `parser.c` / `irparse.c` 的 postfix 分支把 `TOK_SAFE_DOT` 与 `TOK_DOT_PAREN` 同路处理，`token.h` 注释亦未更新；移除需回灌 PB |
| `.()` / `.(T)` 越界检查 | A 后端编译期宽度检查（PA-21） | **未实现**：PB `irparse.c` 仍只支持裸 `p.()`、无 `.(T)`；PB `parser.c` 解引用链无宽度检查。检查规则回灌 PB 前，2.0 线文档不得声称已具备该检查 |
| 从属查询 / 位域偏移内置函数（§2.3，BNF v2.4） | A 后端已实现三参 `structof`/`unionof`/`holdof` 与 `bitoffsetof`（PA-22，见上表） | **未接入**：PB `parse_primary` 只把 `sizeof`/`typeof`/`alignof`/`offsetof`/`visof` 分派给 `parse_builtin_kw()`（PB parser.c:1953–1957），`structof`/`unionof`/`holdof`/`bitoffsetof` 落入默认分支；PB `irparse.c` 亦无对应实现。1.0 线的 PA-22 实现需回灌 PB 后 2.0 线才可声称支持 |
| 指针后缀链与切片（§5.1，BNF v2.5） | A 后端已实现 `.(T)` 通用后缀链、`p[]` 空下标、切片读/写、`T[n]=malloc(T[n])` 退化、多维声明（PA-25，见上表） | **未接入**：PB `parser.c` 解引用仍是独立的 `parse_deref_chain` 路径（无 `[]` 空下标、无切片读/写回、无数组退化），PB `irparse.c` 同样无对应实现。1.0 线的 PA-25 代码生成需回灌 PB 后 2.0 线才可声称支持 |
| `len(x)` 逻辑长度（§2.3，BNF v2.6） | A 后端（c/native）三类全实现并在静态不可知时报错（PA-27，见上表） | **部分**：PB `irparse.c` 由 `ve[]` 表实现数组/字符串/切片三类，但实参静态不可知时返回 0、不发前端错误；PB `parser.c`（A 方案）**完全没有 `len` 分支**，`len(x)` 会把 `len` 当普通函数输出到 C，错误延迟到 tcc 链接期。1.0 线的 PA-27 实现需回灌 PB |
| `alignof(T)` 类型对齐（§2.3） | A 后端编译期算出对齐值并输出字面量，不再依赖 C 的 `_Alignof`（PA-24，见上表） | **未回灌**：PB `parser.c` 两处仍生成 `_Alignof(T)`，在 tcc 下链接期报 `undefined symbol '_Alignof'`；PB `irparse.c` 按 IR 槽模型对所有 `alignof` 固定返回 8（`irparse.c:759` 注释），非真实类型对齐 |
| `else if` 递归形式（§6 `<if-stmt>`，BNF） | 双前端一致：A 后端 `parse_if_stmt`、IR 前端 `ir_if_stmt` 递归（PA-28，见上表） | **IR 侧仍缺**：PB `parser.c:1625` 的 A 后端分支已支持，但 PB `irparse.c:2014` 的 `else` 分支仍无条件调 `ir_block`，`else if` 在 ir-c / ir-native 报 `expected '{', got 'if'`；1.0 线的 PA-28 修复需回灌 PB |
| 切片赋值的字符串右值与推断声明取值（§5.1.4 / §5.1.5，BNF v2.7） | A 后端支持 `p[a..b] = "abc"` 按字节复制（含结尾 `\0`、超容量报错），且 `v = s.m` / `v = fp(a)` 的推断声明分别取成员类型（数组成员退化为指针）与被调返回类型（PA-26，见上表） | **未回灌**：PB `parser.c` 完全没有切片赋值（无 `rhs_was_slice` 分支，见上「指针后缀链与切片」行），`infer_init_type`（PB parser.c:576）的标识符分支仍是 `memcpy(out, s->type)` 配 `out->sym = s`——成员访问把变量符号当类型符号用、调用取函数类型而非返回类型；PB `irparse.c` 亦无对应路径。1.0 线的 PA-26 修复需回灌 PB，`tests/pos/slice_str_infer.nc` 与 `tests/err/slice_str_overflow.nc` 可作回归用例 |
| 定长字符数组的字符串初值（§5.1.2，BNF v2.8） | A 后端按声明容量分配数组存储，并拒绝超容量与非 `char` 元素（PA-31，见上表） | **未回灌**：PB 两前端（`parser.c` / `irparse.c`）对 `char[n] s = "..."` 均无容量检查、也不拒绝非 `char` 元素数组，声明容量被丢弃后退化为指针；1.0 线的 PA-31 实现需回灌 PB，`tests/pos/str_array_init.nc` 与 `tests/err/str_array_overflow.nc` / `str_array_bad_elem.nc` 可作回归用例（当前三例在 IR 后端经 `IR_ERR_SKIP` / 不入 `IR_SUBSET` 而跳过） |
| `flow` 自动释放的存储来源判定（§11.1） | A 后端登记 `Symbol.no_auto_free`：字符串字面量／`&x`／聚合初值三类非堆右值在块与函数退出时豁免 `free`，整变量重绑定按新右值重判（PA-32，见上表） | **未回灌**：PB 两前端对任何 `flow` 指针都无条件在退出点释放，`flow s char[] = "字面量"`、`flow p void = &x` 都会生成非法 `free`（运行期中止）；1.0 线的 PA-32 实现需回灌 PB，`tests/pos/flow_no_free.nc` 可作回归用例（当前该用例未入 `IR_SUBSET`，IR 后端自动跳过） |
| `flow → flow` 所有权转移的可书写性（§12.1 / §14.2） | A 后端登记 `ParserState.moved_src` 使转移语句自身右值放行、语句结束即失效，冻结源转交所有权即时报错，失效源豁免自动释放（PA-33，见上表） | **未回灌**：PB `vis.c` 无 `moved_src` 与冻结源守卫（`vis_check_assign` 仍在检查阶段直接 `vis_update_source`），`flow b void = a` 与 `b = a` 两种写法在 PB 线同样自报 `'a' is invalidated`；1.0 线的 PA-33 实现需回灌 PB，`tests/pos/flow_move.nc` 与 `tests/err/flow_move_frozen.nc` 可作回归用例 |
| 多变量声明的定长数组存储（§4.2 / §5.1.2） | 数组后缀保留为数组存储、登记 `len()` 容量、三条诊断（PA-34，见上表） | **未回灌**：PB `parser.c` 的多变量分支仍只输出 `c_type_name` + 变量名（不接 `c_type_suffix`），`var {aa = "aa"} char[2]` 在 PB 线仍静默生成 `char aa = "aa";` 且无诊断；1.0 线的 PA-34 实现可直接移植，`tests/pos/multi_arr_decl.nc` 与两条 err 用例作回归 |
| 聚合成员默认值（§3.1 `<field-decl>` 的 `[ "=" <expr> ]`，BNF v2.11） | 初值文本从聚合体内截回挂到 `Symbol.def_init`，声明点展开为 C 指定初始化器（PA-49，见上表 F1）；**PA-56 起**聚合类型**数组变量**亦按元素逐项展开（`default_init_text()`，parser.c:731 起，上限诊断 `default_init_too_big()` parser.c:610 起），`parse_init_list` 改为带本层类型的递归 | **未回灌**：PB `parser.c` 的 `parse_member_list` 与 PA 修复前逐字相同（初值直接落在类型名前缀位置），`P struct { x i32 = 7  y i32 }` 在 PB 线仍生成非法 C；`cgen.c` / `ncc.h` 两侧同源，`tests/pos/struct_member_default.nc` 可作回归用例 |
| 产物 C 的字符串字面量转义（可移植性） | 字面量经 `cgen_string_lit()` 重新编码，控制字节出八进制、UTF-8 高字节保留（PA-50，见上表 F2） | **未回灌**：PB 的 `cgen.c` / `parser.c` 两处 emit 点仍原样吐字节，且 PB 的 `xmake.lua` 无非 tcc 编译器 `-fsyntax-only` 抽查；回灌时须连同**已无 `STRICT_SKIP` 排除名单**的当前 `xmake.lua` 一并移植（PA 线该名单已于 2026-09-24 / PA-57 随 F10 修复整体删除，PB 若照旧带名单会被抽查判红）。IR→C（`ir_to_c.c`）与汇编后端的 `.string` 属 2.0 自身范围（PB-33） |
| `<array-size>` 四档容量口径（§3，BNF v2.11） | `[N]` / `[N..]` `[N...]` / `[..]` `[...]` / `[..N]` `[...N]` 七种写法全接受，省略容量取 `NH_DEFAULT_ARRAY_CAP = 8`（ncc.h:43，parser.c:417–446） | **PB 已是该模型的来源**：PB `parser.c:339–375` 自 2026-09-01 起就按四档解析并把 `[...]` 定为 8 槽，1.0 线的 PA-51 反而是向 PB 对齐；两侧唯一差异是 PB 把默认容量**硬编码为 `8`**，回灌 `NH_DEFAULT_ARRAY_CAP` 后改由常量单一来源。注意结构体**数组成员**在 IR 前端仍被拒绝（见上「结构体数组成员」行） |
| `void` 通用槽的所指类型还原（§5.1.1 / §5.1.5） | 新增 `Symbol.pointee_type`（ncc.h:144）在声明处静态登记所指类型，裸 `.()`／空下标 `p[]`／`->` 成员均按该类型还原，推不出时即时报错（PA-55，见上表 F10） | **未回灌**：PB `Symbol` 既无 1.0 线 PA-21 起的 `pointee_bytes` 字节记账（PB `ncc.h` 中两者都不存在），更没有 `pointee_type`，因此 `void` 槽在声明处静态确定所指类型这条链路在 PB 线整体缺失（`ir_builtin` 的产物 C 需靠排除名单放过，见 `TODO-PA.md` PA-55 的 PB 回灌段）。回灌需一并移植 `record_init_pointee`（parser.c:837–844）、`deref_prefix` 的 void 分支（parser.c:2756–2762）与 `->` 的 void 拒绝（parser.c:2917–2930）。`tests/pos/void_slot_infer.nc` 与 `tests/err/void_member_unknown.nc` 可作回归用例 |
| 括号类型声明 `(T)`（§3 `<type-name>`） | `parse_type` 有 `TOK_LPAREN` 分组分支（parser.c:395–404），`is_paren_type_ahead()`（parser.c:121–127）在形参（1139）、声明入口（1266）、for 头部（1714）三处识别 `v (i32) = 7` / `a (i32)[3]`（PA-57） | **未回灌**：PB `parse_type` 无 `TOK_LPAREN` 分支，声明/形参/for 头部三处也无该前瞻，括号类型形态在 PB 线整条链路不存在；`tests/pos/type_forms.nc` 可作回归用例 |
| 语句与顶层终止符 `;` / `#`（§1.4 `<empty-stmt>`） | `eat_stmt_terminator()`（parser.c:11–14）在 `parse_statement` 包装层统一吞掉行尾与独立成句的 `;` / `#`，模块头 `module main;`、`};`、`}#` 均可写（PA-57） | **未回灌**：PB 有 `TOK_SEMICOLON` / `TOK_POUND` 词法，但解析侧只有散点的 `if (cur_tok(cs) == TOK_SEMICOLON) next_tok(cs)` 容忍（parser.c:1354、1357、1414、1550、1557、1562 等），无统一包装层；`module main;`、`};`、`}#` 与独立成句的终止符形态未被系统性接受。`tests/pos/stmt_terminators.nc` 可作回归用例 |
| `<designator>` 指定式初值与成员默认值的共存（§5.1.3 × §3.1） | `parse_init_list(cs, base)`（parser.c:619 起）识别 `.` 前缀指定式并把名单记在**本层局部数组**（PA-56 起不再是 `ParserState` 字段，三字段已从 `ncc.h` 删除），展开时按名过滤默认值（`member_default_text`，parser.c:911–935）（PA-57 + PA-56） | **未回灌**：PB 的初始化列表无指定式分支，`{ .x = 1 }` 直接报语法错误；与上一行同属聚合初始化路径，回灌须与 PA-49 的 `def_init` 机制同批移植。`tests/pos/designator_init.nc` 可作回归用例 |
| `<for-init>` 的声明形态（§6） | for 头部支持 `i i32 = 0` 带类型声明 / `k = 2` 推断 / `m (i32) = 0` 括号类型三形态（parser.c:1709–1727）（PA-57） | **部分**：PB `parser.c` 的 for 分支已支持 `i = 0` 类型推断形态（并复用 `infer_init_type`），但 `i i32 = 0` 与 `m (i32) = 0` 两种带类型声明形态无对应分支；`tests/pos/for_init_forms.nc` 可作回归用例 |
| 二进制字面量取值（§1.3 `<int-literal>`） | 词法保留 `0b` 前缀文本，取值改为 `strtoll(buf + 2, NULL, 2)`（lexer.c:261–262）（PA-57） | **未回灌**：PB `lexer.c:261` 与 PA 修复前逐字相同（`strtoll(buf, NULL, 0)`）。base-0 在 C23 之前不认 `0b` 前缀，因此 `0b1011` 的取值随 CRT 而变（Microsoft/旧 CRT 得 `0`，glibc 扩展得 `11`），静默算错不报错；回灌为三行改动，`tests/pos/literal_forms.nc`（断言 `0b1011 == 11`）可作回归用例 |
| `<top-level>` 的 `use` / `link` 交错与点号多段模块名（§2） | use/link 合并为同一循环（parser.c:478–546），`use std.io` 由词法把点号拼进模块名、`module.c` 再按目录层级找 `std/io.nc`（module.c:81–99，同时保留点号原样路径兼容单段名）（PA-57） | **未回灌**：PB `parser.c:411`（`use` 段）与 `425`（`link` 段）是两个先后独立循环，`link` 之后再写 `use` 不被接受；`use` 只取单个标识符，点号后续 token 落到顶层循环报错。PB `module.c` 亦无点号→目录层级的候选路径拆分 |
| `linkas`（§2.1 `<linkas-decl>`） | **A 后端已给专属诊断**：`parse_linkas_decl()`（parser.c:449–458）报 `linkas is not implemented in 1.x; ...` 并整条消费（PA-52，2026-09-25）；**实现本身仍缺**，随 2.0 模块系统 | **两前端均无实现**：PB `parser.c` 落到通用 `unexpected token ... at declaration level`（PA 修前的同一形态）、`irparse.c` 报 `ir: unsupported top-level token 'linkas'`。回灌时须带上 PA 的 `parse_linkas_decl()`，否则 `tests/err/linkas_not_implemented.nc` 在 c 后端会因文案不同而判红（IR 侧不受影响，`.expect` 只取 `linkas`） |
| cooking 块内未落地 item 的诊断（§8 `<cooking-item>`） | **A 后端即时报错并按行跳过**：`unsupported cooking item ...`（PA-59，2026-09-25）、`cooking const: expected name`；`const NAME(...)` 形态自 PA-58 起**已是真实现**，不再报「未实现」；块内 `;` / `#` 终止符一并支持 | **IR 前端仍静默跳过**：`ir_cooking`（PB `irparse.c` 同名分支）对未识别 item 逐个 `next_tok`，错误延迟到引用处，故 1.x 的专属文案对 IR 无意义，`tests/err/cooking_bare_var.nc` 入 `IR_ERR_SKIP` |
| cooking 编译期函数（§8 `<ct-func-def>` / `<ct-func-call>`；PA-58 起 A 后端已实现） | 体源文本**按字符扫**（`ct_capture_body`，parser.c:2601–2618）：自 `=` 之后的缓冲区位置起，遇换行 / `;` / `#` / `}` 截止，token 流另按 `last_line_num` 推进到 item 末。实参先求成编译期常量再代入十进制字面量；形参 >4 报 `supports at most 4 params` 且不登记，实参个数不符报 `takes N arg(s), got M`；实参代入后展开式超 512 字节报 `expands beyond 512 bytes` 并放弃求值 | 实现更早（PB-9，`irparse.c:84–141` 表与展开、`1624–1662` 定义），但**体源文本按 token 流的起止末位定位**（`mark = lx->buf_ptr` → 跳到 `TOK_NEWLINE` / `TOK_RBRACE` 取 `end`），而括号内词法器不发 `TOK_NEWLINE`，实测覆盖不完备：`tests/pos/cooking_ct_func.nc` 在 ir-c / ir-native 报 `ir: constant expression: unexpected token '*'`（位于 `<cooking-fn>:1:3`，两次）与 `unknown identifier 'area'`。最小失败复现＝**定义写在一个 cooking 块、嵌套调用 `sq(sq(2))` 写在相邻的后续块**；同一块内的嵌套调用与同一块内的多形参定义（`const add3(a, b, c) = a + b + c`）两种形态实测均可过，`tests/pos/ir_cook.nc` 的写法也不受影响（该用例四后端一致）。IR 侧另无实参个数校验（`irparse.c:1473–1487` 只按 `pi < ac` 替换），形参配不上实参时退化成 `unknown identifier`。PA 未动 `irparse.c`（属 PB 文件），**回灌时须一并修**（随 PB-33），修后方可把 `cooking_ct_func` 列入 `IR_SUBSET` |
| 测试覆盖 | v1.0.2 门禁 38P/0F/5S（含 PA-18 ~ PA-22、PA-25 ~ PA-27、PA-31 十四条 err 用例与 PA-16/PA-30/PA-32/PA-33/PA-34 五条语义用例；IR 侧 6P/0F/33S）。**2026-09-24 语法标准对齐轮（PA-57，承接 PA-49 + PA-50 轮的 39P/0F/5S）后为 c/native 各 51P/0F/5S、ir-c/ir-native 各 7P/0F/45S**，+12 来自 11 条新用例与 `ir_struct` 补齐 `.expect` 后的升格；**同日 PA-56 轮再 +2 → c/native 各 53P/0F/5S、ir-c/ir-native 各 7P/0F/47S**、**2026-09-25 PA-52 / PA-59 轮再 +2 err → c/native 各 55P/0F/5S、ir-c/ir-native 各 8P/0F/48S**、**同日 PA-58 轮再 +1 pos（`cooking_ct_func`，不入 `IR_SUBSET`）并把 `ir_cook` 由 `IR_ONLY` 改列入 `IR_SUBSET` → c/native 各 56P/0F/4S、ir-c/ir-native 各 8P/0F/49S**（详见上「测试覆盖」表） | `xmake test --all` 全矩阵（c/native/ir-c/ir-native），见 `ROADMAP.md` 里程碑「验收」行；详值以 `CHANGELOG.md` 当前版本段为准。**回灌注意**：PA 线 `xmake.lua` 的 `STRICT_SKIP` 已整体删除、`IR_ERR_SKIP` 现为十八条（PA-57 新增 `void_member_unknown` / `is_empty_range`，PA-56 新增 `default_init_overflow`，PA-59 新增 `cooking_bare_var`），PB 移植须按无排除名单的当前版本 |
