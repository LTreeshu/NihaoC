# NihaoC 规范与编译器实现对应关系

本文档记录 NihaoC 语言规范（Chinese.md / English.md / BNF.md）中各项语义与编译器（`ncc/`）实际实现的对应状态。

> 更新日期：2026-11-18（PB-76 聚合整块连续槽；PB-78 `void` 指针声明处指向类型；PB-79 成员默认值 + 指定式初值；PB-80 聚合体数组；PB-81 第 33 条按 (c) provenance 结案；PB-82 定长 char 数组字节紧凑）。此前本文件含大量 A 线（`parser.c`/`cgen.c`/`native.c`）
> 实现表与源码行号锚点，随 A 线物理删除全部作废，已整体重写为下方 IR-only 简明版。历史细节见 `CHANGELOG.md`。
>
> 本文件是**实现状态层**文档：`BNF.md` / `Chinese.md` / `English.md` / 两份语法元素表只写设计、不写落地；状态一律记在这里或 `TODO.md`。

## 1. 现态（2026-11-18）

| 项 | 现值 |
|---|---|
| 管线 | 单一 IR 管线：`irparse.c`（前端）→ `ir.c`（中间层）→ 出码 |
| 后端 | `ir-c`（默认）、`ir-native`（本机 x86-64 上等价 `ir-c`）、`ir-riscv64`、`ir-loongarch64`（交叉 `.s`） |
| 门禁 | `ir-c` **59P/0F/11S**、`ir-native` **59P/0F/11S**、0 FAIL、比对 36 格（2 后端） |
| pos 覆盖 | IR 可编 **70 / 72**（`IR_SUBSET` 67 + `IR_ONLY` 3）；缺口 **2** 份 = 聚合体内字节布局 |
| err 覆盖 | IR 覆盖 **25 / 34**（`IR_ERR_COVERED`）；缺口 9 份 |
| 机器码发射器 | `ir-riscv64` / `ir-loongarch64` 两档（交叉验证：60 链接 / 59 AGREE / 2 既有 CRASH）；`ir-riscv32` / `ir-arm` 未开工 |

## 2. 核心实现锚点

| 规范项 | 实现（`ncc/irparse.c` 或以符号名） |
|---|---|
| 顶层声明 / `alias` / 全局变量 | `ir_parse_top_level`、`ir_top_decl`、`ir_top_var`、`ir_alias_decl`、全局池 `ir_add_global` |
| 模块 `use` | `ir_use_decl` → `ir_use_module` → `ir_parse_module_source`（内联进同一 `IrProg`） |
| 语句终止符 `;` / `#` | `ir_stmt`（包装层）+ `ir_stmt_impl` + `ir_eat_stmt_terminator` |
| `is` 模式 | `ir_is_int`、`is_matched_vreg`（首匹配即止）、`emit_is_cmp_with_const` |
| 指针解引用 `.()` / `.(T)` / `[]` / `->` | 后缀链求值器 `ir_chain` / `ir_chain_assign`、旧路径 `.()`/`[]` 分支 |
| 窄型存取 | `ir_load_width_imm` / `ir_store_width`（`IR_LOAD`/`IR_STORE` 的 imm 承载字节宽） |
| `malloc` | `ir_malloc_size`、`last_malloc_sc`（指向类型记录） |
| `len()` | `vlen_known` 表 + 诊断 |
| 可见性 / 所有权 | `ir_vis_*` 一族（vstate / vborrow_src / vptr） |
| 类型对齐 | `type_align_token`、`ir_agg_align`（PB-74） |
| 聚合体 | `agg_types`（`msize`/`mscode`/`mtype`/`marr_*`）、`agg_compute_offsets`、`ir_agg_init`/`ir_agg_decl`；**聚合体存储 = 整块连续 8 字节槽**：前端发 `IR_AGG_ALLOCA`（`imm=8*mslots`），汇编后端跳过（槽按 vreg 编号连续），ir-c 落成 `int64_t name[slots]` 并把成员 vreg 映射到 `name[k]`（PB-76） |

## 3. 已知边界（真实字节布局，IR 线尚未实现的能力）

聚合体已是**连续的 8 字节槽块**（`&p + 8` 能到 `p.y`，PB-76），但**尚未字节紧凑**：成员各占一个 8 字节槽、
数组成员仍按 1 槽且只可寻址首 8 字节；`structof` / `holdof` / `unionof` / `bitoffsetof` 与指定式初值 / 成员默认值
仍要真实字节偏移。两交叉档还存在**帧生长方向分歧**（待裁第 33 条）：汇编槽向下生长，`&p + 8*k` 向高地址走，
`type_forms` 因此在交叉档打 `bad`（ir-c / ir-native 已正确）。明细见 `docs/IR_GAP_LEDGER.md` §11。

| 用例 | 卡点 |
|---|---|
| `ownerof` | `structof`/`holdof`/`unionof`/`bitoffsetof` 四内置需聚合体内字节偏移与位域位偏移 |
| `slice_str_infer` | `char[9]` 数组成员需真实宽（当前成员仍 1 槽） |
| （注）`deref_slice` / `array_member_default` | 已在名单；其 `main` 帧超 2047，交叉两档出码即被帧护栏拒（既有上限） |

9 份 err 缺口：E-a 3（诊断文案不同源）、E-b 6（断在解析层、依赖 pos 侧布局）。

## 4. 输出内建（2026-11-12 设计改变）

`print` / `puts` 不再是编译器内置，改由 **stdio 标准库**（`use stdio`）提供。实现侧待办：移除 `irparse.c` 里
`print` 的内置下降（格式串透传 `printf` / 整型打印 / 浮点 DTOI），改为像 `puts` 一样直发外部符号，由 stdio 模块提供实现。
