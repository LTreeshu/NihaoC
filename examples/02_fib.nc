module main
use stdio

/* ============================================================
 * 02_fib — 函数、递归与循环：斐波那契数列
 *
 * 演示：func 递归 / while / 前缀自增 / printf 格式化输出
 * 1.0 可编译：是（c/native 后端）
 * ============================================================ */

/* 递归实现 */
func fib(n i32) i32 {
    if n < 2 {
        return n
    }
    return fib(n - 1) + fib(n - 2)
}

/* 迭代实现 */
func fib_iter(n i32) i32 {
    a i32 = 0
    b i32 = 1
    i i32 = 0
    while i < n {
        t i32 = a + b
        a = b
        b = t
        i++
    }
    return a
}

func main() {
    i i32 = 0
    while i <= 10 {
        printf("fib(%d) = %d\n", i, fib(i))
        i++
    }
    printf("fib_iter(10) = %d\n", fib_iter(10))
}
