module main
use stdio

func main() {
    /* 声明容量 4 放不下 8 字符 + 结尾 NUL（§5.1.1），诊断口径与切片赋值一致 */
    ch char[4] = "xiaoming"
    puts(ch)
}
