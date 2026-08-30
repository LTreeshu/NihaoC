module main

func main() {
    x i32 = 10
    x += 5
    x -= 2
    x *= 3
    x /= 2
    x %= 7
    if x == 5 {
        puts("cmp ok")
    } else {
        puts("cmp bad")
    }
    y i32 = -3
    z i32 = !y
    if z == 0 {
        puts("neg ok")
    } else {
        puts("neg bad")
    }
    i i32 = 0
    while i < 3 {
        i++
    }
    if i == 3 {
        puts("inc ok")
    } else {
        puts("inc bad")
    }
    j i32 = 5
    j--
    if j == 4 {
        puts("dec ok")
    } else {
        puts("dec bad")
    }
    n i32 = 0
    w i32 = ~n
    if w == -1 {
        puts("bitnot ok")
    } else {
        puts("bitnot bad")
    }
    m i32 = 5
    t i32 = (m > 3) ? 100 : 200
    if t == 100 {
        puts("tern ok")
    } else {
        puts("tern bad")
    }
    u i32 = (m > 10) ? 1 : ((m > 3) ? 2 : 3)
    if u == 2 {
        puts("tern2 ok")
    } else {
        puts("tern2 bad")
    }
    return
}
