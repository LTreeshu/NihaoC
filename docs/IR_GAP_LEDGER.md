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
  的那部分，其余进跨后端比对池。

## 1. 四桶定义

| 桶 | 判据 | 谁该动 |
|---|---|---|
| **G1 语法缺失** | IR 前端拒绝一句 BNF 合法的源文本（解析层就断） | `irparse.c`，PB 可自决 |
| **G2 语义缺失** | IR 接受并出码，但含义与 A 线不同 / 出码非法 / 静默接受非法程序 | 先定口径再动，多数牵共享面 |
| **G3 布局缺失** | 根因是「聚合体没有真实内存布局」（待裁第 23 条） | 需 ltree 先裁槽宽与布局口径 |
| **G4 名单与用例** | 缺口在测试资产或名单本身：可提升、文案不同源、用例写法越界 | 改名单 / 改用例，不动编译器 |

## 2. pos 缺口 28 份（72 份总数中，只有 A 线能编或能编但语义不同的那些）

| 用例 | IR 侧第一手结果 | 桶 | 要补的动作 |
|---|---|---|---|
| `alignof_builtin` | 四档都出码，但 IR 把 4 行 `align ... ok` 全打成 `bad` | G2 | `alignof` 在 IR 恒返回 8（那句「IR 8 字节槽对齐」的注释就是根因），要按类型真实对齐取值 |
| `is_no_fallthrough` | 四档都出码，IR 多打 `wild` / `range` | G2 | IR 前端没有「首匹配即止」标记（`__is_matched` 在 irparse 里 0 命中），重叠 `is` 子句全部执行 |
| `multi_arr_decl` | IR 出码非法 C：`lvalue expected` | G2 | 多维数组读写在 IR 是降级路径，产物编译期就错 |
| `str_array_init` | IR 出码：`assignment makes integer from pointer` | G2 | 字符串数组初始化按整型槽发，指针没落位 |
| `use_mod` | IR 出码：`implicit declaration of function 'add'` | G2 | 跨模块符号未接（路线图阶段 2「link/use 跨文件」） |
| `flow` / `flow_move` / `flow_no_free` / `malloc_demo` | `expected ')', got 'i32'` | G1 | `malloc(类型)` 的类型实参形态 + `p.(T)` 设计器（PB-56 已量这一条挡 8 份用例） |
| `borrow` | 同上 | G1 | 同上，借用通路一起 |
| `deref_slice` / `len_builtin` / `array_capacity_tiers` | `expected '=', got '['` | G1 | 数组 / 切片容量档声明形态 |
| `array_member_default` | `ir: unexpected token '[' in expression` | G3 | 数组元素默认值展开，需成员级真实偏移 |
| `struct_member_default` | `ir: member default value: only simple constant supported` | G3 | 成员默认值只落了「简单常量」那一半，PA-49 / PA-56 家族未回灌完 |
| `designator_init` | `ir: unexpected token '.' in expression` | G1 | `.field = v` 指定式初值不解析 |
| `features` | `ir: unsupported top-level token 'alias'` | G1 | 顶层 `alias` |
| `mathmod` / `transfer` | `unsupported top-level token 'const'` / `'flow'` | G1 | 顶层变量声明（含带可见性前缀的），在册既有缺口 |
| `for_init_forms` | `ir: unexpected top-level identifier 'count'` | G1 | 同上，顶层变量 |
| `module_top_forms` | bracket attributes 只对 `func` 生效（got `const`） | G1 | P12 只落属性表在 `func` 上那一档 |
| `is_pattern_forms` | `ir: bad 'is' pattern` | G1 | 带符号区间 `is -9..-5` 一类模式 |
| `ownerof` | `ir: unexpected token '.' in expression` | G1 | `.()` 后缀链形态 |
| `slice_str_infer` | `ir: expected member name` | G1 | 切片 / 字符串指针推断 |
| `stmt_terminators` | `ir: unsupported top-level token ';'` | G1 | 顶层 `;` / `#` 终止符 |
| `type_forms` | `ir: unexpected token 'i32' in expression` | G1 | 类型出现在表达式位置（C 风格转换形态） |
| `void_slot_infer` | `ir: unexpected token ']' in expression` | G1 | `void` 指针下标推断 |

**桶分布**：G1 **21**、G2 **5**、G3 **2**（G4 在 pos 侧本轮为 0——两份「看似可提升」的 `alignof_builtin` 与
`is_no_fallthrough` 实测四档输出不同，**不可提升**；这也复核了 PB-56 当时「可提升 0 份」的结论仍然成立）。

## 3. err 缺口 15 份（IR 侧不参与或参与了但判据不同源）

| 子类 | 份数 | 用例 | 说明 |
|---|---|---|---|
| **E-a 已诊断、文案不同源** | 3 | `cooking_bare_var` `is_dangling_range` `is_empty_range` | IR 有等价拒绝，只是句子与共用 `.expect` 不同源；已在待裁第 26 条（READY / PARTIAL / COLLIDES 三批清单）里 |
| **E-b IR 在更外层就拒，够不到语义诊断** | 6 | `deref_bounds` `flow_move_frozen` `structof_bad_member` `void_member_unknown` `slice_str_overflow` `default_init_overflow` | 全都断在解析层（`expected ')', got 'i64'` / `unexpected token '.'`），**修复依赖本表第 2 节的 G1**，不是独立工作 |
| **E-c IR 完全不诊断** | 6 | `len_unknown` `void_subscript` `str_array_bad_elem` `str_array_overflow` `multi_arr_nostr` `multi_arr_overflow` | **最严重的一桶**：非法程序被 IR 接受。其中 4 份的报错来自下游 C 编译器（`lvalue expected` / `implicit declaration`），不是 ncc；`str_array_overflow` **连下游都不拦，直接产出可执行文件**（实测 art=Y） |

## 4. 从这张表读出的排期结论

1. **单点最大的是「真实布局」**：G3 的 2 份 + E-c 的 6 份 + G2 的 3 份（多维读写、字符串数组、`alignof`）共
   **11 份**挂在同一条欠账上（待裁第 23 条）。它不解，IR 线永远补不到 A 线的 68 份。
2. **G1 的 21 份聚成 4 簇**，不是 21 件事：`malloc(类型)` + `p.(T)`（6 份）、顶层声明与 `alias`（5 份）、数组 /
   切片容量档（3 份）、表达式细节与模式（7 份）。按簇排，一簇一轮。
3. **E-c 那 6 份应当优先于「补齐语法」**：它们是「编译器不报错」而不是「编译器不支持」，属于会往外漏错程序的一类；
   最小动作是先给专属诊断（形状同 PA-52 / PA-59 那两轮），真实布局到位后再换成精确检查。
4. **台账更新方式**：每轮 P2 收一簇后重跑 `build/p61/promotable.sh` 与本节计数，把改完的行从表里删掉——本文件只存
   「还欠什么」，不存历史（历史归 `CHANGELOG.md` 与 `docs/IMPLEMENTATION_STATUS.md`）。
