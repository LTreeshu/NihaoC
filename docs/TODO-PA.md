# 方案 A — libtcc native 后端待办（PA 分支）

> 更新日期：2026-09-19（PA-1 ~ PA-15、PA-17 ~ PA-19 全部完成；PA-16 为已登记的既有语义缺口，不纳入 v1.0.2；v1.0.2 发布准备就绪待 tag）
> 本文件为 PA 分支（1.0 冻结线）专属待办。通用里程碑与跨分支待办见 `ROADMAP.md`。
> PA 分支处于冻结维护态，仅接受规范合规修复与代码卫生项。

---

## 已完成

- libtcc 封装（tcc_new → compile_string → output_file / run）；目录探测（NIHAO_TCC_DIR > PATH）
- `-backend=native` 可执行文件模式实测通过；`-run` Linux 路径已实现、Windows 明确报错
- xmake 直链 libtcc.dll / -ltcc；`xmake test -b native` 支持

---

## 待办

- [x] **PA-1 `-run` Windows 不可用（已于 2026-08-06 文档化闭环）**：libtcc 0.9.27 Windows 版 TCC_OUTPUT_MEMORY 损坏（relocate 251）。决策：**正式文档化 Linux-only** —— README 新增「-run 内存执行（Linux only）」小节（含原因/替代方案/能力开关/参数透传说明）；帮助信息已标注 `(Linux only)`；Windows 下报错明确（native_memory_available() 检查）。修复/升级 libtcc 留作独立攻坚项（PA-1b，暂不进行）
- [x] **PA-2 native 无自动化回归（已于 2026-08-05 完成）**：`xmake test -b native` / `--all` 全矩阵覆盖，native 13/13 通过（测试体系已统一为 xmake，python run_tests.py 已移除）
- [x] **PA-3 `-g` 接入（已于 2026-08-06 完成）**：native_state 按 debug_mode 加 `-g`，native_compile_string/native_run_string 接收 debug 参数
- [x] **PA-4 Makefile legacy 化（已于 2026-08-06 完成）**：Makefile 顶部标注 LEGACY（构建统一走 xmake，注明缺 libtcc.h 等已知缺陷），避免误用
- [x] **PA-5 双测试脚本不一致（已于 2026-08-05 解决）**：测试统一为 `xmake test [-b ...] [--all] [-f ...]`，run_tests.py 删除
- [x] **PA-6 `-run` argv 透传（已于 2026-08-06 完成）**：CompilerState 加 run_argc/run_argv，`-run` 之后参数透传 main；Windows 不可用（PA-1）
- [x] **PA-7 link 库声明（已于 2026-08-06 完成）**：native_state 遍历 cs->link_libs 调 tcc_add_library
- [x] **PA-8 错误信息包装（已于 2026-08-06 完成）**：tcc_set_error_func 回调统一输出 `native: <msg>`
- [x] **PA-9 Linux 路径实测（2026-08-31 WSL Ubuntu-24.04 完成）**：c/native 各 12P/0F/6S（0 FAIL）；examples 6/6 双后端；`-run` 内存执行修复（tcc_relocate(NULL) 语义误判 → 直接 tcc_run）；实测修复 4 个平台 bug：① xmake os.exec Linux 不走 shell（`|| true` 被当参数）→ `/bin/sh -c` 包装；② c_type_name static buf 重叠写（递归调用 src==dst，Linux glibc 损坏）→ 独立 tmp 拷贝；③ 源码构建 libtcc.so 未导出 tcc_install_dir → 非 Windows 硬编码 /usr/local/lib/tcc；④ libtcc.so 内部符号（sym_push 等）与 ncc 重名被 ELF 符号插值劫持 → `make libtcc.so LDFLAGS="-fPIC -Wl,-Bsymbolic"` 重建。ir-native 在 Linux ELF 运行时崩溃（2.0 预览），测试框架非 Windows 跳过（类比 p0_link）
- [x] **PA-10 可用性语义（已于 2026-08-06 完成）**：新增 native_memory_available()（Windows 0 / 其他 1），run_mode 分支改用它替代 #ifdef
- [x] **PA-11 tcc 目录探测重复**：native.c 与 xmake.lua 各一套，易漂移（已记录；建议后续以 NIHAO_TCC_DIR 为唯一来源）
- [x] **PA-12 A 方案 1.0 发布（2026-08-31 完成）**：门禁验证 ✅（c/native 各 12P/0F/5S + examples 6/6，Windows）；Linux 实测 ✅（PA-9，c/native 各 12P/0F/6S + examples 6/6 + `-run` 修复）；README 更新 ✅（安装/CLI/后端表 1.0/2.0 范围）；BNF v2.0 终校 ✅（`=>`/`->`/`T*` 补全，中英文档同步）；CHANGELOG.md 建立 ✅；本地 `v1.0.0` tag ✅（commit `2036fba`，合 main + push 由 ltree 决定，2026-08-31 已授权执行）。发布后 PA 分支进入冻结维护态
- [x] **PA-13 `is` 移除 `=>` 箭头形式（2026-09-15 完成，v1.0.2 收口）**：文档已删除单语句 `=>` 形式，C 后端 parser.c parse_is_stmt 的 TOK_FAT_ARROW 分支移除；IR 前端 irparse.c 的同分支于 2026-09-19 一并移除（双前端与 BNF v2.2 一致，错误路径吞 token 防死循环），只保留块形式 `is <pattern> { ... }`。测试文件 ir_is.nc 同步清理 `=>` 用例。token.h 注释改为"`=>` 词法保留、语法不使用"。属规范合规，允许在冻结线执行
- [x] **PA-14 清理废弃死代码（2026-09-15 起，2026-09-17 全链收官，冻结例外·代码卫生）**：parser.c 的 `parse_statement_full` / `parse_function_full` / `compile_file_full`（共 199 行）已废弃且无调用点，删除。ncc.h 对应声明同步移除。2026-09-17 续：删除早期直出 C 后端 `ncc/codegen.c`（507 行，已被 cgen.c + parser.c 管线取代）及其调用链——linker.c 120 行、ncc.h 84 行、ncc.c 4 行注册、xmake.lua add_files 条目
- [x] **PA-15 `is _` 通配符补全（2026-09-19 完成，v1.0.2）**：BNF v2.2 `<pattern>` 首项即通配符 `_`，但 1.0 线 A 后端 `parse_is_stmt` 未处理（`_` 被当作普通标识符按值比较，符号表无 `_` 时行为不确定），IR 前端 `irparse.c` 同样缺失（PB 线 parser.c 已实现，属分支漂移）。补全方式与 PB 一致：`_` 分支提前返回，A 后端 emits `if (1)` 恒真、IR 前端跳过比较与 JZ，块体仍走原有 `{ ... }` 入口；非块形式报错。`tests/pos/pattern.nc` 新增 `is _` 用例并断言输出（`wild ok` / `pattern ok`），c/native/ir-c/ir-native 四后端输出一致。属规范合规（文档已承诺该模式），允许在冻结线执行
- [ ] **PA-16 `is` 多子句 fallthrough 缺口（已登记，未修）**：BNF/中英文档规定多个 `is-clause`「首个匹配者执行、无 fallthrough」，但 A 后端当前为每个子句生成并列独立 `if`，条件重叠时多个子句都会执行；IR 前端同理（每子句独立比较+跳转，块末无 jmp 到合并出口）。修复需改动 `is` 控制流生成方式（子句块末补 jmp 出口或改判为 else-if 链），会影响冻结线上既有语义与回归基线，**不纳入 v1.0.2**，待 ltree 决策后另立版本处理
- [x] **PA-17 共享文档 PA↔PB 双向同步（2026-09-19 完成）**：以 `merge-base 7d4c652` 对 9 个共享文档做 `git merge-file` 三方合并（17 处冲突），PA 侧 v1.0.1/v1.0.2 成果进 PB（PB `37a112a`）；PB 侧更新的 4 项回灌 PA——中英 §5.1 指针声明定案表述、BNF `<array-size>` 细目与动态数组写法说明、VERSIONING_ROADMAP 的 2.0 进展条目与 `codegen.c` 清单残留、IMPLEMENTATION_STATUS「2.0 线（PB）差异」节 + `pattern.nc` 覆盖修正；`docs/LEGACY_CODEGEN.md` 纳入共享集。`TODO-PA.md`/`TODO-PB.md` 为分支专属不同步
- [x] **PA-18 循环体外 `is` 前端守卫（2026-09-19 完成，v1.0.2）**：BNF §6 规定 `is` 仅配合 `while` 循环体，但 A 后端把 `TOK_IS` 挂在 `parse_statement` 通用语句分支，循环体外写 `is` 前端照收、错误延迟到 C 编译期由 tcc 报 `'__is_val' undeclared`（`do` 体内同样静默接受）。实现方式：`CompilerState` 加 `while_depth`，`TOK_WHILE` 体前递增、体后递减，`case TOK_IS` 在 `while_depth == 0` 时报 `'is' pattern match only valid inside while loop body` 并吞 token 防死循环；IR 前端 `is_val_vreg < 0` 守卫保持不变（两侧口径统一）。新增 `tests/err/is_outside_while.nc` + `.expect`；门禁 c/native 各 13P/0F/5S（原 12P + 本用例）。副带效应：`do` 体内的 `is` 在 A 后端也即时报错（原为 C 编译期未定义符号），与 `do` 不支持 `is` 的规范一致。属规范合规，允许在冻结线执行。待回灌 PB 线 A 后端（见 `IMPLEMENTATION_STATUS.md`「2.0 线（PB）差异」）
- [x] **PA-19 `do` 体内 `is` 在 IR 前端拒绝（2026-09-19 完成，v1.0.2）**：PA 线 `irparse.c` 的 `TOK_WHILE || TOK_DO` 分支无条件写 `is_val_vreg`（残留 `is_do` 变量算了不用），使 `do cond { is pat { ... } }` 在 ir-c / ir-native 下合法，与 BNF §6 / 中英 §6.1「`do` 不支持 `is`」相反。改为 `do` 分支置 `is_val_vreg = -1` 并删除残留 `(void)is_do`，体内 `is` 走 `is_val_vreg < 0` 守卫报错（错误文案去掉 `while/do` 中的 `/do`，与 A 后端 PA-18 文案一致）；等价于 PB-27.1 的 IR 侧修复。配套测试框架收紧：err 用例改为仅 `IR_ERR_SKIP`（`m2a`~`m2d` M2 四条）在 IR 后端跳过，其余 err 用例四后端均执行；新增 `tests/err/is_in_do_body.nc` + `.expect`。门禁 c/native 各 14P/0F/5S、ir-c/ir-native 各 3P/0F/12S，0 FAIL。属规范合规，允许在冻结线执行。同项续做（A3 决策）：A 后端 `case TOK_DO` 进入体前把 `while_depth` 清零、退出恢复，使嵌套于 `while` 内的 `do` 也即时拒绝 `is`（原会静默匹配外层 `__is_val`），与 IR 前端一致，`tests/err/is_in_do_body.nc` 增补该嵌套情形；并修正「`do` 不支持 `is`」的理由句——BNF §6 与中英 §6.1 原写「因为 `do` 先执行块再判断条件」，与实现相反（本语言 `do` 与 `while` 同为前测循环），改为陈述为规范规定（`is` 只绑定 `while`，`do` 是否纳入留待 2.0 定案）
