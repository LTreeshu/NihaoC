# NihaoC 规范与编译器实现对应关系

本文档记录 NihaoC 语言规范（Chinese.md / English.md）中各项指针安全规则与编译器（ncc）实际实现的对应状态。

> 更新日期：2026-09-20。本表主体以 **1.0 冻结线（v1.0.2）** 的现态书写，行号基于该时点 `PA`/`main` 的 `parser.c` / `vis.c` / `irparse.c`，仅作导航用。
> **PB（2.0 线）差异集中在文末「2.0 线（PB）差异」一节**：`vis.c` 的矩阵实现两侧同源（行号一致），`parser.c` / `irparse.c` 行号与部分状态不同。

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
| `flow → flow`（同一指针的所有权转移，§12.1 允许） | 声明式 `flow neu void = ptr`（parser.c:1110–1122）与赋值式 `neu = ptr`（parser.c:2866–2888）两种写法均可书写：`vis_check_assign` 把源置 `BS_INVALID` 的同时登记 `ParserState.moved_src`，`vis_check_usable` 只对**本语句自身的**转移源放行，下一条语句进入 `parse_statement` 即清除（parser.c:1357–1359）——源在转移语句内可读、语句结束后失效。转移同时置源的 `ownership_transferred`，失效源不再参与 §11.1 的自动释放，释放责任移交接收方。源已被 `const`/`var` 借用（`BS_FROZEN`）时转交所有权即时报 `cannot transfer ownership of 'ptr': it is borrowed (frozen) by an active const/var`，与 §14.2 分析表一致 | parser.c:1119、1357–1359、2190、2875，vis.c:134、194–201、205–211 | ✅ 已实现（v1.0.2 / PA-33 修复检查顺序缺陷） |

> 本矩阵不适用于函数返回值接收：返回值没有可冻结/可解冻的源变量，规则由 §7.3 单独约束且更严格，见下文「调用方接收规则（§7.3）」。

---

## 函数参数传递（§12.2）

| 规范要求 | 编译器实现 | 对应代码 | 状态 |
|---------|----------|---------|------|
| `flow` 参数接收所有权 | 参数前缀被忽略 | parser.c:951–955、966 | ⚠️ 未实现 |
| `var` 参数冻结实参 | 参数前缀被忽略 | parser.c:951–955、966 | ⚠️ 未实现 |
| `const` 参数冻结实参 | 参数前缀被忽略 | parser.c:951–955、966 | ⚠️ 未实现 |

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
| 裸标识符赋值检查 | 完整实现 | parser.c:1109–1122（声明初始化）、2865–2878（表达式赋值） | ✅ 已实现 |
| 表达式级检查 | 仅检查裸标识符 | parser.c:1109–1122、2865–2878 | ⚠️ 部分实现 |
| IR 后端所有权检查 | 无 | irparse.c | ⚠️ 未实现（2.0 范围，PB-26/PB-29 已覆盖） |
| `?=` 安全赋值记号 | 已从语法移除（BNF v2.3）：A 后端 `case TOK_SAFE_ASSIGN` 显式拒绝并提示改用 `=`；IR 前端无该分支，词法 token 落入通用"unexpected token"报错 | lexer.c:613（词法保留）、parser.c:2891–2899（语法拒绝） | ✅ 已按规范移除（PA-20） |
| `?.` / `?(` 安全解引用记号 | 已从语法移除（BNF v2.3）：A 后端 postfix 循环遇 `TOK_SAFE_DOT` 显式拒绝并提示改用 `.()`；IR 前端同口径显式分支。`?(` 从未是 token（词法切成 `?` + `(`），无需处理 | token.h（`TOK_SAFE_DOT` 词法保留）、parser.c:2513–2519、irparse.c:1033–1039 | ✅ 已按规范移除（PA-21） |
| `.()` / `.(T)` 解引用安全检查 | 读侧 `vis_check_usable`；写穿侧 `vis_check_writable`（对 lhs 指针检查）；编译期宽度检查：`sizeof(T)` 超过该指针静态已知所指字节数即报错。所指字节数来自 `malloc(T)`/`malloc(T,n)` 初值或 `&x`（`sym->type->ref->size`），对指针本身重新赋值时清零（保守不检查）| parser.c:2188–2191（可见性）、2476–2488（越界检查）、2809 与 2913（写穿检查）、2811（重赋值撤销）、2297–2330 与 1104/1144（malloc 字节数记录）、ncc.h（`Symbol.pointee_bytes`）| ✅ 已实现（PA-21）。IR 前端仅支持裸 `p.()`，无 `.(T)` 与宽度检查 |
| 运行期空指针检查 | 无 | — | ⚠️ 未实现（非空由静态规则保证：指针声明必须初始化，§5.1；`malloc` 返回 `NULL` 的运行期检查属 2.0 范围）|

