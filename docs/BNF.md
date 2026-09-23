# NiHao 语言 BNF 语法定义

> 本文档为 NiHao 编程语言的**完整、严格** BNF（巴科斯范式）语法定义。
>
> - 以 [`Chinese.md`](./Chinese.md)（中文语法规范）为唯一语义标准；本文件与编译器实现（`ncc/lexer.c`、`ncc/parser.c`、`ncc/token.h`）的逐条核对结果记在 [`IMPLEMENTATION_STATUS.md`](./IMPLEMENTATION_STATUS.md)。
> - 记号（terminals）一律使用双引号字符串；非终结符使用 `<...>` 尖括号表示。
> - 约定：`::=` 定义；`|` 选择；`[ x ]` 可选（0 或 1 次）；`{ x }` 重复（0 或多次）；`( x | y )` 分组。
> - 本文件是**语言设计层**文档：只描述记号、产生式与语义约束，**不含任何后端的实现状态**（哪个后端已落地、哪项待做、行号、测试计数一律记在 [`IMPLEMENTATION_STATUS.md`](./IMPLEMENTATION_STATUS.md)，待办记在 `TODO-PA.md` / `TODO-PB.md`）。1.0 线与 2.0 线的本文件内容应逐字一致。
> - 版本：v2.11（2026-09-21 修订，见下表）。
>
> ### 版本历史（只记语法/语义口径变化）
>
> | 版本 | 变化 |
> | ---- | ---- |
> | v2.11 | 语法设计固定轮（R1~R19 裁定落地）：`<for-step>` 放宽为 `<expr>`；新增 `switch` 各 `case` **不穿透**（编译器自动收尾，1.x 无显式穿透写法）的语义规定；`<statement>` 补入 `<label-def>`；`is <identifier>` 模式定为**按值比较**（撤销"变量绑定"承诺，绑定归 2.0）；C 风格类型别名 `short`/`int`/`long`/`float`/`double` 从 `<primitive-type>` 移除（基本类型定为 16 个）；`fx32`/`fx64` 定性为"与同宽整型等价存储、定点语义预留 2.0"；`#` 语句终止符降级为兼容保留；`register`/`restrict`/`volatile` 明写为词法保留、语法不使用；`[...]` 默认容量与 `[...N]` 历史顺序均标注为兼容保留、新代码不推荐；C 风格声明标注为兼容写法（规范风格为主）；内置名与关键字统一为"不参与用户作用域解析"。产生式之外的语义约束未削弱 |
> | v2.10 | 多变量声明的定长数组口径定案：`var {aa = "aa", …} char[N]` 与单变量声明同口径（每变量按声明容量分配数组存储、`len()` 取容量），初值非字符串字面量为前端错误 |
> | v2.9 | `flow → flow` 所有权转移口径定案：转移语句内右值仍可读源、失效自下一条语句起；冻结源转移为编译期错误；释放责任随转移移交 |
> | v2.8 | 定长字符数组的字符串初值口径：按声明容量分配数组存储，须 `strlen + 1 <= N`，放不下即前端报错；非 `char` 元素不许用字面量初始化；`len(s)` 取声明容量 |
> | v2.7 | 切片赋值右值补字符串字面量（按字节复制含 `\0`，须 `strlen <= b-a`）；推断声明右值补成员访问与调用两种取值 |
> | v2.6 | `len(x)` 口径定案：数组=容量（多维取乘积）、动态字符串=字面量长、切片变量=`hi-lo`；静态不可知时前端报错；推断切片声明按**视图**（指针）处理 |
> | v2.5 | 后缀链语义定案：`.(T)` 可在链上任意位置继续下标/成员/切片；空下标 `p[]` 等价 `p.()`；切片读取"指向第 a 号元素的指针"、切片写逐元素写回；`T[n] x = malloc(T[n])` 声明退化为指针；裸 `void` 下标为前端错误 |
> | v2.4 | 从属查询内置函数签名定为三参 `structof`/`unionof`/`holdof(Type, member, ptr)`；`bitoffsetof` 定为声明序位偏移 |
> | v2.3 | `?=` 安全赋值与 `?.` / `?(` 安全解引用两类附加记号从语法移除，安全检查统一由 `=` 与 `.()` 自身承担 |
> | v2.2 | `is <pattern> => <statement>` 单语句形式移除；`is` 只保留块形式且仅配合 `while` |
> | v2.1 | 指针语法收敛：一元 `*` 解引用移除，解引用统一 `.()` / `.(T)` / `->` |
>
> 逐版的落地过程、后端归属与探针细节见 `CHANGELOG.md` 与 `IMPLEMENTATION_STATUS.md`。

---

