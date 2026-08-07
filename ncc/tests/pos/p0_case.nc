module main

func main() {
    /* P0 ①：数组初始化列表 = {1,2,3} */
    arr i32[4] = {10, 20, 30, 40}
    if arr[0] == 10 && arr[3] == 40 {
        puts("init ok")
    } else {
        puts("init bad")
    }
    /* P0 ②：前缀 ++/-- */
    x i32 = 5
    p i32 = ++x
    q i32 = --x
    if p == 6 && q == 5 && x == 5 {
        puts("pre ok")
    } else {
        puts("pre bad")
    }
    /* P0 ③：switch/case/default（case 自动跳出） */
    v i32 = 2
    switch (v) {
        case 1:
            puts("one")
        case 2:
            puts("two")
        default:
            puts("other")
    }
    n i32 = 9
    switch (n) {
        case 0:
            puts("zero")
        case 1:
            puts("one")
    }
    puts("end")
    return
}
