module m2f_retflow_static_flow
/* §12.3 PB-29.1 返回值前缀错误用例：
 *   static → flow 禁止（callee 持 static 静态期所有权，caller flow 短命
 *   立即失效，无法承接长生命周期）。 */

func make_static() static void {
    static p void = 0
    p = malloc(i32)
    return p
}

func main() {
    flow s void = make_static()
    return
}