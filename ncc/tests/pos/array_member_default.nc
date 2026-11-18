module main
use stdio

/* PA-56：聚合类型数组变量的成员默认值逐项展开（BNF §3.1 × §5.1.2）
 * `P arr[N]` 的每个元素都是聚合体，未给初值的成员一律取定义处的默认值 */
P struct {
    x i32 = 7
    y i32 = 8
    z i32 = 9
}

func main() {
    /* 无初值：N 个元素全部取默认值 */
    a P[3]
    print("%d %d %d %d\n", a[0].x, a[1].y, a[2].z, len(a))

    /* 位置初值只覆盖首元素的前若干员：该元素内省略的成员补默认，尾数元素全默认 */
    b P[3] = { {5} }
    print("%d %d %d\n", b[0].x, b[0].z, b[2].z)

    /* 指定式初值按元素各自记账：未被点名的成员取默认值 */
    c P[2] = { { .y = 1 }, { .z = 2 } }
    print("%d %d %d %d\n", c[0].x, c[0].y, c[1].x, c[1].z)

    /* 省略容量档 `[...]`（默认容量 8）同样逐元素展开 */
    d P[...]
    print("%d %d %d\n", d[0].x, d[7].x, len(d))

    /* 多维：按元素个数乘积展开 */
    e P[2][2]
    print("%d %d %d\n", e[0][1].y, e[1][0].z, len(e))

    /* 扁平（无内层花括号）的位置初值走 C 的花括号可省略规则，不追加尾数元素 */
    f P[3] = { 1, 2, 3 }
    print("%d %d %d\n", f[0].x, f[0].y, f[1].x)

    /* union 元素数组：只取首个带默认值的成员 */
    S union {
        asInt i32 = 7
        asBits u32
    }
    g S[2]
    print("%d %d\n", g[0].asInt, g[1].asInt)
    return 0
}
