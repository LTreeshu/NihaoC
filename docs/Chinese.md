# NiHao 编程语言参考手册

## 1. 概述

NiHao 是一种新型静态编译语言，专为系统级编程和高性能应用设计，融合了现代语言特性与底层控制能力。

## 2. 基本语法

### 2.1 注释

```nihao
// 单行注释
/* 多行注释 */
```

### 2.2 语句分隔

- 语句以换行符分隔或者`;`换行,可选
- 多语句同行使用分号分隔：`stmt1; stmt2`

### 2.3 内置函数

- `typeof(type)` 类型判断 返回类型
- `sizeof(type)` 长度判断 返回长度
- `alignof(type)` 对齐判断 返回对齐长度
- `structof(member)` 从属判断 返回成员所有者
- `unionof(member)` 从属判断 返回成员所有者
- `offsetof(type,member)` 返回偏移量
- `bitoffsetof(type,bitmember)` 返回偏移量
- `len(x)` 逻辑长度（2026-08-19）：数组=容量；动态字符串 `char[]`=字面量长度；
  切片变量 `s = arr[lo..hi]`=边界差 `hi-lo`（边界须编译期常量，返回编译期值）
- `visof(var)` 可见性判断 返回可见属性

### 2.4 关键字说明

- `alias` 类型别名
- `const` 修饰固定可见不可变
- `flow` 修饰动态可见
- `static` 修饰静态可见
- `var` 修饰局部可见 可自动推断
- `_undef` 未定义不可见属性枚举值
- `_const` 固定可见不可变属性枚举值
- `_flow` 动态可见属性枚举值
- `_static` 静态可见属性枚举值
- `_var` 局部可见属性枚举值
- `cooking {...}` 编译期执行代码块关键字
- `align n {...}` 字节对齐代码块
- `use ...` 模块引用
- `module ...` 模块定义
- `linkas "..."` 静态库导出命名
- `link "..." ... ` 静态库导入使用
- `func` 无返回值属性 和 无返回值 函数定义
- `[[inline]]` 内联函数属性修饰
- `[[weak]]` 弱定义函数属性修饰
- `[[local]]` 内部链接函数属性修饰
- `[[used]]` 强制保留函数属性修饰
- `[[unused]]` 强制弃用函数属性修饰
- `[[export] ".my_section"]` 导出到指定段函数属性修饰

## 3. 类型系统

### 3.1 基础类型

| 类型       | 描述             | 大小     |
| -------- | -------------- | ------ |
| `void`   | 通用指针类型         | 机器指针大小 |
| `char[]` | 字符串类型          | 动态     |
| `char`   | 字符类型           | 1字节    |
| `u8`     | 无符号8位整型        | 1字节    |
| `u16`    | 无符号16位整型       | 2字节    |
| `u32`    | 无符号32位整型       | 4字节    |
| `u64`    | 无符号64位整型       | 8字节    |
| `i8`     | 有符号8位整型        | 1字节    |
| `i16`    | 有符号16位整型       | 2字节    |
| `i32`    | 有符号32位整型       | 4字节    |
| `i64`    | 有符号64位整型       | 8字节    |
| `f32`    | 单精度浮点          | 4字节    |
| `f64`    | 双精度浮点          | 8字节    |

> f32 严格宽度（2026-08-19 实现）：赋值/初始化时按单精度舍入截断（存储截断语义），
> 运算仍按 double 提升；`f32 x = 0.1` 存回后 `x != 0.1`（f64 字面量）。
| `fx32`   | 单精度定点数(Q16.16) | 4字节    |
| `fx64`   | 双精度定点数(Q32.32) | 8字节    |

### 3.2 复合类型

基本的定义声明结构 ` [属性] [名字] [类型] `
**数组声明：**

```nihao
static fixedArray char[3]       // 固定大小数组 char[3] 为类型
flow dynamicArray i8[...]   // 动态数组（1.0 语义：仅声明/索引，不自动增长；扩容留 2.0）
var initArray u16[6...]   // 有初始容量的数组（1.0 语义：固定容量 6，不自动增长）

// 数组索引访问赋值
fixedArry[0] = 0

// 数组切片访问赋值
dynamicArray[2..3] = {3,4}
```

**类型别名：**

```nihao
alias Byte = u8
alias StringPtr = char[]
```

**类型定义：**

```nihao
Person struct{
    name char[]
    age u8
    flag u8:1 // 支持位域语法
}
```

**共用体：**

```nihao
Data union{
    asInt i32
    asFloat f32
}
```

**枚举：**

```nihao
Color enum{ RED, GREEN, BLUE }
```

### 3.3 类型操作

#### 3.3.1 类型对齐

```nihao
// 类型判断
if typeof(value) == i32 { ... }

// 类型大小
size u8 = sizeof(Person)

// 类型对齐
align 4 {
  Protocol struct {
    data u8
    len  u32
    flag u32:1 
    tag  u32:2
  }
}  // 4字节对齐
```

#### 3.3.2 类型嵌套

```nihao
// 普通嵌套
aunion union{
    value u16
    reg struct{
        r0 u8
        r1 u8
    }
}
aunion.reg.r1 = 1

// 匿名嵌套
xunion union{
    value u8
    struct{
        r0 u8:2
        r1 u8:2
        r2 u8:2
        r3 u8:2
    }
}
xunion.r1 = 1
```

**命名类型嵌套（2026-08-19 实现）**——链式成员访问 + 整体赋值拷贝：

```nihao
Point struct { x i32 y i32 }
Line struct { a Point b Point }

l Line
l.a.x = 1
l.b.y = 2            // 链式访问（递归展开偏移）

m Line
m = l                // 整体赋值（逐成员拷贝，嵌套递归展开；拷贝后互不影响）

n Line = {{1, 2}, {3, 4}}   // 嵌套初始化列表（递归填充，2026-08-26）

// union 嵌套：聚合成员共享槽（总槽数=最大成员，2026-08-27）
U union { a Point b Point }
un U
un.a.x = 1
un.b.y = 5          // 写 b.y 覆盖共享槽 → un.a.y == 5
```

## 4. 变量声明与可见性

### 4.1 声明修饰符

| 修饰符      | 描述          |
| -------- | ----------- |
| `const`  | 定义固定可见不可变变量 |
| `flow`   | 定义动态可见变量    |
| `static` | 定义静态可见变量    |
| `var`    | 定义局部可见变量    |

### 4.2 示例

```nihao
const MAX_SIZE i32 = 1024
flow counter i8 = 0 
static globalVar f32 = 3.14
{var inferred char[] = "Hello"}
// 局部变量不强制 var 前缀, 可以忽略不写
{localstr char[] = "Hello"}

// 多变量声明
var {a = 0,b = 1,c = 0} i8
var {aa = "aa",bb = "bb",cc = "cc"} char[2]
var {aaa = "aaa",bbb = "bbb",ccc = "ccc"} char[]
```

## 5. 指针与内存管理

### 5.1 指针操作

#### 5.1.1 指针定义

