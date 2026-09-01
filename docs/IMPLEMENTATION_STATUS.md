# NihaoC 规范与编译器实现对应关系

本文档记录 NihaoC 语言规范（Chinese.md / English.md）中各项指针安全规则与编译器（ncc）实际实现的对应状态。

---

## 指针传递矩阵（§12.1）

| 规范要求 | 编译器实现 | 对应代码 | 状态 |
|---------|----------|---------|------|
| 4×4 传递矩阵 | `vis_check_transfer()` | vis.c:70–86 | ✅ 已实现 |
| 源状态变更（冻结/失效/保持） | `vis_update_source()` | vis.c:89–113 | ✅ 已实现 |
| 仅指针类型受约束 | `vis_is_pointer_type()` 守卫 | vis.c:55–60 | ✅ 已实现 |
| 借用作用域释放 | `vis_unfreeze_borrows()` | vis.c:117–124 | ✅ 已实现 |
| 失效变量不可读 | `vis_check_usable()` | vis.c:127–136 | ✅ 已实现 |
| 冻结变量不可写 | `vis_check_writable()` | vis.c:139–148 | ✅ 已实现 |
| 赋值检查编排 | `vis_check_assign()` | vis.c:154–198 | ✅ 已实现 |

---

## 函数参数传递（§12.2）

| 规范要求 | 编译器实现 | 对应代码 | 状态 |
|---------|----------|---------|------|
| `flow` 参数接收所有权 | 参数前缀被忽略 | parser.c:870–874 | ⚠️ 未实现 |
| `var` 参数冻结实参 | 参数前缀被忽略 | parser.c:870–874 | ⚠️ 未实现 |
| `const` 参数冻结实参 | 参数前缀被忽略 | parser.c:870–874 | ⚠️ 未实现 |

> 当前编译器解析函数参数上的属性前缀（`flow`/`var`/`const`），但在内部将所有参数统一视为 `var`（`VIS_DEFAULT`）。参数的所有权/借用检查将在后续版本中完善。

---

## 检查范围

| 规范要求 | 编译器实现 | 对应代码 | 状态 |
|---------|----------|---------|------|
| 裸标识符赋值检查 | 完整实现 | parser.c:1028–1039 | ✅ 已实现 |
| 表达式级检查 | 仅检查裸标识符 | parser.c:1028–1039 | ⚠️ 部分实现 |
| IR 后端所有权检查 | 无 | irparse.c | ⚠️ 未实现 |

> `vis_check_assign()` 仅在赋值右侧为**单个裸标识符**时触发。`x = y + 1` 或 `x = malloc(...)` 等表达式形式的赋值不经过传递矩阵检查。IR 后端（irparse.c）仅记录可见性值用于 `visof()` 查询，不执行所有权/借用检查。

---

## 存储期属性（§11.1）

| 规范要求 | 编译器实现 | 对应代码 | 状态 |
|---------|----------|---------|------|
| `const` 模块级静态 / 块级自动 | C 后端自然实现 | parser.c:1010–1017 | ✅ 隐式实现 |
| `flow` 动态分配 + 自动释放 | 块退出时自动 free | parser.c:1405–1415 | ✅ 已实现 |
| `flow` 返回值所有权转移 | `ownership_transferred` 标志 | parser.c:1306–1315 | ✅ 已实现 |
| `static` 静态存储 | C `static` 关键字 | cgen | ✅ 已实现 |

> `const` 的存储期区分（模块级静态 / 块级自动）由 C 后端编译器的自然语义实现——ncc 不显式区分两种 `const`，但生成的 C 代码中，文件作用域 `const` 自然获得静态存储期，块作用域 `const` 自然获得自动存储期。`vis_check_transfer()` 对两种 `const` 一视同仁。

---

## 测试覆盖

| 测试文件 | 测试规则 | 状态 |
|---------|---------|------|
| tests/err/m2a_flow_static.nc | `flow`→`static` 禁止 | ✅ |
| tests/err/m2b_const_flow.nc | `const`→`flow` 禁止 | ✅ |
| tests/err/m2c_frozen.nc | 冻结源不可写 | ✅ |
| tests/err/m2d_invalid.nc | 失效源不可读 | ✅ |
| tests/pos/borrow.nc | `flow`→`var` 借用 + 解冻 | ✅ |
| tests/pos/flow.nc | `flow` 块级自动释放 | ✅ |
| tests/pos/transfer.nc | `flow` 返回值所有权转移 | ✅ |
