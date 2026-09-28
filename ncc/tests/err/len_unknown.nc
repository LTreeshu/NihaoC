module main
use stdio

func main() {
    n i32 = 5
    /* 标量的逻辑长度静态不可知：`len(n)` 必须在前端即时报错（§2.3） */
    print(len(n))
}
