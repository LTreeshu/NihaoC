module main
use stdio

func main() {
    /* 定长数组的多变量声明只支持字符串字面量初值；其余形态必须改用单变量声明，
       不能静默丢掉 `[4]` 生成 `int x = 1` 这类错误 C（§4.2 / §5.1.2） */
    var {x = 1,y = 2} i32[4]
    puts("unreachable")
}
