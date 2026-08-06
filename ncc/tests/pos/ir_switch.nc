module main

func main() {
    /* do 循环（NihaoC 中 do 是 while 的别名，前测循环） */
    n i32 = 0
    do n < 5 {
        n++
    }
    if n == 5 {
        puts("do ok")
    } else {
        puts("do bad")
    }

    /* while 条件为赋值表达式（值供 is 匹配）+ is 模式匹配 */
    cnt i32 = 0
    while cnt += 1 {
        is 1 {
            puts("is one")
        }
        is 3..4 {
            puts("is range")
        }
        if cnt == 5 {
            break
        }
    }
    if cnt == 5 {
        puts("is ok")
    } else {
        puts("is bad")
    }

    /* switch：case 值匹配 + default */
    x i32 = 2
    switch (x) {
        case 1:
            puts("case one")
        case 2:
            puts("case two")
        default:
            puts("case def")
    }
    return
}