```nihao
// 不允许声明空指针，声明时必须赋值
variable i8 = 0
varptr void = &var

// 单级指针
ptr void = malloc(i32)   // 分配内存
ptr.(i32) = 42           // 解引用赋值
ptr?.(i64)               // 安全解引用 编译将报错 因为 i64 > i32 不能越界

// 多级指针
ptr2 void[] = &ptr       // 二级指针定义
ptr = ptr2.()            // 一层解引用 解引用void类型时()内可以略写
variable = ptr2[].(i32)  // 二层解引用

ptr3 void[][] ?= &ptr2    //三级指针定义
ptr2 = ptr3.()            // 一层解引用
ptr  = ptr3[].()          // 二层解引用
variable = ptr3[][].(i32) // 三层解引用
```

> **指针声明语法（2026-08-19 定案：隐式推断 + 显式声明双支持）**
>
> - **隐式推断**：`p = &x` 自动推断为指向 `x` 类型的指针（无需写类型名）。

> - **`->` 指针成员访问**：`p->field` 等价 `p.()->field`，支持链式 `p->a->b` 与复合赋值 `p->n += 1`（A 方案与 IR 层双线一致，2026-08-19 起）。

#### 5.1.2 数组指针

```nihao
arry char[9] = {1,2,3,4,5,6,7,8,9}
arryptr void = &arry           // 获取数组指针
arryptr[0] = 0
arryptr[9] = 9                 // 未定义行为
arryptr.(char[9])[0] = 0       // 对[0]成员解引用
// arryptr.(char[9])[9] = 9       // 编译错误:越界



arrybuffer char[8] = arryptr.(char[9])[0..7]
// arrybuffer == {0,1,2,4,5,6,7,8}

// 数组指针数组
arryptr2 void[2] = {&arry,&arrybuffer}
arryptr2[0].(char[9])[8] = arry[8]
arryptr2[1].(char[8])[7] = arrybuffer[7]
```

#### 5.1.3 指针数组

```nihao
dptrarry1 void[3] = malloc(void[3]) // 动态分配一维指针数组
dptrarry1[2] = ptr
dptrarry1[2].(i32) += 1

dptr3 void[4][5] = malloc(void[4][5]) // 动态分配二维指针数组
dptr3[3][4] = ptr       // 安全指针传递
// 错误：dptr3[0][0].(int64) error: int64 type size > i32 type size!
dptr3[3][4].(i32) += 1  // 多级指针解引用 

// 指针数组指针
ptrarry void = &arryptr2
ptrarry.(void[2])[0].(char[9])[8] = 8
ptrarry.(void[2])[1].(char[8])[7] = 7

// arry == {0,1,2,4,5,6,7,8,8}
// arrybuffer == {0,1,2,4,5,6,7,7}
```

#### 5.1.4 复合体指针

```nihao
Say struct{
    name char[9]
    say char[]
}
xiaoming Say 
stptr void = &xiaoming
stptr.(char[9])[0..8] = "xiaoming"  // 指针切片赋值
stptr.(Say).say = "NiHao I am xiaoming!" // 指针类型引用
talk = xiaoming.say
puts(talk)
// puts(talk) out--> "NiHao I am xiaoming!"
```

#### 5.1.5 函数指针

**类型语法：**  
函数指针类型由 `void(参数类型列表)` 表示，若函数有返回值，则在末尾加上返回值类型。例如：  

- `void(u8, char[])` 表示无返回值、参数为 `u8` 和 `char[]` 的函数指针；  
- `void(u8, char[]) i32` 表示返回 `i32` 的函数指针；  
- 若返回通用指针（`void` 指针），则需在返回值位置写 `void`，同时**必须**在变量声明时使用对应的属性前缀（`flow`/`static`/`const`）来标明内存类别，例如 `flow cb void(u8) void` 表示返回动态内存的函数指针, 函数指针变量的属性必须与返回值的变量一致。

**变量声明：**  
完全遵循 `[属性] [变量名] [类型]` 的通用范式，例如：  

```nihao
var call void(i32, i32)        // 无返回的函数指针
var add_cb void(i32, i32) i32  // 返回 i32 的函数指针
flow factory void(char[]) void  // 返回动态内存指针的函数指针
static loader void() void       // 返回静态内存指针的函数指针
const getter void() void        // 返回只读指针的函数指针
```

**函数指针的赋值与调用：**

```nihao
// 定义目标函数（无返回值）
func callback_handle(argc u16) {
    puts("callback call!")
}
// 赋值给函数指针变量（类型匹配）
var cb void(u16) = callback_handle
// 调用
cb(100)

// 有返回值的函数指针
func add(a i32, b i32) i32 { return a + b }
var calc void(i32, i32) i32 = add
result = calc(10, 20)  // result == 30

// 返回动态内存的函数指针（必须用 flow 接收）
flow create_user(name char[]) void {
    flow user void = malloc(sizeof(User))
    user.(User).name = name
    return user
}
flow factory void(char[]) void = create_user
flow user = factory("Alice")  // 自动管理生命周期

// 返回静态内存的函数指针（用 static 或 const 接收）
static get_config() void {
    static config Config = { .port = 8080 }
    return &config
}
static loader void() void = get_config   // static → static ✅ 推荐
const config_reader void() void = get_config  // static → const ✅ 只读借用
// var bad_loader void() void = get_config  // static → var ❌ 编译错误

// 返回只读指针（必须用 const 接收）
const get_version() void {
    static ver char[] = "v2026"
    return &ver
}
const version_getter void() void = get_version
```

**函数指针作为参数：**

```nihao
// 注册回调（参数类型直接写 void(u32)）
func register_callback(cb void(u32)) {
    global_cb = cb
}

// 带动态内存返回的回调参数（必须用 flow 接收）
func process_async(flow handler void(i32) void) {
    flow data = handler(100)
    // ...
}

// 调用示例
func my_handler(x i32) void { return malloc(16); }
process_async(my_handler)   // 自动匹配 flow 属性
```

**函数指针数组（类型名 `void(...)` 后直接加 `[n]`）：**

```nihao
// 无返回值函数指针数组
func handle_a(u8) { puts("A"); }
func handle_b(u8) { puts("B"); }
var table void(u8)[2] = { handle_a, handle_b }
table[0](1)  // 调用 handle_a

// 返回动态指针的函数指针数组（每个元素都是 flow 回调）
flow create_packet(u8) void { return malloc(64); }
flow create_frame(u8) void { return malloc(128); }
flow dispatcher void(u8)void[2] = { create_packet, create_frame }
flow packet = dispatcher[0](0x01)
```

**类型别名（alias）简化复杂类型：**

```nihao
alias Callback = void(u8, i32) i32
alias AsyncFactory = void(char[]) void
alias ConfigLoader = void() void

var handler Callback = some_function
flow factory AsyncFactory = create_async
static loader ConfigLoader = get_config
```

**函数指针的接收规则（强制遵循第7.3节）：**  

- 若函数指针类型为 `void(...) void`（返回 `void` 指针），变量前缀必须与其返回属性一致：返回动态指针用 `flow`，返回静态指针用 `static`，返回只读指针用 `const`。  
- 若返回非指针类型（如 `i32`、`char[]`）或无返回值，则前缀可为 `func` 或 `var`（可省略）。  
- 错误示例（编译报错）：`flow bad void() void = get_static_ptr`（静态返回赋给 flow 被拒绝）。

### 5.2 内存判断

