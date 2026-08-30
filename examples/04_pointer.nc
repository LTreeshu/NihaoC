module main
use stdio

/* ============================================================
 * 04_pointer — 指针与解引用
 *
 * 演示：& 取地址 / p.() 解引用（读写）/
 *       *(expr) 一元解引用 / 指针隐式类型推断
 * 1.0 可编译：是（c/native 后端）
 * ============================================================ */

func main() {
    x i32 = 42

    /* 取地址：p 由 &x 自动推断为指向 i32 的指针 */
    p = &x

    /* 解引用写 */
    p.() = 43
    y i32 = p.()
    if y == 43 {
        puts("deref write ok")
    } else {
        puts("deref write bad")
    }

    /* 一元 * 取地址再解引用 */
    z i32 = *(&x)
    if z == 43 {
        puts("addr deref ok")
    } else {
        puts("addr deref bad")
    }
}
