module main

func main() {
    k i32 = 0
    ++k
    if k == 1 {
        puts("preinc ok")
    } else {
        puts("preinc bad")
    }
    m i32 = 10
    --m
    if m == 9 {
        puts("predec ok")
    } else {
        puts("predec bad")
    }
    return
}