> `vis_check_assign()` 仅在赋值右侧为**单个裸标识符**时触发。`x = y + 1` 或 `x = malloc(...)` 等表达式形式的赋值不经过传递矩阵检查。IR 后端（irparse.c）仅记录可见性值用于 `visof()` 查询，不执行所有权/借用检查。

---

## 指针后缀链与切片（§5.1，BNF v2.5 / v2.7）

| 规范要求 | 编译器实现 | 对应代码 | 状态 |
|---------|----------|---------|------|
| `.(T)` 可处于后缀链任意位置，其后继续下标/成员/切片 | 通用 postfix 循环：每个算子把「已生成的前缀 C 文本」整体包裹后继续 | parser.c:2408–2422（前缀回取）、2434–2481（`deref_prefix` / `deref_step`）、2484 起（`parse_postfix`） | ✅ 已实现（v1.0.2 / PA-25，c/native）。IR 前端属 2.0 待做 |
| 数组类型化解引用 `p.(char[9])[i]` | 数组 cast 生成 `char (*)[9]`（而非 `char**`），下标按整数组步长推进 | parser.c:2425–2439（`c_cast_name`） | ✅ 已实现（PA-25） |
| 空下标 `p[]` = 省略类型的 `p.()`（一层解引用） | postfix `[` 紧跟 `]` 时走 `deref_prefix`；`p.().()` 多级链由此统一写作 `p[][].(i32)` | parser.c:2535–2538 | ✅ 已实现（PA-25） |
| 通用 `void` 指针裸下标/切片 | 前端报错并给出修复写法 `p.(T)[i]`（元素宽度静态未知，无法翻译成合法 C） | parser.c:2540–2546 | ✅ 已实现（PA-25，§5.1.2） |
| 切片读 `p[a..b]` | 生成「指向第 a 号元素的指针」`&p[a]`，长度不进入值本身（C 无数组切片类型）；上下界同为字面量时另记录 `hi-lo` 供 `len(切片变量)` 取用（PA-27）；`[..b]` 省略起点等价 `[0..b]`，长度记为 `b` | parser.c:2547–2590 | ✅ 已实现（PA-25 / PA-27） |
| 切片读赋给固定数组 `T[n] x = p[a..b]` | 按**声明容量**逐元素复制（`memcpy`），非别名 | parser.c:1139–1145、1193–1200 | ✅ 已实现（PA-25） |
| 定长字符数组的字符串初值 `s char[n] = "abc"` | 按声明容量分配**数组存储**（不退化为指针），字面量含结尾 `\0` 整段写入；`strlen+1 > n` 时前端即时报 `string needs N bytes with terminator, array 's' holds M`；元素类型非 `char` 的定长数组用字面量初始化即时报 `cannot initialize array 's' with a string literal; ...`；`len(s)` 取声明容量 | parser.c:1123–1138（初值形态判定与两条诊断）、1167–1171（数组形态声明） | ✅ 已实现（v1.0.2 / PA-31，c/native）。IR 前端两项诊断均无，属 2.0 待对齐 |
| 切片写 `p[a..b] = {v0, v1, …}` 与 `p[a..b] = "abc"` | 值列表逐元素写回（逗号表达式，一条 C 语句）；字符串右值按**字节**整段 `memcpy`（含结尾 `\0`），闭区间右界 `b` 即最后一个可写字节，`strlen > b-a` 时前端报错；两者之外的右值形态报错 | parser.c:2813–2862（2804–2821 为字符串右值分支） | ✅ 已实现（PA-25；§5.1.4 的字符串右值形式与容量诊断由 PA-26 补齐） |
| `T[n] x = malloc(T[n])` 声明退化为指针 | 生成 `T (*x)…`（仅省略最前维度：`void[4][5]` → `void* (*x)[5]`），`malloc` 字节数按元素个数乘算 | parser.c:542–556（`c_decay_suffix`）、1161–1166、2298–2309 | ✅ 已实现（PA-25） |
| 切片变量 `s = a[lo..hi]`（类型推断声明） | 推断自源数组，声明后退化为**元素指针** `E *s = &a[lo]`（视图，不复制），`s.()` 取首元素 | parser.c:1164–1169（指针形态声明）、2444–2448（数组槽裸 `.()` 按最深元素解引用） | ✅ 已实现（PA-27） |
| 推断声明 `v = s.m` / `v = f(a)` 的取值 | 右值为成员访问时取**成员**类型（数组成员按退化处理成 `T*`）、为调用时取被调（或函数指针）的返回类型；聚合体变量本身作右值时保留标签符号，避免把变量名当类型名输出 | parser.c:592–645（`infer_init_type` 的标识符分支） | ✅ 已实现（v1.0.2 / PA-26，c/native）。IR 前端无成员类型推断路径，属 2.0 待回灌 |
| 多维数组声明 `T[a][b]` | C 后缀按源码顺序输出全部维度（`int32_t m[2][3]`） | cgen.c（`c_type_suffix`） | ✅ 已实现（PA-25） |
| 多变量声明带数组类型 `var {aa = "aa", …} char[2]` | 声明的数组后缀整体丢弃，每个变量生成为**无维数**的 `char aa = "aa";`（字面量赋给标量，C 侧仅告警），容量既不登记进 `len()` 也不做长度检查，与单变量的「定长字符数组字符串初值」口径不一致 | parser.c:779–845（多变量分支，827 处只输出 `c_type_name` + 变量名，不接 `c_type_suffix`） | ⚠️ 缺陷（v1.0.2 / PA-31 复核发现）：中英 §4.2「多变量声明」示例给出该写法，实现不支持且无诊断；登记 `TODO-PA.md` PA-34 待决策 |

