# NihaoC 版本路线图：1.0（A 方案产品线）+ 2.0（B 方案演进线）

> 制定日期：2026-08-30 ｜ 最近更新：2026-09-20（一致性核对 C6b／PA-41：双仓库流程三处加废止注、PB 基线计数绑定制定时日期）
> 决策：保留双路线——**A 方案 = 对外可用 1.0**（产品化，先发布）；**B 方案 = 下一代 2.0**（持续演进）。
> 关联：`docs/PB24_DECISION.md`（三路线评估）、`docs/STAGE_SUMMARY.md`（§3.1 IR 覆盖清零）、`docs/GIT_CONVENTIONS.md`（分支约定，本文 §3 为其延伸）。

---

## 0. 战略定位

| 线 | 方案 | 定位 | 时间线 |
| ---- | ---- | ---- | ---- |
| **1.0** | A（parser.c → C 文本 → tcc） | 对外可用产品：功能全、够简单、靠 tcc 全平台 | 当前 → 发布 |
| **2.0** | B（irparse.c → IR → 多后端） | 下一代：平台中立 IR、多架构汇编、可优化 | 持续演进 → 就绪后接管 |

两条线**共享**：lexer.c / token.h / ncc.h / sym.c / type.c / vis.c / stdlib.c / linker.c / module.c 与测试框架（xmake.lua）。
两条线**独占**：A=parser.c/cgen.c/native.c；B=irparse.c/ir.c/ir_to_c.c/ir_backend.c/ir_riscv64.c/ir_loongarch64.c（`PB` 分支 2026-09-26 起删去 ir_x86_64.c / ir_arm64.c，见 TODO-PB 的 PB-34；`PA` 分支两文件仍在）。

---

## 1. A 方案 1.0 发布清单

### 1.1 功能闭环（编译正确性）

- [x] **指针声明语法决策 + 实现**（8/19 PA 分支定案并落地，1.0.x 收敛为**隐式推断声明**）：`p = &x` 自动推断为指向 x 的指针类型；具名指针 `T*` 显式声明已移除（parse_type 的 `*` 指针构造分支删除），解引用统一为 `.()` / `. (T)` / `->`。同时修复 infer_init_type 契约（`Name = expr` 推断此前被 `=` 卡住恒落 int32，`s = "hello"` 曾误推 int）与 IR 层 `->` 成员偏移缺 ×8 的隐藏 bug。A 方案 `->` 链式/复合赋值全部可用
- [x] **全量语法回归盘点**（8/30）：38 个 pos 用例 A 方案可编译 **32/38**——IR_SUBSET 24 例全部可编译，不可编译 6 例均为因子集语法（切片/多返回/编译期等 IR-only 特性）
- [x] **A 方案已知 bug 清零**（8/30）：c/native 全量回归 **12P / 0F / 5S**（ir_arrow 转正后 6S→5S）；交叉验证期间暴露并修复 IR 层 `->` 成员偏移缺 ×8 的隐藏 bug（四后端输出 md5 一致）
- [x] **动态数组暂缓确认**（8/30）：`[N...]` 固定容量不自动增长、`[...]` 仅声明/索引语义已写入 BNF.md/Chinese.md/English.md；增长留 2.0

### 1.2 工程质量

- [x] **CLI 完善收尾**（8/30）：`debug` 子命令 A 方案视角核查通过（`--ir` 正常输出 IR 视图）；`-run` Windows 报错文案明确；错误消息"外壳中文 + 正文英文"符合 1.0 定位
- [x] **构建单一入口确认**（8/30）：xmake 唯一构建入口；Makefile 标 LEGACY 且 test 目标已删除，21 个旧语法用例（`const main()` 等）随 test/ 目录清理（git rm）
- [x] **P3 卫生**（8/30）：test/ 生成二进制（a.out / a.out.c）清理；codegen.c（507 行）死代码面评估——`parse_function_full`/`parse_statement_full`/`gen_function_prologue_full`/`gen_if_statement`/`gen_while_loop` 无外部调用，`type_check_statement` 仅被 `parse_function_full` 引用，全文件无对外入口（现役路径为 main → cgen.c/native.c）；与 cgen.c 职责边界：codegen=遗留全量生成器、cgen=现役生成器，**1.0 不重构，仅记录**
- [x] **P3 卫生落实**（9/1）：上述 codegen.c 死代码面已落地清理（-980 行，见 docs/LEGACY_CODEGEN.md）；A 方案独占 = parser.c/cgen.c/native.c
- [x] **linker.c 职责注释**（9/1）：现仅承担 `link` 库声明收集（linker_init/link_add_library），generate_* 已随 codegen 链删除，头部注释更新

