# NihaoC 项目路线图

> 更新日期：2026-09-20
> 本文件为跨分支共享层（里程碑、架构概览、通用待办、推荐下一步），所有分支保持一致。
> 分支特有待办见 `TODO-PA.md`（1.0 冻结线）和 `TODO-PB.md`（2.0 IR 演进）。

---

## 一、里程碑（git 历史）

> **本表计数写法约定**（与 README／STAGE_SUMMARY 的同类漂移一致后定案）：
> ① 本表只记**不随用例增删漂移的不变量**（如「全矩阵 0 FAIL」）并**必带日期**；
> ② 单分支某时点的详细 `PASS / FAIL / SKIP` 计数写在该分支专属 `TODO-PA.md` / `TODO-PB.md`，不入本表；
> ③ 仅「发布 v1.0.x / v2.0.0」这类发布行可写门禁计数，且必须绑定该版本号（形如「v1.0.2 门禁 …」），其权威详值以 `CHANGELOG.md` 对应版本段为准。
> 本约定自 2026-08-29 分支化之后的行起适用；更早的 M0~后端 B 各行为骨架期历史记录，按当时口径原样保留。

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
| 后端 B | 阶段 1 类型化指针模型（pt[] 类型化 + `.() op=` 复合，含一元 `*p` 收敛移除） | ✅ |
| PB-25 | 指针语法收敛：移除一元 `*p` 解引用 + 补 `.() op=` 复合，与 A 1.0.x 对齐 | ✅ |
| PB-26 | 参数前缀 `flow/var/const/static` + 调用点 M2 所有权检查移植 | ✅ |
| PB-27 | `is` 模式匹配升级（2026-09-16）：do+is 拒绝、畸形/反向范围校验、`_` 通配、`__is_val` 类型感知、移除 `=>`、VIS_* 枚举统一（PB-27.10~12 依赖类型系统演进未排期） | ✅ |
| PB-29 | 返回值可见性前缀 `flow/var/const/static` + 调用点 M2 所有权转移（§7.3 接收规则，2026-09-10；PB-29.1 六条禁止路径 2026-09-12） | ✅ |
| 验收 | PB `xmake test --all` 全矩阵（c/native/ir-c/ir-native）**0 FAIL**（PB-25 ~ PB-29 阶段验收，2026-09-04 ~ 09-16；PASS/SKIP 详细计数随 PB 线用例增删波动，权威口径见 PB 分支 `docs/TODO-PB.md`） | ✅ |
| C | ir-c 输出质量：保留变量名（vreg_name 接入 ir.h/ir.c/irparse.c，ir_to_c 用 vrid 输出可读名；结构体直出待 B 真实布局） | ✅ |
| 发布 v1.0.1 | 1.0.x 指针语法收敛（移除 `T*` 具名声明与一元 `*` 解引用）+ Linux/WSL 平台修复（PA-9）+ `is` 模式匹配规范定案 + §12 矩阵文档统一（2026-09-03，tag → `75c59cc`） | ✅ |
| 发布 v1.0.2 | 1.0.x `is` 移除 `=>` 单语句形式（PA-13，双前端）+ `is _` 通配符补全（PA-15，双前端）+ 循环体外 `is` 前端守卫（PA-18，A 后端 `while_depth`）+ `do` 体内 `is` 在 IR 前端拒绝（PA-19）+ `?=` 安全赋值记号移除（PA-20，BNF v2.3）+ `?.` / `?(` 安全解引用记号移除、可见性与越界检查落入 `.()`（PA-21，BNF v2.3）+ `structof` / `unionof` / `holdof` / `bitoffsetof` 补实现并把签名定稿为三参（PA-22，BNF v2.4，A 后端；IR 前端属 2.0 布局待做）+ §5.1 指针后缀链与切片补实现（`.(T)` 进通用 postfix 链、空 `[]`、切片读/写、`T[n]=malloc` 退化、多维声明，PA-25，BNF v2.5，A 后端；IR 前端属 2.0 待做）+ `len(x)` 内置函数补实现（数组=容量 / 动态字符串=字面量长度 / 切片=`hi-lo`，静态不可知时前端报错；推断切片声明改为指针视图，PA-27，BNF v2.6，A 后端；IR 前端三类取值已有但报错口径属 2.0 对齐）+ `else if` 在 IR 前端补支持（`ir_if_stmt` 递归消化 `else` 后的 `if`，此前强制块而拒绝，PA-28，BNF 既有产生式不变）+ §7.3 调用方接收规则与 §12.1 传递矩阵矛盾收口（文档定案：矩阵只管同作用域变量赋值，返回值接收按 §7.3 更严；1.0 未实现该检查并注明，PA-29，无代码变更）+ 结构体成员分隔符文档示例改回空白分隔（BNF 与实现本就一致，PA-23，无代码变更；探针另登记 IR 前端两项 2.0 缺口：数组类型成员被拒、`print` 未内建）+ `alignof` 改由编译器编译期求对齐并输出字面量（PA-24）+ §5.1.4 / §5.1.5 复合体指针与函数指针示例补实现（切片赋值的字符串右值、推断声明取成员类型与调用返回类型，PA-26，BNF v2.7，A 后端；IR 前端属 2.0 待做）+ `print` / `puts` 输出语义写入规范 §2.3.1（`print` 两形态按首实参静态区分：字面量→`printf` 直通不换行、非字面量→按十进制整数打印并换行；文档矛盾示例 §7.4.1 与 §14.2 共 6 处改写，PA-30，无代码变更；新增 `print_forms.nc` 回归用例）+ `is` 多子句首个匹配即止（无 fallthrough）在 A 后端实现（`while` 每轮迭代清零 `__is_matched`、子句守卫 `!__is_matched && <pat> && (__is_matched = 1)`，IR 前端按决策留待 2.0，PA-16；新增 `is_no_fallthrough.nc`）+ 定长字符数组的字符串初值改为真实数组存储并补两条前端诊断（`s char[n] = "字面量"` 按声明容量分配数组存储不退化；超容量报 `string needs N bytes with terminator, array 's' holds M`；非 `char` 元素数组用字面量初始化即报错；IR 前端两项检查均缺属 2.0 待对齐，PA-31，BNF v2.8；新增 `str_array_init.nc` 与两条 err 用例）+ `flow` 自动释放收紧为只发给堆所有权（`Symbol.no_auto_free` 登记存储来源：字符串字面量／`&x`／聚合初值三类非堆右值在块与函数退出时豁免 `free`，整变量重绑定按新右值重判；修复对静态只读段与栈地址的非法 `free`，IR 前端无释放路径属 2.0，PA-32；新增 `flow_no_free.nc`）+ `flow → flow` 所有权转移补通（`ParserState.moved_src` 局部豁免本语句自身转移源、下一条语句即失效，失效源移交释放责任给接收方不再参与自动释放；冻结源转交所有权即时报错；中英 §12.1/§11.1/§14.2 定案，BNF v2.9，PA-33；新增 `flow_move.nc` 与 `flow_move_frozen.nc`）+ 多变量声明的定长数组后缀不再静默丢弃（`var {…} char[N]` 复用 PA-31 判定：每个变量按声明容量分配数组存储、登记 `len()` 容量，非字面量初值／非 `char` 元素／超容量三条诊断，中英 §4.2 示例改 `char[3]`、§5.1.2 补多变量形态口径，BNF v2.10，PA-34；新增 `multi_arr_decl.nc` 与两条 err 用例）+ 编译器版本号改由构建期注入做单一真源（`ncc.h` 撤除硬编码 `NIHAO_VERSION`，改由 `xmake.lua` 的 `nihao_version` 经 `add_defines` 字符串化注入，`--version`／帮助横幅／verbose 启动行与 `nihao.toml` 脚手架版本同源；非 xmake 构建输出占位 `0.0.0-unknown`，PA-35）+ README 的现在时计数换为 v1.0.2 现值并加权威指针（文档-only，PA-36）+ 本文件「当前代码架构」重写为真实目录树（消除 `test/`／`arch/`／`myapp/` 幽灵路径与 `前端`／`后端` 假子目录，PA-37，文档-only）+ STAGE_SUMMARY 定性为历史快照并把现在时计数换成权威指针（§1 双仓库同步流程标废止，PA-38，文档-only）+ 本文件里程碑表新增计数写法约定，PB 验收行的 `73 PASS / 26 SKIP` 换为不变量 `0 FAIL`＋日期＋指向 `TODO-PB.md`（PA-39，文档-only）+ `GIT_CONVENTIONS.md` 三处失实/失效陈述修正：分支基线改为带日期的历史陈述、已废止的双仓库成对提交义务撤下、失效的「见 §1 发布门禁」引用改指 §3.1（PA-40，文档-only）+ `codegen.c` 死代码全链清理（PA-14）+ xmake PATH 探测与含空格 TCC 目录修复 + BNF v2.1/v2.2/v2.3/v2.4/v2.5/v2.6/v2.7/v2.8/v2.9/v2.10 收敛 + TODO 分层 + 文档-实现全量核对；v1.0.2 门禁 c/native 各 38P/0F/5S（ir-c/ir-native 各 6P/0F/33S，权威详值见 `CHANGELOG.md` v1.0.2 段），`main` 已重新对齐为 `PA` 镜像；遗留文档-实现一致性核对 C 组（C6b 双仓库章节、C7/C8/C9 表格残缺等零星项）与 D 组（语法元素表 `=>` 行残留、v1.0.2 tag 与推送）待决策 | 🟡 待 tag |

