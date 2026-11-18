# IR 线缺口台账（P1）

> **2026-11-12 口径变更：A 线已物理删除。** 本文件此前是「IR 缺口 = 只有 A 能编 / 与 A 语义不同」的追平台账，判据与分桶（G1 语法缺失「IR 前端可自决」、G2 语义缺失「先定口径」、G3 布局缺失「待裁第 23」）都建立在「A 线作尺子、IR 要追平 A」的前提上。**A 线删除后，这把尺子不再存在**——下面的 9 份 pos 缺口（真实布局家族）与 9 份 err 缺口（E-a 文案 / E-b 依赖 pos 布局）改读作 **IR 线的已知边界**：语法与诊断已尽量补齐（G1 / E-c 桶已清空），剩下的**真实字节布局**（聚合体成员偏移、数组成员真实宽、offsetof/四个从属查询内置）是 IR 线尚未实现的能力，不再是「待 A 追平」的欠账。分桶与条目按历史原样保留，仅此处声明新口径。

生成日期：2026-11-10（PB-61）。**分支专属文档**（PB），非设计层：允许出现计数、桶名与在册条目编号，不允许当规范引用。
判据与原始数据：`ncc/build/p61/gap_raw.tsv`（每份用例在 ir-c 的第一手 stderr）与 `ncc/build/p61/promotable.tsv`
（四档是否都能跑、输出是否逐行一致、`.expect` 子串是否命中）。这两份是 gitignore 的中间产物，**本文件是它们的唯一
正式落点**。

## 0. 口径修正（先记账，再看表）

- 先前各处写的「err 侧 IR 只覆盖 7/33」是**名单条目数**，不是实际参与数。实跑口径：IR 两档每档跑 18 份、跳过 15 份。
  那 18 份里只有 7 份写在 `IR_ERR_COVERED`，另 **11 份 m2 族是靠门禁里 `stem:startswith("m2")` 这条命名豁免才进档**
  ——名单说了不算、名字说了算。本轮把这 11 份显式列入 `IR_ERR_COVERED` 并**删掉该前缀豁免**，此后「IR 跑哪些 err」
  只由名单决定。复跑门禁：c / native **68P/0F/3S**、ir-c / ir-native **28P/0F/43S**，0 FAIL，err 档 SKIP 仍 30 行
  （即每档 15 份），**行为一字未变**。
- pos 侧「IR 能编 44 份」= `IR_SUBSET` 41 + `IR_ONLY` 3，与门禁里 ir-c 的 PASS 数（28）不同源：后者只数带 `.expect`
  的那部分，其余进跨后端比对池。**PB-66 后这行改为「IR 能编 49 份」= `IR_SUBSET` 46 + `IR_ONLY` 3**（顶层声明与 `alias`
  簇五份放行入名单），pos 侧缺口相应由 28 份收到 **23** 份；门禁 ir-c / ir-native 的 PASS 数同步 35 → **40**。
  **PB-67 后这行再改为「IR 能编 51 份」= `IR_SUBSET` 48 + `IR_ONLY` 3**（数组 / 切片容量档簇里两份放行入名单），
  pos 侧缺口由 23 份收到 **21** 份；门禁 ir-c / ir-native 的 PASS 数同步 40 → **42**、SKIP 29 → **27**。
- **PB-68（2026-11-12）后这行再改为「IR 能编 52 份」= `IR_SUBSET` 49 + `IR_ONLY` 3**（指针类型后缀档的
  `deref_slice` 放行入名单），pos 侧缺口由 21 份收到 **20** 份；门禁 ir-c / ir-native 的 PASS 数同步
  42 → **43**、SKIP 27 → **26**，c / native **68P/0F/3S** 不变、四档 0 FAIL。
- **2026-11-12（PB-69）计数更正：上面三行的 `IR_SUBSET` 数与 pos 缺口数都错了，门禁数一直是对的。**
  上面 PB-66 / PB-67 / PB-68 三行**原文保留不改**（历史条目），以本条为准：

  | 轮次 | 原记 `IR_SUBSET` | 实测 | 原记 IR 能编 | 实测 | 原记 pos 缺口 | 实测 |
  |---|---|---|---|---|---|---|
  | PB-66 后 | 46 | **51** | 49 | **54** | 23 | **18** |
  | PB-67 后 | 48 | **53** | 51 | **56** | 21 | **16** |
  | PB-68 后 | 49 | **54** | 52 | **57** | 20 | **15** |

  **根因是单一的、恒定偏 5**：PB-66 那轮把**改前**的 46 当成**改后**写进台账（该轮放行 5 份，46 + 5 = 51），
  PB-67 / PB-68 再逐轮 +2 / +1，错基址一路沿用；pos 缺口 = 72 − (`IR_SUBSET` + 3)，所以也跟着恒偏 5。
  这三轮**没有增删任何用例文件**（`git diff --name-status HEAD~2 HEAD -- ncc/tests/` 为空），故分母恒为 pos 72 / err 33。
  **判据（可复算，不靠名单自述）**：三份名单的条目**与 `tests/pos/` `tests/err/` 里真实存在的 `.nc` 求交集**再取补集。
  实测 `IR_SUBSET` **54** 条、`IR_ONLY` **3** 条（`ir_builtin` / `ir_slice` / `ir_sparam`）、`IR_ERR_COVERED` **22** 条，
  **全部有对应文件，无悬空条目**。⇒ **pos：IR 能编 57 / 72，缺口 15**；**err：IR 覆盖 22 / 33，缺口 11**。
  **与门禁对账（这才是发现错误的入口）**：`xmake test -b ir-c` 实跑 **21 条 `[PASS] pos/` + 22 条 `[PASS] err/` = 43 PASS**、
  **15 条 `[SKIP] pos/` + 11 条 `[SKIP] err/` = 26 SKIP**，与 tally 逐项吻合。那 21 条 pos PASS 里有 **2 条是无 `.expect`
  的模块型用例**（`mathmod` / `module_top_forms`，`has_main` 判假、只出 `.c` 产物即计 PASS），所以「pos 带 `.expect`
  且在名单内 = 19」加这 2 条才是 21——**这一条也解释了为什么光看 PASS 数推不出名单大小**。
  **另有一处与计数无关的行错误**：§2 表里 `borrow` 行早已过期（PB-64 就放行了），而真正还在缺口的 `literal_forms`
  **从未进表**；两者一出一入，使表的行数（15）恰好等于真缺口数却**成员错一个**，光数行数发现不了。已在本轮改正。
  **教训（第五次同族）**：白名单 / 台账里的数字要**拿名单 ∩ 实际文件、再与门禁 tally 三方对账**，不能只在上一轮
  的数字上加本轮增量——PB-54 / PB-56 立的那条「理由要复测」同样适用于**计数**。
- **PB-70（2026-11-12）后现值**：`IR_SUBSET` **57** + `IR_ONLY` 3 ⇒ **pos IR 能编 60 / 72，缺口 12**；
  `IR_ERR_COVERED` 22 不变 ⇒ **err 覆盖 22 / 33，缺口 11**。门禁 ir-c / ir-native **43 → 46 PASS**、
  SKIP **26 → 23**（本轮共放行 3 份），c / native **68P/0F/3S** 不变、四档 0 FAIL。
  三方对账已按 §4 第 5 条执行：名单 ∩ 文件 = 57 / 3 / 22 无悬空；补集 12 与本文件标题及桶分布一致；
  `xmake test -b ir-c` 的 `[PASS]` / `[SKIP]` 逐类计数吻合。
- **PB-71 / PB-72（2026-11-12）后现值**：`IR_SUBSET` **58** + `IR_ONLY` 3 ⇒ **pos IR 能编 61 / 72，缺口 11**；
  `IR_ERR_COVERED` **24** ⇒ **err 覆盖 24 / 33，缺口 9**。门禁 ir-c / ir-native **46 → 49 PASS**、
  SKIP **23 → 20**，c / native 68P/0F/3S 不变、四档 0 FAIL；Linux 68P/0F/4S + **49P/0F/21S**、0 FAIL。
  **E-c 桶（非法程序被静默接受）本轮清空**——这是台账里最该优先收的一桶（PB-61 的排期结论第 4 条）。
