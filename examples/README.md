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
| [04_pointer.nc](04_pointer.nc) | & 取地址 / `p.()` 解引用 / `*()` 一元解引用 / 指针隐式推断 | ✅ c/native |
| [05_string.nc](05_string.nc) | char[] 字符串（含 NUL）/ 字符字面量 / string 类型 / 字符串池去重 | ✅ c/native |
| [06_cooking.nc](06_cooking.nc) | cooking 块 / static_assert / 编译期常量与函数 | ⚠️ 2.0 预览（IR_ONLY，需 `-b ir-c`） |
| [07_multiret.nc](07_multiret.nc) | 多返回值（命名 struct 返回，与 C 机制一致） | ✅ c/native |

> 注：`06_cooking.nc` 使用 cooking/static_assert 子集语法，全量 parser（1.0 的 c/native 后端）
> 暂不支持，属 2.0 预览特性（用 `-backend ir-c` / `-backend ir-native` 编译）；其余示例均为 1.0 可编译验证通过。
