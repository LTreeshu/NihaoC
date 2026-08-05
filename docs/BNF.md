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
                   | "struct" | "union" | "enum" | "alias" | "multireturn"
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

<delimiter>      ::= "(" | ")" | "[" | "]" | "{" | "}" | ";" | "\n"

<visibility-enum> ::= "_undef" | "_const" | "_flow" | "_static" | "_var"
```

> 说明：
> - `.()`：指针解引用操作（`.()` 内可写目标类型，也可省略）。
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
                   | "multireturn" "struct" "{" { <field-decl> } "}"

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
                   | <int-literal> ".."          (* 初始大小 + 动态增长，如 u16[6...] *)
                   | ".."                        (* 纯动态数组，如 i8[...] *)
                   | "..."                       (* 等价动态数组 *)

(* 指针类型：以 void 为基的多维指针/数组指针 *)
<pointer-type>   ::= "void" { "[" [ <array-size> ] "]" }
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
> - `const`：静态、不可变；`static`：静态、可变；`flow`：动态（拥有所有权，自动释放）；`var`：局部自动存储期。
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
                   | <is-stmt>

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

<do-stmt>        ::= "do" <expr> <block-stmt> { <is-clause> }

<is-clause>      ::= "is" <pattern> <block-stmt>
<pattern>        ::= <expr> | <int-literal> ".." <int-literal> | <visibility-enum>

<is-stmt>        ::= "is" <pattern> "=>" <statement>      (* 单语句模式匹配（如 while 内） *)

<return-stmt>    ::= "return" [ <expr> ]
<break-stmt>     ::= "break"
<continue-stmt>  ::= "continue"
<goto-stmt>      ::= "goto" <identifier>

<expr-stmt>      ::= <expr>
<empty-stmt>     ::= ";" | "#"
```

> 说明：
> - `is` 模式匹配用于 `while` / `do` 循环体内，如 `is -1 { break }`、`is 0..50 { continue }`。
> - `while var1 += 1 { ... }`：循环条件允许赋值表达式。
> - `do <expr> { ... }`：条件在块外（求值后执行块），区别于 C 的 do-while。

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
<cooking-item>   ::= <var-decl> | <cooking-call> | <static-assert>

<cooking-call>   ::= "const" <identifier> "(" <expr> { "," <expr> } ")" [ <identifier> ]
                   | <identifier> "(" <expr> { "," <expr> } ")"     (* 编译期函数调用，如 maker("var", id++) *)

<static-assert>  ::= "static_assert" "(" <expr> "," <string-literal> ")"

<align-block>    ::= "align" <int-literal> "{" { <top-level> } "}"
```

> 说明：
> - `cooking`：编译期执行块。块内可声明编译期变量/常量、调用编译期函数（如 `cooking PI = 3.1415926` 定义编译期常量、`cooking { const X i32 = ... }` 计算常量、`static_assert` 编译期断言）。
> - `align n { ... }`：块内类型按 n 字节对齐。

---

## 9. 存储期兼容矩阵（语义约束，非语法）

赋值/传递时**目标变量的存储期必须不短于源变量**（目标寿命 ≥ 源寿命），否则为编译错误：

| 源 \ 目标 | `const` | `static` | `flow` | `var` |
| ------- | ------- | -------- | ------ | ----- |
| `const`  | 安全 | 错误 | 错误 | 错误 |
| `static` | 安全 | 安全 | 错误 | 错误 |
| `flow`   | 安全 | 错误 | 安全 | 错误 |
| `var`    | 安全 | 错误 | 错误 | 安全 |

指针所有权/借用传递规则（`flow`→`var` 可变借用冻结源、`flow`→`flow` 所有权转移失效源、`flow`→`static` 禁止等）详见 [`Chinese.md`](./Chinese.md) 第 12 章。

---

## 附：语法完整性对照（已实现特性 → BNF 规则）

| 特性 | BNF 规则 |
| ---- | -------- |
| 模块/use/link/linkas | §2.1 |
| alias 类型别名 | §2.2 |
| struct/union/enum 命名定义、multireturn | §2.2 |
| 位域 `name u8:1` | §3.1 |
| 数组/动态数组/切片 | §3 |
| 多维指针数组 `void[n][m]` | §3 |
| 函数指针 `void(u8) i32`、函数指针数组 | §3 |
| 解引用 `.()`、安全解引用 `?(`、`?.` | §5 postfix |
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
