# NihaoC 示例集（examples/）

NihaoC 1.0 发布门面示例。每个示例演示一组语言特性，均可独立编译运行。

## 编译运行

```bash
# 编译为可执行文件（保留 .c 中间产物）
nihao build examples/01_hello.nc -o bin/hello

# 直接编译并运行
nihao run examples/01_hello.nc
```

## 示例清单

| 文件 | 演示特性 | 1.0 可编译 |
|---|---|---|
| [01_hello.nc](01_hello.nc) | module / use / func / puts | ✅ c/native |
| [02_fib.nc](02_fib.nc) | 递归 / while / 前缀自增 / printf | ✅ c/native |
| [03_struct.nc](03_struct.nc) | struct 嵌套 / union / enum / 嵌套初始化列表 / 整体拷贝 | ✅ c/native |
| [04_pointer.nc](04_pointer.nc) | & 取地址 / `p.()` 解引用（读与写）/ `p.() op= e` 复合 / 指针隐式类型推断（一元 `*p` 已于 1.0.x 移除，等价写法是 `p.()`） | ✅ c/native |
| [05_string.nc](05_string.nc) | char[] 字符串（含 NUL）/ 字符字面量 / string 类型 / 字符串池去重 | ✅ c/native |
| [06_cooking.nc](06_cooking.nc) | cooking 块 / static_assert / 编译期常量与函数 | ✅ c/native/ir-c/ir-native |
| [07_multiret.nc](07_multiret.nc) | 多返回值（命名 struct 返回，与 C 机制一致） | ✅ c/native |

> 注：`06_cooking.nc` 的 cooking 块与编译期函数（`const sq(x) = x * x`）自本分支 2026-10-03
> 的 PB-33 P11 回灌轮起进入全量 parser（`PA` 侧为 2026-09-25 的实现轮），四个后端均可编译运行；
> 其余示例均为 1.0 可编译验证通过。
>
> **「均可独立编译运行」的边界（2026-10-27 PB-51 实测）**：`examples/` 不在 `xmake test --all` 里
> （`ncc/xmake.lua` 中该词出现 0 次），历来的判据由手工脚本给出，内容是「编译成功 + 产物存在 + 运行 `rc=0`」，
> **不比对 stdout**。所以「四后端 7/7」不等于「四后端输出一致」：同一轮实测 **`03_struct` 在 `ir-c` / `ir-native`
> 上打印 `struct copy bad`**（A 后端打印 `ok`），根因是 IR 线没有聚合体真实布局（在册缺口，非本轮新发）。
> 上表的「1.0 可编译」列只声明 c / native 能编译运行，不代表机器码路线的输出正确性。