- **PB-73（2026-11-12）后现值**：`IR_SUBSET` **59** + `IR_ONLY` 3 ⇒ **pos IR 能编 62 / 72，缺口 10**；err 覆盖 **24 / 33，缺口 9** 不变。门禁 ir-c / ir-native **49 → 50 PASS**、SKIP **20 → 19**，c / native 与比对档不变、四档 0 FAIL；Linux 68P/0F/4S + **50P/0F/20S**、0 FAIL。**⇒ 剩下的 pos 缺口 10 份全部挂待裁第 23 条（槽宽 vs 真实布局），`irparse.c` 一块能自决的活都不剩了；err 9 份里 E-a 3 属待裁第 26 条、E-b 6 依赖 pos 的 G3。**
- **PB-74（2026-11-12）后现值**：ltree 裁第 23 条选 **(b) 写宽降到元素宽度**。第一刀 `alignof` 真实对齐已放行
  `alignof_builtin`：`IR_SUBSET` **60**，**pos 缺口 9**；门禁 ir-c / ir-native **50 → 51 PASS**、SKIP 19 → 18，
  Linux 68P/0F/4S + **51P/0F/19S**、0 FAIL；交叉 `comm` 与 PB-73 基线**逐条不变**、`alignof_builtin` 两档输出
  改对。剩余 9 份仍需「按字节宽的存储/寻址层改造」（阶段 A 窄标量 + 阶段 B 聚合/数组真实布局），见 §10。

## 1. 四桶定义

| 桶 | 判据 | 谁该动 |
|---|---|---|
| **G1 语法缺失** | IR 前端拒绝一句 BNF 合法的源文本（解析层就断） | `irparse.c`，PB 可自决 |
| **G2 语义缺失** | IR 接受并出码，但含义与 A 线不同 / 出码非法 / 静默接受非法程序 | 先定口径再动，多数牵共享面 |
| **G3 布局缺失** | 根因是「聚合体没有真实内存布局」（待裁第 23 条） | 需 ltree 先裁槽宽与布局口径 |
| **G4 名单与用例** | 缺口在测试资产或名单本身：可提升、文案不同源、用例写法越界 | 改名单 / 改用例，不动编译器 |

## 2. pos 缺口 9 份（72 份总数中，只有 A 线能编或能编但语义不同的那些）

> **2026-11-12（PB-69）更正**：本节标题原写「20 份」，实测 **15** 份（判据与根因见 §0 末条）。表内同时改正两处成员错误：`borrow` 行划掉（PB-64 已放行）、补入从未进表的 `literal_forms`。
> **2026-11-12（PB-70）**：15 → **12** 份（放行 3 份），另 5 份改归 G3；**G1 桶清空**，详见本节末的桶分布。

| 用例 | IR 侧第一手结果 | 桶 | 要补的动作 |
|---|---|---|---|
| ~~`alignof_builtin`~~ | ~~四档都出码，IR 把 `align ... ok` 全打成 `bad`~~ | G2→放行 | **PB-74 已解**：`alignof` 改按类型真实对齐折叠——标量=字节宽（`void`/`string` 为指针对齐 8），聚合体=成员最大对齐（递归嵌套、union 同），enum=4，数组=元素对齐。为此给 `IrAggType` 补了标量成员字节宽 `msize[]`（此前只记数组成员宽）。四档输出与 `.expect` 九行逐字节相同，已入 `IR_SUBSET`。**这是 ltree 选 (b)「写宽降到元素宽度」后的第一刀** |
| ~~`is_no_fallthrough`~~ | ~~四档都出码，IR 多打 `wild` / `range`~~（14 行 vs A 的 7 行） | G2→放行 | **PB-71 已解**：按 A 侧 `__is_matched`（PA-16 / P4）同口径移植——新增 `is_matched_vreg`，每轮 while 迭代在条件检查后、body 之前重置 0；每条 `is` 子句开头 `IR_JNZ` 跳过、匹配后进块前 `MOV` 置 1。四档输出与 `.expect` 七行逐字节相同，已入 `IR_SUBSET` |
| `multi_arr_decl` | IR 出码非法 C：`lvalue expected` | G2 | 多维数组读写在 IR 是降级路径，产物编译期就错 |
| `str_array_init` | IR 出码：`assignment makes integer from pointer` | G2 | 字符串数组初始化按整型槽发，指针没落位 |
| ~~`use_mod`~~ | ~~IR 出码：`implicit declaration of function 'add'` + `tcc: undefined symbol 'add'`~~ | G2→放行 | **PB-73 已解**：IR 侧模块加载落地——`ir_parse_file` 的顶层声明循环抽成可重入的 `ir_parse_top_level`，`use` 由 `ir_use_decl` 触发 `ir_use_module`，按 cwd / 源目录 / stdlib 三层解析模块文件（点号名按目录层级），保存 / 还原 lexer 后把模块顶层声明内联进同一个 `IrProg`（与 A 侧 `module_import` 同语义，错误定位在模块自己的文件名与行号上），循环 import 由 `ir_imported` 挡。四档输出与 `.expect` 逐字节相同，已入 `IR_SUBSET` |
| ~~`flow` / `flow_move` / `flow_no_free` / `malloc_demo`~~ | **PB-63 已解**：真正卡点只有 `p.(T)` 一处（`malloc(类型)` 一直没问题——实测 `p void = malloc(i32)` 在 IR 能编能跑） | G1→放行 1 | `malloc_demo` 四档一致已入 `IR_SUBSET`；`flow` / `flow_move` / `flow_no_free` / `borrow` 过前端后改卡 **IR 不下降 `print`**（待裁第 7 条；IR-only 名单下已属必须做） |
| ~~`borrow`~~ | ~~同 `flow` 族~~ | ~~G1→卡 print~~ | **PB-64 已放行**（`print` 下降后四档一致，已在 `IR_SUBSET`）。**2026-11-12 PB-69 划掉**：本行自 PB-64 起就是过期行，却一直留在缺口表里充数 |
| ~~`literal_forms`~~ | ~~`ir: unexpected token '+' in expression`~~（12:14 `p i32 = +23`、14:15 `bn i32 = +0b10000`、24:15 `f4 f64 = +1.5e-2`）+ `ir: unexpected token 'true' in expression`（44:18 `t bool = true`），共 4 条；A 档能编能跑 | G1→放行 | **PB-69 补入、PB-70 同日解掉**：一元 `+` 作字面量前缀（A 侧 P7 / PA-57 已落，IR 侧没有）、布尔字面量 `true` / `false`、**以及第三处此前没看出来的卡点——声明型 `s string = "lit"` 的 `len(s)` 静默给 0**（PB-67 只覆盖了字面量推断与 `char[]` 两条路径）。三处补齐后四档输出与 `.expect` 逐字节相同，已入 `IR_SUBSET` |
| ~~`len_builtin` / `array_capacity_tiers`~~ | **PB-67 已解**：多维数组声明、切片读入声明数组、开区间切片 rhs、字面量推断字符串的 `len()` 四处补齐后四档输出逐字相同，已入 `IR_SUBSET` | G1→放行 2 | — |
| ~~`deref_slice`~~ | **PB-68 已解**：后缀链求值器落地后四档实跑输出逐字相同、与 `.expect` 逐字节相同（八行 ok），已入 `IR_SUBSET`。原快照留档：`ir: array dimension 1 must have a positive length`（`void[][]`）/ `expected ']', got '['`（`p3[][].(i32)`）——**PB-61 快照**；2026-11-11 PB-67 复测实况是 `array dimension 1 ...` + `array dimension 2 ...` / `ir: unexpected token ']' in expression` | G1→放行 1 | 真因确为「指针类型后缀档」而非容量档（PB-67 的改记成立）。**两条边界未解、登记不修**：① `char[9]` 在 IR 仍是 9 个 8 字节槽（A 侧 9 字节紧凑），`.(char[9])[k]` 按槽宽 8 寻址才与 IR 自己的 `arry[k]` 自洽 ⇒ 归待裁第 23 条；② 两交叉档新暴露的**帧生长方向分歧**（见 §5） |
| `array_member_default` | `ir: unexpected token '[' in expression` | G3 | 数组元素默认值展开，需成员级真实偏移 |
| `struct_member_default` | `ir: member default value: only simple constant supported` | G3 | 成员默认值只落了「简单常量」那一半，PA-49 / PA-56 家族未回灌完。**2026-11-18（PB-79）已解**：默认值表 + 无初值/未点名两处回填 + 指定式与浮点读值，入 `IR_SUBSET` |
| `designator_init` | `ir: unexpected token '.' in expression`（198 条级联） | **G3**（PB-70 改归，原记 G1） | `.field = v` 指定式初值：**解析层缺口只是表象**，要落对必须知道成员真实偏移（A 侧靠 PA-49 / PA-56 的 C 指定初始化器），故根因是布局。**2026-11-18（PB-79）已解**：槽模型下按成员槽偏移落位即可（不需要字节偏移），入 `IR_SUBSET` |
| ~~`features`~~ | ~~`ir: unsupported top-level token 'alias'`~~ | G1→放行 | **PB-66 已解**：顶层 `alias` + 顶层 `const` / `var` + `for` 声明式 init + `else if` 链 + struct 数组成员齐活，四档输出逐字相同，已入 `IR_SUBSET` |
| ~~`mathmod` / `transfer`~~ | ~~`unsupported top-level token 'const'` / `'flow'`~~ | G1→放行 | **PB-66 已解**：顶层变量声明（含带可见性前缀的 `flow`）落成 `IrProg` 全局池，`transfer` 的 `flow create() void` 顶层函数声明一并放行 |
| ~~`for_init_forms`~~ | ~~`ir: unexpected top-level identifier 'count'`~~ | G1→放行 | **PB-66 已解**：顶层变量 + `for` 的三种 init 形态（`i i32 = 0` / `i = 0` / `i (i32) = 0`）+ 任意表达式 step（`i++` / `i--` / `j = j - 1` / `tick()`）全部放行，step 改为 body 之后执行 |
| ~~`module_top_forms`~~ | ~~bracket attributes 只对 `func` 生效（got `const`）~~ | G1→放行 | **PB-66 已解**：属性表落到 `const` 声明上（`[[export ".mydata"]]`），模块型用例四档都出 `.c` 产物 |
| ~~`is_pattern_forms`~~ | ~~`ir: bad 'is' pattern`~~ | G1→放行 | **PB-70 已解**：`is` 模式的整数字面量可带前缀符号、闭区间两端都可带（`is +5` / `is -9..-5`），四档输出与 `.expect` 逐字节相同，已入 `IR_SUBSET`。**首匹配即止不成问题**——本份每条 `is` 子句都带 `break`，故 IR 无 `__is_matched` 也能给出正确结果（与 `is_no_fallthrough` 的区别正在此） |
| `ownerof` | `ir: unexpected token '.' in expression`（`&boy.name`）+ `unexpected token 'structof'` / `expected '{', got '('` | **G3**（PB-70 改归，原记 G1「`.()` 后缀链形态」） | **原归类被第一手复测推翻**：真卡点是 `structof` / `holdof` / `unionof` / `bitoffsetof` 四个从属查询内置 + `&聚合体.成员`，全都要成员真实偏移（PB-33 的 P2 当年就明确让 IR 保持拒绝）。与后缀链无关 |
| `slice_str_infer` | `ir: member 'name' element 8 lies beyond the IR 8-byte slot`（19:27）+ `slice assignment expects { v0, v1, ... }` + `'.()' dereference takes a scalar type or nothing, not 'Say'` | **G3**（PB-70 改归，原记 G1） | 首错就是**数组成员越首槽的专属诊断**（PB-66 立的那道护栏），另卡聚合体 `.(Say)`。不是推断语法缺口 |
| ~~`stmt_terminators`~~ | ~~`ir: unsupported top-level token ';'`~~（11 条） | G1→放行 | **PB-70 已解**：语句尾终止符 + 空语句 + 顶层空声明 + `return;`，四档输出与 `.expect` 逐字节相同，已入 `IR_SUBSET`。**边界**：本份的 `Cfg struct { a i32 b i32 }` 在 IR 只占 1 槽，产物 C 里 `p.b = 2` 写的是**无关临时槽 `&t1`**，输出恰好为 3 只因 t1 未被复用 ⇒ 进名单是因为输出相同，不是布局已定案（第 23 条） |
| `type_forms` | 改前 `ir: unexpected token 'i32' in expression`（括号类型）；**PB-70 后能编能跑**，四条断言里 `arrow typed` 仍打 `bad` | **G3**（PB-70 改归，原记 G1） | 括号类型 `(i32)` / `(i32)[3]`、一元 `+`、`q.()` 三处 PB-70 已补齐；**剩下的唯一卡点是 `r->y` / `pt.y`**——聚合体第二成员越出 IR 的 1 个 8 字节槽 ⇒ 第 23 条。**刻意不入名单**（入去就是当场红）。**2026-11-18（PB-76）更新**：聚合体整块连续槽落地后 ir-c / ir-native 已打 `ok` 且与 `.expect` 相同；两交叉档仍 `bad`（帧方向，待裁第 33 条）⇒ 仍不入名单，闸门换成第 33 条。**2026-11-18（PB-81）已解**：第 33 条按 (c) provenance 结案后两交叉档转 `ok`，入 `IR_SUBSET` |
| `void_slot_infer` | `ir: 'q' is not a pointer to aggregate (assign &var first)` + `unexpected token '->'`（28:8 / 29:8） | **G3**（PB-70 改归，原记 G1） | 真卡点是 `q void = &pt`（pt 是聚合体）的**聚合体指针跟踪**与 `q->x` / `q->y`；`q->y` 是第二成员、越出 1 槽。裸 `.()` / 空下标 `[]` 那几组 PB-68 后已能过 |