```nihao
// 可见性判断
if visof(ptr) == _static { 
    // ...
}

// 从属判断
var boy Person = {"xiaoming", 13}
var ptr void = &boy.name
if structof(Person,ptr) == boy { 
    // ...
}
```

## 6. 控制结构

### 6.1 条件语句

```nihao
if condition {
    // ...
} else if anotherCondition {
    // ...
} else {
    // ...
}
```

### 6.2 循环结构

**do 循环：**

```nihao
do value > 0 {
    value++
    if value == 100 {
        break
    }
    else if value == 50{
      continue
    }
}
```

**while 循环（带模式匹配）：**

```nihao
var1 u8
while var1 += 1 {
    is -1 {
        break
    }
    is 0..50 {
        continue
    }
    is _ {
        break
    }
}
```

**for 循环：**

```nihao
for i = 0; i < 10; i++ {
    // ...
}
```

**goto 与标签（2026-08-19）：**

```nihao
i i32 = 0
loop:
i = i + 1
if i < 3 {
    goto loop        // 跳转到标签（先定义或后定义均可，函数内唯一）
}
```

### 6.1 模式匹配（`is` 子句）

`is` 子句配合 `while` 循环使用，对循环条件表达式的值（隐式存储于 `__is_val`）进行模式匹配。`do` 不支持 `is`，因为 `do` 先执行块再判断条件，`__is_val` 的语义容易产生混乱。

#### 语法

```bnf
<is-clause>      ::= "is" <pattern> <block-stmt>

<pattern>        ::= "_"                                (* 通配符 *)
                   | <int-literal>                      (* 整数字面量 *)
                   | "-" <int-literal>                  (* 负整数字面量 *)
                   | <int-literal> ".." <int-literal>   (* 闭区间范围 *)
                   | <enum-variant>                     (* 枚举变体 *)
                   | <visibility-enum>                  (* 可见性枚举 *)
                   | <struct-destructure>               (* 结构体解构 — 预留 *)
                   | <adt-destructure>                  (* ADT 变体解构 — 预留 *)
                   | <identifier>                       (* 变量绑定 *)
```

#### 模式语义

| 模式 | 匹配条件 | 绑定 | 示例 |
|------|----------|------|------|
| `_` | 总是匹配 | 无 | `is _ { break }` |
| `<int>` | `__is_val == N` | 无 | `is 0 { break }` |
| `-<int>` | `__is_val == -N` | 无 | `is -1 { break }` |
| `lo..hi` | `__is_val >= lo && __is_val <= hi` | 无 | `is 0..50 { continue }` |
| `<enum-variant>` | `__is_val == VARIANT_VAL` | 无 | `is RED { ... }` |
| `<vis-enum>` | `visof == NH_*` | 无 | `is _flow { ... }` |
| `<identifier>` | 总是匹配 | 绑定值到新变量 | `is x { printf(x) }` |
| `Struct(f1, f2)` | 类型匹配 + 字段解构 | 绑定各字段 | `is Point(x, y) { ... }` |
| `Variant(pat)` | ADT tag 匹配 + 子模式 | 绑定载荷 | `is Some(v) { ... }` |

#### 语义规则

- **R1 — `__is_val` 类型**：等于 `while` 条件表达式的类型。
- **R2 — 变量绑定作用域**：限于 `is-clause` 的 `block-stmt` 内部。
- **R3 — 结构体解构字段匹配**：支持按位置、具名（`.field`）、忽略（`_`）、混合嵌套。
- **R4 — ADT 变体匹配（未来）**：tag 检查 + 载荷绑定。

#### 标识符消歧

裸标识符若为已知枚举变体（编译器符号表可查）则按值匹配，否则视为变量绑定。

#### 匹配顺序

多个 `is-clause` 按源码顺序求值，首个匹配者执行（无 fallthrough）。

#### 示例

**枚举匹配：**

```nihao
Color enum { RED, GREEN, BLUE }

c Color = RED
while c {
    is RED {
        puts("red")
    }
    is GREEN {
        puts("green")
    }
    is _ {
        break
    }
}
```

**结构体解构（`__is_val` 为结构体值）：**

```nihao
Point struct { x i32; y i32 }

// while 条件返回 Point，__is_val 即为该结构体
while get_next_point() {
    is Point(x, y) {
        // x = __is_val.x, y = __is_val.y（隐式解构绑定）
        printf("x=%d y=%d\n", x, y)
    }
    is _ {
        break
    }
}
```

**ADT 变体解构（未来语法，需语言先支持带载荷枚举）：**

```nihao
(* 未来语法 — Option 为带载荷的枚举，当前不存在 *)
Option enum { Some(i32), None }

opt Option = Some(42)
while opt {
    is Some(v) {
        // 匹配 Some 变体，将载荷绑定到 v
        printf("got %d\n", v)
        opt = None
    }
    is None {
        break
    }
}
```

## 7. 函数定义

### 7.1 函数声明语法

#### 7.1.1 核心规则

> **所有函数声明遵循统一的布局：**
> 
> ```
> [方括号属性] [返回值属性] 函数名(参数列表) [返回类型] { 函数体 }
> ```
> 
> - **方括号属性**（可选）：修饰函数的链接性或编译器行为，如 `[[local]]`、`[[inline]]` 等
> - **返回值属性**（必选）：决定函数返回值（或函数自身）的可见性/生命周期
> - **返回类型**：当返回 `void` 指针时必须写 `void`；当返回非指针类型时写具体类型；无返回值时可省略

#### 7.1.2 返回值属性的选择规则

| 返回情况                                | 必须使用的属性  | 说明                    |
| ----------------------------------- | -------- | --------------------- |
| 无返回值                                | `func`   | 普通函数，无返回值             |
| 返回非指针类型（如 `i32`, `f64`, `Person` 等） | `func`   | 返回值是普通类型              |
| 返回 `void` 指针（动态内存）                  | `flow`   | 返回动态分配的指针，由调用方拥有，自动释放 |
| 返回 `void` 指针（静态存储期）                 | `static` | 返回指向静态内存的指针，生命周期为程序全程 |
| 返回 `void` 指针（只读）                    | `const`  | 返回只读指针，不可修改指向的内容      |

#### 7.1.3 方括号属性（链接性与编译器行为）

| 属性                     | 作用              | 适用场景         |
| ---------------------- | --------------- | ------------ |
| `[[local]]`           | 内部链接，仅当前文件可见    | 私有辅助函数       |
| `[[inline]]`           | 建议编译器内联展开       | 频繁调用的短小函数    |
| `[[weak]]`             | 弱符号定义，可被同名强符号覆盖 | 库提供的默认实现     |
| `[[used]]`             | 强制保留符号，即使未被引用   | 被调试器或汇编调用的函数 |
| `[[unused]]`           | 标记为已弃用，触发编译警告   | 过渡期旧接口       |
| `[[export] ".section"]` | 导出到指定的段         | 链接器脚本控制的特殊段  |

**组合规则**：方括号属性与返回值属性**可共存**，方括号属性在前。

---

### 7.2 完整函数修饰表

