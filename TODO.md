# NihaoC 项目进度与 TODO 清单

> 更新日期：2026-08-18
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

- [~] **IR 前端覆盖全量语法**：已完成大部分（截至 8/18：struct/union/enum、数组/位域、cooking 编译期、flow/visof、多返回值(sret)、switch、link、函数指针、类型别名、切片；IR_SUBSET 25 用例四后端一致）。剩余细项：goto、命名空间、部分 is 模式。
- [x] **IR 路径用户函数调用符号 bug（已于 2026-08-05 修复，ncc 提交 c3c91dc）**：`tests/pos/hello.nc`（含用户自定义函数 `add`）经 `-backend=ir-native` 编译报 `undefined symbol '__imp_add'`。根因与修复：
  - `ir_to_native.c`：Windows 下所有 CALL 都生成 `call *__imp_<sym>`，用户函数（程序内符号）无 `__imp_` 别名 → 区分用户函数（直接 `call sym`）与外部导入符号（`__imp_` 间接调用）；
  - `ir_to_native.c`：栈帧对齐逻辑取反（frame 应为 16 的倍数，原代码在已是 16 倍数时 +8 反而破坏对齐）；
  - `ir_to_native.c`：函数无显式 `return`（如 main 隐式返回）时 IR 无 IR_RET，生成代码缺 `leave; ret` 导致执行流坠落 → 函数尾补隐式返回；
  - `ir_to_c.c`：为用户函数生成前置原型，消除"调用先于定义"的隐式声明。
  - 验证：hello.nc 四后端（c/native/ir-c/ir-native）输出一致；13/13 回归通过；调用先于定义场景双后端通过。
- [ ] **统一后端管线**：当前存在"parser→C 文本"与"irparse→IR→C/asm"两条并行管线，需决策最终走向——若以 IR 为长期架构，则 parser.c 逐步替换为 irparse.c 的全量版本，避免三套 C 生成（codegen.c / cgen.c / ir_to_c.c）长期并存。
- [~] **IR 层数据模型扩展**：浮点(f64/x87+riscv D)、64 位整型、struct(含 sret 返回/参数展开)、数组下标、指针运算、位运算、字节读写(LOAD8/STORE8)、.() 解引用均已覆盖（PB-1/3/4/13/14/16/18 + 位域 + 动态字符串）。剩余：struct 嵌套赋值拷贝、f32 严格宽度。
- [ ] **IR native 后端寄存器分配**：`ir_to_native.c` 目前虚拟寄存器全部映射为 rbp 栈槽（无寄存器分配），性能与调用约定（Windows x64 shadow space / SysV）需完善，并支持浮点调用。

### P1 — 工程质量

- [ ] **构建系统统一**：Makefile 与 xmake.lua 双轨并存，需明确主用一套（建议 xmake），另一套标记 legacy；`make test` 内的测试用例仍是旧语法，需更新。
- [x] **回归测试扩容（2026-08-13 完成）**：IR_SUBSET 9→23（14 用例升四后端：12 全量可跑 + ir_prefix/ir_multi 修复后升）；顺带修复 3 个全量真 bug（跨行后缀 ++ 误吃 / multi-decl init 残留 / 动态字符串池寻址）。IR_ONLY 剩 4（语义差异：ir_builtin 槽模型 sizeof/mr 旧语法/slice 读/sparam 无类型成员）。
- [x] **多后端参数化测试（已于 2026-08-05 完成）**：`xmake test --all` 四后端全矩阵（c/native/ir-c/ir-native），双向白名单 IR_SUBSET/IR_ONLY，PASS/FAIL/SKIP 统计；原 python run_tests.py 已废弃删除
- [x] **IR 管线回归测试（已于 2026-08-05 完成）**：`xmake test -b ir-c / ir-native` 对 IR_SUBSET 白名单用例建立等价基线（当前 3 用例，随语法扩展扩充）

### P2 — 架构与扩展

