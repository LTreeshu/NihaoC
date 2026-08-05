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
- [x] **IR 路径用户函数调用符号 bug（已于 2026-08-05 修复，ncc 提交 c3c91dc）**：`tests/pos/hello.nc`（含用户自定义函数 `add`）经 `-backend=ir-native` 编译报 `undefined symbol '__imp_add'`。根因与修复：
  - `ir_to_native.c`：Windows 下所有 CALL 都生成 `call *__imp_<sym>`，用户函数（程序内符号）无 `__imp_` 别名 → 区分用户函数（直接 `call sym`）与外部导入符号（`__imp_` 间接调用）；
  - `ir_to_native.c`：栈帧对齐逻辑取反（frame 应为 16 的倍数，原代码在已是 16 倍数时 +8 反而破坏对齐）；
  - `ir_to_native.c`：函数无显式 `return`（如 main 隐式返回）时 IR 无 IR_RET，生成代码缺 `leave; ret` 导致执行流坠落 → 函数尾补隐式返回；
  - `ir_to_c.c`：为用户函数生成前置原型，消除"调用先于定义"的隐式声明。
  - 验证：hello.nc 四后端（c/native/ir-c/ir-native）输出一致；13/13 回归通过；调用先于定义场景双后端通过。
- [ ] **统一后端管线**：当前存在"parser→C 文本"与"irparse→IR→C/asm"两条并行管线，需决策最终走向——若以 IR 为长期架构，则 parser.c 逐步替换为 irparse.c 的全量版本，避免三套 C 生成（codegen.c / cgen.c / ir_to_c.c）长期并存。
- [ ] **IR 层数据模型扩展**：浮点、64 位整型、结构体（含嵌套与赋值拷贝）、数组下标、指针运算目前在 IR 指令集中未覆盖，需补充指令或降级策略。
- [ ] **IR native 后端寄存器分配**：`ir_to_native.c` 目前虚拟寄存器全部映射为 rbp 栈槽（无寄存器分配），性能与调用约定（Windows x64 shadow space / SysV）需完善，并支持浮点调用。

### P1 — 工程质量

- [ ] **构建系统统一**：Makefile 与 xmake.lua 双轨并存，需明确主用一套（建议 xmake），另一套标记 legacy；`make test` 内的测试用例仍是旧语法，需更新。
- [ ] **回归测试扩容**：tests/pos（9）+ tests/err（4）覆盖面不足。补充：struct/数组/位域/别名/cooking 编译期执行/visof/多返回值/跨文件模块/flow 自动释放等用例。
- [x] **多后端参数化测试（已于 2026-08-05 完成）**：`xmake test --all` 四后端全矩阵（c/native/ir-c/ir-native），双向白名单 IR_SUBSET/IR_ONLY，PASS/FAIL/SKIP 统计；原 python run_tests.py 已废弃删除
- [x] **IR 管线回归测试（已于 2026-08-05 完成）**：`xmake test -b ir-c / ir-native` 对 IR_SUBSET 白名单用例建立等价基线（当前 3 用例，随语法扩展扩充）

### P2 — 架构与扩展

- [ ] **arch/ 多架构后端**：arch/x86、arm、loongarch、rsicv 目前是空目录。若计划脱离 libtcc 自研代码生成，需逐步填充（x86 已有 IR→汇编基础，可先行）。
- [x] **editors/sublime 语法高亮分支丢失（已于 2026-08-05 合并，ncc 提交 959c9f2）**：当前分支（feat/backend-ir）下 editors/ 为空；已从 main 分支（`bffabcd` 创建、`e208b7f` 修复 [[attr]]）提取合并 `editors/README.md`、`editors/sublime/nihaoc.sublime-syntax`（164 行）、`editors/sublime/demo.nc`，并同步至 NihaoC/ncc。
- [ ] **CLI 完善**：`debug` 子命令实现、`init` 模板与当前语法对齐、错误信息带行列定位并中文化。
- [ ] **文档与实现对齐**：docs/ 中 Chinese.md / English.md / BNF.md 描述的部分语法（cooking、link、多返回值等）与当前实现有差距，需产出一份"已实现 vs 规划"对照表；README 示例代码需用当前编译器实测。
- [ ] **跨平台验证**：`-run`（内存执行）注释为 Linux only；Windows 下 libtcc 动态链接（NIHAO_TCC_DIR 探测）需文档化。

### P3 — 代码卫生

