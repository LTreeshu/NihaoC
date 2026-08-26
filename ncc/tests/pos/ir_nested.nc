module main
Point struct { x i32 y i32 }
Line struct { a Point b Point }
func main() {
    l Line
    l.a.x = 1
    l.a.y = 2
    l.b.x = 3
    l.b.y = 4
    if l.a.x == 1 && l.a.y == 2 && l.b.x == 3 && l.b.y == 4 {
        puts("nested field ok")
    } else {
        puts("nested field bad")
    }
    m Line
    m = l
    if m.b.y == 4 && m.a.x == 1 {
        puts("nested copy ok")
    } else {
        puts("nested copy bad")
    }
    /* 嵌套初始化列表（递归填充成员槽） */
    n Line = {{5, 6}, {7, 8}}
    if n.a.x == 5 && n.b.y == 8 {
        puts("nested init ok")
    } else {
        puts("nested init bad")
    }
    return
}
