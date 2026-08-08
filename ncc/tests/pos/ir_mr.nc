module main

/* 多返回值：命名结构体返回（与 C 机制一致） */
Result struct {
    value1 i32
    value2 i32
    value3 i32
}

/* 返回结构体：聚合字面量 */
func calc(a i32, b i32) Result {
    return {a + b, a * b, a - b}
}

/* 返回结构体：先组装变量再 return */
func calc2(a i32, b i32) Result {
    r Result = {a - b, a * 2, b * 2}
    return r
}

func main() {
    /* 接收结构体返回值 + 成员访问 */
    r Result = calc(6, 4)
    if r.value1 == 10 && r.value2 == 24 && r.value3 == 2 {
        puts("multi return lit ok")
    } else {
        puts("multi return lit bad")
    }

    /* 第二次调用（新缓冲） */
    r2 Result = calc(3, 7)
    if r2.value1 == 10 && r2.value2 == 21 && r2.value3 == -4 {
        puts("multi return2 ok")
    } else {
        puts("multi return2 bad")
    }

    /* return 结构体变量写法 */
    r3 Result = calc2(10, 3)
    if r3.value1 == 7 && r3.value2 == 20 && r3.value3 == 6 {
        puts("multi return var ok")
    } else {
        puts("multi return var bad")
    }
    return
}
