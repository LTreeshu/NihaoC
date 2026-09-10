module main

/* §12.3 static 参数共享引用正向用例：
 *   static 参数转移矩阵允许 static→static / static→const（源不变）。
 *   验证：同一 static 源可被多次借用/共享，4 后端一致。 */

func shared(static p void) {
    return
}

func read_only(const p void) {
    return
}

func main() {
    /* static 池（常量初始化 0 + 运行时分配，避开 C static-local+func-init 限制） */
    static s void = 0
    s = malloc(i32)
    /* static→static：源不变（共享引用），可多次调用 */
    shared(s)
    shared(s)
    /* static→const：源不变（只读借用） */
    read_only(s)
    /* 再 shared 仍 OK（借用在调用结束已解冻） */
    shared(s)
    puts("static share ok")
    return
}
