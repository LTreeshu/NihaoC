module main

func main() {
    // else-if 链：只有首个匹配分支执行
    a i32 = 15
    if a < 0 {
        puts("branch neg")
    } else if a == 0 {
        puts("branch zero")
    } else if a < 10 {
        puts("branch small")
    } else {
        puts("branch big")
    }

    // 链中间命中，末尾无 else
    b i32 = 0
    if b < 0 {
        puts("b neg")
    } else if b == 0 {
        puts("b zero")
    } else if b < 10 {
        puts("b small bad")
    }

    // 不命中任何分支时静默跳过
    c i32 = 3
    if c > 100 {
        puts("c big bad")
    } else if c > 50 {
        puts("c mid bad")
    } else if c < 0 {
        puts("c neg bad")
    }

    // 循环体内的 else-if
    i i32 = 0
    while i < 3 {
        if i == 0 {
            puts("loop zero")
        } else if i == 1 {
            puts("loop one")
        } else {
            puts("loop other")
        }
        i++
    }

    // else 后接普通块不受影响
    d i32 = 1
    if d == 1 {
        puts("d one")
    } else {
        puts("d other bad")
    }
    return
}
