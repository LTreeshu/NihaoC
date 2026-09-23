# NihaoC 语法设计全貌（固定基准审阅稿）

> 用途：给 ltree 逐条审阅「要固定下来的语法设计到底是哪些」。基准 = `PA` 分支 `docs/BNF.md` **v2.10**（PA tip `a21a317`）+ `Chinese.md` + 两份语法元素表。
> 审阅方式：每节末尾若有要改的地方，直接回「R7 改成 …」这样按编号说即可；没点编号的节按现状固定。
> 本文件是**审阅工件，未纳入 git 提交**；定稿后内容折进 `BNF.md` / `Chinese.md`，本稿是否归档再定。

---

## 0. 固定的分层原则（先确认这条，否则下面都白定）

| 层 | 放哪 | 跨分支是否必须逐字一致 |
| --- | --- | --- |
| **语言设计**：记号集、关键字、产生式、类型/声明/语句/函数的写法、语义约束（§12.1 矩阵、存储期排序） | `BNF.md`、`Chinese.md`、`English.md`、两份语法元素表 | **是**（本轮目标） |
| **实现状态**：某条线哪个后端已落地、哪个待做、行号、测试计数 | `IMPLEMENTATION_STATUS.md`、`TODO-PA.md`、`TODO-PB.md` | 否（各分支自己的） |

现在设计文档里混了第二层的句子（"A 后端 c/native 落地／IR 前端属 2.0 待做／PA-31"这类）。按行计数（2026-09-21 实测，核对命令见下）：`BNF.md` **22 行**（含第 8 行的版本修订前言）、`Chinese.md` **9 行**、`English.md` **8 行**。固定设计 = 把这些句子从三份设计文档剥到 `IMPLEMENTATION_STATUS.md`，只留下与版本/日期绑定的不变量。

```
# 计数复核
grep -n "PA-[0-9]\|PB-[0-9]\|A 后端\|IR 前端\|IR 后端\|2\.0 " docs/BNF.md | cut -d: -f1 | sort -un | wc -l
grep -c "实现状态\|A 后端\|IR 后端\|PB-2\|PA-[0-9]" docs/Chinese.md
grep -c "Implementation status\|A backend\|IR backend\|PB-2\|PA-[0-9]" docs/English.md
```

---

## 1. 词法层

```
<token> ::= <identifier> | <keyword> | <int-literal> | <float-literal> | <char-literal>
          | <string-literal> | <operator> | <delimiter> | <visibility-enum> | <eof>
```

- **标识符**：`(字母 | "_") {字母 | 数字 | "_"}`
- **关键字（33 个，固定清单）**：
  `module use link linkas as` ／ `func flow static const var` ／ `struct union enum alias`
  ／ `if else switch case default` ／ `for do while is break continue` ／ `goto return cooking align`
  ／ `sizeof typeof alignof offsetof bitoffsetof holdof structof unionof visof` ／ `true false`
  ／ `register restrict volatile`
- **字面量**：十进制 / `0x` 十六进制 / `0b` 二进制整数（可带 `+ -`）；浮点含 `e` 科学计数；字符 `'a'` `'\n'`；字符串 `"..."`（转义 `n t r 0 \ " '`）
- **运算符**：算术 `+ - * / %`、自增 `++ --`、比较 `== != < > <= >=`、逻辑 `&& || !`、位 `& | ^ ~ << >>`、赋值 `=` 与 9 个复合赋值、成员/指针 `.` `->` `.(` `?`（三元）`..` `::` `#` `,` `:`
- **分隔符**：`( ) [ ] { } ; 换行`
- **可见性枚举**：`_undef _const _flow _static _var`（可作表达式，主要给 `is` 和 `visof` 用）
- **语句终止**：换行、`;`、`#` 三者等价 —— 三种都保留是本稿要确认的点（R1）

```
module main
use stdio
func main() {
    x i32 = 0b1010          # 64 位整数字面量族
    y f64 = 1.5e3
    s char[] = "hi"
    flag u8:1               # 位域宽度写在类型后
}
```

---

## 2. 程序结构与模块

```
<program>   ::= { <top-level> }
<top-level> ::= <module-decl> | <use-decl> | <link-decl> | <linkas-decl>
              | <alias-decl> | <type-decl> | <var-decl> | <func-decl>
              | <cooking-block> | <align-block> | <empty-stmt>
```

