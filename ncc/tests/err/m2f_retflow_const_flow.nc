module m2f_retflow_const_flow
/* §12.3 PB-29.1 返回值前缀错误用例：
 *   const → flow 禁止（callee 持 const 永生，caller flow 短命无法承接长生命周期）。 */

func make_const() const void {
    const p void = malloc(i32)
    return p
}

func main() {
    flow s void = make_const()
    return
}