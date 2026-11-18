# NihaoC 项目进度与 TODO

> 本文件为索引，详细内容见 docs/ 目录。

- [项目路线图与通用待办](docs/ROADMAP.md) — 里程碑、架构概览、P0-P2 通用待办、推荐下一步
- [PA 方案待办](docs/TODO-PA.md) — 已冻结历史线（`PA`，只读，1.x / A 线追溯）
- [PB 方案待办](docs/TODO-PB.md) — 唯一开发主线（`PB`，IR-only 0.2.x）

---

## 分支与同步规则（2026-11-17 ltree 裁定）

现行模型：**`PB` 开发 + `main` 发布镜像**（`PA` 冻结只读，`backup-pb-20260916` 归档），详见 `docs/GIT_CONVENTIONS.md` §一。

| 场景 | 操作 |
|------|------|
| 新增/完成 PB-* 项 | 只改 `docs/TODO-PB.md` |
| 里程碑完成 / 架构决策变更 | 改 `docs/ROADMAP.md`（随 `PB` 提交） |
| 索引结构调整 | 改 `TODO.md`（随 `PB` 提交） |
| 发布 | `main` 从 `PB` 快进 + ltree 审核打 tag |

> A 线已物理删除、`PA` 冻结，**不再有跨分支同步**；历史「PA→main / 三分支同步」规则仅适用于 1.x 时期。
