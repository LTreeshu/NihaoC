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
- `[[static]]` 内部链接函数属性修饰
- `[[used]]` 强制保留函数属性修饰
- `[[unused]]` 强制弃用函数属性修饰
- `[[export]".my_section"]` 导出到指定段函数属性修饰

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
| `fx32`   | 单精度定点数(Q16.16) | 4字节    |
| `fx64`   | 双精度定点数(Q32.32) | 8字节    |

### 3.2 复合类型

基本的定义声明结构 ` [属性] [名字] [类型] `
**数组声明：**

```nihao
static fixedArray char[3]       // 固定大小数组 char[3] 为类型
flow dynamicArray i8[...]   // 无初始值动态数组
var initArray u16[6...]   // 有初始大小的动态数组

// 数组索引访问赋值
fixedArry[0] = 0

// 数组切片访问赋值
dynamicArray[2..3] = {3,4}
```

**类型别名：**

```nihao
alias Byte = u8
alias StringPtr = void[]
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

###### 3.3.1 类型对齐

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

###### 3.3.2 类型嵌套

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

###### 5.1.1 指针定义

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

###### 5.1.2 数组指针

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

###### 5.1.3 指针数组

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

###### 5.1.4 复合体指针

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

###### 5.1.5 函数指针

` void() ` 代表函数指针类型, ` () ` 内可以声明参数个数和类型 例如:
` func ptr void(u8,char[]) ` 无返回值
` func ptr void(u8,char[]) u32` 有返回值类型u32
` const ptr void(u8,char[]) u32` 有属性为const的返回值类型u32
` flow ptr void(u8,char[]) void` 有属性为flow的返回值类型void 
` static ptr void(u8,char[]) char[]` 有属性为static的返回值类型char[]

```nihao
func callback_handle(argc u16) {
    puts("callback call!")
}

func callback void(u16) = callback_handle

func callback_register(func cb void(u16), event u32) u32 {
    if event == 1 {
        callback = cb
    }

    return event
}

flow callback_handle_with_return(argc u16) i32 {
    puts("callback call!")
    return 42
}

flow callback2 void(u16)i32 = callback_handle_with_return

func callback_register_with_return(flow cb void(u16)i32 , event u32) u32 {
    if event == 1 {
        callback2 = cb
    }
    return event
}
```

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
    break
}
```

**for 循环：**

```nihao
for i = 0; i < 10; i++ {
    // ...
}
```

## 7. 函数定义

### 7.1 函数声明

```nihao
// 无返回无参函数
func greet() {
    print("Hello")
}

// 有返回有参无void返回值函数
func add(a i8, b i8) i8 {
    return a + b
}

Person struct {
    id u32
    name char[]
}

// 有void返回值属性函数 
flow create(id u32) void {
    flow person void = malloc(sizeof(Person))
    person.(Person).id = id
    return person
}

const create(id u32) void {
    static xxx Person = {32, "xxx"}
    return &person
}

static get_arry(size u32) void {
    static arry char[u32]
    return &arry
}

// 有函数属性函数
[[static]]
func getCounter() u32 {
    static counter u32
    return (counter + 1)
}

[[inline]]
const sub(a u8, b u8) {
    return (a+b)
}

[[weak]]
flow newCard(id u32) void {
    flow card struct {
        id u32
        name char[]
    } = {id,"obj"}

    return &card
}
```

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

multireturn struct{
    value1 u8
    value u8
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
    returnValue multireturn = calculate()

    return
    /* If the flow variable: dynptr, temp, is not returned,
     * they will be automatically free.
    */
}

func calculate() multireturn  
{
    if visof(value) != _undef {
      return {0,0}
    }
    else if visof(ConstValue) == _static {
      return {ConstValue, (ConstValue*2)}
    }
}
```

## 11. 高级特性

### 11.1 类型安全

```nihao
// 安全的指针传递
flow safePtr void ?= ptr

