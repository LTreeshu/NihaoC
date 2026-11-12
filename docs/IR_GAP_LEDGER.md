# IR 线缺口台账（P1）

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

## 1. 四桶定义

| 桶 | 判据 | 谁该动 |
|---|---|---|
| **G1 语法缺失** | IR 前端拒绝一句 BNF 合法的源文本（解析层就断） | `irparse.c`，PB 可自决 |
| **G2 语义缺失** | IR 接受并出码，但含义与 A 线不同 / 出码非法 / 静默接受非法程序 | 先定口径再动，多数牵共享面 |
| **G3 布局缺失** | 根因是「聚合体没有真实内存布局」（待裁第 23 条） | 需 ltree 先裁槽宽与布局口径 |
| **G4 名单与用例** | 缺口在测试资产或名单本身：可提升、文案不同源、用例写法越界 | 改名单 / 改用例，不动编译器 |

## 2. pos 缺口 20 份（72 份总数中，只有 A 线能编或能编但语义不同的那些）

| 用例 | IR 侧第一手结果 | 桶 | 要补的动作 |
|---|---|---|---|
| `alignof_builtin` | 四档都出码，但 IR 把 4 行 `align ... ok` 全打成 `bad` | G2 | `alignof` 在 IR 恒返回 8（那句「IR 8 字节槽对齐」的注释就是根因），要按类型真实对齐取值 |
| `is_no_fallthrough` | 四档都出码，IR 多打 `wild` / `range` | G2 | IR 前端没有「首匹配即止」标记（`__is_matched` 在 irparse 里 0 命中），重叠 `is` 子句全部执行 |
| `multi_arr_decl` | IR 出码非法 C：`lvalue expected` | G2 | 多维数组读写在 IR 是降级路径，产物编译期就错 |
| `str_array_init` | IR 出码：`assignment makes integer from pointer` | G2 | 字符串数组初始化按整型槽发，指针没落位 |
| `use_mod` | IR 出码：`implicit declaration of function 'add'` | G2 | 跨模块符号未接（路线图阶段 2「link/use 跨文件」） |
| ~~`flow` / `flow_move` / `flow_no_free` / `malloc_demo`~~ | **PB-63 已解**：真正卡点只有 `p.(T)` 一处（`malloc(类型)` 一直没问题——实测 `p void = malloc(i32)` 在 IR 能编能跑） | G1→放行 1 | `malloc_demo` 四档一致已入 `IR_SUBSET`；`flow` / `flow_move` / `flow_no_free` / `borrow` 过前端后改卡 **IR 不下降 `print`**（待裁第 7 条；IR-only 名单下已属必须做） |
| `borrow` | 同 `flow` 族 | G1→卡 print | `print` 下降（A 侧口径见 §4 第 2 条） |
| ~~`len_builtin` / `array_capacity_tiers`~~ | **PB-67 已解**：多维数组声明、切片读入声明数组、开区间切片 rhs、字面量推断字符串的 `len()` 四处补齐后四档输出逐字相同，已入 `IR_SUBSET` | G1→放行 2 | — |
| ~~`deref_slice`~~ | **PB-68 已解**：后缀链求值器落地后四档实跑输出逐字相同、与 `.expect` 逐字节相同（八行 ok），已入 `IR_SUBSET`。原快照留档：`ir: array dimension 1 must have a positive length`（`void[][]`）/ `expected ']', got '['`（`p3[][].(i32)`）——**PB-61 快照**；2026-11-11 PB-67 复测实况是 `array dimension 1 ...` + `array dimension 2 ...` / `ir: unexpected token ']' in expression` | G1→放行 1 | 真因确为「指针类型后缀档」而非容量档（PB-67 的改记成立）。**两条边界未解、登记不修**：① `char[9]` 在 IR 仍是 9 个 8 字节槽（A 侧 9 字节紧凑），`.(char[9])[k]` 按槽宽 8 寻址才与 IR 自己的 `arry[k]` 自洽 ⇒ 归待裁第 23 条；② 两交叉档新暴露的**帧生长方向分歧**（见 §5） |
| `array_member_default` | `ir: unexpected token '[' in expression` | G3 | 数组元素默认值展开，需成员级真实偏移 |
| `struct_member_default` | `ir: member default value: only simple constant supported` | G3 | 成员默认值只落了「简单常量」那一半，PA-49 / PA-56 家族未回灌完 |
| `designator_init` | `ir: unexpected token '.' in expression` | G1 | `.field = v` 指定式初值不解析 |
| ~~`features`~~ | ~~`ir: unsupported top-level token 'alias'`~~ | G1→放行 | **PB-66 已解**：顶层 `alias` + 顶层 `const` / `var` + `for` 声明式 init + `else if` 链 + struct 数组成员齐活，四档输出逐字相同，已入 `IR_SUBSET` |
| ~~`mathmod` / `transfer`~~ | ~~`unsupported top-level token 'const'` / `'flow'`~~ | G1→放行 | **PB-66 已解**：顶层变量声明（含带可见性前缀的 `flow`）落成 `IrProg` 全局池，`transfer` 的 `flow create() void` 顶层函数声明一并放行 |
| ~~`for_init_forms`~~ | ~~`ir: unexpected top-level identifier 'count'`~~ | G1→放行 | **PB-66 已解**：顶层变量 + `for` 的三种 init 形态（`i i32 = 0` / `i = 0` / `i (i32) = 0`）+ 任意表达式 step（`i++` / `i--` / `j = j - 1` / `tick()`）全部放行，step 改为 body 之后执行 |
| ~~`module_top_forms`~~ | ~~bracket attributes 只对 `func` 生效（got `const`）~~ | G1→放行 | **PB-66 已解**：属性表落到 `const` 声明上（`[[export ".mydata"]]`），模块型用例四档都出 `.c` 产物 |
| `is_pattern_forms` | `ir: bad 'is' pattern` | G1 | 带符号区间 `is -9..-5` 一类模式 |
| `ownerof` | `ir: unexpected token '.' in expression` | G1 | `.()` 后缀链形态 |
| `slice_str_infer` | `ir: expected member name` | G1 | 切片 / 字符串指针推断 |
| `stmt_terminators` | `ir: unsupported top-level token ';'` | G1 | 顶层 `;` / `#` 终止符 |
| `type_forms` | `ir: unexpected token 'i32' in expression` | G1 | 类型出现在表达式位置（C 风格转换形态） |
| `void_slot_infer` | `ir: unexpected token ']' in expression` | G1 | `void` 指针下标推断 |

