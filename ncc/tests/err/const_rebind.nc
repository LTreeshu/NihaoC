module main
use stdio

func main() {
    /* PB-90：const 变量不可重绑定 */
    const c i32 = 1
    c = 2
    return
}