```
module main                     # 模块名
use stdio                       # 导入（`use a.b.c` 允许点分）
use stdlib
link "user32" MessageBoxA       # 绑定到具名外部符号
linkas "my_lib.so"              # 整库链接别名 —— 见 R2：A 后端无实现
alias Handle = void             # 类型别名
```

---

## 3. 类型系统

**基本类型（固定 19 个）**：`void char string bool` ／ `i8 i16 i32 i64 u8 u16 u32 u64 f32 f64 fx32 fx64` ／ `short int long float double`
（`string` 是 `char[]` 的别名，见 R3。`fx32/fx64` 已实测：**语法与类型系统全接通，但存储就是整数** —— `a fx32 = 1` 生成 `int32_t a = 1;`、`b fx64 = 2` 生成 `int64_t b = 2;`，`cgen.c:174-175` 的注释写着 Q16.16 / Q32.32，但没有任何定点字面量、定点运算或隐式缩放。所以 R4 的真问题不是"有没有实现"，而是"1.x 要不要把它说成定点"）

**指针 = 通用 `void` 起写**（本语言没有 C 的 `T*`，一元 `*` 已于 v2.1 移除）：

```
p1 void      = &v               # 通用指针
p2 void[]    = &p1              # 指向数组的指针形态
p3 void[][]  = &p2              # 多维指针数组 void[n][m]
fp  void(i32) char              # 函数指针：void(参数类型) [返回类型]
```

**数组的四种尺寸写法（固定）**：

```
a i32[8]      = {…}             # 定长 8
b u16[6...]   = {…}             # 动态数组，声明容量 6（规范写法）
c u16[6..]    = {…}             # 同上，另一种后缀
d u16[...]    = {…}             # 未指定容量 → 默认 8 槽（R5：默认值要不要留）
e u16[...6]   = {…}             # 已废弃的历史顺序（R6：改成硬报错还是继续兼容接受）
```

`[N]` 与 `[N...]` 当前生成的存储完全相同，差别只在源码层，为 2.0 的 `ptr+len+cap` 改造预留区分。

**聚合类型**：成员**空白分隔**（逗号被拒），位域 `:宽度`，成员可带属性前缀与默认值。默认值这条产生式**语法接受、代码生成是坏的**（实测见 §11.1 F1：`x i32 = 7` 生成了 `7int32_t x;`），R7 就是"留产生式并挂实现待办"还是"先摘掉产生式"。

```
Point struct { x i32  y i32 }
Line  struct { a Point  b Point }
Data  union  { asInt i32  asFloat f64 }
Color enum   { RED, GREEN, BLUE = 10 }     # 枚举变体允许逗号（与 struct 不对称，R8）
```

---

## 4. 声明与存储期

四种属性，寿命从长到短：`static > flow > const = var`

```
const K i32 = 10                # 模块级=静态只读；块级=自动不可变
static counter i32 = 0          # 静态、可变
flow  buf void = malloc(i32)    # 拥有所有权，退出自动释放
var   t i32 = 0                 # 局部自动，可省略写 `t i32 = 0`
```

**两种书写风格并存**（R9：是否砍掉 C 风格只留一种）：

```
x, y : i32          = 0         # 兼容 C 风格（冒号引类型）
x i32               = 0         # 规范风格（类型后置）
v                   = 5         # 推断声明：取字面量类型
p                   = &v        # 推断：指向 v 的指针
m                   = s.w       # 推断：取成员类型（数组成员退化为指针）
r                   = f(a)      # 推断：取被调（或函数指针）返回类型
{aa = "aa", bb = "bb"} char[3]  # 多变量声明：逐变量按容量分配数组存储
```

**硬约束**：指针声明**必须初始化**，语言层没有空指针字面量（R10：是否补 `null`／`nil`）。

**定长字符数组 vs 动态字符串**（本条是最容易写错的一组，值得你确认口径）：

```
s char[4]  = "abc"              # OK：3 字符 + '\0' = 4，数组存储，len(s)==4
s char[3]  = "abc"              # 前端错误：string needs 4 bytes with terminator, array 's' holds 3
a i32[4]   = "abc"              # 前端错误：非 char 元素不许用字面量，须 {…} 值列表
d char[]   = "abc"              # 动态字符串：退化为指针，len(d)==3
```

---

## 5. 表达式与后缀链