// 等价于
if visof(ptr) == _flow {
    safePtr = ptr
}
```

## 12.NiHao语言可见性系统与生命周期管理

根据变量属性划分可见属性

![Visibility definition.png](Visibility%20definition.png)
指针赋值可见性兼容矩阵 作用域交集中
源 -> 目标    const    static    flow    local
const         安全      错误      错误    错误
static        安全      安全      错误    错误  
flow          安全      错误      安全    错误
local         安全      错误      错误    安全

### 12.1 存储期和作用域的组合规则

```nihao
// 存储期和作用域规则
const MAX_SIZE i32 = 1024     // 静态存储期，全局作用域
static counter i32 = 0        // 静态存储期，模块作用域  
flow dynamic_var i32 = 42     // 动态存储期，动态作用域

// 局部代码块
{
    local_var i32 = 100       // 自动存储期，局部作用域
}

// 指针安全传递规则
func safe_pointer_operations() {
    // 安全传递：同作用域或更长寿作用域
    flow ptr1 void = &dynamic_var     // 安全：flow -> flow
    static ptr2 void = &counter       // 安全：static -> static

    // 不安全传递：短寿作用域向长寿作用域传递
    // static ptr3 void = &local_var   // 错误：局部变量不能传递给静态指针
    // const ptr4 void = &dynamic_var  // 错误：flow不能传递给const
}
```

### 12.2 可见性检查的安全传递

```nihao
// 安全指针赋值操作符 ?= 的可见性检查规则
func visibility_checks() {
    var varconst u32
    var source_ptr = &varconst

    const target_ptr void

    {
        var undefptr void
        source_ptr = undefptr
    }

    // 安全赋值：检查可见性兼容性
    target_ptr ?= source_ptr  // 等价于以下检查：

    // 编译时生成的检查逻辑
    if visof(source_ptr) == _flow && visof(target_ptr) == _flow {
        target_ptr = source_ptr  // flow -> flow 安全
    }
    else if visof(source_ptr) == _static && visof(target_ptr) == _static {
        target_ptr = source_ptr  // static -> static 安全
    }
    else if visof(source_ptr) == _const && visof(target_ptr) == _const {
        target_ptr = source_ptr  // const -> const 安全
    }
    else if visof(source_ptr) == _var && visof(target_ptr) == _var {
        target_ptr = source_ptr  // var -> var 安全
    }
    else if visof(source_ptr) == _flow && visof(target_ptr) == _const {
        target_ptr = source_ptr  // flow -> const 安全
    }
    else if visof(source_ptr) == _static && visof(target_ptr) == _const {
        target_ptr = source_ptr  // static -> const 安全
    }
    else if visof(source_ptr) == _const && visof(target_ptr) == _const {
        target_ptr = source_ptr  // const -> const 安全
    }
    else if visof(source_ptr) == _var && visof(target_ptr) == _const {
        target_ptr = source_ptr  // var -> const 安全
    }
    else if visof(source_ptr) == _undef{
        panic("未定义指针不能赋值")
    }
    else {
        panic("可见性不兼容的指针赋值")
    }
}
```

## 13. 基于可见性的指针安全系统

### 13.1 指针传递的可见性规则表

```nihao
// 指针赋值可见性兼容矩阵 作用域交集中
// 源 -> 目标    const    static    flow    local
// const         安全      错误      错误    错误
// static        安全      安全      错误    错误  
// flow          安全      错误      安全    错误
// local         安全      错误      错误    安全

func demonstrate_rules() {
    const GLOBAL i32 = 100
    static MODULE_VAR i32 = 200
    flow DYNAMIC_VAR i32 = 300
    LOCAL_VAR i32 = 400

    // 安全示例
    flow ptr1 void ?= &DYNAMIC_VAR     // flow -> flow: 安全
    const ptr6 void ?= &MODULE_VAR    // static -> const: 安全

    // 错误示例（编译时检查）
    // static ptr5 void ?= &DYNAMIC_VAR  // flow -> static: 错误
    // static ptr7 void ?= &LOCAL_VAR    // 局部 -> static: 错误
}
```

### 13.2 函数参数的可见性约束

```nihao
// 函数参数的可见性注解
func process_static_data(static ptr void) i32 {
    // 只能接受static指针
    return ptr.(i32)
}