> A 后端不把切片长度编码进值本身：`p[a..b]` 生成的是「指向第 a 号元素的指针」，`b` 只在与 `lo` 同为字面量时被记录下来供 `len(切片变量)` 求 `hi-lo`（parser.c:2552–2562、2570–2581），随后即丢弃；因此接收方为固定数组时按**声明容量**复制、写回时按右值元素个数决定，`b` 越界仍属运行期未定义行为（与 §5.1.2 一致）。

---

## 内置查询与输出内建（§2.3 / §2.3.1，BNF v2.4 / v2.6）

A 后端把 `sizeof` / `typeof` / `alignof` / `offsetof` / `bitoffsetof` / `visof` / `structof` / `unionof` / `holdof` 作为**关键字 token**处理，统一由 `parse_builtin_kw()` 分派（`parse_primary` 的 `switch` 进入，parser.c:2149）；标识符路径的内置函数表看不到这些关键字。`len` 与之相反，走 `parse_primary` 的标识符内置函数表（parser.c:2204–2225）。

| 规范要求 | 编译器实现 | 对应代码 | 状态 |
|---------|----------|---------|------|
| `len(x)` 逻辑长度 | 编译期常量：数组=各维元素个数乘积；动态字符串 `char[]` / 推断字符串=字面量长度；切片变量=`hi-lo`（两侧须为字面量）。长度在声明处登记到符号表，整变量重新赋值后撤销记录（保守报错而非返回过期值）；静态不可知或非标识符实参即时报错 | parser.c:1183–1199（登记）、2195–2216（求值与报错）、2794–2797（重赋值撤销）、ncc.h（`Symbol.len_known` / `logical_len`） | ✅ 已实现（v1.0.2 / PA-27，c/native）。IR 前端 `len()` 走 `ve[]` 表，静态不可知时返回 0 而非报错（`tests/err/len_unknown.nc` 经 `IR_ERR_SKIP` 跳过），2.0 待对齐 |
| `structof(T, m, p)` / `unionof(T, m, p)` / `holdof(T, m, p)` 从属查询 | 生成 `((void*)((char*)(p) - offsetof(T, m)))`，即反推成员所属聚合体首地址；`T` 必须是聚合体，`m` 必须是其成员，`structof` 仅接受 `struct`、`unionof` 仅接受 `union`、`holdof` 两者通用 | parser.c:1715–1721（成员查找）、1785–1822（三参分派与诊断） | ✅ 已实现（v1.0.2 / PA-22，c/native）。IR 前端属 2.0 布局待做，`ir-*` 经 `IR_SUBSET`/`IR_ERR_SKIP` 跳过 |
| `bitoffsetof(T, m)` 位域位偏移 | 按声明顺序的位布局：以前置非位域成员为锚点，用 C 自身 `offsetof`/`sizeof` 求存储单元起点，再累加整单元与单元内位；对齐以基类型 `sizeof` 近似（基本整型 `align == size`）；目标非位域、基类型宽度未知、同一组成员存储类型不一致均报错 | parser.c:1833–1911 | ✅ 已实现（v1.0.2 / PA-22，c/native）。IR 前端未实现 |
| `alignof(T)` 类型对齐 | 编译期由编译器算出对齐值并把**字面量**输出到 C（不再依赖 C 的 `_Alignof`）：数组递归取元素对齐、结构体/联合体取最宽非位域成员对齐、`void` 按通用指针计 8、其余类型取声明对齐（`parse_type` 的栈上 `CType.align` 为 0 时回落到 `type_default_align(kind)`）| parser.c:1749–1756 与 2225–2231（两处分派）、type.c:71–102（`type_align`）、ncc.h:400（原型）| ✅ 已实现（v1.0.2 / PA-24，c/native）。IR 前端仍按 IR 槽模型对任何 `alignof` 固定返回 8（`irparse.c:759`），2.0 布局待对齐 |
| 聚合体成员列表分隔符 | 成员以**空白**分隔（`Point struct { x i32 y i32 }`），逗号被拒绝（`expected member name, got ','`） | parser.c:103（成员解析） | ✅ 与 BNF 一致（v1.0.2 / PA-23）：文档 4 处示例原用逗号分隔，已改回空白分隔，语法与实现均未改动 |
| `print` 两种形态（§2.3.1） | 实参首 token 为字符串字面量 → **C `printf` 直通**（格式符与参数须自行匹配，不自动换行，多余参数不拼接）；否则 → `printf("%lld\n", (long long)expr)`，即按十进制整数打印该值并换行（对指针实参打印其地址整数值） | parser.c:2336–2355 | ✅ 已实现且文档已定义（v1.0.2 / PA-30 补 §2.3.1 口径）。IR 前端**未内建** `print`（直出成未定义符号，链接期报错），2.0 待做 |
| `puts` 输出字符串（§2.3.1） | C `puts` 直通：实参须为字符串指针，自动补换行；把整型（`p.(i32)`）传给 `puts` 会按地址解引用，运行期崩溃或乱码 | parser.c:2356–2370（`puts`/`printf` 普通直通调用） | ✅ 双前端一致（v1.0.2 / PA-30 补文档口径；文档 5 处 `puts(整型)` 示例已改为 `print(整型)`） |

