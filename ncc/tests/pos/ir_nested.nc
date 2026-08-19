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
    return
}