| 场景          | 写法示例                                                | 说明            |
| ----------- | --------------------------------------------------- | ------------- |
| 外部链接，无返回值   | `func greet() { ... }`                              | 最常见的无返回函数（默认外部链接）     |
| 外部链接，返回非指针  | `func add(a i8, b i8) i8 { ... }`                   | 返回普通类型（默认外部链接）        |
| 外部链接，返回动态指针 | `flow create_buffer(size u32) void { ... }`         | 返回动态内存（默认外部链接）        |
| 外部链接，返回静态指针 | `static get_counter() void { ... }`                 | 返回静态变量地址（默认外部链接）      |
| 外部链接，返回只读指针 | `const get_version() void { ... }`                  | 返回只读数据（默认外部链接）        |
| 内部链接，返回动态指针 | `[[local]] flow create() void { ... }`             | 仅本文件可见，返回动态内存 |
| 内部链接，返回静态指针 | `[[local]] static get() void { ... }`              | 仅本文件可见，返回静态内存 |
| 内部链接，返回非指针  | `[[local]] func helper(x i32) i32 { ... }`         | 仅本文件可见的普通函数   |
| 内联，返回动态指针   | `[[inline]] flow create_small() void { ... }`       | 建议内联          |
| 弱符号，返回静态指针  | `[[weak]] static get_default() void { ... }`        | 允许覆盖的默认实现     |
| 弃用，返回动态指针   | `[[unused]] flow old_api() void { ... }`            | 触发弃用警告        |
| 导出到段，返回静态指针 | `[[export]".init"] static init_data() void { ... }` | 放入指定段         |

---

### 7.3 调用方接收规则

函数返回值属性决定了**调用方必须使用何种属性来接收返回值**，以保证内存安全。

| 函数返回属性   | 调用方可使用的接收属性       | 禁止的属性                     | 原因                        |
| -------- | ----------------- | ------------------------- | ------------------------- |
| `flow`   | `flow`            | `func`, `static`, `const` | 所有权必须转移，只有 `flow` 拥有内存管理权 |
| `static` | `static`, `const` | `flow`, `var`             | 静态内存生命周期最长，可安全借用，但不可被释放   |
| `const`  | `const`           | `func`, `static`, `flow`  | 只读约束必须保持，禁止可修改的接收方式       |

```nihao
// 接收示例
flow buf void = create_buffer(1024)      // flow → flow ✅
static p void = get_counter()            // static → static ✅
const ver void = get_version()           // const → const ✅
// flow bad = get_version()              // const → flow ❌ 编译错误
```

---

### 7.4 完整示例集

#### 7.4.1 `func` 修饰 —— 无返回值或返回值非指针

```nihao
// 无返回值
func greet() {
    print("Hello, Nihao C!")
}

// 无返回值，有参数
func log(msg char[]) {
    print("[LOG] ", msg)
}

// 返回非指针类型
func add(a i8, b i8) i8 {
    return a + b
}

// 返回结构体（非指针）
Person struct { name char[], age i32 }
func make_person(name char[], age i32) Person {
    return Person{name, age}
}

// 返回数组
func make_array() i32[3] {
    return {1, 2, 3}
}
```

#### 7.4.2 `flow` 修饰 —— 返回动态内存指针

```nihao
// 返回动态内存指针
flow create_buffer(size u32) void {
    return malloc(size)
}

// 返回动态结构体指针
flow create_person(name char[], age i32) void {
    flow p void = malloc(sizeof(Person))
    p.(Person).name = name
    p.(Person).age = age
    return p
}

// 内部链接，仅本文件可见
[[local]] 
flow create_internal_buffer(size u32) void {
    return malloc(size)
}
```

#### 7.4.3 `static` 修饰 —— 返回静态内存指针

```nihao
// 返回静态计数器指针
static get_counter() void {
    static count i32 = 0
    return &count
}

// 返回静态配置结构体指针
static get_default_config() void {
    static config Config = {.timeout = 30, .retries = 3}
    return &config
}

// 内部链接 + 返回静态指针
[[local]] 
static get_internal_config() void {
    static config Config = {.timeout = 10}
    return &config
}
```

#### 7.4.4 `const` 修饰 —— 返回只读指针

```nihao
// 返回只读版本字符串
const get_version() void {
    static version char[] = "v1.0.0"
    return &version
}

// 返回只读构建信息
const get_build_info() void {
    static info BuildInfo = {.date = "2025-01-01", .commit = "abc123"}
    return &info
}

// 内部链接，仅本文件可见
[[local]] 
const get_internal_version() void {
    static version char[] = "v1.0.0-internal"
    return &version
}
```

#### 7.4.5 方括号属性组合示例

```nihao
// 内联 + 返回动态指针
[[inline]] 
flow create_small_buffer() void {
    return malloc(64)
}

// 弱符号 + 返回静态指针（可被覆盖）
[[weak]] 
static get_default_handler() void {
    static handler Handler = {.id = 0}
    return &handler
}

// 强制保留 + 无返回值（即使未调用也保留符号）
[[used]] 
func debug_dump() {
    print("Debug dump called")
}

// 弃用标记 + 返回动态指针（触发警告）
[[unused]] 
flow old_create() void {
    return malloc(1024)
}

// 导出到初始化段 + 返回静态指针
[[export] ".init"] 
static get_init_data() void {
    static data InitData = {.magic = 0xDEADBEEF}
    return &data
}

// 多重组合：内部 + 弱符号 + 返回动态指针
[[local, weak]] 
flow create_fast() void {
    return malloc(32)
}
```

---

### 7.5 函数原型声明（无函数体）

函数原型（声明）用于告诉编译器函数的存在，不提供实现，通常用于头文件或交叉引用。

```nihao
// 普通函数声明
func add(a i8, b i8) i8;
flow create_buffer(size u32) void;
static get_counter() void;
const get_version() void;

// 带方括号属性的声明
[[local]] flow create_internal() void;
[[inline]] func square(x i32) i32;
[[weak]] static get_default() void;
```

---

### 7.6 函数指针类型

#### 7.6.1 语法定义

函数指针类型使用 `void()` 形式，参数类型列表和返回类型（可选）：

```
void( 参数类型列表 ) [返回值类型]
```

如果返回类型为void带有属性（如 `flow`, `static`, `const`），则需在返回值类型前加上属性。

#### 7.6.2 示例

```nihao
// 无返回值函数指针：void(u16)
func callback_handle(argc u16) {
    puts("callback called")
}
var cb void(u16) = callback_handle   // cb 是函数指针变量

// 有返回值函数指针：void(u16) i32
flow callback_with_return(argc u16) i32 {
    return 42
}
flow cb2 void(u16)i32 = callback_with_return

// 作为参数传递
func register_callback(flow cb void(u16)i32, event u32) u32 {
    if event == 1 {
        return cb(event)
    }
    return 0
}

// 使用 flow 函数指针
flow async_cb void(u8)void = async_handler
```

---

### 7.7 函数定义速查表