---

## 条件与分支（§6，BNF `<if-stmt>`）

| 规范要求 | 编译器实现 | 对应代码 | 状态 |
|---------|----------|---------|------|
| `if <expr> <block> [else (if-stmt \| block)]`——`else if` 递归形式 | A 后端 `parse_if_stmt` 用 `while (cur_tok == TOK_ELSE)` 循环消化整条 `else if` 链；IR 前端 `ir_if_stmt` 在 `else` 后 peek 到 `if` 时递归自身，多级分支共用同一条 `JZ`/`JMP` 标签链 | parser.c:1680–1702、irparse.c:1771–1793 | ✅ 双前端一致（v1.0.2 / PA-28 补 IR 侧；此前 `else if` 在 ir-c / ir-native 报 `expected '{', got 'if'`） |
| `else` 分支必须是块 | IR 前端 `ir_block` 严格要求 `{`；A 后端语法上对 `else` 后接非块语句不报错，但生成的 C 缺少分隔符（`elseputs(...)`）而延迟到 C 编译期失败 | parser.c:1696–1700、irparse.c:1759–1769（`ir_block`） | ✅ 有效语义与 BNF 一致（只有块形式可用）；A 后端该分支的宽松属代码生成缺陷，无文档承诺该写法，未纳入本版 |

---

## `is` 模式匹配（§6.1，BNF v2.2）