- [ ] 清理 `test/` 下的生成二进制（f1b、f4 等），改为构建目录输出。
- [ ] 评估 codegen.c（旧）与 cgen.c / ir_to_c.c 的重复度，逐步收敛。
- [ ] linker.c 与后端的关系梳理（当前仅 default 后端使用）。
- [ ] 根目录 `docs/archive/Chinese.md.bak` 确认无价值后删除。

---

## 专项一：方案 A — libtcc native 后端（-backend=native，native.c）

### 已完成
- libtcc 封装（tcc_new → compile_string → output_file / run）；目录探测（NIHAO_TCC_DIR > PATH）
- `-backend=native` 可执行文件模式实测通过；`-run` Linux 路径已实现、Windows 明确报错
- xmake 直链 libtcc.dll / -ltcc；`xmake test -b native` 支持

### 待办
- [x] **PA-1 `-run` Windows 不可用（已于 2026-08-06 文档化闭环）**：libtcc 0.9.27 Windows 版 TCC_OUTPUT_MEMORY 损坏（relocate 251）。决策：**正式文档化 Linux-only** —— README 新增「-run 内存执行（Linux only）」小节（含原因/替代方案/能力开关/参数透传说明）；帮助信息已标注 `(Linux only)`；Windows 下报错明确（native_memory_available() 检查）。修复/升级 libtcc 留作独立攻坚项（PA-1b，暂不进行）
- [x] **PA-2 native 无自动化回归（已于 2026-08-05 完成）**：`xmake test -b native` / `--all` 全矩阵覆盖，native 13/13 通过（测试体系已统一为 xmake，python run_tests.py 已移除）
- [x] **PA-3 `-g` 接入（已于 2026-08-06 完成）**：native_state 按 debug_mode 加 `-g`，native_compile_string/native_run_string 接收 debug 参数
- [x] **PA-4 Makefile legacy 化（已于 2026-08-06 完成）**：Makefile 顶部标注 LEGACY（构建统一走 xmake，注明缺 libtcc.h 等已知缺陷），避免误用
- [x] **PA-5 双测试脚本不一致（已于 2026-08-05 解决）**：测试统一为 `xmake test [-b ...] [--all] [-f ...]`，run_tests.py 删除
- [x] **PA-6 `-run` argv 透传（已于 2026-08-06 完成）**：CompilerState 加 run_argc/run_argv，`-run` 之后参数透传 main；Windows 不可用（PA-1）
- [x] **PA-7 link 库声明（已于 2026-08-06 完成）**：native_state 遍历 cs->link_libs 调 tcc_add_library
- [x] **PA-8 错误信息包装（已于 2026-08-06 完成）**：tcc_set_error_func 回调统一输出 `native: <msg>`
- [ ] **PA-9 Linux 路径未实测**：-run、libtcc.so、SysV 调用约定无验证环境
- [x] **PA-10 可用性语义（已于 2026-08-06 完成）**：新增 native_memory_available()（Windows 0 / 其他 1），run_mode 分支改用它替代 #ifdef
- [x] **PA-11 tcc 目录探测重复**：native.c 与 xmake.lua 各一套，易漂移（已记录；建议后续以 NIHAO_TCC_DIR 为唯一来源）

---

## 专项二：方案 B — IR 中间层（ir.h/irparse.c/ir_to_c.c/ir_to_native.c）

### 已完成
- 28 条三地址码指令集定义；irparse 最小子集前端（func/局部变量/if/while/return/算术比较/puts）
- ir_to_c（→C）、ir_to_native（→x86-64 AT&T，Windows x64 ABI 简化版）双后端
- 端到端跑通：用户函数直接 call、栈帧 16 字节对齐、隐式返回兜底、前置原型（c3c91dc）
- hello.nc 四后端输出一致；13/13 回归通过

