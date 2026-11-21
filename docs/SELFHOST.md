# 自举准备（路线 1：宿主自举，目标 `ir-c`）

> 建立：2026-11-21 ｜ 分支：`selfhost`（本地分支，从 `PB` 的 `9d9b625` / v0.2.3 切出，**不推送**）
> 目的：为「用 NihaoC 写 NihaoC 编译器」做编译器侧准备。自举编译器本体在仓库外的
> `NihaoC-selfhost/` 目录开发，本分支只承载**编译器能力补齐**与**能力矩阵实测**，不污染 `PB` 主线。

## 1. 为什么选 `ir-c`

| 后端 | 结论 | 原因 |
|---|---|---|
| `ir-c` | **唯一可用载体** | 产物是 C，交 tcc 编译，天然带宿主 libc（文件 I/O、malloc、getenv 全部可用） |
| `ir-native` | 同 `ir-c`（x86-64 宿主） | 本机没有 x86-64 发射器，`ir-native` 回退 IR→C |
| `ir-riscv64` / `ir-loongarch64` | 不可用 | 只出 `.s`，无 libc 链接、无多文件 TU、执行靠 zig+qemu |
| `ir-riscv32` / `ir-arm` | 未开工 | — |

## 2. 本分支已落地

### SH-1 声明初值可省略（`ncc/irparse.c`）

此前声明路径无条件 `expect('=')`，于是 `buf char[64]` 这种「只要一块缓冲」的声明在 IR 线
直接语法错，并把后续语句整条级联带死（实测：`buf char[64]` 后紧跟的 `k i32 = 5` 也报错）。
现在 `=` 可省略，无初值做**确定性零填充**：

- 数组：运行期小循环逐元素写 0（约 12 条 IR，与容量无关，64KB 缓冲也不会让产物暴涨）；
- 标量 / `char[]` 指针槽：直接写 0（整型 0、`f64` 0.0、指针 NULL）。

用例 `ncc/tests/pos/decl_no_init.nc`（含 `.expect`）已入 `IR_SUBSET`；门禁由
**109P/0F/0S → 110P/0F/0S**（`ir-c` / `ir-native` 各 110）。

### SH-2 `link` 真正进入链接命令行（`ncc/irparse.c`）

此前 `link "lib" [as alias]` 解析即跳过，外部库只能靠「未知标识符当外部符号」碰运气。
现在库名收进 `CompilerState.link_libs`（去重），并在最终 `tcc … -o …` 命令上追加 `-l<lib>`。
两种写法都收：`link "lib" alias`（`p0_link.nc` 的历史形态）与 `link "lib" as alias`（BNF 形态）。

**平台注意**：Windows 的 tcc 没有独立 `libm`，`link "m"` 会报 `library 'm' not found`（实测），
libm 符号在 CRT 里直接可用；`link "kernel32"` 正常。**自举本身不需要 `link`**（libc 默认就链）。

## 3. 能力矩阵（2026-11-21 实测，Windows x86-64 / tcc 0.9.27 / `-backend ir-c`）

| 能力 | 状态 | 可用写法 / 备注 |
|---|---|---|
| 文件读 | ✅ | `f void = fopen("x","r")` + `fread(buf,1,n,f)` + `fclose(f)`（外部符号隐式声明，tcc 链接 libc） |
| 文件写 | ✅ | `fopen("y","w")` + `fwrite(p,1,n,f)` |
| 环境变量 | ✅ | `e void = getenv("NAME")` |
| `main` 参数 | ⚠️ 部分 | `func main(argc i32, argv void)` 可编译且 `argc` 真实传入（实测 3）；**取 `argv[i]` 字符串尚未走通**，建议先用 `getenv` 传文件名 |
| 动态内存 | ✅ | `p void = malloc(i8[4096])`（`malloc` 必须带类型） |
| `free` | ✅ | 外部符号可用；**语言层没有 auto-free，必须自己释放** |
| 结构体 / 位域 / 数组 / 多维 | ✅ | 聚合体内字节布局（PB-83）已落地 |
| 递归 / 函数指针 / `switch` / `goto` | ✅ | — |
| 数组形参 | ❌ | `func f(a char[])` 解析失败 → 用 `void` 指针 + 显式寻址 |
| 函数体换行 `{` | ❌ | `{` 必须与签名同行：`func f() i32 {` |
| 可增长容器 / 切片运行时长度 | ❌ | `[N...]` 固定容量；自举侧需自己用 `malloc` 做块链 |
| 指针/整数互转告警 | ⚠️ | ir-c 产物有 `assignment makes integer from pointer` 一类警告，tcc 放行 |

## 4. 自举验收判据（分阶段）

| 阶段 | 判据 |
|---|---|
| S1 | `boot0` 能把样例 `.bc` 编译成 C，产物经 tcc 编译后可运行且输出符合期望 |
| S2 | 用 `boot0` 编译 `boot0` 自身源码（子集内自洽），产出 `boot1` 可执行 |
| S3 | `boot1` 与 `boot0` 对同一测试集输出一致（含错误诊断），才算真正自举 |

当前进度见仓库外 `NihaoC-selfhost/README.md`。