## 二、当前代码架构

```
NihaoC/                            仓库根
├── README.md                      对外入口：安装、CLI、后端范围、门禁计数
├── CHANGELOG.md                   版本变更日志（发布计数的权威口径）
├── TODO.md                        待办索引 → docs/ROADMAP.md / TODO-PA.md / TODO-PB.md
├── LICENSE
├── docs/                          规范与工程文档：Chinese.md（语义标准）、English.md（其英译）、
│                                  BNF.md、IMPLEMENTATION_STATUS.md（实现状态 + 源码行号单点维护）、
│                                  ROADMAP.md（本文件）、VERSIONING_ROADMAP.md、GIT_CONVENTIONS.md、
│                                  STAGE_SUMMARY.md（阶段快照）、LEGACY_CODEGEN.md（已删后端归档）、
│                                  PB24_DECISION.md、关键字与语法元素表（中 / 英各一份）、images/
├── examples/                      手工示例 .nc（01_hello / 02_fib / 03_struct / 04_pointer /
│                                  05_string / 06_cooking / 07_multiret）
└── ncc/                           编译器（全部 .c / .h 扁平放置，无子目录分层；职责按注释区分）
    ├── lexer.c / token.h              [前端] 词法分析（最长匹配）
    ├── parser.c                       [前端·A 线] 表达式/声明/函数/语句解析，直接发 C 文本
    ├── irparse.c                      [前端·IR 线] 三地址码构造
    ├── module.c                       [前端] use 跨文件模块解析
    ├── sym.c / type.c / vis.c         [前端] 符号表 / 类型 / 可见性（存储期）分析
    ├── stdlib.c                       [前端] 内置库
    ├── cgen.c                         [后端] C 文本生成 → -backend=c（默认，外部 tcc 编译）
    ├── native.c                       [后端] libtcc 进程内编译 → -backend=native
    ├── ir.c / ir.h / ir_to_c.c        [后端] IR 中间层与 IR→C → -backend=ir-c
    ├── ir_backend.c / ir_backend.h    [后端] 多架构后端注册表 + 统一骨架（TargetBackend 抽象）
    ├── ir_x86_64.c / ir_riscv64.c / ir_arm64.c / ir_loongarch64.c
    │                                  [后端] 四架构汇编发射（ir-native 与三个交叉架构）
    ├── linker.c                       链接辅助
    ├── ncc.c / ncc.h                  CLI 入口：init/build/run/debug/lex + 后端选择 + 版本宏
    ├── xmake.lua                      官方构建与测试驱动（tcc 工具链；版本号单一真源在此）
    ├── Makefile                       LEGACY 构建入口（已弃用，构建与测试一律走 xmake）
    ├── tests/pos / tests/err          回归用例（`xmake test --all` 四后端矩阵）
    ├── editors/sublime/               Sublime Text 语法高亮（nihaoc.sublime-syntax + demo.nc）
    └── build/                         构建产物（ncc.exe、libtcc.dll 副本；由 ncc/.gitignore 忽略）
```