| 规范要求 | 编译器实现 | 对应代码 | 状态 |
|---------|----------|---------|------|
| `is` 仅配合 `while`（块形式） | A 后端 `while_depth` 计数守卫：`case TOK_IS` 在循环体外直接报错；IR 前端 `is_val_vreg < 0` 守卫 | parser.c:1406–1408、1522–1529、irparse.c:2021–2024 | ✅ 已实现（v1.0.2 补 A 后端守卫，PA-18） |
| `do` 不支持 `is` | A 后端进入 `do` 体前把 `while_depth` 清零、退出恢复；IR 前端 `do` 分支置 `is_val_vreg = -1`。嵌套在 `while` 内的 `do` 同样拒绝，不会静默匹配外层条件值 | parser.c:1421–1425、1522–1529、irparse.c:1896、1909 | ✅ 已实现（双前端显式拒绝，v1.0.2 补 IR 侧，PA-19） |
| `is <int-literal>` / `is -<int>` | `== v` / `== -v` | parser.c:1305–1323 | ✅ 已实现 |
| `is lo..hi` 闭区间 | `>= lo && __is_val <= hi` | parser.c:1314–1320 | ✅ 已实现 |
| `is <visibility-enum>` | 比较 `NH_*` 常量 | parser.c:1333–1340 | ✅ 已实现 |
| `is <enum-variant>` / 已知常量 | `TOK_IDENTIFIER` 分支按值比较 `== pat` | parser.c:1324–1332 | ✅ 已实现 |
| `is _` 通配符恒匹配 | 恒真分支（A 后端 `if (!__is_matched && (1) && …)`，IR 侧不发比较与 JZ） | parser.c:1293–1302、irparse.c:2029–2032 | ✅ 已实现（v1.0.2 补全） |
| `is <identifier>` 变量绑定 | 按**值**比较，非绑定 | parser.c:1324–1332 | ⚠️ 语义差异（2.0 范围，PB-27 类型感知后统一） |
| 多个 `is-clause` 首个匹配即止（无 fallthrough） | A 后端：`while` 声明 `int __is_matched`（parser.c:1395）并在每轮迭代清零（parser.c:1405），每个子句的守卫为 `if (!__is_matched && <pat> && (__is_matched = 1))`（parser.c:1296、1304），故同一条 `while` 体内至多执行一个 `is-clause` 块，前子句不匹配时后续子句照常执行。IR 前端：各 is-clause 仍是**并列**独立 `if`（块末无跳到合并出口的 `jmp`），条件重叠时连续执行 | parser.c:1296、1304、1395、1405、irparse.c:2140–2144 | ✅ A 后端已实现（v1.0.2 / PA-16，`tests/pos/is_no_fallthrough.nc` 覆盖）；⚠️ IR 前端未实现（1.0 线的 2.0 预览与 PB 线同样待对齐） |
| `is <pat> => <stmt>` 单语句 | 已移除（BNF v2.2 / PA-13），双前端仅接受块形式 | parser.c:1347–1351、irparse.c:2140–2144 | ✅ 已按规范移除 |
| 反向范围 `lo > hi` 编译期校验 | 无 | parser.c:1314–1320 | ⚠️ 未实现（2.0 范围，PB-27.4 已覆盖） |
| 结构体解构 / ADT 变体解构 | 预留语法，未实现 | — | ⚠️ 预留（依赖类型系统，PB-27.10/11） |

---

## 存储期属性（§11.1）

| 规范要求 | 编译器实现 | 对应代码 | 状态 |
|---------|----------|---------|------|
| `const` 模块级静态 / 块级自动 | C 后端自然实现 | parser.c:827、1055 | ✅ 隐式实现 |
| `flow` 动态分配 + 自动释放 | 块/函数退出时对**堆所有权**的 `flow` 自动 free；是否属于堆所有权由存储来源登记决定（`Symbol.no_auto_free`，零初始化默认保持释放），`ownership_transferred`（返回值转移与 `flow → flow` 转移后的失效源）与 `no_auto_free`（非堆来源）都会豁免 | parser.c:1095–1103（声明期登记）、2879–2887（整变量重绑定重登记）、1249–1256（函数退出）、1608–1620（块退出） | ✅ 已实现（v1.0.2 / PA-32 收紧为「只发给堆右值」）。IR 前端不做所有权与自动释放（2.0） |
| `flow` 返回值所有权转移 | `ownership_transferred` 标志 | parser.c:1503–1513 | ✅ 已实现 |
| `static` 静态存储 | C `static` 关键字 | cgen | ✅ 已实现 |
| `flow` 绑定非堆右值（字符串字面量 / `&x` / 聚合初值） | 声明期登记存储来源并豁免自动释放：`flow s char[] = "abc"` 生成 `char* s = "abc";` 且退出点不再发 `free(s)`；`flow p void = &n` 同样不释放（原实现两处都会对静态只读段／栈地址执行 `free`，运行期中止且无任何输出） | parser.c:1095–1103（来源登记）、1249–1256 与 1608–1620（两处豁免）、ncc.h（`Symbol.no_auto_free`） | ✅ 已修复（v1.0.2 / PA-32，c/native；`tests/pos/flow_no_free.nc` 固化）。中英 §11.1 已定案四类右值口径。IR 前端无自动释放路径，2.0 待回灌 |
| `flow` 堆所有权被重绑定后的旧堆块 | 整变量重绑定按新右值重判来源，不在重绑定点插入释放：`flow q void = malloc(i32)` 后 `q = &n` → 旧块**泄漏**（保守选择，避免经别名双释放） | parser.c:2879–2887 | ⚠️ 已知限制（v1.0.2 / PA-32 决策②刻意保留）：精确回收属 2.0 所有权转移分析，中英 §11.1 已写明 |

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
| tests/pos/transfer.nc | `flow` 返回值所有权转移 | ✅ |

