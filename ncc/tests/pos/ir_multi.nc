module main

func main() {
    /* 多变量声明（带 var 前缀） */
    var {a = 1, b = 2, c = 3} i32
    if a == 1 && b == 2 && c == 3 {
        puts("multi ok")
    } else {
        puts("multi bad")
    }
    a = 10
    if a == 10 && b == 2 {
        puts("multi write ok")
    } else {
        puts("multi write bad")
    }

    /* const 前缀多变量 */
    const {MIN = 0, MAX = 100} i32
    if MAX == 100 && visof(MAX) == _const {
        puts("const multi ok")
    } else {
        puts("const multi bad")
    }

    /* 复合初始化表达式（按声明顺序求值） */
    x i32 = 5
    var {p = x + 1, q = x * 2} i32
    if p == 6 && q == 10 {
        puts("expr init ok")
    } else {
        puts("expr init bad")
    }
    return
}
