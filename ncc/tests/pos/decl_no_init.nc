module main
use stdio

/* SH-1（自举准备）：声明初值可省略，无初值时确定性零填充。
 * 此前 IR 前端在声明路径上无条件 expect('=')，`buf char[64]` 直接语法错，
 * 后续语句整条级联失败——写编译器的人第一步就卡在"要一块缓冲"。 */
func main() {
    buf char[8]
    n i32
    d f64

    k i32 = 0
    sum i32 = 0
    while k < 8 {
        sum = sum + buf[k]
        k = k + 1
    }

    if n == 0 {
        puts("int zero")
    } else {
        puts("int not zero")
    }
    if d == 0.0 {
        puts("f64 zero")
    } else {
        puts("f64 not zero")
    }
    print("bufsum=%d\n", sum)
}