**桶分布**：G1 **14**、G2 **5**、G3 **2**（G4 在 pos 侧本轮为 0——两份「看似可提升」的 `alignof_builtin` 与
`is_no_fallthrough` 实测四档输出不同，**不可提升**；这也复核了 PB-56 当时「可提升 0 份」的结论仍然成立）。
**PB-67 追加**：G1 由 16 收到 **14**——`len_builtin` / `array_capacity_tiers` 两份放行；`deref_slice` 留在 G1 但
**改记到「指针类型后缀档」一簇**（本轮第一手复测的结论，见下）。
**PB-68（2026-11-12）追加**：G1 由 14 收到 **13**——`deref_slice` 放行（指针类型后缀档整簇收完），
pos 侧合计 **20** 份 = G1 13 + G2 5 + G3 2。
**2026-11-12（PB-69）更正——以上两段的数都作废，按本段读**：pos 侧合计 **15** 份 = **G1 8 + G2 5 + G3 2**。
逐份点名（与 §0 的名单 ∩ 文件判据一致）：
- **G1 语法缺失 8 份**：`designator_init`、`is_pattern_forms`、`literal_forms`、`ownerof`、`slice_str_infer`、`stmt_terminators`、`type_forms`、`void_slot_infer`
- **G2 语义缺失 5 份**：`alignof_builtin`、`is_no_fallthrough`、`multi_arr_decl`、`str_array_init`、`use_mod`
- **G3 布局缺失 2 份**：`array_member_default`、`struct_member_default`

G4 在 pos 侧仍为 0（`alignof_builtin` 与 `is_no_fallthrough` 四档输出不同，**不可提升**，PB-56 的结论继续成立）。
**2026-11-12（PB-70）再更正——pos 侧合计 12 份 = G1 **0** + G2 5 + G3 **7****：
本轮放行 3 份（`is_pattern_forms` / `stmt_terminators` / `literal_forms`），另把 5 份**改归 G3**
（`designator_init` / `ownerof` / `void_slot_infer` / `slice_str_infer` / `type_forms`——第一手复测证明
它们的根因都是聚合体真实布局，原记 G1 有误，逐份证据见上表）。
- **G1 语法缺失 0 份——纯语法缺口桶本轮清空**。这是 P2 的一个里程碑：`irparse.c` 能自决的那一类活干完了。
- **G2 语义缺失 5 份**（不变）：`alignof_builtin`、`is_no_fallthrough`、`multi_arr_decl`、`str_array_init`、`use_mod`
- **G3 布局缺失 7 份**：`array_member_default`、`struct_member_default`、`designator_init`、`ownerof`、`slice_str_infer`、`type_forms`、`void_slot_infer`
**（PB-71 后 `is_no_fallthrough` 已放行，本段「12 份」按 11 份读：G2 由 5 收到 4。）**
**⇒ 剩下 12 份里没有一份能靠「补语法」解决**：7 份直接挂待裁第 23 条，另 5 份 G2 里 `multi_arr_decl` /
`str_array_init` / `alignof_builtin` 三份也与第 23 条同源（合计 **10 份**），只有 `is_no_fallthrough`
（`__is_matched` 首匹配即止）与 `use_mod`（跨模块符号）是独立可做的。**下一手要么等第 23 条裁定，
要么先做这两份独立的 G2。**

## 3. err 缺口 9 份（IR 侧不参与或参与了但判据不同源）

> **2026-11-12（PB-69）更正**：本节标题原写「15 份」，那是 **PB-62 之前**的值；PB-62 收掉 4 份 E-c 后应为 **11**，标题当时漏改，于是与它自己的子类计数（3 + 6 + 2 = 11）自相矛盾了两天。实测 `IR_ERR_COVERED` 22 条 / err 33 份 ⇒ 缺口 **11**，与门禁 ir-c 的 11 条 `[SKIP] err/` 吻合。逐份：`cooking_bare_var`、`is_dangling_range`、`is_empty_range`（E-a 3）；`deref_bounds`、`flow_move_frozen`、`structof_bad_member`、`void_member_unknown`、`slice_str_overflow`、`default_init_overflow`（E-b 6）；`len_unknown`、`void_subscript`（E-c 2）。

