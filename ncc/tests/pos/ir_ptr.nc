module main

func main() {
    x i32 = 42
    p = &x
    p.() = 43
    y i32 = p.()
    if y == 43 {
        puts("ptr ok")
    } else {
        puts("ptr bad")
    }
    z i32 = *(&x)
    if z == 43 {
        puts("addr ok")
    } else {
        puts("addr bad")
    }
    return
}
