module main
use stdio

/* 编译期函数（<ct-func-def> / <ct-func-call>，BNF §8）：
 * cooking 块内 `const NAME(p1, p2) = expr` 定义；调用 NAME(a, b) 时实参先
 * 求成编译期常量，再按词边界替换形参名展开求值。 */
cooking {
    const sq(x) = x * x
    const add3(a, b, c) = a + b + c
    const area(w, h) = w * h
    static_assert(sq(5) == 25, "sq")
    static_assert(add3(1, 2, 3) == 6, "add3")
    static_assert(area(3, 4) == 12, "area")
}

/* 嵌套、组合与带运算的实参 */
cooking {
    static_assert(sq(sq(2)) == 16, "nested")
    static_assert(sq(area(2, 3)) == 36, "compose")
    static_assert(sq(1 + 2) == 9, "expr arg")
    static_assert(sq(-3) == 9, "unary arg")
}

/* 实参可为编译期变量；结果再存为编译期变量，供运行时声明引用 */
cooking {
    const BASE i32 = 10
    const SIDE i32 = sq(BASE + 1)
    static_assert(SIDE == 121, "derived")
}

func main() {
    v i64 = SIDE
    if v == 121 {
        puts("ct fn ok")
    } else {
        puts("ct fn bad")
    }
    return
}