| 子类 | 份数 | 用例 | 说明 |
|---|---|---|---|
| **E-a 已诊断、文案不同源** | 3 | `cooking_bare_var` `is_dangling_range` `is_empty_range` | IR 有等价拒绝，只是句子与共用 `.expect` 不同源；已在待裁第 26 条（READY / PARTIAL / COLLIDES 三批清单）里 |
| **E-b IR 在更外层就拒，够不到语义诊断** | 6 | `deref_bounds` `flow_move_frozen` `structof_bad_member` `void_member_unknown` `slice_str_overflow` `default_init_overflow` | 全都断在解析层（`expected ')', got 'i64'` / `unexpected token '.'`），**修复依赖本表第 2 节的 G1**，不是独立工作 |
| **E-c IR 完全不诊断** | ~~6~~ → ~~2~~ → **0**（PB-62 收 4 份，**PB-72 收余下 2 份**） | 全部已收：`str_array_bad_elem` `str_array_overflow` `multi_arr_nostr` `multi_arr_overflow`（PB-62）+ **`len_unknown`**（新增 `vlen_known` 表，与 A 侧 `Symbol.len_known` 同判据）、**`void_subscript`**（`ir_check_void_subscript`，与 A 侧同判据同文案；两条例外：空下标 `p[]` 合法、PB-68 的 `VM_PTRARR` 可下标）。六份现在都是「有诊断 + 无产物」且已入 `IR_ERR_COVERED` ⇒ **E-c 桶清空** |

更新（2026-11-10，PB-62）：**E-c 桶已由 6 份收到 2 份**——`str_array_bad_elem` / `str_array_overflow` / `multi_arr_nostr` / `multi_arr_overflow` 四份在 IR 前端补上与 A 同口径的三条诊断后转为「有诊断 + 无产物」，已入 `IR_ERR_COVERED`；门禁 ir-c / ir-native 由 28 份 PASS 升到 32 份（0 FAIL，c / native 与比对档不变，交叉矩阵 48/48 不变）。两份余量的卡点见本行右侧。
更新（2026-11-11，PB-64 / PB-65）：**`print` 已在 IR 下降到 printf**，`flow` 四份全部放行——`flow` / `flow_no_free` / `borrow` 入 `IR_SUBSET` 并计 PASS，`flow_move` 入名单升比对档；`features` 现在只剩顶层 `alias` 一挡。同轮修掉**字符串池序列化不转义**的缺陷（含换行的串在 ir-c 产物里是非法 C、在交叉 `.s` 里劈成两行只靠汇编器容错），修好后交叉的 `unterminated string` 告警由 4 降到 0。`str_array_init` 由「出码语义坏」变成「能编能跑但输出截断」（IR `x/9/A` vs A `xiaoming/9/ABCDEFGH`），归第 23 条家族，**不入名单**。
更新（2026-11-11，PB-66）：**顶层声明与 `alias` 那一簇 5 份全部放行**——`features` / `for_init_forms` / `transfer` / `mathmod` / `module_top_forms` 四档实跑输出逐字相同（三份带 `.expect` 计入 PASS，两份模块型只出 `.c` 产物），已入 `IR_SUBSET`；门禁 ir-c / ir-native 由 35 升到 **40 PASS**、SKIP 34 → **29**、0 FAIL，c / native 68P/0F/3S 不变，比对档 34 + 2 不变。pos 侧缺口 28 → **23** 份（G1 21 → 16）。**边界照旧诚实**：`char[32]` 这类数组成员在 IR 侧仍只占 1 个 8 字节槽（真实布局仍是待裁第 23 条），`features` 恰巧不回读该成员才逐字相同——它进名单是因为输出相同，不是因为布局已定案；越出首槽的元素写给专属诊断。
## 4. 从这张表读出的排期结论

1. **单点最大的是「真实布局」**：PB-61 时 G3 的 2 份 + E-c 的 6 份 + G2 的 3 份（多维读写、字符串数组、`alignof`）
   共 **11 份**挂在同一条欠账上（待裁第 23 条）；PB-62 收掉 4 份 E-c 后为 **7 份**。它不解，IR 线永远补不到 A 线的 68 份。
2. **G1 的 13 份聚成 2 簇**，不是 13 件事（**2026-11-12 PB-69 更正：G1 实为 8 份、聚成 3 簇，见 §6；本条原文保留**）：**`p.(T)` 类型解引用（PB-63 已做：1 份当场放行、4 份改卡 `print`）**、
   **`print` 下降（PB-64 已做，四份全放行）**、**顶层声明与 `alias`（PB-66 已做，5 份全放行）**、
   **数组 / 切片容量档（PB-67 已做：`len_builtin` / `array_capacity_tiers` 两份全放行）**、
   **指针类型后缀档（PB-68 已做：`deref_slice` 放行，整簇收完）**、表达式细节与模式（7 份）。按簇排，一簇一轮。
   ——PB-67 的第一手复测还纠正了台账旧分类：`deref_slice` 此前记在「数组 / 切片容量档」簇，
   实测其真因是**指针类型后缀档**（`void[][]` 逐层解引用、`.(char[9])` 数组型后缀、`void[2]` 指针数组、
   `malloc(void[3])`、`.(i32) += 1` 落在指针数组元素上），与容量档无关。
3. **「指针类型后缀档」PB-68 已收完**，做法与预判不同：**没有等真实布局**——`void[N] = malloc(void[N])`
   按 A 侧同形态退化成「标量指针槽 + 指针值寻址」（A 出的是 `void*(*p) = malloc(N*sizeof(void*))`），
   `void[2] = {&a, &b}` 仍是帧数组，两者都不需要聚合体真实布局。新增后缀链求值器只在三种情况下接管
   （根变量是 malloc 退化的指针值数组 / 链含新形态 / 链有两组以上后缀），单组旧形态一律走原路径。
   **仍挂第 23 条的只剩布局口径**：IR 的 `char[9]` 是 9 个 8 字节槽而 A 是 9 字节紧凑，
   `.(char[9])[k]` 只能按槽宽 8 寻址才与 IR 自己的 `arry[k]` 自洽——`deref_slice` 进名单是因为四档输出相同，
   不是因为布局已定案。**另新暴露一条交叉档缺陷（§5），带设计权重，登记不修。**
4. **E-c 那 6 份应当优先于「补齐语法」**（PB-62 已照此收 4 份，余 2 份）：它们是「编译器不报错」而不是「编译器不支持」，属于会往外漏错程序的一类；
   最小动作是先给专属诊断（形状同 PA-52 / PA-59 那两轮），真实布局到位后再换成精确检查。
5. **台账更新方式**：每轮 P2 收一簇后重跑 `build/p61/promotable.sh` 与本节计数，把改完的行从表里删掉——本文件只存
   「还欠什么」，不存历史（历史归 `CHANGELOG.md` 与 `docs/IMPLEMENTATION_STATUS.md`）。
   **2026-11-12（PB-69）把这条口径补硬**：收簇之后**必须做三方对账**——① 名单条目 ∩ `tests/` 真实文件；
   ② 补集大小 vs 本文件标题与桶分布；③ `xmake test -b ir-c` 的 `[PASS]` / `[SKIP]` 逐类计数 vs 前两者。
   三者对不上就是账错了，**不许只在上一轮数字上加增量**（PB-66 起的恒偏 5 就是这么攒出来的）。
   同时**逐行核对成员**，不只数行数：`borrow` 过期行与 `literal_forms` 漏行一出一入，行数恰好对上而成员是错的。

## 5. PB-68 新暴露：帧生长方向在 IR 两条 lowering 上不一致（登记不修，待裁）

**症状**：`deref_slice` 整份在两交叉档**出码即被帧护栏拒**（`main` 帧 3328 字节 > 2047 寻址限，属既有天花板）。
把八组形态拆成八个函数的探针（`ncc/build/p68x/probe.nc`）能出码、GNU as 与 zig 链接 **0 告警**，
但八组里**三组算错地址**（切片读入 / 数组指针数组 / 指针数组的指针），riscv64 与 loongarch64 **同症状**。

**隔离**（`ncc/build/p68x/iso.nc`，五件对照）把范围收窄到「新形态 + 非零下标」：

| 探针 | 形态 | ir-c | rv | la |
|---|---|---|---|---|
| A | 旧形态切片读入 `buf char[8] = arry[0..7]` | ok | ok | ok |
| B | 旧形态切片读入（`i32` 元素对照） | ok | ok | ok |
| C | 新形态 `p.(char[9])[8] = 42` | ok | **bad** | **bad** |
| D | 新形态 `buf char[8] = p.(char[9])[0..7]` | ok | **bad** | **bad** |
| E | `void[2]` 帧数组取指针再 `.(char[9])[8]` | ok | **bad** | **bad** |

**静态根因**（同一个 `arry[8]`，两条路算出相差 128 字节的两个地址）：
写侧 `p.(char[9])[8]` 出 `mul` + `add` ⇒ `base + 8*8` = `s0+56`；
读侧 `arry[8]` 出 `slli` + `sub` ⇒ `base - 8*8` = `s0-72`。
落点：`ir_riscv64.c` 与 `ir_loongarch64.c` 的 `IR_ADDR` / `IR_ELEM_ADDR` 注释明写「槽向下生长」，
而 `ir_to_c.c` 的 `IR_ADDR` 出 `&vrid[a][imm]`（C 数组，**向上**）。

