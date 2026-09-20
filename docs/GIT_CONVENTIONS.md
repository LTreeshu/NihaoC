# Git 提交与推送约定 | Git Commit & Push Conventions

> 本文档定义 NihaoC 仓库的 Git 提交、推送与分支协作规则。
> 适用对象：所有开发者（含 AI 协作代理）。

---

## 一、分支策略 | Branch Strategy

仓库维护三条长期分支，各司其职（版本语义见 `docs/VERSIONING_ROADMAP.md`）：

| 分支 | 职责 |
| ---- | ---- |
| `main` | 主分支 / **稳定发布线**。1.x 累积成果，始终可构建、可测试、可发布；仅接受 `PA` 合入（2.0 就绪前 `PB` 不合入） |
| `PA`  | 方案 A：`parser → C 文本` → 外部 tcc / libtcc（native）后端的**产品化路径（1.0）**。特性冻结，只修 bug / 文档 / 发布准备 |
| `PB`  | 方案 B：`irparse → IR（三地址码）` → C / 多架构汇编（x86-64、riscv64、arm64、loongarch64）的**下一代演进线（2.0）**，持续开发 |

### 分支规则
- 三条分支于 2026-08-29 从同一稳定点分出（当时 `main` 为 `04f1435`）；此后 `main` 随 `PA` 合入继续前进，`PA` 与 `PB` 的实际共同祖先为 `7d4c652`（2026-08-31）。
- 功能/修复先落在对应的 `PA` 或 `PB` 分支，验证通过后再视需要合回 `main`。
- 新增命名分支时遵循 `feature/<name>`、`fix/<name>`、`docs/<name>` 命名；以 `PA`/`PB` 为主分支的工作，直接提交到对应分支即可。
- 分支与 `origin` 保持一致：新建分支后建立上游跟踪（`git push -u origin <branch>` 或 `git branch --set-upstream-to`）——**推送本身受 §3.1 门禁约束，需所有者授权**；无网络或未获授权时，用本地 `git config branch.<name>.remote/merge` 建立跟踪即可。

### 版本化与发布
- `v1.0.0`：`PA` 通过发布门禁（c/native 全量 0 FAIL + examples 全跑通）→ 合入 `main` → tag `v<major>.<minor>.<patch>`（`release:` 前缀）。
- `v1.0.x`：hotfix 走 `PA` 分支修复 → 合 `main` → tag。
- **发布前必查版本号**：编译器版本只写在 `ncc/xmake.lua` 顶部的 `nihao_version` 一处（`set_version` 与注入 C 的 `NIHAO_VERSION` 都由它派生，`--version`／帮助横幅／verbose 启动行／`nihao.toml` 脚手架版本全部取该值），上抬版本时改这一处并与 tag 一致（口径见 `VERSIONING_ROADMAP.md` §3.2）。
- `v2.0.0`：`PB` 阶段 2（能力平移）达标、全矩阵 0 FAIL → 合入 `main` → tag。
- **共享文档的测试计数写法**（一致性核对 PA-36/PA-38/PA-39 定案，防再次漂移）：共享层文档（`docs/ROADMAP.md` 里程碑表、`README.md`、`docs/STAGE_SUMMARY.md` 等）只写**不随用例增删变化的不变量**（如「全矩阵 0 FAIL」），且必须绑定版本号或日期；某分支某时点的详细 `PASS / FAIL / SKIP` 计数只写在该分支专属 `TODO-PA.md` / `TODO-PB.md` 与 `CHANGELOG.md` 对应版本段。快照类文档须在开头标明快照日期、给出权威指针，不得以现在时陈述现状。
- 共享文件（lexer/token/sym/type/vis/stdlib/module/linker 等）改动：语法演进**先 `PB` 验证 → 再 `PA` 移植**；bug 修复**先 `PA` → 再同步 `PB`**；双仓库（`NihaoC/ncc/` 发布副本 ↔ 独立活跃仓库 `ncc`）成对提交流程**已废止**（PA-38 定案，见 `docs/STAGE_SUMMARY.md` §1），两条线现由同一仓库的 `PA` / `PB` 分支承载，共享文档跨分支用 `git merge-file` 三方合并同步。

