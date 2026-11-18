module main
use stdio

/* BNF §5 <postfix-op>：通用 `void` 槽的所指类型由声明处（`&x` / `malloc(T)`）静态确定后，
   省略类型的 `.()`、空下标 `[]` 与 `->` 都按该类型还原 */

Pt struct {
    x i32
    y i32
}

func main() {
    /* 1) `p void = &x`：裸 `.()` 读写都按 x 的宽度 */
    x i32 = 3
    p void = &x
    p.() = 7
    if x == 7 { puts("void deref write ok") } else { puts("void deref write bad") }
    v i32 = p.()
    if v == 7 { puts("void deref read ok") } else { puts("void deref read bad") }
    /* 空下标 `p[]` 等价省略类型的 `p.()` */
    if p[] == 7 { puts("void empty subscript ok") } else { puts("void empty subscript bad") }
    p[] = 9
    if x == 9 { puts("void subscript write ok") } else { puts("void subscript write bad") }

    /* 2) `q void = &pt`：`->` 先解引用再取成员，可反复使用 */
    pt Pt
    q void = &pt
    q->x = 5
    q->y = 6
    q->x += 4
    if pt.x == 9 && pt.y == 6 { puts("void arrow ok") } else { puts("void arrow bad") }

    /* 3) `malloc(T)` 定下元素宽度：裸 `.()` 与 `.(T)` 一致 */
    flow m void = malloc(i32)
    m.() = 11
    if m.(i32) == 11 { puts("void malloc deref ok") } else { puts("void malloc deref bad") }

    /* 4) 数组初值：所指类型是数组时按最深元素还原 */
    arr i32[3] = { 1, 2, 3 }
    flow e void = &arr
    if e.() == 1 { puts("void array elem ok") } else { puts("void array elem bad") }
    e.() = 8
    if arr[0] == 8 { puts("void array elem write ok") } else { puts("void array elem write bad") }
    return
}