**为什么此前没暴露**：此前 `ir_elem_addr(..., is_ptr=1)` 的寻址只指向**堆**（`malloc`）或**字符串池**（全局），
两者都向上，与 `+idx*width` 一致；PB-68 的新形态第一次把**帧地址**装进指针值再做下标运算，方向分歧才显形。
帧数组自身的读写两侧都走 `IR_ADDR`，自洽，所以既有用例全绿。

**归属与处置**：这是两档发射器与 `ir_to_c` 之间的**方向口径分歧**，改哪一侧都要动整条交叉出码面
（且与待裁第 23 条「槽宽与真实布局」同源），**带设计权重 ⇒ 登记不修，交 ltree 裁**。本轮未改任何发射器一行。

**零交叉回归（实测对照）**：全语料交叉扫 `tests/pos/*.nc` —— 58 份出 ir-c 参照，两档各 **56 份链接成功**
（各 2 份帧护栏拒），各 **54 份 AGREE**、**2 份 CRASH_139**（`ir_str` / `str_array_init`）。
把这 2 份用 **HEAD 版 `irparse.c`**（PB-68 之前）重新出码、链接、实跑，**同样 CRASH_139**
⇒ 既有缺陷，非本轮引入。扫脚本与产物在 `ncc/build/p68x/`（gitignore）。

**（2026-11-18 结案，PB-81）**：ltree 裁 **选 (c) provenance**——见 §15。前端变量表 `vfrm[]` 区分「帧地址来源」
与「堆/池来源」，成员偏移对帧来源改用方向感知的 `IR_ELEM_ADDR`；两发射器帧布局与既有交叉基线不动。
本节隐含的 (a)/(b) 两档随之作废。

## 6. 计数更正后的排期读数（2026-11-12，PB-69）

§4 第 1 / 2 条的**结论不变、数字更新**：

- **第 1 条（真实布局是单点最大）**：仍成立。今天挂在待裁第 23 条上的是 **G3 2 份 + E-c 2 份 + G2 3 份**
  （`multi_arr_decl` / `str_array_init` / `alignof_builtin`）= **7 份**，与 PB-62 之后的读数一致。
- **第 2 条（G1 聚簇）**：G1 由「13 份」更正为 **8 份**，且**已收的五簇不再计入**。剩下的 8 份聚成 **3 簇**：
  **① 表达式细节 4 份**（`literal_forms` 一元 `+` 与布尔字面量、`type_forms` 类型出现在表达式位置、
  `ownerof` 的 `.()` 后缀链形态、`void_slot_infer` 的 `void` 指针下标推断）——`ownerof` / `void_slot_infer`
  与 PB-68 刚落地的后缀链求值器同族，**可能是最便宜的一簇，动手前先复测**；
  **② 声明与终止符 2 份**（`designator_init` 指定式初值、`stmt_terminators` 顶层 `;` / `#`）；
  **③ 模式与推断 2 份**（`is_pattern_forms` 带符号区间、`slice_str_infer` 切片 / 字符串指针推断）。
- **err 侧**：E-b 那 6 份的修复依赖 G1，不是独立工作；E-a 3 份属待裁第 26 条的分批清单；E-c 2 份要 `len_known` 表
  与指针指向类型跟踪。**err 缺口 11 份里没有一份可以靠改名单解决**。
- **与路线图的对账**：`docs/VERSIONING_ROADMAP.md` §5.2 那张代价表是 **2026-11-09 的快照**（pos 44 能编 / 28 缺口、
  err 7 覆盖 / 26 缺口），按「历史快照不改写」口径**原文保留**；今天的现值以本文件为准（pos 57 / 15、err 22 / 11）。
  §5.3 里 P2 的退出判据「pos IR 可编 44 → 68」按现值读作 **57 → 69**（69 = 72 − 3 份 `IR_ONLY`，那 3 份是 A 后端
  本就拒的非法用例，不该算进 IR 的追平目标）——**这条口径变更需要 ltree 确认，本轮只登记不擅改判据**。

## 7. PB-70 实测：G1 桶清空，剩下的活全在语义与布局（2026-11-12）

**本轮放行 3 份**（`is_pattern_forms` / `stmt_terminators` / `literal_forms`），**改归 5 份到 G3**，
落地七处形态（`is` 带符号模式与带符号区间、语句尾终止符与空语句与顶层空声明、`return;`、
一元 `+` 字面量前缀、布尔字面量、括号类型 `(T)` / `(T)[N]`、声明型 `string` 的 `len()`）。

**最有决策价值的一条：G1 语法缺失桶清空了。** 台账原把 8 份挂在 G1，第一手复测后只剩 3 份真是纯语法，
另 5 份（`designator_init` / `ownerof` / `void_slot_infer` / `slice_str_infer` / `type_forms`）的根因
都是**聚合体真实布局**。⇒ **`irparse.c` 能自决的那一类活干完了**，剩下 12 份里：

| 类别 | 份数 | 用例 | 能不能自决 |
|---|---|---|---|
| 直接挂待裁第 23 条（布局） | 7 | `array_member_default` `struct_member_default` `designator_init` `ownerof` `slice_str_infer` `type_forms` `void_slot_infer` | **不能**，等裁定 |
| G2 但与第 23 条同源 | 3 | `multi_arr_decl` `str_array_init` `alignof_builtin` | **不能**，等裁定 |
| G2 独立可做 | 2 | `is_no_fallthrough`（`__is_matched` 首匹配即止）、`use_mod`（跨模块符号） | **能** |

**⇒ 下一手只有两条路**：要么 ltree 先裁第 23 条（一次解开 10 份），要么先做那 2 份独立的 G2。
err 侧 11 份不变，其中 E-b 6 份的修复依赖 pos 侧，E-a 3 份属第 26 条分批清单，E-c 2 份要 `len_known` 表
与指针指向类型跟踪。

**两条本轮量到的诚实边界**：
1. **`stmt_terminators` 的聚合体成员写落在无关临时槽**：`Cfg struct { a i32 b i32 }` 在 IR 只占 1 个 8 字节槽，
   产物 C 里 `p.a = 1` 写 `&p`、`p.b = 2` 写 **`&t1`**（一个临时 vreg 槽）。输出恰好为 3 只因 `t1` 在被读回前
   没被复用。这比 PB-48 记的「落在相邻槽」更尖锐——**落点是任意临时槽**。本份进名单是因为四档输出相同，
   不是因为布局已定案；成员寻址代码本轮一行未改。
2. **`type_forms` 交叉档记 AGREE 但输出是错的**：它四条断言里 `arrow typed` 打 `bad`，而交叉参照用的是
   **ir-c**（同一个 IR 前端），所以两侧同错 ⇒ AGREE。**「AGREE 只代表输出相同」在这里又一次应验**，
   故本份刻意不入名单。

**交叉零回归（与 PB-68 基线逐条对账）**：ir-c 参照出码 **58 → 62**，两档各链接成功 **56 → 60**
（各 2 份仍被帧护栏拒：`deref_slice` 3328、`ir_slice` 2208 > 2047），qemu 实跑各 **54 → 58 AGREE**、
**2 CRASH_139 不变**（`ir_str` / `str_array_init`，PB-68 已用 HEAD 版 `irparse.c` 证明是先存缺陷）。
`comm` 对账：**新增 8 行 AGREE（4 份 × 2 档）、消失 0 行** ⇒ 零回归。扫脚本与产物在 `ncc/build/p70x/`（gitignore）。

## 8. PB-71 / PB-72 实测：首匹配即止 + E-c 桶清空（2026-11-12）

**三项全部是「追平 A 的既有语义」，无新设计**：

| 项 | A 侧机制 | IR 侧落地 | 结果 |
|---|---|---|---|
| `is_no_fallthrough` | `__is_matched`（PA-16 / P4）：while 每轮重置、每条 `is` 守卫 `!__is_matched && (…) && (__is_matched = 1)` | `is_matched_vreg`：每轮在条件检查后 / body 前 `MOV 0`；子句开头 `IR_JNZ` 跳过、匹配后进块前 `MOV 1` | IR 由 14 行降到 **7 行**，四档与 `.expect` 逐字节相同 |
| `len_unknown` | `Symbol.len_known`，`len(x)` 对未登记者即时报错 | 新增 `vlen_known` 表（**不能用 `ve>0` 判**：0 既是标量也是空动态字符串），只在数组声明 / 字符串字面量初值 / **常量边界**切片处登记 | 由「静默给 0 且产出可执行文件」转为**有诊断 + 无产物** |
| `void_subscript` | 非空下标落在 `TYPE_VOID` 变量上即拒 | `ir_check_void_subscript`，同判据同文案；**两条例外**：空下标 `p[]` 是省略类型解引用（合法）、PB-68 的 `VM_PTRARR`（`ve[]` 为 0 但可下标） | 同上 |

