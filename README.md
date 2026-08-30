# NiHao

A new better C language for my fantasy， a better programing world for void !

一个新的 "better C" 语言，给void一个更好的编程世界!

"Empty your mind, be formless, shapeless, like water. You put water into a cup, it becomes the cup; you put it into a bottle, it becomes the bottle; you put it into a teapot, it becomes the teapot. Now water can flow, or it can crash. Be water, my friend."
— Bruce Lee

"清空你的思绪，无形无相，如水一般。

水入杯，则成杯形；入瓶，则成瓶形；入茶壶，则成茶壶之形。

水可缓缓流淌，亦可奔涌冲击。

像水一样吧，我的朋友。"

— 李小龙

语法文档|Document

[中文](./docs/Chinese.md) | [English](./docs/English.md)

开发约定|Development Conventions

[Git 提交与推送约定](./docs/GIT_CONVENTIONS.md) · [版本路线图（1.0/2.0）](./docs/VERSIONING_ROADMAP.md) · [进度总结](./docs/STAGE_SUMMARY.md) · [TODO](./TODO.md)

Source code example demonstration:

源码展示:

```c
module main
use stdio

// 命名类型：struct（含位域字段）
Packet struct {
    type u8
    len u8:4
    flag u8:4
}

const MaxLen i8 = 100

func main()
{
    puts("Program starting")

    // 数组与浮点运算
    arry f32[3] = {1.1, 1.2, 1.3}
    sum f64 = arry[0] + arry[1] + arry[2]
    if sum > 3.0 {
        puts("array ok")
    }

    // 结构体成员访问
    p Packet
    p.type = 1
    p.len = MaxLen
    p.flag = 1

    // 可见性检查（编译期查询）
    if visof(MaxLen) == _const {
        puts("MaxLen is _const")
    }

    return
}
```

> 说明：上例为**当前全量（A 方案）后端可编译**的语法。更多特性示例见
> [`docs/Chinese.md`](./docs/Chinese.md)：多返回值（命名 struct 返回）、编译期
> cooking/static_assert、指针 `.()` 解引用、`flow/static/const` 存储期与所有权、
> 函数指针等（部分为 BNF 规划特性，实现进度见 [`TODO.md`](./TODO.md)）。
>
> 语言设计规范（完整 BNF）：[`docs/BNF.md`](./docs/BNF.md)。

## 构建与运行 | Build & Run

编译器源码位于 `ncc/`，构建统一使用 [xmake](https://xmake.io)（Makefile 已标为 legacy）：

```bash
cd ncc
xmake                          # 构建 ncc.exe（需预先安装 tcc，见下方 TCC 说明）
xmake test                     # 默认 c 后端回归测试
xmake test --all               # 四后端全矩阵（c / native / ir-c / ir-native）
xmake test -b ir-c             # 指定后端
```

### 后端 | Backends

| 后端 | 说明 |
| ---- | ---- |
| `c`（默认） | 生成 C 文本，调用外部 tcc 编译为可执行文件 |
| `native`（方案 A） | libtcc 进程内编译生成的 C 为机器码（无需外部 tcc） |
| `ir-c` / `ir-native`（方案 B） | IR 中间层：→C 文本 / →x86-64 汇编（Windows x64 ABI） |
| `ir-riscv64`（方案 B） | IR → RISC-V 64 汇编（RV64I + D 浮点扩展，AAPCS 类约定，验汇编生成） |
| `ir-arm64`（方案 B） | IR → AArch64 汇编（AAPCS64，验汇编生成） |
| `ir-loongarch64`（方案 B） | IR → LoongArch64 汇编（LA64 基础指令集，验汇编生成，2026-08-15） |

> **四架构后端全齐**：x86-64（Win64）、riscv64（RV64）、arm64（AAPCS64）、loongarch64（LA64）；
> 后三者生成标准 GAS 汇编（`-backend=ir-riscv64 -o out.s`），本机无交叉汇编器，
> 仅验证汇编生成正确性（`ncc build xx.nc -backend=ir-riscv64 -o rv.s`）。

TCC 安装目录通过 `NIHAO_TCC_DIR` 环境变量指定（如 `/d/devtools/tcc`，MSYS 路径自动归一化），否则从 PATH 探测。

### `-run` 内存执行（Linux only）

`-run` 将程序编译到内存并直接执行，**仅 Linux 可用**：

```bash
nihao build src/main.nc -run -- arg1 arg2    # Linux
```

- 原因：libtcc 0.9.27 的 Windows 版 `TCC_OUTPUT_MEMORY` 存在缺陷（relocate 失败，错误码 251）
- Windows 下使用 `-run` 会得到明确报错；请改用 `-backend=native -o out.exe` 输出文件模式
- 能力检测由 `native_memory_available()` 完成（Windows 返回 0，其余平台返回 1）
- 程序参数在 `-run` 之后直接透传给 `main(argc, argv)`（PA-6）
