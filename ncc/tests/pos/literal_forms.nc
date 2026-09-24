module main
use stdio

/* BNF §1.3 字面量：整数十/十六/二进制（各带符号）、浮点（含 e 计数法）、
   字符与字符串转义 */

func main() {
    d i32 = 42
    h i32 = 0x1F
    b i32 = 0b1011
    n i32 = -17
    p i32 = +23
    hn i32 = -0x10
    bn i32 = +0b10000
    if d == 42 && h == 31 && b == 11 && n == -17 && p == 23 && hn == -16 && bn == 16 {
        puts("int forms ok")
    } else {
        puts("int forms bad")
    }

    f1 f64 = 1.5
    f2 f64 = -0.25
    f3 f64 = 2e3
    f4 f64 = +1.5e-2
    if f1 == 1.5 && f2 == -0.25 && f3 == 2000.0 && f4 == 0.015 {
        puts("float forms ok")
    } else {
        puts("float forms bad")
    }

    c1 char = 'A'
    c2 char = '\n'
    c3 char = '\0'
    c4 char = '\\'
    c5 char = '\''
    if c1 == 65 && c2 == 10 && c3 == 0 && c4 == 92 && c5 == 39 {
        puts("char forms ok")
    } else {
        puts("char forms bad")
    }

    s string = "tab\there\nnl\\q\"w"
    print(len(s))
    t bool = true
    if t { puts("bool ok") } else { puts("bool bad") }
    return
}
