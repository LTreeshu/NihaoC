# NihaoC 版本路线图：1.0（A 方案产品线）+ 2.0（B 方案演进线）

> 制定日期：2026-08-30
> 决策：保留双路线——**A 方案 = 对外可用 1.0**（产品化，先发布）；**B 方案 = 下一代 2.0**（持续演进）。
> 关联：`docs/PB24_DECISION.md`（三路线评估）、`docs/STAGE_SUMMARY.md`（§3.1 IR 覆盖清零）、`docs/GIT_CONVENTIONS.md`（分支约定，本文 §3 为其延伸）。

---

## 0. 战略定位

| 线 | 方案 | 定位 | 时间线 |
| ---- | ---- | ---- | ---- |
| **1.0** | A（parser.c → C 文本 → tcc） | 对外可用产品：功能全、够简单、靠 tcc 全平台 | 当前 → 发布 |
| **2.0** | B（irparse.c → IR → 多后端） | 下一代：平台中立 IR、多架构汇编、可优化 | 持续演进 → 就绪后接管 |

两条线**共享**：lexer.c / token.h / ncc.h / sym.c / type.c / vis.c / stdlib.c / linker.c / module.c 与测试框架（xmake.lua）。
两条线**独占**：A=parser.c/codegen.c/cgen.c/native.c；B=irparse.c/ir.c/ir_to_c.c/ir_backend.c/ir_x86_64.c/ir_riscv64.c/ir_arm64.c/ir_loongarch64.c。

---

## 1. A 方案 1.0 发布清单

### 1.1 功能闭环（编译正确性）

- [x] **指针声明语法决策 + 实现**（8/19 PA 分支定案并落地）：**隐式推断 + 显式声明双支持**——`p = &x` 自动推断为指向 x 的指针类型；`p T* = &x` 显式声明亦合法（parse_type 原有 `*` 支持 + cgen TYPE_POINTER 输出具名指针 `T*`）。同时修复 infer_init_type 契约（`Name = expr` 推断此前被 `=` 卡住恒落 int32，`s = "hello"` 曾误推 int）与 IR 层 `->` 成员偏移缺 ×8 的隐藏 bug。A 方案 `->` 链式/复合赋值全部可用
- [x] **全量语法回归盘点**（8/30）：38 个 pos 用例 A 方案可编译 **32/38**——IR_SUBSET 24 例全部可编译，不可编译 6 例均为因子集语法（切片/多返回/编译期等 IR-only 特性）
- [x] **A 方案已知 bug 清零**（8/30）：c/native 全量回归 **12P / 0F / 5S**（ir_arrow 转正后 6S→5S）；交叉验证期间暴露并修复 IR 层 `->` 成员偏移缺 ×8 的隐藏 bug（四后端输出 md5 一致）
- [x] **动态数组暂缓确认**（8/30）：`[N...]` 固定容量不自动增长、`[...]` 仅声明/索引语义已写入 BNF.md/Chinese.md/English.md；增长留 2.0

### 1.2 工程质量

- [x] **CLI 完善收尾**（8/30）：`debug` 子命令 A 方案视角核查通过（`--ir` 正常输出 IR 视图）；`-run` Windows 报错文案明确；错误消息"外壳中文 + 正文英文"符合 1.0 定位
- [x] **构建单一入口确认**（8/30）：xmake 唯一构建入口；Makefile 标 LEGACY 且 test 目标已删除，21 个旧语法用例（`const main()` 等）随 test/ 目录清理（git rm）
- [x] **P3 卫生**（8/30）：test/ 生成二进制（a.out / a.out.c）清理；codegen.c（507 行）死代码面评估——`parse_function_full`/`parse_statement_full`/`gen_function_prologue_full`/`gen_if_statement`/`gen_while_loop` 无外部调用，`type_check_statement` 仅被 `parse_function_full` 引用，全文件无对外入口（现役路径为 main → cgen.c/native.c）；与 cgen.c 职责边界：codegen=遗留全量生成器、cgen=现役生成器，**1.0 不重构，仅记录**
- [ ] **linker.c 职责注释**（当前仅 default 后端使用，1.0 文档化即可）

### 1.3 发布准备

- [x] **examples/ 示例集**（8/30）：7 例已建（hello/fib/struct/pointer/string/cooking/multiret）+ README 对照表；**6 例 1.0 可编译验证通过**（c/native 编译+运行）；期间暴露并修复 A 方案 `p.()` 解引用类型 bug（此前硬编码 `(*(void**)p)` → 现按符号指针 ref 输出 `(*(int32_t*)p)`）；06_cooking 标注 2.0 预览（IR_ONLY 子集语法）
- [ ] **README 更新**：安装/构建（xmake + tcc 依赖探测）、CLI 用法、后端表（c/native 为 1.0 范围；ir-* 标注"2.0 预览"）、-run Linux only 说明
- [ ] **语言规格冻结**：BNF v2.0 终校（含 8/19-8/30 新语法：goto label/len/cooking 函数/嵌套/f32/`=>`/`->`/三元）+ 中英语法元素表核对
- [ ] **版本与发布**：CHANGELOG.md 建立（M0→M4→1.0 里程碑条目）；`v1.0.0` tag 流程（见 §3）
- [ ] **Linux 实测（PA-9，环境就绪时）**：-run、libtcc.so、SysV——无环境则文档标注"Windows 已验证"