## 1. 词法定义（Lexical Grammar）

### 1.1 记号总览

```
<token>          ::= <identifier> | <keyword> | <int-literal> | <float-literal>
                   | <char-literal> | <string-literal> | <operator> | <delimiter>
                   | <visibility-enum> | <eof>
```

### 1.2 标识符与关键字

```
<identifier>     ::= ( <letter> | "_" ) { <letter> | <digit> | "_" }
<letter>         ::= "a".."z" | "A".."Z"
<digit>          ::= "0".."9"

<keyword>        ::= <grammar-keyword> | <reserved-keyword>
<grammar-keyword> ::= "module" | "use" | "link" | "linkas" | "as"
                   | "func" | "flow" | "static" | "const" | "var"
                   | "struct" | "union" | "enum" | "alias"
                   | "if" | "else" | "switch" | "case" | "default"
                   | "for" | "do" | "while" | "is" | "break" | "continue"
                   | "goto" | "return" | "cooking" | "align"
                   | "sizeof" | "typeof" | "alignof" | "offsetof"
                   | "bitoffsetof" | "holdof" | "structof" | "unionof" | "visof"
                   | "true" | "false"
<reserved-keyword> ::= "register" | "restrict" | "volatile"
                   | "short" | "int" | "long" | "float" | "double"
```

> - `<grammar-keyword>`（**40** 个）在下面的产生式里各有位置。
> - `<reserved-keyword>`（**8** 个：**`register` / `restrict` / `volatile`** 三个 C 风格限定符，加 **`short` / `int` / `long` / `float` / `double`** 五个 C 风格类型别名）**仅作词法保留**：本版本没有任何产生式使用它们，写进源码是保留字冲突而非合法语法。限定符保留目的是为将来的语义占位；五个别名自 v2.11 起从 `<primitive-type>` 移除（见 §3），按宽度改写即可：`i16` / `i32` / `i64` / `f32` / `f64`。
> - **基本类型名**（`void` `char` `string` `bool` `i8`…`fx64`，共 16 个，见 §3 `<primitive-type>`）与**声明修饰符**（`const` `flow` `static` `var`）在词法上是保留字，不参与用户作用域解析。

### 1.3 字面量

```
<int-literal>    ::= [ "+" | "-" ] <digit> { <digit> }
                   | [ "+" | "-" ] "0x" <hex-digit> { <hex-digit> }
                   | [ "+" | "-" ] "0b" ( "0" | "1" ) { "0" | "1" }
<hex-digit>      ::= <digit> | "a".."f" | "A".."F"

<float-literal>  ::= [ "+" | "-" ] <digit> { <digit> } "." { <digit> }
                   | [ "+" | "-" ] <digit> { <digit> } [ "." { <digit> } ] ( "e" | "E" ) [ "+" | "-" ] <digit> { <digit> }

<char-literal>   ::= "'" <printable-char> "'"
                   | "'" "\" ( "n" | "t" | "r" | "0" | "\\" | "'" | "\"" ) "'"

<string-literal> ::= "\"" { <printable-char> | <escape-seq> } "\""
<escape-seq>     ::= "\" ( "n" | "t" | "r" | "0" | "\\" | "\"" | "'" )
```

### 1.4 运算符与分隔符

```
<operator>       ::= "+" | "-" | "*" | "/" | "%"
                   | "++" | "--"
                   | "==" | "!=" | "<" | ">" | "<=" | ">="
                   | "&&" | "||" | "!"
                   | "&" | "|" | "^" | "~" | "<<" | ">>"
                   | "=" | "+=" | "-=" | "*=" | "/=" | "%="
                   | "&=" | "|=" | "^=" | "<<=" | ">>="
                   | "->" | "." | ".(" | "?"
                   | ":" | "::" | "," | ".." | "#"

<delimiter>      ::= "(" | ")" | "[" | "]" | "{" | "}" | ";" | "\n"

<visibility-enum> ::= "_undef" | "_const" | "_flow" | "_static" | "_var"
```

