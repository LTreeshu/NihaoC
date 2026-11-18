module main

/* 窄整数截断：赋值 / 复合赋值 / 自增 三条路径（2026-09-01 补）
 * 背景：IR 前端此前只在「声明初始化」路径经 ir_coerce 做目标类型截断，
 * 普通赋值 `x = e`、复合赋值 `x op= e`、自增 `x++` 三条路径直接 IR_MOV 落槽，
 * 不做截断 —— `b u8 = 0; b = 300` 在 IR 后端得 300，而 A 方案（C 语义）得 44。
 * 该缺口此前无任何用例覆盖（四后端一致地错，跨后端一致性检查也发现不了）。
 * 本用例为 IR_SUBSET（四后端通用），逐条与 C 语义对齐。 */

func main() {
    /* 声明初始化（对照组：这条路径一直是对的） */
    a u8 = 300
    if a == 44 {
        puts("decl-init ok")
    } else {
        puts("decl-init bad")
    }

    /* 普通赋值 */
    b u8 = 0
    b = 300
    if b == 44 {
        puts("assign u8 ok")
    } else {
        puts("assign u8 bad")
    }
    c i8 = 0
    c = 300
    if c == 44 {
        puts("assign i8 ok")
    } else {
        puts("assign i8 bad")
    }
    d i16 = 0
    d = 700 * 100
    if d == 4464 {
        puts("assign i16 ok")
    } else {
        puts("assign i16 bad")
    }
    e u16 = 0
    e = 700 * 100
    if e == 4464 {
        puts("assign u16 ok")
    } else {
        puts("assign u16 bad")
    }
    /* u32：0-1 回绕为 4294967295 */
    g u32 = 0
    g = 0 - 1
    if g == 4294967295 {
        puts("assign u32 ok")
    } else {
        puts("assign u32 bad")
    }

    /* 复合赋值 */
    h u8 = 200
    h += 100
    if h == 44 {
        puts("compound u8 ok")
    } else {
        puts("compound u8 bad")
    }

    /* 自增回绕：u8 255 + 1 → 0 */
    k u8 = 255
    k++
    if k == 0 {
        puts("incr u8 ok")
    } else {
        puts("incr u8 bad")
    }

    /* f64 目标接收整数字面量（IR 需 ITOD，否则整数位模式落进 double 槽） */
    f f64 = 0
    f = 5
    if f == 5.0 {
        puts("f64 assign ok")
    } else {
        puts("f64 assign bad")
    }
    f += 2
    if f == 7.0 {
        puts("f64 compound ok")
    } else {
        puts("f64 compound bad")
    }
    return
}
