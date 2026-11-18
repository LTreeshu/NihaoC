module main
use stdio

/* PA-49 / F1：成员默认值在变量声明处展开为 C 指定初始化器（BNF §3.1 field-decl） */
Cfg struct {
    width i32 = 800
    height i32 = 600
    title char[]
    ratio f64 = 1.5
}

Slot union {
    asInt i32 = 7
    asBits u32
}

func main() {
    /* 无初值：全部默认值生效 */
    c Cfg
    print("%d %d %d\n", c.width, c.height, c.ratio == 1.5)

    /* 位置初值覆盖前两员，省略的 ratio 仍取默认值 */
    d Cfg = {320, 200}
    print("%d %d %d\n", d.width, d.height, d.ratio == 1.5)

    /* union 只取首个带默认值的成员；用户一给初值就以用户为准 */
    s Slot
    print("%d\n", s.asInt)
    s2 Slot = {9}
    print("%d\n", s2.asInt)

    /* PA-50 / F2：字面量里的转义须以转义序列落入产物 C，而非真字节 */
    t char[] = "a\tb"
    puts(t)
    puts("q\"z")
    puts("c:\\d")
}
