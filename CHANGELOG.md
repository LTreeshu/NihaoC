# NihaoC 变更日志 | Changelog

版本号遵循 [Semantic Versioning](https://semver.org)。本文件记录 NihaoC 编译器（`ncc/`）的里程碑与版本变更。

## [v1.0.0] — 2026-08-31

**首个对外版本（A 方案产品线）**。A 方案（parser → C 文本 → tcc）功能闭环，特性冻结，只修 bug/文档。

### 功能（A 方案 c/native 后端）

- 全量语法回归 **12P / 0F / 5S**（c/native 双后端一致）
- 指针声明语法定案：**隐式推断 + 显式声明双支持**（`p = &x` / `p T* = &x`），`->` 指针成员访问（链式/复合赋值）
- 三元 `?:`、`is pat => stmt` 单语句匹配（`=>` 新 token）、goto/label
- struct/union/enum（嵌套、位域、嵌套初始化列表、整体拷贝）、数组/动态数组（固定容量）、切片、多返回值（命名 struct 返回，C 机制）
- 存储期与所有权（const/static/flow/var + 借用状态机）、cooking 编译期（常量/函数/static_assert）、len()、visof()
- 后端 `-backend=native`：libtcc 进程内编译执行；`-run` 内存执行（Linux only）

### 工程质量

- CLI 完整：init/build/run/debug/lex + -o/-c/-shared/-static/-I/-L/-l/--link/-g
- 构建统一 xmake（Makefile 标 legacy）；错误信息"外壳中文 + 正文英文"
- examples/ 示例集 7 例（6 例 1.0 可编译运行，06_cooking 为 2.0 预览）
- 语言规格冻结：BNF v2.0 终校（`=>`/`->`/`T*` 补全）+ 中英语法元素表核对
- **双平台验证（Windows + Linux/WSL Ubuntu-24.04）**：c/native 全量回归 0 FAIL（12P/0F）+ examples 6/6 双后端一致；`-run` 内存执行 Linux 实测通过

### Linux 平台修复（2026-08-31 WSL 实测）

- `xmake.lua`：os.exec 在 Linux 不走 shell（`|| true` 被当作程序参数）→ Linux 分支显式 `/bin/sh -c` 包装；非 Windows 跳过 p0_link target 与 ir-native 测试
- `cgen.c`：c_type_name 三处 static buf 重叠写（递归调用 src==dst，Linux glibc 损坏输出）→ 独立 `char tmp[256]` 拷贝
- `native.c`：源码构建 libtcc.so 未导出 `tcc_install_dir`（隐式声明 int 截断指针）→ 非 Windows 硬编码 `/usr/local/lib/tcc`
- `native.c`：`-run` 内存执行误判 `tcc_relocate(s, NULL)` 返回值（NULL 语义为"返回所需内存大小"，>0 即成功）→ 改为直接 `tcc_run`（内部自动 relocate）
- 构建依赖：libtcc.so 内部符号（sym_push 等）与 ncc 重名被 ELF 符号插值劫持 → 以 `make libtcc.so LDFLAGS="-fPIC -Wl,-Bsymbolic"` 重新构建安装

### 明确留给 2.0

- 动态数组增长、切片运行时边界、命名空间
- IR 一切（ir-c/ir-native/ir-riscv64/ir-arm64/ir-loongarch64，当前为 2.0 预览）
- native 寄存器分配（性能项）；ir-native 在 Linux ELF 运行时崩溃（2.0 预览，Windows PE 正常）

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