> 上表为 1.0 发布集（`tests/pos` + `tests/err`，c/native 双后端）。`tests/pos/ir_*.nc` 属 2.0 IR 线（PB 分支），不在 1.0 发布门禁内。err 用例自 v1.0.2（PA-19）起在四个后端下均执行，`m2a`~`m2d` 四条 M2 静态检查用例与 `deref_bounds`（`.(T)` 类型化解引用）、`structof_bad_member`（从属查询内置函数）、`void_subscript`（通用 `void` 指针裸下标检查）、`len_unknown`（`len()` 静态不可知的报错口径）与 `slice_str_overflow`（切片赋值的字符串右值容量检查）、`str_array_overflow`（定长字符数组的字符串初值超容量）与 `str_array_bad_elem`（非 `char` 元素数组用字面量初始化）与 `flow_move_frozen`（冻结源转交所有权的检查）共十二条经 `xmake.lua` 的 `IR_ERR_SKIP` 在 IR 后端跳过（IR 前端无所有权/借用检查，只支持裸 `p.()`，且未实现从属/位域内置函数与 `.(T)` 相关检查）。`ownerof`（从属查询 + 位域偏移）、`deref_slice`（后缀链解引用 + 切片 + 数组退化）、`len_builtin`（`len()` 三类取值）、`alignof_builtin`（`alignof()` 编译期对齐）、`slice_str_infer`（字符串右值切片赋值 + 成员/调用类型推断）、`print_forms`（`print` 两种形态，IR 前端未内建 `print`）与 `is_no_fallthrough`（首个匹配即止，IR 前端未实现）、`str_array_init`（定长字符数组的字符串初值，IR 前端无容量检查）与 `flow_no_free`（`flow` 绑定非堆右值时豁免自动释放，IR 前端无所有权与释放路径）与 `flow_move`（`flow → flow` 所有权转移，IR 前端无所有权检查故语义无从校验）未列入 `IR_SUBSET`，在 IR 后端按「IR 子集未覆盖」自动跳过。`is` 模式匹配由 `tests/pos/pattern.nc` 覆盖，该用例含 `is _` 通配符分支（v1.0.2 / PA-15 起，四后端输出一致）；`is <identifier>` 变量绑定仍无用例（对应上表 ⚠️ 项）。门禁（v1.0.2 / PA-33 后）：`xmake test --all` → c / native 各 **35 PASS / 0 FAIL / 5 SKIP**，ir-c / ir-native 各 **6 PASS / 0 FAIL / 30 SKIP**。

---

## 2.0 线（PB）差异

以下为 PB 与上表（1.0 现态）不同的条目，行号基于 PB `8079ad0`：

