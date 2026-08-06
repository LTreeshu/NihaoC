module main

/* 编译期块：static_assert 在编译期求值 */
cooking {
    static_assert(2 + 3 * 4 == 14, "arith precedence")
    static_assert(sizeof(i32) == 4, "sizeof i32")
    static_assert(sizeof(i64) == 8, "sizeof i64")
    static_assert(10 > 5 && 2 != 3, "logical")
    const FLAG i32 = 1
}

func main() {
    /* 函数内 cooking 块 */
    cooking {
        static_assert(100 >= 100, "ge ok")
        static_assert(!false, "not ok")
    }
    if 1 == 1 {
        puts("cook ok")
    } else {
        puts("cook bad")
    }
    return
}
