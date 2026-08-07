module main

/* 窄整数参数（值已符号扩展，入槽直通） */
func i8add(a i8, b i8) i32 {
    c i32 = a + b
    return c
}

func main() {
    /* i8 截断：300 & 0xFF = 44 */
    a i8 = 300
    if a == 44 {
        puts("i8 ok")
    } else {
        puts("i8 bad")
    }
    /* i8 负数：255 → 符号扩展 = -1 */
    b i8 = 255
    if b == -1 {
        puts("i8 neg ok")
    } else {
        puts("i8 neg bad")
    }
    /* u8 无符号：255 → 255 */
    c u8 = 255
    if c == 255 {
        puts("u8 ok")
    } else {
        puts("u8 bad")
    }
    /* i16 截断 + 复合赋值截断 */
    d i16 = 70000
    if d == 4464 {
        puts("i16 ok")
    } else {
        puts("i16 bad")
    }
    d += 1
    if d == 4465 {
        puts("i16 inc ok")
    } else {
        puts("i16 inc bad")
    }
    d++
    if d == 4466 {
        puts("i16 pp ok")
    } else {
        puts("i16 pp bad")
    }
    /* i32 截断 */
    e i32 = 5000000000
    if e == 705032704 {
        puts("i32 ok")
    } else {
        puts("i32 bad")
    }
    /* u32 高位清零（结果同 i32 截断值） */
    f u32 = 5000000000
    if f == 705032704 {
        puts("u32 ok")
    } else {
        puts("u32 bad")
    }
    /* 窄整数参数传递 + 读回运算 */
    s i32 = i8add(100, 27)
    if s == 127 {
        puts("i8 param ok")
    } else {
        puts("i8 param bad")
    }
    /* 窄整数与浮点混合（int 提升为 double） */
    g f64 = a * 1.5
    if g == 66.0 {
        puts("mix ok")
    } else {
        puts("mix bad")
    }
    return
}