> 说明：
> - `.()` / `.(T)`：指针解引用（`.()` 内可写目标类型，也可省略）。**安全语义由该记号自身承担**：读/写前检查指针可见性（冻结 / 失效即报错，§12.1），`.(T)` 另在编译期比较 `sizeof(T)` 与指针静态目标宽度，超出即报越界。
> - `->`：指针成员访问（`p->field`，等价 `p.()` 后取成员）。
> - **以下记号已从语法移除，写下即语法错误**（v2.1~v2.3 的收敛结果，本语言不再为它们提供任何语义）：
>
>   | 记号 | 原用法 | 现写法 |
>   | ---- | ------ | ------ |
>   | `T*` | 具名指针类型 | `p void = &x` |
>   | 一元 `*p` | 解引用 | `p.()` / `p.(T)` / `p->f` |
>   | `=>` | `is <pattern> => <statement>` 单语句匹配 | `is <pattern> { … }` |
>   | `?=` | 安全赋值 | `=`（`=` 本身无条件执行 §12.1 检查，无需专用记号） |
>   | `?.` / `?(` | 安全成员访问 / 安全解引用 | `.()` / `.(T)`（检查由记号自身承担） |
>
>   个别记号在词法层仍被识别（兼容既有源码），但**语法一律拒绝**；是否连词法一起摘除由 2.0 线定案，1.x 冻结不动。
> - `..`：数组/切片范围 `[start..end]`，以及 `is` 的闭区间模式 `lo..hi`。
> - `::`：保留（作用域解析，预留，语法不使用）。
> - `#`：**兼容保留的语句终止符**，与 `;`、换行等价。规范写法只用 `;` 与换行；v2.11 起新代码与文档示例不再使用 `#`，该记号本身不移除，既有源码继续可写。
> - 换行符 `\n` 在大多数上下文中是语句终止符；`;` 显式终止。

---

## 2. 程序结构（Program Structure）

```
<program>        ::= { <top-level> }

<top-level>      ::= <module-decl> | <use-decl> | <link-decl> | <linkas-decl>
                   | <alias-decl> | <type-decl> | <var-decl> | <func-decl>
                   | <cooking-block> | <align-block> | <empty-stmt>
```

### 2.1 模块与导入

```
<module-decl>    ::= "module" <identifier>
<use-decl>       ::= "use" <identifier> { "." <identifier> }
<link-decl>      ::= "link" <string-literal> <identifier>
<linkas-decl>    ::= "linkas" <string-literal>
```

### 2.2 类型别名与命名类型

```
<alias-decl>     ::= "alias" <identifier> "=" <type-name>

<type-decl>      ::= <identifier> "struct" "{" { <field-decl> } "}"
                   | <identifier> "union"  "{" { <field-decl> } "}"
                   | <identifier> "enum"   "{" { <enum-variant> } "}"

<enum-variant>   ::= <identifier> [ "=" <int-literal> ] | <enum-variant> "," <enum-variant>
```

---

## 3. 类型系统（Type System）

```
<type-name>      ::= <primitive-type> | <named-type>
                   | <array-type> | <pointer-type> | <function-ptr-type>
                   | "(" <type-name> ")"

<primitive-type> ::= "void" | "char" | "string" | "bool"
                   | "i8" | "i16" | "i32" | "i64"
                   | "u8" | "u16" | "u32" | "u64"
                   | "f32" | "f64" | "fx32" | "fx64"

<named-type>     ::= <identifier>          (* 用户定义类型：struct/union/enum/alias *)

(* 数组类型 *)
<array-type>     ::= <type-name> "[" [ <array-size> ] "]"
<array-size>     ::= <int-literal>                  (* 固定数组，容量 N *)
                   | <int-literal> ( ".." | "..." ) (* 动态数组，固定容量 N —— 规范写法，如 u16[6...]；自动增长留 2.0 *)
                   | ".." | "..."                   (* 动态数组，未指定容量 → 取默认容量 8（兼容保留，见下方说明） *)
                   | ( ".." | "..." ) <int-literal> (* 已废弃：历史兼容写法，等同上一行上一项的 [N...] / [N..] *)

(* 指针类型 *)
<pointer-type>   ::= "void" { "[" [ <array-size> ] "]" }      (* 通用指针 / 多维指针数组 *)
                   | "void" "(" <param-type-list> ")" [ <type-name> ]   (* 见函数指针 *)

(* 函数指针类型 *)
<function-ptr-type> ::= "void" "(" [ <param-type-list> ] ")" [ <type-name> ]
<param-type-list>   ::= <type-name> { "," <type-name> }
```