| 条目 | 1.0 线（上表） | 2.0 线（PB）现态 |
|------|---------------|-----------------|
| §12.2 `flow`/`var`/`const` 参数前缀 | 前缀被忽略，统一 `VIS_DEFAULT`（parser.c:951–955、966） | **已实现**：`param->vis = pv` 记录前缀（parser.c:934），调用点 `vis_check_call_arg()` 执行 M2 检查（parser.c:2137）；IR 前端由 `vvis` 状态机等价实现（irparse.c，PB-26） |
| 返回值可见性前缀（§7.3 接收规则，另见 §12.2「返回值」） | 无（见上方「调用方接收规则（§7.3）」表） | **已实现**：`ret_vis` 调用点检查（parser.c:1114）+ IR 侧 PB-29（含 PB-29.1 六条禁止路径 err 用例） |
| 检查范围表「IR 后端所有权检查」 | ⚠️ 未实现 | **已实现**（PB-26/M2 移植进 irparse.c，err 用例 m2a~m2d 在 ir-c/ir-native 下同样拒绝） |
| `is` 各模式行号 | parser.c:1283–1352 | parser.c:1203–1267（`_` 通配符 1201/`if (1)` 1204；错误信息 `expected block after 'is' pattern` 1258）；`is` 块由 irparse.c:2234 起的 `_` 分支处理 |
| `is _` 通配符 | v1.0.2 补全（PA-15） | PB-27.5 先于 1.0 线完成（parser.c + irparse.c 双前端） |
| 反向范围 `lo > hi` 校验 | ⚠️ 未实现 | **IR 前端已实现**（irparse.c:2259–2261 编译期报错）；A 后端 `parser.c` 仍不校验 |
| `__is_val` 类型 | `int` 固定 | **类型感知**，等于 `while` 条件表达式类型（PB-27.7） |
| 结构体数组成员 | **A 后端可编译可运行**：`T struct { n char[8] a i32 }` 与省略长度的 `name char[]` 均接受（PA-23 探针） | **IR 前端拒绝**：任何数组类型成员都报 `ir: expected member name`（`char[8]` 与 `char[]` 同），标量成员正常。标量-only 结构体两前端一致 |
| `print` 内建（§2.3.1） | A 后端两种形态（v1.0.2 / PA-30 已在文档定稿口径）：首实参为字符串字面量 → 转发 C `printf(...)`（格式符须自配，不自动换行/拼接）；否则 → `printf("%lld\n", (long long)expr)` 按整数打印（`print(200)` 输出 `200`，`print(指针)` 输出地址整数值） | **IR 前端未内建 `print`**：作为未知函数直出，链接期报 `undefined symbol 'print'`（ir-native 为 `__imp_print`）；IR 侧仅 `puts` 可用，故 `IR_SUBSET` 用例一律用 `puts`。PB 侧文档尚无 §2.3.1 口径，`tests/pos/print_forms.nc` 可作 2.0 接入时的回归用例 |
| 循环体外使用 `is` | A 后端 `while_depth` 守卫即时报错（parser.c:1531–1538，v1.0.2 / PA-18）+ IR 前端 `is_val_vreg < 0` 报错 | 双前端均拒绝：IR 前端由 PB-27.1 先落地，**A 后端守卫尚未从 1.0 线回灌**（PB `parser.c` 的 `case TOK_IS` 仍为通用分支、无守卫） |
| 多个 `is-clause` 无 fallthrough | 1.0 线 **A 后端已实现**（v1.0.2 / PA-16：`while` 每轮迭代清零 `__is_matched`，子句守卫 `!__is_matched && <pat> && (__is_matched = 1)`）；PA 侧 IR 前端**仍未实现**（属 2.0 预览，按决策不在冻结线改动） | **两前端均未实现**：A 后端各子句生成并列 `if`，IR 侧每子句独立比较 + 跳转、块末无合并出口 jmp。可直接移植 1.0 的 `__is_matched` 标记方案（`tests/pos/is_no_fallthrough.nc` 可作回归用例） |
| `?=` 安全赋值记号 | 已从语法移除，A 后端显式拒绝（PA-20，BNF v2.3） | **仍接受**：PB `parser.c` 在声明（498）、语句窥探（1544）、赋值（2505）三处把 `?=` 与 `=` 同路处理，`token.h:112` 注释亦未更新；移除需回灌 PB（与 `while_depth` 守卫同批） |
| `?.` 安全解引用记号 | 已从语法移除，A/IR 双前端显式拒绝（PA-21，BNF v2.3） | **仍作为 `.(` 的别名接受**：PB `parser.c` / `irparse.c` 的 postfix 分支把 `TOK_SAFE_DOT` 与 `TOK_DOT_PAREN` 同路处理，`token.h` 注释亦未更新；移除需回灌 PB |
| `.()` / `.(T)` 越界检查 | A 后端编译期宽度检查（PA-21） | **未实现**：PB `irparse.c` 仍只支持裸 `p.()`、无 `.(T)`；PB `parser.c` 解引用链无宽度检查。检查规则回灌 PB 前，2.0 线文档不得声称已具备该检查 |
| 从属查询 / 位域偏移内置函数（§2.3，BNF v2.4） | A 后端已实现三参 `structof`/`unionof`/`holdof` 与 `bitoffsetof`（PA-22，见上表） | **未接入**：PB `parse_primary` 只把 `sizeof`/`typeof`/`alignof`/`offsetof`/`visof` 分派给 `parse_builtin_kw()`（PB parser.c:1917–1921），`structof`/`unionof`/`holdof`/`bitoffsetof` 落入默认分支；PB `irparse.c` 亦无对应实现。1.0 线的 PA-22 实现需回灌 PB 后 2.0 线才可声称支持 |
| 指针后缀链与切片（§5.1，BNF v2.5） | A 后端已实现 `.(T)` 通用后缀链、`p[]` 空下标、切片读/写、`T[n]=malloc(T[n])` 退化、多维声明（PA-25，见上表） | **未接入**：PB `parser.c` 解引用仍是独立的 `parse_deref_chain` 路径（无 `[]` 空下标、无切片读/写回、无数组退化），PB `irparse.c` 同样无对应实现。1.0 线的 PA-25 代码生成需回灌 PB 后 2.0 线才可声称支持 |
| `len(x)` 逻辑长度（§2.3，BNF v2.6） | A 后端（c/native）三类全实现并在静态不可知时报错（PA-27，见上表） | **部分**：PB `irparse.c` 由 `ve[]` 表实现数组/字符串/切片三类，但实参静态不可知时返回 0、不发前端错误；PB `parser.c`（A 方案）**完全没有 `len` 分支**，`len(x)` 会把 `len` 当普通函数输出到 C，错误延迟到 tcc 链接期。1.0 线的 PA-27 实现需回灌 PB |
| `alignof(T)` 类型对齐（§2.3） | A 后端编译期算出对齐值并输出字面量，不再依赖 C 的 `_Alignof`（PA-24，见上表） | **未回灌**：PB `parser.c` 两处仍生成 `_Alignof(T)`，在 tcc 下链接期报 `undefined symbol '_Alignof'`；PB `irparse.c` 按 IR 槽模型对所有 `alignof` 固定返回 8（`irparse.c:759` 注释），非真实类型对齐 |
| `else if` 递归形式（§6 `<if-stmt>`，BNF） | 双前端一致：A 后端 `parse_if_stmt`、IR 前端 `ir_if_stmt` 递归（PA-28，见上表） | **IR 侧仍缺**：PB `parser.c:1589` 的 A 后端分支已支持，但 PB `irparse.c:2014` 的 `else` 分支仍无条件调 `ir_block`，`else if` 在 ir-c / ir-native 报 `expected '{', got 'if'`；1.0 线的 PA-28 修复需回灌 PB |
| 切片赋值的字符串右值与推断声明取值（§5.1.4 / §5.1.5，BNF v2.7） | A 后端支持 `p[a..b] = "abc"` 按字节复制（含结尾 `\0`、超容量报错），且 `v = s.m` / `v = fp(a)` 的推断声明分别取成员类型（数组成员退化为指针）与被调返回类型（PA-26，见上表） | **未回灌**：PB `parser.c` 完全没有切片赋值（无 `rhs_was_slice` 分支，见上「指针后缀链与切片」行），`infer_init_type`（PB parser.c:576）的标识符分支仍是 `memcpy(out, s->type)` 配 `out->sym = s`——成员访问把变量符号当类型符号用、调用取函数类型而非返回类型；PB `irparse.c` 亦无对应路径。1.0 线的 PA-26 修复需回灌 PB，`tests/pos/slice_str_infer.nc` 与 `tests/err/slice_str_overflow.nc` 可作回归用例 |
| 定长字符数组的字符串初值（§5.1.2，BNF v2.8） | A 后端按声明容量分配数组存储，并拒绝超容量与非 `char` 元素（PA-31，见上表） | **未回灌**：PB 两前端（`parser.c` / `irparse.c`）对 `char[n] s = "..."` 均无容量检查、也不拒绝非 `char` 元素数组，声明容量被丢弃后退化为指针；1.0 线的 PA-31 实现需回灌 PB，`tests/pos/str_array_init.nc` 与 `tests/err/str_array_overflow.nc` / `str_array_bad_elem.nc` 可作回归用例（当前三例在 IR 后端经 `IR_ERR_SKIP` / 不入 `IR_SUBSET` 而跳过） |
| `flow` 自动释放的存储来源判定（§11.1） | A 后端登记 `Symbol.no_auto_free`：字符串字面量／`&x`／聚合初值三类非堆右值在块与函数退出时豁免 `free`，整变量重绑定按新右值重判（PA-32，见上表） | **未回灌**：PB 两前端对任何 `flow` 指针都无条件在退出点释放，`flow s char[] = "字面量"`、`flow p void = &x` 都会生成非法 `free`（运行期中止）；1.0 线的 PA-32 实现需回灌 PB，`tests/pos/flow_no_free.nc` 可作回归用例（当前该用例未入 `IR_SUBSET`，IR 后端自动跳过） |
| `flow → flow` 所有权转移的可书写性（§12.1 / §14.2） | A 后端登记 `ParserState.moved_src` 使转移语句自身右值放行、语句结束即失效，冻结源转交所有权即时报错，失效源豁免自动释放（PA-33，见上表） | **未回灌**：PB `vis.c` 无 `moved_src` 与冻结源守卫（`vis_check_assign` 仍在检查阶段直接 `vis_update_source`），`flow b void = a` 与 `b = a` 两种写法在 PB 线同样自报 `'a' is invalidated`；1.0 线的 PA-33 实现需回灌 PB，`tests/pos/flow_move.nc` 与 `tests/err/flow_move_frozen.nc` 可作回归用例 |
| 测试覆盖 | 1.0 门禁 35P/0F/5S（含 PA-18 ~ PA-22、PA-25 ~ PA-27、PA-31 十二条 err 用例与 PA-16/PA-30/PA-32/PA-33 四条语义用例；IR 侧 6P/0F/30S） | `xmake test --all` 全矩阵（c/native/ir-c/ir-native），见 `ROADMAP.md` 里程碑「验收」行 |
