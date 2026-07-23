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

[中文](./Chinese.md) | [English](./English.md)

Source code example demonstration:

源码展示:

```c
module main
use stdio
use stdlib
link "libhttp.so" http

alias http_client = http.http_client
alias time = stdlib.time

const ConstValue i8 = 100

Structreturn struct {
    value1 u8:4
    value u8:4
}

func main() 
{
    puts("Program starting\n")

    // Dynamic memory allocation
    flow dynptr void = malloc(i32)

    // Pointer operations
    dynptr.() = ConstValue
    dynptr.(i8) = ConstValue

    // Array operations
    arry f32[3] = {1.1, 1.2, 1.3}
    ptr void[3] = &arry
    ptrarry void[3] = {&arry[2], &arry[1], &arry[0]}
    ptrarry2 void[][3] = &ptrarry

    ptrarry2[][1].() = 2.1

    // Visibility checking
    if visof(staticptr) == _static {
        flow temp void = malloc(float32)
    }

    if visof(dynptr) == _flow {
        puts("the ptr is _flow attribute \n");
    }

    // Multiple return value handling
    result Structreturn = calculate()

    return
    /* If the flow variable: dynptr, temp, is not returned,
     * they will be automatically free.
    */
}

func calculate() Structreturn  
{
    if visof(value) != _undef {
      return {0,0}
    }
    else if visof(ConstValue) == _static {
      return {ConstValue, (ConstValue*2)}
    }
}

// Inline function definition
[[inline]]
func add(a int, b int) int
{
    return (a+b)
}

// Static function definition
[[static]]
func mul(a int, b int) int
{
    return (a-b)
}

// Compile-time definitions

// Compile-time function define 
cooking maker(name char[], id u8) var_name {
    return `name``id`
}

// define const u8 variable: var0, var1, var2 in Compile-time
cooking {
    var id u8 = 0
    const maker("var", id++)
    const maker("var", id++)
    const maker("var", id++)
}


// Assign values at compile time
cooking PI = 3.1415926
// Assign values at initialization time
const DoublePI f64 = PI * 2

cooking {
    // Compile-time calculation
    const COMPILE_TIME_VALUE i32 = 10 * 20 + 5

    // Compile-time assert
    static_assert(sizeof(i32) == 4, "i32必须是4字节")
    static_assert(COMPILE_TIME_VALUE == 205, "编译期计算错误")
}
```
