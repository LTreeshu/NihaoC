# NihaoC 示例集（examples/）

NihaoC 2.0 示例。每个示例演示一组语言特性，均可独立编译运行。

## 编译运行

```bash
# 编译为可执行文件（保留 .c 中间产物）
ncc build examples/01_hello.nc -o bin/hello

# 运行产物
./bin/hello
```

默认后端为 `ir-c`；`-backend ir-native|ir-riscv64|ir-loongarch64` 可选（见仓库根 `README.md`）。

## 示例清单

| 文件 | 演示特性 | 可编译 |
|---|---|---|
| [01_hello.nc](01_hello.nc) | module / use / func / puts | ✅ ir-c |
| [02_fib.nc](02_fib.nc) | 递归 / while / 前缀自增 / print | ✅ ir-c |
| [03_struct.nc](03_struct.nc) | struct 嵌套 / union / enum / 嵌套初始化列表 / 整体拷贝 | ⚠️ ir-c 能编但输出受聚合体布局缺口影响 |
| [04_pointer.nc](04_pointer.nc) | & 取地址 / `p.()` 解引用（读与写）/ `p.() op= e` 复合 / 指针隐式类型推断（一元 `*p` 已移除，等价写法 `p.()`） | ✅ ir-c |
| [05_string.nc](05_string.nc) | char[] 字符串（含 NUL）/ 字符字面量 / string 类型 / 字符串池去重 | ✅ ir-c |
| [06_cooking.nc](06_cooking.nc) | cooking 块 / static_assert / 编译期常量与函数 | ✅ ir-c |
| [07_multiret.nc](07_multiret.nc) | 多返回值（命名 struct 返回） | ✅ ir-c |

> 注：`03_struct` 的整体拷贝依赖聚合体真实布局（在册缺口，见 `docs/IR_GAP_LEDGER.md`），
> `ir-c` 能编译运行但输出与规范不符。`examples/` 不在 `xmake test --all` 里，判据历来由
> 手工脚本给出（编译成功 + 产物存在 + 运行 rc=0），不比对 stdout。
