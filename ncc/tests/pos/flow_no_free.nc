module main
use stdio

func main() {
    /* §11.1：flow 的自动释放只发给堆所有权。字符串字面量在静态只读段、
     * `&x` 指向栈帧对象，两者在块/函数退出时释放都是非法释放 */
    flow s char[] = "literal"
    puts(s)
    n i32 = 7
    flow p void = &n
    p.(i32) = 99
    print("%d\n", n)
    /* 堆所有权仍按块/函数退出自动释放 */
    flow q void = malloc(i32)
    q.(i32) = 5
    print("%d\n", q.(i32))
    /* 整变量重绑定重新登记来源：q 改指栈对象后不再被 free */
    q = &n
    print("%d\n", q.(i32))
    puts("flow no free ok")
}
