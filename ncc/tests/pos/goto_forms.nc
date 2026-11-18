module main

/* PB-59 自写用例：BNF 声明但门禁此前未覆盖的 goto / 标签形态
 * 覆盖：前跳跳过语句 / 后跳循环 / 同名标签分处两函数 / 标签定义在内层块 / 从 switch 的 case 体跳出到块外标签 */

func skip_forward() i32 {
    n i32 = 0
    goto tail
    n = n + 100
tail:
    return n
}

func spin_backward() i32 {
    i i32 = 0
step:
    i = i + 1
    if i < 4 {
        goto step
    }
    return i
}

func block_label() i32 {
    k i32 = 0
    {
inner:
        k = k + 1
    }
    if k < 3 {
        goto inner
    }
    return k
}

func classify(v i32) i32 {
    r i32 = 0
    switch (v) {
        case 1:
            r = r + 1
            goto join
        case 2:
            r = r + 2
        default:
            r = r + 3
    }
join:
    return r + 10
}

func main() {
    if skip_forward() == 0 {
        puts("goto forward ok")
    } else {
        puts("goto forward bad")
    }
    if spin_backward() == 4 {
        puts("goto backward ok")
    } else {
        puts("goto backward bad")
    }
    if block_label() == 3 {
        puts("goto block ok")
    } else {
        puts("goto block bad")
    }
    if classify(1) == 11 {
        puts("goto from case ok")
    } else {
        puts("goto from case bad")
    }
    if classify(2) == 12 {
        puts("case auto-exit ok")
    } else {
        puts("case auto-exit bad")
    }
}
