module main

func main() {
    /* 字符串数组：逐字节拷贝含 NUL */
    s char[6] = "hello"
    if s[0] == 104 && s[1] == 101 && s[4] == 111 && s[5] == 0 {
        puts("char arr ok")
    } else {
        puts("char arr bad")
    }
    /* 字符字面量 */
    c char = 'A'
    if c == 65 {
        puts("char lit ok")
    } else {
        puts("char lit bad")
    }
    /* char 数组元素写字符字面量 */
    s[1] = 'H'
    if s[1] == 72 {
        puts("char write ok")
    } else {
        puts("char write bad")
    }
    /* 字符串池去重（hello 只生成一个 __str_N） */
    s2 char[6] = "hello"
    if s2[0] == 104 {
        puts("str pool ok")
    } else {
        puts("str pool bad")
    }
    /* string 类型（指针别名 i64，存字符串地址） */
    p string = "world"
    q char[6] = "world"
    if q[0] == 119 && q[4] == 100 {
        puts("string alias ok")
    } else {
        puts("string alias bad")
    }
    /* char 截断（255 → -1 符号扩展） */
    x char = 255
    if x == -1 {
        puts("char trunc ok")
    } else {
        puts("char trunc bad")
    }
    /* 字符串数组动态容量（默认 8 槽） */
    d char[] = "hi"
    if d[0] == 104 && d[1] == 105 && d[2] == 0 {
        puts("dyn str ok")
    } else {
        puts("dyn str bad")
    }
    return
}
