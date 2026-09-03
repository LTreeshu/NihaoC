# NiHao 语言 BNF 语法定义

> 本文档为 NiHao 编程语言的**完整、严格** BNF（巴科斯范式）语法定义。
>
> - 以 [`Chinese.md`](./Chinese.md)（中文语法规范）为唯一语义标准，与编译器实现（`ncc/lexer.c`、`ncc/parser.c`、`ncc/token.h`）保持一致。
> - 记号（terminals）一律使用双引号字符串；非终结符使用 `<...>` 尖括号表示。
> - 约定：`::=` 定义；`|` 选择；`[ x ]` 可选（0 或 1 次）；`{ x }` 重复（0 或多次）；`( x | y )` 分组。
> - 版本：v2.0（2026-08-05 修订，覆盖全部已实现语法）。

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

<keyword>        ::= "module" | "use" | "link" | "linkas" | "as"
                   | "func" | "flow" | "static" | "const" | "var"
                   | "struct" | "union" | "enum" | "alias"
                   | "if" | "else" | "switch" | "case" | "default"
                   | "for" | "do" | "while" | "is" | "break" | "continue"
                   | "goto" | "return" | "cooking" | "align"
                   | "sizeof" | "typeof" | "alignof" | "offsetof"
                   | "bitoffsetof" | "holdof" | "structof" | "unionof" | "visof"
                   | "true" | "false"
                   | "register" | "restrict" | "volatile"
```

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
                   | "->" | "." | ".(" | "?." | "?(" | "?=" | "?"
                   | ":" | "::" | "," | ".." | "#"
                   | "=>"

<delimiter>      ::= "(" | ")" | "[" | "]" | "{" | "}" | ";" | "\n"

<visibility-enum> ::= "_undef" | "_const" | "_flow" | "_static" | "_var"
```

> 说明：
> - `.()`：指针解引用操作（`.()` 内可写目标类型，也可省略）。
> - `->`：指针成员访问（`p->field`，等价 `p.()->field`；8/19 起 A 方案与 IR 层双线支持）。
> - `?.` / `?(`：安全成员访问 / 安全解引用（带可见性/边界检查）。
> - `?=`：安全赋值。
> - `..`：数组/切片范围 `[start..end]`。
> - `::`：保留（作用域解析，预留）。
> - `#`：语句终止符（与 `;`、换行等价）。
> - 换行符 `\n` 在大多数上下文中是语句终止符；`;` 与 `#` 显式终止。

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
                   | "short" | "int" | "long" | "float" | "double"

<named-type>     ::= <identifier>          (* 用户定义类型：struct/union/enum/alias *)

(* 数组类型 *)
<array-type>     ::= <type-name> "[" [ <array-size> ] "]"
<array-size>     ::= <int-literal>
                   | <int-literal> ".."          (* 固定容量 N（1.0 语义）：u16[6...] 即容量 6；自动增长留 2.0 *)
                   | ".."                        (* 动态数组（1.0 仅声明/索引，不自动增长），如 i8[...] *)
                   | "..."                       (* 等价动态数组 *)

(* 指针类型 *)
<pointer-type>   ::= "void" { "[" [ <array-size> ] "]" }      (* 通用指针 / 多维指针数组 *)
                 | "void" "(" <param-type-list> ")" [ <type-name> ]   (* 见函数指针 *)

(* 函数指针类型 *)
<function-ptr-type> ::= "void" "(" [ <param-type-list> ] ")" [ <type-name> ]
<param-type-list>   ::= <type-name> { "," <type-name> }
```

> 说明：
> - `char[]` 表示字符串类型（动态长度）；`string` 为其关键字别名。
> - `void` 单独出现表示**通用指针**；`void[n]` 为 n 元指针数组；`void[n][m]` 为多维指针数组。
> - 函数指针类型 `void( 参数类型列表 ) [ 返回类型 ]`：无返回类型表示无返回值；
>   返回类型为 `void`（通用指针）时，变量声明前缀必须与其返回存储期一致（`flow`/`static`/`const`）。

### 3.1 结构体/联合体/枚举字段

```
<field-decl>     ::= [ <field-attr> ] <identifier> <type-name> [ ":" <int-literal> ] [ "=" <expr> ]
<field-attr>     ::= "const" | "flow" | "static" | "var"
```

> `:` 后为位域宽度（bits），如 `flag u8:1`。

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
> - `var` 为局部变量显式修饰，可省略（`var x i32 = 0` 与 `x i32 = 0` 等价）。
> - `const`：模块级静态 / 块级自动、不可变；`static`：静态、可变；`flow`：动态（拥有所有权，自动释放）；`var`：局部自动存储期。
> - 指针声明**必须初始化**，不允许声明空指针。

---

## 5. 表达式（Expressions）

```
<expr>           ::= <assignment-expr>

<assignment-expr> ::= <conditional-expr>
                   | <unary-expr> <assign-op> <assignment-expr>
<assign-op>      ::= "=" | "+=" | "-=" | "*=" | "/=" | "%="
                   | "&=" | "|=" | "^=" | "<<=" | ">>=" | "?="

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
                   | ( "-" | "!" | "~" | "&" | "*" ) <unary-expr>
                   | ( "++" | "--" ) <unary-expr>

