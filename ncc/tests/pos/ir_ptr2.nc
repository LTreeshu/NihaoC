module main

/* 阶段 1：类型化指针模型（2026-08-31，1.0.x 解引用统一为 .()）
 * .() 读（double 标记）/ .() = e 写（窄型截断）/ 指针算术 p+k（槽宽 8）
 * 复合解引用 *p += e 已展开为 p.() = p.() + e（双后端兼容） */
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

    /* 复合解引用展开为 p.() = p.() + e（RMW） */
    p.() = p.() + 2
    if x == 45 {
        puts("ptr compound ok")
    } else {
        puts("ptr compound bad")
    }
    return
}
