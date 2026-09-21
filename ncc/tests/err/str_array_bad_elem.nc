module main
use stdio

func main() {
    /* 元素类型不是 char 的定长数组不能用字符串字面量初始化（§5.1.1） */
    x i32[3] = "ab"
    print("%d\n", x[0])
}