### 1.3 发布准备

- [x] **examples/ 示例集**（8/30）：7 例已建（hello/fib/struct/pointer/string/cooking/multiret）+ README 对照表；**6 例 1.0 可编译验证通过**（c/native 编译+运行）；期间暴露并修复 A 方案 `p.()` 解引用类型 bug（此前硬编码 `(*(void**)p)` → 现按符号指针 ref 输出 `(*(int32_t*)p)`）；06_cooking 标注 2.0 预览（IR_ONLY 子集语法）
- [x] **README 更新**（8/31）：安装小节（xmake + tcc 依赖探测与获取）、CLI 命令一览表、后端表标注 1.0/2.0 范围（c/native = 1.0 正式支持；ir-* = 2.0 预览）、-run Linux only 说明
- [x] **语言规格冻结**（8/31）：BNF v2.0 终校完成——补 `=>`（TOK_FAT_ARROW）词法、`->` 指针成员访问 postfix 规则、`T*` 具名指针 pointer-type 规则；Chinese/English 补指针声明双支持（隐式推断 + 显式声明）；语法元素表核对通过（`=>`/`->` 已含）；multireturn 无残留。**注**：1.0.x 起 `T*` 具名指针声明与一元 `*` 解引用已移除（解引用统一 `.()`/`.(T)`/`->`）；2.0 起 `=>` 单语句匹配形式已移除（PA-13 / PB-27.6，BNF v2.2），`=>` 保留词法但语法不使用；`->` 保留
- [x] **版本与发布**（8/31）：CHANGELOG.md 建立（M0→M4→1.0 里程碑条目）；`v1.0.0` tag 当时仅本地创建（后已推送，台账见 §3.2）
- [x] **Linux 实测（PA-9，2026-08-31 WSL Ubuntu-24.04 完成）**：`-run` 内存执行修复（tcc_relocate(NULL) 语义误判→直接 tcc_run）、libtcc.so `-Bsymbolic` 重建（符号插值劫持）、SysV 调用约定（x87 浮点）；c/native 各 12P/0F/5S + examples 6/6 双后端一致——**Windows + Linux 双平台验证通过**（1.0 范围已含 Linux）

### 1.4 明确留给 2.0（1.0 不做）

- 动态数组增长（ptr+len+cap 堆结构）
- 切片运行时边界长度
- 命名空间（无语言定义，待议）
- IR 相关一切（ir-c/ir-native/多架构后端、IR 用例转正）
- native 寄存器分配（性能项）

---

## 2. B 方案 2.0 演进清单

### 阶段 1：类型化指针模型（✅ 2026-08-31 完成，PB 分支）

- [x] `p = &标量` 记录类型（pt[] 表扩展：`PT_SCALAR(-2-code)` 标量编码，聚合索引 >=0 / 标量 < -1 / -1 非指针三分区）
- [x] 指针算术边界（p+1 语义：×8 槽宽缩放，与数组寻址一致；链式 p+k+1 保持字节语义——NihaoC 无 C 连续指针算术，语义已文档化）
- [x] ir_arrow.nc 从 IR_ONLY 转正为 IR_SUBSET（8/19 PA 分支：A 方案指针声明落地 + IR 层 `->` 偏移 ×8 修复后，四后端一致）
- [x] 类型化解引用：`.() = e` 写（按指向类型 coerce/TRUNC）、`.() op= e` 复合（LOAD→类型协调→op→截断→STORE）、`.() read` 浮点标记（f64/f32 → double vreg）；一元 `*p` 已于 2026-09-04 收敛移除（等价能力由 `.()` 提供）——新用例 ir_ptr2.nc（IR_SUBSET）四后端一致，WSL Linux 实测输出一致

### 阶段 2：产品能力补齐（A 方案能力平移，P0-P1）

- [x] **M2 静态检查移植（2026-09-04 完成，PB-26）**：所有权/借用状态机移植进 IR 层（irparse.c），参数前缀 `flow/var/const/static` 记录 `vvis`；调用点 M2 所有权检查生效，`err/m2a..m2e` 在 ir-c/ir-native 双后端转正为 PASS（详见 TODO.md PB-26）
- [ ] **link/use 跨文件**（module.c 语义接入 IR 前端，单文件模型 → 模块化）
- [ ] **布局内置函数**（structof/unionof/holdof/bitoffsetof——需真实内存布局替代 8 字节槽模型）

