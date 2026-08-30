module main
use stdio

/* ============================================================
 * 03_struct — 聚合类型：struct / union / enum
 *
 * 演示：struct 定义与嵌套 / 嵌套初始化列表 / 成员读写 /
 *       整体拷贝 / union 共享存储 / enum 常量
 * 1.0 可编译：是（c/native 后端）
 * ============================================================ */

Point struct {
    x i32
    y i32
}

/* struct 嵌套：Line 由两个 Point 组成 */
Line struct {
    a Point
    b Point
}

/* union：成员共享同一块存储 */
Data union {
    asInt i32
    asFloat f64
}

/* enum：常量从 0 递增 */
Color enum { RED, GREEN, BLUE }

func main() {
    /* 嵌套初始化列表（递归填充成员槽） */
    l Line = {{1, 2}, {3, 4}}
    if l.a.x == 1 && l.b.y == 4 {
        puts("nested init ok")
    } else {
        puts("nested init bad")
    }

    /* 成员读写 */
    l.a.y = 100
    if l.a.y == 100 {
        puts("member write ok")
    } else {
        puts("member write bad")
    }

    /* 整体拷贝 */
    m Line = l
    if m.b.x == 3 && m.a.y == 100 {
        puts("struct copy ok")
    } else {
        puts("struct copy bad")
    }

    /* enum 常量比较 */
    c Color = GREEN
    if c == 1 {
        puts("enum ok")
    } else {
        puts("enum bad")
    }
}