func process_dynamic_data(flow ptr void) i32 {
    // 只能接受flow指针
    return ptr.(i32)
}

// 返回值的可见性约束
static get_static_pointer() void {
    static data i32 = 42
    return &data  // 返回static指针
}

flow get_dynamic_pointer() void {
    flow data i32 = 42
    return &data  // 返回flow指针
}

func example_usage() {
    static static_ptr void = get_static_pointer()
    flow dynamic_ptr void = get_dynamic_pointer()

    // 安全调用
    process_static_data(static_ptr)     // static -> static: 安全
    process_dynamic_data(dynamic_ptr)  // flow -> flow: 安全
}
```

## 14. 动态作用域的生命周期管理

### 14.1 flow变量的作用域推导

```nihao
// 编译器自动推导flow变量的作用域
func scope_demonstration() {
    flow var1 i32 = 10

    if condition {
        flow var2 i32 = 20
        flow ptr1 void ?= &var1     // 安全：var1作用域包含var2
        flow ptr2 void ?= &var2     // 安全：同作用域

        // var2的作用域在此结束
    }

    // 这里不能再使用ptr2，因为var2已离开作用域
    flow ptr3 void ?= &var1         // 安全：var1仍然在作用域内
}

// 嵌套作用域的生命周期检查
func nested_scopes() {
    flow outer_var i32 = 100

    {
        flow inner_var i32 = 200
        flow inner_ptr void ?= &outer_var  // 安全：外部作用域包含内部
        flow outer_ptr void ?= &inner_var  // 安全：同函数作用域
    }

    // 离开内部作用域后，inner_var失效
    // 但outer_var仍然有效
}
```

### 14.2 跨函数调用的作用域管理

```nihao
// 函数调用时的作用域传递
func caller_function() {
    flow local_dynamic i32 = 42
    flow result i32 = process_with_callback(local_dynamic, &callback_function)
}

flow process_with_callback(flow data i32, 
                           flow callback void(i32)i32
                           ) i32 {
    // data和callback都是flow，保证生命周期兼容
    return callback(data)
}

func callback_function(value i32) i32 {
    return value * 2
}
```

## 15. 安全的指针操作模式

### 15.1 基于可见性的安全解引用

```nihao
// 安全解引用操作符 ! 的可见性检查
func safe_dereference_examples() {
    flow dynamic_ptr void = &some_flow_variable
    static static_ptr void = &some_static_variable

    // 安全解引用
    value1 = dynamic_ptr?.(i32)     // flow指针的安全解引用
    value2 = static_ptr?.(i32)     // static指针的安全解引用

    // 安全解引用等价于
    if visof(dynamic_ptr) != _undef && dynamic_ptr != null {
        value1 = dynamic_ptr.(i32)
    } else {
        panic("不安全解引用")
    }
}
```

### 15.2 数组和结构体指针的安全访问

```nihao
// 结构体定义
Person struct {
    name char[]   // 动态字符串
    age i32   // 静态年龄
}

flow some_person Person
dynamic_array u8[10...]

func safe_structure_access() {
    flow person_ptr void = &some_person

    // 安全访问结构体字段
    flow   name_ptr void ?= person_ptr.(Person).name  // flow字段安全传递
    static age_ptr  void ?= person_ptr.(Person).age   // static字段安全传递

    // 数组边界检查与可见性结合
    flow array_ptr void = &dynamic_array

    //
    element = array_ptr!.(i32[10])
    /* 等价于
        if visof(array_ptr) == _flow && sizeof(dynamic_array) >= 10 
        {
            element = array_ptr.(i32[10])[5]
        }
    */
}
```

## 16. 错误处理和调试支持

### 16.1 可见性错误诊断

```nihao
// 详细的可见性错误信息
func demonstrate_visibility_errors() {
    flow dynamic_var i32 = 42
    static static_var i32 = 100

    // 触发可见性错误时的诊断信息
    static error_ptr void ?= &dynamic_var  
    // 编译错误：无法将flow可见性(_flow)赋值给static可见性(_static)
    // 原因：static指针可能比flow变量寿命更长，导致悬垂指针
}