| 需求          | 写法                                      | 返回值属性    | 方括号属性(可选)    |
| ----------- | --------------------------------------- | -------- | ------------ |
| 普通无返回值      | `func greet() { ... }`                  | `func`   | —            |
| 返回非指针       | `func add(a i8, b i8) i8 { ... }`       | `func`   | —            |
| 返回动态指针      | `flow create() void { ... }`            | `flow`   | —            |
| 返回静态指针      | `static get() void { ... }`             | `static` | —            |
| 返回只读指针      | `const get() void { ... }`              | `const`  | —            |
| 内部链接，返回动态指针 | `[[local]] flow create() void { ... }` | `flow`   | `[[local]]` |
| 内部链接，返回非指针  | `[[local]] func helper() i32 { ... }`  | `func`   | `[[local]]` |
| 内联，返回动态指针   | `[[inline]] flow create() void { ... }` | `flow`   | `[[inline]]` |
| 弱符号，返回静态指针  | `[[weak]] static get() void { ... }`    | `static` | `[[weak]]`   |
| 弃用，返回动态指针   | `[[unused]] flow old() void { ... }`    | `flow`   | `[[unused]]` |

---

> **📖 完整 BNF 语法定义**：参见同目录 [`BNF.md`](./BNF.md)，包含函数声明、类型系统、语句表达式、模块系统等完整语法的巴科斯范式。

## 8. 模块系统

### 8.1 模块定义

```nihao
module mathUtils

func add(a i32, b i32) i32 {
    return a + b
}
```

### 8.2 模块使用

```nihao
use mathUtils
```

### 8.3 库链接

```nihao
link "libc.so" libc
```

## 9. 编译指令

### 9.1 常用命令

```bash
nihao init     # 初始化项目
nihao build    # 构建项目
nihao run      # 构建并运行
nihao debug    # 调试模式构建
```

### 9.2 编译期执行

```nihao
cooking {
    // 编译期执行的代码
    const BUILD_TIME = time.now()
    // 编译期变量：const NAME [TYPE] = expr（跨块共享，运行时引用折叠为常量）
    const BASE i32 = 10
    // 编译期函数（宏式展开，2026-08-19）：const NAME(p1, p2) = expr
    const sq(x) = x * x
    static_assert(sq(5) == 25, "sq(5) != 25")   // 支持嵌套 sq(sq(2)) / 组合 sq(cube(2))
}
```

## 10. 示例程序

```nihao
module main
use stdio
use stdlib
link "libhttp.so" http

alias http_client = http.http_client
alias time = stdlib.time

const ConstValue i8 = 100

/* 多返回值：命名结构体（与 C 机制一致） */
MultiResult struct {
    value1 u8
    value2 u8
}

func main() 
{
    puts("Program starting\n")

    // Dynamic memory allocation
    flow dynptr void = malloc(i32)

    // Pointer operations
    dynptr.(i32) = ConstValue

    // Array operations
    arry f32[3] = {1.1, 1.2, 1.3}
    ptrarry void[3] = &arry
    ptrarry2 void[3] = {&arry[2], &arry[1], &arry[0]}

    // Visibility checking
    if visof(staticptr) == _static {
        flow temp void = malloc(float32)
    }

    if visof(dynptr) == _flow {
        puts("the ptr is _flow attribute \n");
    }

    // Multiple return value handling
    returnValue MultiResult = calculate()

    return
    /* If the flow variable: dynptr, temp, is not returned,
     * they will be automatically free.
    */
}

func calculate() MultiResult  
{
    if visof(value) != _undef {
      return {0,0}
    }
    else if visof(ConstValue) == _static {
      return {ConstValue, (ConstValue*2)}
    }
}
```

## 11. 可见性系统与指针生命周期管理

Nihao语言通过统一的属性系统管理变量的存储期、所有权和借用关系，确保内存安全。本章将可见性(存储期)体系与指针的所有权/借用规则整合，形成完整的静态分析框架。

---

### 11.1 变量存储期属性(可见性)

每个变量声明时必须指定以下四种存储期属性之一，用于控制其生命周期和作用域：

| 属性       | 存储期             | 可变性 | 作用域         | 典型用途            |
| -------- | --------------- | --- | ----------- | ---------------- |
| `const`  | 模块级静态 / 块级自动 ¹ | 不可变 | 模块级或声明所在块   | 常量、只读借用          |
| `static` | 静态              | 可变  | 模块级         | 模块内部共享状态         |
| `flow`   | 动态              | 可变  | 函数级/块级     | 动态分配(拥有所有权)      |
| `var`    | 自动              | 可变  | 块级          | 局部变量(自动存储期)      |

¹ `const` 的存储期取决于声明位置：模块级声明具有静态存储期（程序全程存活），块级声明具有自动存储期（随块进出）。两种声明方式的只读语义相同。

```nihao
const MAX_SIZE i32 = 1024     // 模块级静态存储，全局只读
static counter i32 = 0        // 静态存储，模块内可变
flow dynamic_data i32 = 42    // 动态存储，函数内可变
var local_temp i32 = 100      // 自动存储，块内可变
```

---

### 11.2 存储期与赋值安全原则

四种属性的存储期从长到短排列为：

    static (程序全程) > flow (动态) > const = var (自动/块级)

其中 `const` 在模块级声明时具有静态存储期（与 `static` 同级），在块级声明时具有自动存储期（与 `var` 同级）。

赋值时须满足：**目标变量的存储期不短于源变量**（目标寿命 ≥ 源寿命），否则可能导致悬垂指针。

完整的兼容性判定（含所有权与借用语义）见 §12.2 综合传递矩阵。

---

## 12. 指针所有权与借用规则

指针的传递不仅受存储期限制，还涉及**所有权**和**借用**语义。Nihao 将指针分为两类：

- **拥有所有权的指针**：`flow`(动态)和 `static`(静态)，指向的数据由其管理  (释放或持久化 )。
- **借用指针**：`var`(可变借用)和 `const`(只读借用)，不拥有数据，生命周期受制于源指针。

### 12.1 指针传递总矩阵

所有指针赋值/传递的兼容性由以下 4×4 矩阵统一判定。每个单元格格式为：`允许/禁止 (源状态变化)`。

#### 综合传递矩阵

| 源 \ 目标   | `const`        | `static`     | `flow`        | `var`        |
| -------- | -------------- | ------------ | ------------- | ------------ |
| `const`  | 允许 (保持有效)      | 禁止           | 禁止            | 禁止           |
| `static` | 允许 (保持有效，只读借用) | 允许 (保持有效，共享) | 禁止            | 禁止           |
| `flow`   | 允许 (冻结，只读借用)   | 禁止           | 允许 (失效，转移所有权) | 允许 (冻结，可变借用) |
| `var`    | 允许 (冻结，只读借用)   | 禁止           | 禁止            | 允许 (冻结，可变借用) |

此表是编译器进行静态分析的依据，确保所有指针操作既满足生命周期要求，又符合所有权/借用语义。

#### 允许传递的完整清单

从矩阵中提取的 16 条传递规则（含 8 条允许、8 条禁止），供逐条查阅：

| 源 → 目标(均为 `void`)   | 语义     | 源变量状态 |
| ------------------- | ------ | ----- |
| `const` → `const`   | 只读借用   | 保持有效  |
| `static` → `const`  | 只读借用   | 保持有效  |
| `static` → `static` | 共享引用   | 保持有效  |
| `flow` → `const`    | 只读借用   | 冻结    |
| `flow` → `flow`     | 所有权转移  | 失效    |
| `flow` → `var`      | 可变借用   | 冻结    |
| `var` → `const`     | 只读借用   | 冻结    |
| `var` → `var`       | 可变借用   | 冻结    |
| `const` → `var`     | **禁止** | —     |
| `const` → `flow`    | **禁止** | —     |
| `const` → `static`  | **禁止** | —     |
| `flow` → `static`   | **禁止** | —     |
| `var` → `flow`      | **禁止** | —     |
| `var` → `static`    | **禁止** | —     |
| `static` → `var`    | **禁止** | —     |
| `static` → `flow`   | **禁止** | —     |