> 说明：
> - **基本类型共 16 个**（v2.11 定案）：`void` `char` `string` `bool` `i8` `i16` `i32` `i64` `u8` `u16` `u32` `u64` `f32` `f64` `fx32` `fx64`。整族一律**精确宽度命名**，`short` / `int` / `long` / `float` / `double` 这五个 C 风格别名**已从类型清单移除**（它们仍在 §1.2 `<reserved-keyword>` 里作词法保留，写进类型位置即语法错误）；需要 C 语义时直接写对应宽度：`short`→`i16`、`int`→`i32`、`long`→`i64`、`float`→`f32`、`double`→`f64`。
> - **`f32` 为严格单精度存储**：赋值/初始化按单精度舍入截断，运算过程按 `double` 提升，因此 `f32 x = 0.1` 存回后 `x != 0.1`（右侧是 `f64` 字面量）。
> - **`fx32` / `fx64` 定性（v2.11）**：这两个名字保留在类型清单里，但 1.x 的语义就是**与同宽整型等价存储**（`fx32` 宽 4、`fx64` 宽 8，运算/字面量/转换均按整型处理）；定点小数点位置（Q16.16 / Q32.32）**未定义**，属 2.0 预留。写它们不报错，但也不要指望定点行为。
> - `char[]`（方括号内为空）表示**动态字符串类型**，`string` 是其关键字别名，二者完全等价、可互换书写；`string` 不进入数组产生式，`char[N]` 才是字符数组。
> - `void` 单独出现表示**通用指针**；`void[n]` 为 n 元指针数组；`void[n][m]` 为多维指针数组。
> - **数组容量写法四档（v2.11 定性）**：
>   - `[N]` — 固定数组，容量 `N`。**规范写法**。
>   - `[N...]` / `[N..]` — 动态数组，声明容量 `N`。**规范写法**（自动增长留 2.0）。
>   - `[...]` / `[..]` — 省略容量，取**默认容量 8**。属兼容保留：新代码应显式写容量或用 `malloc`，默认魔数不视为设计目标。
>   - `[...N]` / `[..N]` — 早期实现的反序写法，**已废弃**，仅为兼容继续接受；等同 `[N...]`，新代码不得使用（是否加编译告警见 `IMPLEMENTATION_STATUS.md`）。
>   `[N]` 与 `[N...]` 当前的存储布局完全相同（都不自动增长），差异只存在于源码层，供 2.0 的 `ptr+len+cap` 堆结构改造时区分。
> - **定长字符数组的字符串初值**：`s char[N] = "abc"` 按声明容量 `N` 分配**数组存储**（不退化为指针），字面量连同结尾 `\0` 写入，故须 `strlen + 1 <= N`；放不下为前端错误，诊断文案 `string needs N bytes with terminator, array 's' holds M`。元素类型非 `char` 的定长数组以字面量初始化同样是前端错误，须改用 `{...}` 值列表；`len(s)` 取声明容量 `N`。`char[] s = "abc"`（或 `string s = "abc"`）走动态字符串：退化为指针、`len()` 取字面量长度，不适用本条容量检查。
> - 函数指针类型 `void( 参数类型列表 ) [ 返回类型 ]`：无返回类型表示无返回值；
>   返回类型为 `void`（通用指针）时，变量声明前缀必须与其返回存储期一致（`flow`/`static`/`const`）。

### 3.1 结构体/联合体/枚举字段

```
<field-decl>     ::= [ <field-attr> ] <identifier> <type-name> [ ":" <int-literal> ] [ "=" <expr> ]
<field-attr>     ::= "const" | "flow" | "static" | "var"
```

> - `:` 后为位域宽度（bits），如 `flag u8:1`。
> - 成员之间**只用空白分隔**，不写逗号；`=` 后是**成员默认值**（声明该聚合类型且初值省略该成员时取此值）。这是 `struct` / `union` 与 `enum` 的**刻意差异**：枚举变体是常量列表，允许逗号分隔（`RED, GREEN, BLUE`），聚合成员不允许。

---

## 4. 变量声明（Variable Declarations）

```
<var-decl>       ::= [ <var-attr> ] <declarator-list> [ ":" <type-name> ] [ "=" <expr> ]   (* 兼容 C 风格 *)
                   | [ <var-attr> ] <identifier> [ <type-name> ] [ "=" <expr> ]             (* 规范风格 *)
                   | [ <var-attr> ] "{" <multi-init> { "," <multi-init> } "}" <type-name>   (* 多变量声明 *)

<var-attr>       ::= "const" | "flow" | "static" | "var"

<multi-init>     ::= <identifier> "=" <expr>

<declarator-list> ::= <identifier> { "," <identifier> }
```

> 说明：
> - **风格优先级（v2.11）**：上面第二行（属性 + 名字 + 后置类型）是**规范风格**，文档、示例与新代码一律用它；第一行的 C 风格（`x, y : i32 = 0`）为兼容保留，只在本节这一处出现，不作为推广写法。第三种（花括号多变量声明）是并列形态，不是风格降级。
> - `var` 为局部变量显式修饰，可省略（`var x i32 = 0` 与 `x i32 = 0` 等价）。
> - `const`：模块级静态 / 块级自动、不可变；`static`：静态、可变；`flow`：动态（拥有所有权，自动释放）；`var`：局部自动存储期。
> - 指针声明**必须初始化**，不允许声明空指针（语言层没有空指针字面量）。
> - **多变量声明的定长数组口径**：`<type-name>` 为定长数组 `char[N]` 时，每个变量各自按声明容量分配**数组存储**（不退化为指针），`len()` 取声明容量；字面量含结尾 `\0` 放不下、元素类型非 `char`、初值不是字符串字面量三种形态均为前端错误（前两条诊断与单变量声明 `s char[N] = "..."` 同文案，见 §3 与中英规范 §5.1.2）。省略容量的 `char[]` 仍退化为指针并登记字面量长度。

