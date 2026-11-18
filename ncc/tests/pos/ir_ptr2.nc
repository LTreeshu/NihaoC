module main

/* 阶段 1：类型化指针模型（2026-08-31，1.0.x 解引用统一为 .()）
 * .() 读（double 标记）/ .() = e 写（窄型截断）/ 指针算术 p+k（槽宽 8）
 * 复合解引用 p.() op= e（2.0 对齐 .() 复合：LOAD→类型协调→op→截断→STORE） */
func main() {
    /* .() 读：普通标量指针 */
    x i32 = 42
    p = &x
    y i32 = p.()
    if y == 42 {
        puts("ptr read ok")
    } else {
        puts("ptr read bad")
    }

    /* .() = e 写 */
    p.() = 43
    if x == 43 {
        puts("ptr write ok")
    } else {
        puts("ptr write bad")
    }

    /* double 指针：.() 读值标记浮点（否则位模式被当整数） */
    d f64 = 3.5
    pd = &d
    z f64 = pd.()
    if z == 3.5 {
        puts("ptr double ok")
    } else {
        puts("ptr double bad")
    }

    /* 窄类型指针：.() = e 按指向宽度截断（300 → i8 44） */
    c i8 = 100
    pc = &c
    pc.() = 300
    if c == 44 {
        puts("ptr narrow ok")
    } else {
        puts("ptr narrow bad")
    }

    /* 指针算术：p + k = 地址 + k*8（槽宽；值不可断言，编译运行不崩即可） */
    q = p + 1
    puts("ptr arith ok")

    /* 复合解引用：通用指针 p.() += 2（RMW，等价 p.() = p.() + 2） */
    p.() += 2
    if x == 45 {
        puts("ptr compound ok")
    } else {
        puts("ptr compound bad")
    }

    /* 复合解引用：double 指向 pd.() += 1.0（FADD） */
    pd.() += 1.0
    z = pd.()
    if z == 4.5 {
        puts("ptr double compound ok")
    } else {
        puts("ptr double compound bad")
    }

    /* 复合解引用：窄型指向 pc.() += 1（i8 截断 44→45） */
    pc.() += 1
    if c == 45 {
        puts("ptr narrow compound ok")
    } else {
        puts("ptr narrow compound bad")
    }

    /* 复合解引用：*= / -= 也验证（x 此时 45，*3=135，-10=125） */
    p.() *= 3
    if x == 135 {
        puts("ptr mul ok")
    } else {
        puts("ptr mul bad")
    }
    p.() -= 10
    if x == 125 {
        puts("ptr sub ok")
    } else {
        puts("ptr sub bad")
    }
    return
}