优先级（高→低）：后缀 → 一元 → `* / %` → `+ -` → 移位 → 关系 → 相等 → `&` → `^` → `|` → `&&` → `||` → 三元 → 赋值。

**后缀链（可任意串接，是本语言的核心写法）**：

```
p.(i32)             # 解引用（省略类型 = 按指针静态目标宽度）
p.(char[9])[0]      # .(T) 固定元素宽度后再下标
p[]                 # 空下标 = 一层 p.()
p[a..b]             # 切片读：取「指向第 a 个元素的指针」，长度由接收方决定
p->field            # 指针成员访问（等价 p.().field）
f(x)  e++  e--      # 调用 / 自增自减
```

`.(T)` 自带两重检查：写前可见性（冻结/失效即错）+ 编译期 `sizeof(T)` 与目标宽度比较（超出即越界错）。裸 `void` 指针下标/切片是前端错误，必须先 `p.(T)[i]` 固定宽度。

**聚合初始化与设计指定初始化**：

```
l Line = {{1, 2}, {3, 4}}
cfg    = {.port = 8080, .host = h}
```

**内置函数（10 个，签名固定）**：

| 写法 | 语义 |
| --- | --- |
| `sizeof(T)` / `typeof(T)` / `alignof(T)` | 类型尺寸 / 类型 / **编译期算出的对齐字面量**（不依赖 C 的 `_Alignof`）；`void` 按 8 计 |
| `offsetof(T, m)` / `bitoffsetof(T, m)` | 成员字节偏移 / 位域按声明序的位偏移 |
| `structof(T, m, ptr)` / `unionof(T, m, ptr)` / `holdof(T, m, ptr)` | **三参**，由成员地址反推所属聚合体首地址，返回 `void`；`structof` 仅 struct、`unionof` 仅 union、`holdof` 通用 |
| `visof(expr)` | 取表达式的可见性枚举（`_const`/`_flow`/…） |
| `len(x)` | 数组=容量（多维取各维乘积）、动态字符串=字面量长、切片变量=`hi-lo`；静态不可知即前端报错 |
| `malloc(T)` / `malloc(T[n])` | 堆分配；赋给 `T[n] x` 时声明退化为指针（仅最前维省略） |

`print` / `puts` 不是关键字而是内置调用：`print` 首实参是字面量 → `printf` 直通不换行；非字面量 → 按十进制整数打印并换行（`print("%d\n", x)` / `print(x)`）。`static_assert` 同理是标识符匹配（R11：内置函数与关键字/标识符的识别方式要不要统一）。

---

## 6. 语句与控制流

```
if c { … } else if c2 { … } else { … }        # else 后可直接跟 if（递归形式）
switch (v) { case 1: …  case 2: …  default: … }
for i = 0; i < n; i += 1 { … }                # for-init 可写声明；step 产生式只列了赋值/++/--（R12）
while v += 1 { … }                            # 条件允许赋值表达式
do value > 0 { … }                            # 条件在块外先求值（不是 C 的 do-while）
break / continue / return expr / goto lbl / lbl:  …  /  ;  /  #
```

两条实测结论（细节见 §11.1）：
- **R12**：`for i = 0; i < 3; bump(&cnt) { … }` 在 A 后端**能编过**并原样生成 `for (int32_t i = 0; i < 3; bump(&cnt))`。也就是说实现的 step 位置实际是 `<expr>`，比产生式宽。固定设计时二选一：把产生式放宽到 `<expr>`（跟实现一致、零改动），或保留窄产生式并给两条线各挂"补前端守卫"待办。
- **R13**：`switch` 的 case **不穿透** —— 编译器给每个 case 体末尾自动插入 `break;`（实测 `case 1:` 只打印 `hit case1` 就出块）。这条语义文档里**一字未提**，而 C 程序员默认相反，属固定前必须补进 `BNF.md` §6 说明的条目。

**`is` 模式匹配 —— 只存在于 `while` 循环体内，只有块形式**：

```
w i32 = 9
while w -= 1 {
    is 3        { puts("lit")   ; break }
    is -1       { puts("neg")   ; break }
    is 1..5     { puts("range") ; break }
    is _        { puts("wild")  ; break }     # 恒匹配
    is RED      { puts("enum")  ; break }     # 枚举变体按值
    is _flow    { puts("vis")   ; break }     # 可见性枚举
}
```

