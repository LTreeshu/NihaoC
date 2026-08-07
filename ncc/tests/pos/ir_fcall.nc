module main

/* 浮点参数传递 + 浮点返回（PB-浮点 ABI） */
func addf(a f64, b f64) f64 {
    return a + b
}

/* 混合参数：int 前、float 后 */
func mix(n i32, x f64) f64 {
    return x * n
}

/* 浮点比较 + 浮点返回 */
func negf(x f64) f64 {
    if x > 0.0 {
        return 0.0 - x
    }
    return x
}

/* 多浮点参数（4 个，覆盖寄存器上限边界） */
func avg4(a f64, b f64, c f64, d f64) f64 {
    return (a + b + c + d) / 4.0
}

func main() {
    r f64 = addf(1.5, 2.5)
    if r == 4.0 {
        puts("addf ok")
    } else {
        puts("addf bad")
    }
    m f64 = mix(3, 1.5)
    if m == 4.5 {
        puts("mix ok")
    } else {
        puts("mix bad")
    }
    n f64 = negf(2.0)
    if n == 0.0 - 2.0 {
        puts("negf ok")
    } else {
        puts("negf bad")
    }
    a f64 = avg4(1.0, 2.0, 3.0, 4.0)
    if a == 2.5 {
        puts("avg4 ok")
    } else {
        puts("avg4 bad")
    }
    /* f32 声明（槽化 double，宽转换） */
    s f32 = 1.5
    t f32 = s + 1.5
    if t == 3.0 {
        puts("f32 ok")
    } else {
        puts("f32 bad")
    }
    return
}