---

## 三、通用待办

### P0 — 核心功能缺口

- [ ] **统一后端管线**：当前存在"parser→C 文本"与"irparse→IR→C/asm"两条并行管线，需决策最终走向——若以 IR 为长期架构，则 parser.c 逐步替换为 irparse.c 的全量版本，避免两套 C 生成（cgen.c / ir_to_c.c）长期并存。（早期第三套 codegen.c 直通后端已于 2026-09-01 删除，见 docs/LEGACY_CODEGEN.md）
- [ ] **IR native 后端寄存器分配**：native 汇编后端（ir_x86_64.c 等，TargetBackend 骨架 ir_backend.c）目前虚拟寄存器全部映射为 rbp 栈槽（无寄存器分配，PB-15 决策保底），性能与调用约定（Windows x64 shadow space / SysV）需完善，浮点调用已在 PB-13/16 覆盖。

### P1 — 工程质量

- [ ] **构建系统统一**：Makefile 与 xmake.lua 双轨并存，需明确主用一套（建议 xmake），另一套标记 legacy；`make test` 内的测试用例仍是旧语法，需更新。

### P2 — 架构与扩展

- [ ] **CLI 完善**：`debug` 子命令实现、`init` 模板与当前语法对齐、错误信息带行列定位并中文化。
- [~] **文档与实现对齐（2026-08-20 增量更新；2026-09-19 全量一致性核对完成）**：STAGE_SUMMARY 刷新 8/20；BNF 补 label/len/cooking 函数定义；README 后端表四架构+示例实测；Chinese/English 补 len/编译期函数/goto label/嵌套/f32 严格宽度；archive 备份已删（P3 卫生项闭环）。**2026-09-19 逐节核对**：BNF 升 v2.2（`is =>` 单语句形式移除、`_` 通配/区间/可见性模式、do 不支持 is 对齐实现）、CHANGELOG 补 2.0 条目、TODO-PB PB-27/PB-29 状态回填、ROADMAP 架构图清 ir_to_native.c。**剩余**：English.md 全文与 2.0 语法逐节复核（工作量大，选做）。
- [ ] **跨平台验证**：`-run`（内存执行）注释为 Linux only；Windows 下 libtcc 动态链接（NIHAO_TCC_DIR 探测）需文档化。