---

## 二、提交规则 | Commit Rules

- **提交信息使用规范前缀**：`[AI]`（AI 协作代理完成）、`fix:`、`feat:`、`docs:`、`refactor:`、`test:` 等，便于检索与回溯。
- **所有由 AI 协作代理（Agent）产生的提交，提交信息必须带 `[AI]` 标签**。例如：
  - `[AI] TODO 落账（8/29）：IR 覆盖清单更新 + CLI 完善标注`
  - `[AI] docs: 新增 Git 提交与推送约定（GIT_CONVENTIONS）`
- 人工开发者提交可自行选择是否加 `[AI]`，但改动内容相同。
- 提交信息应简要说明**改了什么、为什么**，尽量保持单条提交原子、可读、可回滚。
- 提交前先 `git status` / `git diff` 核对改动范围，避免误提交生成物（构建产物、`.xmake` 缓存等）。

---

## 三、推送规则 | Push Rules

### 3.1 硬性约束：禁止自动推送

**任何写入远端的操作都必须由仓库所有者（ltree）逐次明确授权后执行。这是不可绕过、不可推断的门禁。**

以下操作一律**禁止代理自行执行**，无论是否处于自动批准 / bypass / yolo 权限模式：

- `git push`（含 `-u`、`--follow-tags`、`--atomic`）
- `git push --force` / `--force-with-lease`（改写已推送历史，风险最高）
- `git push origin <tag>` / `git push --tags`（发布 tag）
- 通过平台 CLI/API 触发远端写入：`gh pr create`、GitLink/GitHub 合并请求、release 上传、CI 手动触发等

允许且不需要授权的**纯本地**操作：`git add` / `commit` / `merge` / `cherry-pick` / `rebase`（未推送的提交）/ `stash` / 本地 `git tag`（创建，但不推送）。

### 3.2 什么才算授权

- 授权必须是**当前会话内、针对具体动作与具体引用**的明确要求（例："推送 main 和 PA"、"打 v1.0.2 tag 并推上去"）。
- **不得从间接措辞推断**：如"提交并同步""做好发布准备""合到 main"等只授权到本地提交/合并为止；是否推送由所有者另行决定。
- **一次授权只对当次有效**：上一次同意推送 `main`，不等于同意下次推送或推送 tag。
- 授权范围不扩展：同意普通推送不等于同意 force-push；同意推送分支不等于同意推送 tag。
- 拿不准时**停下来询问**，不要先做。

### 3.3 常规流程（已获授权后）

- 常规推送：`git push`（已跟踪分支）或 `git push -u origin <branch>`（首次推送新建分支）。
- 推送前确认：`git status` 干净、目标分支已切到、`git log` 核对待推送提交。
- 冲突/分叉时：先 `git fetch` + `git pull --rebase` 整合，再推送。
- 改写已推送历史（force-push）前必须逐条列出受影响分支并取得确认，推送后校验远端与本地一致。

---

## 四、流程速查 | Quick Reference

```bash
# 新建分支并建立上游跟踪（推送需 §3.1 授权）
git switch -c <branch>
git push -u origin <branch>     # ← 需所有者授权

# 提交（AI 代理必须加 [AI] 标签）
git add <files>
git commit -m "[AI] <描述>"

# 推送（推送由所有者决定/授权；未授权时停在本地提交）
git push                        # ← 需所有者授权

# 发布 tag（本地创建免授权，推送需授权）
git tag -a v1.0.2 -m "release: v1.0.2 ..."
git push origin v1.0.2          # ← 需所有者授权

# 与远端同步（只读，无需授权）
git fetch
git pull --rebase
```

---

## 五、里程碑记录

