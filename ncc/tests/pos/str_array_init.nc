module main
use stdio

func main() {
    /* §5.1.1：定长 char 数组配字符串字面量 → 按声明容量分配数组存储（不退化为指针），
     * 结尾 NUL 占一格，可原地改写；len() 取声明容量 */
    ch char[9] = "xiaoming"
    puts(ch)
    print("%d\n", len(ch))
    i i32 = 0
    while i < 8 {
        ch[i] = 65 + i
        i = i + 1
    }
    puts(ch)
    /* 刚好放下：2 字符 + 结尾 NUL == 容量 3；len() 按 §2.3 取声明容量 */
    tf char[3] = "ab"
    print("%d\n", len(tf))
    puts(tf)
}
