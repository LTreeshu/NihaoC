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

[Git 提交与推送约定](./docs/GIT_CONVENTIONS.md) · [版本路线图（1.0/2.0）](./docs/VERSIONING_ROADMAP.md) · [阶段总结（历史快照）](./docs/STAGE_SUMMARY.md) · [TODO](./TODO.md) · [变更日志](./CHANGELOG.md)

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

> 说明：上例为**当前全量（A 方案）后端可编译**的语法（2026-10-27 复测：`c` / `native` 均编译运行，输出
> `Program starting` / `array ok` / `MaxLen is _const`；同一份源在 `ir-c` / `ir-native` 上会报
> `ir: unsupported top-level token 'const'`——IR 前端目前没有顶层变量声明，属在册缺口，不是本例写错）。更多特性示例见
> [`docs/Chinese.md`](./docs/Chinese.md)：多返回值（命名 struct 返回）、编译期
> cooking/static_assert、指针 `.()` 解引用、`flow/static/const` 存储期与所有权、
> 函数指针等（部分为 BNF 规划特性，实现进度见 [`TODO.md`](./TODO.md)）。
>
> 语言设计规范（完整 BNF）：[`docs/BNF.md`](./docs/BNF.md)。

## 安装 | Installation

### 1. 安装 xmake