<postfix-expr>   ::= <primary-expr> { <postfix-op> }
<postfix-op>     ::= "(" [ <expr> { "," <expr> } ] ")"        (* 调用 *)
                   | "[" [ <expr> [ ".." <expr> ] ] "]"       (* 下标/切片 *)
                   | "." <identifier>                         (* 成员访问 *)
                   | "->" <identifier>                        (* 指针成员访问（p->field，A/IR 双线支持） *)
                   | "?." <identifier>                        (* 安全成员访问 *)
                   | ".(" [ <type-name> ] ")"                 (* 解引用 *)
                   | "?(" [ <type-name> ] ")"                 (* 安全解引用 *)
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
                   | "offsetof" "(" <type-name> "," <identifier> ")"
                   | "bitoffsetof" "(" <type-name> "," <identifier> ")"
                   | "holdof" "(" <type-name> "," <expr> ")"
                   | "structof" "(" <type-name> "," <expr> ")"
                   | "unionof" "(" <type-name> "," <expr> ")"
                   | "visof" "(" <expr> ")"
                   | "len" "(" <identifier> ")"
                     (* len(x)：x 的逻辑长度——数组=容量、动态字符串 char[]=字面量长、
                        切片变量 s=arr[lo..hi] 的边界差 hi-lo（编译期常量，返回编译期值），2026-08-19 *)

<malloc-expr>    ::= "malloc" "(" <type-name> ")"
                   | "malloc" "(" <type-name> "[" <int-literal> "]" ")"
```

> 优先级（从高到低）：后缀 → 一元 → `* / %` → `+ -` → 移位 → 关系 → 相等 → `&` → `^` → `|` → `&&` → `||` → 三元 → 赋值。

---

## 6. 语句（Statements）

```
<statement>      ::= <var-decl> | <assign-stmt> | <if-stmt> | <switch-stmt>
                   | <loop-stmt> | <return-stmt> | <break-stmt> | <continue-stmt>
                   | <goto-stmt> | <block-stmt> | <expr-stmt> | <empty-stmt>

<assign-stmt>    ::= <unary-expr> <assign-op> <expr>

<block-stmt>     ::= "{" { <statement> } "}"

<if-stmt>        ::= "if" <expr> <block-stmt> [ "else" ( <if-stmt> | <block-stmt> ) ]

<switch-stmt>    ::= "switch" "(" <expr> ")" "{" { <case-clause> } [ "default" ":" { <statement> } ] "}"
<case-clause>    ::= "case" <expr> ":" { <statement> }

<loop-stmt>      ::= <for-stmt> | <while-stmt> | <do-stmt>

<for-stmt>       ::= "for" <for-init> ";" <expr> ";" <for-step> <block-stmt>
<for-init>       ::= <identifier> "=" <expr> | <var-decl>
<for-step>       ::= <identifier> <assign-op> <expr>
                   | <identifier> "++" | <identifier> "--"

<while-stmt>     ::= "while" <expr> <block-stmt>
                   | "while" <expr> <block-stmt> { <is-clause> }    (* 带模式匹配 *)

<do-stmt>        ::= "do" <expr> <block-stmt>

<is-clause>      ::= "is" <pattern> <block-stmt>

<pattern>        ::= "_"                                (* 通配符 *)
                   | <int-literal>                      (* 整数字面量 *)
                   | "-" <int-literal>                  (* 负整数字面量 *)
                   | <int-literal> ".." <int-literal>   (* 闭区间范围 *)
                   | <enum-variant>                     (* 枚举变体 *)
                   | <visibility-enum>                  (* 可见性枚举 *)
                   | <struct-destructure>               (* 结构体解构 *)
                   | <adt-destructure>                  (* ADT 变体解构 — 预留 *)
                   | <identifier>                       (* 变量绑定 *)

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
> - `is` 模式匹配**仅**用于 `while` 循环体内，不可独立使用。循环条件表达式的值赋给隐式变量 `__is_val`，`is` 对其进行匹配。`do` 不支持 `is`，因为 `do` 先执行块再判断条件，`__is_val` 的语义容易产生混乱。
> - 合法模式：通配符 `_`、整数字面量（含负整数）、闭区间范围 `lo..hi`、枚举变体、可见性枚举（`_flow` 等）、结构体解构（预留）、ADT 变体解构（预留）、标识符变量绑定。
> - 标识符消歧：裸标识符若为已知枚举变体（编译器符号表可查）则按值匹配，否则视为变量绑定。
> - 匹配顺序：多个 `is-clause` 按源码顺序求值，首个匹配者执行（无 fallthrough）。
> - 结构体解构和 ADT 变体解构为预留语法，待类型系统支持后实现。
> - `while var1 += 1 { ... }`：循环条件允许赋值表达式。
> - `do <expr> { ... }`：条件在块外（求值后执行块），区别于 C 的 do-while。
> - 标签 `name:` 与 goto 配套：函数内先定义或后定义均可（跳转目标延迟解析）；IR 与全量实现一致（2026-08-19）。

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
| 解引用 `.()`、安全解引用 `?(`、`?.` | §5 postfix |
| 指针成员访问 `p->field` | §5 postfix |
| 安全赋值 `?=` | §5 assign |
| 范围 `..` | §5 postfix |
| 多变量声明 `var {a=0,b=1} i8` | §4 |
| 复合赋值 `+=` 等 | §5 |
| 三元 `?:`、逻辑/位运算 | §5 |
| 内置函数 sizeof/typeof/…/visof/malloc | §5 |
| if/else、switch/case、for/while/do、break/continue/goto | §6 |
| `is` 模式匹配（循环内） | §6 |
| 函数属性 `[[inline]]` 等、原型 `;` | §7 |
| cooking 编译期、static_assert | §8 |
| align 对齐块 | §8 |
| 可见性枚举 `_const`/`_flow`/`_static`/`_var`/`_undef` | §1.4 |
| `#` 语句终止符 | §1.4 / §6 |