#### 状态说明

> **冻结**：源指针在借用期间不可读写（类似 Rust 的不可变借用）。
> **失效**：源指针不可再使用（所有权已转移）。
> **保持有效**：源仍可用，不发生变化。

#### 设计原理

矩阵中每个单元格的判定综合了存储期和所有权/借用两个维度：

**允许传递（8 个单元格）**：

| 传递 | 语义 | 为什么允许 |
|------|------|-----------|
| `const` → `const` | 只读借用的再借用 | 只读承诺可以链式传递，源数据不会被修改 |
| `static` → `const` | 静态数据的只读借用 | 只读借用不修改数据，安全；源保持有效 |
| `static` → `static` | 静态数据的共享引用 | 多个 static 指针可安全指向同一静态数据 |
| `flow` → `const` | 动态数据的只读借用 | 借用期间源冻结，const 不承诺长期持有，借用期在 flow 有效期内结束 |
| `flow` → `flow` | 所有权转移 | 新 owner 接管内存管理责任，源失效 |
| `flow` → `var` | 动态数据的可变借用 | 借用期间源冻结，var 的作用域在 flow 有效期内结束 |
| `var` → `const` | 可变借用的只读再借用 | 只读链不会修改底层数据，源冻结 |
| `var` → `var` | 可变借用的再借用 | 源冻结，新 var 获得可变访问但受制于原借用链 |

**禁止传递（8 个单元格）**：

| 传递 | 为什么禁止 |
|------|-----------|
| `const` → `var` | const 是不可变语义；var 暗示可变借用，与只读承诺矛盾 |
| `const` → `flow` | const 不拥有数据；flow 需要拥有所有权并负责释放，const 无法转移不属于自己的所有权 |
| `const` → `static` | 同上，const 无法转移所有权给需要长期持有的 static |
| `flow` → `static` | static 承诺程序全程持有，但 flow 可能在函数返回时释放，导致 static 指向已释放内存 |
| `var` → `flow` | var 是借用者，不拥有数据；flow 需要所有权，var 无法转移 |
| `var` → `static` | var 是借用者且不拥有数据；static 承诺程序全程持有，var 既无法转移所有权，也无法保证长期有效性 |
| `static` → `var` | static 是模块级数据；var 的可变借用可能逃逸到模块作用域之外，产生跨模块可变别名 |
| `static` → `flow` | static 是静态数据；flow 会在析构时尝试释放，static 数据不应被动态释放 |

#### `func` 属性

`func` 是函数返回属性（§7.1.2），不属于变量存储期，不参与本矩阵。函数返回值的接收规则见 §7.3。

> 规范与编译器实现的详细对应关系见 [`IMPLEMENTATION_STATUS.md`](./IMPLEMENTATION_STATUS.md)。

---

## 12.2 函数参数与返回值的传递规则

### 参数传递

函数参数的属性决定了实参的传递方式：

```nihao
// 参数为 flow：获得所有权，函数结束时释放
func consume(flow val void) { ... }

// 参数为 var：可变借用，实参冻结
func modify(var val void) { ... }

// 参数为 const：只读借用，实参冻结
func inspect(const val void) { ... }
```

调用时，编译器根据参数属性执行对应的状态变更：

```nihao
flow p void = malloc(i32)
consume(p)   // p 失效(所有权转移)

flow q void = malloc(i32)
modify(q)    // q 冻结(可变借用)
inspect(q)   // q 冻结(只读借用)
```

> **实现状态**：当前编译器（ncc）解析参数属性前缀但不强制执行对应的借用语义（parser.c:870–874）。
> 所有参数在内部统一视为 `var`（VIS_DEFAULT）。参数的所有权/借用检查将在后续版本中完善。
> 详见 [`IMPLEMENTATION_STATUS.md`](./IMPLEMENTATION_STATUS.md)。

### 返回值

- 返回 `flow` 指针：将所有权转移给调用者(调用者负责释放)。
- 返回 `static` 指针：返回静态地址，调用者获得共享引用。
- 返回 `var` 或 `const` 借用指针：必须确保借用有效(生命周期不短于调用者作用域)。

```nihao
flow create_ptr() void {
    flow ptr void = malloc(i32)
    ptr.(i32) = 100
    return ptr   // 所有权转移给调用者
}

static get_static_ptr() void {
    static data i32 = 42
    return &data  // 返回静态指针
}
```

---

## 13. 生命周期管理(作用域推导)

### 13.1 作用域与借用有效期

借用指针(`var`/`const`)的生命周期必须包含在源指针的有效期内。编译器通过作用域嵌套分析自动推导：

```nihao
func demo() {
    flow outer void = malloc(i32)   // outer 作用域为整个函数
    {
        var inner void = outer      // 可变借用，outer 冻结
        // inner 只在当前块内有效
    }                          // inner 销毁，outer 解冻
    // 此处 outer 可恢复使用
}
```

嵌套作用域中，借用指针不能超出源指针的作用域：

```nihao
flow source = malloc(i32)
{
    var borrow void = source
    // 错误：不能将 borrow 返回给外部作用域
    // flow leaked void = borrow   // 编译错误：borrow 是借用，不能转移所有权
}
// borrow 已销毁，source 可继续使用
```

### 13.2 跨函数调用的生命周期

当函数接收借用参数时，编译器确保传入的实参在函数调用期间保持有效：

```nihao
func use_borrow(var ptr void) {
    // ptr 在函数内有效
}

func caller() {
    flow data void = malloc(i32)
    use_borrow(data)   // data 冻结，调用期间有效
    // 调用返回后 data 解冻
}
```

若函数返回借用指针，必须返回生命周期不短于调用者作用域的指针(通常是静态或外部传入的借用)：

```nihao
const get_const_ref() void {
    static data i32 = 100
    return &data   // 安全：静态生命周期
}
```

### 13.3 生命周期错误诊断

编译器提供详细的错误信息，帮助定位生命周期不匹配的问题：

```nihao
func invalid() {
    flow local u32 = 42
    static bad_ptr = &local   // 编译错误：flow 不能赋给 static
    // 错误信息：目标 static 寿命长于源 flow，可能导致悬垂指针
}
```

运行时可见性检查(调试模式)可通过 `visof` 查询：

```nihao
func debug_vis(ptr void) {
    while visof(ptr) {
        is _flow {
            puts("动态指针")
        }
        is _static {
            puts("静态指针")
        }
        is _var {
            puts("可变借用")
        }
        is _const {
            puts("只读借用")
        }
        break
    }
}
```

---

## 14. 安全操作模式与完整示例

### 14.1 安全解引用与访问

使用 `?.` 操作符进行安全解引用，编译器会结合可见性进行边界检查：

```nihao
func safe_access(flow ptr void) {
    value = ptr?.(i32)   // 确保 ptr 非空且可见性正确
}

// 等价于
if visof(ptr) == _flow {
    safePtr = ptr
}else{
    // 抛出编译期错误
}
```