固定口径：
- 匹配对象是 `while` 条件表达式的值（隐式 `__is_val`，类型=条件类型）。
- 多个 `is` 按源码顺序求值，**首个匹配即执行且只执行一个**（无 fallthrough）。
- 循环体外写 `is` → 前端报错；`do` 体内写 `is` → 前端报错（即使 `do` 嵌在 `while` 里也不许静默匹配外层）。
- 模式族共 8 类：`_` / 整数字面量 / 负字面量 / `lo..hi` 闭区间 / 枚举变体 / 可见性枚举 / 结构体解构（预留）/ ADT 变体解构（预留）/ `<identifier>`。
- **`is <identifier>` 的语义要请你定（R14）**：设计上写的是"变量绑定"，但两条线当前都按"与该标识符的值比较"实现；且裸标识符要先跟枚举变体做消歧。要么把设计文字改成"按值比较"，要么保留绑定语义并要求实现跟上。

---

## 7. 函数

```
[[inline]] func add(a i32, b i32) i32 { return a + b }

flow  make(void) void      { … }     # 返回堆指针，调用方拥有并自动释放
static shared(void) void   { … }     # 返回静态存储期指针
const peek(void) void      { … }     # 返回只读指针
func  plain(a i32)         { … }     # 无返回值
ext   printf(void(i32)) i32 ;        # 原型声明（函数体位置写 `;`）
```

- 返回属性：`func` / `flow` / `static` / `const`；`func` = 无返回或非指针返回。
- `[[…]]` 属性：`local inline weak used unused export["\".section\""]`。
- 参数属性 `flow/var/const/static` 写在参数名前。
- **调用方接收规则（矩阵，固定）**：`flow` 返回值只能 `flow` 接；`static` 可 `static`/`const` 接、禁 `flow`/`var`；`const` 只能 `const` 接。

---

## 8. 编译期块

```
cooking {
    static_assert(2 + 3 * 4 == 14, "arith precedence")
    const BASE i32 = 10
    const MULT i32 = 4
    static_assert(BASE * MULT == 40, "ct var mul")
    const SQUARE(n) = n * n          # 编译期函数（宏式，实参字面量代入）
}
align 16 { Packed struct { a u8  b u64 } }
```

`cooking` 在 1.0 产品线**不可编译**（属 2.0 预览，仅 IR 前端支持）。本稿按"设计保留、实现状态另记"处理（R15：1.0 是否连语法承诺一起摘掉）。

---

## 9. 语义约束（非语法，但属设计层，须固定）

- **存储期排序**：`static > flow > const = var`；赋值要求目标寿命 ≥ 源寿命。
- **§12.1 传递矩阵**：`const→仅 const`、`static→const/static`、`flow→const/flow/var`、`var→const/var`。
- **`flow → flow` 转移**：转移语句内右值仍可读源，"失效"自下一条语句起；源被 `const`/`var` 借用（冻结）时转移为编译期错误；转移后释放责任移交接收方，失效源不再参与自动释放。
- **自动释放**只对"确实拥有堆所有权"的 `flow` 生效：字符串字面量、`&x`、聚合初值三类右值不发给 `free`。
- **`is` 无 fallthrough**（同 §6）。

---

## 10. 已从设计中移除的记号（固定，两分支一致）

| 记号 | 状态 | 现写法 |
| --- | --- | --- |
| `T*` 具名指针声明 | v2.1 移除 | `p void = &x` |
| 一元 `*p` 解引用 | v2.1 移除 | `p.()` / `p.(T)` / `p->f` |
| `is <pat> => <stmt>` 单语句 | v2.2 移除 | `is <pat> { … }` |
| `?=` 安全赋值 | v2.3 移除 | `=`（`=` 本身无条件执行 §12.1 检查） |
| `?.` / `?(` 安全成员/解引用 | v2.3 移除 | `.()` / `.(T)`（检查由记号自身承担） |

**你已裁定**：语法元素表里 `=>` / `?=` / `?.` 的"词法保留"行**统一删除**（沿用 PA 现口径），两分支各 120 bullet。
遗留一个可选动作（R16）：`token.h` 里 `TOK_FAT_ARROW` / `TOK_SAFE_ASSIGN` / `TOK_SAFE_DOT` 三个 lexer 仍识别的死记号，要不要在某条线一并摘掉，让"词法保留"这句话也变成历史。

---

