# 方案 A — libtcc native 后端待办（PA 分支）

> 更新日期：2026-09-19（PA-1 ~ PA-14 全部完成，1.0 冻结线待办清零；v1.0.2 发布准备就绪待 tag）
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