---

## 5. 表达式（Expressions）

```
<expr>           ::= <assignment-expr>

<assignment-expr> ::= <conditional-expr>
                   | <unary-expr> <assign-op> <assignment-expr>
<assign-op>      ::= "=" | "+=" | "-=" | "*=" | "/=" | "%="
                   | "&=" | "|=" | "^=" | "<<=" | ">>="

<conditional-expr> ::= <logical-or-expr> [ "?" <expr> ":" <expr> ]

<logical-or-expr> ::= <logical-and-expr> { "||" <logical-and-expr> }
<logical-and-expr> ::= <bitwise-or-expr> { "&&" <bitwise-or-expr> }
<bitwise-or-expr> ::= <bitwise-xor-expr> { "|" <bitwise-xor-expr> }
<bitwise-xor-expr> ::= <bitwise-and-expr> { "^" <bitwise-and-expr> }
<bitwise-and-expr> ::= <equality-expr> { "&" <equality-expr> }
<equality-expr>  ::= <relational-expr> { ( "==" | "!=" ) <relational-expr> }
<relational-expr> ::= <shift-expr> { ( "<" | ">" | "<=" | ">=" ) <shift-expr> }
<shift-expr>     ::= <additive-expr> { ( "<<" | ">>" ) <additive-expr> }
<additive-expr>  ::= <multiplicative-expr> { ( "+" | "-" ) <multiplicative-expr> }
<multiplicative-expr> ::= <unary-expr> { ( "*" | "/" | "%" ) <unary-expr> }

<unary-expr>     ::= <postfix-expr>
                   | ( "-" | "!" | "~" | "&" ) <unary-expr>
                   | ( "++" | "--" ) <unary-expr>

<postfix-expr>   ::= <primary-expr> { <postfix-op> }
<postfix-op>     ::= "(" [ <expr> { "," <expr> } ] ")"        (* 调用 *)
                   | "[" [ <expr> [ ".." <expr> ] ] "]"       (* 下标/切片；`p[]` 空下标 = 省略类型的 `p.()` 一层解引用。
                      通用 `void` 指针（`varptr void = &x` 等）元素宽度静态未知，裸下标/切片为前端错误，
                      须先写 `p.(T)[i]` 固定元素类型（§5.1.2）*)
                   | "." <identifier>                         (* 成员访问 *)
                   | "->" <identifier>                        (* 指针成员访问（p->field） *)
                   | ".(" [ <type-name> ] ")"                 (* 解引用：可见性检查 + `.(T)` 编译期宽度越界检查；
                      可处于后缀链任意位置，其后继续接 `[]`/`[i]`/`[a..b]`/`.`/`->`*)
                   | "++" | "--"

<primary-expr>   ::= <int-literal> | <float-literal> | <char-literal>
                   | <string-literal> | "true" | "false"
                   | <identifier> | <visibility-enum>
                   | <builtin-call> | <malloc-expr>
                   | "(" <expr> ")"
                   | <aggregate-init>

<aggregate-init> ::= "{" [ <expr> { "," <expr> } ] "}"                     (* 数组/结构体初始化 *)
                   | "{" <designator> { "," <designator> } "}"             (* 指定初始化 *)
<designator>     ::= "." <identifier> "=" <expr>                           (* 如 {.port = 8080} *)

(* 内置函数 *)
<builtin-call>   ::= "sizeof" "(" <type-name> ")"
                   | "typeof" "(" <type-name> ")"
                   | "alignof" "(" <type-name> ")"
                     (* alignof(T)：编译期对齐常量。数组取元素对齐、结构体/联合体取最宽成员对齐、
                        void 按通用指针计 8；由编译器自身算出，不依赖 C 的 _Alignof *)
                   | "offsetof" "(" <type-name> "," <identifier> ")"
                   | "bitoffsetof" "(" <type-name> "," <identifier> ")"
                     (* bitoffsetof(T, m)：位域成员 m 在 T 中的位偏移（按声明顺序的位布局模型，编译期常量） *)
                   | "holdof" "(" <type-name> "," <identifier> "," <expr> ")"
                   | "structof" "(" <type-name> "," <identifier> "," <expr> ")"
                   | "unionof" "(" <type-name> "," <identifier> "," <expr> ")"
                     (* structof/unionof/holdof(T, m, ptr)：由成员地址 ptr 反推所属聚合体首地址，
                        即 (char*)ptr - offsetof(T, m)，返回 void*。structof 仅接受 struct、
                        unionof 仅接受 union，holdof 两者通用 *)
                   | "visof" "(" <expr> ")"
                   | "len" "(" <identifier> ")"
                     (* len(x)：x 的逻辑长度，编译期常量——数组=容量（多维为各维乘积）、
                        动态字符串 char[]=字面量长、切片变量 s=arr[lo..hi] 的边界差 hi-lo。
                        实参逻辑长度静态不可知（标量、非字面量边界切片等）时为前端错误 *)

<malloc-expr>    ::= "malloc" "(" <type-name> ")"
                   | "malloc" "(" <type-name> "[" <int-literal> "]" ")"
```

