module main
func main() {
    /* §4.2 多变量声明 + 定长数组：初值含结尾 NUL 必须放得下（与单变量口径一致） */
    var {aa = "aa",bb = "bb"} char[2]
    puts(aa)
}
