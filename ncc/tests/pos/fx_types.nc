module main

/* fx32 / fx64 按同宽整型（BNF v2.11 定案：宽 4 / 宽 8，运算、字面量、转换均按整型处理）
   覆盖：字面量截断（声明与二次赋值两路）/ 四则 / 与 i32 混算 / 形参与返回 / sizeof(类型) / fx64 宽整型对照 */

func add32(a fx32, b fx32) fx32 {
    return a + b
}

func wide64(a fx64, b fx64) fx64 {
    return a + b
}

func main() {
    x fx32 = 7
    y fx32 = 3
    if x + y == 10 {
        puts("fx32 add ok")
    } else {
        puts("fx32 add bad")
    }
    if x - y == 4 {
        puts("fx32 sub ok")
    } else {
        puts("fx32 sub bad")
    }
    if x * y == 21 {
        puts("fx32 mul ok")
    } else {
        puts("fx32 mul bad")
    }
    if x / y == 2 {
        puts("fx32 div ok")
    } else {
        puts("fx32 div bad")
    }
    t fx32 = 5000000000
    if t == 705032704 {
        puts("fx32 decl trunc ok")
    } else {
        puts("fx32 decl trunc bad")
    }
    s fx32 = 0
    s = 5000000000
    if s == 705032704 {
        puts("fx32 assign trunc ok")
    } else {
        puts("fx32 assign trunc bad")
    }
    u fx64 = 5000000000
    if u == 5000000000 {
        puts("fx64 wide ok")
    } else {
        puts("fx64 wide bad")
    }
    if sizeof(fx32) == 4 {
        puts("fx32 size ok")
    } else {
        puts("fx32 size bad")
    }
    if sizeof(fx64) == 8 {
        puts("fx64 size ok")
    } else {
        puts("fx64 size bad")
    }
    i i32 = 5
    if i + x == 12 {
        puts("fx int mix ok")
    } else {
        puts("fx int mix bad")
    }
    if add32(x, y) == 10 {
        puts("fx32 call ok")
    } else {
        puts("fx32 call bad")
    }
    if wide64(u, 1) == 5000000001 {
        puts("fx64 call ok")
    } else {
        puts("fx64 call bad")
    }
}
