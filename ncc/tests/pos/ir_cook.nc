module main

/* 编译期块：static_assert 在编译期求值 */
cooking {
    static_assert(2 + 3 * 4 == 14, "arith precedence")
    static_assert(sizeof(i32) == 4, "sizeof i32")
    static_assert(sizeof(i64) == 8, "sizeof i64")
    static_assert(10 > 5 && 2 != 3, "logical")
    const FLAG i32 = 1
}

/* 编译期变量表（PB-9 深化）：cooking 块内 const 声明，跨块共享 */
cooking {
    const BASE i32 = 10
    const MULT i32 = 4
    static_assert(BASE * MULT == 40, "ct var mul")
    static_assert(BASE + 5 == 15, "ct var add")
}

/* 跨块使用 + 由变量推导新变量 */
cooking {
    static_assert(BASE == 10, "ct var cross block")
    const W i32 = BASE * 2
    static_assert(W == 20, "ct var derived")
}

func main() {
    /* 函数内 cooking 块 + 顶层变量可用 */
    cooking {
        static_assert(100 >= 100, "ge ok")
        static_assert(!false, "not ok")
        static_assert(BASE + W == 30, "ct var in func")
    }
    /* 编译期变量运行时引用 → 折叠为常量 */
    x i64 = BASE
    y i64 = W
    if x == 10 && y == 20 {
        puts("ct var ok")
    } else {
        puts("ct var bad")
    }
    if 1 == 1 {
        puts("cook ok")
    } else {
        puts("cook bad")
    }
    return
}
