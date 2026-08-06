module main

func main() {
    /* A 方案（c/native 后端）：malloc + .() 解引用语法 */
    flow p void = malloc(i32)
    p.(i32) = 42
    if p.(i32) == 42 {
        puts("malloc ok")
    } else {
        puts("malloc bad")
    }
    p.(i32) = 99
    if p.(i32) == 99 {
        puts("malloc write ok")
    } else {
        puts("malloc write bad")
    }
    return
}