## 11. 请你逐条裁定的点（R1~R18）

| 编号 | 问题 | 现在的口径 | 我的建议 |
| --- | --- | --- | --- |
| R1 | 语句终止符三种（`\n` `;` `#`）是否都留 | 全留，等价 | 留 `;` 与 `\n`，`#` 降级为"兼容保留、文档不再推荐" |
| R2 | `linkas` 写在 BNF 但 A 后端无实现（`TOK_LINKAS` 零消费） | 设计承诺有、实现缺 | 本轮先固定设计（保留），PB/PA 各挂实现待办 |
| R3 | `string` 与 `char[]` 同义并存 | 并存 | 保留（`string` 更可读），但在规范里明写二者等价 |
| R4 | `fx32/fx64` 定点型是什么 | **实测已接通但只是整数**：`fx32`→`int32_t`、`fx64`→`int64_t`（`parser.c:240-241`、`cgen.c:174-175`），无定点字面量/运算/缩放 | 建议 1.x 就把设计文字从"定点"改成"整型别名、定点语义预留 2.0"，或直接从类型清单摘掉；别继续承诺一个行为等同 int 的"定点型" |
| R5 | `[...]` 默认 8 槽 | 未指定容量给魔数 8 | 建议改为要求显式容量或 `malloc` 初始化，魔数 8 只作历史兼容 |
| R6 | `[...N]` 废弃顺序 | 静默按兼容接受 | 建议加一条编译告警，指定版本后转错误 |
| R7 | 结构体成员 `[ "=" <expr> ]` 默认值 | **实测：语法接受、代码生成坏掉**（§11.1 F1，A 后端把 `x i32 = 7` 生成 `7int32_t x;`，tcc 报 `invalid number`） | 这是缺陷不是设计分歧。建议：产生式保留（设计意图明确），PA 线挂缺陷修复待办；在修好前该写法属"语法可写、不可编译"，需在 `IMPLEMENTATION_STATUS.md` 记一行 |
| R8 | `struct` 成员禁逗号、`enum` 变体允许逗号 | 不对称 | 确认是刻意（enum 更像常量列表）还是统一到空白分隔 |
| R9 | 声明两种风格（C 风格 `x,y : i32` 与规范风格） | 并存 | 建议固定为"规范风格为主，C 风格保留但只文档一处提及" |
| R10 | 无空指针字面量，声明必须初始化 | 无 `null` | 确认设计层是否需要 `null`（不做就是"指针必有初值"这条纪律的代价） |
| R11 | 内置 `print/puts/malloc/static_assert/len` 靠标识符名匹配，`sizeof` 等是关键字 | 不统一 | 建议统一为"内置名不参与用户作用域解析"，写法不变、识别机制在实现层归一 |
| R12 | `for` 的 step 允许什么 | **实测比产生式宽**：A 后端接受任意表达式（调用式 step 编过并原样生成，见 §11.1） | 建议把产生式放宽为 `<for-step> ::= <expr>`，与实现和 `<expr-stmt>` 一致；同时明写"编译器不保证 step 里会更新循环变量"（漏写 `i += 1` 就是死循环，语言层不检测） |
| R13 | `switch` case 是否穿透 | **实测不穿透**：A 后端给每个 case 末尾自动插 `break;`（§11.1 F3） | 建议把"case 不穿透（自带 break）"写进 `BNF.md` §6 说明固定下来；要不要提供显式穿透关键字（如 `fallthrough`）单独定，本稿倾向 1.x 不提供、穿透需求走 `goto` |
| R14 | `is <identifier>` 是变量绑定还是按值比较 | 设计写绑定、实现按值 | 二选一，别继续两说；改设计文字最省事，改实现才更"模式匹配" |
| R15 | `cooking` 在 1.0 不可编译 | 语法承诺、1.0 后端拒 | 按第 0 节分层处理：设计保留，状态进 `IMPLEMENTATION_STATUS.md` |
| R16 | 死记号 token（`=>` `?=` `?.`）是否从 lexer 摘除 | 词法仍识别 | 设计文档已不描述它们；lexer 摘除留给各线自主，建议 2.0 摘、1.0 冻结不动 |
| R17 | `register/restrict/volatile` 在关键字表但无任何产生式位置 | 纯预留 | 建议本稿就把它们标"词法保留、语法不使用"，否则固定了一份含死条目的清单 |
| R18 | `<label-def>` 没进 `<statement>` 选择项（`goto`/标签实测可用） | BNF 漏列 | 补 `<label-def>` 到 `<statement>`，属纯文档修正 |