- [x] **arch/ 多架构后端（2026-08-15 完成）**：四目标架构全齐——x86-64（ir_x86_64.c，Win64 ABI）、riscv64（RV64I+D）、arm64（AAPCS64）、**loongarch64（2026-08-15 新增，LA64 指令映射）**；均经 TargetBackend 抽象（ir_backend.c 注册表 + CLI -backend=ir-<arch>）。arch/ 空目录为历史遗留，实际后端在 ncc/ 下。
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
- [x] **PB-1 类型系统（2026-08-07 全部完成）**：变量 vtype 扩展为类型编码（0=i64 1=f64/f32 2=i8 3=i16 4=i32 5=u8 6=u16 7=u32，与 IrFn.param_types 同编码）；**窄整数语义**：槽仍 8 字节，赋值时截断+符号/零扩展存槽（读回即正确值）——新指令 **IR_TRUNC**（imm: 0=i8 1=i16 2=i32 3=u8 4=u16 5=u32）：ir-to-c `(int8_t)` 等强转、x86-64 `shlq+sarq/shrq` 移位法（无 SSE 依赖）、riscv64 `slli+srai/srli`；截断点：声明初始化、表达式级赋值（=、复合 op=、++/--）；参数声明记录窄类型（值已符号扩展，入槽直通）。**数组元素宽度（完成）**：新增 vetyp[] 元素类型表——初始化列表/元素赋值 STORE 前截断、浮点元素读值标记 double；**ir-to-c LOAD/STORE 类型感知**（dst/源 vreg 为 double → `*(double*)`，原硬编码 `*(int64_t*)` 会把 double 位模式截断成整数）。**双向转换（完成）**：新指令 **IR_DTOI**（double→int64 截断向零：ir-to-c `(int64_t)`、x86-64 x87 `fistpq` 前 **fstcw/fldcw 切 RC=向零 0x0C00**（fistpq 默认最近舍入 3.7→4，实验确认后改用 rsp 动态让出 16 字节临时区）、riscv64 `fcvt.l.d`）；**ir_coerce 统一目标类型协调**（float 目标+int 源→ITOD、int 目标+double 源→DTOI、窄→TRUNC），4 个调用点。**顺带修一元负 double bug**（`-x` 原发整数 IR_NEG 毁 double 位模式 → float 走 FSUB 0.0-a）与**逻辑非 double**（`!x` → FCMP EQ 0.0）。**char[]/字符串类型化（完成）**：TOK_CHAR → i8 编码（变量/参数/数组元素宽度截断）、字符字面量 `'A'` → ASCII 常量、字符串数组 `s char[N] = "hello"` 逐字节 STORE 含 NUL（截断到容量）、`string` 类型=指针别名（LD_ADDR 字符串地址）。ir_narrow/narray/conv/str.nc（IR_ONLY）双后端一致通过，riscv64 验证。
- [x] **PB-2 一元与复合运算（全部完成 2026-08-06）**：一元 `-x`（IR_NEG）、`!x`（==0 比较）、`~x`（新增 IR_NOT 指令，双后端实现）、复合赋值 `+= -= *= /= %=`、后缀 `x++`/`x--`、前缀 `++x`/`--x` 全部实现。用例：ir_expr.nc（IR_SUBSET，四后端）+ ir_prefix.nc（IR_ONLY，前缀专用）。
- [x] **P1 前缀 `++x`/`--x`（2026-08-07 验证已支持）**——全量 parse_unary 已有 case（PB-2 期间顺带实现）；顺带补齐 IR 前端前缀 ++/--（ir_primary 处理，含浮点变量 FADD/FSUB 路由）
- [x] **PB-3 数组（全部完成 2026-08-07）**：声明 `name i32[N]`、下标 `arr[i]` 读写、初始化列表；**动态数组** `[6...]`/`[...]`（固定容量槽，增长语义留 TODO）；**切片** `arr[lo..hi]`/`arr[..hi]` → 返回 `&arr[lo]` 指针。**重要修复**：新增 `IR_ELEM_ADDR`（元素地址按后端布局方向：ir-c 向上 +idx*8 / ir-native 向下 -idx*8，slot 向下生长）；ir-c 数组基改 C 数组声明 `t{base}[N]`（元素槽 vreg 连续分配但只发基 ALLOCA，ADDR imm=k 生成 `&t{base}[k]`）——**修复 ir_array native 的 sum bad（原字节偏移 &t0+8 方向错、C 变量不连续被测试盲区掩盖）**。用例 ir_array.nc + ir_slice.nc（IR_ONLY）双后端通过。**切片赋值（2026-08-09 完成）**：`arr[lo..hi] = {v0, v1, ...}` 批量写回（逐个 STORE + ir_coerce 窄元素；hi 边界校验留 TODO）——ir_slice.nc 扩展（写回/读取/窄元素）双后端一致。剩余：动态增长、按类型宽度元素、切片长度语义（void 指针无长度）。
- [x] **P1 数组初始化列表 `= {1,2,3}`（2026-08-07 完成）**——parse_declaration 的 initializer 分支加 `{` 处理（原样输出元素列表给 C），数组/聚合初始化均可用
- [x] **PB-4 struct/union/enum（2026-08-06 完成）**：命名类型定义 `Name struct/union/enum { }`、enum 常量（`Color enum{RED,GREEN,BLUE}`，可显式赋值）、成员读写 `s.field`（含复合赋值）、union 共享槽 0、struct/union 初始化列表、聚合类型变量（成员按序分配独立槽，ADDR imm 直接引用槽 vreg）。用例 ir_struct.nc（IR_ONLY）双后端通过。剩余：匿名/内联嵌套类型、位域宽度（8 字节槽模型忽略）、成员默认值仅支持简单常量。
- [x] **PB-5 存储期属性 + visof（2026-08-06 完成）**：变量声明前缀 `const/static/flow/var`（记录可见性到变量表 vvis）；`visof(x)` 编译期查询（visof 是关键字 TOK_VISOF，返回 NH_* 常量 0-4）；可见性枚举常量 `_undef/_const/_flow/_static/_var` 作表达式常量；`is _flow` 等可见性模式（比较 is_val == NH_*）+ 标识符/枚举模式。用例 ir_vis.nc（IR_ONLY）双后端通过。
- [x] **P1 全量 parser 关键字内置函数缺口（2026-08-06 修复，A 方案可用）**：新增 `parse_builtin_kw()` 在 parse_primary 分派 TOK_SIZEOF/TOK_TYPEOF/TOK_ALIGNOF/TOK_OFFSETOF/TOK_VISOF（含 `_undef/_const/_flow/_static/_var` 可见性枚举常量 case、parse_is_stmt 可见性模式 token 分支）；cgen 补 `#include <stddef.h>`（offsetof 宏）。**顺带修复全量 parser 语句边界缺陷**：函数体内无 NEWLINE token，`f()\n*p = 1` 的 `*` 会被上句乘法循环吞掉 → 表达式 binop 链（multiplicative→assign 全部 12 层）传起始行号参数，换行即语句边界（与 IR 前端同方案）。新增 A 方案用例 malloc_demo.nc（malloc + `.()` 解引用）c/native 后端通过
- [x] **PB-6 内置函数（2026-08-06 完成，核心 5 个）**：`sizeof(type/expr)`（类型大小表 i8=1..f64=8、聚合=成员数*8 槽模型、数组=size*N）、`typeof`（映射为 sizeof）、`alignof`（IR 槽模型返回 8）、`offsetof(Type,member)`（成员序*8，union 0）、`malloc(Type[ N ])`（编译期定大小 → 调用外部 malloc，动态分配 + 指针读写验证）。用例 ir_builtin.nc（IR_ONLY）双后端通过。剩余：structof/unionof/holdof/bitoffsetof（需真实内存布局，IR 8 字节槽模型不支持，报错）、`*p op= e` 复合赋值解引用。另发现：sizeof/typeof/alignof/offsetof 是关键字 token，**全量 parser（parser.c）的 identifier 字符串比较分支永远走不到**（与 TOK_VISOF 同问题，记 P1）
- [x] **PB-7 控制流补齐（全部完成 2026-08-06）**：for（init;cond;step，step 记录重放）、break/continue（循环栈）、do（while 别名，前测循环）、is 模式匹配（匹配循环条件值，支持 `-1` / `0..50` 闭区间；配套新增**表达式级赋值** `x += 1` / `x++` 使 `while x += 1 { is -1 {...} }` 可用）、switch（C 风格 `switch(e){ case e: ... default: ... }`，延迟绑定 JZ 布局，break 跳出 switch / continue 非法）。用例 ir_switch.nc（IR_ONLY）双后端通过。剩余：is 的标识符模式（_flow/_static，需 visof）、`is pat => stmt` 单语句形式（lexer 无 `=>` token）
- [x] **P1 全量 switch/case（2026-08-07 完成）**——parse_statement 加 TOK_SWITCH 分支，生成 C 原生 switch（case 表达式须编译期常量），每个 case 后自动 break（NihaoC 无 fallthrough 语义）；break 天然跳出。P0 综合用例 p0_case.nc（IR_SUBSET）四后端一致通过
- [x] **PB-8 多返回值、函数指针、多变量声明（全部完成 2026-08-07）**：多变量声明 `var {a=0,b=1} i8`——ir_stmt 前缀后 `{` 走 ir_multi_decl（收集 name=init 对 → 类型/聚合/数组 → 逐个 var_declare+MOV），无前缀 `{` 仍是块语句（无冲突）；用例 ir_multi.nc。**顺带补齐 &&/|| 短路逻辑层**（ir_logical_and/or，JZ/JNZ 跳转跳过右侧求值）。**函数指针最小集**——新指令 `IR_CALLI`（dst=call *(a)）：ir_to_c 生成 `((int64_t (*)())tN)(args)`，ir_to_native 生成 `movq slot(a),%rax; call *%rax`；声明 `fp void(i32,i32) i32 = add2`（声明分支跳过函数指针类型参数列表+返回类型）；函数名引用 → IR_LD_ADDR sym（取函数地址）；变量后跟 `(` → 间接调用。用例 ir_fptr.nc。**多返回值（struct 返回 sret 机制）**——命名结构体返回：`func f() Result`（IrFn.is_mr，隐藏 out-param `_mr_ret` 注入 var 表头部第 0 槽）；`return {e0,e1,...}` 聚合返回（STORE 到 *_mr_ret+k*8 + bare RET）；调用 `v Result = f()`（malloc 连续缓冲 → PARAM 缓冲+参数 → CALL → 缓冲值偏移 LOAD → 拷贝到聚合槽，last_mr_buf 标记）；成员访问复用 struct 机制。⚠️ `multireturn` 关键字已于 2026-08-08 移除（设计澄清：多返回=命名 struct，非关键字），用例 ir_mr.nc 改用 `Result struct`。三用例均 IR_ONLY 双后端通过
- [x] **PB-9 编译期（2026-08-08 完成）**：`static_assert(expr,"msg")` 编译期断言——`ir_const_expr` 常量折叠求值链（int 字面量/一元 -!~/四则取模/比较/&&/||/括号/enum 常量/sizeof(type)/visof(x)/可见性枚举/编译期变量），失败时报 `static_assert failed: msg`；`cooking { ... }` 编译期块（顶层+函数内）；**编译期变量表（完成）**：`const NAME [TYPE] = expr` 存 ct_vars（跨块共享、重复声明报错），static_assert/编译期表达式可用，**运行时引用折叠为常量**（ir_primary var_find 失败分支查 ct_var——⚠️ 该分支前已 next_tok 消费标识符，勿再 next_tok 双重消费）；`align N { ... }` 对齐块——IR 8 字节槽模型下跳过块体。用例 ir_cook.nc（IR_ONLY）双后端通过。剩余：编译期函数调用（cooking-call 执行，全量也未实现）
- [x] **PB-10 use 模块（2026-08-07 最小支持）**：`use name[.name]` 解析跳过（IR 单文件模型，模块内容需内联；入口 use 循环修复 `.` 残留 token）
- [x] **PB-11 字符串池去重（2026-08-07）**：ir_add_string 查重，相同字符串复用同一 __str_N