> 优先级（从高到低）：后缀 → 一元 → `* / %` → `+ -` → 移位 → 关系 → 相等 → `&` → `^` → `|` → `&&` → `||` → 三元 → 赋值。
>
> **内置名的地位（v2.11 定案）**：`sizeof` `typeof` `alignof` `offsetof` `bitoffsetof` `holdof` `structof` `unionof` `visof` 是关键字；`len` `malloc` `print` `puts` `static_assert` 是**内置名**——它们在各自的位置被当作语言设施识别，**不参与用户作用域解析**（用户不能声明同名符号来遮蔽它们）。两种识别方式在源码层的写法差异只是历史遗留，语义上属同一类；实现层的识别机制统一为"内置名优先"登记为待办（见 `IMPLEMENTATION_STATUS.md`）。
>
> `print` 的两种形态按首实参静态区分：首实参是字符串字面量 → 格式化输出且不自动换行；首实参不是字面量 → 按十进制整数打印并换行。`puts` 输出字符串并换行。

---

## 6. 语句（Statements）

```
<statement>      ::= <var-decl> | <assign-stmt> | <if-stmt> | <switch-stmt>
                   | <loop-stmt> | <return-stmt> | <break-stmt> | <continue-stmt>
                   | <goto-stmt> | <label-def> | <block-stmt> | <expr-stmt> | <empty-stmt>

<assign-stmt>    ::= <unary-expr> <assign-op> <expr>
                   (* 左值为切片读形态 `p[a..b]` 时，右值另有两种写法：
                      `= {v0, v1, …}` 从第 a 号元素起逐元素写回（值个数超过 b-a 时以值列表为准）；
                      `= <string-literal>` 按字节复制字面量（含结尾 `\0`），闭区间右界 b 即最后一个
                      可写字节，须 strlen <= b-a，否则前端即时报错*)

<block-stmt>     ::= "{" { <statement> } "}"

<if-stmt>        ::= "if" <expr> <block-stmt> [ "else" ( <if-stmt> | <block-stmt> ) ]

<switch-stmt>    ::= "switch" "(" <expr> ")" "{" { <case-clause> } [ "default" ":" { <statement> } ] "}"
<case-clause>    ::= "case" <expr> ":" { <statement> }
                     (* 各 case 体执行完即结束 switch，**不向下穿透**；1.x 不提供显式穿透写法 *)

<loop-stmt>      ::= <for-stmt> | <while-stmt> | <do-stmt>

<for-stmt>       ::= "for" <for-init> ";" <expr> ";" <for-step> <block-stmt>
<for-init>       ::= <identifier> "=" <expr> | <var-decl>
<for-step>       ::= <expr>                       (* v2.11 放宽：与 <expr-stmt> 同域，赋值、`i++`/`i--`、
                                                      函数调用等任意表达式均可；编译器不检测该表达式
                                                      是否推进循环变量，漏写自增即死循环 *)

<while-stmt>     ::= "while" <expr> "{" { <statement> | <is-clause> } "}"    (* 循环体内可含 is 模式匹配 *)

<do-stmt>        ::= "do" <expr> <block-stmt>    (* do 不支持 is：is 仅配合 while *)

<is-clause>      ::= "is" <pattern> <block-stmt>                (* 仅 while 循环体，块形式 *)

<pattern>        ::= "_"                                (* 通配符，恒匹配 *)
                   | <int-literal>                      (* 整数字面量 *)
                   | "-" <int-literal>                  (* 负整数字面量 *)
                   | <int-literal> ".." <int-literal>   (* 闭区间范围，编译期校验 lo ≤ hi *)
                   | <enum-variant>                     (* 枚举变体 *)
                   | <visibility-enum>                  (* 可见性枚举 *)
                   | <struct-destructure>               (* 结构体解构 — 预留 *)
                   | <adt-destructure>                  (* ADT 变体解构 — 预留 *)
                   | <identifier>                       (* 按值比较：与该标识符当前的值相等则匹配，不引入新绑定 *)

<struct-destructure> ::= <struct-name> "(" <field-pattern> { "," <field-pattern> } ")"
<field-pattern>    ::= <identifier>                    (* 按位置绑定字段 *)
                     | "." <identifier>                (* 具名字段绑定 *)
                     | "_"                             (* 忽略字段 *)

<adt-destructure>  ::= <variant-name> [ "(" <pattern> { "," <pattern> } ")" ]

<return-stmt>    ::= "return" [ <expr> ]
<break-stmt>     ::= "break"
<continue-stmt>  ::= "continue"
<goto-stmt>      ::= "goto" <identifier>
<label-def>      ::= <identifier> ":"                 (* 标签定义（C 风格，语句级，函数内唯一）*)

<expr-stmt>      ::= <expr>
<empty-stmt>     ::= ";" | "#"
```

