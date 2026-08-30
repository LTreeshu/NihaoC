module main
use stdio

/* ============================================================
 * 06_cooking — 编译期求值（cooking / static_assert）
 *
 * 演示：cooking 块 / static_assert 编译期断言 /
 *       const 编译期常量 / const NAME(p) 编译期函数（宏式展开）
 *
 * 1.0 可编译：否（IR_ONLY——cooking 为子集语法，全量 parser
 *             不支持；属 2.0 预览特性，需 -b ir-c / ir-native）
 * ============================================================ */

/* 编译期块：断言在编译期求值，不满足则编译报错 */
cooking {
    static_assert(2 + 3 * 4 == 14, "arith precedence")
    static_assert(sizeof(i32) == 4, "sizeof i32")
    static_assert(10 > 5 && 2 != 3, "logical")
    const FLAG i32 = 1
}

/* 编译期变量：跨块共享 */
cooking {
    const BASE i32 = 10
    const MULT i32 = 4
    static_assert(BASE * MULT == 40, "ct var mul")
}

/* 编译期函数：调用时实参字面量替换，支持嵌套与组合 */
cooking {
    const sq(x) = x * x
    const cube(x) = x * x * x
    static_assert(sq(5) == 25, "sq(5) != 25")
    static_assert(cube(3) == 27, "cube(3) != 27")
    static_assert(sq(sq(2)) == 16, "ct fn nested")
    static_assert(sq(cube(2)) == 64, "ct fn compose")
}

func main() {
    puts("compile-time checks passed")
}