#- [x] **PB-15 目标后端抽象（2026-08-07 阶段 1/2/3 完成）**：设计敲定（讨论后）——TargetBackend 接口（布局/调用约定数据字段 + fn_prologue/emit_ins/fn_epilogue/expand 四钩子）+ 每平台独立 .c（ir_x86_64.c）+ 统一骨架（ir_backend.c：NBuf/注册表/字符串池/函数循环/帧计算）。借鉴 tcc（每平台 *-gen.c 指令选择）与 LLVM（TargetLowering 抽象调用约定）。决策：全栈槽保底 / 浮点每后端专用 / riscv64 只验汇编生成。**tcc 0.9.27 坑：结构体内联函数指针声明解析 bug（报 '; expected (got *)'）→ 函数指针类型必须 typedef 到结构体外**。阶段 1 行为不变（ir_to_native.c → ir_x86_64.c + 框架），全矩阵 0 FAIL。**阶段 2（参数化收尾）**：CALL 参数寄存器走 tb->int_arg_regs。**阶段 3（riscv64 后端）**：新增 ir_riscv64.c（RV64I + D 浮点：fld/fadd.d、div/rem 天然有、beqz/bnez、la/addi 寻址、a0-a7 调用约定；IR_RET 帧恢复用 g_rv_frame 静态变量——emit 拿不到 frame）；CLI `-backend=ir-riscv64`（backend=4，生成 .s 后跳过 tcc 交叉汇编）；ir_expr/ir_float/ir_loop/ir_ptr/ir_mr/ir_fptr 汇编生成验证通过

