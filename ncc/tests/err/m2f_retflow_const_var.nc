module m2f_retflow_const_var
/* §12.3 PB-29.1 返回值前缀错误用例：
 *   const → var 禁止（callee 返回值带 const 所有权，赋值给 var 后 var 可写，
 *   破坏 const 永生不变的契约）。 */

func make_const() const void {
    const p void = malloc(i32)
    return p
}

func main() {
    var s void = make_const()
    return
}