### 1.4 明确留给 2.0（1.0 不做）

- 动态数组增长（ptr+len+cap 堆结构）
- 切片运行时边界长度
- 命名空间（无语言定义，待议）
- IR 相关一切（ir-c/ir-native/多架构后端、IR 用例转正）
- native 寄存器分配（性能项）

---

## 2. B 方案 2.0 演进清单

### 阶段 1：类型化指针模型（立项，~100 行，P0）

- [ ] `p = &标量` 记录类型（pt[] 表扩展到标量，或引入完整指针类型表）
- [ ] 指针算术边界（p+1 语义？1.0 决策若补了 A 方案指针声明，语义以此为准）
- [x] ir_arrow.nc 从 IR_ONLY 转正为 IR_SUBSET（8/19 PA 分支：A 方案指针声明落地 + IR 层 `->` 偏移 ×8 修复后，四后端一致）

### 阶段 2：产品能力补齐（A 方案能力平移，P0-P1）

- [ ] **M2 静态检查移植**（所有权/借用状态机——A 方案早期实现，2.0 重构进 IR 层；err 测试从 SKIP 转正）
- [ ] **link/use 跨文件**（module.c 语义接入 IR 前端，单文件模型 → 模块化）
- [ ] **布局内置函数**（structof/unionof/holdof/bitoffsetof——需真实内存布局替代 8 字节槽模型）

### 阶段 3：质量与性能（P1-P2）

- [ ] **ir-c 输出质量**：平铺槽 `tN` 风格 → 可读 C（变量名保留/结构体直出）——2.0 产品化最大风险点
- [ ] **native 寄存器分配**（PB-15 决策的长期项：全栈槽保底已敲定，live range/spill 参考 LLVM RegAllocGreedy）
- [ ] 汇编后端转正评估：riscv64/arm64/loongarch64 从"仅验汇编生成"→ 真编译（需交叉工具链环境）

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
- 1.x 期间 main 与 PA 保持同步（PA 是开发源，main 是发布镜像）

### 3.3 工作流

```
PA 开发：  feature/*（或直接）→ PA 验证（c/native 0 FAIL）→ 合 main → tag
PB 开发：  直接 PB 提交 → IR 双后端/四后端一致性验证（56 PASS 0 FAIL 基线）
hotfix：   PA 分支修 bug → 合 main（tag v1.0.x）→ 同步共享文件到 PB
```

**共享文件同步规则**（lexer/token/sym/type/vis/stdlib/module/linker/xmake.lua 测试）：
- 语法/词法演进：**先 PB 验证（IR 一致性防线更严）→ 再 PA 移植**（`=>` 即此流程先例）
- bug 修复：**先 PA（1.0 优先）→ 再同步 PB**（或双线同改，提交信息注明对应）
- 同步手段：现有 `cp + 双仓库双提交` 流程（ncc 活跃仓库 ↔ NihaoC）

### 3.4 双仓库映射

| 仓库 | 分支 | 角色 |
| ---- | ---- | ---- |
| NihaoC（发布副本） | main / PA / PB | 三分支齐全；PA 开发与 1.0 发布在此 |
| ncc（活跃开发副本） | feat/backend-ir | **PB 线活跃开发**（对应 NihaoC/PB） |

- **1.0 期间新增工作流**：PA 的共享文件改动（lexer 等）需 cp 到 ncc 活跃仓库保持同步（防 2.0 基线漂移）
- PB 线活跃开发继续走 ncc（feat/backend-ir），周期性 cp 回 NihaoC/PB 提交

### 3.5 发布门禁（合 main 条件）

| 线 | 门禁 |
| ---- | ---- |
| PA → main | `xmake test -b c` / `-b native` 全量 0 FAIL；examples 全跑通；README/CHANGELOG 更新；规格文档冻结 |
| PB → main | 全矩阵 56 PASS / 0 FAIL（或当时基线）；ir-c 输出质量达标；M2/link/布局函数补齐 |

### 3.6 风险与对策

| 风险 | 对策 |
| ---- | ---- |
| 双线共享文件漂移（如 lexer 各自改了 token） | 共享文件改动必须双仓库同步提交；提交信息互相注明 |
| PA 1.0 发布被 PB 需求干扰 | 1.0 特性冻结规则；PB 新语法先验证不反哺 PA（除非是 bug） |
| 2.0 遥遥无期导致 PB 价值质疑 | 阶段 2（能力平移）设里程碑检查点；native 后端以"可跑真实程序"为 2.0 就绪 KPI |
| 指针语义 A/B 分歧 | 1.0 决策的指针声明语义写入 BNF，PB 阶段 1 必须对齐（语言只有一个语义） |

---

## 4. 待 ltree 确认的决策点

1. **1.0 指针声明语法**：✅ 已定案（8/19 PA 分支）——**隐式推断 + 显式声明双支持**（`p = &x` 自动推断；`p T* = &x` 显式亦合法），BNF 终校时按此写入
2. **1.0 发布范围**：是否包含 Linux 实测（无环境则 Windows-only 标注）？
3. **examples 优先级**：先做哪 3-5 个示例？（建议 hello/fib/struct+指针/字符串/编译期）
4. **分支计划采纳**：§3 分支语义与版本化是否照此执行（需更新 GIT_CONVENTIONS.md）？