> 说明：
> - `switch` 的各 `case`（含 `default`）**不向下穿透**：某个 case 的语句列执行完即离开整个 `switch`，不会继续落入下一个 case 体。1.x **不提供**显式穿透写法（没有 `fallthrough` 之类的关键字）；确需共享代码就把语句列提到 `switch` 外，或用 `goto` + 标签。
> - `for` 的三段以 `;` 分隔：`<for-init>` 可以是声明（含类型推断形式 `for i = 0; …`）或表达式；`<for-step>` 是任意表达式（v2.11 放宽），语言层不检测它是否推进循环变量。
> - `while var1 += 1 { ... }`：循环条件允许赋值表达式；该表达式的值就是循环继续的判据，同时是 `is` 的匹配对象。
> - `do <expr> { ... }`：条件在块外**先**求值（求值后执行块），区别于 C 的 do-while；`do` 不支持 `is`。
> - 标签 `name:` 与 `goto` 配套：函数内先定义或后定义均可（跳转目标延迟解析），标签名在函数内唯一。
> - **`is` 模式匹配**：
>   - 只出现在 `while` 循环体内，不可独立使用；在循环体外写 `is` 是编译期错误。
>   - `do` 体内写 `is` 同样是编译期错误，**包括 `do` 嵌套在 `while` 内的情形**（不得静默匹配外层 `while` 的条件值）。`do` 是否纳入 `is` 留待 2.0 定案。
>   - 匹配对象是 `while` 条件表达式的值（隐式条件值，类型等于条件类型）。
>   - 合法模式：通配符 `_`、整数字面量（含负整数）、闭区间范围 `lo..hi`、枚举变体、可见性枚举（`_flow` 等）、`<identifier>`（**按值比较**，v2.11 定案：不引入新绑定，"变量绑定/解构式模式"整体归 2.0）、结构体解构与 ADT 变体解构（预留语法，类型系统支持后实现）。
>   - 标识符消歧：裸标识符若为已知枚举变体则按该变体取值匹配，否则按该变量的当前值比较。
>   - 匹配顺序：多个 `is-clause` 按源码顺序求值，**首个匹配者执行且只执行一个**（无 fallthrough）。
>   - 各模式在各后端的落地状态见 `IMPLEMENTATION_STATUS.md`。
> - 推断声明（无类型名）在语句位置写作 `<identifier> "=" <expr>`，语法上就是 `<assign-stmt>`，只是同时完成声明：字面量右值取字面量类型、`&x` 取指向 `x` 的指针、成员访问 `s.m` 取成员 `m` 的类型（数组成员退化为指针）、调用 `f(a)` / `fp(a)` 取被调（或函数指针）的返回类型。

---

## 7. 函数定义（Function Definitions）

```
<func-decl>      ::= [ <bracket-attr-list> ] <return-attr> <identifier>
                     "(" [ <param-list> ] ")" [ <type-name> ] <func-body>

<return-attr>    ::= "func" | "flow" | "static" | "const"

<bracket-attr-list> ::= "[[" <bracket-attr> { "," <bracket-attr> } "]]"
<bracket-attr>   ::= "local" | "inline" | "weak" | "used" | "unused"
                   | "export" [ <string-literal> ]        (* [[export] ".section"] *)

<param-list>     ::= <param> { "," <param> }
<param>          ::= [ <param-attr> ] <identifier> <type-name>
<param-attr>     ::= "func" | "flow" | "static" | "const" | "var"

<func-body>      ::= "{" { <statement> } "}"
                   | ";"                          (* 函数原型声明（无函数体） *)
```

