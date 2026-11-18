module main
/* §12.3 PB-27 返回值前缀错误用例：
 *   flow → static 禁止（callee 持 flow 所有权，caller 静态引用无法承接短生命周期）。 */

func make_flow() flow void {
    flow p void = malloc(i32)
    return p
}

func main() {
    static s void = make_flow()
    return
}
