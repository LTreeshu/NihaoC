module main

Point struct {
    x i32
    y i32
}

Nested struct {
    a Point
    b i32
}

func main() {
    /* -> 指针成员访问：p = &pt 后 p->x 读写 */
    pt Point
    p = &pt
    p->x = 10
    p->y = 20
    if p->x == 10 && p->y == 20 {
        puts("arrow ok")
    } else {
        puts("arrow bad")
    }
    /* -> 链式：q->a.x（嵌套 struct 成员） */
    np Nested
    q = &np
    q->a.x = 30
    q->a.y = 40
    q->b = 50
    if q->a.x == 30 && q->a.y == 40 && q->b == 50 {
        puts("arrow chain ok")
    } else {
        puts("arrow chain bad")
    }
    /* -> 复合赋值：p->x += 5 */
    p->x += 5
    if p->x == 15 {
        puts("arrow compound ok")
    } else {
        puts("arrow compound bad")
    }
    return
}
