module main

func main() {
    /* f64 浮点变量声明与运算 */
    a f64 = 1.5
    b f64 = 2.5
    c f64 = a * b + 1.0
    if c == 4.75 {
        puts("float ok")
    } else {
        puts("float bad")
    }
    d f64 = c - b
    if d == 2.25 {
        puts("float sub ok")
    } else {
        puts("float sub bad")
    }
    e f64 = c / a
    if e == 4.75 / 1.5 {
        puts("float div ok")
    } else {
        puts("float div bad")
    }
    /* 浮点比较 */
    if a < b && b <= 2.5 && c > 4.0 {
        puts("fcmp ok")
    } else {
        puts("fcmp bad")
    }
    return
}
