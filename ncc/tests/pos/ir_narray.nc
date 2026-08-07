module main

func main() {
    /* i8 数组：初始化列表截断 */
    a i8[3] = {300, 1, 2}
    if a[0] == 44 {
        puts("i8 init ok")
    } else {
        puts("i8 init bad")
    }
    /* 元素赋值截断 + 负数符号扩展 */
    a[1] = 255
    if a[1] == -1 {
        puts("i8 store ok")
    } else {
        puts("i8 store bad")
    }
    /* i16 数组 */
    b i16[2] = {70000, 5}
    if b[0] == 4464 {
        puts("i16 ok")
    } else {
        puts("i16 bad")
    }
    /* u8 数组：255 保持、300 截断为 44 */
    d u8[2] = {255, 300}
    if d[0] == 255 && d[1] == 44 {
        puts("u8 ok")
    } else {
        puts("u8 bad")
    }
    /* f64 数组：元素读写 + 运算（ir-c 类型感知 LOAD/STORE） */
    c f64[3] = {1.5, 2.5, 3.0}
    s f64 = c[0] + c[1] + c[2]
    if s == 7.0 {
        puts("f64 ok")
    } else {
        puts("f64 bad")
    }
    c[2] = 10.0
    if c[2] == 10.0 {
        puts("f64 write ok")
    } else {
        puts("f64 write bad")
    }
    /* 窄数组元素参与浮点混合（int 提升） */
    m f64 = a[0] * 1.5
    if m == 66.0 {
        puts("mix ok")
    } else {
        puts("mix bad")
    }
    return
}
