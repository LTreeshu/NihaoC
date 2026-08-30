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
- 三条分支均从同一稳定点分出（当前基于 `main` 的 `04f1435`）。
- 功能/修复先落在对应的 `PA` 或 `PB` 分支，验证通过后再视需要合回 `main`。
- 新增命名分支时遵循 `feature/<name>`、`fix/<name>`、`docs/<name>` 命名；以 `PA`/`PB` 为主分支的工作，直接提交到对应分支即可。
- 分支与 `origin` 保持一致：新建分支后执行 `git push -u origin <branch>` 建立上游跟踪（`--set-upstream-to`）。

### 版本化与发布
- `v1.0.0`：`PA` 通过发布门禁（c/native 全量 0 FAIL + examples 全跑通）→ 合入 `main` → tag `v<major>.<minor>.<patch>`（`release:` 前缀）。
- `v1.0.x`：hotfix 走 `PA` 分支修复 → 合 `main` → tag。
- `v2.0.0`：`PB` 阶段 2（能力平移）达标、全矩阵 0 FAIL → 合入 `main` → tag。
- 共享文件（lexer/token/sym/type/vis/stdlib/module/linker 等）改动：语法演进**先 `PB` 验证 → 再 `PA` 移植**；bug 修复**先 `PA` → 再同步 `PB`**；双仓库（NihaoC ↔ ncc 活跃）必须成对提交防基线漂移。

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

- **推送动作由仓库所有者决定**：AI 代理仅在获得明确授权后执行 `git push`。
- AI 代理负责：准备好提交、设置上游跟踪（`-u`）、保持分支与远端同步就绪，但**不擅自推送**。
- 常规推送流程：`git push`（已跟踪分支）或 `git push -u origin <branch>`（首次推送新建分支）。
- 推送前确认：`git status` 干净、目标分支已切到、`git log` 核对待推送提交。
- 冲突/分叉时：先 `git fetch` + `git pull --rebase` 整合，再推送。

---

## 四、流程速查 | Quick Reference

```bash
# 新建分支并建立上游跟踪
git switch -c <branch>
git push -u origin <branch>

# 提交（AI 代理必须加 [AI] 标签）
git add <files>
git commit -m "[AI] <描述>"

# 推送（推送由所有者决定/授权）
git push

# 与远端同步
git fetch
git pull --rebase
```

---

## 五、里程碑记录

- **2026-08-29 建立 `PA` / `PB` 分支**：基于 `main`（`04f1435`），与 `origin/main` 同步，确立双方案并行开发模型。
- **2026-08-29 制定本约定**：确立 `[AI]` 提交标签、推送授权制、分支策略。