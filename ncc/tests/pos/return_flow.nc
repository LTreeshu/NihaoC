module main

/* §12.3 PB-27 返回值前缀正向用例：
 *   flow  f() flow  void  → 返回值所有权转移给调用方
 *   static f() static void → 返回值共享引用（源不变）
 *   验证：flow→flow / static→const 转移 4 后端一致无回归。 */

/* 返回 flow：所有权从 callee 转移到 caller */
func make_flow() flow void {
    flow p void = malloc(i32)
    return p
}

/* 返回 static：callee 持静态池，caller 共享引用（pool 仍 VALID） */
func make_static() static void {
    static pool void = 0
    pool = malloc(i32)
    return pool
}

func main() {
    /* flow→flow：所有权转移（p 成为新所有者） */
    flow p void = make_flow()

    /* static→const：共享引用（只读借用，pool 仍 VALID） */
    const s void = make_static()

    puts("return vis ok")
    return
}
