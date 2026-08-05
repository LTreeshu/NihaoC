# NihaoC 项目进度与 TODO 清单

> 更新日期：2026-08-05
> 本文档反映 `ncc/` 活跃开发目录（git `28bfb3f`）的最新状态，源码已同步至本目录 `ncc/`。

---

## 一、当前进度

### 1.1 里程碑（git 历史）

| 里程碑 | 内容 | 状态 |
| ------ | ---- | ---- |
| 基线 | 导入 ncc 骨架现状 | ✅ |
| M0 | 复活骨架：统一 token 命名、补关键字、词法最长匹配、恢复编译管线 | ✅ |
| M1 | 表达式/声明/函数/语句解析 + C 后端单趟转 C（端到端可运行） | ✅ |
| M1 完善 | 修复数组/位域/for/别名/while 系列 bug（11 个测试通过） | ✅ |
| M2 | 所有权/借用静态分析：存储期矩阵 + 冻结/失效状态机 + 作用域解冻 | ✅ |
| M3 | use 跨文件解析 + flow 自动释放 + print 映射 + main 返回 0 | ✅ |
| M4 | CLI 工具链（init/build/run/debug/lex）+ 回归测试套件 | ✅ |
| 后端 A | `-backend=native`：libtcc 进程内生成机器码 | ✅ |
| 后端 B | IR 中间层骨架（三地址码 + 双后端） | ✅ |
| 后端 B | IR 双后端端到端跑通（`-backend=ir-c` / `ir-native`） | ✅ |

### 1.2 当前代码架构

```
ncc/
├── 前端
│   ├── lexer.c / token.h        词法分析（最长匹配）
│   ├── parser.c                 表达式/声明/函数/语句解析（直接转 C 文本）
│   ├── module.c                 use 跨文件模块解析
│   ├── sym.c / type.c / vis.c  符号表 / 类型 / 可见性(存储期)分析
│   └── stdlib.c                 内置库
├── 后端（4 种可切换，-backend=）
│   ├── codegen.c / cgen.c       C 文本生成（默认后端，外部 tcc 编译）
│   ├── native.c                 libtcc 进程内编译并执行/生成可执行文件
│   └── ir.c / ir.h / ir_to_c.c / ir_to_native.c
│                               IR 中间层（三地址码）：IR→C、IR→x86-64 汇编
├── irparse.c                    IR 前端（最小子集，骨架验证用）
├── linker.c                     链接辅助
├── ncc.c / ncc.h                CLI 入口：init/build/run/debug/lex + 后端选择
├── Makefile / xmake.lua         双构建系统（tcc 工具链）
├── test/                        手工示例（.nc）
├── tests/pos, tests/err        回归测试套件（9 pos + 4 err，run_tests.py 驱动）
├── arch/                        x86/arm/loongarch/rsicv（目录已建，内容为空）
├── editors/sublime              （空）
└── myapp/                       CLI init 生成的示例项目
```

---

## 二、需要完善的 TODO

### P0 — 核心功能缺口（影响"可用"）

- [ ] **IR 前端覆盖全量语法**：`irparse.c` 目前只支持最小子集（module/use/func、局部变量、if/else/while/return、整数表达式、puts 调用）。需将 parser.c 已实现的 struct、数组、位域、类型别名、`cooking` 编译期、`flow`/`visof`、多返回值、跨文件模块等全部语法迁移到 IR 管线。
- [ ] **IR 路径用户函数调用符号 bug（已验证复现）**：`tests/pos/hello.nc`（含用户自定义函数 `add`）经 `-backend=ir-c` / `ir-native` 编译报 `undefined symbol '__imp_add'`，同时生成 C 中带"assignment makes pointer from integer"警告——IR→C 对用户函数缺少原型声明/符号修正（Windows tcc `__imp_` 前缀），仅 puts 等外部符号可用；ir_demo.nc（最小子集）双后端正常。
- [ ] **统一后端管线**：当前存在"parser→C 文本"与"irparse→IR→C/asm"两条并行管线，需决策最终走向——若以 IR 为长期架构，则 parser.c 逐步替换为 irparse.c 的全量版本，避免三套 C 生成（codegen.c / cgen.c / ir_to_c.c）长期并存。
- [ ] **IR 层数据模型扩展**：浮点、64 位整型、结构体（含嵌套与赋值拷贝）、数组下标、指针运算目前在 IR 指令集中未覆盖，需补充指令或降级策略。
- [ ] **IR native 后端寄存器分配**：`ir_to_native.c` 目前虚拟寄存器全部映射为 rbp 栈槽（无寄存器分配），性能与调用约定（Windows x64 shadow space / SysV）需完善，并支持浮点调用。

### P1 — 工程质量

- [ ] **构建系统统一**：Makefile 与 xmake.lua 双轨并存，需明确主用一套（建议 xmake），另一套标记 legacy；`make test` 内的测试用例仍是旧语法，需更新。
- [ ] **回归测试扩容**：tests/pos（9）+ tests/err（4）覆盖面不足。补充：struct/数组/位域/别名/cooking 编译期执行/visof/多返回值/跨文件模块/flow 自动释放等用例。
- [ ] **多后端参数化测试**：run_tests.py 增加按 `-backend=c|native|ir-c|ir-native` 全矩阵运行同一用例，防止双管线行为漂移。
- [ ] **IR 管线回归测试**：为 `ir-c` / `ir-native` 建立与默认后端等价的测试基线。

### P2 — 架构与扩展

- [ ] **arch/ 多架构后端**：arch/x86、arm、loongarch、rsicv 目前是空目录。若计划脱离 libtcc 自研代码生成，需逐步填充（x86 已有 IR→汇编基础，可先行）。
- [ ] **editors/sublime 语法高亮分支丢失**：当前分支（feat/backend-ir）下 editors/ 为空；语法高亮文件 `nihaoc.sublime-syntax` 实际已在 main 分支完成（`bffabcd` 创建、`e208b7f` 修复 [[attr]]），需从 main cherry-pick/合并回当前分支。
- [ ] **CLI 完善**：`debug` 子命令实现、`init` 模板与当前语法对齐、错误信息带行列定位并中文化。
- [ ] **文档与实现对齐**：docs/ 中 Chinese.md / English.md / BNF.md 描述的部分语法（cooking、link、多返回值等）与当前实现有差距，需产出一份"已实现 vs 规划"对照表；README 示例代码需用当前编译器实测。
- [ ] **跨平台验证**：`-run`（内存执行）注释为 Linux only；Windows 下 libtcc 动态链接（NIHAO_TCC_DIR 探测）需文档化。

### P3 — 代码卫生

- [ ] 清理 `test/` 下的生成二进制（f1b、f4 等），改为构建目录输出。
- [ ] 评估 codegen.c（旧）与 cgen.c / ir_to_c.c 的重复度，逐步收敛。
- [ ] linker.c 与后端的关系梳理（当前仅 default 后端使用）。
- [ ] 根目录 `docs/archive/Chinese.md.bak` 确认无价值后删除。

---

## 三、推荐下一步（建议执行顺序）

1. **扩 IR 前端语法**（P0）：以 tests/pos 现有用例为验收，逐个迁移到 irparse。
2. **多后端回归矩阵**（P1）：先让 4 个后端在同一测试集上"行为一致"。
3. **构建统一为 xmake**（P1）：清理 Makefile 入口，`make test` 用例同步更新。
4. 随后再推进 P2/P3 各项。