**桶分布**：G1 **14**、G2 **5**、G3 **2**（G4 在 pos 侧本轮为 0——两份「看似可提升」的 `alignof_builtin` 与
`is_no_fallthrough` 实测四档输出不同，**不可提升**；这也复核了 PB-56 当时「可提升 0 份」的结论仍然成立）。
**PB-67 追加**：G1 由 16 收到 **14**——`len_builtin` / `array_capacity_tiers` 两份放行；`deref_slice` 留在 G1 但
**改记到「指针类型后缀档」一簇**（本轮第一手复测的结论，见下）。
**PB-68（2026-11-12）追加**：G1 由 14 收到 **13**——`deref_slice` 放行（指针类型后缀档整簇收完），
pos 侧合计 **20** 份 = G1 13 + G2 5 + G3 2。

## 3. err 缺口 15 份（IR 侧不参与或参与了但判据不同源）

| 子类 | 份数 | 用例 | 说明 |
|---|---|---|---|
| **E-a 已诊断、文案不同源** | 3 | `cooking_bare_var` `is_dangling_range` `is_empty_range` | IR 有等价拒绝，只是句子与共用 `.expect` 不同源；已在待裁第 26 条（READY / PARTIAL / COLLIDES 三批清单）里 |
| **E-b IR 在更外层就拒，够不到语义诊断** | 6 | `deref_bounds` `flow_move_frozen` `structof_bad_member` `void_member_unknown` `slice_str_overflow` `default_init_overflow` | 全都断在解析层（`expected ')', got 'i64'` / `unexpected token '.'`），**修复依赖本表第 2 节的 G1**，不是独立工作 |
| **E-c IR 完全不诊断** | ~~6~~ → **2**（2026-11-10 PB-62 收了 4 份） | 已收：`str_array_bad_elem` `str_array_overflow` `multi_arr_nostr` `multi_arr_overflow`（现为「有诊断 + 无产物」并已入 `IR_ERR_COVERED`）；余：`len_unknown`（需 `len_known` 表，否则会把空动态字符串误判）、`void_subscript`（需指针指向类型跟踪，与 G1 的 `p.(T)` 同根） |

更新（2026-11-10，PB-62）：**E-c 桶已由 6 份收到 2 份**——`str_array_bad_elem` / `str_array_overflow` / `multi_arr_nostr` / `multi_arr_overflow` 四份在 IR 前端补上与 A 同口径的三条诊断后转为「有诊断 + 无产物」，已入 `IR_ERR_COVERED`；门禁 ir-c / ir-native 由 28 份 PASS 升到 32 份（0 FAIL，c / native 与比对档不变，交叉矩阵 48/48 不变）。两份余量的卡点见本行右侧。
更新（2026-11-11，PB-64 / PB-65）：**`print` 已在 IR 下降到 printf**，`flow` 四份全部放行——`flow` / `flow_no_free` / `borrow` 入 `IR_SUBSET` 并计 PASS，`flow_move` 入名单升比对档；`features` 现在只剩顶层 `alias` 一挡。同轮修掉**字符串池序列化不转义**的缺陷（含换行的串在 ir-c 产物里是非法 C、在交叉 `.s` 里劈成两行只靠汇编器容错），修好后交叉的 `unterminated string` 告警由 4 降到 0。`str_array_init` 由「出码语义坏」变成「能编能跑但输出截断」（IR `x/9/A` vs A `xiaoming/9/ABCDEFGH`），归第 23 条家族，**不入名单**。
更新（2026-11-11，PB-66）：**顶层声明与 `alias` 那一簇 5 份全部放行**——`features` / `for_init_forms` / `transfer` / `mathmod` / `module_top_forms` 四档实跑输出逐字相同（三份带 `.expect` 计入 PASS，两份模块型只出 `.c` 产物），已入 `IR_SUBSET`；门禁 ir-c / ir-native 由 35 升到 **40 PASS**、SKIP 34 → **29**、0 FAIL，c / native 68P/0F/3S 不变，比对档 34 + 2 不变。pos 侧缺口 28 → **23** 份（G1 21 → 16）。**边界照旧诚实**：`char[32]` 这类数组成员在 IR 侧仍只占 1 个 8 字节槽（真实布局仍是待裁第 23 条），`features` 恰巧不回读该成员才逐字相同——它进名单是因为输出相同，不是因为布局已定案；越出首槽的元素写给专属诊断。
## 4. 从这张表读出的排期结论

1. **单点最大的是「真实布局」**：PB-61 时 G3 的 2 份 + E-c 的 6 份 + G2 的 3 份（多维读写、字符串数组、`alignof`）
   共 **11 份**挂在同一条欠账上（待裁第 23 条）；PB-62 收掉 4 份 E-c 后为 **7 份**。它不解，IR 线永远补不到 A 线的 68 份。
2. **G1 的 13 份聚成 2 簇**，不是 13 件事：**`p.(T)` 类型解引用（PB-63 已做：1 份当场放行、4 份改卡 `print`）**、
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