// 运行时可见性检查
func runtime_visibility_check(ptr void) {

    while visof(ptr) {
        is _const => puts("常量指针，全局生命周期")
        is _static => puts("静态指针，模块生命周期") 
        is _flow => puts("动态指针，需要作用域分析")
        is _var => puts("局部指针，需要作用域分析")
        is _undef => puts("未定义或无效指针")
        break
    }
}
```

### 16.2 作用域调试工具

```nihao
// 编译时作用域分析报告
func analyzed_function() {
    flow var1 i32 = 10        // [作用域: 函数级]
    {
        flow var2 i32 = 20    // [作用域: 未确定]
        var3 i32 = 30    // [作用域: 块级]
    }
    // [警告: var2, var3 离开作用域]

    /* [警告:var2 作用域变更]
        function {
            {---------------scope start

            }---------------scope end old

        }-------------------scope end new

    */
    var4 i32 = var2; 
    // [编译错误: var3 离开作用域 --> 未定义]
    var4 = var3;

    // [警告: tracked_var 作用域变更]
    flow ptrvar1 void = scope_tracing_example()

    // [编译错误: tracked_var 只能传递给 flow 变量]
    ptrvar2 void = scope_tracing_example()
}

flow scope_tracing_example() i32{
    flow tracked_var i32 = 42

    return &tracked_var  // 启用作用域追踪
    // 在调试模式下记录作用域进入/离开
}
```

## 17. 实际应用示例

### 17.1 完整的模块示例

```nihao
module security_module

// 模块级静态数据
static module_counter i32 = 0
const MAX_CONNECTIONS i32 = 1000

// 安全的数据处理器
SecurityProcessor struct {
    config static ConfigData
    state flow ProcessorState
}


flow process_request(flow self SecurityProcessor, flow request Request) Response {
    // 安全的状态访问
    self.state.current_request ?= request

    // 静态配置访问
    if self.state.connection_count < self.config.(ConfigData).max_connections {
        self.state.connection_count++
        return create_response(200, "OK")
    }

    return create_response(429, "Too Many Requests")
}

func main() {
    flow processor SecurityProcessor = create_processor()
    flow request Request = receive_request()

    // 安全的方法调用
    flow response Response = processor.process_request(request)
    send_response(response)
}
```

### 17.2 内存安全模式

```nihao
// 基于可见性的内存安全模式
func memory_safe_patterns() {
    // 模式1：动态数据在封闭作用域内处理
    {
        flow temporary_data Data = load_temporary_data()
        process_data(temporary_data)  // 数据在处理完成后自动清理
    }

    // 模式2：静态数据长期持有
    static persistent_cache Cache = initialize_cache()
    use_cache(persistent_cache)

    // 模式3：安全的数据传递链
    flow source_data Data = acquire_data()
    flow processed_data Data = transform_data(source_data)
    flow result Result = analyze_data(processed_data)
    // 所有flow数据在函数结束时自动清理
}
```

## 总结

这个设计基于NiHao语言现有的可见性系统，通过以下方式实现内存安全：

1. 1.
   
   **明确的存储期和作用域**：const/static/flow/局部变量的生命周期规则清晰

2. 2.
   
   **可见性检查的指针安全**：通过`?=`操作符在赋值时检查可见性兼容性

3. 3.
   
   **动态作用域管理**：flow变量的作用域由编译器自动推导

4. 4.
   
   **渐进式安全**：从局部安全到模块安全，最后到全局安全

关键优势：

- 不引入复杂的所有权系统

- 利用现有的可见性修饰符

- 编译时和运行时结合的安全检查

- 与NiHao语言设计哲学高度一致

这样既保持了语言的简洁性，又提供了强大的内存安全保障。

---

*NiHao v1.0 语言规范 - © 2025 NiHao 开发团队*