结构体和数组访问遵循同样的规则：

```nihao
Person struct { name char[], age i32 }
flow person_ptr void = &some_person
flow name_ptr void = person_ptr.(Person).name   // 字段传递需符合可见性
```

### 14.2 完整示例(所有权、借用与存储期)

以下示例综合展示了所有规则：

```nihao
module main
use stdio
use stdlib

// ---------- 辅助函数 ----------
func consume(flow val void) {
    puts("Consuming pointer")
    // val 自动 free
}

func modify(var val void) {
    val.(i32) = 200
    puts("Modified to 200")
}

func inspect(const val void) {
    puts("Inspecting value: ")
    puts(val.(i32))
}

flow create_ptr() void {
    flow ptr void = malloc(i32)
    ptr.(i32) = 100
    return ptr          // 转移所有权
}

// ---------- 主函数 ----------
func main() {
    // 值类型(非指针)值拷贝，无所有权概念
    var a i32 = 10
    var b i32 = a
    b = 20                // a 仍为 10

    // 指针类型
    flow ptr void = malloc(i32)
    ptr.(i32) = 50

    // flow -> var(可变借用)
    var mut_ref void = ptr     // ptr 冻结
    mut_ref.(i32) = 100
    // ptr.(i32) = 200     // 错误：ptr 冻结

    // flow -> const(只读借用)
    const read_ref void = ptr  // ptr 冻结(共享)
    // read_ref.(i32) = 300 // 错误：只读
    // ptr.(i32) = 400     // 错误：ptr 仍冻结

    // flow -> flow(所有权转移)
    flow new_owner void = ptr  // ptr,read_ref,mut_ref 失效
    // ptr.(i32) = 500     // 错误：ptr 已失效

    // 函数调用传递
    flow p void = malloc(i32)
    p.(i32) = 1000
    consume(p)            // p 失效

    flow q void = malloc(i32)
    q.(i32) = 1100
    modify(q)             // q 冻结，内部修改
    puts(q.(i32))         // 输出 200
    inspect(q)            // q 冻结(只读)

    // 链式调用(借用嵌套)
    flow r void = malloc(i32)
    r.(i32) = 50
    inspect(r)            // r 冻结
    // modify(r)           // 错误：r 仍被借用
    modify(r)             // 先可变
    inspect(r)            // 再只读(允许)

    // 返回值
    flow p2 void = create_ptr()  // 获得所有权
    puts(p2.(i32))          // 100
    // p2 自动 free

    // 嵌套作用域
    flow m void = malloc(i32)
    m.(i32) = 500
    {
        var n void = m                // m 冻结
        n.(i32) = 600
        {
            const o void = n          // n 冻结
            // o.(i32) = 700     // 错误
            // n.(i32) = 800     // 错误
        }                        // o 结束，n 解冻
        n.(i32) = 900
    }                            // n 结束，m 解冻
    puts(m.(i32))                // 900
}
```

#### 编译期静态分析过程

编译器对示例代码执行多阶段静态检查，确保所有指针传递均符合 §12.1 传递矩阵和所有权/借用规则。其分析流程如下：

1. **属性推导**：为每个变量和函数参数标注存储期属性（`const`/`static`/`flow`/`var`）及所有权状态（拥有 / 借用）。
2. **可见性兼容性检查**：依据第 12.1 节的传递矩阵，验证指针传递的兼容性。
3. **所有权/借用规则检查**：依据第 12.1 节的传递规则，验证源变量的状态变化（冻结/失效/保持）是否合法。
4. **作用域生命周期推断**：计算每个借用指针的活跃区间，确保其不超出源指针的作用域。

下表逐行摘录示例中的关键指针操作，展示编译器的具体分析过程：

| 代码语句（或注释）                                           | 操作类型                      | 规则应用                                                               | 分析结论与状态变化                                                                       |
|:--------------------------------------------------- |:------------------------- |:------------------------------------------------------------------ |:------------------------------------------------------------------------------- |
| `flow ptr void = malloc(i32)`                       | 动态分配                      | —                                                                  | `ptr` 为 `_flow`，**拥有所有权**，状态有效。                                                 |
| `var mut_ref void = ptr`                            | 赋值（`_flow` → `_var`）      | 存储期兼容，`_var` 寿命短于 `_flow`（允许）；传递规则：`flow→var` 为**可变借用**。           | **`ptr` 冻结**，不可读写；`mut_ref` 为可变借用（`_var`），在作用域内有效。                              |
| `const read_ref void = ptr`                         | 赋值（`_flow` → `_const`）    | 传递矩阵：`flow→const` 为**只读借用**（允许）；借用期间源冻结。                       | **`ptr` 继续冻结**（多个借用共存），`read_ref` 为只读借用（`_const`）。                              |
| `flow new_owner void = ptr`                         | 赋值（`_flow` → `_flow`）     | 存储期兼容（允许）；传递规则：`flow→flow` 为**所有权转移**。                             | **检查失败**！`ptr` 当前已被 `mut_ref` 和 `read_ref` 借用，处于冻结状态，不得转移所有权。编译器报错。             |
| `consume(p)`                                        | 函数调用（实参 `p` → 形参 `flow`）  | 形参为 `_flow`，接收所有权；传递规则：`flow→flow` 为所有权转移。                         | **`p` 失效**，所有权转移至形参，函数返回后形参自动释放内存。                                              |
| `modify(q)`                                         | 函数调用（实参 `q` → 形参 `var`）   | 形参为 `_var`，接收可变借用；传递规则：`flow→var` 为可变借用。                           | **`q` 冻结**，函数内部可修改所指数据，但无法转移或释放。调用结束后 `q` 解冻。                                   |
| `inspect(q)`                                        | 函数调用（实参 `q` → 形参 `const`） | 形参为 `_const`，接收只读借用；传递规则：`flow→const` 为只读借用。                       | **`q` 冻结**（仅在该语句期间），函数内部只能读取。调用结束后 `q` 解冻。                                      |
| `inspect(r)` <br>`// modify(r)` （注释）<br>`modify(r)` | 连续调用                      | 第一条 `inspect` 产生**语句级只读借用**，`r` 在该语句期间冻结；语句结束后借用终止，`r` 解冻。         | 第二条 `modify` 调用前，前一条借用已结束，`r` 可重新被可变借用，分析通过。若在借用期内（如同一条表达式内）调用 `modify`，编译器将报错。 |
| `flow p2 void = create_ptr()`                       | 函数返回（返回值 `_flow`）         | 返回值类型为 `_flow`，所有权转移给 `p2`。                                        | `p2` 获得所有权，状态有效。`create_ptr` 内的局部指针已失效（所有权已转出）。                                 |
| `var n = m`                                         | 赋值（`_flow` → `_var`）      | 同 `ptr → mut_ref`，为**可变借用**。                                       | **`m` 冻结**（直到 `n` 的作用域结束）；`n` 为可变借用。                                            |
| `const o = n`                                       | 赋值（`_var` → `_const`）     | 源 `n` 为 `_var`（可变借用），目标 `o` 为 `_const`；传递规则：`var→const` 为**只读借用**。 | **`n` 冻结**（直到 `o` 的作用域结束）；`o` 为只读借用。                                            |
| `// o.(i32) = 700` （注释）<br>`// n.(i32) = 800` （注释）  | 尝试写入                      | `o` 为只读借用，`n` 因 `o` 存在而冻结；二者均不可变。                                  | 编译器在注释位置报错（若取消注释），违反借用规则。                                                       |
| `n.(i32) = 900`                                     | 写入操作                      | `o` 的作用域已结束，`n` 解冻；`n` 自身为可变借用，可写入。                                | 分析通过，`n` 可修改所指数据（`m` 所拥有的数据）。                                                   |
| `puts(m.(i32))`                                     | 读取操作                      | `n` 的作用域已结束，`m` 解冻；`m` 拥有所有权，可读取。                                  | 分析通过，读取 `m` 所指数据值为 900。                                                         |

