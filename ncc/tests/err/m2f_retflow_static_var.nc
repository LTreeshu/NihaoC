module m2f_retflow_static_var
/* §12.3 PB-29.1 返回值前缀错误用例：
 *   static → var 禁止（callee 持 static 静态期所有权，赋值给 var 后 var
 *   视为可任意使用/释放，但 static 在程序结束才释放，var 的作用域短于 static）。 */

func make_static() static void {
    static p void = 0
    p = malloc(i32)
    return p
}

func main() {
    var s void = make_static()
    return
}