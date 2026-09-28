module main
use stdio

func main() {
    /* §4.2 多变量声明 + 定长数组：按声明容量分配数组存储，与单变量分支同口径（§5.1.2） */
    var {aa = "aa",bb = "bb",cc = "cc"} char[3]
    if len(aa) == 3 {
        puts("cap3 ok")
    }
    puts(aa)
    puts(cc)

    /* 动态字符串形态 `char[]`：退化为指针，len() 取字面量长度 */
    var {d1 = "abcd",d2 = "xy"} char[]
    if len(d1) == 4 {
        puts("dyn4 ok")
    }
    if len(d2) == 2 {
        puts("dyn2 ok")
    }
    puts(d1)

    /* 标量多变量声明不受影响 */
    var {a = 0,b = 1,c = 0} i8
    if a == 0 && b == 1 && c == 0 {
        puts("scalar ok")
    }

    puts("multi arr decl ok")
}
