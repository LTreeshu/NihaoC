module main

func main() {
    /* double → int64 截断（向零，与 C 一致） */
    a i64 = 3.7
    if a == 3 {
        puts("dtoi ok")
    } else {
        puts("dtoi bad")
    }
    /* 负数向零 */
    b i64 = -3.7
    if b == -3 {
        puts("dtoi neg ok")
    } else {
        puts("dtoi neg bad")
    }
    /* int → double（float 目标 + int 源 → ITOD） */
    c f64 = 5
    if c == 5.0 {
        puts("itod ok")
    } else {
        puts("itod bad")
    }
    /* double 表达式结果赋 int */
    d i32 = 10.0 / 4.0
    if d == 2 {
        puts("dtoi expr ok")
    } else {
        puts("dtoi expr bad")
    }
    /* double 源赋窄整数（DTOI + TRUNC 链） */
    e i8 = 3.7
    if e == 3 {
        puts("dtoi narrow ok")
    } else {
        puts("dtoi narrow bad")
    }
    /* 数组元素：double 源赋 i32 元素 */
    arr i32[2] = {0, 0}
    arr[1] = 7.9
    if arr[1] == 7 {
        puts("dtoi arr ok")
    } else {
        puts("dtoi arr bad")
    }
    /* float 数组元素赋 int 值 */
    fa f64[2] = {1.5, 0.0}
    fa[0] = 9
    if fa[0] == 9.0 {
        puts("itod arr ok")
    } else {
        puts("itod arr bad")
    }
    /* 一元负 double（FSUB 0.0-a，整数 NEG 会毁位模式） */
    g f64 = -2.5
    if g == 0.0 - 2.5 {
        puts("neg f ok")
    } else {
        puts("neg f bad")
    }
    /* 逻辑非 double（FCMP EQ 0.0） */
    if !(g == 0.0) {
        puts("not f ok")
    } else {
        puts("not f bad")
    }
    return
}
