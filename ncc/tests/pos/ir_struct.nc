module main

/* 类型定义 */
Person struct {
    name char[]
    age u8
    score i32
}

Data union {
    asInt i32
    asFloat f64
}

Color enum { RED, GREEN, BLUE }

func main() {
    /* struct 声明 + 初始化 + 成员读写 */
    p Person = {100, 25, 90}
    if p.age == 25 {
        puts("age ok")
    } else {
        puts("age bad")
    }
    p.age = 26
    if p.age == 26 {
        puts("age set ok")
    } else {
        puts("age set bad")
    }
    p.score += 10
    if p.score == 100 {
        puts("score ok")
    } else {
        puts("score bad")
    }

    /* union 共享槽 */
    u Data = {7}
    if u.asInt == 7 {
        puts("union int ok")
    } else {
        puts("union int bad")
    }
    u.asInt = 99
    if u.asInt == 99 {
        puts("union set ok")
    } else {
        puts("union set bad")
    }

    /* enum 常量 */
    c i32 = GREEN
    if c == 1 {
        puts("enum ok")
    } else {
        puts("enum bad")
    }
    if BLUE == 2 {
        puts("enum blue ok")
    } else {
        puts("enum blue bad")
    }
    return
}
