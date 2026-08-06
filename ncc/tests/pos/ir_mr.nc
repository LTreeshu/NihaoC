module main

/* 多返回值类型定义 */
multireturn struct{
    value1 u8
    value2 u8
    value3 u8
}

/* 多返回值函数：返回聚合字面量 */
func calc(a i32, b i32) multireturn {
    return {a + b, a * b, a - b}
}

func main() {
    /* 多返回值调用 + 成员访问 */
    r multireturn = calc(6, 4)
    if r.value1 == 10 && r.value2 == 24 && r.value3 == 2 {
        puts("multireturn ok")
    } else {
        puts("multireturn bad")
    }

    /* 第二次调用（新缓冲） */
    r2 multireturn = calc(3, 7)
    if r2.value1 == 10 && r2.value2 == 21 && r2.value3 == -4 {
        puts("multireturn2 ok")
    } else {
        puts("multireturn2 bad")
    }
    return
}
