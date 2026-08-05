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
    return
}