- **2026-08-29 建立 `PA` / `PB` 分支**：基于 `main`（`04f1435`），与 `origin/main` 同步，确立双方案并行开发模型。
- **2026-08-29 制定本约定**：确立 `[AI]` 提交标签、推送授权制、分支策略。
- **2026-08-31 `v1.0.0` tag（commit `2036fba`，PA-12 TODO 状态更新）**：发布门禁达标（c/native 12P/0F + examples 6/6）；合 `main` 与 tag 推送由 ltree 决定（推送授权门禁见 §3.1）。
- **2026-09-03 1.0.x 指针语法收敛（`PA`）+ `v1.0.1` tag（commit `75c59cc`）**：移除 `T*` 显式声明与一元 `*` 解引用，解引用统一 `.()`/`.(T)`/`->`；另含 Linux 平台修复（PA-9）、`is` 模式匹配规范定案（仅文档）、§12 指针传递矩阵文档统一。PB 线对应落地提交 `887067a`（2026-09-04）。
- **2026-09-19 ~ 09-20 `v1.0.2` 发布准备（待 tag）**：`PA` 上 v1.0.1 之后的全部提交（`is` 移除 `=>`／PA-13、`is _` 通配符补全／PA-15、循环体外 `is` 前端守卫／PA-18、`do` 体内 `is` IR 侧拒绝／PA-19、`?=` 安全赋值记号移除／PA-20、`?.` / `?(` 安全解引用记号移除＋检查落入 `.()`／PA-21、从属查询与位域偏移内置函数补实现＋三参签名定稿／PA-22、§5.1 指针后缀链与切片补实现／PA-25、`len(x)` 内置函数补实现／PA-27、`else if` 在 IR 前端补支持／PA-28、§7.3 调用方接收规则与 §12.1 传递矩阵矛盾收口（文档定案）／PA-29、结构体成员分隔符文档示例修正（文档-only）／PA-23、`alignof` 改由编译器编译期求对齐并输出字面量／PA-24、§5.1.4／§5.1.5 复合体指针与函数指针示例补实现（切片赋值的字符串右值＋推断声明取成员类型与返回类型）／PA-26、`print`/`puts` 输出语义写入规范 §2.3.1（文档-only）／PA-30、`is` 多子句首个匹配即止在 A 后端实现／PA-16、定长字符数组的字符串初值改数组存储＋容量诊断（BNF v2.8）／PA-31、`flow` 自动释放收紧为只发给堆右值（`no_auto_free` 登记存储来源，修复对静态字面量与 `&x` 的非法 `free`）／PA-32、`flow → flow` 所有权转移在 1.0 线可书写（本语句右值豁免 `moved_src`、失效自下一条语句生效、释放责任移交接收方、冻结源禁止转交）／PA-33、`var {…} char[N]` 多变量声明的定长数组存储＋三条诊断（复用 PA-31 判定并登记 `len()` 容量，BNF v2.10）／PA-34、编译器版本号改由 `xmake.lua` 注入做单一真源（`--version`／帮助／verbose／`nihao.toml` 同源）／PA-35、README 的现在时计数换为 v1.0.2 现值＋权威指针／PA-36、ROADMAP「当前代码架构」重写为真实目录树（消除 `test/`／`arch/`／`myapp/` 幽灵路径与 `前端`／`后端` 假子目录）／PA-37、STAGE_SUMMARY 定性为历史快照＋现在时计数换为权威指针（§1 双仓库同步流程标废止）／PA-38、里程碑表计数写法约定＋PB 验收行去无法溯源的详细计数（本文件 §1 同步新增「共享文档的测试计数写法」）／PA-39、本文件三处失实/失效陈述修正（分支基线改带日期的历史陈述、已废止的双仓库成对提交义务撤下、失效的「§1 发布门禁」引用改指 §3.1）／PA-40、`codegen.c` 死代码全链清理、xmake PATH 探测修复、BNF v2.1/v2.2/v2.3/v2.4/v2.5/v2.6/v2.7/v2.8/v2.9/v2.10 收敛、TODO 分层、文档-实现一致性核对、禁止自动推送规则入文档、共享文档 PA↔PB 双向同步）已就绪，v1.0.2 门禁 c/native 各 38P/0F/5S（IR 线 6P/0F/33S，权威详值见 `CHANGELOG.md` v1.0.2 段）；`main` 已重新对齐为 `PA` 镜像；遗留文档-实现一致性核对 C 组（C6b 双仓库章节、表格残缺等零星项）与 D 组（语法元素表 `=>` 行残留、v1.0.2 tag 与推送）待决策。tag 与推送由 ltree 决定（推送授权门禁见 §3.1）。
