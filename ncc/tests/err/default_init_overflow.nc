module main
use stdio

/* §3.1 × §5.1.2：成员默认值的逐项展开有缓冲上限（DEFAULT_INIT_BUF），
 * 超出时前端即时报错，而不是产出被截断的初始化器 */

P struct {
    x i32 = 7
    y i32 = 8
    z i32 = 9
}

func main() {
    big P[400]
    return
}
