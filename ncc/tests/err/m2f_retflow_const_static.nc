module m2f_retflow_const_static
/* §12.3 PB-29.1 返回值前缀错误用例：
 *   const → static 禁止（callee 持 const 永生，caller static 静态期但生命周期
 *   与 const 不重叠——const 永生延续超过静态期，static 在程序结束时释放，
 *   之后 const 仍持有原对象引用，悬垂）。 */

func make_const() const void {
    const p void = malloc(i32)
    return p
}

func main() {
    static s void = make_const()
    return
}