### 阶段 3：质量与性能（P1-P2）

- [x] **ir-c 输出质量：变量名保留（2026-09-05 完成，PB 阶段3 C 项）**：`IrFn` 新增 `vreg_name` 表（ir.h），`ir_new_vreg` 同步增长（ir.c），`var_declare` 在基 vreg 写入源码名（irparse.c，变量与参数均经此登记）；`ir_to_c` 构建 `vrid` 标识符表（有名用源码名、无名用 `tN`），局部变量声明与全部指令操作数引用均输出可读名。`xmake test --all` 全矩阵 0 FAIL、跨后端一致性一致。**结构体直出（真实结构体类型代替平铺 8 字节槽）依赖阶段 2 B 项「真实内存布局」，留待 B 完成后补**——届时 ir_to_c 可直出 `struct T { ... }` 并复用真实偏移。
- [ ] **native 寄存器分配**（PB-15 决策的长期项：全栈槽保底已敲定，live range/spill 参考 LLVM RegAllocGreedy）
- [ ] 汇编后端转正评估：riscv64/loongarch64 从"仅验汇编生成"→ 真编译（需交叉工具链环境；arm64 发射器已于 2026-09-27 随 PB-34 删除）。**2026-09-27 已推进半格（PB-35）**：本机用 msys2 `riscv64-unknown-elf-as` 与 zig 0.15.2 的 LoongArch MC 把两档全部产物逐份过了一遍——**汇编 46/46、静态链接 45/46**（唯一失败是跨模块符号 `use_mod`），并据此修掉 loongarch64 发射器一整套非法语法与两档共有的大帧越界；**剩下的那一格是执行**：本机只有 `qemu-system-*`、无 `qemu-user`，也没有 loongarch64 的 GNU 工具链 / libc，故「汇编后端转正」仍不能勾。


### 阶段 4：新语言特性（2.0 独占，P2）

- [ ] 动态数组增长（ptr+len+cap 堆结构，等 1.0 发布后设计）
- [ ] 切片运行时边界长度（同需切片二元组结构）
- [ ] 命名空间（若 1.0 期间语言定义确立）
- [ ] 优化通道（常量折叠/死代码消除，IR 层天然适合）

---

## 3. 分支管理计划

### 3.1 分支语义（GIT_CONVENTIONS.md 更新版）