**A 侧置位放在守卫条件里、IR 放在条件跳转之后**——IR 没有短路表达式，故改为「条件跳转 → MOV 1 → 块体」，
语义等价（能走到 MOV 就说明模式已匹配）。这是移植时唯一需要改形的地方。

**E-c 桶清空的意义**：PB-61 的排期结论把「编译器不报错」排在「编译器不支持」之前，理由是前者会**往外漏错程序**。
六份现在全部有专属诊断且无产物，这一类风险在 IR 线上归零。

**剩余缺口（pos 11 / err 9）**：
- pos：**G3 布局 7 份** + **G2 4 份**（`alignof_builtin` / `multi_arr_decl` / `str_array_init` / `use_mod`）。
  其中 **10 份挂待裁第 23 条**，只有 **`use_mod`** 独立可做——它需要 **IR 侧的模块加载**：
  A 侧 `module_import` 用「保存 lexer / 上下文 → 为模块源码建新 lexer → 递归 `parse_module` → 还原」
  把模块函数内联进同一份产物；而 `ir_parse_file` **每次调用都新建 `P = ir_prog_new()`**，是真正的单文件模型。
  落地要把顶层声明循环抽成可重入的助手 + 模块路径解析。**注意 `module_import` 内部调的是 A 的 `parse_module`，
  IR 不能直接复用它**；路径解析要么在 `irparse.c` 内自带一份，要么动共享的 `module.c`——**这一步的取舍另立一轮**。
- err：**E-a 3 份**（文案不同源，属待裁第 26 条的分批清单）+ **E-b 6 份**（断在解析层，修复依赖 pos 侧的 G3）。
  **E-c 0 份。**

## 9. PB-73 实测：IR 侧模块加载（use_mod 放行，追平撞墙在待裁第 23 条）

**落地**：`ir_parse_file` 的顶层声明循环抽成可重入的 `ir_parse_top_level`；`use <name>` 由 `ir_use_decl`
累积点号多段名后触发 `ir_use_module`——按 **cwd / 源文件目录 / stdlib** 三层解析模块文件（点号名按目录
层级，`a.b` → `a/b.nc`），**保存 / 还原 lexer** 后把模块顶层声明内联进**同一个 `IrProg`**（与 A 侧
`module_import` 同语义），循环 import 由 `ir_imported` 去重名单挡。`use_mod` 四档输出与 `.expect` 逐字节
相同（`7` + `use ok`），已入 `IR_SUBSET`。错误定位落在模块自己的文件名与行号上（每次递归换来新的
`LexerState filename`）。

**为什么不能直接复用 A 的 `module_import`**：它内部递归调用的是 **A 前端的 `parse_module`**，会往 A 的
符号表与 cgen 缓冲里写，IR 前端一调用就直接串线。所以路径解析在 `irparse.c` 内自带一份（约 40 行，与
`module.c` 同口径），**不触碰共享的 `module.c`**——这也是「PB 只改 IR 线」的一贯做法。

**验证**：Windows c/native 68P/3S 不变、ir **50P/19S**、0 FAIL；Linux 68P/4S + **50P/20S**、0 FAIL；
`mathmod` / `module_top_forms` 两份既有模块型用例 PASS 不变（无回归）。三方对账：`IR_SUBSET` 59 /
`IR_ONLY` 3 / `IR_ERR_COVERED` 24 无悬空，补集 pos **10** / err 9。

**⇒ 追平到这里撞墙了**。剩余 pos 缺口 10 份**全部**挂待裁第 23 条（槽宽 vs 真实布局）：
- G3 布局 7 份：`array_member_default` / `struct_member_default` / `designator_init` / `ownerof` /
  `slice_str_infer` / `type_forms` / `void_slot_infer`
- G2 但与第 23 条同源 3 份：`multi_arr_decl` / `str_array_init` / `alignof_builtin`

err 9 份：E-a 3（文案不同源，属待裁第 26 条的分批清单）+ E-b 6（断在解析层，**修复依赖 pos 侧的 G3**）。
**`irparse.c` 一块不需要裁定的活都不剩了**——G1 语法桶（PB-70 清空）、E-c 诊断桶（PB-72 清空）、
独立的 G2（PB-71 收 `is_no_fallthrough`、PB-73 收 `use_mod`）全部做完。

**下一步只有待裁第 23 条一条路**，且它同时是 §6 里 P2 退出判据重述（待裁第 34 条）与 §5 帧方向分歧
（待裁第 33 条）的共同前提——三个待裁项在这条上会师。

## 10. PB-74：ltree 裁 (b)—— 写宽/布局降到元素宽度，改造分两阶段

**裁定（2026-11-12）**：待裁第 23 条三口径里选 **(b)**：写宽降到元素宽度（`int32_t` / 窄 STORE），
**放弃「一切皆 8 字节槽」**这条简化。

**现状（勘测实据）**：8 字节槽渗透到整条 IR 路线——`vreg` 只有 int/double 两型、无字节宽；
`LOAD`/`STORE` 恒 8 字节、窄型只有 `LOAD8`/`STORE8`（1 字节）；产物 C 全线 `int64_t`
（连 `i32` 的存储类型都是 `int64_t`，窄型靠 `IR_TRUNC` 在值域截断）；聚合体只有 1 槽、
成员无真实偏移；两发射器 `slot_stride = 8`。

**分两阶段**：

| 阶段 | 内容 | 解开用例 |
|---|---|---|
| **A 窄标量** | vreg 加字节宽表（ir.h 的 `IrFn` 增 `vreg_size`/`vreg_sign`）、窄型 LOAD/STORE、`ir_to_c` 发 `int32_t` 等、两发射器变宽帧槽 | 修 PB-48 D1（`malloc(i32)` 堆越界 8 字节写）；为阶段 B 打地基 |
| **B 聚合/数组真实布局** | 成员按真实字节偏移（`moff` 改字节偏移，按 `msize`/`marr_esize`/嵌套递归累加）、数组元素真实宽、`p.y` 发 `IR_ADDR(base, 字节偏移)` | `type_forms` / `void_slot_infer` / `struct_member_default` / `array_member_default` / `designator_init` / `ownerof`（四内置）/ `slice_str_infer` |

**阶段 A 的第一处落地已在本轮做掉**：`IrAggType` 新增标量成员字节宽 `msize[]`（此前只记数组成员宽），
`alignof` 的真实对齐折叠（标量=宽、聚合=成员最大对齐递归、enum=4、数组=元素对齐）就是靠它——
这是「成员真实宽度」信息进入前端的第一处；`alignof_builtin` 已放行。
**PB-75（2026-11-12）阶段 A 的第二刀已落**：`IR_LOAD` / `IR_STORE` 的 imm 承载字节宽（0=8 兼容；
±w=窄读、正=零扩展、负=符号扩展；STORE 正 w 写低 w 字节），前端 `ir_load_width_imm` / `ir_store_width` +
全局 `last_malloc_sc`（`malloc(T)` 参数类型码的通道——**此前 `p void = malloc(i32)` 根本不记指向类型**），
`.()`/`.(T)` 读、`.()=`/`op=` 写、复合 RMW 三处按指向类型码发窄读写；两发射器补窄 ld/st。
**修掉 PB-48 D1 堆越界**（ASan 阴性对照：`ir_builtin` 产物 0 report，`*(int64_t*)` → `*(uint32_t*)` 写 +
`*(int32_t*)` 读），门禁 / Linux / 交叉**零回归**。范围刻意收窄：只动**指针值寻址（堆）**，帧数组元素
仍走 8 字节槽（自洽、不动避免回归）。聚合体真实字节偏移 / 数组成员真实宽是阶段 B（下一刀）。

**验收口径**：每阶段全门禁 + Linux + 交叉 `comm` 零回归；阶段 B 收尾后把这 9 份一次性入名单，
并用 ASan（宿主档）+ 布局探针（交叉档）验证 D1/D2 越界清零。

## 11. PB-76 实测：阶段 B 第一刀——聚合体整块连续槽（2026-11-18）

**先复测（探针 `build/pb76a.nc`，ir-c）**：改前输出 `direct 11 22 / after ptr 33 22 / via ptr 33 44 / mix 55 44`
——`r->y = 44` 只改到指针路线的读回，直接访问 `p.y` 仍是 22。**根因**：ir-c 里聚合成员是互不相邻的
C 标量（`int64_t p; int64_t t1;`），`&p + 8` 落到两者之间的栈空隙；直接访问 `p.y` 取的是 `&t1`。
即 §10 阶段 B 的**前置缺口**——存储不连续，字节偏移再准也没用。

**落地（不新增成员字节偏移，先做连续性）**：