---

## 11.1 本轮实测记录（用 `ncc/build/ncc.exe` v1.0.2，backend `c`）

上面 R4/R7/R12/R13 原来标"未证"，这次用编译器跑过再写结论。样本放在 gitignore 的 `ncc/build/probe{1..5}.nc`，需要复现时直接 `./ncc.exe build probeN.nc -o probeN.exe`。

| 编号 | 输入 | 结果 | 定性 |
| --- | --- | --- | --- |
| F1 | `P struct { x i32 = 7  y i32 }` | 前端接受；生成 `typedef struct P { 7int32_t x; int32_t y; } P;` → tcc `error: invalid number` | **A 后端代码生成缺陷**（成员默认值被当成类型名前缀输出）。设计产生式 `<field-decl> ::= … [ "=" <expr> ]` 意图清楚，属实现没跟上 → R7 |
| F2 | `print("%d\n", i)` | 生成的 C 里字符串**含真实换行字节**：`printf("%d⏎", i)`（`⏎` 是 0x0A，不是 `\n` 两字符） | **A 后端代码生成缺陷（潜在可移植性）**。tcc 作为扩展接受，严格的 gcc/clang/MSVC 会报 unclosed string literal。当前全部 examples/tests 都在 tcc 下过，所以门禁没抓到 |
| F3 | `switch (1) { case 1: … case 2: … default: … }` | 输出只有 `hit case1`；生成的 C 每个 case 末尾带 `break;` | **实现已定：case 不穿透**。文档从未写过 → R13 补规定 |
| F4 | `for i = 0; i < 3; bump(&cnt) { … }` | 编过并原样生成 `for (int32_t i = 0; i < 3; bump(&cnt))`（因 step 未推进 `i`，运行是死循环 —— 我的样本故意如此，用来证明语法位置被接受） | 实现比 `<for-step>` 产生式宽 → R12 |
| F5 | `a fx32 = 1` / `b fx64 = 2` / `c string = "abc"` | 分别生成 `int32_t a`、`int64_t b`、`char* c`，均编译运行通过 | `fx32/fx64` 语法与类型系统已接通，只是**没有定点语义** → R4；`string`→`char*` 与 R3 口径一致 |

F1、F2 落在 **`PA` 发布线（v1.0.2）自己的 A 后端**上，不是 PB 缺口；本轮你定的是"只固定设计、代码缺口逐条登记"，所以这两条我建议登记成 PA 待办（下一节 P 表之外另立）。F2 尤其要留意：一旦哪天换 gcc 做后端或有人用外部 C 编译器编译产物，全量测试会一起红。

---

## 12. PB 侧代码缺口（你已定：本轮只固定设计，下列登记为 PB 待办）

| 编号 | 违反定稿设计之处（实测） | 需要落到 PB 的动作 |
| --- | --- | --- |
| P1 | `?=`（`parser.c` 3 处消费 `TOK_SAFE_ASSIGN`）、`?.`（2 处）仍被语法接受 | 语法移除 + 词法保留 + 补 err 用例（对齐 PA 的 `safe_assign_removed` / `safe_dot_removed`） |
| P2 | `structof/unionof/holdof/bitoffsetof` 在 PB **两个前端零实现**（仅 `token.h` 有关键字），且 PB 的 BNF 还是二参签名 | 按三参 `(T, m, ptr)` 实现 + `bitoffsetof` 位布局 + err 用例 |
| P3 | `alignof` 仍输出 C 的 `_Alignof`（`parser.c` 2 处） | 改编译期自算、输出字面量（PA-24 口径） |
| P4 | 无 `while_depth` / `__is_matched` / `no_auto_free` / `moved_src` | `is` 出循环守卫、`is` 无 fallthrough、自动释放只发堆右值、`flow→flow` 转移与冻结源拒绝 |
| P5 | 数组/切片口径缺（定长字符数组容量诊断、非 `char` 元素字面量、多变量数组声明、`len` 静态不可知报错、`void` 裸下标拒绝） | 按 BNF v2.5~v2.10 逐条补前端诊断 |
| P6 | `nihao_version` 单一真源未接入 | 版本号改构建期注入（PA-35） |