### 前端 irparse.c 待办（语法覆盖）
- [ ] **PB-1 类型系统**：IrFn 无类型表，参数/返回/局部全固定 int/64 位；需为 IR 增加类型表（i8~u64/f32/f64/char[]/指针）
- [x] **PB-2 一元与复合运算（全部完成 2026-08-06）**：一元 `-x`（IR_NEG）、`!x`（==0 比较）、`~x`（新增 IR_NOT 指令，双后端实现）、复合赋值 `+= -= *= /= %=`、后缀 `x++`/`x--`、前缀 `++x`/`--x` 全部实现。用例：ir_expr.nc（IR_SUBSET，四后端）+ ir_prefix.nc（IR_ONLY，前缀专用）。
- [ ] **P1 全量 parser（parser.c）缺口：前缀 `++x`/`--x` 未支持**——当前被误解析为 `0++` 导致 lvalue 错误；IR 前端已支持，需在 parser.c 表达式解析中补前缀自增/自减。
- [x] **PB-3 数组（部分完成 2026-08-06）**：声明 `name i32[N]`（N 个连续 8 字节 ALLOCA 槽）、下标 `arr[i]` 读写（地址 = &arr[0] + idx*8，复用 ADDR/LOAD/STORE）、初始化列表 `= {1,2,3}` 逐个 STORE。用例 ir_array.nc（IR_ONLY）双后端通过。剩余：切片 `[a..b]`、动态数组 `[...]`/`[6...]`、按类型宽度元素（当前统一 8 字节）。
- [ ] **P1 全量 parser（parser.c）缺口：数组初始化列表 `= {1,2,3}` 未支持**——报 unexpected token '{'；IR 前端已支持
- [ ] **PB-4 struct/union/enum**：匿名嵌套、位域 u8:1、.成员访问
- [ ] **PB-5 存储期属性**：flow/static/const/var 前缀 + visof
- [ ] **PB-6 内置函数**：malloc / sizeof / typeof / offsetof / structof 等
- [x] **PB-7 控制流补齐（部分完成 2026-08-06）**：for（init;cond;step，step 记录重放）、break/continue（循环栈）；剩余：do 循环、is 模式匹配、switch
- [ ] **PB-8 多返回值、函数指针、多变量声明** var {a=0,b=1} i8
- [ ] **PB-9 编译期**：cooking / align / static_assert
- [ ] **PB-10 use 跨文件模块**（当前直接跳过）
- [ ] **PB-11 字符串池去重**（当前每字面量独立 __str_N）

### IR 指令集/数据模型待办
- [x] **PB-12 IR_ADDR/IR_LOAD/IR_STORE 实现（已于 2026-08-05 完成）**：ir.h 新增 IR_ADDR（局部变量取地址）；ir_to_c / ir_to_native 实现 ADDR（leaq/&tN）、LOAD、STORE；irparse 支持一元 `*`（解引用）、`&`（取地址，局部变量）、`*p = expr` 语句；ir_to_c 的 vreg 统一 int64_t（8 字节槽，防 STORE 越界）。
  配套修复：**块内换行语句边界**——lexer 仅顶层发 NEWLINE，函数体内 `*p = 43` 的 `*` 会被上一表达式当乘法吞掉；用"运算符与表达式首 token 同行"判定语句边界（ir_mul/ir_add/ir_cmp 传行号）。
  用例：tests/pos/ir_ptr.nc（IR_ONLY），四后端全矩阵 32 PASS / 0 FAIL。
- [ ] **PB-13 浮点指令**：f32/f64 运算、比较、转换、调用 ABI（xmm）
- [ ] **PB-14 类型化内存访问**：按类型宽度 load/store（当前假定 8 字节）

### 后端待办
- [ ] **PB-15 native 寄存器分配**：当前 vreg 全映射 rbp 栈槽
- [ ] **PB-16 调用约定完整化**：SysV vs Windows x64、浮点参数、结构体传参/返回（sret）
- [ ] **PB-17 IR_CALL 参数收集 bug**：收集函数内全部历史 PARAM 取最后 n 个，嵌套调用 puts(f(1)) 错位
- [ ] **PB-18 ir_to_c 类型化输出**：消除 (int)(long) 与 pointer-from-integer 警告
- [x] **PB-19 -run 内存执行（已于 2026-08-06 与 PA-1 一并文档化）**：Linux only，README 已说明原因与替代方案
- [ ] **PB-20 arch/ 多架构**：arm/loongarch/riscv（空目录）

### 测试与集成待办
- [x] **PB-21 IR 后端回归基线（已于 2026-08-05 完成）**：run_tests.py 加 --backend 全矩阵；IR 双后端用 IR_SUBSET 白名单（当前 hello/ir_demo），其余用例标 SKIP，语法扩展时同步扩充白名单
- [ ] **PB-22 IR 专用用例扩充**：浮点、数组、复杂控制流、嵌套调用
- [ ] **PB-23 CLI**：-backend 帮助文档化、debug 子命令支持 IR

### 架构决策
- [ ] **PB-24 统一管线**：irparse 全量迁移完成前决策最终走向（parser→IR 单管线 vs 双管线并存）

---

## 三、推荐下一步（建议执行顺序）

1. **扩 IR 前端语法**（P0）：以 tests/pos 现有用例为验收，逐个迁移到 irparse。
2. **多后端回归矩阵**（P1）：先让 4 个后端在同一测试集上"行为一致"。
3. **构建统一为 xmake**（P1）：清理 Makefile 入口，`make test` 用例同步更新。
4. 随后再推进 P2/P3 各项。
