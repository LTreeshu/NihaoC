module main

/* §12.3 参数前缀 flow/var/const 正常用法（静态检查不引入运行时副作用） */
func consume(flow p void) {
    return
}

func mutate(var p void) {
    return
}

func inspect(const p void) {
    return
}

func main() {
    flow a void = malloc(i32)
    consume(a)            /* flow→flow：所有权转移（a 失效），本测试不再使用 a */
    flow b void = malloc(i32)
    mutate(b)             /* flow→var：可变借用，调用期间冻结，返回后解冻 */
    inspect(b)            /* flow→const：只读借用，调用期间冻结，返回后解冻 */
    puts("param prefix ok")
    return
}