## IR 指令集/数据模型待办
- [x] **PB-12 IR_ADDR/IR_LOAD/IR_STORE 实现（已于 2026-08-05 完成）**：ir.h 新增 IR_ADDR（局部变量取地址）；ir_to_c / ir_to_native 实现 ADDR（leaq/&tN）、LOAD、STORE；irparse 支持一元 `*`（解引用）、`&`（取地址，局部变量）、`*p = expr` 语句；ir_to_c 的 vreg 统一 int64_t（8 字节槽，防 STORE 越界）。
  配套修复：**块内换行语句边界**——lexer 仅顶层发 NEWLINE，函数体内 `*p = 43` 的 `*` 会被上一表达式当乘法吞掉；用"运算符与表达式首 token 同行"判定语句边界（ir_mul/ir_add/ir_cmp 传行号）。
  用例：tests/pos/ir_ptr.nc（IR_ONLY），四后端全矩阵 32 PASS / 0 FAIL。
- [x] **PB-13 浮点（2026-08-07 f64 最小集 + 调用 ABI）**：vreg_type 类型表（0=int 1=double，IrFn 动态增长）；变量 vtype 表 + 槽 vreg 同步标记；浮点字面量（CONST 存 double 位模式）；FADD/FSUB/FMUL/FDIV/FCMP 指令；**ir-to-native 用 x87 浮点栈实现**（fldl/faddp/fsubp/fmulp/fdivp/fcomip+setcc）——tcc 汇编器不支持 SSE movsd；**fsubp/fdivp 有方向（st0=st0 op st1）需先压 b 再压 a**（实验确认）；CONST 改 `movq $imm64,%rax`（原 movq $imm 超 32 位失败）。**浮点调用 ABI（2026-08-07 完成）**：IrFn 加 param_types[32]/ret_is_double；CALL 按 vreg_type 分流装载（int→int_arg_regs / double→fp_arg_regs；x86-64 用 xmm0-3 走 movq 位模式搬运——tcc 汇编器支持 xmm 的 movq、riscv64 用 fa0-fa7 fld/fsd）；返回值按目标函数 ret_is_double 存 xmm0/fa0；**混合类型提升**：新指令 IR_ITOD（int→double：ir-to-c `(double)`、x86 fildq+fstpl、riscv fcvt.d.l），binop/cmp 任一侧 double 时先提升 int 侧；f32 声明槽化 double（宽转换，严格宽度留待类型宽度改造）。**顺带修 multi-function label 冲突**（label 编号 per-function 从 0 起，多函数同一汇编文件 .L0 重名 → ir_backend.c 发射前加跨函数 lbl_base）。用例 ir_float.nc + ir_fcall.nc（IR_ONLY）双后端通过。剩余：f32 严格宽度、double→int 转换
- [x] **PB-14 类型系统（2026-08-07 基础）**：vreg_type + 变量 vtype 表（浮点类型记录与槽标记）；与 PB-13 合并交付。剩余：按类型宽度 load/store（i8/i16/i32 截断+符号扩展，当前统一 8 字节槽）

