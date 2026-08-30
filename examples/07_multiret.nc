module main
use stdio

/* ============================================================
 * 07_multiret — 多返回值（命名结构体返回，与 C 机制一致）
 *
 * 演示：struct 类型返回 / 聚合字面量 return /
 *       返回值成员访问
 *
 * 1.0 可编译：待验证——聚合字面量返回为 IR 子集语法，
 *             若全量 parser 不支持则属 2.0 预览（-b ir-c）
 * ============================================================ */

Result struct {
    value1 i32
    value2 i32
    value3 i32
}

/* 返回结构体：聚合字面量 */
func calc(a i32, b i32) Result {
    return {a + b, a * b, a - b}
}

func main() {
    r Result = calc(6, 4)
    printf("sum=%d prod=%d diff=%d\n", r.value1, r.value2, r.value3)
}