### P3 — 代码卫生

- [x] **清理生成二进制（2026-09-05 登记并闭环）**：源码树生成产物统一落 `build/`（xmake.lua `rule "nihao"` 与 `task "test"` 均写 `build/tests/<backend>/...`），`tests/pos`、`tests/err` 仅存 `.nc`/`.expect`；`ncc/.gitignore` 已忽略 `build/`、`a.out`、`a.out.c`，历史 `test/` 死项（f1b/f4）清理说明。源码根目录的 `ncc/a.out` 已删除。
- [x] **收敛后端生成器（2026-09-01 完成）**：codegen.c（旧直通后端）已删除，仅存 cgen.c（parser→C）与 ir_to_c.c（IR→C）双生成器；设计归档见 docs/LEGACY_CODEGEN.md。
- [x] **linker.c 与后端的关系梳理（2026-09-05 完成）**：头注释精确化——本模块为"库声明收集器"，`link` 指令写入点只 A/默认后端链路 `parser.c`（`link_add_library`）；消费方 `native.c:144`/`stdlib.c:94` 读取 `link_libs` 交 tcc；**IR 后端链路 `irparse.c` 当前不调用本模块**（`link` 指令在 IR 双后端未生效），该接入归属 2.0 阶段 2「link/use 跨文件」项（路线图 A）。

---

## 四、推荐下一步（建议执行顺序）

1. **扩 IR 前端语法**（P0）：以 tests/pos 现有用例为验收，逐个迁移到 irparse。
2. **多后端回归矩阵**（P1）：先让 4 个后端在同一测试集上"行为一致"。
3. **构建统一为 xmake**（P1）：清理 Makefile 入口，`make test` 用例同步更新。
4. 随后再推进 P2/P3 各项。
