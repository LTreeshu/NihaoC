# NihaoC 变更日志 | Changelog

版本号遵循 [Semantic Versioning](https://semver.org)。本文件记录 NihaoC 编译器（`ncc/`）的里程碑与版本变更。

## [v1.0.2] — 2026-09-19（待 tag）

1.0 冻结线的规范合规与代码卫生版本，无新增语言特性。

- **`is` 移除 `=>` 单语句形式（PA-13，用户可见语法变更）**：`is <pattern> => <statement>` 全线下线，`is` 只保留块形式 `is <pattern> { ... }`。C 后端 `parser.c` 的 `parse_is_stmt` 删除 `TOK_FAT_ARROW` 分支（改报 `expected '{' after 'is' pattern`）；IR 前端 `irparse.c` 同分支删除（双前端与 BNF v2.2 一致，错误路径吞 token 防死循环）；`tests/pos/ir_is.nc` 清理 `=>` 用例。`=>` 仍由 lexer 识别为 `TOK_FAT_ARROW`（词法保留，语法不使用），`token.h` 注释同步。
- **`codegen.c` 死代码全链清理**：删除早期直出 C 的后端 `ncc/codegen.c`（507 行，已由 `cgen.c` + `parser.c` 管线取代）及其调用链——`linker.c` 相关 120 行、`ncc.h` 84 行声明与枚举、`ncc.c` 4 行后端注册；`ncc/xmake.lua` 的 `add_files` 去掉 `codegen.c`。
- **构建修复（用户可见）**：`xmake.lua` PATH 探测的 `gmatch` 模式误把盘符冒号当分隔符切开（`D:\...` 被截断）导致编译器探测失败；同时修正含空格 TCC 安装目录的参数引号处理。
- **BNF 收敛入文档**：v2.1 记录指针语法收敛（一元 `*` 解引用移除，解引用统一 `.()` / `.(T)` / `->`）；v2.2 重写 `<pattern>`（通配符 / 整数 / 负整数 / 闭区间 `lo..hi` / 枚举变体 / 可见性枚举 / 结构体解构与 ADT 变体解构（预留）/ 标识符），`<is-stmt>` 从 `<statement>` 移除、`while` 体内 `is-clause` 与 `do` 不支持 `is` 口径对齐，中英 §6.1/§13.3 与语法元素表同步。
- **TODO 分层拆分**：`TODO.md` 拆为 `ROADMAP.md`（跨分支里程碑与通用待办）+ `TODO-PA.md`（1.0 冻结线专属）+ `TODO-PB.md`（2.0 IR 线专属），并建立共享文档跨分支同步规则。
- **文档-实现全量一致性核对**：PB-27 子项状态回填、PB-29 `ret_vis` 检查条件表述修正（`ret_vis > VIS_VAR` 恒假 → 显式 `∈ {CONST, FLOW, STATIC}`）、ROADMAP 架构图清除已删除的 `ir_to_native.c` 引用、VERSIONING_ROADMAP 阶段 2 重复条目去重、`ncc.h` 后端注释与 `set_version` 对齐。
- **PA TODO 清零**：PA-1 ~ PA-14 全部 `[x]`，1.0 冻结线无待办。
- **已知缺口（登记于 `docs/IMPLEMENTATION_STATUS.md`）**：1.0 线 A 后端 `is _` 通配符与 `is <identifier>` 变量绑定为"已定义、待实现"（当前按值比较；2.0 线已覆盖）；函数参数可见性前缀仍统一按 `VIS_DEFAULT` 处理；IR 后端不做所有权/借用检查。
- **回归验证**：c / native 双后端各 **12P / 0F / 5S**（0 FAIL），examples 6/7 编译运行（`06_cooking` 为 2.0 预览，预期不通过）；IR→C 对 `tests/pos/ir_is.nc` 生成正常。

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
- 三元 `?:`、`is pat => stmt` 单语句匹配（`=>` 新 token；已于 v1.0.2 / PA-13 移除，`is` 只保留块形式）、goto/label
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