| 位置 | 改动 |
|---|---|
| `ncc/ir.h` | 新增 `IR_AGG_ALLOCA`（紧邻 `IR_ALLOCA`） |
| `ncc/irparse.c` | `var_declare` 对聚合体只由基槽发一条 `IR_AGG_ALLOCA`（`imm=8*mslots`），成员 vreg 仍按 `base+k` 分配；sret 返回 / `_mr_ret` 缓冲 / 多变量聚合声明的槽数由 `mcount` 改 `mslots`；verbose dump 表补一格 |
| `ncc/ir_backend.c` | 指令循环与 `IR_ALLOCA` 一样跳过（两发射器零改动） |
| `ncc/ir_to_c.c` | 聚合块声明为 `int64_t name[slots]`；成员 vreg 标识符映射为 `name[k]`（不单独声明）；`IR_ADDR` 用存储名取址（`&name[imm]`） |

**验证**：
- 探针四项全对（`after ptr 33 44 / via ptr 33 44 / mix 55 55`）；`type_forms` 的 `arrow typed` 在 ir-c / ir-native 变 `ok`。
- ASan（WSL gcc `-fsanitize=address -O1`）：探针与 `ir_arrow` 产物 **0 report** ⇒ PB-48 D2 的栈越界结构性消除。
- **零回归硬证据**：用 `git archive HEAD` 建改动前编译器，全 pos 语料 × `ir-riscv64` / `ir-loongarch64` 共 **126 份 `.s` 逐字节相同**（两发射器确实零变化）；ir-c 产物只有 **10 份聚合相关文件**变化，其中 8 份可执行用例新旧输出逐字相同。门禁 **52P/0F/18S、0 FAIL**（与改前一致）。
- **交叉实跑**：`type_forms` 在 riscv64 / loongarch64 上仍打 `arrow typed bad`。原因**不是**本轮引入——汇编帧槽向下生长，`&p + 8` 向高地址走、离开成员槽；此前该用例在交叉档记 `AGREE`，是「指针写 + 指针读」自洽造成的**假一致**（对照：`ir_arrow` 至今仍 AGREE，因为它的断言也只走指针）。

**⇒ 对缺口表的影响**：`type_forms` 的 ir-c 侧已修好，但交叉档不一致使其**仍不入名单**（待裁第 33 条现为唯一闸门，见 `TODO-PB.md` 待裁 33 的 2026-11-18 补注）。**名单与计数不变**：`IR_SUBSET` 60 / `IR_ONLY` 3 ⇒ pos 可编 **63 / 72、缺口 9**；err 覆盖 **25 / 34、缺口 9**（分母由 33 更正为 34，`deref_void_type` 加入后未同步）。

**仍属第 23 条后续（本轮明确未做）**：成员字节偏移（成员仍各占 8 字节槽，非字节紧凑）、
数组成员真实宽（仍 1 槽 + 首 8 字节可寻址）、`structof` / `holdof` / `unionof` / `bitoffsetof` 四内置、
指定式初值 `.field = v`、成员默认值展开。逐份剩余：`array_member_default` / `designator_init` /
`ownerof` / `slice_str_infer` / `str_array_init` / `struct_member_default` / `void_slot_infer`（7 份）
+ `type_forms`（等第 33 条）+ `multi_arr_decl`（多维读写路径，非布局）。

## 12. PB-78 实测：`void` 指针声明处的指向类型（2026-11-18）

**复测**：`void_slot_infer` 只在 `q void = &pt` 之后报 `'q' is not a pointer to aggregate (assign &var first)`
（`q->x`），其余七组本来就过。**根因**：`pt[]` 指向类型登记只在**赋值语句**分支（`p = &pt`），
**声明初始化**分支 `name type = &var` 漏了；`&` 本身由 `ir_expr` 一元分支正常发 `IR_ADDR`。

**修法**：声明初始化分支加前瞻——`&` 后是标识符、且其后不是 `.`/`[`（排除 `&agg.member`）时，
按赋值路径同口径登记 `pt[vi]`（`&聚合`→聚合索引，`&标量`→`PT_SCALAR(code)`）。不新增 IR 操作码。

**验证**：`void_slot_infer` 八行与 `.expect` 逐字节相同；门禁 52P/0F/18S ×2、0 FAIL 不变
（`deref_slice` 等既有声明型 void 指针零回归）；**交叉两档 7/8 行 ok，仅 `void arrow bad`**
——`q->y` 与 `type_forms` 同因（第 33 条帧方向），故两者继续留在名单外。
**计数不变**：`IR_SUBSET` 60 / `IR_ONLY` 3 ⇒ pos 63/72、缺口 9；err 25/34、缺口 9。

## 13. PB-79 实测：聚合成员默认值 + 指定式初值（2026-11-18）

**复测**：`struct_member_default` 卡在 `ratio f64 = 1.5`（默认值只认整型/标识符/字符串），
`designator_init` 卡在 `.field = v` 解析；更根本的是 IR **对默认值一直「解析后忽略」**，
能编也不会生效。

**落地**：
- `IrAggType` 补 `mdef_kind`（0 无 / 1 整型 / 2 浮点 / 3 字符串）、`mdef_int`、`mdef_dbl`、`mdef_str`；
  解析扩到整型（可负）/浮点/字符串字面量/枚举常量。
- 回填两处：**无初值声明**（`ir_agg_decl` else）与**初值列表未点名成员**（`ir_agg_init` 收尾）；
  union 只取首个默认成员、用户给过初值就不补（共享槽 0 会被覆盖）。
- 指定式 `.member = expr` 按成员名落位（`seen[]` 记账），可与默认值混用；嵌套聚合默认值递归回填。
- 连带：浮点成员**读值**补 double 标记；ir-c 的 `puts(变量)` 补 `(char*)(intptr_t)` cast
  （字符串变量槽里的池地址不是 `LD_ADDR` 直连，严格 clang 报 int→pointer 硬错误）。

**验证**：`struct_member_default` 七行、`designator_init` 五行与 `.expect` 逐字节相同；
两交叉档 zig 链接 + qemu 实跑同输出 ⇒ 两份入 `IR_SUBSET`；门禁 **54P/0F/16S、0 FAIL**
（PASS 52→54、SKIP 18→16），产物 C 严格档 0 error。

**计数**：`IR_SUBSET` **62** / `IR_ONLY` 3 ⇒ pos **65/72、缺口 7**；err **25/34、缺口 9**。
**剩余 7 份**：`type_forms` / `void_slot_infer`（交叉档待裁 33）、`array_member_default`（聚合数组）、
`ownerof`（四内置）、`slice_str_infer` / `str_array_init`（数组成员真实宽）、`multi_arr_decl`（char 数组成员字节布局）——
后五份均属第 23 条家族。

## 14. PB-80 实测：聚合体数组（2026-11-18）

**复测**：`a P[3]` 后 `a[0].x` 报 `unexpected token '[' in expression`；另两处次生卡点：`[...]`
（TOK_ELLIPSIS）不被聚合容量解析接受、`S union {…}` 写在 `main` 体内不被认（IR 只有顶层类型定义）。

**落地**：`ir_agg_decl` 支持 `[N]` / `[...]`（默认 8）/ 多维（总槽 = 元素数 × `mslots`，`vndim` 标形态、
`ve` 记元素数）；新增 `ir_agg_elem_read`（`flat*mslots` 经 `ELEM_ADDR` + 成员偏移，方向由后端处理）；
新增 `ir_aggarr_init`（内层花括号逐元素 `ir_agg_init` + 尾数默认；扁平写法按 C 省略规则写槽、剩余补 0）；
`ir_type_decl` 拆 `ir_type_decl_named` 供语句层复用。

**验证**：`array_member_default` 七行与 `.expect` 逐字节相同 ⇒ 入 `IR_SUBSET`；门禁 **55P/0F/15S、0 FAIL**。
**交叉边界**：本份 `main` 帧 3456 > 2047，两档出码即被帧护栏拒（既有上限，同 `deref_slice`）；
语义用缩小探针（`P[2]` 三组：默认值 / 部分位置初值 / 指定式）在两档 zig 链接 + qemu 实跑，
输出 `7 8 9 2 / 5 9 8 / 7 1 7 2` 与 ir-c 逐字相同。

**计数**：`IR_SUBSET` **63** / `IR_ONLY` 3 ⇒ pos **66/72、缺口 6**；err **25/34、缺口 9**。
**剩余 6 份**：`type_forms` / `void_slot_infer`（待裁 33）、`ownerof` / `slice_str_infer` /
`str_array_init` / `multi_arr_decl`（第 23 条字节布局家族）。**⇒ `irparse.c` 里不需要裁定的活再次清零。**

## 15. PB-81 实测：帧生长方向结案（第 33 条，ltree 裁 (c) provenance；2026-11-18）

**裁定**：不动两发射器的帧布局（会移动全部交叉 `.s` 基线），改为在 IR 前端区分指针来源。