### 7.1 返回值属性选择规则（语义约束）

| 返回情况 | 必须使用 |
| ------- | ------- |
| 无返回值 | `func` |
| 返回非指针类型（`i32`/`f64`/结构体等） | `func` |
| 返回 `void` 指针（动态内存，调用方拥有、自动释放） | `flow` |
| 返回 `void` 指针（静态存储期） | `static` |
| 返回 `void` 指针（只读） | `const` |

### 7.2 调用方接收规则（语义约束）

| 函数返回属性 | 允许接收 | 禁止接收 |
| ---------- | ------- | ------- |
| `flow`   | `flow`  | `func`、`static`、`const` |
| `static` | `static`、`const` | `flow`、`var` |
| `const`  | `const` | `func`、`static`、`flow` |

---

## 8. 编译期与对齐块（Compile-time & Alignment）

```
<cooking-block>  ::= "cooking" "{" { <cooking-item> } "}"
<cooking-item>   ::= <var-decl> | <ct-func-def> | <ct-func-call> | <static-assert>

<ct-func-def>    ::= "const" <identifier> "(" <identifier> { "," <identifier> } ")"
                     "=" <expr>            (* 编译期函数定义（宏式，参数编译期常量）*)
<ct-func-call>   ::= <identifier> "(" <expr> { "," <expr> } ")"
                     (* 编译期函数调用：参数替换为实参字面量后求值（支持嵌套/组合），2026-08-19 *)

<static-assert>  ::= "static_assert" "(" <expr> "," <string-literal> ")"

<align-block>    ::= "align" <int-literal> "{" { <top-level> } "}"
```

> 说明：
> - `cooking`：编译期执行块。块内可声明编译期变量/常量、调用编译期函数（如 `cooking PI = 3.1415926` 定义编译期常量、`cooking { const X i32 = ... }` 计算常量、`static_assert` 编译期断言）。
> - `align n { ... }`：块内类型按 n 字节对齐。

---

## 9. 存储期与赋值安全原则（语义约束，非语法）

四种属性的存储期从长到短排列为：

    static (程序全程) > flow (动态) > const = var (自动/块级)

其中 `const` 在模块级声明时具有静态存储期（与 `static` 同级），在块级声明时具有自动存储期（与 `var` 同级）。

赋值时须满足：**目标变量的存储期不短于源变量**（目标寿命 ≥ 源寿命），否则可能导致悬垂指针。

完整的兼容性判定（含所有权与借用语义）详见 [`Chinese.md`](./Chinese.md) 第 12.1 节传递矩阵。

> **`flow → flow` 所有权转移口径**：转移语句（`flow b void = a` 与 `b = a`）的右值在**该语句内**仍须能读取 `a`，失效自下一条语句起生效；源已冻结（被 `const`/`var` 借用）时转移为编译期错误；转移后释放责任移交接收方，失效源不再参与 §11.1 的自动释放。
>
> **自动释放的范围**：只对确实拥有堆所有权的 `flow` 生效；字符串字面量、`&x`、聚合初值三类右值不拥有堆，绑定给 `flow` 时不参与退出点的 `free`。

---

## 附：语法完整性对照（已实现特性 → BNF 规则）

| 特性 | BNF 规则 |
| ---- | -------- |
| 模块/use/link/linkas | §2.1 |
| alias 类型别名 | §2.2 |
| struct/union/enum 命名定义、struct 返回值 | §2.2 |
| 位域 `name u8:1` | §3.1 |
| 数组/动态数组/切片 | §3 |
| 多维指针数组 `void[n][m]` | §3 |
| 函数指针 `void(u8) i32`、函数指针数组 | §3 |
| 解引用 `.()` / `.(T)`（可见性 + 编译期越界检查） | §5 postfix |
| 指针成员访问 `p->field` | §5 postfix |
| 范围 `..`（切片/区间模式） | §5 postfix / §6 |
| 多变量声明 `var {a=0,b=1} i8` | §4 |
| 复合赋值 `+=` 等 | §5 |
| 三元 `?:`、逻辑/位运算 | §5 |
| 内置函数 sizeof/typeof/…/visof/malloc | §5 |
| if/else、switch/case、for/while/do、break/continue/goto | §6 |
| `is` 模式匹配（仅 while 循环体，块形式；含 `_` 通配/区间/可见性/标识符模式，解构预留） | §6 |
| 函数属性 `[[inline]]` 等、原型 `;` | §7 |
| cooking 编译期、static_assert | §8 |
| align 对齐块 | §8 |
| 可见性枚举 `_const`/`_flow`/`_static`/`_var`/`_undef` | §1.4 |
| `#` 语句终止符 | §1.4 / §6 |
