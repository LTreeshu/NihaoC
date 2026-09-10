module main

/* §12.3 struct 参数 × 前缀组合正向用例：
 *   按 §12.3 struct 参数非指针类，跳过 M2 所有权检查（所有权仅对 void 指针类生效）。
 *   验证：含可见性前缀的 struct 参数声明/调用在 4 后端一致无回归。 */

Person struct {
    age
    score
}

/* struct + flow 前缀：所有权转移（实参 struct 变量失效，但因非指针类无运行时副作用） */
func consume_struct(flow p Person) {
    return
}

/* struct + var 前缀：可变借用（同上，非指针类跳过检查） */
func mutate_struct(var p Person) {
    return
}

/* struct + const 前缀：只读借用 */
func inspect_struct(const p Person) {
    return
}

/* struct 参数 + 标量参数混排：含前缀 */
func tagged(flow p Person, const n i64) i64 {
    return n
}

func main() {
    a Person = {30, 85}
    consume_struct(a)
    mutate_struct(a)
    inspect_struct(a)

    b Person = {40, 90}
    c i64 = tagged(b, 7)
    if c == 7 {
        puts("struct param prefix ok")
    } else {
        puts("struct param prefix bad")
    }
    return
}
