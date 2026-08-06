module main

func main() {
    /* 存储期前缀声明 */
    const MAX i32 = 100
    var count i32 = 5
    flow dynp i32 = 0
    static sv i32 = 7

    /* visof 编译期可见性查询 */
    if visof(MAX) == _const {
        puts("const ok")
    } else {
        puts("const bad")
    }
    if visof(count) == _var {
        puts("var ok")
    } else {
        puts("var bad")
    }
    if visof(dynp) == _flow {
        puts("flow ok")
    } else {
        puts("flow bad")
    }
    if visof(sv) == _static {
        puts("static ok")
    } else {
        puts("static bad")
    }
    if visof(unknown_var) == _undef {
        puts("undef ok")
    } else {
        puts("undef bad")
    }

    /* is 可见性模式：匹配循环条件值 == NH_* 常量 */
    while visof(MAX) {
        is _const {
            puts("is const ok")
        }
        is _var {
            puts("is var bad")
        }
        break
    }
    return
}