| 分支 | 职责 | 变更规则 |
| ---- | ---- | ---- |
| `main` | **稳定发布线**：1.x 累积成果，始终可构建可测试可发布 | 仅接受 PA 合入（1.0/1.x）；PB 在 2.0 就绪前不合入 |
| `PA` | **1.0 产品线**：特性**冻结**，只修 bug/文档/发布准备 | 直接提交；功能类改动需评估是否推迟到 2.0 |
| `PB` | **2.0 演进线**：IR 前端/后端持续开发 | 直接提交（或 feature/* → PB）；不抢 PA 发布节奏 |

### 3.2 版本化

```
v1.0.0  = PA 通过发布门禁 → 合入 main → tag（首个对外版本）
v1.0.x  = main 上 hotfix（bug 修复，PA 分支修 → 合 main → tag）
v2.0.0  = PB 就绪（阶段 3 达标）→ 合入 main → tag（届时 main 语义切换）
```

- tag 命名：`v<major>.<minor>.<patch>`，提交信息带 `release:` 前缀
- **tag 形态：统一使用附注 tag（`git tag -a`，消息写门禁基线与版本范围）**。自 `v1.0.2`（PA-47，2026-09-21）起生效；`v1.0.1` 是历史上的轻量 tag，不回改。落点约定：打在发布线（1.x 为 `PA`）已验证的 tip 上，随后 `PA` 合入 `main` 后该提交从 `main` 可达（核对命令 `git merge-base --is-ancestor <tag> main`）。
- **版本号单一真源**：`ncc/xmake.lua` 顶部的 `nihao_version` 常量——`set_version(nihao_version)` 与 `add_defines("NIHAO_VERSION_TAG=" .. nihao_version)` 同源于此，`ncc.h` 的 `NIHAO_VERSION` 由注入值字符串化得到，因此 `ncc --version`、帮助横幅、verbose 启动行与 `nihao init` 生成的 `nihao.toml` `[project] version` 全部等于构建时的那一处。**发布上抬版本号只改这一处**，不再手工同步头文件常量。不经 xmake 构建（LEGACY `Makefile`，已标注弃用）时无注入，上述输出为占位 `0.0.0-unknown`，不代表任何发布口径。（跨分支发布规则：某条线尚未接入构建注入时，按本条回灌。）
- 1.x 期间 main 与 PA 保持同步（PA 是开发源，main 是发布镜像）
- tag 台账：`v1.0.0` → `2036fba`（2026-08-31，annotated）、`v1.0.1` → `75c59cc`（2026-09-03，lightweight）、`v1.0.2` → `18f9c77`（2026-09-21 建本地 annotated tag，**2026-09-24 经 ltree 单独授权推送**；推送门禁见 `GIT_CONVENTIONS.md` §3.1）。`v1.0.2` 内容见 `CHANGELOG.md` 与 `GIT_CONVENTIONS.md` §五；tag 指向 `PA` 的 PA-46 提交，因为该提交与门禁复跑提交（PA-44 `6b69cfb`）在 `ncc/` 与 `examples/` 下逐字节相同（核对命令 `git diff --stat 6b69cfb v1.0.2^{commit} -- ncc/ examples/` 应为空），其后的提交仅动文档。

### 3.3 工作流

```
PA 开发：  feature/*（或直接）→ PA 验证（c/native 0 FAIL）→ 合 main → tag
PB 开发：  直接 PB 提交 → IR 双后端/四后端一致性验证（56 PASS 0 FAIL，2026-08-30 制定时用例集）
hotfix：   PA 分支修 bug → 合 main（tag v1.0.x）→ 同步共享文件到 PB
```

**共享文件同步规则**（lexer/token/sym/type/vis/stdlib/module/linker/xmake.lua 测试）：
- 语法/词法演进：**先 PB 验证（IR 一致性防线更严）→ 再 PA 移植**（`=>` 即此流程先例）
- bug 修复：**先 PA（1.0 优先）→ 再同步 PB**（或双线同改，提交信息注明对应）
- 同步手段：现有 `cp + 双仓库双提交` 流程（ncc 活跃仓库 ↔ NihaoC）—— **【2026-09-20 废止】**（PA-38 定案、PA-40 从 `GIT_CONVENTIONS.md` §1 撤下）：该外部活跃仓库流程已停用，两条线现由同一仓库的 `PA` / `PB` 分支承载，共享文档跨分支用 `git merge-file` 三方合并同步；现行权威规则见 `GIT_CONVENTIONS.md` §1

### 3.4 双仓库映射

> **【2026-09-20 本节整体废止，原文保留作历史记录】** 外部活跃仓库 `ncc`（分支 `feat/backend-ir`）与 `NihaoC/ncc/` 发布副本的 `cp + 双提交` 映射已停用（PA-38 定案、PA-40 入规则文档）。现行做法：`PA` / `PB` 两条线均在 `NihaoC` 单仓库内演进，共享文档跨分支以 `git merge-file` 三方合并（对 `merge-base 7d4c652`）同步，推送与 tag 由所有者逐次授权（`GIT_CONVENTIONS.md` §3.1）。下表与下两条工作流仅描述本文件制定时（2026-08-30）的状态。

| 仓库 | 分支 | 角色 |
| ---- | ---- | ---- |
| NihaoC（发布副本） | main / PA / PB | 三分支齐全；PA 开发与 1.0 发布在此 |
| ncc（活跃开发副本） | feat/backend-ir | **PB 线活跃开发**（对应 NihaoC/PB） |

- **1.0 期间新增工作流**（已废止）：PA 的共享文件改动（lexer 等）需 cp 到 ncc 活跃仓库保持同步（防 2.0 基线漂移）
- PB 线活跃开发继续走 ncc（feat/backend-ir），周期性 cp 回 NihaoC/PB 提交（已废止）

### 3.5 发布门禁（合 main 条件）

| 线 | 门禁 |
| ---- | ---- |
| PA → main | `xmake test -b c` / `-b native` 全量 0 FAIL；examples 全跑通；README/CHANGELOG 更新；规格文档冻结 |
| PB → main | 全矩阵 56 PASS / 0 FAIL（2026-08-30 制定时用例集，或合入时当时基线）；ir-c 输出质量达标；M2/link/布局函数补齐 |

### 3.6 风险与对策

| 风险 | 对策 |
| ---- | ---- |
| 双线共享文件漂移（如 lexer 各自改了 token） | 共享文件改动必须双仓库同步提交；提交信息互相注明（同步手段已改为单仓库分支 + `git merge-file` 三方合并，见 §3.4 废止注） |
| PA 1.0 发布被 PB 需求干扰 | 1.0 特性冻结规则；PB 新语法先验证不反哺 PA（除非是 bug） |
| 2.0 遥遥无期导致 PB 价值质疑 | 阶段 2（能力平移）设里程碑检查点；native 后端以"可跑真实程序"为 2.0 就绪 KPI |
| 指针语义 A/B 分歧 | 1.0 决策的指针声明语义写入 BNF，PB 阶段 1 必须对齐（语言只有一个语义） |

---

## 4. 决策点（已全部确认）

1. **1.0 指针声明语法**：✅ 已定案（8/19 PA 分支，1.0.x 收敛为**隐式推断声明**）——`p = &x` 自动推断为指向 x 的指针类型；具名指针 `T*` 显式声明已移除，解引用统一为 `.()` / `. (T)` / `->`
2. **1.0 发布范围**：✅ 已定（2026-08-31）——**包含 Linux 实测**：WSL Ubuntu-24.04 双平台验证通过（c/native 各 12P/0F/5S + examples 6/6 + `-run` 修复），见 PA-9；1.0 范围生效 Windows + Linux
3. **examples 优先级**：✅ 已定——hello/fib/struct/pointer/string/cooking/multiret 7 例已建（6 例 1.0 可编译运行，06_cooking 标注 2.0 预览）
4. **分支计划采纳**：✅ 已落账——`docs/GIT_CONVENTIONS.md` 分支语义与 §3 对齐（main/PA/PB 职责、版本化 v1.0.0/v1.0.x/v2.0.0、共享文件同步规则）；`[AI]` 提交标签与推送授权制已写入
---

## 5. 后端名单与 A 线退役裁定（2026-11-09 ltree 裁定；本节只记裁定与判据，代码未动）

**名单**：2.0 线只保留 IR 管线，后端收敛为六档——`ir-c`、`ir-native`、`ir-riscv64`、`ir-riscv32`、`ir-arm`、`ir-loongarch64`。
（**2026-11-12 ltree 裁定：档名一律写 CLI 实名**，原写的简称 `ir-rv64` / `ir-rv32` / `ir-loongarch` 已就地正名，见 §5.8。**代码未改名**。）
`c` / `native` 两档（A 线 `parser.c` → `cgen.c` / `native.c`）退出产品线。

### 5.1 四条配套裁定（同日）

1. **A 线节奏 = 先冻结、再删**。A 线立即停止发展（不再修缺陷、不再加特性），转为 **oracle-only**——它今天的价值是
   「IR 与 A 逐字节比对」这条唯一能证 IR 语义正确的判据（32 格 4 后端一致全靠它）。IR 追平（P2 判据达成）后在 P5
   物理删除并归档，先例是 2026-09-01 删 `codegen.c` 并写 `docs/LEGACY_CODEGEN.md`。
2. **`ir-riscv32` 数据模型 = 保持 8 字节槽、成对读写**（两条 `lw` / `sw` 完成一次 8 字节槽访问），不在这一档引入
   per-target 槽宽（那是 P3 另立的事）。
3. **`ir-arm` 选型（实现方定）= ARMv7E 基线的 Thumb-2 / AAPCS ILP32，浮点后置**。可核理由：Thumb-2 是 Cortex-A 与
   Cortex-M **唯一共存**的指令集（M 系列没有 A32）；`LDRD` / `STRD` 在 ARMv7E-M 可用，正好承接第 2 条的「8 字节槽 +
   成对读写」。**下限声明为 ARMv7E-M 及以上**（M3 / M4 / M7、A5 / A7 / A8 / A15），**不含 ARMv6-M（M0 / M0+）**——
   那一档既没有 Thumb-2 的 32 位编码也没有 `LDRD` / `STRD`，8 字节槽模型在那里不成立。浮点先不做：M4F / M7 只有
   单精度 D 寄存器子集，与 IR 现在的 double 通路不匹配。PA 现存的 `ir_arm64.c` 是 AArch64，与本选型不同码本，
   只能当写法参考，不能直接复用。
4. **版本叙事 = 1.x 冻结在 A 线**。已发布的 v1.0.x 语义（`c` / `native`）不改写，六档名单属 **2.0**；A 线物理删除
   落在 2.0 里程碑上。

### 5.2 这份名单的代价（今天实测，全部可复算）

| 面 | A 线 | IR 线 | 缺口 |
|---|---|---|---|
| pos 用例（72 份，循环内 71 份） | 68 份能编 | 44 份能编（`IR_SUBSET` 41 + `IR_ONLY` 3） | **28 份只有 A 能编** |
| err 诊断（33 份） | 33 份 | 7 份（`IR_ERR_COVERED`） | **26 份 IR 侧不诊断** |
| 机器码发射器 | — | riscv64、loongarch64 两档 | **rv32 全新**、**arm 全新**（x86-64 已随 PB-34 删除，不在新名单内） |
| 门禁矩阵 | `xmake test --all` 四档 | 同四档 | 六档落地后矩阵需重排；交叉档是否进门禁待定（待裁第 32 条附带） |

**工具链同日实测**：zig 0.15.2 能出 `riscv32-linux-musl` 静态 ELF、能编 `arm-freestanding-none` 与
`arm-linux-musleabihf`；WSL 有 `qemu-riscv32-static`、`qemu-armhf-static`（Linux-user 执行可用），
**但没有 `qemu-system-arm`，也没有 `arm-linux-gnueabihf-as`** ⇒ Cortex-M 那种 bare-metal 产物今天只能验到
「汇编 + 链接」两级，执行要么装 `qemu-system-arm`、要么上真板——这是 P4 里 arm 档的前置条件。

### 5.3 里程碑与判据（顺序不可随意换）

| 阶段 | 内容 | 退出判据 |
|---|---|---|
| P0 | 定名与契约：六档 ABI、槽宽、`ir-native` 在各宿主的语义、A 线冻结声明 | 本节 + `docs/BACKEND_MATRIX.md` 落地，README 2.0 段与之逐字一致 |
| P1 | 缺口台账：28 份 pos + 26 份 err 逐份定性（语法缺失 / 语义缺失 / 布局缺失 / 用例本身非法） | 每份归入某桶；`IR_SUBSET` 由「名单制」改「默认全进、例外登记」 |
| P2 | IR 补齐（最大一块）：真实聚合布局、`print` / `alias` / 布尔字面量 / 顶层变量声明等 IR 缺口、`flow` 与 auto-free 在 IR 落地、多维数组与切片（`sizeof(变量)` 先按第 31 条定口径再动） | pos IR 可编 **44 → 68**、err 诊断 **7 → 33**（达成才允许启动 P5） |
| P3 | 数据模型复核：8 字节槽在 rv32 / arm32 的成对读写落地、帧寻址 2047 护栏随 ABI 重算 | 同一份 pos 在 rv64 / rv32 / arm / la 四档出码并过汇编 + 链接 |
| P4 | 发射器：`ir-riscv64` / `ir-loongarch64` **保持现名**进门禁（改名子任务已由 §5.6 第③条取消）→ `ir-arm`（按 ARMv6-M 基础 Thumb 编到指令集，一次覆盖 M0 到 A 系，见 §5.6 / §5.7）→ `ir-riscv32`（新写） | 每档 S1–S5 五级都由版本化 `xmake probe` 出数，四档互比逐字节一致 |
| P5 | A 线退役：删 `parser.c` / `cgen.c` / `native.c` 并归档；门禁改 IR 单线；`IR_SUBSET` / `IR_ONLY` / `IR_ERR_COVERED` 三个名单作废 | 全部门禁在 IR 线跑绿，没有任何文档还把 `c` / `native` 当产品能力写 |
| P6 | 管理面：`xmake probe` 六档 + examples + 方言 / sanitizer 版本化，`BACKEND_MATRIX.md` 由脚本生成 | 每个进文档的数字都有点名入口，`build/` 里不留判据 |

**顺序不可换的三条理由**：尺子（A 线）必须在被量对象（IR 线）追平之后才能砸；rv32 / arm32 的成对读写依赖槽模型
定案；槽模型又依赖「真实聚合布局」这条欠账（待裁第 23 条）先解。

**受影响而暂不改写的既有条目**：A 线专属缺陷类待裁项（第 21 / 24 / 28 条等）从「要不要修 A」变为「IR 线要不要做
这件事」——措辞与账都不动，等 P1 台账时逐条重挂到新线上。


### 5.4 P1 已落账（2026-11-10，PB-61）

缺口台账落盘为 `docs/IR_GAP_LEDGER.md`（本文件只留结论，逐份明细在那份里）。P1 退出判据达成情况：**54 份缺口（28 pos + 15 err 真缺口 + 11 err 靠命名豁免进档）逐份归入四桶**，且桶内给出聚簇：G1 语法缺失 21 份聚成 4 簇、G2 语义缺失 5 份、G3 布局缺失 2 份；err 侧 E-a 3 / E-b 6 / E-c 6。**P2 的排期依据由此变成可数的东西**：11 份用例同挂「聚合体真实布局」一条欠账（待裁第 23 条），所以 P3 的那条定案实际挡在 P2 前半段之前——本节原先「顺序不可换」的第 3 条理由今天有实测支撑。另记一条门禁卫生：err 侧 IR 的真实参与数原是 18/33 而非名单写的 7/33，差额全靠 `stem:startswith("m2")` 这条命名豁免；本轮已把这 11 份显式列入 `IR_ERR_COVERED` 并删掉豁免，此后判据只认名单。

### 5.5 P2 已开第一簇（2026-11-10，PB-62）

按 5.4 的排期结论，P2 的第一刀没有从「补语法」开始，而是先收台账里的 **E-c 桶（非法程序被静默接受）**：IR 前端补齐与 A 后端同口径的三条字符串 / 定长数组初始化诊断，四份 err 转为有诊断且无产物，门禁 ir-c / ir-native 由 **28 份 PASS 升到 32 份**（0 FAIL，c / native 与比对档不变）。**E-c 6 份 → 2 份**，两份余量在台账里写明卡点（`len_known` 表、指针指向类型跟踪），不用命名豁免或放宽判据糊过去。下一簇按计划是 G1 的 `malloc(类型)` + `p.(T)`（6 份用例挂上面，且与 `void_subscript` 同根）。

### 5.6 第 32 条三问已裁（2026-11-10，ltree）

| 问 | 裁 | 落地口径 |
|---|---|---|
| ① Cortex-M0/M0+（ARMv6-M）要不要支持 | **要** | `ir-arm` 的**下限定在 ARMv6-M 的 16 位 Thumb 基础指令集**，一档同时覆盖 M0 / M0+ / M3 / M4 / M7 与 Cortex-A 的 Thumb 态，不做第二档位 |
| ② Cortex-M 的执行验证环境 | **授权装 `qemu-system-arm`** | **已装成（2026-11-10）**：首装退出码 100，根因是**先存的 dpkg 中断态**（半配置 `man-db`、`dbus` / `libc-bin` 触发未处理，与 qemu 无关）；经授权 `dpkg --configure -a` 修复后重装返回 0，版本 `1:8.2.2+ds-0ubuntu1.18`（与 qemu-user-static 8.2.2 同代），`-machine help` 匹配 microbit / stellaris / realview / vexpress 等 10 项 ⇒ M 系列 freestanding 可验到执行级 |
| ③ 后端改名时机 | **代码不改名** | CLI 保持 `ir-riscv64` / `ir-loongarch64`；本文件 §5 名单里的 `ir-rv64` / `ir-loongarch` **一律按简称读**，正式名以 CLI 实名为准；新档跟随现有风格叫 **`ir-riscv32`** 与 **`ir-arm`**，**不引入任何别名**；§5.3 里 P4 的「改名 + 兼容别名」一项取消 |

**对 §5.3 的两处影响**：P4 的 arm 子任务从「先 Linux-user 后 freestanding」改为「**按 ARMv6-M 基础
Thumb 编到指令集，一次覆盖 M0 到 A 系**」，软件除法与字面量池两条前置进 P3 一并复核；P4 的改名子任务取消（第③条）。其余阶段与判据不变。

**§5.1 第 3 条有一句今天作废**（就地声明，不回头改写原文）：我写的「ARMv6-M 没有 `LDRD`/`STRD`，所以 8 字节槽模型在那一档不成立」是错的——**错在默认 8 字节槽必须用双字载入指令访问**。两个 32 位 `ldr` / `str` 即可完成一次 8 字节槽读写，ARMv6-M 有这两条指令，所以「保持 8 字节槽 + 成对读写」这条裁定（§5.1 第 2 条）**对 M0 同样成立**，第 3 条的下限因此从 ARMv7E-M 降到 ARMv6-M。压到 ARMv6-M 换来的是四条实打实的约束，记在 §5.7 的发射器待办里：**软件除法**（无 `SDIV`/`UDIV`，`/` 与 `%` 需 helper）、**立即数只能走字面量池**（无 `MOVW`/`MOVT`）、**分支范围窄**（16 位 `B` 为 ±256，远距离需长跳）、**`LDR literal` 基址受限**与 `PUSH`/`POP` 寄存器表受限。

### 5.7 发射器待办（arm 下限降到 ARMv6-M 带进来的四件事，2026-11-10）

| 约束 | 具体含义 | 影响 |
|---|---|---|
| 软件除法 | ARMv6-M 无 `SDIV`/`UDIV`（M4/M7 才有）⇒ `/` 与 `%` 要么调 `__aeabi_idiv`/`uidiv` helper，要么内联循环除 | `ir-arm` |
| 立即数只能走字面量池 | 无 `MOVW`/`MOVT`，32 位常量必须 `LDR Rd,[PC,#imm]` ⇒ 每个大常量都要管池与对齐 | `ir-arm` |
| 分支范围窄 | 16 位 `B` 为 ±256、`BL` ±2048K ⇒ 远距离要长跳序列或 veneers，发射器要有分支距离检查 | `ir-arm` |
| 受限寻址与压栈表 | `LDR literal` 在 M0 只允许 R7 以下基址；`PUSH`/`POP` 寄存器表受限 ⇒ 不能照抄 RV64 的帧习惯 | `ir-arm` |

**共同前提**：8 字节槽在 32 位目标上按**两条 32 位 `ldr`/`str`** 访问（`ir-riscv32` 同理），这条已由 rv32 裁定覆盖，不需要为 M0 单开窄槽。执行验证前置已解除：`qemu-system-arm` `1:8.2.2+ds-0ubuntu1.18` 于 2026-11-10 装成（先 `dpkg --configure -a` 修先存中断态），`-machine help` 有 microbit / stellaris / realview / vexpress 等 10 项可选。

### 5.8 档名统一为 CLI 实名（2026-11-12 ltree 裁定；纯文档，代码未动）

**裁定**：名单档名与 CLI 实名**统一为 CLI 实名的版本**。即不再维护「文档写简称、读的时候在脑子里映射到实名」这套两层命名，
文档里凡指称一个后端档，一律写 CLI 接受的那个名字。

| 原简称 | 统一后（= CLI 实名） | 状态 |
|---|---|---|
| `ir-rv64` | **`ir-riscv64`** | 已实现，CLI 现名 |
| `ir-loongarch` | **`ir-loongarch64`** | 已实现，CLI 现名 |
| `ir-rv32` | **`ir-riscv32`** | 未开工，新档按此名落地 |
| `ir-arm` | `ir-arm`（不变） | 未开工，本就是实名 |
| `ir-c` / `ir-native` | 不变 | 已实现 |

**与 §5.6 第③条的关系**：那条裁定的是「**代码不改名**」，这一条**完全保留、继续有效**——CLI 仍只接受
`ir-riscv64` / `ir-loongarch64`，不引入任何别名。变的只是**文档写法**：§5.6 第③条里「名单里的简称一律按简称读」
这个**读法**自本次起作废，因为名单里已经没有简称了。**§5.6 与 §5.1 的裁定原文不改写**（历史记录），
本节即其带日期的续记；`docs/TODO-PB.md` 里 2026-11-09 / 11-10 两条 dated 条目同样原文保留。

**本轮就地正名的三处**（只改名字拼写，裁定内容与语义一字未动）：
- §5 名单那一行（六档列举）
- §5.1 第 2 条（`ir-riscv32` 数据模型 = 保持 8 字节槽、成对读写）
- §5.3 的 P4 里程碑（同时把已由 §5.6 取消的「改名 + 兼容别名」子任务从表述里去掉，arm 子任务口径同步到 ARMv6-M）

**未改的简称**：正文里 `rv32` / `rv64` / `la` 这类**架构口语缩写**不是档名，保持原样（例如「rv32 / arm32 的成对读写」）。
判据是**有没有反引号包成一个后端名**——`ir-*` 形态的才正名。

**顺带记一条现值口径**（与本次裁定同轮量出，明细在 `docs/IR_GAP_LEDGER.md` §0 / §6）：§5.2 那张代价表是
**2026-11-09 的快照**，按「历史快照不改写」保留原文；今天的现值是 **pos IR 能编 57 / 72（缺口 15）、err IR 覆盖 22 / 33（缺口 11）**。
§5.3 里 P2 的退出判据「pos IR 可编 44 → 68」因此需要重述，**但改判据带设计权重，本轮只登记不擅改**（见台账 §6 末条）。
