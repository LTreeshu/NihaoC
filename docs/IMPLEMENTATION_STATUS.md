# NihaoC 规范与编译器实现对应关系

本文档记录 NihaoC 语言规范（Chinese.md / English.md / BNF.md）中各项语义与编译器（`ncc/`）实际实现的对应状态。

> 更新日期：2026-11-20（0.2.3 门禁判别力加固：35 份 pos 补 `.expect`，门禁 PASS 74→109；修 0.2.2 聚合参数注册回归与 i8 元素符号扩展。此前：PB-76 聚合整块连续槽；PB-78 `void` 指针声明处指向类型；PB-79 成员默认值 + 指定式初值；PB-80 聚合体数组；PB-81 第 33 条按 (c) provenance 结案；PB-82 定长 char 数组字节紧凑；PB-83 聚合体内字节布局收官；PB-84 err 缺口清零；PB-85 位域读宽；PB-87/88 大帧出码 + 聚合参数字节布局；PB-89 字面量可写副本；PB-90 const 决定可写性；0.2.2 嵌套聚合 8 字节块 ABI）。此前本文件含大量 A 线（`parser.c`/`cgen.c`/`native.c`）
> 实现表与源码行号锚点，随 A 线物理删除全部作废，已整体重写为下方 IR-only 简明版。历史细节见 `CHANGELOG.md`。
>
> 本文件是**实现状态层**文档：`BNF.md` / `Chinese.md` / `English.md` / 两份语法元素表只写设计、不写落地；状态一律记在这里或 `TODO.md`。

## 1. 现态（2026-11-18）

| 项 | 现值 |
|---|---|
| 管线 | 单一 IR 管线：`irparse.c`（前端）→ `ir.c`（中间层）→ 出码 |
| 后端 | `ir-c`（默认）、`ir-native`（本机 x86-64 上等价 `ir-c`）、`ir-riscv64`、`ir-loongarch64`（交叉 `.s`） |
| 门禁 | `ir-c` **109P/0F/0S**、`ir-native` **109P/0F/0S**、0 FAIL（pos 73/73 全部有 `.expect`，err 36/36） |
| pos 覆盖 | IR 可编 **73 / 73**（`IR_SUBSET` 71 + `IR_ONLY` 2）；**缺口 0** |
| err 覆盖 | IR 覆盖 **36 / 36**（`IR_ERR_COVERED`）；**缺口 0** |
| 机器码发射器 | `ir-riscv64` / `ir-loongarch64` 两档（PB-87 大帧窗口化后 **73/73 pos 均可出码**；交叉实跑 71 份可执行用例 × 两档 **142/142 与 ir-c 逐字相同**，仅 2 份模块型无 main）；`ir-riscv32` / `ir-arm` 未开工 |

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

**pos 与 err 语料已全覆盖（72/72、34/34）**；剩余边界：

| 项 | 说明 |
|---|---|
| `string` 可写语义 | 按 §5.1 用例实现 (a)：非 const 的 `string` / `char[]` 绑字面量时复制到可写内存（PB-89）；**可写性由 `const` 决定**（PB-90）：const 绑只读池且自有存储写入静态拒绝，指针指向内存不受限；副本暂不追踪释放 |
| 聚合值跨函数 | sret 与按值参数走字节拷贝/展平；聚合参数按 **8 字节块**（`ceil(bytes/8)`）展平并在函数入口搬入字节布局块（0.2.2），**嵌套聚合/大聚合**已覆盖；块 ABI 为内部约定 |
| 默认值展开守卫 | 聚合数组的成员默认值展开**超过 1024 个初始化点**即拒绝（`default_init_overflow`；等价 A 侧 `DEFAULT_INIT_BUF` 的实现资源上限，可按裁定放宽） |
| 大帧上限 | 三个 2040 字节窗口覆盖帧 ≤ **6000**；更大会被发射器拒绝（寄存器分配/次级基址之外的情形） |

9 份 err 缺口：E-a 3（诊断文案不同源）、E-b 6（断在解析层、依赖 pos 侧布局）。

## 4. 输出内建（2026-11-12 设计改变）

`print` / `puts` 不再是编译器内置，改由 **stdio 标准库**（`use stdio`）提供。实现侧待办：移除 `irparse.c` 里
`print` 的内置下降（格式串透传 `printf` / 整型打印 / 浮点 DTOI），改为像 `puts` 一样直发外部符号，由 stdio 模块提供实现。