---

#### 最终生命周期与状态结果

经过编译期静态分析后，每个指针变量的最终生命周期归属和状态变化如下表所示。其中“销毁/释放时机”由存储期属性和所有权转移情况共同决定。

| 变量          | 存储期属性        | 初始状态                           | 状态变化序列                                                                                     | 最终销毁/释放时机                   |
|:----------- |:------------ |:------------------------------ |:------------------------------------------------------------------------------------------ |:--------------------------- |
| `ptr`       | `_flow`      | 拥有所有权，有效                       | 有效 → 冻结（被 `mut_ref` 和 `read_ref` 借用）→ 保持冻结至作用域结束                                           | `main` 函数结束前释放（若未被转移）       |
| `mut_ref`   | `_var`（借用）   | —                              | 创建时为可变借用 → 作用域结束销毁                                                                         | `main` 函数结束前销毁（仅借用，无释放）     |
| `read_ref`  | `_const`（借用） | —                              | 创建时为只读借用 → 作用域结束销毁                                                                         | `main` 函数结束前销毁（仅借用，无释放）     |
| `new_owner` | `_flow`      | —                              | 因 `ptr` 冻结，该赋值语句在编译期被拒绝，变量不生成                                                              | 不生成                         |
| `p`         | `_flow`      | 拥有所有权，有效                       | 有效 → 调用 `consume(p)` 后**失效**（所有权转移）                                                        | 在 `consume` 函数返回时由形参自动释放    |
| `q`         | `_flow`      | 拥有所有权，有效                       | 有效 → 调用 `modify(q)` 时冻结 → 调用结束后解冻 → 调用 `inspect(q)` 时冻结 → 结束后解冻 → 保持有效至函数结束                | `main` 函数结束前释放              |
| `r`         | `_flow`      | 拥有所有权，有效                       | 有效 → `inspect(r)` 语句级冻结 → 解冻 → `modify(r)` 语句级冻结 → 解冻 → `inspect(r)` 再次冻结 → 解冻 → 保持有效至函数结束 | `main` 函数结束前释放              |
| `p2`        | `_flow`      | 拥有所有权，有效（继承自 `create_ptr` 返回值） | 保持有效至函数结束                                                                                  | `main` 函数结束前释放              |
| `m`         | `_flow`      | 拥有所有权，有效                       | 有效 → 进入嵌套块后被 `n` 可变借用，**冻结** → 离开 `n` 所在块后解冻 → 保持有效至函数结束                                   | `main` 函数结束前释放              |
| `n`         | `_var`（借用）   | —                              | 创建时为对 `m` 的可变借用 → 内部嵌套块中被 `o` 只读借用，自身冻结 → 离开 `o` 的块后解冻 → 离开自身块时销毁                          | 在 `m` 所在块内部（内层块结束时）销毁，不释放内存 |
| `o`         | `_const`（借用） | —                              | 创建时为对 `n` 的只读借用 → 离开自身块时销毁                                                                 | 在最内层块结束时销毁，不释放内存            |

#### 嵌套作用域的借用链分析（针对 `m`, `n`, `o`）

为了更直观地展示生命周期嵌套，下表给出了三个变量在作用域中的有效区间和借用关系：

| 作用域层级                | 活跃变量                     | 借用关系                  | 拥有所有权的源 | 状态说明                           |
|:-------------------- |:------------------------ |:--------------------- |:------- |:------------------------------ |
| **外层 `main` 函数作用域**  | `m`（`_flow`）             | 无                     | `m` 自身  | `m` 有效，拥有内存块。                  |
| **中间嵌套块 `{ ... }`**  | `m`（`_flow`）、`n`（`_var`） | `n` 借用 `m`            | `m`     | `m` 冻结，`n` 可修改数据。              |
| **最内层嵌套块 `{ ... }`** | `m`、`n`、`o`（`_const`）    | `o` 借用 `n`，`n` 借用 `m` | `m`     | `m` 冻结，`n` 冻结，`o` 只读。读写操作均被禁止。 |
| **退出最内层块**           | `m`、`n`                  | `o` 销毁，`n` 解冻         | `m`     | `o` 的借用结束，`n` 恢复可变借用能力。        |
| **退出中间块**            | `m`                      | `n` 销毁，`m` 解冻         | `m`     | 所有借用结束，`m` 恢复完全所有权，可读写。        |

此嵌套分析确保在每一层中，指针的借用不会超出源指针的有效作用域，完全满足第 11 章和第 12 章的所有静态约束。编译器通过此类分析，在编译期彻底杜绝了悬垂指针、双重释放和数据竞争等内存安全问题。

### 14.3 常见安全模式

- **动态数据封闭处理**：在块内创建 `flow` 数据，处理完成后自动释放。
- **静态缓存共享**：使用 `static` 指针长期持有数据，`const` 借用供其他模块只读访问。
- **所有权转移链**：通过 `flow` 返回值将所有权在不同函数间传递，避免拷贝。

```nihao
func safe_patterns() {
    // 模式1：临时动态数据
    {
        flow temp void = load_data()
        process(temp)
    }  // temp 自动释放

    // 模式2：静态缓存
    static cache void = initialize_cache()
    const cache_ref void = &cache   // 只读借用
    use_cache(cache_ref)

    // 模式3：所有权传递
    flow data void = acquire()
    flow processed = transform(data)  // data 失效
    analyze(processed)                // processed 继续传递
}
```

---

## 15. 总结

Nihao 通过统一的属性系统(`const`, `static`, `flow`, `var`)同时管理：

- **存储期**(生命周期)：通过 §12.1 传递矩阵防止悬垂指针。
- **所有权与借用**：通过传递规则控制指针的状态变化(冻结、失效、保持)。
- **作用域推导**：编译器自动分析借用有效期，确保安全。

这个设计基于NiHao语言现有的可见性系统，通过以下方式实现内存安全：

1.  **明确的存储期和作用域**：const/static/flow/var变量的生命周期规则清晰

2.  **可见性检查的指针安全**：通过`?=`操作符在赋值时检查可见性兼容性

3.  **动态作用域管理**：flow变量的作用域由编译器自动推导

4.  **渐进式安全**：从局部安全到模块安全，最后到全局安全

关键优势：

- 引入简单易理解的所有权传递规则

- 利用现有的可见性修饰符

- 编译时和运行时结合的安全检查

- 无垃圾回收, 静态内存安全, 兼具灵活性和性能

- 可见性设计哲学与所有权传递融为一体

---

*NiHao v1.0 语言规范 - © 2026 NiHao 开发团队*
