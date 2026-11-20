module main
use stdio

P struct {
    x i32
}

func main() {
    /* PB-90：const 变量的自有存储不可写（string/char[]/定长数组/聚合体内容） */
    const s char[] = "hi"
    s[1] = 'x'

    const a i32[3] = {1, 2, 3}
    a[0] = 9

    const p P
    p.x = 1
    return
}