### 后端待办
- [x] **PB-15 native 寄存器分配（2026-08-07 决策确认）**：全栈槽保底是后端抽象设计时用户敲定的决策（正确性优先、后端最简单）；当前 x86-64/riscv64/arm64 均全栈槽 + 调用约定参数寄存器。真实寄存器分配（live range/spill）留作未来性能优化项（LLVM RegAllocGreedy 路线参考）
- [x] **PB-16 调用约定完整化（2026-08-08 完成）**：**struct 参数按值展开**——IrFn.param_agg_ti 记录参数聚合类型，聚合参数占 mcount 个虚拟参数槽（成员槽 vreg 连续，prologue 按展开索引入槽）；调用方 arglist 展开收集（聚合实参=struct 变量成员槽 / mr 调用从 last_mr_buf LOAD）；**return p（聚合变量）**——逐字段 STORE 到 _mr_ret 缓冲（复用 sret 机制）。多 struct 参数/链式调用（makePerson() 作实参）/修改后返回双后端一致。**浮点参数（xmm/d0/fa0）已在 PB-13 完成**；**SysV 由 riscv64（RV64 ABI）与 arm64（AAPCS64）后端天然体现**，x86-64 保持 Windows x64（本机无 SysV 运行验证环境）。ir_sparam.nc（IR_ONLY）双后端通过，riscv64 汇编验证（a0-a2 展开入槽）
- [x] **PB-17 IR_CALL 参数收集 bug（2026-08-07 验证已修复）**：sret/多返回机制重构时已改为收集模式（args[] 收集 → 按序发射 PARAM+CALL），嵌套调用 dbl(add(3,4))、多参数嵌套 add(dbl(2),dbl(3))、连续调用链、嵌套作 puts 参数——双后端全对（已验证）
- [x] **PB-18 ir_to_c 类型化输出（2026-08-07 完成）**：字符串池地址参数包 `(char*)`（is_str_addr 检测 LD_ADDR __str_N——puts 等外部函数参数消除 pointer-from-integer）；malloc 返回值包 `(int64_t)(intptr_t)`（void* → int64 消除 integer-from-pointer）。生成 C 全矩阵 0 warning（assignment makes 类），行为不变 0 FAIL
- [x] **PB-19 -run 内存执行（已于 2026-08-06 与 PA-1 一并文档化）**：Linux only，README 已说明原因与替代方案
- [x] **PB-20 arch/ 多架构（2026-08-08 验证完成）**：**arm64 后端 ir_arm64.c 已就绪**（AAPCS64：stp x29,x30 帧、d0-d7 浮点参数、fadd/fsub/fmul/fdiv、ldp 恢复，全栈槽 x29-stride*(N+1)，只验汇编生成）——ir_fcall/ir_sparam/ir_float/ir_loop 汇编生成正确；riscv64（RV64I+D）+ arm64 + x86-64（Win64）**三架构后端全齐**。**loongarch64（2026-08-15 完成）——四架构收官**（PB-20 延伸，克隆 riscv64 + LA64 映射，汇编生成验证）

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