构建依赖 [xmake](https://xmake.io)（唯一构建入口，Makefile 已标为 legacy）：

```bash
# Windows（powershell）
winget install xmake

# Linux / macOS
curl -fsSL https://xmake.io/shget.text | bash
```

### 2. 安装 tcc（编译器后端依赖）

NihaoC 1.0 的 `c` / `native` 后端依赖 [TinyCC](https://bellard.org/tcc/)（tcc 负责把生成的 C 编译为机器码）：

- **Windows**：将 tcc 解压到任意目录（如 `C:\dev\tcc`），设置环境变量 `NIHAO_TCC_DIR=C:\dev\tcc`；未设置时从 `PATH` 探测。MSYS 路径（`/d/devtools/tcc`）会自动归一化
- **Linux**：`sudo apt install tcc`（或 `pacman -S tcc`）即可满足 `c` 后端；`-backend=native` 还要求**进程内 libtcc**：
  需要 `/usr/local/lib/libtcc.so` 在位（apt 只给静态 `libtcc.a` 时会与 `ncc` 自身的 `expect` / `sym_find` / `sym_push`
  符号撞成 multiple definition），且 `native.c` 把安装前缀**硬编码**成 `/usr/local/lib/tcc`，故 `tcc -vv` 输出的
  `install:` 行必须与之一致——这三条前置是 2026-10-11 在 WSL Ubuntu-24.04 上复跑门禁时实测的（见 `docs/TODO-PB.md` 的 PB-37）。
  `-run` 内存执行与 `libtcc.so` 均可用
- 验证：`tcc -v` 能看到版本号即可

## 构建与运行 | Build & Run

编译器源码位于 `ncc/`，构建统一使用 [xmake](https://xmake.io)：

```bash
cd ncc
xmake                          # 构建 nihao 可执行文件
xmake test                     # 默认 c 后端回归测试
xmake test -b native           # native 后端回归测试
xmake test --all               # 四后端全矩阵（c / native / ir-c / ir-native）
```

### CLI 命令 | Commands

```bash
nihao <command> [options]
```

| 命令 | 作用 |
| ---- | ---- |
| `init [name]` | 创建新项目（`nihao.toml` + `src/main.nc`） |
| `build <file>` | 编译为可执行文件（保留 `<out>.c` 中间产物） |
| `run <file>` | 编译并运行；`--` 之后的参数透传给 `main(argc, argv)` |
| `debug <file>` | 编译并输出详细信息后运行；`--ir` 打印 IR 三地址码（2.0 预览） |
| `test` | 运行回归测试套件（`tests/`） |
| `lex <file>` | 打印源文件的 token 流 |

常用选项：`-o <file>` 输出名 · `-backend c|native|ir-c|ir-native|ir-riscv64|ir-loongarch64` 选择后端（写错会给候选表；`ir-x86_64` / `ir-arm64` 已随 2026-09-26 的 PB-34 删除，点名时报「backend ... was removed」并提示改用 `ir-native`） · `-run`（Linux only）内存执行 · `-g` 调试信息 · `-I/-L/-l` 头文件/库路径与链接 · `--link <lib> as <alias>` 链接别名。

### 后端 | Backends

| 后端 | 范围 | 说明 |
| ---- | ---- | ---- |
| `c`（默认） | **1.0 正式支持** | 生成 C 文本，调用外部 tcc 编译为可执行文件 |
| `native` | **1.0 正式支持** | libtcc 进程内编译生成的 C 为机器码（无需外部 tcc） |
| `ir-c` | 2.0 预览 | IR 中间层 → C 文本，交外部 tcc |
| `ir-native` | 2.0 预览 | 按宿主分派：riscv64 / loongarch64 宿主出本机汇编再汇编链接，其余宿主（含 x86-64 / arm64）**回退 IR → C → 宿主 cc** |
| `ir-riscv64` | 2.0 预览 | IR → RISC-V 64 汇编（RV64I + D 浮点扩展，交叉 `.s` only；产物经外部汇编器与链接器验证，本机不执行） |
| `ir-loongarch64` | 2.0 预览 | IR → LoongArch64 汇编（LA64 基础指令集，交叉 `.s` only；产物经外部汇编器与链接器验证，本机不执行） |


> **1.0 范围**：`c` / `native` 两后端为对外可用产品线（路线图 A 方案），v1.0.2 门禁实测：全量语法回归每后端 **38 PASS / 0 FAIL / 5 SKIP**，examples **6/7** ——
> `06_cooking.nc` 当时失败于 `cooking` 块内的编译期函数形态，仅 `ir-c` / `ir-native` 可通过；该形态已于本分支 2026-10-03 的 PB-33 **P11** 回灌轮落入全量 parser（`PA` 侧为 2026-09-25），examples 现为 **7/7**（2026-10-04 复跑，四后端均可编译运行）。
> 计数随版本变化，权威口径以 [`CHANGELOG.md`](./CHANGELOG.md) 当前版本段与 [`docs/IMPLEMENTATION_STATUS.md`](./docs/IMPLEMENTATION_STATUS.md) 为准。
> **`examples/` 不在门禁里（2026-10-27 PB-51 实测）**：`xmake test --all` 里没有任何 `examples` 判据（`xmake.lua` 中该词出现 0 次），
> 「examples 四后端 7/7」历来由**手工脚本**跑，判据是「编译成功 + 产物存在 + 运行 `rc=0`」，**stdout / stderr 丢弃**——
> 它能抓「编不过 / 跑崩」，对输出内容零断言（同一轮实测：`03_struct` 在 `ir-c` / `ir-native` 上打印的正是 `struct copy bad`，
> 与「7/7」这句话同时成立）。所以下文出现「examples N/N」时，请读作「N 份都能编译并运行」，不是「N 份输出正确」。
> **双平台验证（2026-08-31，v1.0.0 时点）**：Windows 与 Linux（WSL Ubuntu-24.04）均通过 c/native 全量回归 0 FAIL +
> examples 6/6（c/native 双后端，当时的用例集）；`-run` 内存执行在 Linux 实测通过（WSL 期间修复 4 个平台 bug，
> 详见 CHANGELOG v1.0.1 记录）。
> **2.0 预览**：`ir-*` 为下一代演进线（路线图 B 方案）。IR 线的机器码发射器**只保留 riscv64 与 loongarch64 两档**
> （2026-09-26 裁定，见 `docs/TODO-PB.md` 的 PB-34）：二者生成标准 GAS 汇编（`-backend=ir-riscv64 -o out.s`），
> 产物逐份过**外部汇编器与链接器**（2026-09-27 起，PB-35；当时语料 54 份出码 46，两档各 46/46 过汇编、各 45/46 过链接）。
> **执行也做得到（2026-10-27 更正 PB-35/PB-37 那句「本机无 `qemu-user`、执行正确性仍未验证」）**：交叉链接在 Windows
> 走 `zig cc -target {riscv64,loongarch64}-linux-musl -static`（两档的宿主 `ld` 都只认 x86，这一步非 zig 不可），
> 执行在 WSL 走 `qemu-riscv64-static` / `qemu-loongarch64-static`（qemu-user-static 8.2.2，零安装）；参照系按
> 「同一份源、同一档架构、同一个 musl、同一个模拟器，只换 lowering 路径」三路投票，用来把**发射器缺陷**与
> **IR 前端语义缺口**分开（PB-38 首跑，语料 77 份、两档各出码 53、链接各 51/53）。**发射器专属缺陷已清零**
> （PB-39 loongarch64 取址改 `pcalau12i %pc_hi20` + `addi.d %pc_lo12`、PB-40 riscv64 `fcvt.l.d` 补 `rtz`
> 与两档间接调用改用暂存寄存器、PB-41 riscv64 补 `IR_FTRUNC`）：agree 由 la 12→45、rv 42→45，崩溃归零，
> 剩余分歧已逐条归因到 IR 前端语义或上游共享缺陷（PB-42 起在册）。Linux 侧另用发行版自带的 `llvm-mc` 18 作第二
> 汇编 oracle 复验（PB-44：两档各 53/53 接受，判的是 Linux 自己编译的 `ncc` 产物）。x86-64 / arm64 的发射器已删除，
> 这些平台上 `ir-native` 自动改走 IR → C → 宿主 cc，即由 C 路线代劳（`-backend=ir-arm64` 报专属诊断后退出）。
> **但两档机器码不在门禁矩阵里**（门禁跑的是 c / native / ir-c / ir-native 四档），且交叉产物**没有 sanitizer 可用**
> （2026-10-27 PB-53 实测：zig 0.15.2 缺这两档的 ASan 运行时，链接期即 `undefined symbol: __asan_*`），
> 所以那两档的内存安全目前只能靠静态读帧与定向探针核查。版本规划详见
> [`docs/VERSIONING_ROADMAP.md`](./docs/VERSIONING_ROADMAP.md)。


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