**落地**：变量表 `vfrm[]`（1 = 指针值来自 `&帧变量`）。置位点：`&聚合体变量` 的赋值路径与声明路径
（PB-78 那块）、`r = s` 的指针拷贝传播；`malloc` / 字符串池 / 参数来源保持 0。新增
`ir_ptr_slot_off(vi, base, off)`：帧来源发 `IR_ELEM_ADDR`（idx=槽偏移、宽 8；汇编 `sub`、C `add`），
堆/池来源仍发字节 `ADD`。五处 `->` 成员偏移（链读、primary 读、语句复合读两处、语句写）全部改道。

**验证**：`type_forms` / `void_slot_infer` 两交叉档 qemu 实跑从 `bad` 变全 `ok` ⇒ 入 `IR_SUBSET`；
门禁 **57P/0F/13S、0 FAIL**。**交叉变化面**：全 pos × 两档 `.s` 对比，**仅 6 份聚合指针用例变化**
（`ir_arrow` / `ir_mr` / `ir_nested` / `ir_struct` / `struct_param_prefix` / `type_forms`），其余 120 份
逐字节不变；变化的 5 份可执行用例两档实跑全 `ok`。新增 6 份产物 = 此前编不过的 `designator_init` /
`struct_member_default` / `void_slot_infer`。

**计数**：`IR_SUBSET` **65** / `IR_ONLY` 3 ⇒ pos **68/72、缺口 4**；err **25/34、缺口 9**。
**剩余 4 份全部为第 23 条字节级布局家族**（`ownerof` / `slice_str_infer` / `str_array_init` /
`multi_arr_decl`），ltree 已对「解锁」表态（2026-11-18），下一轮起按字节级布局推进。

## 16. PB-82 实测：第 23 条第一刀——定长 char/i8/u8 数组字节紧凑（2026-11-18）

**复测**：`str_array_init` 改前打 `x/9/A`（每个字符占 8 字节槽，`puts` 只读首字符）；
`multi_arr_decl` 的定长档 `puts("aa")` 同理。

**机制**：变量表 `velemsz[]`（元素字节步长）；数组按 `ceil(N×es/8)` 槽分配；新增 `IR_ADDR_LO`
（字节块最小地址：ir-c `&name[0]`、汇编块最低槽），块内字节一律 `+k` 向上——与 C 字符串语义一致，
两后端无方向分支。初值逐字节写、裸数组名退化块首、链元素宽随 `.(T[N])` 后缀（`char[9]`→1、`i32[3]`→4），
链赋值按元素宽只写低字节。附带修 `sl.()` 等「切片变量解引用」按元素类型窄读。

**验证**：两份与 `.expect` 逐字节相同；**两交叉档 zig 链接 + qemu 实跑同输出**；入 `IR_SUBSET`，
门禁 **59P/0F/11S、0 FAIL**。

**计数**：`IR_SUBSET` **67** / `IR_ONLY` 3 ⇒ pos **70/72、缺口 2**；err **25/34、缺口 9**。
**剩余两份**：`ownerof`（四内置 + 位域位偏移）、`slice_str_infer`（`char[9]` 数组成员 + 动态 `char[]` 成员）
——都需要**聚合体内字节布局**（下一刀）。**边界**：聚合成员仍 8 字节槽。

## 17. PB-83 实测：第 23 条收官——聚合体内字节布局（2026-11-18）

**布局**：`moff` 改 C 布局字节偏移，新增 `align`/`bytes`/`mbit`/`misarr`；struct 按成员对齐
（`Person{name char[]@0, age u8@8, score i32@12}` → 16B/8 对齐）、union 全 0、位域按同类型单元打包
（`Packet`：type@0、len/flag 单元@1（bit 8/12）、data@4、tail 单元@8（bit 64））。

**寻址**：统一「块首（最低地址）+ 字节偏移」——`&agg` 取块首（`IR_ADDR_LO`）、成员 = 块首+`moff`、
`->`/`.(AggType).成员` = 指针值 + 字节偏移、聚合数组元素步长 = `bytes`；成员读写按真实宽
（动态字符串成员 = 8 字节指针），修掉 8 字节写穿相邻成员。

**值语义**：整体赋值 / sret 收发改字节块复制；`_mr_ret` 缓冲 = `bytes`；聚合参数展平按成员偏移取值
（被调方按参数槽访问）。

**新能力**：四个从属查询内置（`structof`/`holdof`/`unionof`/`bitoffsetof`）、`&聚合体.成员`、
`.(AggType).成员`、数组成员名退化、切片字符串赋值（数组侧与链侧）、位域真实位偏移 RMW
（`ir_bitfield` 补 `.expect`，从空比对档升 PASS）。

**验证**：`ownerof` / `slice_str_infer` / `ir_bitfield` 与 `.expect` 逐字节相同；两交叉档 zig 链接 +
qemu 实跑同输出；入/升名单后门禁 **62P/0F/9S、0 FAIL**。

**计数**：`IR_SUBSET` **69** / `IR_ONLY` 3 ⇒ **pos IR 可编 72/72、缺口 0**；err **25/34、缺口 9**
（E-a 3 文案 + E-b 6 依赖 A 侧判据）。**pos 线追平清零；后续工作转向 err 缺口、交叉覆盖与性能项。**

## 18. PB-84 实测：err 缺口清零（2026-11-18）

**9 份逐份复测后分两类**：
- **已报错、文案不对齐（4）**：`cooking_bare_var`（裸 var-decl 从「引用时才 undeclared」改为即时
  `unsupported cooking item`，只跳该 item）；`is_dangling_range` / `is_empty_range`（文案对齐既有
  `.expect`）；`void_member_unknown`（通用 void 指针 `->` 专属文案 + 吞行，消除级联）。
- **静默通过、补诊断（5）**：`structof_bad_member`（既有 `no member` 直接入名单）；`deref_bounds`
  （`.()` 显式宽 > 分配宽）；`flow_move_frozen`（冻结指针再转移给 flow/static）；`slice_str_overflow`
  （切片字符串按常量边界校验容量，数组侧与链侧两路）；`default_init_overflow`（聚合数组默认值展开
  1024 初始化点守卫，与 A 侧 `DEFAULT_INIT_BUF` 同口径同文案）。

**验证**：9 份 rc=1 / 无产物 / 命中 `.expect`；交叉档抽验同文案；全部入 `IR_ERR_COVERED`。
**门禁：71P/0F/0S ×2、0 FAIL**（PASS 62→71、SKIP 9→0）⇒ **pos 72/72 + err 34/34，全语料零跳过、
零失败**。保真口径：`default_init_overflow` 的守卫是**实现资源上限**（A 侧文本缓冲的等价物），
不是语言语义拒绝——若 ltree 认为 2.0 应支持任意规模展开，需要另裁并调整守卫阈值/文案。

## 19. PB-85 实测：位域读宽修复 + ASan 抽检（2026-11-18）

**缺陷**：PB-83 后，表达式路径的位域读仍按 `mcode=0` 发 8 字节 `LOAD`，对 `u8` 存储单元越过聚合体尾部
（ASan 在 `ir_bitfield` 报 `READ of size 8` 越出 8 字节块）。语句复合赋值已用单元宽 `ir_bf_load`，未受影响。

**修复**：四处表达式读（链/primary × `->`/`.`）在位域成员上按 `ir_bf_unit` 窄读（1/2/4；8 整槽），
再右移 + 掩码。

**验证**：`ir_bitfield` 两交叉档 qemu 同输出；**聚合相关 14 份 ir-c 产物 ASan 抽检 14/14 CLEAN**；
门禁 **71P/0F/0S ×2、0 FAIL** 不变。**全语料（pos 72 + err 34）零跳过、零失败 + 内存安全抽检全绿。**

## 20. PB-86 实测：全语料交叉对照（2026-11-18）

字节布局大改后重跑全 pos 语料 × 两交叉档（zig 静态链接 + qemu 实跑，输出先归一行尾）：

| 类 | 数 | 说明 |
|---|---|---|
| **SAME** | **128 / 128** | 64 份可执行用例 × riscv64/loongarch64，与 ir-c 输出逐字相同 |
| NOC（无 main） | 2 × 2 | `mathmod` / `module_top_forms`（模块型，出 `.s` 但不可链接，与既有口径一致） |
| 帧护栏拒 | 5 | `array_capacity_tiers`(2288) / `array_member_default`(4832) / `deref_slice`(3808) / `ir_slice`(2656) / `ir_str`(2112) 超 2047 |
| DIFF | 2 | `ir_sparam`（**IR_ONLY 的非法用例**：成员无类型；交叉档 `mutate bad`）——非回归，随用例去留另行处置 |

**结论**：除「大帧出码能力」与「非法 IR_ONLY 用例」两类既有边界外，**交叉执行面与 ir-c 完全一致**。
ASan（PB-85）：聚合相关 14 份产物 **14/14 CLEAN**。**全语料门禁 71P/0F/0S、0 FAIL。**
