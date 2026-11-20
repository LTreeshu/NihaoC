module main
use stdio

Inner struct {
    a i32
    b i32
    c i32
}

Outer struct {
    x i32
    in Inner
    y i32
}

func makeO() Outer {
    o Outer = {1, {2, 3, 4}, 5}
    return o
}

func takeO(o Outer) i32 {
    return o.x + o.in.a + o.in.b + o.in.c + o.y
}

func main() {
    /* 嵌套聚合：sret 返回 + 成员链读取 */
    o Outer = makeO()
    print("%d %d %d %d\n", o.x, o.in.a, o.in.c, o.y)

    /* 嵌套聚合按值传参：整块 8 字节块展平（含 12 字节 Inner 的尾部） */
    print("sum %d\n", takeO(o))

    /* 嵌套聚合整体赋值：按字节块复制 */
    p Outer
    p = o
    print("copy %d %d %d %d\n", p.x, p.in.b, p.in.c, p.y)
}
