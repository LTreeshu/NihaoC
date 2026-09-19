# NihaoC 变更日志 | Changelog

版本号遵循 [Semantic Versioning](https://semver.org)。本文件记录 NihaoC 编译器（`ncc/`）的里程碑与版本变更。

## [v1.0.2] — 2026-09-19（待 tag）

1.0 冻结线的规范合规与代码卫生版本，无新增语言特性（`is _` 通配符属补全文档已承诺的既有语法，`structof` / `unionof` / `holdof` / `bitoffsetof` / `len` 属补回文档已承诺却缺失的内置函数，§5.1 的指针后缀链与切片写法属补回文档已承诺却编译不过的代码生成（PA-25），均非新特性）；三处语法收敛为**移除冗余记号**——`is` 的 `=>` 单语句形式（PA-13）、`?=` 安全赋值（PA-20）、`?.` / `?(` 安全解引用（PA-21），检查语义统一由 `=` 与 `.()` 自身承担。

- **`is` 移除 `=>` 单语句形式（PA-13，用户可见语法变更）**：`is <pattern> => <statement>` 全线下线，`is` 只保留块形式 `is <pattern> { ... }`。C 后端 `parser.c` 的 `parse_is_stmt` 删除 `TOK_FAT_ARROW` 分支（改报 `expected '{' after 'is' pattern`）；IR 前端 `irparse.c` 同分支删除（双前端与 BNF v2.2 一致，错误路径吞 token 防死循环）；`tests/pos/ir_is.nc` 清理 `=>` 用例。`=>` 仍由 lexer 识别为 `TOK_FAT_ARROW`（词法保留，语法不使用），`token.h` 注释同步。
- **`codegen.c` 死代码全链清理**：删除早期直出 C 的后端 `ncc/codegen.c`（507 行，已由 `cgen.c` + `parser.c` 管线取代）及其调用链——`linker.c` 相关 120 行、`ncc.h` 84 行声明与枚举、`ncc.c` 4 行后端注册；`ncc/xmake.lua` 的 `add_files` 去掉 `codegen.c`。
- **构建修复（用户可见）**：`xmake.lua` PATH 探测的 `gmatch` 模式误把盘符冒号当分隔符切开（`D:\...` 被截断）导致编译器探测失败；同时修正含空格 TCC 安装目录的参数引号处理。
- **BNF 收敛入文档**：v2.1 记录指针语法收敛（一元 `*` 解引用移除，解引用统一 `.()` / `.(T)` / `->`）；v2.2 重写 `<pattern>`（通配符 / 整数 / 负整数 / 闭区间 `lo..hi` / 枚举变体 / 可见性枚举 / 结构体解构与 ADT 变体解构（预留）/ 标识符），`<is-stmt>` 从 `<statement>` 移除、`while` 体内 `is-clause` 与 `do` 不支持 `is` 口径对齐，中英 §6.1/§13.3 与语法元素表同步。
- **TODO 分层拆分**：`TODO.md` 拆为 `ROADMAP.md`（跨分支里程碑与通用待办）+ `TODO-PA.md`（1.0 冻结线专属）+ `TODO-PB.md`（2.0 IR 线专属），并建立共享文档跨分支同步规则。
- **文档-实现全量一致性核对**：PB-27 子项状态回填、PB-29 `ret_vis` 检查条件表述修正（`ret_vis > VIS_VAR` 恒假 → 显式 `∈ {CONST, FLOW, STATIC}`）、ROADMAP 架构图清除已删除的 `ir_to_native.c` 引用、VERSIONING_ROADMAP 阶段 2 重复条目去重、`ncc.h` 后端注释与 `set_version` 对齐；中英 §12.2 的「参数前缀未强制」实现状态注改写为版本口径（1.0 不实现 / 2.0 线 PB-26 已实现），并撤除用户文档中的内联源码行号——行号统一由 `docs/IMPLEMENTATION_STATUS.md` 单点维护。
- **`is _` 通配符补全（规范合规）**：1.0 线 A 后端 `parse_is_stmt` 原先把 `_` 当普通标识符输出到 C（tcc 报 `'_' undeclared`），现补恒匹配分支（生成 `if (1)`，与 2.0 线同源实现）；IR 前端 `irparse.c` 同步补 `_` 分支（不发比较与 JZ，块直接执行）。`tests/pos/pattern.nc` 增加通配符用例并更新 `.expect`，c/native/ir-c/ir-native 四后端输出一致。（PA-15）
- **`is` 循环体外守卫（规范合规，用户可见报错变更）**：BNF §6 规定 `is` 仅配合 `while` 循环体，但 1.0 线 A 后端把 `TOK_IS` 挂在通用语句分支（`parser.c:1335` 旧位置），循环体外写 `is` 会被前端接受、错误延迟到 C 编译期才由 tcc 报 `'__is_val' undeclared`。现加 `while_depth` 计数守卫（`TOK_WHILE` 体内递增/退出时递减），循环体外（含 `do` 体、函数顶层、模块层）的 `is` 由前端即时报 `'is' pattern match only valid inside while loop body`，与 IR 前端既有守卫（`is_val_vreg < 0`）口径统一。新增 `tests/err/is_outside_while.nc` 覆盖。（PA-18）
- **`do` 体内 `is` 在 IR 前端拒绝（规范合规）**：IR 前端 `TOK_WHILE || TOK_DO` 分支此前无条件把条件值写入 `is_val_vreg`（守卫变量 `is_do` 算了却未使用），导致 `do cond { is pat { ... } }` 在 ir-c / ir-native 下被接受，与 BNF §6 / 中英 §6.1「`do` 不支持 `is`」相反（A 后端经 PA-18 已拒绝）。现 `do` 分支置 `is_val_vreg = -1`，体内 `is` 报 `ir: 'is' pattern match only valid inside while loop body`（文案去掉 `while/do` 的 `/do`），与 PB-27.1 对齐。新增 `tests/err/is_in_do_body.nc`。测试框架同步收紧：err 用例改为仅 `IR_ERR_SKIP` 列出的 M2 四条（`m2a`~`m2d`）在 IR 后端跳过，其余 err 用例四后端均执行。同项续做：A 后端进入 `do` 体前把 `while_depth` 清零、退出恢复，使嵌套在 `while` 内的 `do` 也拒绝 `is`（原会静默匹配外层条件值），与 IR 前端对齐；`tests/err/is_in_do_body.nc` 增加该嵌套情形。**文档理由句修正**：BNF §6 与中英 §6.1 原写「`do` 不支持 `is`，因为 `do` 先执行块再判断条件」——与实现相反（本语言 `do cond { ... }` 与 `while` 同为前测循环，A 后端生成 `while (cond) { body }`）。现改为陈述为规范规定（`is` 只绑定 `while`，`do` 是否纳入留待 2.0 定案），不再使用错误的因果论证。（PA-19）
- **`?=` 安全赋值记号从语法移除（PA-20，用户可见语法变更）**：文档曾把 `?=` 列为"安全赋值操作符（带指针检查）"，但 1.0 线实现里 `?=` 与 `=` 走完全同一条分支（声明、语句窥探、赋值三处共用），没有任何额外检查——因为普通 `=` 本身就无条件执行 §12.1 可见性兼容性检查。故 `?=` 是**纯冗余记号**且制造"只有 `?=` 才安全"的误解。现从语法移除：`parser.c` 的 `parse_assign` 不再与 `=` 共用分支，`case TOK_SAFE_ASSIGN` 即时报错 `'?=' is not part of the grammar; use '=' (every assignment is checked for visibility compatibility)`；`is_expr_continuer` 与语句窥探分支同样只认 `TOK_ASSIGN`。词法保留 `TOK_SAFE_ASSIGN`（lexer 仍识别，与 `=>` 同处理）。BNF 升级 **v2.3**：§1.4 运算符与 `<assign-op>` 去掉 `"?="`、说明改为词法保留、附录删除"安全赋值"行；中英 §5.1 多级指针示例与 §12 设计说明同步改写为"每次赋值都做可见性检查"，两份语法元素表移除 `?=` 行并给 `=` 补注。新增 `tests/err/safe_assign_removed.nc`（`.expect` 只取 `'?='` 子串，同时匹配 A 后端的专门文案与 IR 前端的通用 unexpected token）。IR 前端自始无 `?=` 分支，行为即为拒绝，无需改动。**PB 线仍接受 `?=`，待回灌**（见 `IMPLEMENTATION_STATUS.md`「2.0 线（PB）差异」）。
- **`?.` / `?(` 安全解引用记号从语法移除，检查落入 `.()`（PA-21，用户可见语法变更 + 检查补全）**：文档 §14.1 承诺"用 `?.` 做安全解引用，编译器结合可见性检查与边界检查"，但实现里 `?.` 只是 `.(` 的别名（`parse_postfix` 两 token 同路），`?(` 甚至从未成为 token（词法切成 `?` + `(`），所谓"安全"两种写法都没有。按"功能都加、语法收敛"决策：移除附加记号，把承诺的检查真正做进 `.()`。① A 后端 `parse_postfix` 循环加 `TOK_SAFE_DOT` 显式拒绝分支，报 `'?.' is not part of the grammar; use '.()' (dereference performs the visibility and bounds checks)`；`is_expr_continuer` 与解引用窥探去掉 `TOK_SAFE_DOT`。IR 前端 `irparse.c` 同位置加同文案分支（`ir: '?.' is not part of the grammar; ...`），两线口径一致。② `parse_deref_chain` 补编译期越界检查：读侧已有 `vis_check_usable`、写穿侧由赋值语句的 `vis_check_writable` 覆盖，新增宽度检查——`.(T)` 读取的 `sizeof(T)` 若超过该指针**静态已知**所指对象字节数即报错。字节数来源：`malloc(T)` / `malloc(T,n)`（n 为字面量）初值记入新的 `Symbol.pointee_bytes`，或 `&x` 取址由 `sym->type->ref->size` 推得；对指针本身重新赋值时清零（目标宽度不再静态可知，保守不检查，`lhs_was_deref` 区分 `p.(T) = v` 与 `p = v`）。③ 运行期 `NULL` 检查不属本项：非空由静态规则保证（指针声明必须初始化、不允许空指针，§5.1），`malloc` 返回值的运行期检查留待 2.0，文档按此改写并注明现状。BNF v2.3 同步：§1.4 运算符终端与 `<postfix-op>` 去掉 `"?." <identifier>` 与 `"?(" [ <type-name> ] ")"`，`.()` 说明改为承担可见性 + 越界检查，附录改列 `解引用 .() / .(T)`。中英 §5.1 示例（`ptr?.(i64)` → `ptr.(i64)` 并注越界报错）、§14.1「安全解引用与访问」整节改写，中英语法元素表移除 `?.` / `?(` 行、`.()` / `.(type)` 补注检查语义，英文运算符清单去掉 `?. ?(`。新增 `tests/err/safe_dot_removed.nc`、`tests/err/deref_bounds.nc`；后者依赖 `.(T)`，IR 前端仅支持裸 `p.()`，故一并加入 `IR_ERR_SKIP`。struct/array/宽分配探针确认无误报。**PB 线仍把 `?.` 当 `.(` 别名、`.(T)` 与宽度检查均无，待回灌**（见 `IMPLEMENTATION_STATUS.md`「2.0 线（PB）差异」）。
- **从属查询与位域偏移内置函数补实现（PA-22，2026-09-20，规范承诺落地 + 签名定稿）**：中英 §2.3 早已列出 `structof` / `unionof` / `holdof` / `bitoffsetof`，A 后端却完全没有这些分支——`parse_primary` 只把 `sizeof`/`typeof`/`alignof`/`offsetof`/`visof` 分派给 `parse_builtin_kw`，四个关键字落入表达式默认分支报 unexpected token。按"补漏、中英保持一致、补实现到 A 后端"决策执行，并进一步把签名定稿为**三参**（原二参 `structof(T, expr)` 拿不到成员偏移，无法由成员地址反推首地址）：① `structof(T, m, p)` / `unionof(T, m, p)` / `holdof(T, m, p)` 生成 `((void*)((char*)(p) - offsetof(T, m)))`（container_of 语义），并给出诊断——`T` 非聚合体、`structof` 用于 `union`、`unionof` 用于 `struct`、`m` 不是 `T` 的成员，均即时报错；`holdof` 对 `struct`/`union` 通用。② `bitoffsetof(T, m)` 语义定为**声明序位偏移**：以前置最近非位域成员为锚点，用 C 自身 `offsetof`/`sizeof` 求存储单元起点（对齐以基类型 `sizeof` 近似，基本整型 `align == size`），再加整单元数与单元内位数；目标非位域、基类型宽度未知、同组存储类型不一致时报错。③ BNF 升级 **v2.4**：`<builtin-call>` 三条从属查询改为三参并补 `bitoffsetof` 语义注释；中英 §2.3 同步定稿签名与"反推所属聚合体首地址"表述、加版本口径注（1.0 线由 A 后端提供，`ir-*` 属 2.0 布局待做），中英 §5.2 的 `structof` 示例改为可编译写法。两份语法元素表的四个关键字行补签名说明。④ 新增 `tests/pos/ownerof.nc`（struct/union 反推 + 位域 `8/12/64` 三值断言）与 `tests/err/structof_bad_member.nc`；IR 前端不改动（2.0 布局待做），pos 用例不入 `IR_SUBSET` 自动跳过、err 用例加入 `IR_ERR_SKIP`。**探针另发现两项待决缺口**：`alignof(T)` 生成 `_Alignof`，tcc 0.9.27 不提供该符号导致 `c` 后端链接失败；中英 §14.1 等处以逗号分隔结构体成员，而实现只接受空白分隔。均登记 `TODO-PA.md`（PA-24 / PA-23）待决策。**PB 线未接入这四个关键字，实现待回灌**。
- **指针后缀链与切片补实现（PA-25，2026-09-20，规范承诺落地）**：中英 §5.1.2 / §5.1.3 通篇使用 `p.().()`、`arryptr.(char[9])[0]`、`arrybuf char[8] = arryptr.(char[9])[0..7]`、`dptrarry1 void[3] = malloc(void[3])` 等写法，而 A 后端把解引用锁在独立的 `parse_deref_chain` 里——`.(T)` 之后不能再接下标/成员/切片，空 `[]`、切片读、切片写、数组初始化退化全部无法编译。按"补实现，不降级文档"决策执行：① 删除 `parse_deref_chain`，解引用改为**通用 postfix 链**上的 `deref_step` / `deref_prefix`（每个算子把已生成的前缀 C 文本整体包裹后继续），数组类型化解引用生成合法的 `T (*)(N)` 转换（`p.(char[9])` → `*(char (*)[9])(p)`）。② 空下标 `p[]` 定义为省略类型的 `p.()`（一层解引用），§5.1.1 的三级指针链 `p.().().(i32)` 遂可写作 `p[][].(i32)`。③ 切片读 `p[a..b]` 生成"指向第 a 号元素的指针"（C 无数组切片类型，长度不进入值本身），`[..b]` 省略起点等价 `[0..b]`；赋给固定数组 `T[n] x = p[a..b]` 时按**声明容量**逐元素复制（`memcpy`），非别名。④ 切片写 `p[a..b] = {v0, v1, …}` 逐元素写回（逗号表达式），右值非值列表时报错。⑤ `T[n] x = malloc(T[n])` 声明退化为指针（仅省略最前维度：`void[4][5]` → `void* (*x)[5]`），`malloc` 字节数按元素个数乘算，供 `.(T)` 宽度检查使用。⑥ 通用 `void` 指针（`varptr void = &x`）元素宽度静态未知，裸下标/切片改为前端即时报错并给出修复写法：`cannot subscript the generic 'void' pointer 'x'; write x.(T)[i] to fix the element type first`（文档 §5.1.2 相应把 `arryptr[0] = 0` 改写为 `arryptr.(char[9])[0] = 0`）。⑦ 顺带修复多维数组声明 `i32[2][3]` 只输出最后一维（`c_type_suffix` 现按源码顺序输出全部维度）。文档：BNF 升 **v2.5**（`<postfix-op>` 的 `[]` / `.()` 语义注释 + 通用 void 指针限制）、中英 §5.1.2 / §5.1.3 示例全部改写为可编译形式并加"数组指针与切片""`T[n]` 配 `malloc` 退化"两条定案说明、§5.1.3 原先错误的 `.(int64)` 报错注释改为实际的跳过检查口径（指针数组槽位宽度静态不可知）。测试：新增 `tests/pos/deref_slice.nc`（8 项断言，覆盖上述全部形态）与 `tests/err/void_subscript.nc`（入 `IR_ERR_SKIP`）。IR 前端不改动（属 2.0 布局/代码生成待做），pos 用例不入 `IR_SUBSET` 自动跳过。**PB 线未接入，代码生成待回灌**；§5.1.4 的字符串右值切片赋值（`stptr.(char[9])[0..8] = "xiaoming"`）登记为 PA-26。
- **`len(x)` 内置函数补实现（PA-27，2026-09-20，规范承诺落地）**：中英 §2.3 自始把 `len(x)` 列为内置查询（数组=声明容量 / 动态字符串=字面量长度 / 切片=边界差），A 后端却完全没有这个分支——`len` 被当成普通标识符，符号表里没有它就直接报错。按"三类全补"决策实现：① 在 `Symbol` 上新增逻辑长度记录（`len_known` / `logical_len`），于**声明期**登记：数组取各维乘积（多维 `len` 即元素总数），`char[] s = "..."` 这类未定长数组/字符串取字面量长度，类型推断的切片声明取 `hi-lo`。② `len` 接入 `parse_primary` 的标识符内置函数表（实参必须是标识符）；实参的逻辑长度**静态不可知**时前端即时报 `len: logical length of 'x' is not statically known`，而不是返回一个过期或零值。③ 对变量整体重新赋值（`s = ...` 而非 `s[i] = ...`）时撤销登记，避免 `len` 返回与当前值无关的旧长度。④ 类型推断的切片声明 `s = p[a..b]` 改为**指针视图**代码生成（`E *s = &p[a]`，`s.()` 即第 a 号元素），修掉 PA-25 遗留缺陷——旧实现按右界分配并 `memcpy`，会读到切片右界之外；固定数组 `T[n] x = p[a..b]` 仍按声明容量复制。文档：BNF 升 **v2.6**（`len` 产生式注明三类取值与"多维为各维乘积"，并加版本口径）、中英 §2.3 的 `len` 条目定稿（数组=容量、动态字符串=字面量长度、切片=`hi-lo`，静态不可知即报错；`ir-*` 后端三类取值已有但不可知时返回 0）、中英 §5.1.2 的切片定案注由"`b` 只作书写提示、不记录长度"改写为"上下界同为字面量时记录 `b-a` 供 `len()`"，并写明视图/复制之分。测试：新增 `tests/pos/len_builtin.nc`（8 项断言：数组、多维乘积、动态字符串、字面量、闭区间切片、左开区间、切片复制、整体重赋值后不受影响）与 `tests/err/len_unknown.nc`（入 `IR_ERR_SKIP`）。IR 前端不改动。**PB 线未接入 `len` 的报错口径与视图代码生成，实现待回灌**。
- **`else if` 在 IR 前端补支持（PA-28，2026-09-20，规范承诺落地）**：BNF `<if-stmt> ::= "if" <expr> <block> [ "else" ( <if-stmt> | <block> ) ]` 与中英 §6/§13 的示例一贯包含 `else if`，A 后端 `parse_if_stmt` 也支持，但 IR 前端在 `else` 之后无条件进入块解析，`else if` 在 ir-c / ir-native 下报 `expected '{', got 'if'`。修法：把 IR 前端的 if 语句抽成 `ir_if_stmt`，`else` 后遇 `if` 即递归自身，多级分支共用同一条 `JZ`/`JMP` 标签链（不新增 IR 指令，也不改变已有 `if`/`else` 代码生成）。新增 `tests/pos/elseif_chain.nc`（链中命中、中间命中且末尾无 `else`、全不命中、循环体内、`else` 普通块五种形态）并入 `IR_SUBSET`，四后端输出一致。文档：BNF v2.6 版本行追加记录该口径（产生式未变），`IMPLEMENTATION_STATUS.md` 新增「条件与分支（§6）」节并在「2.0 线（PB）差异」登记 PB `irparse.c` 仍缺该项。**PB 线待回灌**。
- **`is` 多子句 fallthrough 缺口（已登记未修）**：文档 R 规则"首个匹配者执行（无 fallthrough）"在两线均未实现——多个 is-clause 生成并列 `if`，条件重叠时会连续执行（`is _` 恒匹配使该问题更易触发）。属既有语义缺陷，需改动 `is` 控制流生成方式，未纳入 v1.0.2；详见 `docs/IMPLEMENTATION_STATUS.md`。（PA-16）
- **PA TODO 处理完毕**：PA-1 ~ PA-15、PA-17 ~ PA-22、PA-25、PA-27、PA-28 全部 `[x]`；唯余 PA-16（fallthrough 缺口）、PA-23 / PA-24（PA-22 探针发现）与 PA-26（切片赋值的字符串右值）登记待决策，不纳入本版。
- **已知缺口（登记于 `docs/IMPLEMENTATION_STATUS.md`）**：1.0 线 A 后端 `is <identifier>` 变量绑定为"按值比较"而非绑定（2.0 线覆盖）；函数参数可见性前缀仍统一按 `VIS_DEFAULT` 处理；IR 后端不做所有权/借用检查。
- **共享文档 PA↔PB 双向同步（2026-09-19）**：以 `merge-base 7d4c652` 三方合并把本版本成果同步进 PB（PB 侧 `37a112a`），并把 PB 侧更新的 4 项回灌本线——① 中英 §5.1 指针声明节定案表述（"隐式推断声明"，原"双支持"自 v1.0.1 起过时）；② BNF `<array-size>` 细目 + 动态数组写法三条说明 + `<is-clause>`/`<pattern>` 注释；③ `VERSIONING_ROADMAP.md` 的 2.0 进展条目（阶段1 类型化指针、PB-26 M2 移植、ir-c 变量名保留已由 `[ ]` 转 `[x]`）与 A 方案独占文件清单残留的 `codegen.c` 修正；④ `IMPLEMENTATION_STATUS.md` 增补「2.0 线（PB）差异」节并修正 `pattern.nc` 覆盖描述。`docs/LEGACY_CODEGEN.md` 归档文档一并纳入共享集。同步后两条线共享文档一致（`TODO-PA.md` / `TODO-PB.md` 为分支专属，不计入）。
- **回归验证**：c / native 双后端各 **24P / 0F / 5S**、ir-c / ir-native 各 **6P / 0F / 19S**（0 FAIL，含 PA-18 ~ PA-22、PA-25、PA-27 八条新 err 用例与 `ownerof` / `deref_slice` / `len_builtin` / `elseif_chain` 用例），跨后端一致性检查全部通过；examples 6/7 编译运行（`06_cooking` 为 2.0 预览，预期不通过，IR 后端下可通过）；`pattern.nc` 经 ir-c / ir-native 实测输出与全量后端一致。

## [v1.0.1] — 2026-09-03

1.0.x 首个补丁版本：指针语法收敛 + Linux 平台修复 + 规范文档定案。

- **移除带 `*` 的指针语法形式（与 BNF/文档一致）**：删除具名指针类型声明 `T*`（如 `p T* = &x`）与一元 `*` 解引用（`*p` 读/写/复合 `*p op=`）；指针解引用统一收敛为 `.()` / `.(T)` / `->`。`void` 通用指针、`void[n]` 指针数组、隐式推断 `p = &x` 与乘法 `*` / `*=` 均保留（属不同语义，非指针语法）。
- **编译器（A 方案 parser.c）**：`parse_type` 删除 `TOK_STAR` 指针构造分支（仅保留 `[]` 数组后缀，并修正 `while` 条件避免 `*` 死循环）；`parse_unary` 删除一元 `*` 解引用分支（落入默认报错）。内部 `TYPE_POINTER` 类型与 `cgen.c` 的 `T*`/`void*` 文本映射**保留**（生成可编译 C 的前提，与用户输入语法无关）。
- **示例与测试同步改写**：`examples/04_pointer.nc`、`tests/pos/ir_ptr.nc`、`ir_ptr2.nc`、`ir_slice.nc` 的 `*p` / `*(&x)` / `*p op=` 全部改为 `.()` 形式；复合 `*p += e` 展开为 `p.() = p.() + e`（双后端兼容写法）。
- **回归验证**：c/native 双后端 **13P / 0F / 5S**（0 FAIL），`examples/04_pointer.nc` 运行输出 `deref write ok` / `addr deref ok`。
- **B 方案（PB）待办登记**：`irparse.c` 仍支持一元 `*p`（读/写/复合 RMW）且 `.() op=` 缺失，已登记 `TODO.md` 专项二 PB-25（移除一元 `*p` + 补 `.() op=`，本次 PB 不实现）。
- **`is` 模式匹配规范定案（仅文档）**：`is` 仅配合 `while`（`do` 不支持）、移除 `=>` 箭头形式；BNF 重写 pattern 产生式；中英 §6.1 新增模式匹配子节（语义表 + R1–R4）。语法实现变更落在 v1.0.2（PA-13）。
- **§12 指针传递矩阵文档统一**：合并 §12.1+§12.2 为单一 §12.1（4×4 矩阵为主、16 条枚举为辅）；§12.3→§12.2 重编号；新建 `docs/IMPLEMENTATION_STATUS.md` 规范-实现对应表。

### Linux 平台修复（2026-08-31 WSL 实测，PA-9）

- `xmake.lua`：os.exec 在 Linux 不走 shell（`|| true` 被当作程序参数）→ Linux 分支显式 `/bin/sh -c` 包装；非 Windows 跳过 p0_link target 与 ir-native 测试
- `cgen.c`：c_type_name 三处 static buf 重叠写（递归调用 src==dst，Linux glibc 损坏输出）→ 独立 `char tmp[256]` 拷贝
- `native.c`：源码构建 libtcc.so 未导出 `tcc_install_dir`（隐式声明 int 截断指针）→ 非 Windows 硬编码 `/usr/local/lib/tcc`
- `native.c`：`-run` 内存执行误判 `tcc_relocate(s, NULL)` 返回值（NULL 语义为"返回所需内存大小"，>0 即成功）→ 改为直接 `tcc_run`（内部自动 relocate）
- 构建依赖：libtcc.so 内部符号（sym_push 等）与 ncc 重名被 ELF 符号插值劫持 → 以 `make libtcc.so LDFLAGS="-fPIC -Wl,-Bsymbolic"` 重新构建安装

## [v1.0.0] — 2026-08-31

**首个对外版本（A 方案产品线）**。A 方案（parser → C 文本 → tcc）功能闭环，特性冻结，只修 bug/文档。

### 功能（A 方案 c/native 后端）

- 全量语法回归 **12P / 0F / 5S**（c/native 双后端一致）
- 指针声明语法定案：**隐式推断声明**（`p = &x` 自动推断为指向 x 的指针）；`->` 指针成员访问（链式/复合赋值）；1.0.x 起具名指针 `T*` 显式声明与一元 `*` 解引用已移除，解引用统一 `.()` / `.(T)` / `->`
- 三元 `?:`、`is pat => stmt` 单语句匹配（`=>` 新 token；已于 1.0.2 / PA-13 与 2.0 / PB-27.6 移除，`is` 只保留块形式）、goto/label
- struct/union/enum（嵌套、位域、嵌套初始化列表、整体拷贝）、数组/动态数组（固定容量）、切片、多返回值（命名 struct 返回，C 机制）
- 存储期与所有权（const/static/flow/var + 借用状态机）、cooking 编译期（常量/函数/static_assert）、len()、visof()
- 后端 `-backend=native`：libtcc 进程内编译执行；`-run` 内存执行（Linux only）

### 工程质量

- CLI 完整：init/build/run/debug/lex + -o/-c/-shared/-static/-I/-L/-l/--link/-g
- 构建统一 xmake（Makefile 标 legacy）；错误信息"外壳中文 + 正文英文"
- examples/ 示例集 7 例（6 例 1.0 可编译运行，06_cooking 为 2.0 预览）
- 语言规格冻结：BNF v2.0 终校（`=>`/`->` 补全）+ 中英语法元素表核对；1.0.x 起 `T*` 具名指针与一元 `*` 解引用已从 BNF 移除
- **双平台验证（Windows + Linux/WSL Ubuntu-24.04）**：c/native 全量回归 0 FAIL（12P/0F）+ examples 6/6 双后端一致；`-run` 内存执行 Linux 实测通过

### 明确留给 2.0

- 动态数组增长、切片运行时边界、命名空间
- IR 一切（ir-c/ir-native/ir-riscv64/ir-arm64/ir-loongarch64，当前为 2.0 预览）
- native 寄存器分配（性能项）；ir-native 在 Linux ELF 运行时崩溃（2.0 预览，Windows PE 正常）

## [2.0-dev] — 未发布（B 方案 IR 演进线）

- **阶段 1 类型化指针模型（2026-08-31）**：`p = &标量` 记录指向类型（pt[] 表 PT_SCALAR 编码，聚合/标量/枚举三分支）；解引用读按指向类型标记浮点；解引用写按指向类型 coerce（窄型 TRUNC 截断 / double ITOD）；复合赋值 RMW；指针算术 `p + k` ×8 槽宽缩放（与数组寻址一致）。新用例 ir_ptr2.nc（IR_SUBSET）读/写/narrow 截断/double/复合+算术 5 组断言，四后端一致 0 FAIL

### 2.0 指针语法收敛（2026-09-04，对齐 A 方案 1.0.x）

- **移除一元 `*p` 解引用（与 BNF/文档一致）**：`irparse.c` 删除一元 `*` 解引用读（`ir_primary`）、`*p = e` / `*p op=` 写+复合（`ir_stmt` 两处），改为 `nihao_error` + 吞 token 防死循环；乘法 `*` 不受影响。指针解引用统一收敛为 `.()` / `.(T)` / `->`。`void` 通用指针、`void[n]` 指针数组与隐式推断 `p = &x` 保留。
- **补 `.() op= e` 复合解引用**：`ir_stmt` 的 `.() = e` 写块支持 `+= -= *= /= %=`——LOAD 当前值 → 按指向类型协调（double 指向走 FADD/FSUB/FMUL/FDIV 且 LOAD 结果直接 `ir_set_double` 勿 ITOD）→ op → 按指向类型 `ir_coerce` 截断 → STORE。
- **修复 2 个既有 IR 后端正确性缺陷**（此前被 `ir_ptr2.nc` 无 `.expect` + 跨后端一致性检查"错得一样也算 PASS"掩盖）：① `.() read` 对浮点指向（f64/f32）标记结果 vreg 为 double，避免位模式被当整数 ITOD 误转；② `.() = e` / `.() op=` 按**指向类型** `pt[vi]` 截断（窄型 TRUNC / double→int 兜底），原代码误用指针自身类型 `vtype[vi]` 导致窄写不截断。
- **测试同步改写**：`ir_ptr2.nc` 复合解引用改为 `p.() += 2` / `pd.() += 1.0` / `pc.() += 1` / `p.() *= 3` / `p.() -= 10`（含 int/double/narrow/mul/sub），四后端一致 0 FAIL；`ir_ptr.nc`/`ir_slice.nc` 保持通过。
- **回归验证**：`xmake test --all` 全矩阵（c/native/ir-c/ir-native）**0 FAIL**，`ir_ptr`/`ir_ptr2`/`ir_slice` 四后端一致。

### 2.0 M2 能力平移：参数前缀 + 返回值可见性（2026-09-04 / 09-10 / 09-12，PB-26 / PB-29）

- **PB-26 参数前缀（2026-09-04）**：函数参数声明支持 `flow/var/const/static` 可见性前缀，记录到变量表 `vvis`；调用点按参数前缀执行 M2 所有权/借用检查（冻结/失效状态机），`err/m2a..m2e` 系列用例在 ir-c/ir-native 双后端转正 PASS。
- **PB-29 返回值可见性前缀（2026-09-10，§12.3 后半段）**：函数声明 `func name(...) [vis] ret_type` 解析返回前缀到 `IrFn.ret_vis`（IR 端）/`Symbol.ret_vis`（cgen 端）；调用点声明赋值按 `ir_vis_transfer(ret_vis, decl_vis)` 检查所有权转移；cgen 端 `flow/const/static` 返回生成 `void*` 签名。新增用例 `pos/return_flow.nc`、`pos/static_param_share.nc`、`pos/struct_param_prefix.nc`、`err/m2f_retflow.nc`。
- **PB-29.1 错误路径补全（2026-09-12）**：`m2f_retflow` 六条禁止路径全覆盖（`flow→static`、`const→flow/static/var`、`static→flow/var` 等），拆分为 6 个独立 err 用例。

### 2.0 `is` 模式匹配升级（2026-09-16，PB-27，规范定案 2026-09-01）

- **Bug 修复**：`do` 循环内 `is` 拒绝（不再设置 `__is_val`，防静默匹配外层 while）；畸形模式 `is -` / `is 5..` 强制校验后续 token；反向范围 `is 5..2` 编译期报错（lo≤hi）；irparse 冗余 `VIS_*` 宏删除，与 `ncc.h` Visibility 枚举统一（`VIS_UNDEF=0/VIS_CONST=1/VIS_FLOW=2/VIS_STATIC=3/VIS_VAR=4`，`VIS_DEFAULT` 全代码更名 `VIS_VAR`）。
- **模式扩展**：通配符 `_` 恒匹配（IR 与 A 端 parse_is_stmt 同步支持）；`__is_val` 类型感知（新增 `emit_is_cmp_with_const`，double 条件走 `IR_FCMP`，不再硬编码 int）；清理重复不可达可见性分支；**移除 `is <pat> => <stmt>` 单语句形式**（A 端 PA-13 已于 09-15 移除，`=>` 保留词法识别但语法不使用，见 BNF v2.2）。
- **附带修正**：PB-29 M2 返回检查条件 `ret_vis > VIS_VAR`（VIS_VAR=4 恒假）改为显式 `== CONST/FLOW/STATIC`。
- **回归**：`xmake test` 全矩阵 0 FAIL（c/native 19P/0F/6S；ir-c/ir-native 13P/0F/8S）；`err/m2f_retflow*` 6 条禁止路径全 PASS。

## [M4] — CLI 工具链

- `nihao init/build/run/debug/lex` 子命令；`debug <file> --ir` 三地址码 dump
- 回归测试套件（tests/pos + tests/err，xmake 驱动全矩阵）

## [M3] — 模块与自动释放

- `use` 跨文件解析、`flow` 自动释放、`print` 映射、`main` 返回 0

## [M2] — 静态分析

- 所有权/借用静态分析：存储期矩阵 + 冻结/失效状态机 + 作用域解冻

## [M1] — 核心编译管线

- 表达式/声明/函数/语句解析 + C 后端单趟转 C（端到端可运行）
- 修复数组/位域/for/别名/while 系列 bug

## [M0] — 骨架复活

- 统一 token 命名、补关键字、词法最长匹配、恢复编译管线

## 基线

- 导入 ncc 骨架现状
